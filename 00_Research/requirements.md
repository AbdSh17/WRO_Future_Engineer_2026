# Requirements 

## Table of Contents


## Car
#### - Physical Requirements
    - Dimensions: Max 300mm (L) × 200mm (W) × 300mm (H).
    - Weight: Max 1.5 kg.
    - Wheels: 4 wheels (no omnidirectional, ball casters, or spherical wheels).
    - Must have one driving axle
    - Steering actuator (servo motor)

#### - Autonomy & Electronics
    - Autonomous Only (No remote control, Wi-Fi, Bluetooth, or external signals during runs.)
    - **Sensors**: Any type allowed (e.g., cameras, ultrasonic, IMU, line sensors).
    - **Motors**:
        - Max 2 driving motors (must be mechanically linked to wheels via gears/axles).
        - Additional motors allowed for steering (servos)

#### - Power & Safety
    - **Batteries**: Any type allowed (no fuel cells).
    - **Switches**: **One power button** to turn on the vehicle, **One start button** (separate from power) to begin the program.
    -
 
#### - Controllers
    - one Raspberry Pi + one Arduino, or two ESP32s
    - If two, communications wireless between them is perhebitied
    
#### - Prohibited Designs
    - Differential drives (e.g., two motors controlling left/right wheels independently).
    - Omnidirectional wheels (e.g., mecanum).
    - Pre-built commercial robot kits unless significantly modified.


## Plagiarism
#### - Copied Code
    - Using code from online sources (e.g., GitHub, tutorials) without significant modification.
    - Using unmodified commercial robot kits (e.g., ready-made self-driving car platforms).
    - Modular kits (e.g., LEGO Mindstorms) are allowed only if the team designs unique mechanics/code.
    - First GITHUB commit must contain ≥20% of the final code.

## Required Sensors & motors
    - Infrared Line Sensors
    - Raspberry Pi Camera Module v3 (Wide-Angle)
    - TOF
    
    - TT Gear Motor (compatible with motor drivers, built-in encoder for speed feedback.) or Pololu 37D Metal Gearmotor
    - Servo Motor
