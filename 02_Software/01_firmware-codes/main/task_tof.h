#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "ToF.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#define WALL_LEFT_MM 150
#define WALL_FRONT_MM 200
#define WALL_WARNING_MM 1000
#define WALL_EMERGENCY_MM 80

typedef struct {
  uint16_t left_mm;
  uint16_t front_mm;
  uint16_t right_mm;
  bool wall_left;
  bool wall_front;
  bool wall_right;
  bool front_emergency_wall;
  bool front_warning_wall;
  bool valid;
  int64_t ts_us;
} tof_state_t;

#ifdef __cplusplus
extern "C" {
#endif

void tof_task_start(ToF *tof, SemaphoreHandle_t mutex);

/**
 * @brief Read the latest ToF state.
 *
 * @param[out] out  Filled with the latest tof_state_t snapshot.
 * @return true if mutex is initialized and out is not NULL.
 */
bool tof_read(tof_state_t *out);

#ifdef __cplusplus
}
#endif
