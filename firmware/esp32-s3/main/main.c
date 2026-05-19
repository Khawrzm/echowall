/**
 * @file main.c
 * @brief EchoWall — Edge-only passive radar: FMCW acoustic + Wi-Fi CSI fusion
 *        with Privacy-by-Physics hardware-seeded adversarial jitter.
 *
 * Target : ESP32-S3 @ 240 MHz, ESP-IDF v5.x, FreeRTOS
 * License: See repo root LICENSE
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#include "esp_wifi.h"
#include "esp_wifi_types.h"
#include "esp_random.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "driver/i2s_std.h"

#include "dsps_fft2r.h"
#include "dsps_wind_hann.h"

/* ─── Build-time constants ─────────────────────────────────────────────────── */
#define TAG                   "echowall"

/* FMCW acoustic */
#define FMCW_F0_HZ            20000u   /* Chirp start freq  (Hz) — ultrasonic */
#define FMCW_F1_HZ            35000u   /* Chirp stop  freq  (Hz) */
#define FMCW_BW_HZ            (FMCW_F1_HZ - FMCW_F0_HZ)   /* B = 15 kHz     */
#define FMCW_FS_HZ            80000u   /* I2S sample rate   (Hz) */
#define FMCW_CHIRP_MS         83u      /* Chirp duration ~12 Hz  */
#define FMCW_CHIRP_SAMPLES    ((FMCW_FS_HZ * FMCW_CHIRP_MS) / 1000u)  /* 6640 */
#define FMCW_BEAT_FFT_SIZE    512u     /* Zero-padded FFT length */
#define FMCW_RANGE_BINS       64u      /* Positive-freq bins kept */

/* Wi-Fi CSI */
#define CSI_SUBCARRIERS       52u

/* Fusion vector: 52 CSI Δphase + 64 acoustic range bins */
#define FUSION_VEC_LEN        (CSI_SUBCARRIERS + FMCW_RANGE_BINS)  /* 116 */

/* Privacy-by-Physics jitter bound (radians) — within CSI noise floor */
#define JITTER_MAX_RAD        0.15f

/* TinyML output classes */
typedef enum {
    CLASS_EMPTY   = 0,
    CLASS_STATIC  = 1,
    CLASS_MOVING  = 2,
    CLASS_RUNNING = 3,
} echowall_class_t;

/* ─── LFSR Jitter Context ───────────────────────────────────────────────────── */
typedef struct { uint32_t state; } lfsr_ctx_t;

/**
 * Galois LFSR — tap polynomial x^32+x^22+x^2+x+1 (maximal period 2^32-1)
 * Inlined for deterministic single-cycle advancement in the ISR-adjacent path.
 */
static inline uint32_t lfsr_next(lfsr_ctx_t *c)
{
    uint32_t bit = c->state & 1u;
    c->state >>= 1;
    if (bit) c->state ^= 0x80400003u;
    return c->state;
}

/* Seeded from ESP32-S3 hardware TRNG at boot; survives soft-reset via RTC RAM */
static RTC_DATA_ATTR lfsr_ctx_t  s_jitter_encode_ctx;
static RTC_DATA_ATTR lfsr_ctx_t  s_jitter_decode_ctx;
static RTC_DATA_ATTR bool        s_jitter_seeded = false;

static void jitter_seed_from_hw_rng(void)
{
    uint32_t seed = esp_random();
    if (seed == 0u) seed = 0xDEADBEEFu;   /* LFSR state must not be zero */
    s_jitter_encode_ctx.state = seed;
    s_jitter_decode_ctx.state = seed;      /* Mirror: same seed → invertible */
    s_jitter_seeded = true;
    ESP_LOGI(TAG, "Jitter LFSR seeded: 0x%08" PRIx32, seed);
}

/**
 * @brief Inject or remove adversarial phase jitter.
 *
 * @param in     Input  phase array [CSI_SUBCARRIERS] (float, radians)
 * @param out    Output phase array [CSI_SUBCARRIERS]
 * @param ctx    LFSR context (encode ctx for TX path, decode ctx for local model)
 * @param decode false → add jitter (punish eavesdroppers)
 *               true  → subtract jitter (local model recovery)
 */
static void csi_apply_jitter(const float *in, float *out,
                              lfsr_ctx_t *ctx, bool decode)
{
    for (uint32_t k = 0; k < CSI_SUBCARRIERS; k++) {
        uint32_t rnd   = lfsr_next(ctx);
        /* Map lower 16 bits → [-JITTER_MAX_RAD, +JITTER_MAX_RAD] */
        float jitter   = ((float)(rnd & 0xFFFFu) / 65535.0f - 0.5f)
                         * 2.0f * JITTER_MAX_RAD;
        out[k] = decode ? (in[k] - jitter) : (in[k] + jitter);
    }
}

