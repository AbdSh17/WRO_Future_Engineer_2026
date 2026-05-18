#include "forward_control.h"
#include "servo_control.h"

void forward_init(forward_ctrl_t *f) {
  f->target_deg = 0.0f;
  f->max_correction =
      45.0f; // tighter clamp than turning — small corrections only
  f->active = false;

  // ── PID ──────────────────────────────────────
  // conservative gains — servo is correcting small drift, not executing a turn
  f->pid.Kp = 2.5f;
  f->pid.Ki = 0.15f;
  f->pid.Kd = 0.02f;
  f->pid.prev_error = 0.0f;
  f->pid.integral = 0.0f;
  f->pid.out_min = -100.0f;
  f->pid.out_max = 100.0f;
  // ─────────────────────────────────────────────
}

void forward_start(forward_ctrl_t *f, float heading_deg) {
  f->pid.prev_error = 0.0f;
  f->pid.integral = 0.0f;
  f->target_deg = heading_deg;
  f->active = true;
}

// Call this every control loop tick while driving forward
void forward_update(forward_ctrl_t *f, float yaw_deg, float dt_s) {
  if (!f->active)
    return;

  // dt safety
  if (dt_s < 0.005f)
    dt_s = 0.005f;
  if (dt_s > 0.050f)
    dt_s = 0.050f;

  float cmd = pid_update(&f->pid, f->target_deg, yaw_deg, dt_s);

  // clamp — forward hold only needs small corrections
  if (cmd > f->max_correction)
    cmd = f->max_correction;
  if (cmd < -f->max_correction)
    cmd = -f->max_correction;

  set_angle((int)cmd);
}

void forward_stop(forward_ctrl_t *f) {
  set_angle(0);
  f->active = false;
}
