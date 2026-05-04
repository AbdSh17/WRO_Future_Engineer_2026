#pragma once
#include "driver/ledc.h"

#ifdef __cplusplus
extern "C" {
#endif

void servo_init(void);
void set_angle(int angle_deg); // -90 to +90
void servo_center(void);
void servo_disable(void);

#ifdef __cplusplus
}
#endif
