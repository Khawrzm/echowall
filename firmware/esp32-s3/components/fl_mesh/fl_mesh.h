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

/* ── Model geometry — must match INT8 TCN in model.c ──────────────────────── */
#define FL_INPUT_DIM        640u
#define FL_HIDDEN_DIM       64u
#define FL_OUTPUT_DIM       8u
#define FL_WEIGHT_COUNT     (FL_INPUT_DIM * FL_HIDDEN_DIM + \
                             FL_HIDDEN_DIM * FL_HIDDEN_DIM + \
                             FL_HIDDEN_DIM * FL_OUTPUT_DIM)  /* 45,568 */

/* ── ESP-NOW framing ────────────────────────────────────────────────────── */
#define FL_ESPNOW_PAYLOAD   220u
#define FL_CHUNK_TOTAL      ((FL_WEIGHT_COUNT + FL_ESPNOW_PAYLOAD - 1) / \
                              FL_ESPNOW_PAYLOAD)

/* ── Federation parameters ───────────────────────────────────────────────── */
#define FL_MAX_PEERS        4u
#define FL_ROUND_INTERVAL_S 30u

/* ── LFSR privacy mask ───────────────────────────────────────────────────── */
#define FL_LFSR_POLY        0xB400u

typedef struct __attribute__((packed)) {
    uint8_t  magic[4];
    uint8_t  node_id[6];
    uint16_t round;
    uint16_t chunk_idx;
    uint16_t chunk_total;
    uint16_t crc16;
    uint8_t  payload[FL_ESPNOW_PAYLOAD];
} fl_frame_t;

esp_err_t fl_mesh_init(int8_t *local_weights, uint32_t weight_count);
void      fl_mesh_notify_trained(const int8_t *new_weights);
void      fl_mesh_get_global_weights(int8_t *out);

#ifdef __cplusplus
}
#endif
