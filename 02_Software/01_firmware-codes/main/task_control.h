#pragma once

#include "forward_control.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "turn_control.h"

#ifdef __cplusplus

#include <atomic>

extern std::atomic<bool> g_turn_requested;
extern std::atomic<bool> g_fwd_requested;
extern std::atomic<float> g_turn_target_deg;
extern std::atomic<float> g_fwd_target_deg;

extern "C" {
#endif

void control_task_start(turn_ctrl_t *turn, forward_ctrl_t *fwd,
                        SemaphoreHandle_t yaw_mtx, SemaphoreHandle_t tof_mtx);

/**
 * @brief Request a turn to a target heading.
 *        Safe to call from any task or the algorithm layer.
 *        The control task picks this up on its next cycle.
 */
void ctrl_request_turn(float target_deg);

/**
 * @brief Request forward driving while holding a heading.
 *        Safe to call from any task or the algorithm layer.
 */
void ctrl_request_forward(float heading_deg);

/**
 * @brief Stop all controllers immediately.
 */
void ctrl_stop(void);

#ifdef __cplusplus
}
#endif
