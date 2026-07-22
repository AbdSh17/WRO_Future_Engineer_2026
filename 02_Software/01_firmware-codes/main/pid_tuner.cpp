#include "pid_tuner.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/sockets.h"

#define TAG "PID_TUNER"
#define TUNER_PORT 5006
#define BUF_SIZE 128

static forward_ctrl_t *s_fwd = nullptr;
static turn_ctrl_t *s_turn = nullptr;
static volatile bool s_running = false;

bool pid_tuner_running(void) { return s_running; }

// ---------------------------------------------------------------------------
// Response helper — sends a reply string back to whoever sent the command
// ---------------------------------------------------------------------------
static void reply(int sock, struct sockaddr_in *src, const char *msg) {
  sendto(sock, msg, strlen(msg), 0, (struct sockaddr *)src, sizeof(*src));
}

// ---------------------------------------------------------------------------
// Build a status string with all current PID values
// ---------------------------------------------------------------------------
static void send_status(int sock, struct sockaddr_in *src) {
  char buf[256];
  snprintf(buf, sizeof(buf),
           "FWD  Kp=%.4f Ki=%.4f Kd=%.4f\n"
           "TURN Kp=%.4f Ki=%.4f Kd=%.4f\n"
           "robot=%s\n",
           s_fwd->pid.Kp, s_fwd->pid.Ki, s_fwd->pid.Kd, s_turn->pid.Kp,
           s_turn->pid.Ki, s_turn->pid.Kd, s_running ? "RUNNING" : "STOPPED");
  reply(sock, src, buf);
}

// ---------------------------------------------------------------------------
// Parse and apply one command
//
// Commands (case-sensitive):
//   start          — set s_running = true
//   stop           — set s_running = false
//   status         — reply with current PID values
//   fKp=<val>      — forward Kp
//   fKi=<val>      — forward Ki
//   fKd=<val>      — forward Kd
//   tKp=<val>      — turn Kp
//   tKi=<val>      — turn Ki
//   tKd=<val>      — turn Kd
// ---------------------------------------------------------------------------
static void handle_cmd(int sock, struct sockaddr_in *src, char *cmd, int len) {
  // strip trailing newline/CR
  while (len > 0 && (cmd[len - 1] == '\n' || cmd[len - 1] == '\r'))
    cmd[--len] = '\0';

  ESP_LOGI(TAG, "CMD: %s", cmd);

  if (strcmp(cmd, "start") == 0) {
    s_running = true;
    reply(sock, src, "OK start\n");

  } else if (strcmp(cmd, "stop") == 0) {
    s_running = false;
    reply(sock, src, "OK stop\n");

  } else if (strcmp(cmd, "status") == 0) {
    send_status(sock, src);

  } else if (strncmp(cmd, "fKp=", 4) == 0) {
    s_fwd->pid.Kp = strtof(cmd + 4, nullptr);
    reply(sock, src, "OK fKp\n");

  } else if (strncmp(cmd, "fKi=", 4) == 0) {
    s_fwd->pid.Ki = strtof(cmd + 4, nullptr);
    reply(sock, src, "OK fKi\n");

  } else if (strncmp(cmd, "fKd=", 4) == 0) {
    s_fwd->pid.Kd = strtof(cmd + 4, nullptr);
    reply(sock, src, "OK fKd\n");

  } else if (strncmp(cmd, "tKp=", 4) == 0) {
    s_turn->pid.Kp = strtof(cmd + 4, nullptr);
    reply(sock, src, "OK tKp\n");

  } else if (strncmp(cmd, "tKi=", 4) == 0) {
    s_turn->pid.Ki = strtof(cmd + 4, nullptr);
    reply(sock, src, "OK tKi\n");

  } else if (strncmp(cmd, "tKd=", 4) == 0) {
    s_turn->pid.Kd = strtof(cmd + 4, nullptr);
    reply(sock, src, "OK tKd\n");

  } else {
    reply(sock, src, "ERR unknown command\n");
  }
}

// ---------------------------------------------------------------------------
// Tuner task
// ---------------------------------------------------------------------------
static void tuner_task(void *arg) {
  (void)arg;

  int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (sock < 0) {
    ESP_LOGE(TAG, "Socket create failed");
    vTaskDelete(nullptr);
    return;
  }

  struct sockaddr_in bind_addr = {};
  bind_addr.sin_family = AF_INET;
  bind_addr.sin_addr.s_addr = INADDR_ANY;
  bind_addr.sin_port = htons(TUNER_PORT);

  if (bind(sock, (struct sockaddr *)&bind_addr, sizeof(bind_addr)) < 0) {
    ESP_LOGE(TAG, "Bind failed");
    close(sock);
    vTaskDelete(nullptr);
    return;
  }

  // 200ms receive timeout so the task yields regularly
  struct timeval tv = {.tv_sec = 0, .tv_usec = 200000};
  setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

  ESP_LOGI(TAG, "Listening on UDP port %d", TUNER_PORT);

  char buf[BUF_SIZE];
  struct sockaddr_in src_addr;
  socklen_t src_len = sizeof(src_addr);

  while (true) {
    int n = recvfrom(sock, buf, sizeof(buf) - 1, 0,
                     (struct sockaddr *)&src_addr, &src_len);
    if (n > 0) {
      buf[n] = '\0';
      handle_cmd(sock, &src_addr, buf, n);
    }
    // timeout is normal — just loop
  }
}

// ---------------------------------------------------------------------------
// Public
// ---------------------------------------------------------------------------
void pid_tuner_start(forward_ctrl_t *fwd, turn_ctrl_t *turn) {
  s_fwd = fwd;
  s_turn = turn;
  xTaskCreate(tuner_task, "pid_tuner", 4096, nullptr, 5, nullptr);
}
