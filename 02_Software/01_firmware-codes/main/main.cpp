#include "ToF.h"
#include "esp_log.h"
#include "forward_control.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "imu_bno055.h"
#include "motor_control.h"
#include "turn_control.h"
#include <i2c_bus.h>
#include <task_imu.h>
#include <task_tof.h>

extern "C" {
#include "encoder.h"
#include "servo_control.h"
}

static bno055_imu_t imu;
static turn_ctrl_t g_turn;
static forward_ctrl_t g_fwd;

SemaphoreHandle_t yaw_mutex = nullptr;
SemaphoreHandle_t tof_mutex = nullptr;

static void init_motor(void) {
  tb6612_setup();
  stop();
}

static void init_servo(void) {
  servo_init();
  servo_center();
}

static void init_encoder(void) {
  encoder_setup();
  encoder_reset();
}

static void init_imu(void) {
  esp_err_t err = bno_setup(&imu, IMU_PORT, IMU_ADDR);
  if (err != ESP_OK) {
    ESP_LOGE("MAIN", "BNO055 init failed: %s", esp_err_to_name(err));
    while (true)
      vTaskDelay(pdMS_TO_TICKS(1000));
  }
  ESP_LOGI("MAIN", "BNO055 ready.");
}

static ToF tof(TOF_PORT, TOF_ADDR);
static void init_tof(void) { tof.tof_setup(); }

static void init_controllers(void) {
  turn_init(&g_turn);
  forward_init(&g_fwd);
}

static void init_mutexes(void) {
  yaw_mutex = xSemaphoreCreateMutex();
  tof_mutex = xSemaphoreCreateMutex();
  if (!yaw_mutex || !tof_mutex) {
    ESP_LOGE("MAIN", "Mutex creation failed");
    while (true)
      vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

extern "C" void app_main(void) {

  // ====== I2C =======
  esp_err_t err = i2c_setup();
  if (err != ESP_OK) {
    ESP_LOGE("MAIN", "I2C init failed: %s", esp_err_to_name(err));
    while (true)
      vTaskDelay(pdMS_TO_TICKS(1000));
  }

  // ====== Driving Logic ======
  init_motor();
  // ==============================

  // ====== Servo ======
  init_servo();
  // ==============================

  // ====== Encoder ======
  init_encoder();
  // ==============================

  // ====== IMU ======
  init_imu();
  // ==============================

  // ====== ToF ======
  init_tof();
  // ==============================

  // ====== Mutexes ======
  init_mutexes();
  // ==============================

  // ====== Control Logic ======
  init_controllers();
  // ==============================

  // ====== Start Tasks =========
  tof_task_start(&tof, tof_mutex);
  imu_task_start(&imu, yaw_mutex);

  // ============================

  while (true) {
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}
