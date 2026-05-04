# Encoder — Design Report

A concise record of the design decisions behind `encoder.c`, written for a single hall-effect encoder on the JGB37-520 motor, used to measure distance traveled on an Ackermann-steered platform.

---

## Table of Contents

- [Single Encoder Architecture](#single-encoder-architecture)
- [Interrupt-Driven Counting](#interrupt-driven-counting)
- [IRAM Placement](#iram-placement)
- [Volatile Ticks](#volatile-ticks)
- [Distance Calculation](#distance-calculation)
- [Edge Detection Choice](#edge-detection-choice)
- [What Was Removed From the Original](#what-was-removed-from-the-original)

---

## Single Encoder Architecture

The original micromouse code used two encoders — one per drive wheel — for odometry and differential speed control. On an Ackermann platform with a single rear drive motor, one encoder on the motor shaft is sufficient. There is no per-side tracking needed; the single encoder measures how far the vehicle has traveled along its driven axis.

---

## Interrupt-Driven Counting

The encoder ISR is triggered on every rising edge of the encoder signal. Each trigger increments a tick counter. This approach offloads all counting work from the main loop — the main task never needs to poll the pin, it simply reads the counter whenever it needs a distance snapshot.

The counter is `int32_t`, giving a range of ±2.1 billion ticks before overflow. At the JGB37-520's gear ratio and wheel diameter, this corresponds to a travel distance far exceeding any practical concern.

---

## IRAM Placement

The ISR is marked `IRAM_ATTR`, placing it in internal RAM rather than flash. This ensures the interrupt handler executes immediately even if the flash cache is busy at the moment the interrupt fires. Without this, the ISR could stall for an unpredictable number of cycles, causing missed ticks at high motor speeds.

---

## Volatile Ticks

The `ticks` field in the encoder struct is declared `volatile`. This tells the compiler it must not cache the value in a CPU register between reads — it must always fetch it from memory. Without `volatile`, the compiler could legally optimize a read loop into reading the value once and reusing it, never seeing updates written by the ISR.

`IRAM_ATTR` and `volatile` solve different problems and are both required:

| Keyword | Scope | Solves |
|---------|-------|--------|
| `IRAM_ATTR` | Linker | ISR lives in IRAM, executes without cache dependency |
| `volatile` | Compiler | Main code always reads the latest value from memory |

---

## Distance Calculation

Distance is derived from tick count using two hardware constants:

```
MM_PER_TICK = (π × wheel_diameter) / (CPR × gear_ratio)
distance    = ticks × MM_PER_TICK
```

These constants are defined in the header and must be verified against the physical hardware before use:

| Constant | Source |
|----------|--------|
| `ENCODER_CPR` | Motor datasheet — counts per motor shaft revolution |
| `GEAR_RATIO` | Motor label — printed as e.g. 1:30 |
| `WHEEL_DIAMETER_MM` | Physical measurement of the mounted wheel |

An incorrect value in any of these three will silently produce wrong distance readings with no compile-time warning. They should be validated by manually rotating the wheel a known distance and comparing the reported value.

---

## Edge Detection Choice

The ISR triggers on `GPIO_INTR_POSEDGE` — rising edges only. This counts one tick per electrical cycle of the encoder signal.

Switching to `GPIO_INTR_ANYEDGE` would trigger on both rising and falling edges, doubling the effective resolution without any hardware change. This is a straightforward upgrade if finer distance granularity is needed later, requiring only a one-line change in `encoder_setup()` and a corresponding update to `ENCODER_CPR`.

---
