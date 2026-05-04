# Motor Control — Design Report

A concise record of the design decisions behind `motor_control.c`, written for the TB6612FNG driving a single JGB37-520 DC motor on an Ackermann-steered platform.

---

## Table of Contents

- [Single Motor Architecture](#single-motor-architecture)
- [PWM Configuration Choices](#pwm-configuration-choices)
- [STBY as Enable/Disable](#stby-as-enabledisable)
- [Brake vs Coast](#brake-vs-coast)
- [Public API Design](#public-api-design)
- [What Was Removed From the Original](#what-was-removed-from-the-original)

---

## Single Motor Architecture

The original micromouse code controlled two independent motors for differential steering. The WRO platform uses Ackermann steering — a servo handles direction, and a single rear motor handles speed. There is no per-side speed differential needed, so the entire right-motor half of the driver was removed. The TB6612FNG has two channels; only channel A is wired and used.

---

## PWM Configuration Choices

| Parameter | Value | Reason |
|-----------|-------|--------|
| Frequency | 20 kHz | Above human hearing — eliminates coil whine |
| Resolution | 10-bit (0–1023) | 1024 speed steps is more than enough for a drive motor |
| Timer | `LEDC_TIMER_0` | Reserved exclusively for the motor |
| Channel | `LEDC_CHANNEL_0` | Single channel, no conflicts |

The servo will use a separate timer (`LEDC_TIMER_1`) at 50 Hz and 16-bit resolution. The two devices cannot share a timer because they require different frequencies.

---

## STBY as Enable/Disable

The TB6612FNG has a standby pin (`STBY`) that disables both output channels when pulled low. The driver initializes with `STBY = 0`, meaning the motor is physically disabled at startup regardless of what the direction pins or PWM are doing. This is intentional — it prevents the motor from twitching during ESP32 boot while GPIO states are undefined.

`motor_enable()` and `motor_disable()` are exposed as part of the public API so higher-level code can arm and disarm the motor explicitly, for example at round start and on emergency stop.

---

## Brake vs Coast

The TB6612FNG supports two distinct stop behaviors:

| Mode | IN1 | IN2 | Effect |
|------|-----|-----|--------|
| Brake | HIGH | HIGH | Motor terminals shorted together — back-EMF creates active braking |
| Coast | LOW | LOW | Motor terminals floating — robot rolls freely |

`stop()` maps to **brake** — the correct choice for a competition robot that needs to stop predictably at a target position. `coast()` is kept available for cases where free-rolling is intentional, such as during a controlled deceleration phase.

---

## Public API Design

The public API was intentionally kept minimal:

| Function | Purpose |
|----------|---------|
| `tb6612_setup()` | One-time init — GPIO, LEDC timer, LEDC channel |
| `motor_enable()` | Pull STBY high — arms the motor |
| `motor_disable()` | Pull STBY low — disarms the motor |
| `move_forward(speed)` | Drive forward at 0–100% speed |
| `move_reverse(speed)` | Drive reverse at 0–100% speed |
| `stop()` | Active brake |
| `coast()` | Free-roll stop |

Speed is expressed as a percentage (0–100) rather than raw duty counts. This keeps higher-level code readable and decoupled from the PWM resolution — if the resolution ever changes, only `pct_to_duty()` needs updating.

---

## What Was Removed From the Original

The micromouse code included functions for differential arcing, point turns, and per-side speed control. All of these were removed because they have no meaning on an Ackermann platform:

- Turning is handled entirely by the servo — the motor has no role in steering
- There is no left/right motor distinction
- `move_forward_max()`, `move_arch()`, `move_left_rotate()` and their variants are all gone

Keeping dead code would have been misleading in a documentation-scored competition.
