#pragma once
#include "esp_wifi_types.h"

void serial_reporter_init(int baud_rate);
void serial_reporter_on_csi(const wifi_csi_info_t* info);
