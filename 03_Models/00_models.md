# Models

Every custom mechanical part of the vehicle, designed in Autodesk Fusion 360 and 3D-printed. This folder holds the print-ready files, the dimensioned engineering drawings, and the reasoning behind the design — what each part does, and why it looks the way it does.

## Contents

| Path | Description |
|------|-------------|
| [`stl/`](stl/) | Print-ready STL files for all 16 custom parts |
| [`drawings/`](drawings/) | Dimensioned multi-view engineering drawings (PDF), exported from Fusion 360 |
| [`materials/`](materials/) | Images used by this document |
| `00_models.md` | This file — the design documentation |

---

## 1. Design Overview

The printed structure implements the two mechanical decisions explained in the [root README](../README.md#3-our-approach--drivetrain-and-steering): **rear-wheel drive through an open differential**, and **front Ackermann steering**.

A single [JGB37-520 12V gear motor](../01_Hardware/01_bill-of-materials.md) drives the rear axle through a custom bevel pinion meshing with a WLTOYS 144001 differential — an off-the-shelf RC part carried by our own printed mounts. Steering is a single MG996R servo whose custom arm drives a tie rod connecting the two steering knuckles. Everything mounts on one printed chassis plate, with raised platforms creating a second floor for the electronics.

Why these mechanisms were chosen over the alternatives (solid axle, parallel steering) is documented in the research folder:
- [`00_Research/04_mechanical/01_Differential.md`](../00_Research/04_mechanical/01_Differential.md)
- [`00_Research/04_mechanical/02_ackermann-steering.md`](../00_Research/04_mechanical/02_ackermann-steering.md)

This document covers how those decisions became physical parts.

---

## 2. The Chassis — One Plate, Three Levels

The vehicle regulations cap the footprint at 300 × 200 mm, so the layout grows *upward* instead of outward. The chassis (`main_base`, 254 × 100 mm) is a single printed plate that carries everything, with integrated columns supporting two second-floor platforms:

| Level | Part | Carries |
|-------|------|---------|
| Ground plate | `main_base` | Motor (mount extruded directly into the plate), differential assembly, steering knuckles, all columns |
| Front deck | `front_top` | MG996R steering servo, camera mast |
| Middle deck | `middle_top` | The main circuit board (soldered prototyping PCB — see [`04_PCB`](../04_PCB/00_PCB.md)) |
| Rear deck | `back_top` | Raspberry Pi 5 and its power bank |

Two decisions worth noting:

**The motor mount is part of the chassis, not a separate bracket.** The JGB37-520 sits in a pocket extruded directly from `main_base`. One part fewer to print, no bracket-to-chassis joint to work loose from vibration, and the motor's alignment to the differential is fixed by the geometry of a single part instead of depending on assembly accuracy.

**Electronics live on removable decks, not on the chassis.** The PCB deck and the Pi deck each come off with a few screws, so electrical work never requires touching the drivetrain — and mechanical work never requires unscrewing electronics. During a competition day, that separation is the difference between a five-minute fix and a full teardown.

The camera sits on a dedicated mast (`camera_holder`, 154.7 mm tall) mounted on the front deck, raising the [Camera Module 3](../README.md#42-vision-raspberry-pi-camera-module-3) high enough to see the track over the vehicle's own body, with the mounting frame angled downward toward the field.

---

## 3. Steering — Ackermann in Plastic

The steering linkage is a classic Ackermann trapezoid built from four printed parts: two knuckles, a tie rod, and a custom servo arm.

- Each **knuckle** (`left_knuckle`, `right_knuckle` — exact mirrors) pivots on a kingpin at the chassis, carries the front wheel axle, and has a tie-rod eye offset behind the kingpin axis.
- The **tie rod** (`ackerman_rod`) connects the two knuckle eyes, with a third linkage point at its center.
- The **servo arm** (`servo_arm`) mounts on the MG996R's splined output and drives the center of the tie rod.

The Ackermann effect comes from where the tie-rod eyes sit: they are angled inward so that the line through each kingpin and its tie-rod eye points toward the rear axle. Verified directly in CAD:

<p align="center">
  <img src="materials/ackerman_geometry_verification.png" width="320"/>
</p>
<p align="center"><sub><b>Top view in Fusion 360 — construction lines from the steering pivots converge at the rear axle centerline, the Ackermann condition.</b></sub></p>

The result: in every turn the inner wheel steers more sharply than the outer one, all four wheels track around a common turn center, and no tire scrubs. The math behind the geometry is in [`02_ackermann-steering.md`](../00_Research/04_mechanical/02_ackermann-steering.md); the torque requirement that selected the MG996R is in [`02_servo-torque-requirement.md`](../00_Research/03_components_selection/02_servo-torque-requirement.md).

---

## 4. Drivetrain — Carrying a Differential That Wasn't Made for This Car

The WLTOYS 144001 differential is designed for a specific RC car, so the entire mounting system around it is custom:

- **`differantial_gear`** — bevel pinion pressed onto the JGB37-520 output shaft, meshing with the differential's crown gear. This is the single point where motor torque enters the axle.
- **`left_differantial_ring`, `right_differantial_ring`** — carrier rings bolted to the chassis; each seats one side of the differential by its bearing.
- **`differantial_holder`** — a 2.25 mm retaining ring glued to the right carrier ring (see [Iteration 1](#iteration-1--eliminating-differential-axial-play)).
- **`differantial_cup`** — drive cup coupling the differential output to the rear drive shaft.
- **`left_shaft_holder`, `right_shaft_holder`** — outboard bearing blocks supporting each 4 mm drive shaft near the wheel (see [Iteration 2](#iteration-2--stabilizing-the-rear-wheels)).

Torque path: motor → bevel pinion → crown gear → differential → drive cups → 4 mm shafts → rear wheels. The differential splits torque between the wheels purely mechanically, which keeps us compliant with rule 11.5 (electronic differentials — one motor per side — are banned) while still letting the rear wheels rotate at different speeds in corners.

---

## 5. Design Iterations

The drivetrain went through two revisions before it worked. Both are visible in the current parts.

### Iteration 1 — Eliminating differential axial play

**Problem.** The differential sat between the two carrier rings with a small axial gap, so it could slide sideways ("wiggle") between them. A stable gear mesh needs the differential held at a fixed axial position.

**Solutions considered and rejected.**
- *Thicken the left carrier ring inward* — rejected: the motor's bevel pinion occupies that space.
- *Thicken the right carrier ring inward* — rejected: it would collide with the gear on that side.

**Chosen solution.** A separate thin retaining ring (`differantial_holder`, 2.25 mm) glued with cyanoacrylate to the right carrier ring. It surrounds the differential's bearing and fills the gap without intruding into the space either gear needs.

**Result.** No axial play, full gear clearance on both sides. The packaging constraint blocked both obvious fixes and forced a third one.

### Iteration 2 — Stabilizing the rear wheels

**Problem.** Originally each 4 mm drive shaft was supported only at its inboard end, at the differential carrier. The wheel hung on the free end of a cantilever — any play at the single support was amplified at the wheel, and the rear wheels visibly tilted and wobbled.

**Solution.** An outboard bearing block per side (`left_shaft_holder`, `right_shaft_holder`), each holding a bearing that carries the shaft next to the wheel. Every rear shaft is now supported at two points.

**Result.** The wheels run true. Load is shared between the carrier rings and the shaft holders instead of the rings bending alone.

---

## 6. Parts List

| # | Part | Qty | Bounding box (mm) | Function |
|---|------|-----|-------------------|----------|
| 1 | `main_base.stl` | 1 | 254.0 × 100.0 × 55.0 | Chassis plate. Integrated JGB37-520 motor mount, mounting points for every subassembly, columns for the upper decks. |
| 2 | `front_top.stl` | 1 | 90.1 × 115.0 × 25.5 | Front deck: holds the MG996R servo and the camera mast. |
| 3 | `middle_top.stl` | 1 | 100.0 × 75.0 × 16.9 | Middle deck: carries the main soldered PCB. |
| 4 | `back_top.stl` | 1 | 100.0 × 68.0 × 22.0 | Rear deck: carries the Raspberry Pi 5 and power bank. |
| 5 | `camera_holder.stl` | 1 | 40.0 × 24.1 × 154.7 | Camera mast; holds the Camera Module 3 at height, angled at the track. |
| 6 | `ackerman_rod.stl` | 1 | 76.9 × 8.0 × 8.0 | Steering tie rod linking both knuckles, driven at its center by the servo arm. |
| 7 | `left_knuckle.stl` | 1 | 17.1 × 47.8 × 12.0 | Left steering knuckle: kingpin pivot, wheel axle, tie-rod eye. |
| 8 | `right_knuckle.stl` | 1 | 17.1 × 47.8 × 12.0 | Right steering knuckle, exact mirror of the left. |
| 9 | `servo_arm.stl` | 1 | 33.5 × 12.0 × 7.5 | Custom arm on the servo's splined output, linking to the tie-rod center. |
| 10 | `differantial_gear.stl` | 1 | 14.5 × 15.9 × 14.5 | Bevel pinion on the motor shaft; drives the differential crown gear. |
| 11 | `left_differantial_ring.stl` | 1 | 21.9 × 40.0 × 17.0 | Left differential carrier: bearing seat, bolts to chassis. |
| 12 | `right_differantial_ring.stl` | 1 | 21.9 × 40.0 × 17.0 | Right differential carrier, mirror of the left. |
| 13 | `differantial_holder.stl` | 1 | 2.2 × 16.6 × 16.6 | Retaining ring eliminating differential axial play (Iteration 1). |
| 14 | `differantial_cup.stl` | 1 | 28.2 × 13.2 × 13.2 | Drive cup coupling differential output to the rear shaft. |
| 15 | `left_shaft_holder.stl` | 1 | 4.0 × 28.0 × 13.0 | Left outboard bearing block for the 4 mm drive shaft (Iteration 2). |
| 16 | `right_shaft_holder.stl` | 1 | 4.0 × 28.0 × 13.0 | Right outboard bearing block, mirror of the left. |

Total printed volume (solid): ≈ 170 cm³.

---

## 7. Technical Drawings

Dimensioned multi-view drawings for every part, exported from Fusion 360, in [`drawings/`](drawings/):

| Drawing | Covers |
|---|---|
| [`main_base.pdf`](drawings/main_base.pdf) | Chassis plate |
| [`front_top.pdf`](drawings/front_top.pdf) | Front deck |
| [`mid_top.pdf`](drawings/mid_top.pdf) | Middle deck |
| [`back_top.pdf`](drawings/back_top.pdf) | Rear deck |
| [`camera_holder.pdf`](drawings/camera_holder.pdf) | Camera mast |
| [`ackerman_rod.pdf`](drawings/ackerman_rod.pdf) | Steering tie rod |
| [`knuckles.pdf`](drawings/knuckles.pdf) | Left & right knuckles |
| [`servo_arm.pdf`](drawings/servo_arm.pdf) | Servo arm |
| [`motor_gear.pdf`](drawings/motor_gear.pdf) | Bevel pinion |
| [`differantial_cup.pdf`](drawings/differantial_cup.pdf) | Drive cup |
| [`differantial_rings&holder.pdf`](drawings/differantial_rings%26holder.pdf) | Carrier rings & retaining ring |
| [`shaft_holder.pdf`](drawings/shaft_holder.pdf) | Outboard bearing blocks |

---

## 8. Vehicle Regulation Compliance

| Rule | Requirement | Our design |
|------|-------------|-----------|
| 11.1 | Footprint ≤ 300 × 200 mm, height ≤ 300 mm | Chassis 254 × 100 mm; tallest point (camera mast on chassis) well under the height limit |
| 11.2 | Weight ≤ 1.5 kg | All printed parts ≈ 170 cm³ solid volume — roughly 100–130 g at typical infill, leaving ample margin for electronics and battery |
| 11.3 | One driving axle, one steering actuator | Single rear axle driven by one JGB37-520; single MG996R steering servo |
| 11.5 | No electronic differential | Mechanical (WLTOYS 144001) differential — one motor, gears split the torque |

*Final assembled dimensions and weight will be added after the vehicle is complete.*

---

## 9. Printing Notes

Parts were printed across several different printers (printing was distributed among team helpers), which imposed a design constraint of its own: hole and slot fits carry clearance so parts remain assemblable despite printer-to-printer tolerance differences.

*Per-part material and settings summary: pending. Editable source files (.f3d / .step) will be added to a `source/` folder.*
