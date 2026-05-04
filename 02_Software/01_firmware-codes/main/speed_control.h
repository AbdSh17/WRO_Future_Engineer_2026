#pragma once
#ifdef __cplusplus
extern "C"
{
#endif

#include <stdbool.h>
#include "PID.h"

#define DEFAULT_SPEED 40
#define WARNING_WALL_SPEED 25

#define DEFAULT_SPEED_MODE 0
#define WARNING_WALL_SPEED_MODE 1

    typedef struct
    {
        PID_t pid;

        int max_speed;
        int min_speed;
        int desired_speed; // the desired one

        bool status;
    } speed_ctrl_t;

    void speed_init(speed_ctrl_t *spc);
    void speed_update(speed_ctrl_t *spc, float setpoint, float yaw);
    void set_speed(speed_ctrl_t *spc, int mode);

#ifdef __cplusplus
}
#endif
