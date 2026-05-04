# LEDC — LED Control (PWM) Peripheral

The ESP32's LEDC peripheral is a dedicated PWM signal generator. Despite the name, it is general-purpose — it can drive anything that needs a PWM signal: LEDs, DC motors, servos, buzzers. It provides up to 8 independent channels, each capable of outputting a configurable frequency and duty cycle on any GPIO pin.

---

## Table of Contents

- [What LEDC Actually Is](#what-ledc-actually-is)
- [Architecture: Timers and Channels](#architecture-timers-and-channels)
- [Speed Modes](#speed-modes)
- [Clock Sources](#clock-sources)
- [The Frequency–Resolution Tradeoff](#the-frequencyresolution-tradeoff)
  - [The Math](#the-math)
  - [Tradeoff Table](#tradeoff-table)
  - [Practical Limits for ESP32](#practical-limits-for-esp32)
- [Timer Configuration](#timer-configuration)
- [Channel Configuration](#channel-configuration)
- [Changing the PWM Signal at Runtime](#changing-the-pwm-signal-at-runtime)
  - [Software Duty Update](#software-duty-update)
  - [Hardware Fading](#hardware-fading)
- [Duty Cycle Range and the Overflow Warning](#duty-cycle-range-and-the-overflow-warning)
- [Practical Application: Motor and Servo](#practical-application-motor-and-servo)
- [Common Pitfalls](#common-pitfalls)

---

## What LEDC Actually Is

LEDC generates a **PWM (Pulse Width Modulation)** signal — a square wave that alternates between high and low at a fixed frequency, with a programmable ratio of high-to-low time called the **duty cycle**.

<p align="center">
  <img src="attachments/ledc-api-settings.jpg" alt="PWM duty cycle illustration" width="600"/>
</p>

A 50% duty cycle means the signal is high for exactly half the period and low for the other half. A 75% duty cycle means it is high for three-quarters of each period. This is how LEDC controls intensity, speed, or position — not by changing voltage, but by changing how long the signal stays high within each cycle.

The peripheral is called "LED Control" because that is its primary intended use in Espressif's product line. The hardware is identical for any PWM application.

---

## Architecture: Timers and Channels

LEDC is structured in two layers:

```
Clock Source
    └── Timer (sets frequency + resolution)
            └── Channel (sets duty cycle, linked to a GPIO pin)
```

- **Timers** control the time base — they define the PWM frequency and how many bits of resolution are available for the duty cycle.
- **Channels** ride on top of a timer — they define what fraction of each timer period the signal stays high, and which GPIO the signal outputs on.

On ESP32, there are **4 timers** and **8 channels** per speed mode group, giving 8 usable channels total in the common case. Multiple channels can share one timer, meaning they share the same frequency but have independent duty cycles.

| Resource | Count (ESP32) |
|----------|--------------|
| High speed timers | 4 |
| High speed channels | 8 |
| Low speed timers | 4 |
| Low speed channels | 8 |
| Usable channels (typical) | 8 |


**LEDC_CHANNEL_0** in **LEDC_HIGH_SPEED_MODE** and **LEDC_CHANNEL_0** in **LEDC_LOW_SPEED_MODE** are two different physical channels. The index just identifies the channel within its group.

---

## Speed Modes

The ESP32 LEDC peripheral has two speed modes: **high speed** and **low speed**. This distinction is unique to the original ESP32 — all newer variants (S2, S3, C3, C6, H2, etc.) only have low speed mode.

| Mode | Duty Cycle Update | Mechanism | Availability |
|------|-------------------|-----------|--------------|
| High speed | Glitch-free, automatic | Hardware applies change at next timer overflow | ESP32 only |
| Low speed | Requires software trigger | Driver calls update in the background | All ESP32 variants |

**High speed mode** means that when you change the duty cycle, the hardware waits for the current PWM period to complete before applying the new value. There is no partial pulse — no glitch. The transition is invisible to the connected device.

**Low speed mode** requires the driver to explicitly trigger the update. The ESP-IDF handles this automatically when you call `ledc_update_duty`, but the update may apply mid-period, causing a single irregular pulse at the moment of change. For motors and servos this is imperceptible.

> **Practical note:** For this project's use case (servo steering, DC motor speed), both modes are functionally equivalent. The glitch difference is only relevant in precision LED dimming or high-frequency signal generation.

---

## Clock Sources

The timer needs a clock source to count from. The selected clock source sets the upper bound on how high a PWM frequency can be configured.

### ESP32 Clock Sources

| Clock | Frequency | Speed Mode | Notes |
|-------|-----------|------------|-------|
| APB_CLK | 80 MHz | High / Low | Default, highest frequency capability |
| REF_TICK | 1 MHz | High / Low | DFS compatible — stays accurate during CPU frequency scaling |
| RC_FAST_CLK | ~8 MHz | Low only | Light-sleep compatible, but frequency is approximate |

**`LEDC_AUTO_CLK`** in the configuration tells the driver to select the best available source automatically. This is the correct default for most applications.

> **Important:** On the standard ESP32, each timer has its own clock mux, so different timers can use different sources. On some other variants, all timers share one clock source — mixing clock sources is impossible on those chips.

### Why Clock Source Matters

The maximum PWM frequency achievable is bounded by:

```
f_max = clock_source_frequency / 2^resolution
```

Using RC_FAST_CLK (8 MHz) at 10-bit resolution gives a maximum of only ~7.8 kHz. If you need 20 kHz for a motor, you must use APB_CLK (80 MHz).

---

## The Frequency–Resolution Tradeoff

This is the most important concept in LEDC configuration. **Frequency and duty resolution are not independent — they share the same timer clock budget.**

### The Math

The timer generates PWM by counting clock ticks. One full PWM period consumes exactly `2^resolution` clock ticks. Therefore:

```
frequency = clock_source / 2^resolution

or equivalently:

max_resolution = log2(clock_source / frequency)
```

The timer is a hardware counter that increments by 1 on every clock tick. It counts from `0` up to `2^resolution − 1`, then resets to `0` and begins again. One full count-and-reset cycle is exactly one PWM period.

The duty cycle is controlled by a **compare register**. The hardware continuously compares the running counter against this value:

- Counter **below** compare value → output is **HIGH**
- Counter **above** compare value → output is **LOW**

```
Counter:  0 ──────────────────► 2^resolution
Output:   HIGH ──────────┐      LOW      ┌──► (next period)
                         └───────────────┘
                      compare value
```

Setting the compare value to `512` on a 10-bit timer (which counts to 1023) produces exactly 50% duty cycle. Setting it to `256` produces 25%, and so on.

The frequency follows directly from how long one full count takes:

This is where the tradeoff originates physically — higher resolution means more ticks per period, which means a longer period, which means a lower frequency. You cannot increase both simultaneously because they are both consuming the same clock budget.


Using APB_CLK at 80 MHz:

| Desired Frequency | Max Resolution | Duty Steps |
|-------------------|----------------|------------|
| 1 Hz | ~26 bit | 67 million |
| 100 Hz | ~19 bit | 524,288 |
| 1 kHz | ~16 bit | 65,536 |
| 5 kHz | ~13 bit | 8,192 |
| 20 kHz | ~11 bit | 2,048 |
| 78 kHz | ~10 bit | 1,024 |
| 312 kHz | ~8 bit | 256 |
| 40 MHz | 1 bit | 2 (50% fixed) |

### Tradeoff Table


| Increase | Effect |
|----------|--------|
| Higher frequency | Lower max resolution → coarser duty cycle steps |
| Higher resolution | Lower max frequency → slower PWM period |

Trying to configure an invalid combination (e.g., 20 MHz at 3-bit) causes the driver to log an error and refuse the configuration:

```
E (196) ledc: requested frequency and duty resolution cannot be achieved,
try reducing freq_hz or duty_resolution. div_param=128
```

### Practical Limits for ESP32

The LEDC driver includes a helper `ledc_find_suitable_duty_resolution()` that computes the maximum resolution for a given clock and desired frequency. You can also compute it manually using the formula above.

At 20 kHz (motor PWM): `log2(80,000,000 / 20,000)` = `log2(4000)` ≈ **11.96 → 11-bit max**. Using 10-bit is safe and leaves headroom.

At 50 Hz (servo PWM): `log2(80,000,000 / 50)` = `log2(1,600,000)` ≈ **20.6 → 20-bit max**. Using 16-bit is safe and gives extremely fine pulse width resolution.

---

## Timer Configuration

The timer is configured through `ledc_timer_config_t`. It defines the frequency and resolution for any channel that binds to it. Timer configuration must happen **before** channel configuration — the ESP-IDF documentation explicitly recommends this ordering so the PWM signal appears at the correct frequency from the first moment the GPIO activates.

| Field | Purpose |
|-------|---------|
| `speed_mode` | High or low speed group |
| `timer_num` | Which of the 4 timers (TIMER_0 to TIMER_3) |
| `freq_hz` | Target PWM frequency |
| `duty_resolution` | Bit depth of duty cycle (determines step count) |
| `clk_cfg` | Clock source — use `LEDC_AUTO_CLK` by default |

A timer can be released when no longer needed by setting the `deconfigure` flag in the config struct and calling `ledc_timer_config` again.

> **Global register warning:** Timer settings are hardware registers — they are global across the entire firmware. If two files configure `LEDC_TIMER_0` with different settings, whichever runs last wins. All timer/channel assignments must be centralized in a single header to avoid conflicts.

---

## Channel Configuration

A channel binds a timer to a GPIO and sets an initial duty cycle. From the moment `ledc_channel_config` is called, the PWM signal begins outputting on the specified pin.

| Field | Purpose |
|-------|---------|
| `speed_mode` | Must match the bound timer's speed mode |
| `channel` | Which of the 8 channels (CHANNEL_0 to CHANNEL_7) |
| `timer_sel` | Which timer this channel uses |
| `gpio_num` | Output GPIO pin |
| `duty` | Initial duty cycle value (0 to 2^resolution − 1) |
| `hpoint` | Rising edge offset within the period — almost always 0 |
| `intr_type` | Interrupt on fade end — use `LEDC_INTR_DISABLE` unless fading |

`hpoint` deserves a brief explanation. Normally all channels driven by the same timer start their high pulse at the same moment within the period. `hpoint` shifts the rising edge of a specific channel forward in time. This can reduce instantaneous current spikes when many PWM devices switch simultaneously — not relevant for one or two channels, but useful in larger systems.

---

## Changing the PWM Signal at Runtime

### Software Duty Update

Changing the duty cycle at runtime is a two-step process:

1. **Write the new value** with `ledc_set_duty` — this stages the value but does not apply it yet.
2. **Commit it** with `ledc_update_duty` — this triggers the hardware to pick up the new value.

This separation exists because the update is not instantaneous at the hardware level. The two-step pattern ensures the new duty cycle takes effect in a controlled way rather than potentially corrupting an in-progress cycle.

> **Hardware overflow warning:** On ESP32, if you select the maximum resolution for a timer, the duty cycle value must not be set to exactly `2^resolution`. Setting duty to `1024` on a 10-bit timer causes the internal hardware counter to overflow and produce an incorrect output. Always cap the maximum value at `(2^resolution) − 1` = 1023 for 10-bit.

### Hardware Fading

LEDC has a built-in fade engine that smoothly transitions the duty cycle from one value to another without any CPU involvement. This is an interrupt-driven hardware feature.

To use it, the fade service must be installed once with `ledc_fade_func_install`. After that, fades are configured with one of the following:

| Function | Description |
|----------|-------------|
| `ledc_set_fade_with_time` | Transition to target duty over a specified duration in ms |
| `ledc_set_fade_with_step` | Transition in fixed increments at a fixed interval |
| `ledc_set_fade` | Manual control over step size and cycle count |

After configuring, `ledc_fade_start` begins the transition. It can run in blocking mode (function returns only when fade completes) or non-blocking mode (returns immediately, fade continues in background).

> On ESP32, a fade in progress cannot be stopped before it reaches its target. On other variants, `ledc_fade_stop` can abort it.

A callback can be registered via `ledc_cb_register` to receive a notification when a fade finishes. The callback and any functions it calls must be placed in IRAM using `IRAM_ATTR`, since the callback runs from within an ISR.

---

## Duty Cycle Range and the Overflow Warning

The valid duty cycle range for a channel is `0` to `(2^resolution) − 1`. This maps directly to 0% to 100% of the PWM period.

| Resolution | Max Duty Value | % Step Size |
|------------|----------------|-------------|
| 8-bit | 255 | ~0.39% |
| 10-bit | 1023 | ~0.098% |
| 13-bit | 8191 | ~0.012% |
| 16-bit | 65535 | ~0.0015% |

Setting duty to exactly `2^resolution` on ESP32 causes a hardware counter overflow and corrupts the output signal. The driver does not catch this — it is the programmer's responsibility to clamp values below the maximum.

---

## Practical Application: Motor and Servo

The two devices have very different PWM requirements, which is why they must use separate timers.

| Parameter | Servo | DC Motor (via TB6612FNG) |
|-----------|-------|--------------------------|
| Frequency | 50 Hz | 20 kHz |
| Resolution | 16-bit | 10-bit |
| Timer | LEDC_TIMER_0 | LEDC_TIMER_1 |
| Channel | LEDC_CHANNEL_0 | LEDC_CHANNEL_1 |
| Duty interpretation | Pulse width (500–2500 µs) | Speed percentage (0–100%) |
| Why this frequency | Servo protocol standard | Above 20 kHz human hearing threshold |

**Servo frequency reasoning:** Standard hobby servos use a 20 ms period (50 Hz). The position is encoded not in the duty cycle percentage, but in the absolute pulse width within that period. A 1500 µs pulse is center, 500 µs is one extreme, 2500 µs is the other. At 50 Hz and 16-bit resolution, each step is approximately 0.3 µs — far finer than any servo can mechanically respond to.

**Motor frequency reasoning:** The TB6612FNG PWM input switches the internal H-bridge MOSFETs. If the switching frequency falls in the 20 Hz–20 kHz audible range, the motor coils physically vibrate at that frequency and produce an audible whine. Running at 20 kHz pushes this above human hearing. At 20 kHz, 10-bit resolution gives 1024 speed steps (~0.1% per step), which is more than sufficient for a drive motor.

**Why they cannot share a timer:** A single timer can only have one frequency. A timer set to 50 Hz cannot simultaneously provide 20 kHz for a motor. They must use separate timers.

---

## Common Pitfalls



| Pitfall | Consequence | Fix |
|---------|-------------|-----|
| Duty set to exactly `2^resolution` | Hardware counter overflow, corrupted output | Cap at `(2^resolution) − 1` |
| Two files configure the same timer with different settings | Last write wins, one device gets wrong frequency | Centralize all timer/channel definesone header |
| Calling `ledc_set_duty` without `ledc_update_duty` | New value staged but never applied, output unchanged | Always call both in sequence |
| Motor PWM frequency in audible range (20 Hz–20 kHz) | Audible coil whine | Use ≥ 20 kHz |
| Using RC_FAST_CLK for precise frequency output | Output frequency may be inaccurate — no calibration on most variants | Use APB_CLK (80 MHz) for precision |
| Fade callback not placed in IRAM | Crash or undefined behavior when callback fires from ISR context | Add `IRAM_ATTR` to callback and any functions it calls |
| Multiple channels sharing one timer with different desired frequencies | Impossible — all channels on a timer share its frequency | Use a separate timer per frequency requirement |
