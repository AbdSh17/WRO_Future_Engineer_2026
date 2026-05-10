#include "task_algorithm.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "task_control.h"
#include "task_imu.h"
#include "task_tof.h"

#define ALG_STACK_SIZE 4096
#define ALG_PRIORITY 4 // lower than control and sensor tasks

// --- State machine -----------------------------------------------------------

typedef enum {
  STATE_IDLE,
  STATE_FORWARD,
  STATE_TURNING,
} alg_state_t;

static alg_state_t s_state = STATE_IDLE;

static void algorithm_task(void *arg) {
  (void)arg;

  while (true) {
    // --- read sensors ------------------------------------------------
    float yaw_deg = 0.0f;
    bool yaw_ok = false;
    int64_t yaw_age = 0;
    yaw_read(&yaw_deg, &yaw_ok, &yaw_age);

    tof_state_t tof = {};
    tof_read(&tof);

    // --- state machine -----------------------------------------------
    switch (s_state) {
    case STATE_IDLE:
      // TODO: wait for start signal
      break;

    case STATE_FORWARD:
      // TODO: monitor tof, decide when to turn
      break;

    case STATE_TURNING:
      // TODO: wait for turn to complete, transition to forward
      break;
    }

    vTaskDelay(pdMS_TO_TICKS(50)); // algorithm runs at 20 Hz
  }
}

void algorithm_task_start(void) {
  xTaskCreate(algorithm_task, "alg_task", ALG_STACK_SIZE, NULL, ALG_PRIORITY,
              NULL);
}
