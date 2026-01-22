#include "webserver.h"
#include "../wifi_handler/include/wifi_handler.h"
#include "cJSON.h"
#include "device_now_info.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "webserver";

extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[] asm("_binary_index_html_end");
extern const uint8_t index_js_start[] asm("_binary_index_js_start");
extern const uint8_t index_js_end[] asm("_binary_index_js_end");
bool gotcha = false;
/* ---------- ROOT HTML---------- */
static esp_err_t root_get_handler(httpd_req_t *req) {
  size_t len = index_html_end - index_html_start;
  return httpd_resp_send(req, (const char *)index_html_start, len);
}

/* ---------- ROOT JS---------- */
static esp_err_t js_get_handler(httpd_req_t *req) {
  httpd_resp_set_type(req, "application/javascript");
  size_t len = index_js_end - index_js_start;
  return httpd_resp_send(req, (const char *)index_js_start, len);
}

void on_wifi_result(bool connected, esp_err_t reason) {
  if (connected) {
    ESP_LOGI("APP", "WiFi connected!");
  } else {
    ESP_LOGE("APP", "WiFi failed reason=%d", reason);
  }
}

/* ---------- SCAN API ---------- */
static esp_err_t scan_get_handler(httpd_req_t *req) {
  char ssid[33]; // max SSID length = 32 + '\0'

  char connected_ssid[33] = {0};
  bool is_connected =
      get_connected_ssid(connected_ssid, sizeof(connected_ssid));

  if (get_connected_ssid(ssid, sizeof(ssid))) {
    ESP_LOGW(TAG, "Connected to SSID: %s", ssid);
  } else {
    ESP_LOGW(TAG, "Not connected to any Wi-Fi");
  }
  wifi_ap_record_t *aps = NULL;
  uint16_t count = wifi_scan(&aps);

  if (count == 0 || !aps) {
    httpd_resp_sendstr(req, "[]");
    return ESP_OK;
  }

  cJSON *arr = cJSON_CreateArray();
  if (!arr) {
    free(aps);
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "JSON alloc");
    return ESP_FAIL;
  }

  for (int i = 0; i < count; i++) {
    cJSON *obj = cJSON_CreateObject();
    if (!obj)
      break;

    cJSON_AddStringToObject(obj, "ssid", (char *)aps[i].ssid);
    cJSON_AddNumberToObject(obj, "rssi", aps[i].rssi);
    cJSON_AddNumberToObject(obj, "channel", aps[i].primary);
    bool connected = false;
    const char *scan_ssid = (char *)aps[i].ssid;

    if (is_connected && strlen(scan_ssid) > 0) {
      connected = (strcmp(connected_ssid, scan_ssid) == 0);
    }
    cJSON_AddBoolToObject(obj, "connected", connected);
    cJSON_AddItemToArray(arr, obj);
  }

  char *json = cJSON_PrintUnformatted(arr);

  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
  // ESP_LOGI(TAG, "%s", json);
  cJSON_free(json);
  cJSON_Delete(arr);
  free(aps);

  return ESP_OK;
}

static esp_err_t get_nearby_devices_handler(httpd_req_t *req) {
  cJSON *root = cJSON_CreateArray();
  if (!root)
    return ESP_FAIL;

  for (int i = 0; i < device_count; i++) {
    cJSON *obj = cJSON_CreateObject();
    char mac_str[18];
    snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X",
             devices[i].mac[0], devices[i].mac[1], devices[i].mac[2],
             devices[i].mac[3], devices[i].mac[4], devices[i].mac[5]);
    cJSON_AddStringToObject(obj, "mac", mac_str);
    cJSON_AddNumberToObject(obj, "rssi", devices[i].rssi);
    cJSON_AddStringToObject(obj, "last_msg", devices[i].last_msg);
    cJSON_AddItemToArray(root, obj);
  }

  char *json_str = cJSON_PrintUnformatted(root);
  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, json_str, HTTPD_RESP_USE_STRLEN);
  // ESP_LOGI(TAG, "%s", json_str);
  cJSON_free(json_str);
  cJSON_Delete(root);
  return ESP_OK;
}

static esp_err_t get_wired_status_handler(httpd_req_t *req) {
  cJSON *root = cJSON_CreateObject();
  if (!root) {
    return ESP_FAIL;
  }

  cJSON_AddBoolToObject(root, "connected", true);

  char *json_str = cJSON_PrintUnformatted(root);

  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, json_str, HTTPD_RESP_USE_STRLEN);

  // ESP_LOGI(TAG, "%s", json_str);

  cJSON_free(json_str);
  cJSON_Delete(root);

  return ESP_OK;
}

