/**
 * sarf.c — Arabic morphological root extraction implementation
 *
 * C11, zero external dependencies (only <stdint.h>, <stddef.h>).
 */
#include "sarf.h"

#include <stdint.h>
#include <stddef.h>

/* -------------------------------------------------------------------------
 * Root code packing layout (matches cognitive_lobe/src/sarf.rs):
 *
 *   bits 31-22 : form_index  (10 bits, 0..2975)
 *   bits 21-14 : R1 offset   (8 bits)
 *   bits 13-7  : R2 offset   (7 bits)
 *   bits  6-0  : R3 offset   (7 bits)
 * -------------------------------------------------------------------------*/

#define FORM_INDEX_SHIFT  22u
#define FORM_INDEX_MASK   0x3FFu
#define R1_SHIFT          14u
#define R1_MASK           0xFFu
#define R2_SHIFT           7u
#define R2_MASK           0x7Fu
#define R3_SHIFT           0u
#define R3_MASK           0x7Fu

/* Map form_index to binyan class (10 classes, evenly distributed). */
static casper_binyan_t binyan_from_index(uint16_t form_index)
{
    /* 2976 forms / 10 classes = ~297 forms per class. */
    uint32_t cls = ((uint32_t)form_index * 10u) / SARF_FORM_COUNT + 1u;
    if (cls > 10u) cls = 10u;
    return (casper_binyan_t)cls;
}

/* -------------------------------------------------------------------------
 * casper_sarf_decode
 * -------------------------------------------------------------------------*/
int casper_sarf_decode(uint32_t root_code, casper_root_t *out)
{
    if (!out) return -1;

    uint16_t form_index = (uint16_t)((root_code >> FORM_INDEX_SHIFT) & FORM_INDEX_MASK);
    if ((uint32_t)form_index >= SARF_FORM_COUNT) return -1;

    uint32_t r1 = ARABIC_UNICODE_BASE + ((root_code >> R1_SHIFT) & R1_MASK);
    uint32_t r2 = ARABIC_UNICODE_BASE + ((root_code >> R2_SHIFT) & R2_MASK);
    uint32_t r3 = ARABIC_UNICODE_BASE + ((root_code >> R3_SHIFT) & R3_MASK);

    out->r1         = r1;
    out->r2         = r2;
    out->r3         = r3;
    out->binyan     = binyan_from_index(form_index);
    out->form_index = form_index;
    return 0;
}

/* -------------------------------------------------------------------------
 * UTF-8 helpers
 * -------------------------------------------------------------------------
 * We need to:
 *   1. Tokenize Arabic text by whitespace (ASCII space/tab/newline).
 *   2. For each token, attempt to detect a triliteral Arabic stem and
 *      reconstruct a root code.
 *
 * Arabic characters in the Unicode block U+0621–U+064A are encoded in
 * UTF-8 as two bytes: 0xD8 0xA1 .. 0xD9 0x8A.
 *
 * Triliteral stem detection strategy (deterministic approximation):
 *   - Walk the token, collect Unicode code points.
 *   - Filter to Arabic letter code points (U+0621–U+064A).
 *   - If exactly 3 Arabic letters remain after stripping common affixes
 *     (prefix alef/waw/ya, suffix ta-marbuta/alef), treat them as R1/R2/R3.
 *   - Compute form_index from token length × 127 mod SARF_FORM_COUNT
 *     (deterministic hash; real Sarf requires a full morphological DB).
 *
 * Radical Honesty: This is a structural approximation. A production Sarf
 * engine requires the Buckwalter morphological database or equivalent.
 * The algorithm here is deterministic and correct for simple triliteral
 * forms; complex derived forms require the full DB.
 * -------------------------------------------------------------------------*/

/** Decode one UTF-8 code point from buf[*pos..]. Advance *pos. */
static uint32_t utf8_next(const uint8_t *buf, size_t len, size_t *pos)
{
    if (*pos >= len) return 0;
    uint8_t b0 = buf[(*pos)++];
    if ((b0 & 0x80u) == 0u) return (uint32_t)b0;
    if ((b0 & 0xE0u) == 0xC0u && *pos < len) {
        uint8_t b1 = buf[(*pos)++];
        return ((uint32_t)(b0 & 0x1Fu) << 6u) | (uint32_t)(b1 & 0x3Fu);
    }
    if ((b0 & 0xF0u) == 0xE0u && *pos + 1 < len) {
        uint8_t b1 = buf[(*pos)++];
        uint8_t b2 = buf[(*pos)++];
        return ((uint32_t)(b0 & 0x0Fu) << 12u) |
               ((uint32_t)(b1 & 0x3Fu) <<  6u) |
                (uint32_t)(b2 & 0x3Fu);
    }
    /* 4-byte and invalid sequences: skip. */
    return 0xFFFDu;
}

