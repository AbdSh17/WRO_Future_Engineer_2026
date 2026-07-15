#pragma once

#include <stdint.h>

// ── Tune these to your hardware ──────────────────────────────────────────────
#define ENCODER_PIN GPIO_NUM_21
#define ENCODER_CPR 11 // Counts per motor revolution (hall effect)
#define GEAR_RATIO 30.0f
#define WHEEL_DIAMETER_MM 67.0f

#define ENCODER_TICKS_PER_REV (ENCODER_CPR * GEAR_RATIO)
#define ENCODER_MM_PER_TICK                                                    \
  ((3.14159f * WHEEL_DIAMETER_MM) / ENCODER_TICKS_PER_REV)
// ─────────────────────────────────────────────────────────────────────────────

typedef struct {
  volatile int32_t ticks;
} encoder_t;

extern encoder_t encoder;

void encoder_setup(void);
void encoder_reset(void);
int32_t encoder_get_ticks(void);
float encoder_get_distance_mm(void);
