#include "task_algorithm.h"

#include <math.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "motor_control.h"
#include "task_RPI.h"
#include "task_config.h"
#include "task_control.h"
#include "task_imu.h"

#include "wifi_telemetry.h"

extern "C" {
#include "encoder.h"
}

// ===========================================================================
// OPEN CHALLENGE — camera-only algorithm
//
// Sensor set:
//   IMU     -> heading (owned by control task)
//   Encoder -> distance travelled since last turn
//   Camera  -> NAV only (wall presence, 0-100% per side)
//              OBST field is ignored — no pillars in Open Challenge
//
// Message format from Pi:
//   NAV:L=<0-100>,R=<0-100>;OBST:NONE
//
// NAV: HIGH = wall present. LOW = wall receded = corner opening.
// ===========================================================================

// ---------------------------------------------------------------------------
// Wall / corner detection
// ---------------------------------------------------------------------------
#define NAV_OPEN_THRESHOLD 25 // below this = wall gone (opening)
#define DIR_DETECT_COUNT 3    // frames to confirm driving direction
#define CORNER_DETECT_COUNT 3 // frames to confirm corner opening

// Don't look for a corner until this far past the previous turn.
#define MIN_FORWARD_MM_BEFORE_CORNER_CHECK 150.0f

// Safety: if no corner is found within this distance something is wrong.
#define MAX_SEGMENT_MM 5000.0f

// ---------------------------------------------------------------------------
// Camera link health
// ---------------------------------------------------------------------------
#define RPI_STALE_US (500 * 1000) // 500 ms

// ---------------------------------------------------------------------------
// Drive speeds (0-100)
// ---------------------------------------------------------------------------
#define DRIVE_SPEED 45

// ---------------------------------------------------------------------------
// Course
// ---------------------------------------------------------------------------
#define TOTAL_LAPS 3
#define TURNS_PER_LAP 4
#define FINISH_SECTION_MM 1000.0f

// Only re-issue forward request when target shifts by more than this.
// Prevents resetting PID integral every cycle.
#define FWD_RETARGET_DEADBAND_DEG 0.75f

// ---------------------------------------------------------------------------
// State machine
// ---------------------------------------------------------------------------
typedef enum {
  STATE_IDLE,
  STATE_DETECT_DIR,
  STATE_FORWARD,
  STATE_TURNING,
  STATE_FINISH,
} alg_state_t;

static alg_state_t s_state = STATE_IDLE;

// ---------------------------------------------------------------------------
// Runtime state
// ---------------------------------------------------------------------------
static int s_turn_count = 0;
static int s_lap_count = 0;
static float s_heading = 0.0f;
static float s_turn_dir_deg = 0.0f;
static bool s_done = false;

static int s_open_right_count = 0;
static int s_open_left_count = 0;
static int s_corner_count = 0;

static float s_first_segment_mm = 0.0f;
static bool s_first_segment_recorded = false;

static float s_last_fwd_cmd_deg = 0.0f;
static bool s_last_fwd_cmd_valid = false;

static bool s_link_lost = false;

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

static const char *state_name(alg_state_t s) {
  switch (s) {
  case STATE_IDLE:
    return "IDLE";
  case STATE_DETECT_DIR:
    return "DETECT_DIR";
  case STATE_FORWARD:
    return "FORWARD";
  case STATE_TURNING:
    return "TURNING";
  case STATE_FINISH:
    return "FINISH";
  default:
    return "UNKNOWN";
  }
}

static void forward_hold(float heading_deg) {
  if (!s_last_fwd_cmd_valid ||
      fabsf(wrap180(heading_deg - s_last_fwd_cmd_deg)) >
          FWD_RETARGET_DEADBAND_DEG) {
    ctrl_request_forward(heading_deg);
    s_last_fwd_cmd_deg = heading_deg;
    s_last_fwd_cmd_valid = true;
  }
}

static void forward_hold_invalidate(void) { s_last_fwd_cmd_valid = false; }

