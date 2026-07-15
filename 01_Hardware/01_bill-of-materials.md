# Bill of Materials (BOM)

Final component list for the WRO 2026 Future Engineers vehicle. Each entry links to the research document that justifies the requirement it meets.

---

## Table of Contents

- [1. Compute](#1-compute)
- [2. Sensors](#2-sensors)
- [3. Actuators](#3-actuators)
- [4. Motor Driver](#4-motor-driver)
- [5. Power](#5-power)
- [6. Full Table](#6-full-table)

---

## 1. Compute

<p align="center">
  <img src="materials/raspberry-pi-5.jpg" alt="Raspberry Pi 5" width="300"/>
  <img src="materials/esp32-devkitc-32d.jpg" alt="ESP32 DevKitC 32D" width="300"/>
</p>

| Component | Role |
|---|---|
| **Raspberry Pi 5** | High-level logic, camera and vision processing |
| **ESP32-DEVKITC-32D** | Low-level control: sensors, motor, servo, FreeRTOS tasks |

Split chosen to separate real-time control (ESP32) from vision workloads (Pi 5), avoiding jitter in motor/servo timing caused by heavier vision processing.

---

## 2. Sensors

<p align="center">
  <img src="materials/bno055-imu.jpg" alt="BNO055 IMU" width="300"/>
  <img src="materials/vl53l0x-tof.jpg" alt="VL53L0X ToF" width="300"/>
</p>

| Component | Qty | Role |
|---|---|---|
| **BNO055** | 1 | Absolute heading (yaw) for turn control and heading hold |
| **VL53L0X** | 3 | Front, left, right distance — corner and wall detection |

See [`01_motor-selection-criteria.md`](../00_Research/03_components_selection/01_motor-selection-criteria.md) for related speed/timing requirements that drove sensor update-rate needs.

---

## 3. Actuators

<p align="center">
  <img src="materials/jgb37-520-motor.png" alt="JGB37-520 DC motor" width="300"/>
  <img src="materials/mg996r-servo.jpg" alt="MG996R servo" width="300"/>
</p>

| Component | Role | Requirement met |
|---|---|---|
| **JGB37-520** (12V DC gear motor) | Rear drive | Stall torque ≥ 3.5 kg·cm — see [`01_motor-selection-criteria.md`](../00_Research/03_components_selection/01_motor-selection-criteria.md) |
| **MG996R** servo | Front Ackermann steering | Stall torque ≥ 9.3 kg·cm — see [`02_servo-torque-requirement.md`](../00_Research/03_components_selection/02_servo-torque-requirement.md) |

---

## 4. Motor Driver

<p align="center">
  <img src="materials/tb6612fng-driver.webp" alt="TB6612FNG driver" width="300"/>
</p>

| Component | Role |
|---|---|
| **TB6612FNG** | Single-channel H-bridge for the drive motor |

Single motor uses one channel; the second channel on the chip is unused.

---

## 5. Power

<p align="center">
  <img src="materials/li-ion-3s-battery.jpg" alt="3S Li-Ion battery" width="300"/>
  <img src="materials/mp1584en-buck.jpg" alt="MP1584EN buck converter" width="300"/>
</p>

| Component | Qty | Role |
|---|---|---|
| **3S Li-Ion battery** | 1 | Main power source |
| **MP1584EN buck converter** | 2 | One 5V rail (logic), one 6V rail (servo, isolated) |
| **Hall-effect encoder** | 1 | Wheel speed / distance feedback, mounted on motor shaft |

Dual regulator design isolates the servo from the logic rail so servo stall current spikes cannot brown out the ESP32. See [`03_passive-components.md`](../00_Research/03_components_selection/03_passive-components.md) for capacitor sizing on each rail.

---

## 6. Full Table

| # | Component | Qty | Category | Interface |
|---|---|---|---|---|
| 1 | Raspberry Pi 5 | 1 | Compute | — |
| 2 | ESP32-DEVKITC-32D | 1 | Compute | UART to Pi 5 |
| 3 | BNO055 | 1 | Sensor | I2C |
| 4 | VL53L0X | 3 | Sensor | I2C (address-switched via XSHUT) |
| 5 | Hall-effect encoder | 1 | Sensor | Digital GPIO (ISR) |
| 6 | JGB37-520 DC gear motor | 1 | Actuator | PWM via TB6612FNG |
| 7 | MG996R servo | 1 | Actuator | PWM (LEDC) |
| 8 | TB6612FNG | 1 | Driver | GPIO + PWM |
| 9 | 3S Li-Ion battery | 1 | Power | — |
| 10 | MP1584EN buck converter | 2 | Power | 5V logic rail, 6V servo rail |
