# Differential Drive System

This document covers the engineering basis for the differential drivetrain used in the WRO 2026 Future Engineers vehicle. It explains what a differential is from first principles, the different types available, why we selected an open bevel gear differential, and how it compares to a solid axle approach.

---

## Table of Contents

- [1. What Is a Differential](#1-what-is-a-differential)
- [2. How It Works Mechanically](#2-how-it-works-mechanically)
- [3. Types of Differentials](#3-types-of-differentials)
    - [3.1 Open Differential](#31-open-differential)
    - [3.2 Limited-Slip Differential (LSD)](#32-limited-slip-differential-lsd)
    - [3.3 Locking Differential](#33-locking-differential)
    - [3.4 Torsen (Torque-Sensing) Differential](#34-torsen-torque-sensing-differential)
    - [3.5 Electronic Differential](#35-electronic-differential)
- [4. What We Use](#4-what-we-use)
- [5. Differential vs Solid Axle](#5-differential-vs-solid-axle)
- [6. Effect on Torque Calculations](#6-effect-on-torque-calculations)
- [Sources](#sources)

---

## 1. What Is a Differential

A **differential** is a mechanical assembly that takes a single rotating input shaft and splits its rotation into two output shafts — the two driven wheels — while allowing each output shaft to spin at a **different speed**.

<figure id="fig-3-1" style="text-align: center;"> 
<img src="../materials/moving-diff.gif" /> <figcaption>Fig 3.1 - Moving differential</figcaption> </figure>

This is necessary because in any turn, the outer wheel must travel a longer arc than the inner wheel. If both wheels are forced to rotate at the same speed, one of them must scrub (slide) sideways across the floor to compensate. On a competition surface, this introduces lateral forces that fight the steering and reduce positional precision.

The differential solves this purely through mechanical geometry — no software, no sensors, no active control. The speed difference between the two wheels happens automatically in response to the resistance each wheel encounters.

As you can see in [Fig 3.1](#fig-3-1), when the car moves toward the right, the left (green) shaft starts to move faster.

---

## 2. How It Works Mechanically

The core of a bevel gear differential consists of:

- A **ring gear** (or differential housing), driven by the motor
- Two **side gears**, one per output shaft (one per wheel)
- Two or four **spider gears** (also called planet gears), sitting between the side gears inside the housing

**Straight line:** The spider gears do not rotate on their own axes. Both side gears turn at the same speed as the housing — both wheels spin identically.

**In a turn:** The outer wheel encounters less resistance than the inner wheel (which is being slowed by the tighter arc). This resistance imbalance causes the spider gears to rotate on their own axes, transferring speed from the slower inner wheel to the faster outer wheel. The average speed of the two wheels always equals the input speed from the motor.

> **Key property:** The differential preserves the **average** speed of the two wheels equal to the input shaft speed. It does not change the total torque — it splits it equally between both wheels (minus friction losses).

---

## 3. Types of Differentials

### 3.1 Open Differential

The standard bevel gear differential described above. Torque is split equally between both wheels at all times.

**Characteristics:**

- Simplest and lightest design
- No active components
- If one wheel loses traction completely, all torque goes to that wheel and the other receives none (the weakness of open differentials in off-road use)
- On a flat, consistent competition floor this weakness is irrelevant

**Used in:** Standard RC cars.

### 3.2 Limited-Slip Differential (LSD)

Adds clutch packs or friction cones between the spider gears and the housing. When speed difference between the two wheels exceeds a threshold, the clutch engages and partially locks the differential, sending more torque to the slower wheel.

**Characteristics:**

- Better traction on uneven or slippery surfaces
- Mechanically more complex and heavier
- Unnecessary for a flat competition floor

### 3.3 Locking Differential

A manually or electronically actuated mechanism that rigidly locks both side gears together, forcing both wheels to spin at exactly the same speed.

**Characteristics:**

- Maximum traction in off-road conditions
- Causes severe wheel scrub on hard surfaces when locked
- Functionally equivalent to a solid axle when engaged

### 3.4 Torsen (Torque-Sensing) Differential

Uses worm gears instead of bevel gears. The worm gear geometry is inherently self-locking in one direction, which means torque is biased toward the wheel with more grip automatically and continuously.

**Characteristics:**

- More responsive than clutch-based LSDs
- Higher manufacturing precision required
- Significantly more complex to fabricate at small scale

### 3.5 Electronic Differential

No mechanical differential at all. Two separate motors, one per wheel, with a control system that calculates the required speed difference based on steering angle and IMU data, then applies it via PWM.

**Characteristics:**

- Requires two drive motors — **not compliant with WRO 2026 rules** which require a single driving axle with motors mechanically linked
- Eliminates mechanical complexity but adds software complexity
- Used in some robotic platforms but ruled out for this competition

---

## 4. What We Use

The drivetrain uses a **standard open bevel gear differential**, the simplest configuration that satisfies the WRO single driving axle requirement while eliminating wheel scrub in turns.

**Reasons for selection:**

- **Rule compliance:** A single motor driving both wheels through a mechanical linkage fully satisfies the WRO 2026 single driving axle requirement.
- **Sufficient for competition surface:** The WRO arena is a flat, consistent floor. The open differential's traction limitation under wheel spin is not a relevant failure mode here.
- **Lowest complexity:** No clutch packs, no active locking, no electronics required in the drivetrain itself.
- **Weight and size:** Open differentials at RC car scale are compact and light, fitting comfortably within the 300 × 200 mm chassis constraint.

**Gear ratio through the differential:** 1:1. The differential does not change the overall gear ratio — torque multiplication happens upstream in the motor's internal gearbox, not in the differential.

---

## 5. Differential vs Solid Axle

A **solid axle** (also called a locked axle) rigidly connects both wheels to the same shaft. Both wheels are forced to spin at identical speeds at all times.

|Property|Open Differential|Solid Axle|
|---|---|---|
|Turning behaviour|Clean — wheels rotate at natural arc speeds|Wheel scrub — one or both wheels slide sideways|
|Positional accuracy|High|Lower — scrub introduces lateral displacement error|
|Mechanical complexity|Moderate|Simple|
|Torque losses|~5–10% (gear mesh friction)|Minimal|
|Odometry accuracy|Good — encoder reads average of both wheels|Good on straights, error accumulates in turns|
|Parallel parking precision|High|Reduced due to turn scrub|
|Weight|Slightly heavier|Lighter|
|Used by top WRO teams|Yes — consistently at international level|Occasionally at national level|


**Why wheel scrub matters for this competition specifically:**

The obstacle challenge requires the robot to pass traffic sign pillars on a specific side and then execute a precise parallel park. Wheel scrub during turns introduces lateral displacement that is difficult to compensate for in software because it is not consistent — it varies with speed, floor friction, and tire compound. A differential eliminates this variable entirely.

<figure id="fig-3-1" style="text-align: center;"> 
<img src="../materials/wheel-distance.gif" /> <figcaption>Fig 3.2 - Wheels turning distances</figcaption> </figure>

> **Note:** Several teams that placed well in the open challenge with a solid axle struggled with parking precision in the obstacle challenge. The lateral displacement from wheel scrub under turns is the primary cause.

---

## 6. Effect on Torque Calculations

Two separate effects must be distinguished:

**Friction losses (efficiency):** The differential's gear mesh and bearing friction consumes some torque before it reaches the wheels. This is already accounted for in the drivetrain efficiency factor (η = 0.70) used in 01-motor-selection-criteria, which explicitly lists differential gears as one of the loss sources.

**Gear ratio:** A standard bevel gear differential is always 1:1. It does not multiply or reduce torque. Torque at the two output shafts equals torque at the input shaft minus friction losses — split equally between both wheels.

Increasing or decreasing the physical size of the differential's internal bevel gears does not change the 1:1 ratio. Size only marginally affects the friction losses. To achieve gear reduction, a separate gear stage must be placed before or after the differential — not inside it.

**Net effect on motor selection:** The differential adds no torque multiplication and does not change the minimum stall torque requirement of ≥ 3.5 kg·cm calculated in 01-motor-selection-criteria.

---

## Sources

| Source                                                                                                                            | Description                                                     |
| --------------------------------------------------------------------------------------------------------------------------------- | --------------------------------------------------------------- |
| [LazyGo WRO 2025 — Bangladesh](https://github.com/A-N-M-Noor/LazyGo_WRO2025)                                                      | Reference team differential implementation and justification    |
| [Nerdvana Cancer WRO 2024 — Romania](https://github.com/mihaipriboi/WRO_Future_Engineers_2024)                                    | Reference team differential implementation                      |
| [WRO Future Engineers Getting Started Guide](https://world-robot-olympiad-association.github.io/future-engineers-gs/p01-chassis/) | Official guidance on chassis and drivetrain approaches          |
| [WRO 2026 Future Engineers Rules](https://wro-association.org)                                                                    | Single driving axle requirement, vehicle size constraints       |
| Shigley's Mechanical Engineering Design                                                                                           | Differential gear theory, torque splitting, efficiency          |
| Norton, R.L. — Machine Design                                                                                                     | Bevel gear geometry and power transmission                      |
| [Differential \| How does it work? - By saiban](https://youtu.be/nC6fsNXdcMQ?si=Ch_aLKpvTPvUU6L_)                                 | Useful YouTube video explaining how differential actually works |
