#pragma once

#include "driver/gpio.h"
#include "driver/i2c.h"
#include "esp_err.h"

/* IMU bus */
#define IMU_PORT I2C_NUM_1
#define IMU_SDA GPIO_NUM_17
#define IMU_SCL GPIO_NUM_16
#define IMU_HZ 400000
#define IMU_ADDR 0x28

/* ToF bus */
#define TOF_PORT I2C_NUM_0
#define TOF_SDA GPIO_NUM_23
#define TOF_SCL GPIO_NUM_22
#define TOF_HZ 400000
#define TOF_ADDR 0x29

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize both I2C buses (IMU then ToF).
 *        Call once before any device setup.
 */
esp_err_t i2c_setup(void);

#ifdef __cplusplus
}
#endif
