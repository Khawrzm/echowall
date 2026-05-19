/**
 * ECHOWALL ESP32-S3 Main
 * Captures Wi-Fi CSI and streams JSON over serial to ECHOWALL host.
 * Initialises the FL mesh for zero-telemetry federated learning.
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "config.h"
#include "csi_extractor.h"
#include "serial_reporter.h"
#include "fl_mesh.h"

static const char* TAG = "ECHOWALL";

/* Resident INT8 TCN model weights — zero-initialised; loaded by model loader
 * or seeded by physics-informed defaults before fl_mesh_init() is called.   */
static int8_t g_model_weights[FL_WEIGHT_COUNT];

void app_main(void)
{
    ESP_LOGI(TAG, "ECHOWALL v0.1.0 starting...");

    // Init NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    serial_reporter_init(ECHOWALL_BAUD_RATE);
    csi_extractor_init(ECHOWALL_WIFI_SSID, ECHOWALL_WIFI_PASS);
    csi_extractor_register_callback(serial_reporter_on_csi);

    // Init FL mesh — must be called after Wi-Fi is up (csi_extractor_init
    // brings up the STA interface which ESP-NOW requires).
    ret = fl_mesh_init(g_model_weights, FL_WEIGHT_COUNT);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "fl_mesh_init failed: %s", esp_err_to_name(ret));
        // Non-fatal: continue without federated learning
    } else {
        ESP_LOGI(TAG, "FL mesh active. Round interval: %us, max peers: %u",
                 FL_ROUND_INTERVAL_S, FL_MAX_PEERS);
    }

    ESP_LOGI(TAG, "Streaming CSI at %d baud. Ready.", ECHOWALL_BAUD_RATE);

    // Main loop: handled by callbacks
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
