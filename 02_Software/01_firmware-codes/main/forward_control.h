#pragma once
#include "PID.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  PID_t pid;
  float target_deg;
  float max_correction; // servo clamp for straight-line hold
  bool active;
} forward_ctrl_t;

void forward_init(forward_ctrl_t *f);
void forward_start(forward_ctrl_t *f, float heading_deg);
void forward_update(forward_ctrl_t *f, float yaw_deg, float dt_s);
void forward_stop(forward_ctrl_t *f);

#ifdef __cplusplus
}
#endif
