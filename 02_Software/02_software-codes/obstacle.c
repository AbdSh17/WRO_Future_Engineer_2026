
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
// OBSTACLE CHALLENGE — camera-only algorithm
// ===========================================================================
//
// Sensor set for this challenge is deliberately narrow:
//
//   IMU     -> heading (owned by control task, read here only for telemetry)
//   Encoder -> distance travelled since the last turn
//   Camera  -> EVERYTHING else, delivered by the Pi as one line per frame:
//
//        NAV:L=<0-100>,R=<0-100>;OBST:<RED|GREEN|NONE>[,<dist>,<lat>]
//
//   NAV  = percentage of the left/right watch box filled with wall-coloured
//          (black) pixels.  HIGH = wall present.  LOW = wall receded => the
//          opening at a corner.  Note this is the INVERSE of the old side-ToF
//          logic, where a HIGH millimetre reading meant "open".
//
//   OBST = nearest confirmed pillar: colour, distance (mm) and lateral offset
//          from the robot centreline (mm, positive = right of centre).
//
// No ToF is used here at all.  There is therefore NO forward-facing emergency
// stop: if the camera link dies the robot is blind, so the only safe response
// is to stop the drive motor and wait for fresh frames.  That behaviour is
// implemented in `link_is_fresh` handling below.
//
// ---------------------------------------------------------------------------
// Rule being implemented (WRO 2026 Future Engineers, rule 9.19):
//
//     "The vehicle must pass the traffic sign represented by the red pillar
//      on the right and the traffic sign represented by the green pillar on
//      the left."
//
// Read from the vehicle's point of view, that means:
//
//     RED   -> the vehicle drives to the RIGHT of the pillar,
//              so the pillar ends up on the vehicle's LEFT   (target lat < 0)
//     GREEN -> the vehicle drives to the LEFT of the pillar,
//              so the pillar ends up on the vehicle's RIGHT  (target lat > 0)
//
// ===========================================================================

// ---------------------------------------------------------------------------
// Wall / corner detection (camera NAV channel)
// ---------------------------------------------------------------------------
// CALIBRATION REQUIRED. Run the Pi vision script beside a real wall and at a
// real corner opening, note the two NAV values, and put the threshold between
// them with margin on both sides.
#define NAV_OPEN_THRESHOLD 25 // below this the wall is considered gone

#define DIR_DETECT_COUNT 3    // frames needed to confirm the driving direction
#define CORNER_DETECT_COUNT 3 // frames needed to confirm a corner opening

// Do not even look for a corner until the robot has driven this far past the
// previous turn. Without it the robot re-triggers on the same opening it just
// turned into and spins on the spot.
#define MIN_FORWARD_MM_BEFORE_CORNER_CHECK 150.0f

// Sanity bound: the longest straight on the field is ~3000 mm. If the encoder
// passes this without a corner, either the camera is mis-calibrated or the
// robot is stuck. Stop rather than keep driving blind.
#define MAX_SEGMENT_MM 5000.0f

// ---------------------------------------------------------------------------
// Camera link health
// ---------------------------------------------------------------------------
// The camera is the only exteroceptive sensor. Stale data is not "slightly
// old", it is "no data" — hold position instead of acting on it.
#define RPI_STALE_US (500 * 1000) // 500 ms

// ---------------------------------------------------------------------------
// Pillar avoidance
// ---------------------------------------------------------------------------
// Engage when the pillar is closer than this. Chosen so the robot has room to
// build lateral offset gradually rather than swerving.
#define AVOID_ENGAGE_MM 700

// Lateral clearance we aim for between the robot centreline and the pillar at
// the moment of passing. Pillar is 50 mm wide, chassis is ~100 mm wide, so
// ~180 mm of centre-to-centre offset clears it with margin.
#define AVOID_CLEARANCE_MM 180.0f

// Bias = AVOID_GAIN * (angle to the desired passing point). A gain of 1.0 is
// pure pursuit of that point; below 1.0 damps the response. Start ~0.9.
#define AVOID_GAIN 0.9f

// Hard clamp on how far the heading target may be biased away from the lane
// heading. Too large and the robot aims itself at a wall.
#define AVOID_MAX_BIAS_DEG 35.0f

// Once committed to a side, hold the bias until the robot has driven the
// distance that was between it and the pillar plus this margin. This is what
// keeps the manoeuvre stable after the pillar leaves the camera's view.
#define AVOID_COMMIT_MARGIN_MM 220.0f

