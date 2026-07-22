#pragma once

#include "forward_control.h"
#include "turn_control.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Start the UDP PID tuner task.
 *        Listens on UDP_TUNER_PORT (5006) for tuning commands.
 *        Holds references to the live forward and turn controllers
 *        so it can update gains in-place without reflashing.
 *
 * @param fwd   Pointer to the live forward_ctrl_t in app_main
 * @param turn  Pointer to the live turn_ctrl_t in app_main
 */
void pid_tuner_start(forward_ctrl_t *fwd, turn_ctrl_t *turn);

/**
 * @brief Returns true if the tuner has received a "start" command.
 *        app_main while-loop gates on this to start/stop the robot.
 */
bool pid_tuner_running(void);

#ifdef __cplusplus
}
#endif
