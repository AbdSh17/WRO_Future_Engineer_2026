#pragma once

#include <math.h>
#include <stdbool.h>
#include <stdint.h>

#include "driver/gpio.h"
#include "driver/i2c.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  i2c_port_t port;
  uint8_t addr;
  bool ready;

  // relative yaw
  bool offset_valid;
  float offset_deg;

  // circular-mean filter buffer (size 5)
  float buf_deg[5];
  int buf_count; // 0..5
  int buf_idx;   // 0..4
} bno055_imu_t;

/**
 * @brief Initialize BNO055 (NDOF) - clone friendly, no calibration gating.
 * @return ESP_OK if sensor responds and mode set succeeded.
 */
esp_err_t bno_setup(bno055_imu_t *imu, i2c_port_t port, uint8_t addr);

/**
 * @brief True if setup succeeded.
 */
bool bno_available(const bno055_imu_t *imu);

/**
 * @brief Read absolute yaw (deg) from Euler heading.
 * @return yaw in degrees, or NAN on read error.
 */
float bno_read_yaw_abs_deg(const bno055_imu_t *imu);

/**
 * @brief Read relative yaw (deg), wrapped [-180,180], filtered (buffer=5
 * circular mean). Auto-captures offset on first valid read (so first rel yaw ~
 * 0).
 * @return filtered relative yaw, or NAN on read error.
 */
float bno_read_yaw_rel_deg(bno055_imu_t *imu);

/**
 * @brief Re-zero relative yaw using current absolute yaw if readable; clears
 * filter.
 */
void bno_zero_yaw(bno055_imu_t *imu);

#ifdef __cplusplus
}
#endif