// Ignore pillars whose reported distance is obviously wrong (bad contour, bad
// focal-length calibration). Filters garbage before it reaches the steering.
#define AVOID_MIN_VALID_MM 60
#define AVOID_MAX_VALID_MM 3000

// ---------------------------------------------------------------------------
// Drive speeds (0-100)
// ---------------------------------------------------------------------------
#define DRIVE_SPEED_CRUISE 90
#define DRIVE_SPEED_AVOID 35

// ---------------------------------------------------------------------------
// Course
// ---------------------------------------------------------------------------
#define TOTAL_LAPS 3
#define TURNS_PER_LAP 4
#define FINISH_SECTION_MM 1000.0f

// Re-issuing a forward request resets the PID integral, so only re-issue when
// the target has actually moved by more than this.
#define FWD_RETARGET_DEADBAND_DEG 0.75f

// ---------------------------------------------------------------------------
// State machine
// ---------------------------------------------------------------------------
typedef enum {
  STATE_IDLE,
  STATE_DETECT_DIR, // drive straight until one side wall opens
  STATE_FORWARD,    // drive, hold heading, bias around pillars
  STATE_TURNING,    // corner turn in progress
  STATE_FINISH,     // final positioning inside the start section
} alg_state_t;

static alg_state_t s_state = STATE_IDLE;

// ---------------------------------------------------------------------------
// Runtime state
// ---------------------------------------------------------------------------
static int s_turn_count = 0;
static int s_lap_count = 0;
static float s_heading = 0.0f;      // lane heading held between turns (deg)
static float s_turn_dir_deg = 0.0f; // +90 = clockwise, -90 = counter-clockwise
static bool s_done = false;

static int s_open_right_count = 0;
static int s_open_left_count = 0;
static int s_corner_count = 0;

static float s_first_segment_mm = 0.0f;
static bool s_first_segment_recorded = false;

// Forward-request de-duplication
static float s_last_fwd_cmd_deg = 0.0f;
static bool s_last_fwd_cmd_valid = false;

// Pillar avoidance latch
static bool s_avoid_active = false;
static obst_color_t s_avoid_color = OBST_NONE;
static float s_avoid_bias_deg = 0.0f;
static float s_avoid_release_mm = 0.0f; // encoder value at which to let go

// Camera link
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

static const char *obst_color_name(obst_color_t c) {
  switch (c) {
  case OBST_RED:
    return "RED";
  case OBST_GREEN:
    return "GREEN";
  default:
    return "NONE";
  }
}

// Request a forward heading only when it has meaningfully changed. Calling
// ctrl_request_forward() every cycle would reset the PID integral every cycle
// and the controller would never accumulate.
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

static void avoid_clear(void) {
  s_avoid_active = false;
  s_avoid_color = OBST_NONE;
  s_avoid_bias_deg = 0.0f;
  s_avoid_release_mm = 0.0f;
}

// ---------------------------------------------------------------------------
// Pillar avoidance geometry
// ---------------------------------------------------------------------------
// Returns the heading offset, in degrees, that aims the robot at a point
// AVOID_CLEARANCE_MM to the correct side of the pillar.
//
//   target_lat  where we want the pillar to sit relative to our centreline
//   error       how far the pillar is from that position, in mm
//   angle       atan(error / distance) — the heading change that closes it
//
// Sign convention: positive lateral = pillar is to our right; positive
// heading = steer right. Steering right pushes the pillar leftwards in view,
// hence the negation.
static float avoid_bias_for(obst_color_t color, int distance_mm,
                            int lateral_mm) {
  float target_lat_mm;

  if (color == OBST_RED) {
    // Pass on the right of the pillar => pillar must end up on our LEFT.
    target_lat_mm = -AVOID_CLEARANCE_MM;
  } else if (color == OBST_GREEN) {
    // Pass on the left of the pillar => pillar must end up on our RIGHT.
    target_lat_mm = +AVOID_CLEARANCE_MM;
  } else {
    return 0.0f;
  }

  float d = (float)distance_mm;
  if (d < 1.0f)
    d = 1.0f;

  float error_mm = target_lat_mm - (float)lateral_mm;
  float bias_deg = -AVOID_GAIN * atan2f(error_mm, d) * (180.0f / (float)M_PI);

  if (bias_deg > AVOID_MAX_BIAS_DEG)
    bias_deg = AVOID_MAX_BIAS_DEG;
  if (bias_deg < -AVOID_MAX_BIAS_DEG)
    bias_deg = -AVOID_MAX_BIAS_DEG;

  return bias_deg;
}

static bool pillar_is_usable(const rpi_state_t *r) {
  return r->obst_color != OBST_NONE &&
         r->obst_distance_mm >= AVOID_MIN_VALID_MM &&
         r->obst_distance_mm <= AVOID_MAX_VALID_MM;
}

