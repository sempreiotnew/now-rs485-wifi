#include "device_now_info.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "now_protocol.h"
#include "nvs_flash.h"
#include "webserver.h"
#include "wifi_handler.h"
#include <stdio.h>

static const char *TAG = "MAIN";

void espnow_rx_cb(const esp_now_recv_info_t *info, const uint8_t *data,
                  int len) {
  char mac_str[18];
  snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X",
           info->src_addr[0], info->src_addr[1], info->src_addr[2],
           info->src_addr[3], info->src_addr[4], info->src_addr[5]);

  ESP_LOGI(TAG, "Received from: %s", mac_str);
  char str[len + 1];
  memcpy(str, data, len);
  str[len] = '\0';
  ESP_LOGI(TAG, "Data (str): %s", str);

  uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS; // current time in ms

  for (int i = 0; i < device_count; i++) {
    if (memcmp(devices[i].mac, info->src_addr, 6) == 0) {
      // Update existing device
      devices[i].rssi = info->rx_ctrl->rssi;
      strncpy(devices[i].last_msg, str, sizeof(devices[i].last_msg) - 1);
      devices[i].last_msg[sizeof(devices[i].last_msg) - 1] = '\0';
      devices[i].last_seen_ms = now;
      return;
    }
  }

  // New device
  if (device_count < MAX_DEVICES) {
    memcpy(devices[device_count].mac, info->src_addr, 6);
    devices[device_count].rssi = info->rx_ctrl->rssi;
    strncpy(devices[device_count].last_msg, str,
            sizeof(devices[device_count].last_msg) - 1);
    devices[device_count].last_msg[sizeof(devices[device_count].last_msg) - 1] =
        '\0';
    devices[device_count].last_seen_ms = now;
    device_count++;
  }
}

void app_main(void) {
  ESP_ERROR_CHECK(nvs_flash_init());
  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());

  init_wifi_ap();
  init_web_server();
  init_esp_now(espnow_rx_cb);

  xTaskCreate(discovery_task, "discovery_task", 4096, NULL, 5, NULL);
  xTaskCreate(remove_stale_devices_task, "cleanup_task", 4096, NULL, 5, NULL);
}
