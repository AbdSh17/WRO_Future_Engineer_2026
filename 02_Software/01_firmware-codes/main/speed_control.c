#include "speed_control.h"
#include "motor_control.h" // so we can apply PWM using move_forward()

void speed_init(speed_ctrl_t *spc)
{
    spc->max_speed = 100;
    spc->min_speed = 70;

    spc->desired_speed = 0;

    // ============ PID ============
    spc->pid.integral = 0.0f;
    spc->pid.prev_error = 0.0f;

    spc->pid.out_max = 100.0f;
    spc->pid.out_min = -100.0f;

    spc->pid.Kd = 2.0f;
    spc->pid.Ki = 3.0f;
    spc->pid.Kp = 2.5f;
    // =============================

    spc->status = true;
}

void set_speed(speed_ctrl_t *spc, int mode)
{
    if (mode == DEFAULT_SPEED_MODE)
    {
        spc->max_speed = 100;
        spc->min_speed = DEFAULT_SPEED;

        spc->desired_speed = DEFAULT_SPEED;

        spc->pid.Kp = 2.50f;
        spc->pid.Ki = 3.0f;
        spc->pid.Kd = 0.025f;
    }

    else if (mode == WARNING_WALL_SPEED_MODE)
    {
        spc->max_speed = 50;
        spc->min_speed = 20;

        spc->desired_speed = WARNING_WALL_SPEED; // (or set to min/max if you want)

        spc->pid.Kd = 0.0f;
        spc->pid.Ki = 0.0f;
        spc->pid.Kp = 1.2f;
    }
}

void speed_update(speed_ctrl_t *spc, float setpoint, float yaw)
{

    if (!spc->status)
    {
        return;
    }

    // FIX: use PID as sync controller: setpoint 0, measurement err
    float corr = pid_update(&spc->pid, setpoint, yaw, 0.001f);

    float left_speed = (float)spc->desired_speed - corr;
    float right_speed = (float)spc->desired_speed + corr;

    // minimal clamp
    if (left_speed > spc->max_speed)
        left_speed = spc->max_speed;
    if (left_speed < 0)
        left_speed = 0;

    if (right_speed > spc->max_speed)
        right_speed = spc->max_speed;
    if (right_speed < 0)
        right_speed = 0;

    // optional: enforce min_speed only when moving
    if (spc->desired_speed > 0)
    {
        if (left_speed > 0 && left_speed < spc->min_speed)
            left_speed = spc->min_speed;
        if (right_speed > 0 && right_speed < spc->min_speed)
            right_speed = spc->min_speed;
    }

    move_forward((uint8_t)left_speed, (uint8_t)right_speed);
}

// void speed_update(speed_ctrl_t *spc, float enc_left, float enc_right)
// {

//     if (!spc->status)
//     {
//         stop();
//         return;
//     }

//     // FIX: don't accumulate forever
//     float vl = enc_left;
//     float vr = enc_right;

//     // left-right difference (goal = 0)
//     float err = vl - vr;

//     // FIX: use PID as sync controller: setpoint 0, measurement err
//     float corr = pid_update(&spc->pid, 0.0f, err, 0.001f);

//     float left_speed = (float)spc->desired_speed + corr;
//     float right_speed = (float)spc->desired_speed - corr;

//     // minimal clamp
//     if (left_speed > spc->max_speed)
//         left_speed = spc->max_speed;
//     if (left_speed < 0)
//         left_speed = 0;

//     if (right_speed > spc->max_speed)
//         right_speed = spc->max_speed;
//     if (right_speed < 0)
//         right_speed = 0;

//     // optional: enforce min_speed only when moving
//     if (spc->desired_speed > 0)
//     {
//         if (left_speed > 0 && left_speed < spc->min_speed)
//             left_speed = spc->min_speed;
//         if (right_speed > 0 && right_speed < spc->min_speed)
//             right_speed = spc->min_speed;
//     }

//     move_forward((uint8_t)left_speed, (uint8_t)right_speed);
// }
