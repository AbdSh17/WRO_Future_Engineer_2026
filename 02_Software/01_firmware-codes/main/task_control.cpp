#include "task_control.h"

#include <atomic>

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/task.h"
#include "motor_control.h"
#include "servo_control.h"
#include "task_imu.h"
#include "task_tof.h"
#include <task_config.h>

#define TAG "CTRL_TASK"

// --- Atomic command interface ------------------------------------------------
std::atomic<bool> g_turn_requested{false};
std::atomic<bool> g_fwd_requested{false};
std::atomic<bool> g_stop_requested{false};
std::atomic<float> g_turn_target_deg{0.0f};
std::atomic<float> g_fwd_target_deg{0.0f};
std::atomic<bool> g_turn_done{false};

// --- Controller handles (owned exclusively by control task) ------------------
static turn_ctrl_t *s_turn = NULL;
static forward_ctrl_t *s_fwd = NULL;

// --- Public command API ------------------------------------------------------

bool ctrl_turn_done(void) { return g_turn_done.exchange(false); }

void ctrl_request_turn(float target_deg) {
  g_fwd_requested.store(false);
  g_stop_requested.store(false);
  g_turn_target_deg.store(target_deg);
  g_turn_requested.store(true);
}

void ctrl_request_forward(float heading_deg) {
  g_turn_requested.store(false);
  g_stop_requested.store(false);
  g_fwd_target_deg.store(heading_deg);
  g_fwd_requested.store(true);
}

void ctrl_stop(void) {
  g_turn_requested.store(false);
  g_fwd_requested.store(false);
  g_stop_requested.store(true);
}

// --- Task --------------------------------------------------------------------

static void control_task(void *arg) {
  (void)arg;

  TickType_t last = xTaskGetTickCount();
  TickType_t inc = pdMS_TO_TICKS(CTRL_PERIOD_MS);
  if (inc == 0)
    inc = 1;

  int64_t prev_us = esp_timer_get_time();

  yaw_zero();

  while (true) {
    // --- dt ----------------------------------------------------------
    int64_t now_us = esp_timer_get_time();
    float dt_s = (now_us - prev_us) * 1e-6f;
    prev_us = now_us;

    // --- read sensors ------------------------------------------------
    float yaw_deg = 0.0f;
    bool yaw_ok = false;
    int64_t yaw_age = 0;
    yaw_read(&yaw_deg, &yaw_ok, &yaw_age);

    // tof_state_t tof = {};
    // tof_read(&tof);

    if (!yaw_ok) {
      vTaskDelayUntil(&last, inc);
      continue;
    }

    // --- consume commands --------------------------------------------
    if (g_stop_requested.exchange(false)) {
      turn_cancel(s_turn);
      forward_stop(s_fwd);
      stop();
      vTaskDelayUntil(&last, inc);
      continue;
    }

    if (g_turn_requested.exchange(false)) {
      turn_start(s_turn, g_turn_target_deg.load());
    }

    if (g_fwd_requested.exchange(false)) {
      forward_start(s_fwd, g_fwd_target_deg.load());
    }

    // --- run active controller ---------------------------------------
    if (s_turn->running) {
      bool done = turn_update(s_turn, yaw_deg, dt_s);
      if (done) {
        forward_start(s_fwd, yaw_deg);
        servo_center();
        g_turn_done.store(true);
        ESP_LOGI(TAG, "turn done, holding yaw=%.1f", yaw_deg);
      }
    }

    else if (s_fwd->active) {
      forward_update(s_fwd, yaw_deg, dt_s);
    }

    vTaskDelayUntil(&last, inc);
    taskYIELD();
  }
}

void control_task_start(turn_ctrl_t *turn, forward_ctrl_t *fwd,
                        SemaphoreHandle_t yaw_mtx, SemaphoreHandle_t tof_mtx) {
  (void)yaw_mtx;
  (void)tof_mtx;
  s_turn = turn;
  s_fwd = fwd;
  xTaskCreatePinnedToCore(control_task, "ctrl_task", CTRL_STACK_SIZE, NULL,
                          CTRL_PRIORITY, NULL, 0);
}
