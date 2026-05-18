#include "motor_control.h"

#include <stdbool.h>
#include <stdint.h>

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// =====================================================================================
// TB6612FNG mapping (single motor)
// IN1 = GPIO_NUM_25
// IN2 = GPIO_NUM_26
// PWM = GPIO_NUM_16
// STBY = GPIO_NUM_27
// =====================================================================================

#define MOTOR_IN1_GPIO GPIO_NUM_18
#define MOTOR_IN2_GPIO GPIO_NUM_5
#define MOTOR_PWM_GPIO GPIO_NUM_17
#define MOTOR_STBY_GPIO GPIO_NUM_19

// =====================================================================================
// LEDC config
// =====================================================================================
#define MOTOR_TIMER LEDC_TIMER_0
#define MOTOR_CHANNEL LEDC_CHANNEL_0
#define MOTOR_MODE LEDC_LOW_SPEED_MODE
#define MOTOR_RES LEDC_TIMER_10_BIT
#define MOTOR_FREQ_HZ 20000
#define MOTOR_MAX_DUTY ((1 << 10) - 1) // 1023 for 10-bit

// =====================================================================================
// Helpers
// =====================================================================================
static inline uint32_t pct_to_duty(uint8_t pct) {
  if (pct > 100)
    pct = 100;
  return ((uint32_t)MOTOR_MAX_DUTY * pct) / 100;
}

static inline void pwm_set(uint32_t duty) {
  ledc_set_duty(MOTOR_MODE, MOTOR_CHANNEL, duty);
  ledc_update_duty(MOTOR_MODE, MOTOR_CHANNEL);
}

// =====================================================================================
// Low-level direction control
// =====================================================================================
static inline void motor_forward(uint32_t duty) {
  gpio_set_level(MOTOR_IN1_GPIO, 1);
  gpio_set_level(MOTOR_IN2_GPIO, 0);
  pwm_set(duty);
}

static inline void motor_reverse(uint32_t duty) {
  gpio_set_level(MOTOR_IN1_GPIO, 0);
  gpio_set_level(MOTOR_IN2_GPIO, 1);
  pwm_set(duty);
}

static inline void motor_brake(void) {
  gpio_set_level(MOTOR_IN1_GPIO, 1);
  gpio_set_level(MOTOR_IN2_GPIO, 1);
  pwm_set(0);
}

static inline void motor_coast(void) {
  gpio_set_level(MOTOR_IN1_GPIO, 0);
  gpio_set_level(MOTOR_IN2_GPIO, 0);
  pwm_set(0);
}

// =====================================================================================
// Init
// =====================================================================================
void tb6612_setup(void) {
  // GPIO init
  gpio_config_t io_conf = {
      .pin_bit_mask = (1ULL << MOTOR_STBY_GPIO) | (1ULL << MOTOR_IN1_GPIO) |
                      (1ULL << MOTOR_IN2_GPIO),
      .mode = GPIO_MODE_OUTPUT,
      .pull_up_en = GPIO_PULLUP_DISABLE,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .intr_type = GPIO_INTR_DISABLE,
  };
  ESP_ERROR_CHECK(gpio_config(&io_conf));

  // Start in standby, coast
  gpio_set_level(MOTOR_STBY_GPIO, 0);
  gpio_set_level(MOTOR_IN1_GPIO, 0);
  gpio_set_level(MOTOR_IN2_GPIO, 0);

  // LEDC timer
  ledc_timer_config_t timer = {
      .speed_mode = MOTOR_MODE,
      .timer_num = MOTOR_TIMER,
      .duty_resolution = MOTOR_RES,
      .freq_hz = MOTOR_FREQ_HZ,
      .clk_cfg = LEDC_AUTO_CLK,
  };
  ESP_ERROR_CHECK(ledc_timer_config(&timer));

  // LEDC channel
  ledc_channel_config_t ch = {
      .speed_mode = MOTOR_MODE,
      .channel = MOTOR_CHANNEL,
      .timer_sel = MOTOR_TIMER,
      .intr_type = LEDC_INTR_DISABLE,
      .gpio_num = MOTOR_PWM_GPIO,
      .duty = 0,
      .hpoint = 0,
  };
  ESP_ERROR_CHECK(ledc_channel_config(&ch));
}

void motor_enable(void) { gpio_set_level(MOTOR_STBY_GPIO, 1); }
void motor_disable(void) { gpio_set_level(MOTOR_STBY_GPIO, 0); }

// =====================================================================================
// Public API
// =====================================================================================
void move_forward(uint8_t speed) { motor_forward(pct_to_duty(speed)); }
void move_reverse(uint8_t speed) { motor_reverse(pct_to_duty(speed)); }
void stop(void) { motor_brake(); }
void coast(void) { motor_coast(); }