// ---------------------------------------------------------------------------
// Telemetry
// ---------------------------------------------------------------------------
static void send_telemetry(const rpi_state_t *rpi, bool link_fresh,
                           float yaw_deg, bool yaw_ok) {
  if (!wifi_telemetry_is_connected())
    return;

  char buf[256];
  int n = 0;

  n += snprintf(buf + n, sizeof(buf) - n, "{");

  if (yaw_ok)
    n += snprintf(buf + n, sizeof(buf) - n, "\"imu\":{\"yaw\":%.2f},", yaw_deg);

  n += snprintf(buf + n, sizeof(buf) - n, "\"enc\":{\"mm\":%d},",
                (int)encoder_get_distance_mm());

  n += snprintf(buf + n, sizeof(buf) - n, "\"link\":%d,", link_fresh ? 1 : 0);

  if (link_fresh)
    n += snprintf(buf + n, sizeof(buf) - n, "\"nav\":{\"L\":%d,\"R\":%d},",
                  rpi->nav_left, rpi->nav_right);

  n += snprintf(buf + n, sizeof(buf) - n,
                "\"state\":\"%s\",\"turn\":%d,\"lap\":%d}\n",
                state_name(s_state), s_turn_count, s_lap_count);

  wifi_telemetry_send(buf);
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

    // ---- read sensors ---------------------------------------------------
    float yaw_deg = 0.0f;
    bool yaw_ok = false;
    int64_t yaw_age = 0;
    yaw_read(&yaw_deg, &yaw_ok, &yaw_age);

    rpi_state_t rpi = {};
    bool rpi_ok = rpi_state_read(&rpi);
    bool link_fresh = rpi_ok && rpi.valid && (rpi.age_us < RPI_STALE_US);

    send_telemetry(&rpi, link_fresh, yaw_deg, yaw_ok);

    // ---- camera link watchdog -------------------------------------------
    if (!link_fresh && s_state != STATE_IDLE && s_state != STATE_TURNING) {
      if (!s_link_lost) {
        s_link_lost = true;
        stop();
        forward_hold_invalidate();
      }
      vTaskDelayUntil(&last, inc);
      continue;
    }

    if (s_link_lost && link_fresh) {
      s_link_lost = false;
      forward_hold_invalidate();
      if (s_state == STATE_FORWARD || s_state == STATE_DETECT_DIR ||
          s_state == STATE_FINISH) {
        move_forward(DRIVE_SPEED);
        forward_hold(s_heading);
      }
    }

    // ---- state machine --------------------------------------------------
    switch (s_state) {

    // -----------------------------------------------------------------
    case STATE_IDLE:
      if (s_done)
        break;

      s_turn_count = 0;
      s_lap_count = 0;
      s_heading = 0.0f;
      s_turn_dir_deg = 0.0f;
      s_open_right_count = 0;
      s_open_left_count = 0;
      s_corner_count = 0;
      s_first_segment_mm = 0.0f;
      s_first_segment_recorded = false;
      forward_hold_invalidate();
      encoder_reset();
      (void)ctrl_turn_done(); // drain stale flag

      move_forward(DRIVE_SPEED);
      forward_hold(s_heading);
      s_state = STATE_DETECT_DIR;
      break;

    // -----------------------------------------------------------------
    case STATE_DETECT_DIR:
      if (rpi.nav_right < NAV_OPEN_THRESHOLD)
        s_open_right_count++;
      else
        s_open_right_count = 0;

      if (rpi.nav_left < NAV_OPEN_THRESHOLD)
        s_open_left_count++;
      else
        s_open_left_count = 0;

      if (s_open_right_count >= DIR_DETECT_COUNT) {
        s_turn_dir_deg = +90.0f;
        s_corner_count = 0;
        s_state = STATE_FORWARD;
      } else if (s_open_left_count >= DIR_DETECT_COUNT) {
        s_turn_dir_deg = -90.0f;
        s_corner_count = 0;
        s_state = STATE_FORWARD;
      }
      break;

    // -----------------------------------------------------------------
    case STATE_FORWARD: {
      float travelled_mm = encoder_get_distance_mm();

      forward_hold(s_heading);

      bool past_min = travelled_mm >= MIN_FORWARD_MM_BEFORE_CORNER_CHECK;

      bool outer_open = past_min && ((s_turn_dir_deg > 0.0f)
                                         ? (rpi.nav_right < NAV_OPEN_THRESHOLD)
                                         : (rpi.nav_left < NAV_OPEN_THRESHOLD));

      if (outer_open)
        s_corner_count++;
      else
        s_corner_count = 0;

      if (s_corner_count >= CORNER_DETECT_COUNT) {
        if (!s_first_segment_recorded) {
          s_first_segment_mm = travelled_mm;
          s_first_segment_recorded = true;
        }
        forward_hold_invalidate();
        ctrl_request_turn(wrap180(s_heading + s_turn_dir_deg));
        s_corner_count = 0;
        s_state = STATE_TURNING;
        break;
      }

      if (travelled_mm > MAX_SEGMENT_MM) {
        ctrl_stop();
        stop();
        forward_hold_invalidate();
        s_done = true;
        s_state = STATE_IDLE;
      }
      break;
    }

    // -----------------------------------------------------------------
    case STATE_TURNING:
      if (ctrl_turn_done()) {
        s_heading = wrap180(s_heading + s_turn_dir_deg);
        s_turn_count++;
        s_corner_count = 0;
        forward_hold_invalidate();
        encoder_reset();

        if (s_turn_count % TURNS_PER_LAP == 0)
          s_lap_count++;

        move_forward(DRIVE_SPEED);
        forward_hold(s_heading);

        if (s_lap_count >= TOTAL_LAPS) {
          s_done = true;
          s_state = STATE_FINISH;
        } else {
          s_state = STATE_FORWARD;
        }
      }
      break;

    // -----------------------------------------------------------------
    case STATE_FINISH: {
      forward_hold(s_heading);

      float target_mm = FINISH_SECTION_MM - s_first_segment_mm;
      if (target_mm < 0.0f)
        target_mm = 0.0f;

      if (encoder_get_distance_mm() >= target_mm) {
        ctrl_stop();
        stop();
        forward_hold_invalidate();
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
