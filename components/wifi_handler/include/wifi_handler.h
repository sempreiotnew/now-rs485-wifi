#pragma once
#include "esp_wifi.h"
#include "structs.h"

void init_wifi_ap();
uint16_t wifi_scan(wifi_ap_record_t **out);
void connect_wifi(const char *ssid, const char *password, wifi_connect_cb_t cb);
void disconnect_wifi();
bool is_wifi_connected();
bool get_connected_ssid(char *ssid, size_t max_len);