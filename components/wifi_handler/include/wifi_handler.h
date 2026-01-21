#include "esp_wifi.h"

void init_wifi_ap();
uint16_t wifi_scan(wifi_ap_record_t **out);
void connect_wifi(const char *ssid, const char *password);