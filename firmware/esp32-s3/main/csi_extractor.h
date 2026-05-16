#pragma once
#include "esp_wifi_types.h"

typedef void (*csi_callback_t)(const wifi_csi_info_t* info);

void csi_extractor_init(const char* ssid, const char* password);
void csi_extractor_register_callback(csi_callback_t cb);
