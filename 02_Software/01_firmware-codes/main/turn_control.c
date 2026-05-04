#include "turn_control.h"
#include "motor_control.h"
#include "stdio.h"
#include <math.h>

static inline float wrap180(float a) {
  while (a >= 180.0f)
    a -= 360.0f;
  while (a < -180.0f)
    a += 360.0f;
  return a;
}

void turn_init(turn_ctrl_t *t) {

  t->min_speed = 20.0f;
  t->max_speed = 80.0f;

  t->target_deg = 0.0f;
  t->tol_deg = 2.0f;
  t->stable_need = 6;
  t->stable_count = 0;

  // ============ PID ============
  t->pid.Kp = 0.8f;
  t->pid.Ki = 0.075f;
  t->pid.Kd = 0.055f;

  t->pid.prev_error = 0.0f;
  t->pid.integral = 0.0f;

  t->pid.out_min = -100.0f;
  t->pid.out_max = +100.0f;
  // =============================

  t->running = false;
}

void turn_start_left(turn_ctrl_t *t, float angle_deg) {
  stop();
  t->pid.prev_error = 0.0f;
  t->pid.integral = 0.0f;
  t->stable_count = 0;

  t->target_deg = wrap180(angle_deg);
  t->running = true;
}

void turn_start_right(turn_ctrl_t *t, float angle_deg) {
  stop();
  t->pid.prev_error = 0.0f;
  t->pid.integral = 0.0f;
  t->stable_count = 0;

  t->target_deg = wrap180(angle_deg);
  t->running = true;
}

bool turn_update(turn_ctrl_t *t, float yaw_rel_deg, float dt_s) {
  if (!t->running)
    return true;

  // --- error with wrap ---
  float err = t->target_deg - yaw_rel_deg;

  if (err > 180.0f)
    err -= 360.0f;
  if (err < -180.0f)
    err += 360.0f;

  if (fabsf(err) <= t->tol_deg) {
    stop();
    t->stable_count++;
    if (t->stable_count >= t->stable_need) {
      t->running = false;
      return true;
    }
    return false;
  }
  t->stable_count = 0;

  // dt safety (important for D term)
  if (dt_s < 0.005f)
    dt_s = 0.005f;
  if (dt_s > 0.050f)
    dt_s = 0.050f;

  float cmd = pid_update(&t->pid, 0, -err, dt_s); // sign only

  float sp = fabsf(cmd);

  if (sp > t->max_speed) {
    sp = t->max_speed;
  }

  if (sp < t->min_speed) {
    sp = t->min_speed;
  }

  if (cmd >= 0.0f) {
    move_left_rotate((uint8_t)sp, (uint8_t)sp);
  }

  else {
    move_right_rotate((uint8_t)sp, (uint8_t)sp);
  }

  return false;
}

void turn_cancel(turn_ctrl_t *t) {
  stop();
  t->running = false;
  t->stable_count = 0;
}

void print_last_sample() {
  printf("yaw=%7.2f | err=%8.3f | out=%6.2f\n", debug_buf[debug_index].yaw,
         debug_buf[debug_index].err, debug_buf[debug_index].output);
}

void turn_debug_print(void) {
  printf("---- TURN DEBUG (%d samples) ----\n", debug_count);

  if (debug_count == 0)
    return;

  // Oldest index = head - count (wrapped)
  int start = debug_index - debug_count;
  if (start < 0)
    start += DEBUG_BUF_SIZE;

  for (int i = 0; i < debug_count; i++) {
    int idx = start + i;
    if (idx >= DEBUG_BUF_SIZE)
      idx -= DEBUG_BUF_SIZE;

    printf("%3d | yaw=%7.2f | err=%8.3f | out=%6.2f\n", i, debug_buf[idx].yaw,
           debug_buf[idx].err, debug_buf[idx].output);
  }

  printf("---------------------------------\n");
}

void reset_integral(turn_ctrl_t *t) { t->pid.integral = 0; }
