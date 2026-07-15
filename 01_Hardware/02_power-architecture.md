# Power Architecture

This document describes the actual power distribution design of the robot: rail structure, current budget, and isolation strategy.

---

## Table of Contents

- [1. Overview](#1-overview)
- [2. Rail Structure](#2-rail-structure)
- [3. Current Budget](#3-current-budget)
- [4. Why the Servo Rail Is Isolated](#4-why-the-servo-rail-is-isolated)
- [5. Capacitor Placement](#5-capacitor-placement)

---

## 1. Overview

<p align="center">
  <img src="materials/dual-rail-power-diagram.jpg" alt="dual rail power architecture" width="500"/>
</p>

The robot runs on a single 3S Li-Ion battery (11.1V nominal) feeding two independent buck converters. Each converter powers a separate rail so that current spikes on one rail cannot affect the other.

```
3S Li-Ion (11.1V)
      │
      ├── MP1584EN #1 → 5V rail  → ESP32, sensors, logic
      │
      └── MP1584EN #2 → 6V rail  → MG996R servo only
```

---

## 2. Rail Structure

<p align="center">
  <img src="materials/buck-converter-block-diagram.jpg" alt="buck converter block diagram" width="500"/>
</p>

| Rail | Voltage | Powers |
|---|---|---|
| Logic rail | 5V | ESP32, BNO055, VL53L0X ×3, encoder |
| Servo rail | 6V | MG996R steering servo only |
| Motor rail | 11.1V (direct from battery) | JGB37-520 via TB6612FNG |

The drive motor is powered directly from the battery through the TB6612FNG — it does not pass through either buck converter, since the motor driver handles its own switching.

---

## 3. Current Budget

| Load | Typical | Peak (stall) |
|---|---|---|
| ESP32 | 160 mA | 240 mA |
| BNO055 | 12 mA | — |
| VL53L0X ×3 | 60 mA (20 mA each) | — |
| Encoder | < 5 mA | — |
| **Logic rail total** | **~240 mA** | **~300 mA** |
| MG996R servo | 500 mA | 2.5 A |
| **Servo rail total** | **500 mA** | **2.5 A** |
| JGB37-520 motor | 450 mA | 1.2 A |

The servo's 2.5A stall spike is the dominant transient in the system — over 8× the entire logic rail's typical draw. This spike is the primary reason for rail isolation (see Section 4).

---

## 4. Why the Servo Rail Is Isolated

If the servo shared a rail with the ESP32 and sensors, a stall event (servo hitting mechanical limit or high steering load) would pull 2.5A instantaneously. This causes the shared rail voltage to sag, which can:

- Brown-out reset the ESP32 mid-operation
- Cause I2C bus errors on the sensor bus
- Corrupt in-progress control loop state

By giving the servo its own MP1584EN converter, a stall event only sags the servo rail — the logic rail stays stable and the ESP32 keeps running.

> This is not about voltage difference (5V vs 6V) — it is about current isolation. Even if the servo ran at 5V, it would still need its own regulator to prevent stall transients from affecting logic.

---

## 5. Capacitor Placement

Bulk and decoupling capacitor sizing follows the calculations in [`03_passive-components.md`](../00_Research/03_components_selection/03_passive-components.md):

- **100 nF decoupling** at every IC's VCC pin (ESP32, BNO055, each VL53L0X)
- **100–470 µF bulk capacitor** at each MP1584EN output, closest to the highest-current load on that rail
- Servo rail bulk capacitor sized toward the upper end (470 µF) given the 2.5A stall spike
