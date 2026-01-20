#include "device_now_info.h"
#include "esp_log.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include <string.h>

static const char *TAG = "now_protocol";
static uint8_t broadcast_mac[ESP_NOW_ETH_ALEN] = {0xFF, 0xFF, 0xFF,
                                                  0xFF, 0xFF, 0xFF};
static void espnow_rx_cb(const esp_now_recv_info_t *info, const uint8_t *data,
                         int len);
void init_esp_now(esp_now_recv_cb_t c) {
  // Init ESP-NOW
  if (esp_now_init() != ESP_OK) {
    ESP_LOGE(TAG, "ESP-NOW Init Failed");
    return;
  }
  ESP_LOGI(TAG, "ESP-NOW initialized");

  // Set primary master key (optional, for encrypted messages)
  uint8_t pmk[16] = {0}; // all zeros is OK for unencrypted
  esp_now_set_pmk(pmk);

  // Register callback for receiving messages
  esp_now_register_recv_cb(espnow_rx_cb);
}

void discovery_task(void *arg) {
  const char *msg = "WHOIS";

  // Add broadcast peer
  esp_now_peer_info_t broadcast_peer = {0};
  memcpy(broadcast_peer.peer_addr, broadcast_mac, ESP_NOW_ETH_ALEN);
  broadcast_peer.channel = 0;
  broadcast_peer.ifidx = WIFI_IF_STA;
  broadcast_peer.encrypt = false;
  esp_now_add_peer(&broadcast_peer);

  while (1) {
    esp_err_t result = esp_now_send(broadcast_mac, (uint8_t *)msg, strlen(msg));
    if (result != ESP_OK) {
      ESP_LOGE(TAG, "[ESP-NOW] - Data LOST!");
    }
    // cleanup_peers(); // remove stale peers regularly
    vTaskDelay(pdMS_TO_TICKS(3000));
  }
}

void remove_stale_devices_task(void *arg) {
  const uint32_t timeout_ms = 10000; // 10 seconds without messages = remove
  while (1) {
    uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
    for (int i = 0; i < device_count;) {
      if (now - devices[i].last_seen_ms > timeout_ms) {
        // Remove device by shifting array
        for (int j = i; j < device_count - 1; j++) {
          devices[j] = devices[j + 1];
        }
        device_count--;
      } else {
        i++;
      }
    }
    vTaskDelay(pdMS_TO_TICKS(1000)); // check every 1 second
  }
}
