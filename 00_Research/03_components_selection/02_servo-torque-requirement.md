# Steering Servo Torque Requirement

This document calculates the minimum torque a steering servo must provide to turn the front wheels of the vehicle under realistic competition conditions. No specific component is named — this defines the requirement the selected servo must meet.

---

## Table of Contents

- [1. The Steering Load Problem](#1-the-steering-load-problem)
- [2. Vehicle Parameters](#2-vehicle-parameters)
- [3. Calculating Steering Torque](#3-calculating-steering-torque)
  - [3.1 Normal Force on Front Axle](#31-normal-force-on-front-axle)
  - [3.2 Tire Scrub Torque](#32-tire-scrub-torque)
  - [3.3 Kingpin Moment Arm](#33-kingpin-moment-arm)
  - [3.4 Total Steering Torque Required](#34-total-steering-torque-required)
- [4. Safety Factor](#4-safety-factor)
- [5. Speed Requirement](#5-speed-requirement)
- [6. Summary of Requirements](#6-summary-of-requirements)
- [Sources](#sources)

---

## 1. The Steering Load Problem

The servo must overcome two resistive forces to turn the front wheels:

1. **Tire scrub torque** — friction between the tire contact patch and the floor as the wheel pivots about the kingpin axis
2. **Inertial load** — resistance of the front wheel assembly mass to angular acceleration

Of these two, tire scrub torque dominates at low speed on a hard floor. Inertial load is negligible at the steering speeds involved and is not calculated separately.

The worst case is **static steering** — turning the wheels while the robot is stationary. Once the robot is moving, rolling reduces the effective friction and steering becomes easier.

---

## 2. Vehicle Parameters

| Parameter | Value | Notes |
|---|---|---|
| Total robot mass `m` | 0.7 kg | Target build weight |
| Front axle load fraction | 40% | Rear-drive vehicle, rear-heavy |
| Front axle normal force `N_f` | 0.7 × 0.40 × 9.81 = **2.75 N** | Weight on front wheels |
| Tire contact patch radius `r_cp` | ~15 mm | Estimated from wheel width |
| Coefficient of friction `μ` | 0.6 | Rubber on competition mat |
| Kingpin moment arm `d` | ~8 mm | Distance from kingpin axis to tie rod pivot |
| Wheelbase `L` | 90 mm | Front to rear axle |
| Track width `T` | 140 mm | Center to center of front wheels |

> **Front axle load fraction:** A rear-wheel-drive vehicle with battery and motor mounted rearward carries approximately 40% of its weight on the front axle. This is a conservative estimate — less front load means less steering torque required. Using 40% ensures the requirement is not underestimated.

---

## 3. Calculating Steering Torque

### 3.1 Normal Force on Front Axle

The total normal force pressing the front wheels against the floor:

```
N_f = m × g × front_fraction
N_f = 0.7 × 9.81 × 0.40
N_f = 2.75 N
```

This is split equally between two front wheels, so each wheel carries:

```
N_per_wheel = 2.75 / 2 = 1.375 N
```

### 3.2 Tire Scrub Torque

When a wheel pivots about its kingpin axis, the contact patch resists rotation. The scrub torque per wheel is modeled as a distributed friction force acting over the contact patch:

```
τ_scrub_per_wheel = μ × N_per_wheel × r_cp
τ_scrub_per_wheel = 0.6 × 1.375 × 0.015
τ_scrub_per_wheel = 0.01238 N·m = 1.238 kg·cm
```

Total scrub torque for both front wheels:

```
τ_scrub_total = 2 × 1.238 = 2.475 kg·cm
```

### 3.3 Kingpin Moment Arm

The servo does not push directly on the kingpin — it acts through a steering arm (tie rod linkage). The mechanical advantage of this linkage determines how much servo torque translates to wheel rotation torque.

For a steering arm length of `d = 8 mm` at the wheel, and assuming a 1:1 servo horn to tie rod geometry:

```
τ_servo_required = τ_scrub_total × (r_cp / d)
τ_servo_required = 2.475 × (15 / 8)
τ_servo_required = 2.475 × 1.875
τ_servo_required ≈ 4.64 kg·cm
```

> **Note:** This ratio depends on the actual steering arm geometry. A longer steering arm reduces the required servo torque. An 8 mm arm is a conservative (short) estimate based on the chassis scale at 90 mm wheelbase.

### 3.4 Total Steering Torque Required

```
τ_required = 4.64 kg·cm
```

---

## 4. Safety Factor

The calculation above uses idealized assumptions:

- Perfectly flat floor with uniform friction
- No binding in the kingpin bearings
- No inertial spike at direction reversal
- No flex in the tie rod or servo horn

A **2× safety factor** is applied to account for these real-world conditions:

```
τ_servo_minimum = τ_required × 2
τ_servo_minimum = 4.64 × 2 ≈ 9.3 kg·cm
```

**The servo stall torque must exceed 9.3 kg·cm at the operating voltage.**

> Standard hobby servos are rated at either 4.8V or 6.0V. The torque at 6.0V is typically 20–30% higher than at 4.8V. The requirement of ≥ 9.3 kg·cm must be met at the actual supply voltage used.

---

## 5. Speed Requirement

The servo must complete a full steering sweep within the time available during a corner approach. The robot approaches a corner at cruise speed and must redirect the wheels before entering the turn.

**Available time for steering sweep:**

At cruise speed of 0.8 m/s, and with the ToF sensor detecting a corner opening at ~150 mm ahead:

```
t_available = distance / speed = 0.150 / 0.8 = 0.19 s
```

**Required angular speed:**

The servo must sweep from center to maximum lock in under 0.19 seconds. Assuming maximum lock is approximately 30°:

```
ω_required = 30° / 0.19 s ≈ 158 °/s
```

Converting to the standard hobby servo speed unit (seconds per 60°):

```
speed_required = 60° / 158°/s ≈ 0.38 s/60°
```

**The servo must achieve full 60° sweep in under 0.38 seconds at operating voltage.**

> Most standard hobby servos achieve 0.10–0.25 s/60° at 6V, comfortably within this requirement. Speed is not the limiting constraint — torque is.

---

## 6. Summary of Requirements

| Requirement | Minimum Value | Notes |
|---|---|---|
| Stall torque | **≥ 9.3 kg·cm** | At operating voltage, with 2× safety factor |
| Speed | **≤ 0.38 s/60°** | At operating voltage |
| Operating voltage | 5V–6V | Must be isolated from logic rail to prevent stall transients resetting MCU |
| Form factor | Standard servo (38×20×30 mm) | Must fit within 300×200 mm chassis |
| Control interface | PWM (50 Hz, 500–2500 µs pulse) | Standard hobby servo protocol |

---

## Sources

| Source | Description |
|---|---|
| Milliken & Milliken — Race Car Vehicle Dynamics (SAE, 1995) | Steering torque and scrub moment theory |
| Norton, R.L. — Machine Design | Linkage mechanical advantage, moment arm calculations |
| [WRO 2026 Future Engineers Rules](https://wro-association.org) | Vehicle dimensions, single steering actuator requirement |
| [FIRGELLI Automations — Ackermann Steering Linkage](https://www.firgelliauto.com/blogs/mechanisms/ackermann-steering-linkage) | Steering arm geometry reference |
