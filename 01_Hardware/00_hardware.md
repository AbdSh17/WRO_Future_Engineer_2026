# Hardware

Overview of the robot's electromechanical design. This folder documents the final, built hardware. For the reasoning and calculations behind each decision, see [`00_Research`](../00_Research).

---

## Table of Contents

- [1. Summary](#1-summary)
- [2. Documents in This Folder](#2-documents-in-this-folder)
- [3. Design Reasoning — Research References](#3-design-reasoning--research-references)

---

## 1. Summary

The vehicle is a rear-wheel-drive, Ackermann-steered platform built around a dual-computer architecture: an ESP32 handles real-time sensing and motor/servo control, while a Raspberry Pi 5 is reserved for vision processing. Full component list, power design, and pinout are documented in this folder.

| Subsystem | Summary |
|---|---|
| Drive | Single JGB37-520 DC motor, rear axle, via TB6612FNG |
| Steering | MG996R servo, Ackermann front geometry |
| Sensing | 3× VL53L0X (front/left/right), BNO055 IMU, hall-effect encoder |
| Compute | ESP32-DEVKITC-32D (control) + Raspberry Pi 5 (vision) |
| Power | 3S Li-Ion, dual isolated MP1584EN rails (5V logic, 6V servo) |

---

## 2. Documents in This Folder

| File | Contents |
|---|---|
| [`01_bill-of-materials.md`](01_bill-of-materials.md) | Final component list, grouped by subsystem |
| [`02_power-architecture.md`](02_power-architecture.md) | Rail structure, current budget, isolation rationale |
| [`03_wiring.md`](03_wiring.md) | Full GPIO pinout, I2C buses, XSHUT sequencing |

---

## 3. Design Reasoning — Research References

Every hardware decision above was driven by a calculation or comparison documented in `00_Research`:

| Decision | Research Document |
|---|---|
| Drive motor torque/speed requirement | [`03_components_selection/01_motor-selection-criteria.md`](../00_Research/03_components_selection/01_motor-selection-criteria.md) |
| Steering servo torque requirement | [`03_components_selection/02_servo-torque-requirement.md`](../00_Research/03_components_selection/02_servo-torque-requirement.md) |
| Decoupling/bulk capacitor and pull-resistor sizing | [`03_components_selection/03_passive-components.md`](../00_Research/03_components_selection/03_passive-components.md) |
| Differential vs. solid axle drivetrain | [`04_mechanical/01_Differential.md`](../00_Research/04_mechanical/01_Differential.md) |
| Ackermann steering geometry | [`04_mechanical/02_ackermann-steering.md`](../00_Research/04_mechanical/02_ackermann-steering.md) |

This folder documents **what was built**. The research folder documents **why it was built that way**.
