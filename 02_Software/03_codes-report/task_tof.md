# task_tof

## Overview

`task_tof` periodically reads all three ToF sensors (front, left, right) and publishes their readings as a single atomic `tof_state_t` snapshot protected by a mutex. It is the sole writer of the ToF state. Any other task that needs distance data calls `tof_read()`.

---

## Shared State

All shared data is packed into a single struct:

```c
typedef struct {
    uint8_t  left_mm;
    uint16_t front_mm;
    uint8_t  right_mm;
    bool     wall_left;
    bool     wall_front;
    bool     wall_right;
    bool     front_emergency_wall;
    bool     front_warning_wall;
    bool     valid;
    int64_t  ts_us;
} tof_state_t;
```

| Field | Description |
|---|---|
| `left_mm` | Left sensor reading in mm (VL6180X, range 0–255) |
| `front_mm` | Front sensor reading in mm (VL53L0X, range 0–2000) |
| `right_mm` | Right sensor reading in mm (VL6180X, range 0–255) |
| `wall_left` | True if left distance ≤ `WALL_LEFT_MM` |
| `wall_front` | True if front distance ≤ `WALL_FRONT_MM` |
| `wall_right` | True if right distance ≤ `WALL_LEFT_MM` |
| `front_warning_wall` | True if front distance ≤ `WALL_WARNING_MM` — slow down |
| `front_emergency_wall` | True if front distance ≤ `WALL_EMERGENCY_MM` — stop immediately |
| `valid` | True once the first successful read has occurred |
| `ts_us` | Timestamp of the last successful write in microseconds |

> The boolean flags are computed inside `tof_write()` before the mutex is taken, so the lock is held only for the struct copy — not for the threshold comparisons.

---

## Threshold Defines

| Define | Value | Meaning |
|---|---|---|
| `WALL_LEFT_MM` | 150 | Side wall detection threshold |
| `WALL_FRONT_MM` | 200 | Front wall detection threshold |
| `WALL_WARNING_MM` | 150 | Front warning zone |
| `WALL_EMERGENCY_MM` | 80 | Front emergency stop zone |

**Note:** These thresholds are starting values and will need tuning on the actual track.

---

## Functions

### `tof_task_start(ToF *tof, SemaphoreHandle_t mutex)`

Entry point called from `app_main`. Stores the ToF object pointer and mutex into module-level statics, then spawns the `tof_task` FreeRTOS task.

Must be called after:
- `tof.tof_setup()` has succeeded
- `tof_mutex` has been created by `init_mutexes()`

---

### `tof_task(void *arg)` *(static)*

The FreeRTOS task function. Runs on a fixed period defined by `TOF_PERIOD_MS` (20 ms).

Each cycle:
1. Calls `readRangeMMFiltered()` on all three sensors sequentially.
2. If all three reads return `ESP_OK`, calls `tof_write()` to update shared state.
3. If any read fails, logs a warning with individual error codes and skips the write — the previous valid snapshot is preserved.
4. Sleeps until the next period using `vTaskDelayUntil()`.

**Note:** The three sensor reads are sequential, not parallel. At 20 ms period with filtered reads this is acceptable. If latency becomes an issue this can be revisited.

---

### `tof_write(uint16_t front, uint8_t left, uint8_t right)` *(static)*

Internal writer. Builds a complete `tof_state_t` locally — raw values, all boolean flags, validity, and timestamp — then takes `s_mutex` only to copy the finished struct into `g_state`. Lock time is one struct assignment.

---

### `tof_read(tof_state_t *out)`

Public reader. Takes the mutex, copies `g_state` into `*out`, then releases. Returns `false` if the mutex is not initialized or `out` is NULL.

The caller gets a full atomic snapshot — no partial reads possible.

---

## Timing

| Parameter | Value |
|---|---|
| Period | 20 ms |
| Stack size | 3072 bytes |
| Priority | 5 |

> Stack is larger than `task_imu` because the ToF object methods have deeper call chains than the IMU read.

---

## Data Flow

```
VL53L0X (front)
VL6180X (left)   ──► tof_task ──[builds tof_state_t]──[tof_mutex]──► g_state
VL6180X (right)                                                          │
                                                                         ▼
                                                                    tof_read()
                                                                         │
                                                                         ▼
                                                                   control_task
```

---

## Related

- [task_imu](task_imu.md)
- `ToF.h` — ToF manager class
- `VL53L0X.h` — Front sensor driver
- `VL6180X.h` — Side sensor driver
- `main.cpp` — calls `tof_task_start()`
