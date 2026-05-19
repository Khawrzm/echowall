/**
 * fl_mesh.c — Zero-Telemetry Federated Learning via ESP-NOW
 *
 * Implements:
 *   1. Galois LFSR-16 stream cipher for delta privacy masking.
 *   2. Chunked ESP-NOW broadcast of masked INT8 weight deltas.
 *   3. FedAvg aggregation with INT16 accumulation (no float on hot path).
 *   4. Mutex-protected weight swap after each completed round.
 *
 * Raw CSI, acoustic data, and inference outputs are NEVER transmitted.
 * Only masked weight deltas (Δw = w_new - w_base) leave the node.
 */

#include "fl_mesh.h"

#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include "nvs.h"
#include "nvs_flash.h"

static const char *TAG = "FL_MESH";

/* ── Module state ────────────────────────────────────────────────────────── */
typedef struct {
    int8_t          *base_weights;    /* snapshot taken at round start       */
    int8_t          *global_weights;  /* latest FedAvg output                */
    int16_t         *accum_buf;       /* INT16 accumulation (1 per weight)   */
    uint8_t          peer_count;      /* number of contributing peers        */
    uint16_t         round;           /* current FL round counter            */
    SemaphoreHandle_t mutex;          /* protects global_weights             */
    QueueHandle_t    tx_queue;        /* outbound frame queue                */
    /* Peer receive state: tracks which chunks arrived per peer per round   */
    struct {
        uint8_t  mac[6];
        uint16_t round;
        uint8_t  chunks_rx[FL_CHUNK_TOTAL / 8 + 1]; /* bitfield             */
        uint8_t *delta_buf;  /* masked delta accumulation for this peer     */
        bool     complete;
    } peers[FL_MAX_PEERS];
} fl_state_t;

static fl_state_t s = {0};

/* ── Galois LFSR-16 ──────────────────────────────────────────────────────── */
static uint16_t s_lfsr = 0xACE1u;  /* non-zero seed */

static inline uint8_t lfsr_next_byte(void)
{
    uint8_t out = 0;
    for (int i = 0; i < 8; i++) {
        uint16_t lsb = s_lfsr & 1u;
        s_lfsr >>= 1;
        if (lsb) s_lfsr ^= FL_LFSR_POLY;
        out |= (uint8_t)(lsb << i);
    }
    return out;
}

/** XOR-mask a buffer in-place with the LFSR stream. Call with same seed to
 *  unmask. Seed is reset to a per-round value derived from MAC + round_id
 *  so each round produces a unique mask stream. */
static void lfsr_mask(uint8_t *buf, uint32_t len, uint16_t seed)
{
    s_lfsr = seed ? seed : 0xACE1u;
    for (uint32_t i = 0; i < len; i++) {
        buf[i] ^= lfsr_next_byte();
    }
}

/* ── CRC-16/CCITT ────────────────────────────────────────────────────────── */
static uint16_t crc16_ccitt(const uint8_t *data, uint16_t len)
{
    uint16_t crc = 0xFFFFu;
    while (len--) {
        crc ^= (uint16_t)(*data++ << 8);
        for (int i = 0; i < 8; i++) {
            crc = (crc & 0x8000u) ? ((crc << 1) ^ 0x1021u) : (crc << 1);
        }
    }
    return crc;
}

/* ── ESP-NOW send callback ───────────────────────────────────────────────── */
static void espnow_send_cb(const uint8_t *mac, esp_now_send_status_t status)
{
    if (status != ESP_NOW_SEND_SUCCESS) {
        ESP_LOGW(TAG, "ESP-NOW send fail to " MACSTR, MAC2STR(mac));
    }
}