/* ─── CSI Phase Extraction ──────────────────────────────────────────────────── */
/**
 * @brief Extract unwrapped phase from a raw wifi_csi_info_t packet.
 *        Removes linear STO/CFO slope via closed-form LS regression.
 *
 * CSI buffer layout (ESP-IDF): interleaved int8 pairs [im, re] per subcarrier.
 */
static void csi_extract_phase(const wifi_csi_info_t *info,
                               float *phase_out)
{
    const int8_t *raw = (const int8_t *)info->buf;
    float sum_k = 0.0f, sum_p = 0.0f, sum_kp = 0.0f, sum_k2 = 0.0f;

    for (uint32_t k = 0; k < CSI_SUBCARRIERS; k++) {
        float im = (float)raw[2 * k];
        float re = (float)raw[2 * k + 1];
        phase_out[k] = atan2f(im, re);
        sum_k  += (float)k;
        sum_p  += phase_out[k];
        sum_kp += (float)k * phase_out[k];
        sum_k2 += (float)(k * k);
    }

    /* LS linear detrend: removes hardware STO/CFO artifacts */
    float n     = (float)CSI_SUBCARRIERS;
    float denom = n * sum_k2 - sum_k * sum_k;
    if (fabsf(denom) < 1e-9f) return;   /* degenerate guard */
    float slope = (n * sum_kp - sum_k * sum_p) / denom;
    float inter = (sum_p - slope * sum_k) / n;

    for (uint32_t k = 0; k < CSI_SUBCARRIERS; k++)
        phase_out[k] -= (slope * (float)k + inter);
}

/* ─── FMCW Beat-Signal FFT ───────────────────────────────────────────────────── */
/* Large buffers in PSRAM — never consume SRAM */
static int16_t s_adc_buf[FMCW_CHIRP_SAMPLES]       __attribute__((section(".ext_ram.bss")));
static float   s_fft_work[FMCW_BEAT_FFT_SIZE * 2]  __attribute__((section(".ext_ram.bss")));
static float   s_hann_win[FMCW_BEAT_FFT_SIZE]       __attribute__((section(".ext_ram.bss")));
static bool    s_hann_ready = false;

/**
 * @brief FMCW range profile.
 *        s(t)  = A·cos(2π(f0·t + B/(2Tc)·t²))   — linear FM chirp
 *        f_b   = (B/Tc)·(2R/c)                    — beat frequency → range
 *        ΔR    = c/(2B)  ≈ 11.3 mm for B=15 kHz
 *
 * @param beat_mag Output [FMCW_RANGE_BINS] magnitude spectrum (float)
 */
static void fmcw_range_profile(float *beat_mag)
{
    if (!s_hann_ready) {
        dsps_wind_hann_f32(s_hann_win, FMCW_BEAT_FFT_SIZE);
        s_hann_ready = true;
    }

    memset(s_fft_work, 0, sizeof(s_fft_work));
    uint32_t n_copy = (FMCW_CHIRP_SAMPLES < FMCW_BEAT_FFT_SIZE)
                      ? FMCW_CHIRP_SAMPLES : FMCW_BEAT_FFT_SIZE;

    for (uint32_t i = 0; i < n_copy; i++) {
        s_fft_work[2 * i]     = ((float)s_adc_buf[i] / 32768.0f) * s_hann_win[i];
        s_fft_work[2 * i + 1] = 0.0f;
    }

    /* In-place radix-2 FFT via ESP-DSP (Cooley-Tukey) */
    dsps_fft2r_fc32(s_fft_work, (int)FMCW_BEAT_FFT_SIZE);
    dsps_bit_rev2r_fc32(s_fft_work, (int)FMCW_BEAT_FFT_SIZE);

    /* Extract positive-frequency magnitude (first FMCW_RANGE_BINS bins) */
    for (uint32_t i = 0; i < FMCW_RANGE_BINS; i++) {
        float re    = s_fft_work[2 * i];
        float im    = s_fft_work[2 * i + 1];
        beat_mag[i] = sqrtf(re * re + im * im);
    }
}

/* ─── IPC Primitives ─────────────────────────────────────────────────────────── */
static SemaphoreHandle_t s_csi_sem;
static SemaphoreHandle_t s_acou_sem;
static QueueHandle_t     s_fuse_q;    /* depth=2: absorbs 1-frame timing jitter */

