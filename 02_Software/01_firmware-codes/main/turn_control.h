#pragma once
#ifdef __cplusplus
extern "C"
{
#endif

#include <stdbool.h>
#include "PID.h"

#define DEBUG_BUF_SIZE 200

    typedef struct
    {
        float yaw;
        float err;
        float output;
    } debug_sample_t;

    static debug_sample_t debug_buf[DEBUG_BUF_SIZE];
    static int debug_index = 0;
    static int debug_count = 0;

    typedef struct
    {
        PID_t pid;

        float target_deg; // relative target (+90 or -90)
        float tol_deg;    // stop tolerance
        int stable_need;  // how many cycles inside tol
        int stable_count;

        float min_speed; // [%] minimum rotate speed to overcome friction
        float max_speed; // [%] clamp

        bool running;
    } turn_ctrl_t;

    void turn_init(turn_ctrl_t *t);
    void turn_start_left(turn_ctrl_t *t, float angle_deg);
    void turn_start_right(turn_ctrl_t *t, float angle_deg);
    void turn_debug_print(void);

    // yaw_rel_deg must be the output of imu.get_yaw() (relative yaw)
    // dt_s in seconds
    bool turn_update(turn_ctrl_t *t, float yaw_rel_deg, float dt_s);
    void turn_cancel(turn_ctrl_t *t);
    void reset_integral(turn_ctrl_t *t);
    void print_last_sample();

#ifdef __cplusplus
}
#endif