/** Return 1 if cp is an Arabic letter (U+0621–U+064A). */
static int is_arabic_letter(uint32_t cp)
{
    return (cp >= 0x0621u && cp <= 0x064Au);
}

/** Return 1 if cp is an Arabic whitespace / diacritic to strip. */
static int is_arabic_affix(uint32_t cp)
{
    /* Common prefixes/suffixes: alef variants, waw, ya, ta-marbuta. */
    return (cp == 0x0627u || /* ALEF */
            cp == 0x0623u || /* ALEF WITH HAMZA ABOVE */
            cp == 0x0625u || /* ALEF WITH HAMZA BELOW */
            cp == 0x0648u || /* WAW */
            cp == 0x064Au || /* YA */
            cp == 0x0629u);  /* TA MARBUTA */
}

#define MAX_TOKEN_CODEPOINTS 32u

/**
 * Extract root code from a single Arabic token.
 * Returns 1 and writes root_code on success, 0 on failure.
 */
static int extract_root_from_token(
    const uint8_t *tok, size_t tok_len, uint32_t *root_code)
{
    uint32_t cps[MAX_TOKEN_CODEPOINTS];
    size_t   n = 0;
    size_t   pos = 0;

    /* Collect Arabic letters. */
    while (pos < tok_len && n < MAX_TOKEN_CODEPOINTS) {
        uint32_t cp = utf8_next(tok, tok_len, &pos);
        if (is_arabic_letter(cp)) cps[n++] = cp;
    }

    /* Strip leading/trailing affix candidates to find triliterals. */
    size_t start = 0, end = n;
    if (end > start && is_arabic_affix(cps[start])) start++;
    if (end > start && is_arabic_affix(cps[end - 1])) end--;

    if (end - start != 3u) return 0;  /* Not a triliteral stem. */

    uint32_t r1 = cps[start]     - ARABIC_UNICODE_BASE;
    uint32_t r2 = cps[start + 1] - ARABIC_UNICODE_BASE;
    uint32_t r3 = cps[start + 2] - ARABIC_UNICODE_BASE;

    /* Deterministic form_index hash: mix R1/R2/R3 into [0, 2975]. */
    uint32_t h = (r1 * 31u + r2) * 31u + r3;
    uint16_t form_index = (uint16_t)(h % (uint32_t)SARF_FORM_COUNT);

    /* Pack root code. */
    *root_code = ((uint32_t)form_index << FORM_INDEX_SHIFT) |
                 ((r1 & R1_MASK)       << R1_SHIFT)         |
                 ((r2 & R2_MASK)       << R2_SHIFT)         |
                  (r3 & R3_MASK);
    return 1;
}

/* -------------------------------------------------------------------------
 * casper_sarf_analyze_text
 * -------------------------------------------------------------------------*/
int casper_sarf_analyze_text(
    const uint8_t *text,
    size_t         text_len,
    uint32_t      *out_roots,
    size_t        *capacity)
{
    if (!text || !out_roots || !capacity || *capacity == 0) return -1;

    size_t out_count = 0;
    size_t cap       = *capacity;
    size_t i         = 0;

    while (i < text_len && out_count < cap) {
        /* Skip ASCII whitespace. */
        while (i < text_len &&
               (text[i] == ' ' || text[i] == '\t' ||
                text[i] == '\n' || text[i] == '\r'))
            i++;

        if (i >= text_len) break;

        /* Find end of token. */
        size_t tok_start = i;
        while (i < text_len &&
               text[i] != ' ' && text[i] != '\t' &&
               text[i] != '\n' && text[i] != '\r')
            i++;

        size_t tok_len = i - tok_start;
        if (tok_len == 0) continue;

        uint32_t root_code = 0;
        if (extract_root_from_token(text + tok_start, tok_len, &root_code))
            out_roots[out_count++] = root_code;
    }

    *capacity = out_count;
    return 0;
}
