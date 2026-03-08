# Ultrasonic Sensor

## Setup

- **Sensor**: HC-SR04
- **Microcontroller**: Arduino


## Important information:

- **Operating Voltage**: 5V DC
- **Distance Coverage**: ( 2cm - 200cm )


## Dangerous Distance:

- Depends on the car’s speed and reaction time
- Every side has his own Dangerous value
- Recommended **range**: **15 cm – 30 cm**

## Pseudocode (Arduino View)

```plaintext
SETUP:
- set TRIG_PIN as OUTPUT
- set ECHO_PIN as INPUT
- begin serial communication at 9600 baud

LOOP (forever):
- set TRIG_PIN LOW
- wait 2–3 microseconds

- set TRIG_PIN HIGH
- wait 10 microseconds
- set TRIG_PIN LOW

- duration = pulseIn(ECHO_PIN, HIGH)

- distance_cm = (duration × 0.034) / 2

- move distance_cm to RaspberryPI through Serial
```

## Pseudocode (RaspberryPI View)

```plaintext

LOOP (forever):

..
..
..

# Front Ultra-Sonic

  if (distance_cm < DANGEROUS_Distance):
    handle_front(distance_cm)

# Right Ultra-Sonic

  if (distance_cm < DANGEROUS_Distance):
    handle_right(distance_cm)

# Left Ultra-Sonic

  if (distance_cm < DANGEROUS_Distance):
    handle_left(distance_cm)
..
..
..

```