/* ── ESP-NOW receive callback ────────────────────────────────────────────── */
static void espnow_recv_cb(const esp_now_recv_info_t *info,
                           const uint8_t *data, int len)
{
    if (len != sizeof(fl_frame_t)) return;
    const fl_frame_t *f = (const fl_frame_t *)data;

    /* Validate magic */
    if (memcmp(f->magic, "FLMX", 4) != 0) return;

    /* Validate CRC (compute over header bytes before crc16 field + payload) */
    uint16_t computed_crc = crc16_ccitt((const uint8_t *)f,
                                         offsetof(fl_frame_t, crc16));
    computed_crc = crc16_ccitt(f->payload, FL_ESPNOW_PAYLOAD);
    if (computed_crc != f->crc16) {
        ESP_LOGW(TAG, "CRC mismatch chunk %u", f->chunk_idx);
        return;
    }

    if (f->chunk_idx >= FL_CHUNK_TOTAL) return;

    /* Find or allocate peer slot */
    int slot = -1;
    for (int i = 0; i < FL_MAX_PEERS; i++) {
        if (memcmp(s.peers[i].mac, f->node_id, 6) == 0) { slot = i; break; }
    }
    if (slot == -1) {
        for (int i = 0; i < FL_MAX_PEERS; i++) {
            if (s.peers[i].delta_buf == NULL) { slot = i; break; }
        }
        if (slot == -1) {
            ESP_LOGW(TAG, "Peer table full — dropping frame");
            return;
        }
        memcpy(s.peers[slot].mac, f->node_id, 6);
        s.peers[slot].delta_buf = (uint8_t *)heap_caps_malloc(
            FL_WEIGHT_COUNT,
            MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!s.peers[slot].delta_buf) {
            /* PSRAM not available — fall back to internal heap (2 peers max) */
            s.peers[slot].delta_buf = (uint8_t *)malloc(FL_WEIGHT_COUNT);
        }
        if (!s.peers[slot].delta_buf) {
            ESP_LOGE(TAG, "OOM: cannot allocate peer delta buffer");
            return;
        }
        memset(s.peers[slot].delta_buf, 0, FL_WEIGHT_COUNT);
        memset(s.peers[slot].chunks_rx, 0, sizeof(s.peers[slot].chunks_rx));
        s.peers[slot].round    = f->round;
        s.peers[slot].complete = false;
    }

    /* Stale round — reset peer state */
    if (s.peers[slot].round != f->round) {
        memset(s.peers[slot].delta_buf, 0, FL_WEIGHT_COUNT);
        memset(s.peers[slot].chunks_rx, 0, sizeof(s.peers[slot].chunks_rx));
        s.peers[slot].round    = f->round;
        s.peers[slot].complete = false;
    }

    /* Copy chunk payload into peer delta buffer */
    uint32_t offset = (uint32_t)f->chunk_idx * FL_ESPNOW_PAYLOAD;
    uint32_t copy_len = FL_ESPNOW_PAYLOAD;
    if (offset + copy_len > FL_WEIGHT_COUNT)
        copy_len = FL_WEIGHT_COUNT - offset;
    memcpy(s.peers[slot].delta_buf + offset, f->payload, copy_len);

    /* Mark chunk received */
    s.peers[slot].chunks_rx[f->chunk_idx / 8] |= (1u << (f->chunk_idx % 8));

    /* Check if all chunks received */
    uint32_t bits_set = 0;
    for (uint32_t i = 0; i < FL_CHUNK_TOTAL; i++) {
        if (s.peers[slot].chunks_rx[i / 8] & (1u << (i % 8))) bits_set++;
    }
    if (bits_set == FL_CHUNK_TOTAL) {
        s.peers[slot].complete = true;
        ESP_LOGI(TAG, "Peer " MACSTR " round %u complete",
                 MAC2STR(s.peers[slot].mac), f->round);
    }
}

/* ── FedAvg Aggregation ──────────────────────────────────────────────────── */
/**
 * Aggregate all complete peer deltas into global_weights.
 *
 * Math: w_global[i] = clamp8( w_base[i] + (1/N) * Σ_j unmask(delta_j[i]) )
 *
 * INT16 accumulation prevents overflow across up to FL_MAX_PEERS=4 peers
 * (INT8 range: -128..127; max sum = 4×127 = 508 fits in INT16).
 * Final division and clamp back to INT8 done once at end.
 */
