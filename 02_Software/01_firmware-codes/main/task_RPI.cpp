#include "task_RPI.h"

#include <stdio.h>
#include <string.h>

#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/task.h"

#define TAG "RPI_UART"

// ---------------------------------------------------------------------------
// UART config — ESP32 side pins.
// Pi side: GPIO14(TX)->this RX, GPIO15(RX)<-this TX, shared GND.
// ---------------------------------------------------------------------------
#define RPI_UART_NUM UART_NUM_2
#define RPI_UART_RX_GPIO GPIO_NUM_4
#define RPI_UART_TX_GPIO GPIO_NUM_33
#define RPI_UART_BAUD 115200
#define RPI_RX_BUF_SIZE 512
#define RPI_LINE_MAX 256

static SemaphoreHandle_t s_mtx = NULL;
static rpi_state_t s_state = {};
static int64_t s_last_rx_us = 0;

// ---------------------------------------------------------------------------
// Parsing
//
// Expected line, either field group may be absent but the ';' separator
// is always present when both are sent:
//   NAV:L=<int>,R=<int>;OBST:<COLOR>,<dist_mm>,<lat_mm>\n
//   NAV:L=<int>,R=<int>;OBST:NONE\n
// ---------------------------------------------------------------------------
static bool parse_line(const char *line, rpi_state_t *out) {
  int nav_l = out->nav_left;
  int nav_r = out->nav_right;
  bool got_nav = false;

  const char *nav_pos = strstr(line, "NAV:");
  if (nav_pos) {
    int l, r;
    if (sscanf(nav_pos, "NAV:L=%d,R=%d", &l, &r) == 2) {
      nav_l = l;
      nav_r = r;
      got_nav = true;
    }
  }

  bool got_obst = false;
  obst_color_t color = OBST_NONE;
  int dist = 0, lat = 0;

  const char *obst_pos = strstr(line, "OBST:");
  if (obst_pos) {
    if (strncmp(obst_pos, "OBST:NONE", 9) == 0) {
      color = OBST_NONE;
      got_obst = true;
    } else {
      char color_str[8] = {0};
      if (sscanf(obst_pos, "OBST:%7[^,],%d,%d", color_str, &dist, &lat) == 3) {
        if (strcmp(color_str, "RED") == 0)
          color = OBST_RED;
        else if (strcmp(color_str, "GREEN") == 0)
          color = OBST_GREEN;
        else
          color = OBST_NONE;
        got_obst = true;
      }
    }
  }

  if (!got_nav && !got_obst) {
    return false; // nothing usable in this line
  }

  if (got_nav) {
    out->nav_left = nav_l;
    out->nav_right = nav_r;
  }
  if (got_obst) {
    out->obst_color = color;
    out->obst_distance_mm = dist;
    out->obst_lateral_mm = lat;
  }
  return true;
}

// ---------------------------------------------------------------------------
// Receive task — reads UART byte-by-byte into a line buffer, parses on '\n'
// ---------------------------------------------------------------------------
static void rpi_rx_task(void *arg) {
  (void)arg;

  uint8_t byte;
  char line[RPI_LINE_MAX];
  int line_len = 0;

  while (true) {
    int n = uart_read_bytes(RPI_UART_NUM, &byte, 1, pdMS_TO_TICKS(100));
    if (n <= 0) {
      continue; // timeout, just loop — keeps task responsive without
                // busy-waiting
    }

    if (byte == '\n') {
      if (line_len > 0) {
        line[line_len] = '\0';

        rpi_state_t parsed;
        if (s_mtx && xSemaphoreTake(s_mtx, pdMS_TO_TICKS(20)) == pdTRUE) {
          parsed = s_state; // start from current state so partial updates don't
                            // wipe the other half
          bool ok = parse_line(line, &parsed);
          if (ok) {
            parsed.valid = true;
            s_state = parsed;
            s_last_rx_us = esp_timer_get_time();
          }
          xSemaphoreGive(s_mtx);
        }
      }
      line_len = 0;
    } else if (byte != '\r') {
      if (line_len < RPI_LINE_MAX - 1) {
        line[line_len++] = (char)byte;
      } else {
        // line too long, drop it — malformed/corrupted, reset buffer
        line_len = 0;
      }
    }
  }
}

// ---------------------------------------------------------------------------
// Public
// ---------------------------------------------------------------------------
esp_err_t rpi_uart_setup(void) {
  uart_config_t cfg = {};
  cfg.baud_rate = RPI_UART_BAUD;
  cfg.data_bits = UART_DATA_8_BITS;
  cfg.parity = UART_PARITY_DISABLE;
  cfg.stop_bits = UART_STOP_BITS_1;
  cfg.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
  cfg.source_clk = UART_SCLK_DEFAULT;
  // remaining fields (rx_flow_ctrl_thresh, rx_glitch_filt_thresh, flags)
  // zero-initialized by `= {}` above — fine since flow control is disabled

  esp_err_t err =
      uart_driver_install(RPI_UART_NUM, RPI_RX_BUF_SIZE, 0, 0, NULL, 0);
  if (err != ESP_OK)
    return err;

  err = uart_param_config(RPI_UART_NUM, &cfg);
  if (err != ESP_OK)
    return err;

  err = uart_set_pin(RPI_UART_NUM, RPI_UART_TX_GPIO, RPI_UART_RX_GPIO,
                     UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
  if (err != ESP_OK)
    return err;

  ESP_LOGI(TAG, "UART2 ready: RX=GPIO%d TX=GPIO%d @ %d baud", RPI_UART_RX_GPIO,
           RPI_UART_TX_GPIO, RPI_UART_BAUD);
  return ESP_OK;
}

void rpi_task_start(SemaphoreHandle_t rpi_mtx) {
  s_mtx = rpi_mtx;
  memset(&s_state, 0, sizeof(s_state));
  xTaskCreate(rpi_rx_task, "rpi_rx_task", 4096, NULL, 4, NULL);
}

bool rpi_state_read(rpi_state_t *out) {
  if (!out || !s_mtx)
    return false;
  if (xSemaphoreTake(s_mtx, pdMS_TO_TICKS(20)) != pdTRUE)
    return false;

  *out = s_state;
  out->age_us = esp_timer_get_time() - s_last_rx_us;

  xSemaphoreGive(s_mtx);
  return true;
}
