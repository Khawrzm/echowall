/**
 * ECHOWALL ESP32-S3 Main — v0.2.0
 * Captures Wi-Fi CSI and streams JSON over serial to ECHOWALL host.
 * Initialises the FL mesh for zero-telemetry federated learning.
 *
 * Hardware: ESP32-S3 (~$5). No cloud. No router reconfiguration required.
 * Privacy:  Raw CSI never leaves this device. Zero upload_to_cloud() calls.
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

/* Resident INT8 TCN model weights (~46 KB).
 * Zero-initialised at boot; adapted by `echowall calibrate`.
 * Updated in-place by fl_mesh FedAvg after each completed round. */
static int8_t g_model_weights[FL_WEIGHT_COUNT];

void app_main(void)
{
    ESP_LOGI(TAG, "ECHOWALL v0.2.0 starting (ESP32-S3 bare-metal)");

    // Init NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    serial_reporter_init(ECHOWALL_BAUD_RATE);

    // Bring up Wi-Fi STA — required before ESP-NOW init inside fl_mesh_init()
    csi_extractor_init(ECHOWALL_WIFI_SSID, ECHOWALL_WIFI_PASS);
    csi_extractor_register_callback(serial_reporter_on_csi);

    // Init FL mesh (ESP-NOW peer-to-peer, FedAvg aggregator, TX/round tasks)
    // Non-fatal: node continues sensing without FL if init fails.
    ret = fl_mesh_init(g_model_weights, FL_WEIGHT_COUNT);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "fl_mesh_init failed: %s — running standalone (no FL)",
                 esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "FL mesh active | round=%us | peers_max=%u | chunks=%u",
                 FL_ROUND_INTERVAL_S, FL_MAX_PEERS, FL_CHUNK_TOTAL);
    }

    ESP_LOGI(TAG, "CSI streaming at %d baud. Ready.", ECHOWALL_BAUD_RATE);

    // Main loop: all work is interrupt/callback/task driven
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
