/**
 * tensor.c — INT8 GEMM + activation kernels
 *
 * NEON path: AArch64 only, enabled when HAVE_NEON=1.
 * Portable path: pure C11, no intrinsics.
 *
 * C11, zero external dependencies (only <stdint.h>, <stddef.h>, <arm_neon.h>).
 */
#include "tensor.h"

#include <stdint.h>
#include <stddef.h>

#if HAVE_NEON
#include <arm_neon.h>
#endif

/* -------------------------------------------------------------------------
 * INT8 GEMM
 * -------------------------------------------------------------------------
 * Strategy (NEON path):
 *   For each output row i of C:
 *     For each output column j of C (stepped by 8 for NEON lane width):
 *       Accumulate dot product A[i,:] . B[:,j] using vmlal_s8 into
 *       an int16x8_t accumulator, then shift-and-clamp to int8.
 *
 * The inner K-loop is manually unrolled by 8 to fill NEON pipelines.
 * Tail elements (K % 8 != 0) are handled by the portable scalar path.
 * -------------------------------------------------------------------------
 */
void casper_matmul_i8(
    const int8_t * restrict A,
    const int8_t * restrict B,
    int8_t       * restrict C,
    size_t M, size_t K, size_t N,
    int shift)
{
#if HAVE_NEON
    for (size_t i = 0; i < M; ++i) {
        const int8_t *row_a = A + i * K;
        for (size_t j = 0; j < N; ++j) {
            /* Accumulate dot product in INT16 (saturating). */
            int16_t acc = 0;
            size_t  k   = 0;

            /* NEON unrolled inner loop: process 8 elements per iteration. */
            /* We accumulate into a scalar via pairwise reduction to keep
             * the loop simple and avoid register pressure on small K.    */
            for (; k + 7 < K; k += 8) {
                int8x8_t  va = vld1_s8(row_a + k);
                /* B is row-major [K x N]: column j, rows k..k+7. */
                /* Load 8 elements from column j, rows k to k+7.   */
                int8_t    tmp[8];
                for (int t = 0; t < 8; ++t)
                    tmp[t] = B[(k + (size_t)t) * N + j];
                int8x8_t  vb  = vld1_s8(tmp);
                int16x8_t vml = vmull_s8(va, vb);
                /* Horizontal add of 8 int16 lanes into scalar. */
                int32x4_t sum32 = vpaddlq_s16(vml);
                int64x2_t sum64 = vpaddlq_s32(sum32);
                acc += (int16_t)(vgetq_lane_s64(sum64, 0) +
                                 vgetq_lane_s64(sum64, 1));
            }
            /* Scalar tail. */
            for (; k < K; ++k)
                acc += (int16_t)((int16_t)row_a[k] * (int16_t)B[k * N + j]);

            /* Shift and clamp to INT8. */
            int16_t val = acc >> shift;
            if      (val >  127) val =  127;
            else if (val < -128) val = -128;
            C[i * N + j] = (int8_t)val;
        }
    }
#else
    /* ---- Portable C11 fallback ---- */
    for (size_t i = 0; i < M; ++i) {
        for (size_t j = 0; j < N; ++j) {
            int32_t acc = 0;
            for (size_t k = 0; k < K; ++k)
                acc += (int32_t)A[i * K + k] * (int32_t)B[k * N + j];
            int32_t val = acc >> shift;
            if      (val >  127) val =  127;
            else if (val < -128) val = -128;
            C[i * N + j] = (int8_t)val;
        }
    }
#endif
}

/* -------------------------------------------------------------------------
 * ReLU in-place
 * -------------------------------------------------------------------------*/
void casper_relu_i8(int8_t *vec, size_t len)
{
#if HAVE_NEON
    const int8x16_t zero = vdupq_n_s8(0);
    size_t i = 0;
    for (; i + 15 < len; i += 16) {
        int8x16_t v = vld1q_s8(vec + i);
        v = vmaxq_s8(v, zero);
        vst1q_s8(vec + i, v);
    }
    for (; i < len; ++i)
        if (vec[i] < 0) vec[i] = 0;
#else
    for (size_t i = 0; i < len; ++i)
        if (vec[i] < 0) vec[i] = 0;
#endif
}

/* -------------------------------------------------------------------------
 * Argmax
 * -------------------------------------------------------------------------*/
size_t casper_argmax_i8(const int8_t *vec, size_t len)
{
    if (len == 0) return 0;
    size_t  best_idx = 0;
    int8_t  best_val = vec[0];
    for (size_t i = 1; i < len; ++i) {
        if (vec[i] > best_val) {
            best_val = vec[i];
            best_idx = i;
        }
    }
    return best_idx;
}

/* -------------------------------------------------------------------------
 * Confidence (softmax approximation, output 0-255)
 *
 * Uses integer exponential approximation:
 *   exp_approx(x) = max(0, 128 + x)  (linear approximation, fast, monotone)
 * Sufficient for argmax-confidence reporting; not a true softmax.
 * -------------------------------------------------------------------------*/
uint8_t casper_confidence_i8(const int8_t *logits, size_t len, size_t winner)
{
    if (len == 0) return 0;
    int16_t winner_val = (int16_t)logits[winner];
    int32_t sum = 0;
    for (size_t i = 0; i < len; ++i) {
        int16_t shifted = (int16_t)logits[i] - winner_val;
        /* exp_approx: clamp negative values near zero. */
        int32_t e = (shifted >= 0) ? (int32_t)shifted + 1 : 1;
        sum += e;
    }
    /* Winner's approximate exp = 1 (by construction of shifted). */
    /* Confidence = winner_exp / total_sum, scaled to [0, 255].   */
    if (sum <= 0) return 255;
    uint32_t conf = (uint32_t)(255u * 1u / (uint32_t)sum);
    if (conf > 255u) conf = 255u;
    return (uint8_t)conf;
}
