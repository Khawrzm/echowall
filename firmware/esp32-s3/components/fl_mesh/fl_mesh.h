/**
 * fl_mesh.h — Zero-Telemetry Federated Learning via ESP-NOW
 *
 * Architecture: INT8 TCN weight delta exchange across ESP32-S3 peers.
 * Transport:    ESP-NOW (MAC layer, no router, no IP stack).
 * Aggregation:  FedAvg with INT16 accumulation to prevent overflow.
 * Privacy:      Galois LFSR XOR masking on all transmitted deltas.
 *
 * SRAM budget (512 KB total, ~128 KB reserved for OS/Wi-Fi/stack):
 *   Model weights (INT8):       ~46 KB
 *   INT16 accumulation buffer:  ~93 KB  (uses PSRAM if available)
 *   ESP-NOW chunk staging:       1 KB
 *   FL state struct:            <512 B
 *   Headroom:                  >240 KB
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "esp_now.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── Model geometry — must match INT8 TCN in model.c ────────────────────── */
#define FL_INPUT_DIM        640u   /* 64 subcarriers × 10 time frames        */
#define FL_HIDDEN_DIM       64u
#define FL_OUTPUT_DIM       8u
#define FL_WEIGHT_COUNT     (FL_INPUT_DIM * FL_HIDDEN_DIM + \
                             FL_HIDDEN_DIM * FL_HIDDEN_DIM + \
                             FL_HIDDEN_DIM * FL_OUTPUT_DIM)  /* 45,568 */

/* ── ESP-NOW framing ─────────────────────────────────────────────────────── */
#define FL_ESPNOW_PAYLOAD   220u   /* bytes of weight data per frame         */
#define FL_CHUNK_TOTAL      ((FL_WEIGHT_COUNT + FL_ESPNOW_PAYLOAD - 1) / \
                              FL_ESPNOW_PAYLOAD)   /* 208 chunks             */

/* ── Federation parameters ───────────────────────────────────────────────── */
#define FL_MAX_PEERS        4u     /* max simultaneous peer nodes            */
#define FL_ROUND_INTERVAL_S 30u    /* seconds between aggregation rounds     */

/* ── LFSR privacy mask ───────────────────────────────────────────────────── */
#define FL_LFSR_POLY        0xB400u  /* Galois 16-bit LFSR, taps: 16,14,13,11 */

/* ── Wire frame header (30 bytes, fits before 220 B payload = 250 B max) ── */
typedef struct __attribute__((packed)) {
    uint8_t  magic[4];    /* "FLMX"                                          */
    uint8_t  node_id[6];  /* sender MAC                                      */
    uint16_t round;       /* FL round counter (wraps at 65535)               */
    uint16_t chunk_idx;   /* 0 … FL_CHUNK_TOTAL-1                            */
    uint16_t chunk_total; /* FL_CHUNK_TOTAL (receiver sanity check)          */
    uint16_t crc16;       /* CRC-16/CCITT of header + payload                */
    uint8_t  payload[FL_ESPNOW_PAYLOAD];
} fl_frame_t;  /* total: 30 + 220 = 250 bytes == ESP-NOW max                 */

/* ── Public API ──────────────────────────────────────────────────────────── */

/**
 * fl_mesh_init() — call once from app_main after nvs_flash_init().
 *
 * Initialises ESP-NOW, registers peer list from NVS, allocates the
 * INT16 accumulation buffer (in PSRAM if CONFIG_ESP32S3_SPIRAM_SUPPORT=y,
 * otherwise internal SRAM limited to 2 peers), and starts the FL task.
 *
 * @param local_weights  Pointer to the resident INT8 model weight array.
 *                       fl_mesh owns this pointer for the lifetime of the app.
 * @param weight_count   Must equal FL_WEIGHT_COUNT (assert-checked).
 * @return ESP_OK on success.
 */
esp_err_t fl_mesh_init(int8_t *local_weights, uint32_t weight_count);

/**
 * fl_mesh_notify_trained() — call after each local training step.
 *
 * Computes the delta (new_weights - base_weights) in INT8, applies
 * Galois LFSR XOR masking, then enqueues the chunked ESP-NOW broadcast.
 * Non-blocking: uses an internal FreeRTOS queue.
 *
 * @param new_weights  Updated INT8 weights after local SGD step.
 */
void fl_mesh_notify_trained(const int8_t *new_weights);

/**
 * fl_mesh_get_global_weights() — retrieve the latest FedAvg result.
 *
 * Safe to call from the CSI/inference task. Internally mutex-protected.
 *
 * @param out  Output buffer of size FL_WEIGHT_COUNT bytes.
 */
void fl_mesh_get_global_weights(int8_t *out);

#ifdef __cplusplus
}
#endif