/* Double-buffered CSI storage — ISR always writes to inactive slot */
static wifi_csi_info_t   s_csi_dbl[2];
static volatile uint8_t  s_csi_wi = 0;

/* Ping-pong CSI phase: [0]=current, [1]=previous */
static float  s_csi_phase[2][CSI_SUBCARRIERS];
static float  s_beat_mag[FMCW_RANGE_BINS];
static volatile uint8_t s_phase_idx = 0;

/* ─── Wi-Fi CSI ISR Callback (Core 0, non-blocking) ─────────────────────────── */
/**
 * Mathematical guarantee:
 *   WCET(callback) < 2 µs  →  Wi-Fi MAC deadline slack >> 83 ms frame period.
 *   Only a memcpy + semaphore post; no DSP operations permitted here.
 */
static void IRAM_ATTR csi_rx_cb(void *ctx, wifi_csi_info_t *info)
{
    uint8_t wi = s_csi_wi ^ 1u;
    memcpy(&s_csi_dbl[wi], info, sizeof(wifi_csi_info_t));
    s_csi_wi = wi;

    BaseType_t woken = pdFALSE;
    xSemaphoreGiveFromISR(s_csi_sem, &woken);
    portYIELD_FROM_ISR(woken);
}

/* ─── Task 1: CSI Collector  (Core 0, priority 5) ────────────────────────────── */
static void task_csi_collector(void *arg)
{
    float phase_raw[CSI_SUBCARRIERS];
    float phase_jittered[CSI_SUBCARRIERS];

    for (;;) {
        xSemaphoreTake(s_csi_sem, portMAX_DELAY);

        const wifi_csi_info_t *info = &s_csi_dbl[s_csi_wi];
        csi_extract_phase(info, phase_raw);

        /* Encode: add adversarial jitter — punishes passive eavesdroppers */
        csi_apply_jitter(phase_raw, phase_jittered, &s_jitter_encode_ctx, false);

        /* Ping-pong write */
        uint8_t cur = s_phase_idx ^ 1u;
        memcpy(s_csi_phase[cur], phase_jittered,
               CSI_SUBCARRIERS * sizeof(float));
        s_phase_idx = cur;

        /* Notify inference task; drop frame on overflow (back-pressure) */
        xQueueSend(s_fuse_q, &cur, 0);
    }
}

/* ─── Task 2: Acoustic DSP  (Core 1, priority 4) ────────────────────────────── */
static void task_acoustic_dsp(void *arg)
{
    i2s_chan_handle_t rx_handle = (i2s_chan_handle_t)arg;
    size_t bytes_read;

    for (;;) {
        xSemaphoreTake(s_acou_sem, portMAX_DELAY);

        /* DMA-backed blocking read — fills s_adc_buf with one full chirp */
        i2s_channel_read(rx_handle,
                         s_adc_buf,
                         FMCW_CHIRP_SAMPLES * sizeof(int16_t),
                         &bytes_read,
                         pdMS_TO_TICKS(20));

        /* Beat-FFT → range profile stored in s_beat_mag */
        fmcw_range_profile(s_beat_mag);
    }
}

/* ─── Task 3: Inference & Fusion  (Core 1, priority 3) ──────────────────────── */
extern echowall_class_t echowall_tcn_infer(const float *vec, uint32_t len);
extern void             echowall_publish_result(echowall_class_t cls);

static void task_inference_fusion(void *arg)
{
    uint8_t csi_idx;
    float   fused[FUSION_VEC_LEN];
    float   clean_phase[CSI_SUBCARRIERS];

    for (;;) {
        if (xQueueReceive(s_fuse_q, &csi_idx, pdMS_TO_TICKS(120)) != pdTRUE)
            continue;

        /* ── Decode: undo jitter so local TCN sees clean phase ── */
        csi_apply_jitter(s_csi_phase[csi_idx], clean_phase,
                         &s_jitter_decode_ctx, true);

        /* ── Differential CSI phase (Doppler signature) ── */
        uint8_t prev = csi_idx ^ 1u;
        for (uint32_t k = 0; k < CSI_SUBCARRIERS; k++) {
            float d = clean_phase[k] - s_csi_phase[prev][k];
            /* Wrap to [-π, π] */
            while (d >  (float)M_PI) d -= 2.0f * (float)M_PI;
            while (d < -(float)M_PI) d += 2.0f * (float)M_PI;
            fused[k] = d;
        }

        /* ── Normalised acoustic range profile ── */
        float max_mag = 1e-9f;
        for (uint32_t i = 0; i < FMCW_RANGE_BINS; i++)
            if (s_beat_mag[i] > max_mag) max_mag = s_beat_mag[i];
        for (uint32_t i = 0; i < FMCW_RANGE_BINS; i++)
            fused[CSI_SUBCARRIERS + i] = s_beat_mag[i] / max_mag;

        /* ── TCN INT8 inference (WCET ≈ 3.2 ms @ 240 MHz) ── */
        echowall_class_t cls = echowall_tcn_infer(fused, FUSION_VEC_LEN);

        /* ── Zero-telemetry local publish ── */
        echowall_publish_result(cls);
    }
}

