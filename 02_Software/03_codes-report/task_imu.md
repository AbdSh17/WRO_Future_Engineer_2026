# task_imu

## Overview

`task_imu` is responsible for periodically reading yaw from the BNO055 IMU and publishing it to a shared state protected by a mutex. It is the sole writer of the yaw state. Any other task that needs yaw calls `yaw_read()` and gets a consistent snapshot.

---

## Shared State

The following variables are module-private and only accessible through the read/write API:

| Variable | Type | Description |
|---|---|---|
| `g_yaw` | `float` | Latest yaw angle in degrees, relative, wrapped \[-180, 180\] |
| `g_yaw_valid` | `bool` | True once the first successful read has occurred |
| `g_yaw_us` | `int64_t` | Timestamp of the last successful write, in microseconds |

> These variables are `volatile` to prevent the compiler from caching them across task context switches.

---

## Functions

### `imu_task_start(bno055_imu_t *imu, SemaphoreHandle_t mutex)`

Entry point called from `app_main`. Stores the IMU handle and mutex pointer into module-level statics, then spawns the `imu_task` FreeRTOS task.

Must be called after:
- `bno_setup()` has succeeded
- `yaw_mutex` has been created by `init_mutexes()`

---

### `imu_task(void *arg)` *(static)*

The FreeRTOS task function. Runs on a fixed period defined by `IMU_PERIOD_MS` (10 ms).

Each cycle:
1. Calls `bno_read_yaw_rel_deg()` to get the latest filtered relative yaw from the BNO055.
2. If the result is not `NAN`, calls `yaw_write()` to update shared state.
3. If the result is `NAN`, logs a warning and skips the write — the previous valid value is preserved.
4. Sleeps until the next period using `vTaskDelayUntil()`, which keeps the period accurate regardless of how long the read takes.

**Warning:** If `bno_read_yaw_rel_deg()` consistently returns `NAN`, `g_yaw_valid` will never be set and consumers will see `age_us = INT64_MAX`.

---

### `yaw_write(float yaw_deg)` *(static)*

Internal writer. Takes `s_mutex`, updates `g_yaw`, `g_yaw_valid`, and `g_yaw_us`, then releases the mutex. Never called from outside the file.

---

### `yaw_read(float *yaw_deg, bool *ok, int64_t *age_us)`

Public reader. Copies the shared state under the mutex into local variables, then assigns to the output pointers after releasing the lock — minimizing time spent holding it.

`age_us` is computed as `now - g_yaw_us`. If `g_yaw_valid` is false, `age_us` is set to `INT64_MAX` as a sentinel so the caller can detect a stale or uninitialized state.

Returns early (no-op) if the mutex is not initialized or any output pointer is NULL.

---

### `yaw_zero(void)`

Delegates to `bno_zero_yaw()` on the stored IMU handle. Resets the relative yaw reference to the current heading and clears the filter buffer. Safe to call from any task.

---

## Timing

| Parameter | Value |
|---|---|
| Period | 10 ms |
| Stack size | 2048 bytes |
| Priority | 5 |

---

## Data Flow

```
BNO055 hardware
      │
      ▼
bno_read_yaw_rel_deg()
      │
      ▼
  imu_task  ──[yaw_mutex]──► g_yaw / g_yaw_valid / g_yaw_us
                                        │
                                        ▼
                                   yaw_read()
                                        │
                                        ▼
                                  control_task
```

---

## Related

- [task_tof](task_tof.md)
- `imu_bno055.h` — BNO055 driver API
- `main.cpp` — calls `imu_task_start()`
