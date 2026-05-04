#pragma once
#include "PID.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  PID_t pid;
  float target_deg;
  float tol_deg;
  int stable_need;
  int stable_count;
  float max_steer_angle; // servo clamp during turn
  bool running;
} turn_ctrl_t;

void turn_init(turn_ctrl_t *t);
void turn_start(turn_ctrl_t *t, float target_deg);
bool turn_update(turn_ctrl_t *t, float yaw_deg, float dt_s);
void turn_cancel(turn_ctrl_t *t);

#ifdef __cplusplus
}
#endif
