#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"

static const char *TAG = "wifi_handler";

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data) {

  ESP_LOGI(TAG, "EVENT ID: %d", event_id);
  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
    esp_wifi_connect();
  }

  else if (event_base == WIFI_EVENT &&
           event_id == WIFI_EVENT_STA_DISCONNECTED) {
    ESP_LOGI(TAG, "WIFI DISCONNECTED");
  }

  else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
    ESP_LOGI(TAG, "WIFI CONNECTED, GOT IP ");
  }

  if (event_id == WIFI_EVENT_STA_CONNECTED) {
    ESP_LOGI(TAG, "<<<<<<<< WIFI CONNECTED >>>>>> ");
  }
}

void init_wifi_ap() {

  esp_netif_create_default_wifi_sta();
  esp_netif_create_default_wifi_ap();

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                             &wifi_event_handler, NULL));

  wifi_config_t wifi_config = {
      .ap =
          {
              .ssid = "ESP32_AP1",
              .ssid_len = 0,
              .channel = 1,
              .password = "12345678",
              .max_connection = 4,
              .authmode = WIFI_AUTH_WPA_WPA2_PSK,
          },
  };

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
  ESP_ERROR_CHECK(esp_wifi_start());

  ESP_LOGI("wifi", "AP+STA started");
}

uint16_t wifi_scan(wifi_ap_record_t **out) {
  uint16_t ap_count = 0;

  wifi_scan_config_t scan_cfg = {.show_hidden = true};

  ESP_ERROR_CHECK(esp_wifi_scan_start(&scan_cfg, true)); // blocking
  ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));

  if (ap_count == 0) {
    *out = NULL;
    return 0;
  }

  wifi_ap_record_t *ap_list = malloc(sizeof(wifi_ap_record_t) * ap_count);
  ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&ap_count, ap_list));

  *out = ap_list;
  return ap_count;
}

void connect_wifi(const char *ssid, const char *password) {
  wifi_config_t wifi_config = {0};

  strncpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
  strncpy((char *)wifi_config.sta.password, password,
          sizeof(wifi_config.sta.password));

  ESP_ERROR_CHECK(esp_wifi_disconnect());
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
  ESP_ERROR_CHECK(esp_wifi_connect());
}
