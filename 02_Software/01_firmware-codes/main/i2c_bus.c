#include "i2c_bus.h"
#include <string.h>

static esp_err_t bus_init(i2c_port_t port, gpio_num_t sda, gpio_num_t scl,
                          uint32_t clk_hz) {
  i2c_config_t conf;
  memset(&conf, 0, sizeof(conf));
  conf.mode = I2C_MODE_MASTER;
  conf.sda_io_num = sda;
  conf.scl_io_num = scl;
  conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
  conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
  conf.master.clk_speed = clk_hz;

  esp_err_t e = i2c_param_config(port, &conf);
  if (e != ESP_OK)
    return e;

  e = i2c_driver_install(port, I2C_MODE_MASTER, 0, 0, 0);
  if (e != ESP_OK)
    return e;

  i2c_set_timeout(port, 0xFFFFF);
  return ESP_OK;
}

esp_err_t i2c_setup(void) {
  esp_err_t e = bus_init(IMU_PORT, IMU_SDA, IMU_SCL, IMU_HZ);
  if (e != ESP_OK)
    return e;

  return bus_init(TOF_PORT, TOF_SDA, TOF_SCL, TOF_HZ);
}