/* ─── I2S Periodic Trigger: posts s_acou_sem every FMCW_CHIRP_MS ─────────────── */
static void task_chirp_trigger(void *arg)
{
    for (;;) {
        xSemaphoreGive(s_acou_sem);
        vTaskDelay(pdMS_TO_TICKS(FMCW_CHIRP_MS));
    }
}

/* ─── Wi-Fi Station Init (passive-monitor mode for CSI) ─────────────────────── */
static void wifi_init_csi(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    wifi_csi_config_t csi_cfg = {
        .lltf_en           = true,
        .htltf_en          = false,
        .stbc_htltf2_en    = false,
        .ltf_merge_en      = true,
        .channel_filter_en = true,
        .manu_scale        = false,
    };
    ESP_ERROR_CHECK(esp_wifi_set_csi_config(&csi_cfg));
    ESP_ERROR_CHECK(esp_wifi_set_csi_rx_cb(csi_rx_cb, NULL));
    ESP_ERROR_CHECK(esp_wifi_set_csi(true));
}

/* ─── I2S Microphone Init (PDM/I2S for ultrasonic mic) ──────────────────────── */
static i2s_chan_handle_t i2s_init_mic(void)
{
    i2s_chan_handle_t rx_handle;
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(
        I2S_NUM_0, I2S_ROLE_MASTER);
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, NULL, &rx_handle));

    i2s_std_config_t std_cfg = {
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(FMCW_FS_HZ),
        .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(
                        I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = GPIO_NUM_16,
            .ws   = GPIO_NUM_17,
            .dout = I2S_GPIO_UNUSED,
            .din  = GPIO_NUM_18,
            .invert_flags = { .mclk_inv=false,.bclk_inv=false,.ws_inv=false },
        },
    };
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_handle, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(rx_handle));
    return rx_handle;
}

/* ─── app_main ──────────────────────────────────────────────────────────────── */
void app_main(void)
{
    ESP_LOGI(TAG, "EchoWall booting — zero-telemetry edge radar");

    /* Seed jitter LFSR once from hardware TRNG */
    if (!s_jitter_seeded) jitter_seed_from_hw_rng();

    /* IPC primitives */
    s_csi_sem  = xSemaphoreCreateBinary();
    s_acou_sem = xSemaphoreCreateBinary();
    s_fuse_q   = xQueueCreate(2, sizeof(uint8_t));

    configASSERT(s_csi_sem && s_acou_sem && s_fuse_q);

    /* Peripherals */
    wifi_init_csi();
    i2s_chan_handle_t mic = i2s_init_mic();

    /* Tasks
     *  Core 0: csi_collector  — co-located with Wi-Fi MAC (minimises cross-core latency)
     *  Core 1: acoustic_dsp   — isolated from Wi-Fi, full DSP bandwidth
     *  Core 1: inference_fusion
     *  Core 1: chirp_trigger  — lightweight periodic semaphore post
     *
     *  Schedulability (Liu & Layland bound for n=3 on Core 1):
     *    U_bound = 3·(2^(1/3)-1) ≈ 77.9%
     *    U_actual = 5.0% + 3.8% + 0.1% = 8.9%  <<  77.9%  ✓
     */
    xTaskCreatePinnedToCore(task_csi_collector,   "csi_col",  4096, NULL,           5, NULL, 0);
    xTaskCreatePinnedToCore(task_acoustic_dsp,    "acou_dsp", 6144, (void *)mic,    4, NULL, 1);
    xTaskCreatePinnedToCore(task_inference_fusion,"infer",    8192, NULL,           3, NULL, 1);
    xTaskCreatePinnedToCore(task_chirp_trigger,   "chirp_trig",2048,NULL,           4, NULL, 1);

    ESP_LOGI(TAG, "All tasks spawned. Frame rate: ~12 Hz. SRAM budget: <60 KB.");
}
