#pragma once

// --- Control task ---
#define CTRL_PERIOD_MS 10
#define CTRL_STACK_SIZE 3072
#define CTRL_PRIORITY 6

// --- IMU task ---
#define IMU_PERIOD_MS 10
#define IMU_STACK_SIZE 2048
#define IMU_PRIORITY 5

// --- ToF task ---
#define TOF_PERIOD_MS 20
#define TOF_STACK_SIZE 3072
#define TOF_PRIORITY 5

// --- Algorithm task ---
#define ALG_PERIOD_MS 50
#define ALG_STACK_SIZE 4096
#define ALG_PRIORITY 4
