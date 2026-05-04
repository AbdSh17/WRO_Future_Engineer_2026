#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct
    {
        float Kp;
        float Ki;
        float Kd;

        float prev_error;
        float integral;

        float out_min;
        float out_max;
    } PID_t;

    float pid_update(PID_t *pid,
                     float setpoint,
                     float measurement,
                     float dt);

#ifdef __cplusplus
}
#endif
