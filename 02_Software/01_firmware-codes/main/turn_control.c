#include "turn_control.h"
#include "motor_control.h"
#include "servo_control.h"
#include <math.h>

void turn_init(turn_ctrl_t *t) {
  t->target_deg = 0.0f;
  t->tol_deg = 7.0f;
  t->stable_need = 6;
  t->stable_count = 0;
  t->max_steer_angle = 30.0f; // tune to your servo/chassis limit
  t->running = false;

  // ── PID ──────────────────────────────────────
  t->pid.Kp = 0.8f;
  t->pid.Ki = 0.075f;
  t->pid.Kd = 0.055f;
  t->pid.prev_error = 0.0f;
  t->pid.integral = 0.0f;
  t->pid.out_min = -100.0f;
  t->pid.out_max = 100.0f;
  // ─────────────────────────────────────────────
}

void turn_start(turn_ctrl_t *t, float target_deg) {
  t->pid.prev_error = 0.0f;
  t->pid.integral = 0.0f;
  t->stable_count = 0;
  t->target_deg = target_deg;
  t->running = true;
}

// Returns true when turn is complete
bool turn_update(turn_ctrl_t *t, float yaw_deg, float dt_s) {
  if (!t->running)
    return true;

  // dt safety
  if (dt_s < 0.005f)
    dt_s = 0.005f;
  if (dt_s > 0.050f)
    dt_s = 0.050f;

  float cmd = pid_update(&t->pid, t->target_deg, yaw_deg, dt_s);

  // clamp to servo physical limit
  if (cmd > t->max_steer_angle)
    cmd = t->max_steer_angle;
  if (cmd < -t->max_steer_angle)
    cmd = -t->max_steer_angle;

  set_angle((int)cmd);

  // check if settled within tolerance
  float err = t->target_deg - yaw_deg;
  if (err > 180.0f)
    err -= 360.0f;
  if (err < -180.0f)
    err += 360.0f;

  // If the error is bare logical, fix the steering
  if (fabsf(err) <= t->tol_deg) {
    t->stable_count++;
    if (t->stable_count >= t->stable_need) {
      set_angle(0);
      t->running = false;
      return true;
    }
  } else {
    t->stable_count = 0;
  }

  return false;
}

void turn_cancel(turn_ctrl_t *t) {
  set_angle(0);
  t->running = false;
  t->stable_count = 0;
}
