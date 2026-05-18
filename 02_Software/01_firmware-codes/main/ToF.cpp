// ToF.cpp
#include "ToF.h"

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// -------------------- Addresses --------------------
static constexpr uint8_t TOF_ADDR_DEFAULT =
    0x29; // boot addr for both VL6180X + VL53L0X

// Pick 2 unique addresses for the side VL6180X sensors.
// (Front VL53L0X will stay at 0x29, but we power it up LAST to avoid
// conflicts.)
static constexpr uint8_t TOF_RIGHT_ADDR = 0x30;
static constexpr uint8_t TOF_LEFT_ADDR = 0x31;
static constexpr uint8_t TOF_FRONT_ADDR = 0x32;

// -------------------- XSHUT pins (change to your wiring) --------------------
#define TOF_RIGHT_SHDN GPIO_NUM_15
#define TOF_FRONT_SHDN GPIO_NUM_25
#define TOF_LEFT_SHDN GPIO_NUM_21

static inline TickType_t ms_to_ticks(uint32_t ms) { return pdMS_TO_TICKS(ms); }

// ---------------------------------------------------------------------------
// Ctor / Dtor
// ---------------------------------------------------------------------------
ToF::ToF(i2c_port_t port, uint8_t addr_default)
    : _port(port), _addr_default(addr_default), rightTof(_port, _addr_default),
      frontTof(_port, _addr_default), leftTof(_port, _addr_default) {}

ToF::~ToF() {}

// ---------------------------------------------------------------------------
// Power control helpers (XSHUT)
// ---------------------------------------------------------------------------
void ToF::all_init_tofs_off() {
  // Configure XSHUT pins as outputs
  gpio_set_direction(TOF_RIGHT_SHDN, GPIO_MODE_OUTPUT);
  gpio_set_direction(TOF_FRONT_SHDN, GPIO_MODE_OUTPUT);
  gpio_set_direction(TOF_LEFT_SHDN, GPIO_MODE_OUTPUT);

  // Optional pulldowns (helps keep them OFF during boot/glitches)
  gpio_pulldown_en(TOF_RIGHT_SHDN);
  gpio_pulldown_en(TOF_FRONT_SHDN);
  gpio_pulldown_en(TOF_LEFT_SHDN);

  // Force all sensors off
  gpio_set_level(TOF_RIGHT_SHDN, 0);
  gpio_set_level(TOF_FRONT_SHDN, 0);
  gpio_set_level(TOF_LEFT_SHDN, 0);

  // Let rails discharge / sensors fully reset
  vTaskDelay(ms_to_ticks(50));
}

void ToF::all_tofs_off() {
  gpio_set_level(TOF_RIGHT_SHDN, 0);
  gpio_set_level(TOF_FRONT_SHDN, 0);
  gpio_set_level(TOF_LEFT_SHDN, 0);
}

void ToF::all_tofs_on() {
  gpio_set_level(TOF_RIGHT_SHDN, 1);
  gpio_set_level(TOF_FRONT_SHDN, 1);
  gpio_set_level(TOF_LEFT_SHDN, 1);
}

// ---------------------------------------------------------------------------
// Init helpers
// ---------------------------------------------------------------------------

esp_err_t ToF::init_side_tofs(VL6180X &right, VL6180X &left) {
  // We assume all sensors are currently OFF.

  // Attach shutdown pins (so each class can toggle its own XSHUT)
  right.shdnAttach(TOF_RIGHT_SHDN);
  left.shdnAttach(TOF_LEFT_SHDN);

  // ---- RIGHT (VL6180X) ----
  right.shdnUp(10); // now only RIGHT is alive at 0x29
  ESP_ERROR_CHECK(right.begin());
  ESP_ERROR_CHECK(right.setAddress(TOF_RIGHT_ADDR));
  vTaskDelay(ms_to_ticks(5)); // small gap

  // ---- LEFT (VL6180X) ----
  left.shdnUp(10); // now only LEFT is alive at 0x29 (RIGHT already moved)
  ESP_ERROR_CHECK(left.begin());
  ESP_ERROR_CHECK(left.setAddress(TOF_LEFT_ADDR));
  vTaskDelay(ms_to_ticks(5));

  return ESP_OK;
}

esp_err_t ToF::init_front_tof(VL53L0X &front) {
  // Front must be powered up AFTER the side sensors are no longer at 0x29,

  front.shdnAttach(TOF_FRONT_SHDN);

  front.shdnUp(20); // allow boot time

  // Use the tuning defaults you baked into VL53L0X::begin()
  // (long_range=true, high_speed=true, high_accuracy=false)
  ESP_ERROR_CHECK(front.begin(true, true, false));
  // front.setAddress(TOF_FRONT_ADDR);

  // esp_err_t err = front.startContinuous(25);
  // if (err != ESP_OK) {
  //     // ESP_LOGE("startContinuous() failed: %s (0x%x)",
  //     esp_err_to_name(err), (unsigned)err); printf("ERROR FROM CONTINUES");
  //     while (1) vTaskDelay(pdMS_TO_TICKS(1000));
  // }

  // We keep the front sensor at default address 0x29 (no setAddress needed)
  return ESP_OK;
}

// ---------------------------------------------------------------------------
// Public setup
// ---------------------------------------------------------------------------
esp_err_t ToF::tof_setup() {
  // Hard reset everything first
  all_init_tofs_off();

  // Side sensors first: move them off the default address
  // ESP_ERROR_CHECK(init_side_tofs(rightTof, leftTof));

  // Front sensor last: can safely stay at 0x29
  ESP_ERROR_CHECK(init_front_tof(frontTof));

  return ESP_OK;
}