// ---------------------------------------------------------------------------
// Telemetry
// ---------------------------------------------------------------------------
static void send_telemetry(const rpi_state_t *rpi, bool link_fresh,
                           float yaw_deg, bool yaw_ok) {
  if (!wifi_telemetry_is_connected())
    return;

  char buf[384];
  int n = 0;

  n += snprintf(buf + n, sizeof(buf) - n, "{");

  if (yaw_ok)
    n += snprintf(buf + n, sizeof(buf) - n, "\"imu\":{\"yaw\":%.2f},", yaw_deg);

  n += snprintf(buf + n, sizeof(buf) - n, "\"enc\":{\"mm\":%d},",
                (int)encoder_get_distance_mm());

  n += snprintf(buf + n, sizeof(buf) - n, "\"link\":%d,", link_fresh ? 1 : 0);

  if (link_fresh) {
    n += snprintf(buf + n, sizeof(buf) - n, "\"nav\":{\"L\":%d,\"R\":%d},",
                  rpi->nav_left, rpi->nav_right);
    n += snprintf(buf + n, sizeof(buf) - n,
                  "\"obst\":{\"color\":\"%s\",\"dist\":%d,\"lat\":%d},",
                  obst_color_name(rpi->obst_color), rpi->obst_distance_mm,
                  rpi->obst_lateral_mm);
  }

  n += snprintf(buf + n, sizeof(buf) - n,
                "\"avoid\":{\"act\":%d,\"col\":\"%s\",\"bias\":%.1f},",
                s_avoid_active ? 1 : 0, obst_color_name(s_avoid_color),
                s_avoid_bias_deg);

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
    // The camera is the only sensor that can see the world. Without it the
    // robot must not keep driving. This runs above the state machine so it
    // applies in every moving state.
    if (!link_fresh && s_state != STATE_IDLE && s_state != STATE_TURNING) {
      if (!s_link_lost) {
        s_link_lost = true;
        stop();
        avoid_clear();
        forward_hold_invalidate();
      }
      vTaskDelayUntil(&last, inc);
      continue;
    }

    if (s_link_lost && link_fresh) {
      // Link came back. Resume driving on the heading we were holding.
      s_link_lost = false;
      forward_hold_invalidate();
      if (s_state == STATE_FORWARD || s_state == STATE_DETECT_DIR ||
          s_state == STATE_FINISH) {
        move_forward(DRIVE_SPEED_CRUISE);
        forward_hold(s_heading);
      }
    }

    // ---- state machine ---------------------------------------------------
    switch (s_state) {

    // ---------------------------------------------------------------------
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
      avoid_clear();
      forward_hold_invalidate();
      encoder_reset();

      // g_turn_done is a latched one-shot flag inside the control task. If a
      // previous run left it set, the first turn of this run would report
      // complete on its first poll. Drain it before starting.
      (void)ctrl_turn_done();

      move_forward(DRIVE_SPEED_CRUISE);
      forward_hold(s_heading);
      s_state = STATE_DETECT_DIR;
      break;

    // ---------------------------------------------------------------------
    // Direction is unknown at the start of a round: the robot is placed in a
    // random section facing either way. Drive straight until one side wall
    // recedes — that side is the outside of the first corner, and it fixes
    // the turn direction for the whole round.
    //
    // Pillar avoidance is intentionally NOT applied here. Steering off the
    // lane heading before the direction is known would corrupt the very
    // measurement being taken.
    // ---------------------------------------------------------------------
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
        s_turn_dir_deg = +90.0f; // clockwise
        s_corner_count = 0;
        s_state = STATE_FORWARD;
      } else if (s_open_left_count >= DIR_DETECT_COUNT) {
        s_turn_dir_deg = -90.0f; // counter-clockwise
        s_corner_count = 0;
        s_state = STATE_FORWARD;
      }
      break;

    // ---------------------------------------------------------------------
    // Straight running. Two concerns, in priority order:
    //
    //   1. Pillar avoidance — bias the heading target so the pillar is passed
    //      on the side the rules require.
    //   2. Corner detection — watch the outer wall for the opening.
    //
    // Corner detection is suppressed while a pillar manoeuvre is in progress:
    // the robot is at an angle to the lane, so the watch boxes are pointing
    // somewhere other than where they were calibrated to point, and a false
    // corner there would be unrecoverable.
    // ---------------------------------------------------------------------
    case STATE_FORWARD: {
      float travelled_mm = encoder_get_distance_mm();

      // ---- 1. pillar avoidance ------------------------------------------
      if (s_avoid_active && travelled_mm >= s_avoid_release_mm) {
        // Committed distance is used up: the pillar is behind us.
        avoid_clear();
        move_forward(DRIVE_SPEED_CRUISE);
      }

      if (pillar_is_usable(&rpi) && rpi.obst_distance_mm <= AVOID_ENGAGE_MM) {
        s_avoid_bias_deg = avoid_bias_for(rpi.obst_color, rpi.obst_distance_mm,
                                          rpi.obst_lateral_mm);

        // Commit: hold this manoeuvre until we have driven past the pillar
        // even if it leaves the frame (it will — it passes out of the FOV
        // well before the robot is level with it).
        float release =
            travelled_mm + (float)rpi.obst_distance_mm + AVOID_COMMIT_MARGIN_MM;

        if (!s_avoid_active || release > s_avoid_release_mm)
          s_avoid_release_mm = release;

        if (!s_avoid_active) {
          s_avoid_active = true;
          move_forward(DRIVE_SPEED_AVOID);
        }
        s_avoid_color = rpi.obst_color;
      }

      forward_hold(wrap180(s_heading + s_avoid_bias_deg));

      // ---- 2. corner detection ------------------------------------------
      bool past_min_distance =
          travelled_mm >= MIN_FORWARD_MM_BEFORE_CORNER_CHECK;

      bool outer_open =
          past_min_distance && !s_avoid_active &&
          ((s_turn_dir_deg > 0.0f) ? (rpi.nav_right < NAV_OPEN_THRESHOLD)
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

        avoid_clear();
        forward_hold_invalidate();
        move_forward(DRIVE_SPEED_CRUISE);

        ctrl_request_turn(wrap180(s_heading + s_turn_dir_deg));
        s_corner_count = 0;
        s_state = STATE_TURNING;
        break;
      }

      // ---- safety bound --------------------------------------------------
      if (travelled_mm > MAX_SEGMENT_MM) {
        ctrl_stop();
        stop();
        avoid_clear();
        forward_hold_invalidate();
        s_done = true;
        s_state = STATE_IDLE;
      }
      break;
    }

    // ---------------------------------------------------------------------
    case STATE_TURNING:
      if (ctrl_turn_done()) {
        s_heading = wrap180(s_heading + s_turn_dir_deg);
        s_turn_count++;
        s_corner_count = 0;
        avoid_clear();
        forward_hold_invalidate();
        encoder_reset();

        if (s_turn_count % TURNS_PER_LAP == 0)
          s_lap_count++;

        move_forward(DRIVE_SPEED_CRUISE);
        forward_hold(s_heading);

        if (s_lap_count >= TOTAL_LAPS) {
          s_done = true;
          s_state = STATE_FINISH;
        } else {
          s_state = STATE_FORWARD;
        }
      }
      break;

    // ---------------------------------------------------------------------
    // Rule 9.22: the vehicle must return to the starting section after three
    // laps. The distance driven before the very first corner is how far into
    // the section the robot started, so driving the remainder of a section
    // puts it back where it began.
    //
    // Pillars are still avoided here — the last stretch is scored like any
    // other, and a pillar sitting in it still has to be passed correctly.
    // ---------------------------------------------------------------------
    case STATE_FINISH: {
      float travelled_mm = encoder_get_distance_mm();

      if (s_avoid_active && travelled_mm >= s_avoid_release_mm) {
        avoid_clear();
        move_forward(DRIVE_SPEED_CRUISE);
      }

      if (pillar_is_usable(&rpi) && rpi.obst_distance_mm <= AVOID_ENGAGE_MM) {
        s_avoid_bias_deg = avoid_bias_for(rpi.obst_color, rpi.obst_distance_mm,
                                          rpi.obst_lateral_mm);

        float release =
            travelled_mm + (float)rpi.obst_distance_mm + AVOID_COMMIT_MARGIN_MM;
        if (!s_avoid_active || release > s_avoid_release_mm)
          s_avoid_release_mm = release;

        if (!s_avoid_active) {
          s_avoid_active = true;
          move_forward(DRIVE_SPEED_AVOID);
        }
        s_avoid_color = rpi.obst_color;
      }

      forward_hold(wrap180(s_heading + s_avoid_bias_deg));

      float target_mm = FINISH_SECTION_MM - s_first_segment_mm;
      if (target_mm < 0.0f)
        target_mm = 0.0f;

      if (travelled_mm >= target_mm) {
        ctrl_stop();
        stop();
        avoid_clear();
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
