#pragma once
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  OBST_NONE = 0,
  OBST_RED = 1,
  OBST_GREEN = 2,
} obst_color_t;

typedef struct {
  bool valid;     // true if a message has ever been successfully parsed
  int64_t age_us; // time since last successful parse (esp_timer_get_time based)

  // --- Wall-following data (replaces side ToF) ---
  int nav_left;  // 0-100, relative "wall presence" on the left watch-box
  int nav_right; // 0-100, relative "wall presence" on the right watch-box

  // --- Obstacle pillar data ---
  obst_color_t obst_color;
  int obst_distance_mm;
  int obst_lateral_mm; // + = right of centerline, - = left
} rpi_state_t;

/**
 * @brief Configure UART2 (GPIO4=RX, GPIO33=TX) and start the background
 *        receive task that continuously parses lines from the Pi.
 */
esp_err_t rpi_uart_setup(void);

/**
 * @brief Start the RPI receive task. Call after rpi_uart_setup().
 */
void rpi_task_start(SemaphoreHandle_t rpi_mtx);

/**
 * @brief Copy out the most recent parsed state (thread-safe).
 *        Returns true if out was filled, false if the mutex could not
 *        be taken in time.
 */
bool rpi_state_read(rpi_state_t *out);

#ifdef __cplusplus
}
#endif
