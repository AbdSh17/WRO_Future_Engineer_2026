#include "VL6180X.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// ---- Common VL6180X register map (subset) ----
static constexpr uint16_t REG_MODEL_ID = 0x000;
static constexpr uint16_t REG_SYSTEM_FRESH_OUT_OF_RESET = 0x016;

static constexpr uint16_t REG_SYSRANGE_START = 0x018;
static constexpr uint16_t REG_SYSRANGE_INTERMEASUREMENT_PERIOD = 0x01B;
static constexpr uint16_t REG_SYSRANGE_MAX_CONVERGENCE_TIME = 0x01C;

static constexpr uint16_t REG_SYSTEM_INTERRUPT_CONFIG_GPIO = 0x014;
static constexpr uint16_t REG_SYSTEM_INTERRUPT_CLEAR = 0x015;

static constexpr uint16_t REG_RESULT_RANGE_STATUS = 0x04D;
static constexpr uint16_t REG_RESULT_RANGE_VAL = 0x062;

// I2C address register (8-bit I2C address value, not shifted)
static constexpr uint16_t REG_I2C_SLAVE_DEVICE_ADDRESS = 0x212;

#define TOF1_SHDN GPIO_NUM_15
#define TOF2_SHDN GPIO_NUM_16
#define TOF3_SHDN GPIO_NUM_17

static constexpr uint8_t TOF_ADDR_DEFAULT = 0x29;
static constexpr uint8_t TOF1_ADDR_NEW = 0x30;
static constexpr uint8_t TOF2_ADDR_NEW = 0x31;
static constexpr uint8_t TOF3_ADDR_NEW = 0x32;

// Timing
static inline TickType_t ms_to_ticks(uint32_t ms) { return pdMS_TO_TICKS(ms); }

VL6180X::VL6180X(i2c_port_t port, uint8_t addr) : _port(port), _addr(addr) {}

esp_err_t VL6180X::write8(uint16_t reg, uint8_t val) {
  uint8_t buf[3] = {(uint8_t)(reg >> 8), (uint8_t)(reg & 0xFF), val};
  return i2c_master_write_to_device(_port, _addr, buf, sizeof(buf),
                                    ms_to_ticks(50));
}

esp_err_t VL6180X::write16(uint16_t reg, uint16_t val) {
  uint8_t buf[4] = {(uint8_t)(reg >> 8), (uint8_t)(reg & 0xFF),
                    (uint8_t)(val >> 8), (uint8_t)(val & 0xFF)};
  return i2c_master_write_to_device(_port, _addr, buf, sizeof(buf),
                                    ms_to_ticks(50));
}

esp_err_t VL6180X::read8(uint16_t reg, uint8_t &val) {
  uint8_t regbuf[2] = {(uint8_t)(reg >> 8), (uint8_t)(reg & 0xFF)};
  return i2c_master_write_read_device(_port, _addr, regbuf, sizeof(regbuf),
                                      &val, 1, ms_to_ticks(50));
}

esp_err_t VL6180X::waitFreshOutOfReset() {
  // Wait until SYSTEM__FRESH_OUT_OF_RESET becomes 1
  for (int i = 0; i < 1000; i++) {
    uint8_t v = 0;
    esp_err_t err = read8(REG_SYSTEM_FRESH_OUT_OF_RESET, v);
    if (err != ESP_OK)
      return err;
    if (v == 0x01)
      return ESP_OK;
    vTaskDelay(ms_to_ticks(5));
  }
  return ESP_ERR_TIMEOUT;
}

esp_err_t VL6180X::clearInterrupts() {
  // Clear range + ALS interrupts (common value 0x07)
  return write8(REG_SYSTEM_INTERRUPT_CLEAR, 0x07);
}

esp_err_t VL6180X::loadRecommendedSettings() {
  // This “private settings” block is the common baseline used by many ports.
  // If your sensor behaves oddly, this is the first place to compare vs ST API.
  esp_err_t err;

  err = write8(0x0207, 0x01);
  if (err)
    return err;
  err = write8(0x0208, 0x01);
  if (err)
    return err;
  err = write8(0x0096, 0x00);
  if (err)
    return err;
  err = write8(0x0097, 0xFD);
  if (err)
    return err;
  err = write8(0x00E3, 0x00);
  if (err)
    return err;
  err = write8(0x00E4, 0x04);
  if (err)
    return err;
  err = write8(0x00E5, 0x02);
  if (err)
    return err;
  err = write8(0x00E6, 0x01);
  if (err)
    return err;
  err = write8(0x00E7, 0x03);
  if (err)
    return err;
  err = write8(0x00F5, 0x02);
  if (err)
    return err;
  err = write8(0x00D9, 0x05);
  if (err)
    return err;
  err = write8(0x00DB, 0xCE);
  if (err)
    return err;
  err = write8(0x00DC, 0x03);
  if (err)
    return err;
  err = write8(0x00DD, 0xF8);
  if (err)
    return err;
  err = write8(0x009F, 0x00);
  if (err)
    return err;
  err = write8(0x00A3, 0x3C);
  if (err)
    return err;
  err = write8(0x00B7, 0x00);
  if (err)
    return err;
  err = write8(0x00BB, 0x3C);
  if (err)
    return err;
  err = write8(0x00B2, 0x09);
  if (err)
    return err;
  err = write8(0x00CA, 0x09);
  if (err)
    return err;
  err = write8(0x0198, 0x01);
  if (err)
    return err;
  err = write8(0x01B0, 0x17);
  if (err)
    return err;
  err = write8(0x01AD, 0x00);
  if (err)
    return err;
  err = write8(0x00FF, 0x05);
  if (err)
    return err;
  err = write8(0x0100, 0x05);
  if (err)
    return err;
  err = write8(0x0199, 0x05);
  if (err)
    return err;
  err = write8(0x01A6, 0x1B);
  if (err)
    return err;
  err = write8(0x01AC, 0x3E);
  if (err)
    return err;
  err = write8(0x01A7, 0x1F);
  if (err)
    return err;
  err = write8(0x0030, 0x00);
  if (err)
    return err;

  // Interrupt: "new sample ready" on GPIO (range)
  err = write8(REG_SYSTEM_INTERRUPT_CONFIG_GPIO, 0x04);
  if (err)
    return err;

  // Inter-measurement period (ms) - OK for single-shot too
  err = write8(REG_SYSRANGE_INTERMEASUREMENT_PERIOD, 0x09);
  if (err)
    return err;

  // Convergence time (ms): 0x30 is a common default (0x30 → 48 ms)
  // How long the VL6180X keeps measuring and refining the distance before it
  // decides “this result is good enough.”
  err = write8(REG_SYSRANGE_MAX_CONVERGENCE_TIME, 0x30);
  if (err)
    return err;

  return ESP_OK;
}

