/**
 * sarf.h — Arabic morphological root extraction (Sarf)
 *
 * Decodes packed uint32_t root codes for the 2,976 Arabic Binyan forms.
 * Root codes follow the packing scheme defined in cognitive_lobe/src/sarf.rs:
 *
 *   Bits 31-22: form_index  (0 .. 2975)
 *   Bits 21-14: R1 offset from Unicode 0x0621 (Arabic block base)
 *   Bits 13-7:  R2 offset
 *   Bits 6-0:   R3 offset
 *
 * Output: an array of casper_root_t descriptors.
 *
 * C11, zero external dependencies.
 */
#ifndef CASPER_SARF_H
#define CASPER_SARF_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Total canonical Arabic verb forms in the Sarf table. */
#define SARF_FORM_COUNT 2976

/** Unicode base of the Arabic presentation block. */
#define ARABIC_UNICODE_BASE 0x0621u

/** Arabic verb binyan class (Form I – X). */
typedef enum {
    BINYAN_I    = 1,
    BINYAN_II   = 2,
    BINYAN_III  = 3,
    BINYAN_IV   = 4,
    BINYAN_V    = 5,
    BINYAN_VI   = 6,
    BINYAN_VII  = 7,
    BINYAN_VIII = 8,
    BINYAN_IX   = 9,
    BINYAN_X    = 10,
} casper_binyan_t;

/** A decoded morphological root descriptor. */
typedef struct {
    uint32_t       r1;          /**< First radical (Unicode scalar). */
    uint32_t       r2;          /**< Second radical. */
    uint32_t       r3;          /**< Third radical. */
    casper_binyan_t binyan;     /**< Binyan class (I – X). */
    uint16_t       form_index;  /**< Index 0 .. SARF_FORM_COUNT-1. */
} casper_root_t;

/**
 * casper_sarf_decode — Decode a single packed root code.
 *
 * @param root_code  Packed uint32 as defined above.
 * @param out        Output descriptor. Populated on success.
 * @return           0 on success, -1 if form_index >= SARF_FORM_COUNT.
 */
int casper_sarf_decode(uint32_t root_code, casper_root_t *out);

/**
 * casper_sarf_analyze_text — Extract morphological roots from a UTF-8
 * Arabic text buffer.
 *
 * Tokenizes the input by whitespace, attempts to match each token against
 * the canonical Sarf form table (root-code reconstruction from tri-literal
 * stem detection), and writes decoded roots to `out_roots`.
 *
 * @param text       UTF-8 Arabic text (not null-terminated requirement;
 *                   length is given by text_len).
 * @param text_len   Byte length of text.
 * @param out_roots  Output array of root codes (uint32_t). Caller-allocated.
 * @param capacity   On input: capacity of out_roots.
 *                   On output: number of roots written.
 * @return           0 on success, -1 on parameter error.
 */
int casper_sarf_analyze_text(
    const uint8_t *text,
    size_t         text_len,
    uint32_t      *out_roots,
    size_t        *capacity
);

#ifdef __cplusplus
}
#endif

#endif /* CASPER_SARF_H */
