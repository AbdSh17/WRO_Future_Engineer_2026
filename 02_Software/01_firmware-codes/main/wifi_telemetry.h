#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize WiFi in station mode and connect.
 *        Blocks until connected or fails after timeout.
 */
void wifi_telemetry_init(void);

/**
 * @brief Send a formatted telemetry string over UDP.
 *        Safe to call from any task after wifi_telemetry_init().
 */
void wifi_telemetry_send(const char *data);

#ifdef __cplusplus
}
#endif
