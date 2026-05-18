#include "task_imu.h"

#include <math.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/task.h"
#include <task_config.h>

#define TAG "IMU_TASK"

static bno055_imu_t *s_imu = NULL;
static SemaphoreHandle_t s_mutex = NULL;

static volatile float g_yaw = 0.0f;
static volatile bool g_yaw_valid = false;
static volatile int64_t g_yaw_us = 0;
static volatile bool g_zero_requested = false;

static void yaw_write(float yaw_deg) {
  if (!s_mutex)
    return;
  xSemaphoreTake(s_mutex, portMAX_DELAY);
  g_yaw = yaw_deg;
  g_yaw_valid = true;
  g_yaw_us = esp_timer_get_time();
  xSemaphoreGive(s_mutex);
}

void yaw_read(float *yaw_deg, bool *ok, int64_t *age_us) {
  if (!s_mutex || !yaw_deg || !ok || !age_us)
    return;
  int64_t now = esp_timer_get_time();
  xSemaphoreTake(s_mutex, portMAX_DELAY);
  float y = g_yaw;
  bool v = g_yaw_valid;
  int64_t t = g_yaw_us;
  xSemaphoreGive(s_mutex);
  *yaw_deg = y;
  *ok = v;
  *age_us = v ? (now - t) : INT64_MAX;
}

void yaw_zero(void) { g_zero_requested = true; }

static void imu_task(void *arg) {
  (void)arg;
  TickType_t last = xTaskGetTickCount();
  TickType_t inc = pdMS_TO_TICKS(IMU_PERIOD_MS);
  if (inc == 0)
    inc = 1;
  while (true) {
    if (g_zero_requested) {
      g_zero_requested = false;
      bno_zero_yaw(s_imu);
    }
    float yaw_rel = bno_read_yaw_rel_deg(s_imu);
    if (!isnan(yaw_rel)) {
      yaw_write(yaw_rel);
    } else {
      ESP_LOGW(TAG, "yaw read failed");
    }
    vTaskDelayUntil(&last, inc);
  }
}

void imu_task_start(bno055_imu_t *imu, SemaphoreHandle_t mutex) {
  s_imu = imu;
  s_mutex = mutex;
  xTaskCreatePinnedToCore(imu_task, "imu_task", IMU_STACK_SIZE, NULL,
                          IMU_PRIORITY, NULL, 1);
}
