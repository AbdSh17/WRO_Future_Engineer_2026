#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_err.h"

// --- UART wiring (Pi <-> ESP32) ---
#define RPI_UART_PORT UART_NUM_1
#define RPI_UART_TX_PIN GPIO_NUM_33
#define RPI_UART_RX_PIN GPIO_NUM_4
#define RPI_UART_BAUD 115200
#define RPI_UART_RX_BUF 256

typedef enum {
  OBST_COLOR_NONE = 0,
  OBST_COLOR_RED,
  OBST_COLOR_GREEN,
} obstacle_color_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Configure UART2 pins and driver. Call once from main, before
 *        any calls to rpi_read_obstacle().
 */
esp_err_t rpi_uart_setup(void);

/**
 * @brief Non-blocking-ish read of one parsed "OBST:..." line from the Pi.
 *        Call this periodically (e.g. from a task elsewhere) to poll for
 *        new data. Accumulates bytes internally across calls until a
 *        full line ('\n') arrives.
 *
 * @param[out] color        Filled with the detected color (or NONE).
 * @param[out] distance_mm  Filled with distance if color != NONE.
 * @param[out] lateral_mm   Filled with lateral offset if color != NONE
 *                          (positive = right, negative = left).
 * @return true if a new complete, valid line was parsed this call.
 *         false if no complete line was ready yet, or the line was
 *         malformed (outputs left unchanged in that case).
 */
bool rpi_read_obstacle(obstacle_color_t *color, int16_t *distance_mm,
                       int16_t *lateral_mm);

#ifdef __cplusplus
}
#endif
