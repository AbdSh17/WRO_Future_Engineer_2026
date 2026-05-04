#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "esp_log.h"
#include "driver/i2c.h"

// Side sensors
#include "VL6180X.h"

// Front sensor
#include "VL53L0X.h"

class ToF
{
private:
    i2c_port_t _port;
    uint8_t    _addr_default;   // default boot address (usually 0x29)

    // Drives all XSHUT pins low at startup (implemented in ToF.cpp)
    void all_init_tofs_off();

    // Init helpers (implemented in ToF.cpp)
    esp_err_t init_side_tofs(VL6180X &right, VL6180X &left);
    esp_err_t init_front_tof(VL53L0X &front);

public:
    /**
     * @brief ToF manager ctor
     * @param port        ESP-IDF I2C port (I2C_NUM_0 / I2C_NUM_1)
     * @param addr_default default I2C address at boot (usually 0x29)
     */
    ToF(i2c_port_t port, uint8_t addr_default = 0x29);
    ~ToF();

    // Power control (XSHUT) for all sensors (implemented in ToF.cpp)
    void all_tofs_off();
    void all_tofs_on();

    // Sensors
    VL6180X rightTof;
    VL53L0X frontTof;
    VL6180X leftTof;

    /**
     * @brief Initialize all ToF sensors (XSHUT sequencing + begin + optional address set)
     */
    esp_err_t tof_setup();
};