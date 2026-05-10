#include "imu_bno055.h"

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// -------------------- Config (matches your working example)
// --------------------
#define I2C_TIMEOUT_MS 1000

// Registers
#define REG_CHIP_ID 0x00
#define REG_OPR_MODE 0x3D
#define REG_PWR_MODE 0x3E
#define REG_UNIT_SEL 0x3B
#define REG_EULER_H_LSB 0x1A

// Values
#define CHIP_ID_OK 0xA0

#define MODE_CONFIG 0x00
#define MODE_IMUPLUS 0x08
#define MODE_NDOF 0x0C

#define PWR_NORMAL 0x00

// -------------------- Low-level I2C helpers --------------------
static esp_err_t i2c_write8(i2c_port_t port, uint8_t addr, uint8_t reg,
                            uint8_t val) {
  uint8_t buf[2] = {reg, val};
  return i2c_master_write_to_device(port, addr, buf, sizeof(buf),
                                    pdMS_TO_TICKS(I2C_TIMEOUT_MS));
}

static esp_err_t i2c_read_bytes(i2c_port_t port, uint8_t addr, uint8_t reg,
                                uint8_t *data, size_t len) {
  return i2c_master_write_read_device(port, addr, &reg, 1, data, len,
                                      pdMS_TO_TICKS(I2C_TIMEOUT_MS));
}

static esp_err_t set_mode(bno055_imu_t *imu, uint8_t mode) {
  esp_err_t e = i2c_write8(imu->port, imu->addr, REG_OPR_MODE, mode);
  vTaskDelay(pdMS_TO_TICKS(25));
  return e;
}

// -------------------- Angle helpers --------------------
static float wrap180(float deg) {
  while (deg > 180.0f)
    deg -= 360.0f;
  while (deg < -180.0f)
    deg += 360.0f;
  return deg;
}

static float deg2rad(float d) { return d * (float)M_PI / 180.0f; }
static float rad2deg(float r) { return r * 180.0f / (float)M_PI; }

// -------------------- Filter (circular mean) --------------------
static void filter_reset(bno055_imu_t *imu) {
  imu->buf_count = 0;
  imu->buf_idx = 0;
  for (int i = 0; i < 5; i++)
    imu->buf_deg[i] = 0.0f;
}

static void filter_push(bno055_imu_t *imu, float rel_deg) {
  imu->buf_deg[imu->buf_idx] = rel_deg;
  imu->buf_idx = (imu->buf_idx + 1) % 5;
  if (imu->buf_count < 5)
    imu->buf_count++;
}

static float filter_circular_mean(const bno055_imu_t *imu) {
  if (imu->buf_count <= 0)
    return NAN;

  float sum_s = 0.0f;
  float sum_c = 0.0f;

  for (int i = 0; i < imu->buf_count; i++) {
    float a = deg2rad(imu->buf_deg[i]);
    sum_s += sinf(a);
    sum_c += cosf(a);
  }

  return wrap180(rad2deg(atan2f(sum_s, sum_c)));
}

// -------------------- Public API --------------------

esp_err_t bno_setup(bno055_imu_t *imu, i2c_port_t port, uint8_t addr) {
  if (!imu)
    return ESP_ERR_INVALID_ARG;

  memset(imu, 0, sizeof(*imu));
  imu->port = port;
  imu->addr = addr;
  imu->ready = false;
  imu->offset_valid = false;
  imu->offset_deg = 0.0f;
  filter_reset(imu);

  // Like your working code: wait for boot
  vTaskDelay(pdMS_TO_TICKS(1200));

  // Verify chip id (retry a bit for clones)
  uint8_t id = 0xFF;
  esp_err_t e = ESP_FAIL;

  for (int i = 0; i < 15; i++) {
    e = i2c_read_bytes(port, addr, REG_CHIP_ID, &id, 1);
    if (e == ESP_OK && id == CHIP_ID_OK)
      break;
    vTaskDelay(pdMS_TO_TICKS(100));
  }

  if (e != ESP_OK)
    return e;
  if (id != CHIP_ID_OK)
    return ESP_FAIL;

  // CONFIG mode
  e = set_mode(imu, MODE_CONFIG);
  if (e != ESP_OK)
    return e;

  // Normal power mode
  e = i2c_write8(port, addr, REG_PWR_MODE, PWR_NORMAL);
  if (e != ESP_OK)
    return e;
  vTaskDelay(pdMS_TO_TICKS(10));

  // Units (degrees) - explicit but harmless
  e = i2c_write8(port, addr, REG_UNIT_SEL, 0x00);
  if (e != ESP_OK)
    return e;

  e = set_mode(imu, MODE_IMUPLUS);
  if (e != ESP_OK)
    return e;

  imu->ready = true;
  return ESP_OK;
}

bool bno_available(const bno055_imu_t *imu) { return (imu && imu->ready); }

float bno_read_yaw_abs_deg(const bno055_imu_t *imu) {
  if (!imu || !imu->ready)
    return NAN;

  uint8_t eul[2] = {0, 0};
  if (i2c_read_bytes(imu->port, imu->addr, REG_EULER_H_LSB, eul, 2) != ESP_OK)
    return NAN;

  int16_t raw = (int16_t)((eul[1] << 8) | eul[0]); // 1/16 deg
  return ((float)raw) / 16.0f;
}

void bno_zero_yaw(bno055_imu_t *imu) {
  if (!imu || !imu->ready)
    return;

  float abs = bno_read_yaw_abs_deg(imu);
  if (!isnan(abs)) {
    imu->offset_deg = abs;
    imu->offset_valid = true;
    filter_reset(imu);
    filter_push(imu, 0.0f);
  }
}

float bno_read_yaw_rel_deg(bno055_imu_t *imu) {
  if (!imu || !imu->ready)
    return NAN;

  float abs = bno_read_yaw_abs_deg(imu);
  if (isnan(abs))
    return NAN;

  // Capture offset on first valid read
  if (!imu->offset_valid) {
    imu->offset_deg = abs;
    imu->offset_valid = true;
    filter_reset(imu);
    filter_push(imu, 0.0f);
    return 0.0f;
  }

  float rel = wrap180(abs - imu->offset_deg);

  filter_push(imu, rel);
  return filter_circular_mean(imu);
}
