/**
 * casper_api.c — C-linkage FFI export interface for the Casper Engine.
 *
 * Exposes exactly the three functions declared in the Rust
 * cognitive_lobe/src/casper_bridge.rs extern "C" block:
 *
 *   casper_init(model_buf, model_len)     -> int
 *   casper_infer(features, feat_len,
 *                out_class, out_conf)     -> int
 *   casper_sarf_analyze(text, text_len,
 *                       out_roots,
 *                       out_count)        -> int
 *
 * All functions return 0 on success, negative errno on failure.
 * No global mutable state shared between calls (re-entrant).
 * C11, zero external dependencies beyond <stdint.h> and <stddef.h>.
 */
#include "tensor.h"
#include "sarf.h"

#include <stdint.h>
#include <stddef.h>

/* -------------------------------------------------------------------------
 * Model state
 *
 * The Casper Engine operates a 3-layer INT8 MLP:
 *   Layer 1: [640 x 64]  (encoder)
 *   Layer 2: [64  x 64]  (hidden)
 *   Layer 3: [64  x  8]  (output logits, 4 used for presence classes)
 *
 * Weights are stored in the caller-supplied model buffer (lifetime:
 * caller must keep buffer alive as long as casper_infer is called).
 *
 * Layout in model_buf (bytes):
 *   [0      .. 40960-1]  encoder weights  (640*64 int8)
 *   [40960  .. 45056-1]  hidden  weights  (64*64  int8)
 *   [45056  .. 45568-1]  output  weights  (64*8   int8)
 *   Total: 45568 bytes minimum.
 * -------------------------------------------------------------------------*/
#define ENC_ROWS   640u
#define ENC_COLS    64u
#define HID_ROWS    64u
#define HID_COLS    64u
#define OUT_ROWS    64u
#define OUT_COLS     8u
#define N_CLASSES    4u   /* presence classes: empty/standing/sitting/fall */

#define ENC_SIZE  (ENC_ROWS * ENC_COLS)   /* 40960 */
#define HID_SIZE  (HID_ROWS * HID_COLS)   /*  4096 */
#define OUT_SIZE  (OUT_ROWS * OUT_COLS)   /*   512 */
#define MODEL_MIN_BYTES (ENC_SIZE + HID_SIZE + OUT_SIZE) /* 45568 */

typedef struct {
    const int8_t *enc_w;   /* pointer into caller model_buf */
    const int8_t *hid_w;
    const int8_t *out_w;
    int           ready;
} casper_model_t;

/*
 * Single module-level model descriptor.
 * NOT thread-safe by design (sovereign single-owner architecture).
 */
static casper_model_t g_model = { .ready = 0 };

/* -------------------------------------------------------------------------
 * casper_init
 * -------------------------------------------------------------------------*/
int casper_init(const int8_t *model_buf, size_t model_len)
{
    if (!model_buf || model_len < MODEL_MIN_BYTES) return -1;

    g_model.enc_w = model_buf;
    g_model.hid_w = model_buf + ENC_SIZE;
    g_model.out_w = model_buf + ENC_SIZE + HID_SIZE;
    g_model.ready = 1;
    return 0;
}

/* -------------------------------------------------------------------------
 * casper_infer
 *
 * Forward pass: features -> enc -> relu -> hid -> relu -> out -> argmax
 * -------------------------------------------------------------------------*/
int casper_infer(
    const int8_t *features,
    size_t        feat_len,
    uint8_t      *out_class,
    uint8_t      *out_confidence)
{
    if (!g_model.ready)               return -2;
    if (!features || !out_class
        || !out_confidence)           return -1;
    if (feat_len != ENC_ROWS)         return -3;

    /* Stack-allocate intermediate activations. */
    int8_t h1[ENC_COLS];  /* 64 bytes */
    int8_t h2[HID_COLS];  /* 64 bytes */
    int8_t logits[OUT_COLS]; /* 8 bytes */

    /* Layer 1: h1 = relu(features @ enc_w)  [1x640] @ [640x64] -> [1x64] */
    casper_matmul_i8(features, g_model.enc_w,
                     h1, 1, ENC_ROWS, ENC_COLS, 7);
    casper_relu_i8(h1, ENC_COLS);

    /* Layer 2: h2 = relu(h1 @ hid_w)  [1x64] @ [64x64] -> [1x64] */
    casper_matmul_i8(h1, g_model.hid_w,
                     h2, 1, HID_ROWS, HID_COLS, 7);
    casper_relu_i8(h2, HID_COLS);

    /* Layer 3: logits = h2 @ out_w  [1x64] @ [64x8] -> [1x8] */
    casper_matmul_i8(h2, g_model.out_w,
                     logits, 1, OUT_ROWS, OUT_COLS, 7);

    /* Argmax over first N_CLASSES logits. */
    size_t winner = casper_argmax_i8(logits, N_CLASSES);
    *out_class      = (uint8_t)winner;
    *out_confidence = casper_confidence_i8(logits, N_CLASSES, winner);
    return 0;
}

/* -------------------------------------------------------------------------
 * casper_sarf_analyze
 * -------------------------------------------------------------------------*/
int casper_sarf_analyze(
    const uint8_t *text,
    size_t         text_len,
    uint32_t      *out_roots,
    size_t        *out_count)
{
    if (!text || !out_roots || !out_count) return -1;
    return casper_sarf_analyze_text(text, text_len, out_roots, out_count);
}
