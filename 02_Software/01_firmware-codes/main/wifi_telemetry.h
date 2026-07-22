#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize WiFi station mode + UDP telemetry socket.
 *        Non-blocking: returns immediately, WiFi connects in background.
 *        Safe to call once from app_main.
 */
void wifi_telemetry_init(void);

/**
 * @brief Send a null-terminated string over UDP to the configured host.
 *        Thread-safe. Silently drops the message if not connected yet
 *        or if the socket send fails.
 */
void wifi_telemetry_send(const char *data);

/**
 * @brief True once the WiFi + UDP socket are ready.
 *        Useful to gate expensive formatting when telemetry isn't up.
 */
bool wifi_telemetry_is_connected(void);

#ifdef __cplusplus
}
#endif
