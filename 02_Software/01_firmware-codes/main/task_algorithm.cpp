#include "task_algorithm.h"

#include <math.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "motor_control.h"
#include "task_config.h"
#include "task_control.h"
#include "task_imu.h"
#include "task_tof.h"

#include "wifi_telemetry.h"

extern "C" {
#include "encoder.h"
}
// ---------------------------------------------------------------------------
// Tuning constants
// ---------------------------------------------------------------------------
#define OPEN_WALL_MM                                                           \
  1000 // side distance above this = wall gone (opening detected)

#define DIR_DETECT_COUNT 1 // consecutive readings required to confirm direction
#define TURN_SETTLE_DEG 5.0f // heading error threshold to consider turn done
#define TURN_SETTLE_COUNT                                                      \
  6 // consecutive cycles within threshold to confirm settled
#define TOTAL_LAPS 3
#define TURNS_PER_LAP 4

static float s_first_segment_mm = 0.0f;
static bool s_first_segment_recorded = false;

// ---------------------------------------------------------------------------
// State machine
// ---------------------------------------------------------------------------
typedef enum {
  STATE_IDLE,
  STATE_DETECT_DIR, // drive forward, wait for one side wall to open
  STATE_FORWARD,    // drive forward, watch front ToF for corner
  STATE_TURNING,    // wait for turn to settle
  STATE_FINISH,
} alg_state_t;

static alg_state_t s_state = STATE_IDLE;

// ---------------------------------------------------------------------------
// Runtime state
// ---------------------------------------------------------------------------
static int s_turn_count = 0;
static int s_lap_count = 0;
static float s_heading = 0.0f;      // heading held after each turn (deg)
static float s_turn_dir_deg = 0.0f; // +90 CW, -90 CCW
static bool s_done = false;

// Direction detection debounce
static int s_open_right_count = 0;
static int s_open_left_count = 0;

// Turn settling debounce
static int s_settle_count = 0;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static float wrap180(float deg) {
  while (deg > 180.0f)
    deg -= 360.0f;
  while (deg < -180.0f)
    deg += 360.0f;
  return deg;
}

// ---------------------------------------------------------------------------
// Task
// ---------------------------------------------------------------------------
static void algorithm_task(void *arg) {
  (void)arg;

  TickType_t last = xTaskGetTickCount();
  TickType_t inc = pdMS_TO_TICKS(ALG_PERIOD_MS);
  if (inc == 0)
    inc = 1;

  while (true) {

    // --- read sensors ------------------------------------------------
    float yaw_deg = 0.0f;
    bool yaw_ok = false;
    int64_t yaw_age = 0;
    yaw_read(&yaw_deg, &yaw_ok, &yaw_age);

    tof_state_t tof = {};
    tof_read(&tof);
    char buf[64];
    snprintf(buf, sizeof(buf), "L=%u F=%u R=%u\n", tof.left_mm, tof.front_mm,
             tof.right_mm);
    // wifi_telemetry_send(buf);
    // --- state machine -----------------------------------------------
    switch (s_state) {

    // -----------------------------------------------------------------
    case STATE_IDLE:
      if (s_done)
        break;

      // Reset counters and start moving
      s_turn_count = 0;
      s_lap_count = 0;
      s_heading = 0.0f;
      s_turn_dir_deg = 0.0f;
      s_open_right_count = 0;
      s_open_left_count = 0;
      s_settle_count = 0;
      encoder_reset();

      move_forward(30);
      ctrl_request_forward(s_heading);
      s_first_segment_mm = 0.0f;
      s_first_segment_recorded = false;
      s_state = STATE_DETECT_DIR;
      break;

    // -----------------------------------------------------------------
    case STATE_DETECT_DIR:
      // Emergency: front wall before direction is known
      if (tof.front_emergency_wall) {
        ctrl_stop();
        stop();
        s_state = STATE_IDLE;
        break;
      }

      // Debounce right opening
      if (tof.right_mm > OPEN_WALL_MM)
        s_open_right_count++;
      else
        s_open_right_count = 0;

      // Debounce left opening
      if (tof.left_mm > OPEN_WALL_MM)
        s_open_left_count++;
      else
        s_open_left_count = 0;

      if (s_open_right_count >= DIR_DETECT_COUNT) {
        s_turn_dir_deg = +90.0f; // clockwise
        s_state = STATE_FORWARD;
      } else if (s_open_left_count >= DIR_DETECT_COUNT) {
        s_turn_dir_deg = -90.0f; // counter-clockwise
        s_state = STATE_FORWARD;
      }
      break;

      // -----------------------------------------------------------------
    case STATE_FORWARD:
      // Emergency stop
      if (tof.front_emergency_wall) {
        ctrl_stop();
        stop();
        s_state = STATE_IDLE;
        break;
      }

      if (!s_done) {
        bool side_open = (s_turn_dir_deg > 0) ? (tof.right_mm > OPEN_WALL_MM)
                                              : (tof.left_mm > OPEN_WALL_MM);

        if (side_open || tof.front_warning_wall) {
          if (!s_first_segment_recorded) {
            s_first_segment_mm = encoder_get_distance_mm();
            s_first_segment_recorded = true;
          }
          float next_heading = wrap180(s_heading + s_turn_dir_deg);
          ctrl_request_turn(next_heading);
          s_state = STATE_TURNING;
        }
      }
      break;

      // -----------------------------------------------------------------
    case STATE_TURNING:
      if (ctrl_turn_done()) {
        s_heading = wrap180(s_heading + s_turn_dir_deg);
        s_turn_count++;
        encoder_reset();

        if (s_turn_count % TURNS_PER_LAP == 0)
          s_lap_count++;

        if (s_lap_count >= TOTAL_LAPS) {
          ctrl_request_forward(s_heading);
          s_done = true;
          s_state = STATE_FINISH;
        } else {
          ctrl_request_forward(s_heading);
          s_state = STATE_FORWARD;
        }
      }
      break;

    case STATE_FINISH: {
      float target_mm = 1000.0f - s_first_segment_mm;
      if (encoder_get_distance_mm() >= target_mm || tof.front_emergency_wall) {
        ctrl_stop();
        stop();
        s_state = STATE_IDLE;
      }
      break;
    }
    }
    vTaskDelayUntil(&last, inc);
  }
}

// ---------------------------------------------------------------------------
// Public
// ---------------------------------------------------------------------------
void algorithm_task_start(void) {
  xTaskCreate(algorithm_task, "alg_task", ALG_STACK_SIZE, NULL, ALG_PRIORITY,
              NULL);
}
