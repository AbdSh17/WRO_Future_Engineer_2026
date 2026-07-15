#pragma once

#include "driver/gpio.h"
#include "driver/i2c.h"
#include "esp_err.h"
#include <stdint.h>

class VL53L0X {
public:
  VL53L0X(i2c_port_t port, uint8_t addr = 0x29);

  /**
   * @brief Initialize the sensor.
   *
   * @param long_range    Longer range (lower signal rate limit + longer VCSEL
   * periods).
   * @param high_speed    Shorter timing budget (~20ms) for faster continuous
   * measurements.
   * @param high_accuracy Longer timing budget (~50ms) for better stability.
   */
  esp_err_t begin(bool long_range = true, bool high_speed = true,
                  bool high_accuracy = false);

  // XSHUT helpers
  void shdnAttach(gpio_num_t shdn_pin);
  void shdnDown();
  void shdnUp(uint32_t delay_ms);

  // Address change (runtime only)
  esp_err_t setAddress(uint8_t new_addr);

  // Single-shot and continuous reading
  esp_err_t readRangeMM(uint16_t &mm);
  esp_err_t readRangeMMFiltered(uint16_t &mm);

  esp_err_t startContinuous(uint32_t period_ms = 25);
  esp_err_t stopContinuous();
  esp_err_t readRangeContinuousMM(uint16_t &mm);

  // Timing and signal settings
  esp_err_t setMeasurementTimingBudgetUs(uint32_t budget_us);
  esp_err_t getMeasurementTimingBudgetUs(uint32_t &budget_us);
  esp_err_t setSignalRateLimitMcps(float mcps);

private:
  i2c_port_t _port;
  uint8_t _addr;

  gpio_num_t _shdn = GPIO_NUM_NC;

  // Filter state (N=5)
  static constexpr int FILTER_N = 5;
  uint16_t _buf[FILTER_N] = {0};
  int _idx = 0;
  int _count = 0;

  // Cached init state
  uint8_t _stop_variable = 0;
  uint32_t _measurement_timing_budget_us = 33000;
  bool _did_init = false;

  // ---------- Low-level I2C helpers (VL53L0X uses 8-bit register addresses)
  // ----------
  esp_err_t write8(uint8_t reg, uint8_t val);
  esp_err_t write16(uint8_t reg, uint16_t val);
  esp_err_t writeMulti(uint8_t reg, const uint8_t *src, uint8_t count);

  esp_err_t read8(uint8_t reg, uint8_t &val);
  esp_err_t read16(uint8_t reg, uint16_t &val);
  esp_err_t readMulti(uint8_t reg, uint8_t *dst, uint8_t count);

  // ---------- Init + tuning ----------
  esp_err_t init_internal();
  esp_err_t getSpadInfo(uint8_t &count,
                        bool &type_is_aperture); // best-effort now
  esp_err_t performSingleRefCalibration(uint8_t vhv_init_byte);

  // VCSEL + timeouts
  enum vcselPeriodType { VcselPeriodPreRange, VcselPeriodFinalRange };
  esp_err_t setVcselPulsePeriod(vcselPeriodType type, uint8_t period_pclks);
  uint8_t getVcselPulsePeriod(vcselPeriodType type);

  esp_err_t getSequenceStepEnables(uint8_t &tcc, uint8_t &dss, uint8_t &msrc,
                                   uint8_t &pre_range, uint8_t &final_range);
  esp_err_t getSequenceStepTimeouts(uint8_t &pre_range_vcsel_period_pclks,
                                    uint16_t &msrc_dss_tcc_mclks,
                                    uint16_t &pre_range_mclks,
                                    uint16_t &final_range_mclks,
                                    uint32_t &msrc_dss_tcc_us,
                                    uint32_t &pre_range_us,
                                    uint32_t &final_range_us);

  uint16_t decodeTimeout(uint16_t reg_val);
  uint16_t encodeTimeout(uint16_t timeout_mclks);
  uint32_t timeoutMclksToMicroseconds(uint16_t timeout_period_mclks,
                                      uint8_t vcsel_period_pclks);
  uint32_t timeoutMicrosecondsToMclks(uint32_t timeout_period_us,
                                      uint8_t vcsel_period_pclks);

  // Range read helpers
  esp_err_t readRangeStatus(uint8_t &st);
  esp_err_t readRangeResult(uint16_t &mm);

  // Filter helpers
  static uint16_t medianN(const uint16_t *a, int n);
};
