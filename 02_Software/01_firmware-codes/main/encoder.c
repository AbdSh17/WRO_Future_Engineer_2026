#include "encoder.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>

encoder_t encoder = {0};

/*
 *
 *	======= IRAM_ATTR =======
 *
 * Region | Used for
 * ------ |-----------------------------------------------------------
 * IRAM   | Code/instructions only (your ISRs with `IRAM_ATTR`)
 * DRAM   | Data — heap (malloc), stack, global variables
 * Flash  | Code + read-only data by default
 *
 */

static void IRAM_ATTR encoder_isr(void *arg) { encoder.ticks++; }

void encoder_setup(void) {
  gpio_install_isr_service(0);

  gpio_config_t io = {
      .pin_bit_mask = 1ULL << ENCODER_PIN,
      .mode = GPIO_MODE_INPUT,
      .pull_up_en = GPIO_PULLUP_ENABLE,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .intr_type = GPIO_INTR_POSEDGE,
  };
  gpio_config(&io);
  gpio_isr_handler_add(ENCODER_PIN, encoder_isr, NULL);
}

void encoder_reset(void) { encoder.ticks = 0; }

int32_t encoder_get_ticks(void) { return encoder.ticks; }

float encoder_get_distance_mm(void) {
  return encoder.ticks * ENCODER_MM_PER_TICK;
}
