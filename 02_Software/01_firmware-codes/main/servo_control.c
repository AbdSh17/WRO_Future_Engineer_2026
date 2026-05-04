#include "servo_control.h"
#include "driver/gpio.h"
#include "driver/ledc.h"

// ── Pin
// ───────────────────────────────────────────────────────────────────────
#define SERVO_PIN GPIO_NUM_18

// ── LEDC
// ──────────────────────────────────────────────────────────────────────
#define SERVO_TIMER LEDC_TIMER_1     // TIMER_0 is reserved for motor
#define SERVO_CHANNEL LEDC_CHANNEL_1 // CHANNEL_0 is reserved for motor
#define SERVO_MODE LEDC_LOW_SPEED_MODE
#define SERVO_FREQ_HZ 50
#define SERVO_RESOLUTION LEDC_TIMER_16_BIT // 0–65535
#define SERVO_MAX_DUTY ((1 << 16) - 1)     // 65535

// ── Pulse width bounds (microseconds) ────────────────────────────────────────
// Verify these against your specific servo datasheet
#define SERVO_MIN_US 500      // full left  (-90°)
#define SERVO_MID_US 1500     // center     (  0°)
#define SERVO_MAX_US 2500     // full right (+90°)
#define SERVO_PERIOD_US 20000 // 1 / 50Hz = 20ms

// ── Helper
// ────────────────────────────────────────────────────────────────────
static inline uint32_t us_to_duty(uint16_t pulse_us) {
  return ((uint32_t)pulse_us * SERVO_MAX_DUTY) / SERVO_PERIOD_US;
}

// ── Public API
// ────────────────────────────────────────────────────────────────
void servo_init(void) {
  ledc_timer_config_t timer = {
      .speed_mode = SERVO_MODE,
      .timer_num = SERVO_TIMER,
      .duty_resolution = SERVO_RESOLUTION,
      .freq_hz = SERVO_FREQ_HZ,
      .clk_cfg = LEDC_AUTO_CLK,
  };
  ESP_ERROR_CHECK(ledc_timer_config(&timer));

  ledc_channel_config_t ch = {
      .speed_mode = SERVO_MODE,
      .channel = SERVO_CHANNEL,
      .timer_sel = SERVO_TIMER,
      .intr_type = LEDC_INTR_DISABLE,
      .gpio_num = SERVO_PIN,
      .duty = us_to_duty(SERVO_MID_US), // center on boot
      .hpoint = 0,
  };
  ESP_ERROR_CHECK(ledc_channel_config(&ch));
}

void set_angle(int angle_deg) {
  if (angle_deg > 90)
    angle_deg = 90;
  if (angle_deg < -90)
    angle_deg = -90;

  // map -90..+90 → SERVO_MIN_US..SERVO_MAX_US
  uint16_t pulse_us =
      (uint16_t)(SERVO_MID_US +
                 (angle_deg * (SERVO_MAX_US - SERVO_MID_US) / 90));

  ledc_set_duty(SERVO_MODE, SERVO_CHANNEL, us_to_duty(pulse_us));
    ledc_update_duty(SERVO_MODE,
