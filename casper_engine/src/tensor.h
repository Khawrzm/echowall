/**
 * tensor.h — INT8 matrix operations for Casper Engine
 *
 * Implements cache-friendly, loop-unrolled INT8 GEMM using ARM NEON
 * intrinsics (AArch64). Falls back to portable C11 on non-NEON targets.
 *
 * All accumulation is INT16 to prevent overflow before final INT8 clamp.
 * No heap allocation. Caller provides output buffer.
 *
 * C11, zero external dependencies.
 */
#ifndef CASPER_TENSOR_H
#define CASPER_TENSOR_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * casper_matmul_i8 — INT8 matrix multiply: C = A * B
 *
 * A is [M x K], B is [K x N], C (output) is [M x N].
 * All matrices are row-major, contiguous.
 *
 * Accumulation is INT16 (saturating). Output is clamped to INT8 [-128, 127]
 * by right-shifting the INT16 accumulator by `shift` bits before clamping.
 *
 * @param A       Input matrix A (M x K), INT8.
 * @param B       Input matrix B (K x N), INT8. Must be column-major for NEON
 *                path (pre-transpose B before calling for best performance).
 * @param C       Output matrix C (M x N), INT8. Caller-allocated.
 * @param M       Rows of A and C.
 * @param K       Cols of A / Rows of B.
 * @param N       Cols of B and C.
 * @param shift   Right-shift applied to INT16 accumulator before INT8 clamp.
 *                Typical value: 7 (divide by 128 to keep INT8 range).
 */
void casper_matmul_i8(
    const int8_t * restrict A,
    const int8_t * restrict B,
    int8_t       * restrict C,
    size_t M, size_t K, size_t N,
    int shift
);

/**
 * casper_relu_i8 — ReLU activation in-place on an INT8 vector.
 *
 * @param vec  INT8 vector, modified in-place.
 * @param len  Number of elements.
 */
void casper_relu_i8(int8_t *vec, size_t len);

/**
 * casper_argmax_i8 — Return index of maximum value in an INT8 vector.
 *
 * @param vec  INT8 vector.
 * @param len  Number of elements.
 * @return     Index of the maximum element (first occurrence on tie).
 */
size_t casper_argmax_i8(const int8_t *vec, size_t len);

/**
 * casper_confidence_i8 — Compute softmax-approximated confidence (0-255)
 * for the winning class.
 *
 * Uses a max-subtracted INT16 exponential approximation to avoid overflow.
 * Output is scaled to [0, 255] (maps to [0.0, 1.0] confidence).
 *
 * @param logits  INT8 logit vector.
 * @param len     Number of classes.
 * @param winner  Index of the winning class.
 * @return        Confidence in [0, 255].
 */
uint8_t casper_confidence_i8(const int8_t *logits, size_t len, size_t winner);

#ifdef __cplusplus
}
#endif

#endif /* CASPER_TENSOR_H */