static void fedavg_aggregate(void)
{
    uint8_t n = 0;
    uint16_t lfsr_seed;

    /* Determine contributing peers */
    for (int p = 0; p < FL_MAX_PEERS; p++) {
        if (s.peers[p].complete) n++;
    }
    if (n == 0) {
        ESP_LOGW(TAG, "FedAvg: no complete peers this round");
        return;
    }

    /* Zero accumulator */
    memset(s.accum_buf, 0, FL_WEIGHT_COUNT * sizeof(int16_t));

    /* Accumulate: unmask each peer's delta and add to INT16 buffer */
    for (int p = 0; p < FL_MAX_PEERS; p++) {
        if (!s.peers[p].complete) continue;

        /* Unmask: derive seed from peer MAC + round (deterministic) */
        uint8_t *mac = s.peers[p].mac;
        lfsr_seed = (uint16_t)((mac[4] ^ (mac[5] << 8)) ^ s.peers[p].round);
        if (lfsr_seed == 0) lfsr_seed = 0xACE1u;

        /* Unmask in-place (XOR is its own inverse) */
        lfsr_mask(s.peers[p].delta_buf, FL_WEIGHT_COUNT, lfsr_seed);

        /* Accumulate INT8 delta into INT16 buffer */
        for (uint32_t i = 0; i < FL_WEIGHT_COUNT; i++) {
            s.accum_buf[i] += (int16_t)((int8_t)s.peers[p].delta_buf[i]);
        }

        /* Free peer buffer and reset slot */
        free(s.peers[p].delta_buf);
        memset(&s.peers[p], 0, sizeof(s.peers[p]));
    }

    /* Divide by N and apply to base weights → new global weights */
    xSemaphoreTake(s.mutex, portMAX_DELAY);
    for (uint32_t i = 0; i < FL_WEIGHT_COUNT; i++) {
        int32_t updated = (int32_t)s.base_weights[i] +
                          (int32_t)(s.accum_buf[i] / (int16_t)n);
        /* Clamp to INT8 */
        if      (updated >  127) updated =  127;
        else if (updated < -128) updated = -128;
        s.global_weights[i] = (int8_t)updated;
    }
    /* Advance base to latest global for next round */
    memcpy(s.base_weights, s.global_weights, FL_WEIGHT_COUNT);
    xSemaphoreGive(s.mutex);

    ESP_LOGI(TAG, "FedAvg round %u complete: %u peers aggregated",
             s.round, n);
    s.round++;
}

/* ── TX task: drains outbound frame queue via ESP-NOW ────────────────────── */
static void fl_tx_task(void *arg)
{
    fl_frame_t frame;
    while (1) {
        if (xQueueReceive(s.tx_queue, &frame, pdMS_TO_TICKS(100)) == pdTRUE) {
            /* Broadcast to all registered ESP-NOW peers */
            uint8_t broadcast[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
            esp_err_t err = esp_now_send(broadcast,
                                         (const uint8_t *)&frame,
                                         sizeof(fl_frame_t));
            if (err != ESP_OK) {
                ESP_LOGW(TAG, "esp_now_send err: %s", esp_err_to_name(err));
            }
            /* 10 ms inter-frame gap: 214 chunks × 10 ms = ~2.1 s per round */
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
}

/* ── FL round timer task ─────────────────────────────────────────────────── */
static void fl_round_task(void *arg)
{
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(FL_ROUND_INTERVAL_S * 1000));
        ESP_LOGI(TAG, "Starting FL aggregation round %u", s.round);
        fedavg_aggregate();
    }
}

/* ── Public API ──────────────────────────────────────────────────────────── */

esp_err_t fl_mesh_init(int8_t *local_weights, uint32_t weight_count)
{
    assert(weight_count == FL_WEIGHT_COUNT);

    s.base_weights   = local_weights;
    s.global_weights = (int8_t *)malloc(FL_WEIGHT_COUNT);
    s.accum_buf      = (int16_t *)heap_caps_malloc(
                           FL_WEIGHT_COUNT * sizeof(int16_t),
                           MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!s.accum_buf) {
        /* Fallback: internal SRAM — warn, works for ≤2 peers */
        ESP_LOGW(TAG, "PSRAM unavailable: accum_buf in internal SRAM. Max 2 peers.");
        s.accum_buf = (int16_t *)malloc(FL_WEIGHT_COUNT * sizeof(int16_t));
    }
    if (!s.global_weights || !s.accum_buf) return ESP_ERR_NO_MEM;

    memcpy(s.global_weights, local_weights, FL_WEIGHT_COUNT);
    s.mutex    = xSemaphoreCreateMutex();
    s.tx_queue = xQueueCreate(FL_CHUNK_TOTAL + 16, sizeof(fl_frame_t));
    if (!s.mutex || !s.tx_queue) return ESP_ERR_NO_MEM;

    /* ESP-NOW init (Wi-Fi must already be inited by caller) */
    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_send_cb(espnow_send_cb));
    ESP_ERROR_CHECK(esp_now_register_recv_cb(espnow_recv_cb));

    /* Read own MAC for frame headers */
    ESP_ERROR_CHECK(esp_read_mac(s.peers[0].mac, ESP_MAC_WIFI_STA));

    xTaskCreate(fl_tx_task,    "fl_tx",    4096, NULL, 5, NULL);
    xTaskCreate(fl_round_task, "fl_round", 4096, NULL, 4, NULL);

    ESP_LOGI(TAG, "FL mesh init OK. %u weights, %u chunks/round, max %u peers",
             FL_WEIGHT_COUNT, FL_CHUNK_TOTAL, FL_MAX_PEERS);
    return ESP_OK;
}

