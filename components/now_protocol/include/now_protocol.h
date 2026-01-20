#pragma once
#include <esp_now.h>

void init_esp_now(esp_now_recv_cb_t c);
void discovery_task(void *arg);
void remove_stale_devices_task(void *arg);