esp_err_t VL6180X::begin() {
  // ---- Optional SHDN handling (only if you attached a pin) ----
  if (_shdn != GPIO_NUM_NC) {
    printf("\n==========================================================\n");
    printf("\nEnter - _shdn != GPIO_NUM_NC -\n");
    printf("\n==========================================================\n");

    // Make sure pin is configured (safe even if already configured)
    gpio_set_direction(_shdn, GPIO_MODE_OUTPUT);

    // Power-cycle to guarantee a clean reset and "fresh out of reset" behavior
    gpio_set_level(_shdn, 0);
    vTaskDelay(pdMS_TO_TICKS(50));
    gpio_set_level(_shdn, 1);

    // Give the sensor time to boot
    vTaskDelay(pdMS_TO_TICKS(50));

    printf("\n==========================================================\n");
    printf("\nEnd - _shdn != GPIO_NUM_NC -\n");
    printf("\n==========================================================\n");
  }

  // Optional sanity check (MODEL_ID is typically 0xB4)
  // uint8_t id = 0; ESP_ERROR_CHECK(read8(REG_MODEL_ID, id));

  esp_err_t err = waitFreshOutOfReset();
  if (err != ESP_OK)
    return err;

  printf("\n==========================================================\n");
  printf("\nEnter - A -\n");
  printf("\n==========================================================\n");

  err = loadRecommendedSettings();
  if (err != ESP_OK)
    return err;

  printf("\n==========================================================\n");
  printf("\nEnter - B -\n");
  printf("\n==========================================================\n");

  // Clear the fresh-out-of-reset flag
  err = write8(REG_SYSTEM_FRESH_OUT_OF_RESET, 0x00);
  if (err != ESP_OK)
    return err;

  printf("\n==========================================================\n");
  printf("\nEnter - C -\n");
  printf("\n==========================================================\n");

  // Clear any pending interrupts
  return clearInterrupts();
}

esp_err_t VL6180X::setRangeConvergenceTime(uint8_t ms) {
  return write8(REG_SYSRANGE_MAX_CONVERGENCE_TIME, ms);
}

esp_err_t VL6180X::setAddress(uint8_t new_addr) {
  // Write new address into device
  esp_err_t err = write8(REG_I2C_SLAVE_DEVICE_ADDRESS, new_addr);
  if (err != ESP_OK)
    return err;
  _addr = new_addr;
  return ESP_OK;
}

esp_err_t VL6180X::readRangeMM(uint8_t &mm) {
  // Start single-shot ranging
  esp_err_t err = write8(REG_SYSRANGE_START, 0x01);
  if (err != ESP_OK)
    return err;

  // Wait for "sample ready"
  for (int i = 0; i < 60; i++) {
    uint8_t st = 0;
    err = read8(REG_RESULT_RANGE_STATUS, st);
    if (err != ESP_OK)
      return err;

    // Common check: bit2 indicates "new sample ready" in many ports.
    if (st & 0x04)
      break;

    vTaskDelay(ms_to_ticks(5));
  }

  // Read range
  err = read8(REG_RESULT_RANGE_VAL, mm);
  if (err != ESP_OK)
    return err;

  // Clear interrupt for next reading
  return clearInterrupts();
}

esp_err_t VL6180X::readRangeMMFiltered(uint8_t &mm) {
  uint8_t raw = 0;
  esp_err_t err = readRangeMM(raw);
  if (err != ESP_OK) {
    mm = 255;
    return err;
  }

  _buf[_idx] = raw;
  _idx = (_idx + 1) % FILTER_N;
  if (_count < FILTER_N)
    _count++;

  uint32_t sum = 0;
  for (int i = 0; i < _count; i++)
    sum += _buf[i];

  mm = (uint8_t)(sum / (uint32_t)_count);
  return ESP_OK;
}

// Adjust the address
void VL6180X::shdnAttach(gpio_num_t shdn_pin) { _shdn = shdn_pin; }

void VL6180X::shdnDown() {
  if (_shdn == GPIO_NUM_NC)
    return;
  gpio_set_level(_shdn, 0);
}

void VL6180X::shdnUp(uint32_t delay_ms) {
  if (_shdn == GPIO_NUM_NC)
    return;
  gpio_set_level(_shdn, 1);
  if (delay_ms)
    vTaskDelay(pdMS_TO_TICKS(delay_ms));
}
