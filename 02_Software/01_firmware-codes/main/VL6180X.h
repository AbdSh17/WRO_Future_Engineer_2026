#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "driver/i2c.h"

class VL6180X {
public:
    VL6180X(i2c_port_t port, uint8_t addr = 0x29);

    // Initializes device and loads recommended settings.
    esp_err_t begin();

    // Single-shot ranging (returns millimeters 0..255).
    esp_err_t readRangeMM(uint8_t &mm);

    // Filtered ranging (simple moving average).
    esp_err_t readRangeMMFiltered(uint8_t &mm);

    // Change I2C address (useful for multiple VL6180X on same bus).
    // NOTE: you must control XSHUT pins to bring up sensors one-by-one.
    esp_err_t setAddress(uint8_t new_addr);

    // Optional: change range convergence time (ms).
    esp_err_t setRangeConvergenceTime(uint8_t ms);


    void shdnAttach(gpio_num_t);
    void shdnDown();
    void shdnUp(uint32_t delay_ms);



private:
    i2c_port_t _port;
    uint8_t _addr;

    // Simple filter state
    static constexpr int FILTER_N = 3;
    uint16_t _buf[FILTER_N] = {0};
    int _idx = 0;
    int _count = 0;

    gpio_num_t _shdn = GPIO_NUM_NC;


    esp_err_t write8(uint16_t reg, uint8_t val);
    esp_err_t write16(uint16_t reg, uint16_t val);
    esp_err_t read8(uint16_t reg, uint8_t &val);

    esp_err_t waitFreshOutOfReset();
    esp_err_t loadRecommendedSettings();
    esp_err_t clearInterrupts();

};
