#pragma once

#include "./PID.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

extern PID_t left_motor_pid;
extern PID_t right_motor_pid;
extern PID_t center_robot_pid;

void drv8833_setup(void);

void move_forward(uint8_t left_speed, uint8_t right_speed);
void move_forward_max(void);

void move_arch(uint8_t left_speed, uint8_t right_speed);

void move_left_arch_max(void);
void move_right_arch_max(void);

void move_left_rotate(uint8_t left_speed, uint8_t right_speed);
void move_left_rotate_max(void);

void move_right_rotate(uint8_t left_speed, uint8_t right_speed);
void move_right_rotate_max(void);

void move_reverse(uint8_t left_speed, uint8_t right_speed);
void move_reverse_max(void);

void motor_sleep_init(void);

void stop(void);

#ifdef __cplusplus
}
#endif