void fl_mesh_notify_trained(const int8_t *new_weights)
{
    /* Compute delta: Δw = new - base (INT8, wraps intentionally) */
    uint8_t delta[FL_WEIGHT_COUNT];
    for (uint32_t i = 0; i < FL_WEIGHT_COUNT; i++) {
        delta[i] = (uint8_t)((int8_t)(new_weights[i] - s.base_weights[i]));
    }

    /* Mask with LFSR: seed = own MAC[4:5] XOR round */
    uint8_t own_mac[6];
    esp_read_mac(own_mac, ESP_MAC_WIFI_STA);
    uint16_t seed = (uint16_t)((own_mac[4] ^ (own_mac[5] << 8)) ^ s.round);
    if (seed == 0) seed = 0xACE1u;
    lfsr_mask(delta, FL_WEIGHT_COUNT, seed);

    /* Enqueue chunked frames */
    fl_frame_t frame;
    memcpy(frame.magic, "FLMX", 4);
    memcpy(frame.node_id, own_mac, 6);
    frame.round       = s.round;
    frame.chunk_total = FL_CHUNK_TOTAL;

    for (uint16_t c = 0; c < FL_CHUNK_TOTAL; c++) {
        frame.chunk_idx = c;
        uint32_t offset   = (uint32_t)c * FL_ESPNOW_PAYLOAD;
        uint32_t copy_len = FL_ESPNOW_PAYLOAD;
        if (offset + copy_len > FL_WEIGHT_COUNT)
            copy_len = FL_WEIGHT_COUNT - offset;
        memset(frame.payload, 0, FL_ESPNOW_PAYLOAD);
        memcpy(frame.payload, delta + offset, copy_len);

        /* CRC over header (before crc16 field) + payload */
        frame.crc16 = crc16_ccitt((const uint8_t *)&frame,
                                   offsetof(fl_frame_t, crc16));
        frame.crc16 ^= crc16_ccitt(frame.payload, FL_ESPNOW_PAYLOAD);

        if (xQueueSend(s.tx_queue, &frame, pdMS_TO_TICKS(50)) != pdTRUE) {
            ESP_LOGW(TAG, "TX queue full at chunk %u", c);
        }
    }
}

void fl_mesh_get_global_weights(int8_t *out)
{
    xSemaphoreTake(s.mutex, portMAX_DELAY);
    memcpy(out, s.global_weights, FL_WEIGHT_COUNT);
    xSemaphoreGive(s.mutex);
}
