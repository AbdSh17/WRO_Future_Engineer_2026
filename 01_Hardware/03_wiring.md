# Wiring


---

## Table of Contents

- [1. I2C Buses](#1-i2c-buses)
- [2. Motor Driver (TB6612FNG)](#2-motor-driver-tb6612fng)
- [3. Servo](#3-servo)
- [4. Encoder](#4-encoder)
- [5. ToF XSHUT Pins](#5-tof-xshut-pins)
- [6. Pi 5 ↔ ESP32 UART](#6-pi-5--esp32-uart)
- [7. Full Pinout Table](#7-full-pinout-table)

---

## 1. I2C Buses

Two separate I2C buses isolate the ToF sensors (which require XSHUT address remapping) from the IMU.

| Bus | Port | SDA | SCL | Frequency | Devices |
|---|---|---|---|---|---|
| IMU bus | I2C_NUM_1 | GPIO22 | GPIO23 | 400 kHz | BNO055 |
| ToF bus | I2C_NUM_0 | GPIO27 | GPIO14 | 400 kHz | 3× VL53L0X |

---

## 2. Motor Driver (TB6612FNG)

| Signal | GPIO |
|---|---|
| IN1 | GPIO18 |
| IN2 | GPIO5 |
| PWM | GPIO17 |
| STBY | GPIO19 |

LEDC assignment: Timer 0, Channel 0, 20 kHz, 10-bit resolution.

---

## 3. Servo

| Signal | GPIO |
|---|---|
| PWM (signal) | GPIO32 |

LEDC assignment: Timer 1, Channel 1, 50 Hz, 16-bit resolution. Pulse width range 500–2500 µs (MID = 1500 µs, requires physical calibration).

---

## 4. Encoder

| Signal | GPIO |
|---|---|
| Signal (hall-effect) | GPIO21 |

Single hall-effect sensor, interrupt-driven (`IRAM_ATTR` ISR), `volatile` tick counter.

---

## 5. ToF XSHUT Pins

| Sensor | XSHUT GPIO | I2C Address (after remap) |
|---|---|---|
| Right | GPIO26 | 0x30 |
| Front | GPIO13 | 0x32 |
| Left | GPIO25 | 0x31 |

All three sensors boot at the default address 0x29. XSHUT sequencing powers them up one at a time so each can be individually reassigned to a unique address before the next is enabled.

---

## 6. Pi 5 ↔ ESP32 UART

| Signal | Pi 5 GPIO | ESP32 |
|---|---|---|
| TX → RX | GPIO14 | RX pin |
| RX ← TX | GPIO15 | TX pin |

Pi 5 side uses `/dev/ttyAMA0`. Not yet implemented in firmware — reserved pins only at this stage.

---

## 7. Full Pinout Table

| Function | GPIO | Notes |
|---|---|---|
| IMU SDA | 22 | I2C_NUM_1 |
| IMU SCL | 23 | I2C_NUM_1 |
| ToF SDA | 27 | I2C_NUM_0 |
| ToF SCL | 14 | I2C_NUM_0 |
| Motor IN1 | 18 | TB6612FNG |
| Motor IN2 | 5 | TB6612FNG |
| Motor PWM | 17 | TB6612FNG, LEDC Timer 0 Ch 0 |
| Motor STBY | 19 | TB6612FNG |
| Servo PWM | 32 | LEDC Timer 1 Ch 1 |
| Encoder | 21 | Hall-effect ISR |
| ToF Right XSHUT | 26 | — |
| ToF Front XSHUT | 13 | Moved off strapping pin GPIO12 |
| ToF Left XSHUT | 25 | — |
| UART TX (Pi→ESP32) | 14 (Pi side) | Reserved, not yet implemented |
| UART RX (ESP32→Pi) | 15 (Pi side) | Reserved, not yet implemented |

All 15 GPIOs above are unique — no pin conflicts in the current pinout.
