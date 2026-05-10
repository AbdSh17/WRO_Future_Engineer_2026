#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "imu_bno055.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the IMU task.
 *        Must be called after bno_setup() and after yaw_mutex is created.
 *
 * @param imu       Pointer to an already-initialized bno055_imu_t.
 * @param mutex     The yaw mutex created in main.
 */
void imu_task_start(bno055_imu_t *imu, SemaphoreHandle_t mutex);

/**
 * @brief Read the latest yaw estimate.
 *
 * @param[out] yaw_deg  Latest yaw angle in degrees ([-180, 180]).
 * @param[out] ok       True if the value is valid.
 * @param[out] age_us   Age of the sample in microseconds.
 */
void yaw_read(float *yaw_deg, bool *ok, int64_t *age_us);

/**
 * @brief Re-zero the IMU yaw reference.
 */
void yaw_zero(void);

#ifdef __cplusplus
}
#endif
