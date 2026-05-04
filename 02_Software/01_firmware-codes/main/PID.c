#include "PID.h"

static inline float wrap180(float a)
{
    while (a > 180.0f)
        a -= 360.0f;
    while (a <= -180.0f)
        a += 360.0f;
    return a;
}

static inline float angle_error_deg(float goal_deg, float current_deg)
{
    return wrap180(goal_deg - current_deg);
}

float pid_update(PID_t *pid, float setpoint, float measurement, float dt)
{
    float error = angle_error_deg(setpoint, measurement);
    pid->integral += error * dt;

    float derivative = (error - pid->prev_error) / dt;

    float output = pid->Kp * error + pid->Ki * pid->integral + pid->Kd * derivative;

    pid->prev_error = error;

    if (output > pid->out_max)
        output = pid->out_max;

    if (output < pid->out_min)
        output = pid->out_min;

    return output;
}
