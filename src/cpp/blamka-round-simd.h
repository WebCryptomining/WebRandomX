#ifndef BLAKE_ROUND_MKA_OPT_H
#define BLAKE_ROUND_MKA_OPT_H

#include "blake2-impl.h"
#include "intrin_wasm_simd.hpp"

#define rotr32(x)                                                              \
    wasm_i32x4_shuffle_imm(x, _WASM_SHUFFLE(2, 3, 0, 1))
#define rotr24(x)                                                              \
    wasm_v128_xor(wasm_u64x2_shr(x, 24), wasm_i64x2_shl(x, 40))
#define rotr16(x)                                                              \
    wasm_v128_xor(wasm_u64x2_shr(x, 16), wasm_i64x2_shl(x, 48))
#define rotr63(x)                                                              \
    wasm_v128_xor(wasm_u64x2_shr(x, 63), wasm_i64x2_add(x, x))

static inline v128_t fBlaMka(v128_t x, v128_t y) {
    const v128_t z = wasm_u64x2_mulu(x, y);
    return wasm_i64x2_add(wasm_i64x2_add(x, y), wasm_i64x2_add(z, z));
}

#define G1(A0, B0, C0, D0, A1, B1, C1, D1)                                     \
    do {                                                                       \
        A0 = fBlaMka(A0, B0);                                                  \
        A1 = fBlaMka(A1, B1);                                                  \
                                                                               \
        D0 = wasm_v128_xor(D0, A0);                                            \
        D1 = wasm_v128_xor(D1, A1);                                            \
                                                                               \
        D0 = rotr32(D0);                                                       \
        D1 = rotr32(D1);                                                       \
                                                                               \
        C0 = fBlaMka(C0, D0);                                                  \
        C1 = fBlaMka(C1, D1);                                                  \
                                                                               \
        B0 = wasm_v128_xor(B0, C0);                                            \
        B1 = wasm_v128_xor(B1, C1);                                            \
                                                                               \
        B0 = rotr24(B0);                                                       \
        B1 = rotr24(B1);                                                       \
    } while ((void)0, 0)

#define G2(A0, B0, C0, D0, A1, B1, C1, D1)                                     \
    do {                                                                       \
        A0 = fBlaMka(A0, B0);                                                  \
        A1 = fBlaMka(A1, B1);                                                  \
                                                                               \
        D0 = wasm_v128_xor(D0, A0);                                            \
        D1 = wasm_v128_xor(D1, A1);                                            \
                                                                               \
        D0 = rotr16(D0);                                                       \
        D1 = rotr16(D1);                                                       \
                                                                               \
        C0 = fBlaMka(C0, D0);                                                  \
        C1 = fBlaMka(C1, D1);                                                  \
                                                                               \
        B0 = wasm_v128_xor(B0, C0);                                            \
        B1 = wasm_v128_xor(B1, C1);                                            \
                                                                               \
        B0 = rotr63(B0);                                                       \
        B1 = rotr63(B1);                                                       \
    } while ((void)0, 0)

#define DIAGONALIZE(A0, B0, C0, D0, A1, B1, C1, D1)                            \
    do {                                                                       \
        v128_t t0 = D0;                                                        \
        v128_t t1 = B0;                                                        \
                                                                               \
        D0 = C0;                                                               \
        C0 = C1;                                                               \
        C1 = D0;                                                               \
                                                                               \
        D0 = wasm_unpackhi_i64x2(D1, wasm_unpacklo_i64x2(t0, t0));             \
        D1 = wasm_unpackhi_i64x2(t0, wasm_unpacklo_i64x2(D1, D1));             \
        B0 = wasm_unpackhi_i64x2(B0, wasm_unpacklo_i64x2(B1, B1));             \
        B1 = wasm_unpackhi_i64x2(B1, wasm_unpacklo_i64x2(t1, t1));             \
    } while ((void)0, 0)

#define UNDIAGONALIZE(A0, B0, C0, D0, A1, B1, C1, D1)                          \
    do {                                                                       \
        v128_t t0 = C0;                                                        \
        C0 = C1;                                                               \
        C1 = t0;                                                               \
        t0 = B0;                                                               \
        v128_t t1 = D0;                                                        \
                                                                               \
        B0 = wasm_unpackhi_i64x2(B1, wasm_unpacklo_i64x2(B0, B0));             \
        B1 = wasm_unpackhi_i64x2(t0, wasm_unpacklo_i64x2(B1, B1));             \
        D0 = wasm_unpackhi_i64x2(D0, wasm_unpacklo_i64x2(D1, D1));             \
        D1 = wasm_unpackhi_i64x2(D1, wasm_unpacklo_i64x2(t1, t1));             \
    } while ((void)0, 0)

#define BLAKE2_ROUND(A0, A1, B0, B1, C0, C1, D0, D1)                           \
    do {                                                                       \
        G1(A0, B0, C0, D0, A1, B1, C1, D1);                                    \
        G2(A0, B0, C0, D0, A1, B1, C1, D1);                                    \
                                                                               \
        DIAGONALIZE(A0, B0, C0, D0, A1, B1, C1, D1);                           \
                                                                               \
        G1(A0, B0, C0, D0, A1, B1, C1, D1);                                    \
        G2(A0, B0, C0, D0, A1, B1, C1, D1);                                    \
                                                                               \
        UNDIAGONALIZE(A0, B0, C0, D0, A1, B1, C1, D1);                         \
    } while ((void)0, 0)

#endif /* BLAKE_ROUND_MKA_OPT_H */