#include "RPI.h"

#include <cstdio>
#include <cstring>

#include "esp_log.h"

static const char *TAG = "RPI";

esp_err_t rpi_uart_setup(void) {
  uart_config_t cfg = {};
  cfg.baud_rate = RPI_UART_BAUD;
  cfg.data_bits = UART_DATA_8_BITS;
  cfg.parity = UART_PARITY_DISABLE;
  cfg.stop_bits = UART_STOP_BITS_1;
  cfg.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
  cfg.source_clk = UART_SCLK_DEFAULT;

  esp_err_t err = uart_param_config(RPI_UART_PORT, &cfg);
  if (err != ESP_OK)
    return err;

  err = uart_set_pin(RPI_UART_PORT, RPI_UART_TX_PIN, RPI_UART_RX_PIN,
                     UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
  ESP_LOGI(TAG, "uart_set_pin result: %s", esp_err_to_name(err));
  if (err != ESP_OK)
    return err;

  gpio_dump_io_configuration(stdout, (1ULL << 4) | (1ULL << 16));

  err = uart_driver_install(RPI_UART_PORT, RPI_UART_RX_BUF, 0, 0, NULL, 0);
  if (err != ESP_OK)
    return err;

  ESP_LOGI(TAG, "UART ready (RX=GPIO%d TX=GPIO%d, %d baud)", RPI_UART_RX_PIN,
           RPI_UART_TX_PIN, RPI_UART_BAUD);
  return ESP_OK;
}

// Parses "OBST:NONE" or "OBST:RED,280,60" / "OBST:GREEN,-80,350"
static bool parse_obstacle_line(const char *line, obstacle_color_t *color,
                                int16_t *distance_mm, int16_t *lateral_mm) {
  if (strncmp(line, "OBST:NONE", 9) == 0) {
    *color = OBST_COLOR_NONE;
    *distance_mm = 0;
    *lateral_mm = 0;
    return true;
  }

  char color_str[8] = {0};
  int distance = 0, lateral = 0;
  int n = sscanf(line, "OBST:%7[^,],%d,%d", color_str, &distance, &lateral);
  if (n != 3)
    return false;

  if (strcmp(color_str, "RED") == 0) {
    *color = OBST_COLOR_RED;
  } else if (strcmp(color_str, "GREEN") == 0) {
    *color = OBST_COLOR_GREEN;
  } else {
    return false;
  }

  *distance_mm = (int16_t)distance;
  *lateral_mm = (int16_t)lateral;
  return true;
}

bool rpi_read_obstacle(obstacle_color_t *color, int16_t *distance_mm,
                       int16_t *lateral_mm) {
  static char line_buf[64];
  static int line_pos = 0;

  uint8_t byte;
  // Drain whatever is currently in the UART FIFO, non-blocking beyond 0 wait.
  while (uart_read_bytes(RPI_UART_PORT, &byte, 1, 0) > 0) {
    if (byte == '\n') {
      line_buf[line_pos] = '\0';
      line_pos = 0;

      if (parse_obstacle_line(line_buf, color, distance_mm, lateral_mm)) {
        return true;
      }
      // malformed line — keep draining, don't return yet
      continue;
    }

    if (byte != '\r') {
      if (line_pos < (int)sizeof(line_buf) - 1) {
        line_buf[line_pos++] = (char)byte;
      } else {
        // line too long / garbage — reset
        line_pos = 0;
      }
    }
  }

  return false;
}
