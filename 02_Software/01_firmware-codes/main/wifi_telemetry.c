#include "wifi_telemetry.h"

#include <string.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "lwip/sockets.h"
#include "nvs_flash.h"

// ---------------------------------------------------------------------------
// Credentials — change these
// ---------------------------------------------------------------------------
static const char *WIFI_SSID = "LAPTOP";
static const char *WIFI_PASS = "12345678";
static const char *UDP_HOST =
    "172.19.48.197"; // IP of the PC running the Python script
static const int UDP_PORT = 5005;
// ---------------------------------------------------------------------------

#define TAG "WIFI_TEL"
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_TIMEOUT_MS 10000

static EventGroupHandle_t s_wifi_event_group = NULL;
static int s_sock = -1;
static struct sockaddr_in s_dest_addr;

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data) {
  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
    ESP_LOGW(TAG, "Disconnected, reconnecting...");
    esp_wifi_connect();
  } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
    xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
  }
}

void wifi_telemetry_init(void) {
  // NVS required by WiFi driver
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
      ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    nvs_flash_erase();
    nvs_flash_init();
  }

  s_wifi_event_group = xEventGroupCreate();

  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  esp_netif_create_default_wifi_sta();

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                             &wifi_event_handler, NULL));
  ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                             &wifi_event_handler, NULL));

  wifi_config_t wifi_config = {};
  strncpy((char *)wifi_config.sta.ssid, WIFI_SSID,
          sizeof(wifi_config.sta.ssid) - 1);
  strncpy((char *)wifi_config.sta.password, WIFI_PASS,
          sizeof(wifi_config.sta.password) - 1);

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
  ESP_ERROR_CHECK(esp_wifi_start());
  esp_wifi_connect();

  EventBits_t bits =
      xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT, pdFALSE,
                          pdTRUE, pdMS_TO_TICKS(WIFI_TIMEOUT_MS));
  if (!(bits & WIFI_CONNECTED_BIT)) {
    ESP_LOGE(TAG, "WiFi connection timed out");
    return;
  }

  // Create UDP socket
  s_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (s_sock < 0) {
    ESP_LOGE(TAG, "Socket creation failed");
    return;
  }

  memset(&s_dest_addr, 0, sizeof(s_dest_addr));
  s_dest_addr.sin_family = AF_INET;
  s_dest_addr.sin_port = htons(UDP_PORT);
  inet_pton(AF_INET, UDP_HOST, &s_dest_addr.sin_addr);

  ESP_LOGI(TAG, "UDP ready → %s:%d", UDP_HOST, UDP_PORT);
}

void wifi_telemetry_send(const char *data) {
  if (s_sock < 0 || !data)
    return;
  sendto(s_sock, data, strlen(data), 0, (struct sockaddr *)&s_dest_addr,
         sizeof(s_dest_addr));
}
