#include "task_tof.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/task.h"
#include <task_config.h>
#define TAG "TOF_TASK"

static ToF *s_tof = NULL;
static SemaphoreHandle_t s_mutex = NULL;
static tof_state_t g_state = {};

static void tof_write(uint16_t front, uint16_t left, uint16_t right) {
  if (!s_mutex)
    return;
  tof_state_t s;
  s.front_mm = front;
  s.left_mm = left;
  s.right_mm = right;
  s.wall_left = (left <= WALL_LEFT_MM);
  s.wall_front = (front <= WALL_FRONT_MM);
  s.wall_right = (right <= WALL_LEFT_MM);
  s.front_warning_wall = (front <= WALL_WARNING_MM);
  s.front_emergency_wall = (front <= WALL_EMERGENCY_MM);
  s.valid = true;
  s.ts_us = esp_timer_get_time();

  xSemaphoreTake(s_mutex, portMAX_DELAY);
  g_state = s;
  xSemaphoreGive(s_mutex);
}

bool tof_read(tof_state_t *out) {
  if (!s_mutex || !out)
    return false;
  xSemaphoreTake(s_mutex, portMAX_DELAY);
  *out = g_state;
  xSemaphoreGive(s_mutex);
  return true;
}

static void tof_task(void *arg) {
  (void)arg;
  TickType_t last = xTaskGetTickCount();
  TickType_t inc = pdMS_TO_TICKS(TOF_PERIOD_MS);
  if (inc == 0)
    inc = 1;

  while (true) {
    uint16_t front = 0;
    uint16_t left = 0;
    uint16_t right = 0;

    esp_err_t ef = s_tof->frontTof.readRangeMMFiltered(front);
    esp_err_t el = s_tof->leftTof.readRangeMMFiltered(left);
    esp_err_t er = s_tof->rightTof.readRangeMMFiltered(right);

    if (ef == ESP_OK && el == ESP_OK && er == ESP_OK) {
      tof_write(front, left, right);
    } else {
      ESP_LOGW(TAG, "ToF read failed: front=%d left=%d right=%d", ef, el, er);
    }

    vTaskDelayUntil(&last, inc);
  }
}

void tof_task_start(ToF *tof, SemaphoreHandle_t mutex) {
  s_tof = tof;
  s_mutex = mutex;
  xTaskCreate(tof_task, "tof_task", TOF_STACK_SIZE, NULL, TOF_PRIORITY, NULL);
}
