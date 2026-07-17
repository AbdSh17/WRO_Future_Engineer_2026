<p align="center">
  <img src="06_Attachments/Logo_Team.png" alt="Team Lannister Logo" width="350"/>
</p>

<h1 align="center">Team Lannister — WRO 2026 Future Engineers</h1>

<p align="center">
  <em>A self-driving car, built from scratch.</em>
</p>

<p align="center">
  <em>Hear me Roar.</em>
</p>

---

## Table of Contents

- [1. The Team](#1-the-team)
- [2. The Challenge](#2-the-challenge)
- [3. Our Approach — Drivetrain and Steering](#3-our-approach--drivetrain-and-steering)
- [4. Hardware Overview](#4-hardware-overview)
  - [4.1 The Two Brains: ESP32 + Raspberry Pi 5](#41-the-two-brains-esp32--raspberry-pi-5)
  - [4.2 Vision: Raspberry Pi Camera Module 3](#42-vision-raspberry-pi-camera-module-3)
  - [4.3 Bill of Materials](#43-bill-of-materials)
  - [4.4 Power Architecture](#44-power-architecture)
  - [4.5 Wiring](#45-wiring)
- [5. Software](#5-software)
- [6. Repository Map](#6-repository-map)
- [7. Build & Run](#7-build--run)

---

## 1. The Team

<table align="center">
<tr>
<td align="center" width="380">
<img src="06_Attachments/team_photo_serious.jpg" width="360"/><br/>
<sub><b>The official photo</b></sub>
</td>
<td align="center" width="380">
<img src="06_Attachments/team_photo_funny.jpg" width="360"/><br/>
<sub><b>The one that actually looks like us</b></sub>
</td>
</tr>
</table>

---

<table align="center">
<tr>
<td align="center" width="240">
<img src="06_Attachments/abdalraheem_photo.jpg" width="200"/><br/>
<sub><b>Abdalraheem Shuaibi</b></sub><br/>
<sub>Hardware & PCB Design / Software</sub>
</td>
</tr>
</table>

I'm a 21-year-old Computer Engineering student at Birzeit University. I started learning embedded systems and firmware development about two years ago — largely self-taught, out of necessity. Structured resources for this kind of work are scarce where I'm from, so my path went from basic circuits and electronics theory all the way to PCB design, and from simple Arduino sketches to FreeRTOS and ESP-IDF, the same stack powering this robot.

Alongside this competition, I've been juggling a graduation project funded by the EU and the Palestinian Ministry of Health — a commitment that's demanded careful time management this season.

This isn't my first attempt at WRO Future Engineers. I competed alone in the 2025 season and things were going well, until a sensor burned out just before the local competition, ending that run early. That experience is a big part of why this repository exists in the form it does now — more testing, more documentation, fewer single points of failure.

---

<table align="center">
<tr>
<td align="center" width="240">
<img src="06_Attachments/second_member_photo.jpg" width="200"/><br/>
<sub><b><em>[Name Pending]</em></b></sub><br/>
<sub><em>[Role Pending]</em></sub>
</td>
</tr>
</table>

*[Bio pending]*

---

## 2. The Challenge

<p align="center">
  <img src="06_Attachments/WRO_Mat.jpg" alt="WRO 2026 Future Engineers game field" width="600"/>
</p>

WRO Future Engineers asks for a vehicle that can drive itself — no remote control, no shortcuts, just sensors, math, and code making decisions in real time. The car has to complete laps around a track that changes every round: walls move, corners narrow, and in the Obstacle Challenge, coloured pillars appear that the car has to read and respond to correctly. Get it wrong, and the round ends early.

It's a small, contained problem on paper. In practice, it touches almost every corner of engineering: mechanical design, power distribution, real-time firmware, computer vision, and the occasional very frustrating GPIO conflict at 2 AM.

---

## 3. Our Approach — Drivetrain and Steering

Two mechanical decisions shaped everything else about this robot: how it turns, and how power reaches the wheels.

### Differential Drive

A single rear motor drives both back wheels through an open bevel-gear differential. The reason is simple — in every turn, the outer wheel has to travel further than the inner one. Without a differential, one wheel is forced to scrub sideways against the floor to compensate, and that scrub is unpredictable: it changes with speed, floor grip, and tire wear. None of that is something you want to fight in software.

<table align="center">
<tr>
<td align="center" width="400">
<img src="00_Research/materials/moving-diff.gif" width="380"/><br/>
<sub><b>The differential in motion</b></sub>
</td>
<td align="center" width="400">
<img src="00_Research/materials/wheel-distance.gif" width="380"/><br/>
<sub><b>Inner vs. outer wheel arc distance</b></sub>
</td>
</tr>
</table>

The differential solves this purely mechanically — no sensors, no control loop, just gears responding to resistance. Full reasoning and the comparison against a solid axle is in [`00_Research/04_mechanical/01_Differential.md`](00_Research/04_mechanical/01_Differential.md).

### Ackermann Steering

The front wheels don't turn together at the same angle — they can't, for the same reason the rear wheels can't spin at the same speed in a turn. Ackermann geometry angles the steering linkage so the inner wheel always turns more sharply than the outer one, keeping every wheel pointed at a common turn center.

<table align="center">
<tr>
<td align="center" width="400">
<img src="00_Research/materials/ackermann-diagram.gif" width="380"/><br/>
<sub><b>Ackermann geometry in action</b></sub>
</td>
<td align="center" width="400">
<img src="00_Research/materials/ackermann-trapezoid.gif" width="380"/><br/>
<sub><b>The trapezoid linkage</b></sub>
</td>
</tr>
</table>

This matters more than it might sound like. Zero scrub means the IMU-based heading controller only has to correct for genuine drift — not fight a lateral displacement that changes every lap. Full geometry, math, and the comparison against parallel steering is documented in [`00_Research/04_mechanical/02_ackermann-steering.md`](00_Research/04_mechanical/02_ackermann-steering.md).

---

## 4. Hardware Overview

### 4.1 The Two Brains: ESP32 + Raspberry Pi 5

The robot runs on two separate computers, each doing a different job.

<table align="center">
<tr>
<td align="center" width="260">
<img src="06_Attachments/ESP32_animation.gif" width="220"/><br/>
<sub><b>ESP32 — real-time control</b></sub>
</td>
<td align="center" width="260">
<img src="06_Attachments/RPI5_animation.gif" width="220"/><br/>
<sub><b>Raspberry Pi 5 — vision</b></sub>
</td>
</tr>
</table>

The **ESP32** handles everything that has to happen in real time and can't afford to be late: reading the ToF sensors, reading the IMU, driving the motor, driving the steering servo, and running the state machine that decides what the car does next. It runs FreeRTOS, with each task — sensing, control, algorithm — kept strictly separated so a slow camera frame on the other board can never introduce jitter into a steering correction.

The **Raspberry Pi 5** is reserved for vision. Camera processing is comparatively heavy and doesn't need to run at the same tight, predictable cadence as motor control — so it lives on its own board entirely, talking to the ESP32 over UART.

<p align="center">
  <img src="06_Attachments/ESP2RPI_Connection.jpg" alt="ESP32 to Raspberry Pi connection" width="450"/>
</p>
<p align="center"><sub><b>How the two boards talk to each other</b></sub></p>

Splitting the compute this way means a heavy vision frame never stalls a steering correction, and a firmware crash on the ESP32 doesn't take the camera pipeline down with it.

### 4.2 Vision: Raspberry Pi Camera Module 3

<table align="center">
<tr>
<td align="center" width="220">
<img src="06_Attachments/Raspberry_Pi_Camera.jpg" width="180"/><br/>
<sub><b>Camera Module 3, wide-angle</b></sub>
</td>
</tr>
</table>

We're using the **Raspberry Pi Camera Module 3**, wide-angle variant (~120° diagonal FoV), connected over the Pi 5's CSI port. The wide field of view matters here — the car needs to see pillars and track edges well before it's close enough for a narrow lens to catch them, giving the vision pipeline more time to make a decision.

<table align="center">
<tr>
<td align="center" width="400">
<img src="06_Attachments/connect-camera.gif" width="380"/><br/>
<sub><b>Connecting the camera</b></sub>
</td>
</tr>
</table>

### 4.3 Bill of Materials

Every component on this robot was chosen against a calculated requirement first, not picked off a shelf. The motor's stall torque, the servo's stall torque, even the capacitor and resistor values, all trace back to a number derived in [`00_Research`](00_Research) before a single part was ordered. Below is a walk through the build, grouped the way we actually think about it: what thinks, what senses, what moves, and what keeps everything alive.

---

**Compute — the two things that think**

<table align="center">
<tr>
<td align="center" width="240">
<img src="01_Hardware/materials/raspberry-pi-5.jpg" width="200"/><br/>
<sub><b>Raspberry Pi 5</b></sub>
</td>
<td align="center" width="240">
<img src="01_Hardware/materials/esp32-devkitc-32d.jpg" width="200"/><br/>
<sub><b>ESP32-DEVKITC-32D</b></sub>
</td>
</tr>
</table>

The **Raspberry Pi 5** handles vision — capturing and processing camera frames, deciding where pillars are. The **ESP32-DEVKITC-32D** handles everything that needs to happen on a strict, predictable schedule: reading sensors, driving the motor, driving the servo, and running the state machine. Splitting these two jobs across two boards means a heavy vision frame can never introduce jitter into a steering correction.

---

**Sensors — the three things that feel the track**

<table align="center">
<tr>
<td align="center" width="240">
<img src="01_Hardware/materials/bno055-imu.jpg" width="200"/><br/>
<sub><b>BNO055 IMU</b></sub>
</td>
<td align="center" width="240">
<img src="01_Hardware/materials/vl53l0x-tof.jpg" width="200"/><br/>
<sub><b>VL53L0X ToF ×3</b></sub>
</td>
</tr>
</table>

The **BNO055 IMU** gives the robot an absolute sense of heading — it's what lets the car hold a straight line and execute a clean 90° turn without drifting. Three **VL53L0X ToF sensors** (front, left, right) act as the robot's proximity sense — detecting walls, corners, and when it's time to turn. A **hall-effect encoder** on the drive shaft rounds this out, giving distance and speed feedback for odometry.

---

**Actuators — the two things that move the car**

<table align="center">
<tr>
<td align="center" width="240">
<img src="01_Hardware/materials/jgb37-520-motor.png" width="200"/><br/>
<sub><b>JGB37-520 DC motor</b></sub>
</td>
<td align="center" width="240">
<img src="01_Hardware/materials/mg996r-servo.jpg" width="200"/><br/>
<sub><b>MG996R servo</b></sub>
</td>
</tr>
</table>

The **JGB37-520 DC gear motor** drives the rear axle through the differential — sized so its stall torque clears ≥ 0.6 kg·cm, with full derivation in [`01_motor-selection-criteria.md`](00_Research/03_components_selection/01_motor-selection-criteria.md). The **MG996R servo** handles Ackermann steering up front, sized to clear ≥ 9.3 kg·cm of stall torque against tire scrub — worked out in [`02_servo-torque-requirement.md`](00_Research/03_components_selection/02_servo-torque-requirement.md).

---

**Power — what keeps it all alive**

<table align="center">
<tr>
<td align="center" width="240">
<img src="01_Hardware/materials/li-ion-3s-battery.jpg" width="200"/><br/>
<sub><b>3S Li-Ion battery</b></sub>
</td>
<td align="center" width="240">
<img src="01_Hardware/materials/mp1584en-buck.jpg" width="200"/><br/>
<sub><b>MP1584EN buck converter</b></sub>
</td>
<td align="center" width="240">
<img src="01_Hardware/materials/tb6612fng-driver.webp" width="200"/><br/>
<sub><b>TB6612FNG driver</b></sub>
</td>
</tr>
</table>

A single **3S Li-Ion battery** feeds everything. Two **MP1584EN buck converters** split that into an isolated 5V logic rail and a 6V servo rail — kept separate on purpose, more on that below. The **TB6612FNG** is the H-bridge that lets the ESP32's low-current logic pins control the drive motor's much higher current draw.

The full parts list, quantities, and every link back to its underlying calculation lives in [`01_Hardware/01_bill-of-materials.md`](01_Hardware/01_bill-of-materials.md).

---

### 4.4 Power Architecture

<p align="center">
  <img src="06_Attachments/power_system_architecture.drawio.png" alt="Power system architecture" width="700"/>
</p>
<p align="center"><sub><b>The full power rail layout, battery to load</b></sub></p>

The robot runs on a single 3S Li-Ion battery feeding two independent MP1584EN buck converters — one 5V rail for logic (ESP32, sensors), one 6V rail dedicated entirely to the steering servo. The drive motor draws directly from the battery through the TB6612FNG, bypassing both regulators entirely since the driver handles its own switching.

**Why isolate the servo?** Its stall current spikes to roughly 2.5A when it hits mechanical resistance — over eight times the entire logic rail's typical draw. If that spike shared a rail with the ESP32, it could sag the voltage enough to brown-out reset the microcontroller mid-turn. Isolating the rail means a stall event only affects the servo — the ESP32 keeps running, uninterrupted.

**Why capacitors matter here:** every regulator and every IC on these rails leans on a decoupling or bulk capacitor to survive sudden current demand. A capacitor, at its core, behaves like a small local reservoir of charge — it can dump current instantly in a way a battery several centimeters away, through resistive wire, simply cannot react to fast enough.

<table align="center">
<tr>
<td align="center" width="440">
<img src="06_Attachments/Capacitor-as-DC-Voltage-Source.gif" width="420"/><br/>
<sub><b>A capacitor acting as a local DC source</b></sub>
</td>
</tr>
</table>

That's the whole idea behind decoupling: place a capacitor close enough to a hungry IC or a switching regulator that it becomes the fast-response source, while the battery handles the slow, steady-state supply. Full current budget, capacitor sizing per rail, and the math behind all of it is documented in [`01_Hardware/02_power-architecture.md`](01_Hardware/02_power-architecture.md), backed by the passive-component calculations in [`00_Research/03_components_selection/03_passive-components.md`](00_Research/03_components_selection/03_passive-components.md).

---

### 4.5 Wiring

<p align="center">
  <img src="04_PCB/Schematic_WRO_2026.png" alt="Full circuit schematic" width="700"/>
</p>
<p align="center"><sub><b>The full circuit, as wired on the robot today</b></sub></p>

This is the full schematic — every sensor, driver, and regulator wired exactly as it sits on the robot today. Two separate I2C buses keep the ToF sensors (which need XSHUT address remapping) electrically apart from the IMU, so a slow or stuck sensor on one bus can never stall the other. The motor driver, servo PWM, encoder, and the reserved Pi↔ESP32 UART lines round out the rest of the GPIO map.

Every pin assignment below is pulled directly from source code, not assumed — full pin-by-pin detail lives in [`01_Hardware/03_wiring.md`](01_Hardware/03_wiring.md).

---

## 5. Software

*This section is being actively developed. It will include the FreeRTOS task architecture diagram, the algorithm state machine flowchart, and use-case scenarios for both the Open and Obstacle Challenges.*

---

## 6. Repository Map

| Folder | Contents |
|---|---|
| [`00_Research`](00_Research) | The reasoning behind every decision — component selection criteria, mechanical calculations, comparisons |
| [`01_Hardware`](01_Hardware) | Final bill of materials, power architecture, and wiring — what was actually built |
| [`02_Software`](02_Software) | Firmware and vision source code |
| [`03_Models`](03_Models) | CAD and 3D print files |
| [`04_PCB`](04_PCB) | Schematics and PCB design files |
| [`05_Media`](05_Media) | Robot and team photos, video links |
| [`06_Attachments`](06_Attachments) | Logo, diagrams, and supporting animations used across this documentation |

---

## 7. Build & Run

*Build, compile, and upload instructions will be added here once the firmware and vision pipeline reach a stable release.*

---

<p align="center"><em>More to come as the build progresses.</em></p>
