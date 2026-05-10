#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void tb6612_setup(void);
void motor_enable(void);
void motor_disable(void);
void move_forward(uint8_t speed);
void move_reverse(uint8_t speed);
void stop(void);
void coast(void);

#ifdef __cplusplus
}
#endif