static esp_err_t post_connect_handler(httpd_req_t *req) {
  char content[256] = {0};
  int received = httpd_req_recv(req, content, req->content_len);
  if (received <= 0) {
    return ESP_FAIL;
  }

  // Parse JSON
  cJSON *root = cJSON_Parse(content);
  if (!root) {
    return ESP_FAIL;
  }

  const cJSON *ssid = cJSON_GetObjectItem(root, "ssid");
  const cJSON *password = cJSON_GetObjectItem(root, "password");

  if (!cJSON_IsString(ssid) || !cJSON_IsString(password)) {
    cJSON_Delete(root);
    return ESP_FAIL;
  }

  ESP_LOGI(TAG, "SSID: %s", ssid->valuestring);
  ESP_LOGI(TAG, "AM I CONNECTED ? %s", is_wifi_connected() ? "true" : "false");
  wifi_ap_record_t ap;

  if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) {
    ESP_LOGI(TAG, "SSID: %s", ap.ssid);
    ESP_LOGI(TAG, "RSSI: %d", ap.rssi);
    ESP_LOGI(TAG, "Channel: %d", ap.primary);
  }

  if (!is_wifi_connected()) {
    connect_wifi(ssid->valuestring, password->valuestring, on_wifi_result);
  }

  // Build response
  cJSON *resp = cJSON_CreateObject();

  if (true) {
    cJSON_AddBoolToObject(resp, "success", true);
  } else {
    cJSON_AddBoolToObject(resp, "success", false);
    cJSON_AddStringToObject(resp, "reason", "connection_failed");
  }

  char *json_str = cJSON_PrintUnformatted(resp);

  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, json_str, HTTPD_RESP_USE_STRLEN);

  cJSON_free(json_str);
  cJSON_Delete(resp);
  cJSON_Delete(root);

  return ESP_OK;
}

static esp_err_t post_disconnect_handler(httpd_req_t *req) {
  cJSON *root = cJSON_CreateObject();
  if (!root) {
    return ESP_FAIL;
  }

  disconnect_wifi();
  cJSON_AddBoolToObject(root, "disconnected", true);

  char *json_str = cJSON_PrintUnformatted(root);

  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, json_str, HTTPD_RESP_USE_STRLEN);

  // ESP_LOGI(TAG, "%s", json_str);

  cJSON_free(json_str);
  cJSON_Delete(root);

  return ESP_OK;
}

static esp_err_t post_pair_handler(httpd_req_t *req) {
  cJSON *root = cJSON_CreateObject();
  if (!root) {
    return ESP_FAIL;
  }

  cJSON_AddBoolToObject(root, "connected", true);

  char *json_str = cJSON_PrintUnformatted(root);

  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, json_str, HTTPD_RESP_USE_STRLEN);

  // ESP_LOGI(TAG, "%s", json_str);

  cJSON_free(json_str);
  cJSON_Delete(root);

  return ESP_OK;
}

static esp_err_t post_unpair_handler(httpd_req_t *req) {
  cJSON *root = cJSON_CreateObject();
  if (!root) {
    return ESP_FAIL;
  }

  cJSON_AddBoolToObject(root, "connected", true);

  char *json_str = cJSON_PrintUnformatted(root);

  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, json_str, HTTPD_RESP_USE_STRLEN);

  // ESP_LOGI(TAG, "%s", json_str);

  cJSON_free(json_str);
  cJSON_Delete(root);

  return ESP_OK;
}

httpd_handle_t init_web_server(void) {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.max_uri_handlers = 16;
  httpd_handle_t server = NULL;

  httpd_uri_t root = {
      .uri = "/",
      .method = HTTP_GET,
      .handler = root_get_handler,
  };

  httpd_uri_t js = {
      .uri = "/index.js",
      .method = HTTP_GET,
      .handler = js_get_handler,
  };

  httpd_uri_t scan = {
      .uri = "/api/scan",
      .method = HTTP_GET,
      .handler = scan_get_handler,
  };

  httpd_uri_t nearby = {
      .uri = "/api/nearby",
      .method = HTTP_GET,
      .handler = get_nearby_devices_handler,
  };

  httpd_uri_t wired_status = {
      .uri = "/api/wired/status",
      .method = HTTP_GET,
      .handler = get_wired_status_handler,
  };

  httpd_uri_t connect = {
      .uri = "/api/connect",
      .method = HTTP_POST,
      .handler = post_connect_handler,
  };

  httpd_uri_t disconnect = {
      .uri = "/api/disconnect",
      .method = HTTP_POST,
      .handler = post_disconnect_handler,
  };

  httpd_uri_t pair = {
      .uri = "/api/pair",
      .method = HTTP_POST,
      .handler = post_pair_handler,
  };

  httpd_uri_t unpair = {
      .uri = "/api/unpair",
      .method = HTTP_POST,
      .handler = post_unpair_handler,
  };

  if (httpd_start(&server, &config) == ESP_OK) {
    httpd_register_uri_handler(server, &root);
    httpd_register_uri_handler(server, &scan);
    httpd_register_uri_handler(server, &js);
    httpd_register_uri_handler(server, &nearby);
    httpd_register_uri_handler(server, &wired_status);
    httpd_register_uri_handler(server, &connect);
    httpd_register_uri_handler(server, &disconnect);
    httpd_register_uri_handler(server, &pair);
    httpd_register_uri_handler(server, &unpair);

    ESP_LOGI(TAG, "Webserver started");
  }

  return server;
}
