#pragma once

#include <wasm_simd128.h>

// shuffle

#define _WASM_SHUFFLE(fp3, fp2, fp1, fp0) (((fp3) << 6) | ((fp2) << 4) | \
                                        ((fp1) << 2) | ((fp0)))

#define wasm_i32x4_shuffle_imm(__a, __imm) __extension__ ({ \
    wasm_i32x4_shuffle((__a), \
                              wasm_i32x4_splat(0), \
                              ((__imm) & 0x3), (((__imm) & 0xc) >> 2), \
                              (((__imm) & 0x30) >> 4), (((__imm) & 0xc0) >> 6)); })

// unpack
static inline v128_t wasm_unpacklo_i8x16(v128_t a, v128_t b) {
  return wasm_i8x16_shuffle(a, b, 0, 16, 1, 17, 2, 18, 3, 19, 4, 20, 5, 21, 6,
                            22, 7, 23);
}

static inline v128_t wasm_unpacklo_i16x8(v128_t a, v128_t b) {
  return wasm_i8x16_shuffle(a, b, 0, 1, 16, 17, 2, 3, 18, 19, 4, 5, 20, 21, 6,
                            7, 22, 23);
}

static inline v128_t wasm_unpacklo_i32x4(v128_t a, v128_t b) {
  return wasm_i8x16_shuffle(a, b, 0, 1, 2, 3, 16, 17, 18, 19, 4, 5, 6, 7, 20,
                            21, 22, 23);
}

static inline v128_t wasm_unpacklo_i64x2(v128_t a, v128_t b) {
  return wasm_i8x16_shuffle(a, b, 0, 1, 2, 3, 4, 5, 6, 7, 16, 17, 18, 19, 20,
                            21, 22, 23);
}

static inline v128_t wasm_unpackhi_i8x16(v128_t a, v128_t b) {
  return wasm_i8x16_shuffle(a, b, 8, 24, 9, 25, 10, 26, 11, 27, 12, 28, 13, 29,
                            14, 30, 15, 31);
}

static inline v128_t wasm_unpackhi_i16x8(v128_t a, v128_t b) {
  return wasm_i8x16_shuffle(a, b, 8, 9, 24, 25, 10, 11, 26, 27, 12, 13, 28, 29,
                            14, 15, 30, 31);
}

static inline v128_t wasm_unpackhi_i32x4(v128_t a, v128_t b) {
  return wasm_i8x16_shuffle(a, b, 8, 9, 10, 11, 24, 25, 26, 27, 12, 13, 14, 15,
                            28, 29, 30, 31);
}

static inline v128_t wasm_unpackhi_i64x2(v128_t a, v128_t b) {
  return wasm_i8x16_shuffle(a, b, 8, 9, 10, 11, 12, 13, 14, 15, 24, 25, 26, 27,
                            28, 29, 30, 31);
}

// convert
// inline v128_t v128_cvtu8x16_i16x8(const v128_t a) {
//   const v128_t z = wasm_i8x16_splat(0);
//   return wasm_unpacklo_i8x16(a, z);
// }

// inline v128_t v128_cvti8x16_i16x8(const v128_t a) {
//   return wasm_i16x8_shr(wasm_unpacklo_i8x16(a, a), 8);
// }

// inline v128_t v128_cvtu8x16_i32x4(const v128_t a) {
//   const v128_t z = wasm_i8x16_splat(0);
//   return wasm_unpacklo_i16x8(wasm_unpacklo_i8x16(a, z), z);
// }

// inline v128_t v128_cvti8x16_i32x4(const v128_t a) {
//   v128_t r = wasm_unpacklo_i8x16(a, a);
//   r = wasm_unpacklo_i8x16(r, r);
//   return wasm_i32x4_shr(r, 24);
// }

// inline v128_t v128_cvtu16x8_i32x4(const v128_t a) {
//   const v128_t z = wasm_i8x16_splat(0);
//   return wasm_unpacklo_i16x8(a, z);
// }

// inline v128_t v128_cvti16x8_i32x4(const v128_t a) {
//   return wasm_i32x4_shr(wasm_unpacklo_i16x8(a, a), 16);
// }

// inline v128_t v128_cvtu32x4_i64x2(const v128_t a) {
//   const v128_t z = wasm_i8x16_splat(0);
//   return wasm_unpacklo_i32x4(a, z);
// }

// inline v128_t v128_cvti32x4_i64x2(const v128_t a) {
//   return wasm_unpacklo_i32x4(a, wasm_i32x4_shr(a, 31));
// }

// inline v128_t v128_cvtu8x16_i16x8_high(const v128_t a) {
//   const v128_t z = wasm_i8x16_splat(0);
//   return wasm_unpackhi_i8x16(a, z);
// }

// inline v128_t v128_cvti8x16_i16x8_high(const v128_t a) {
//   return wasm_i16x8_shr(wasm_unpackhi_i8x16(a, a), 8);
// }

// inline v128_t v128_cvtu16x8_i32x4_high(const v128_t a) {
//   const v128_t z = wasm_i8x16_splat(0);
//   return wasm_unpackhi_i16x8(a, z);
// }

// inline v128_t v128_cvti16x8_i32x4_high(const v128_t a) {
//   return wasm_i32x4_shr(wasm_unpackhi_i16x8(a, a), 16);
// }

// inline v128_t v128_cvtu32x4_i64x2_high(const v128_t a) {
//   const v128_t z = wasm_i8x16_splat(0);
//   return wasm_unpackhi_i32x4(a, z);
// }

// inline v128_t v128_cvti32x4_i64x2_high(const v128_t a) {
//   return wasm_unpackhi_i32x4(a, wasm_i32x4_shr(a, 31));
// }

// arithmetic
static inline v128_t wasm_u64x2_mulu(const v128_t a, const v128_t b) {
  return wasm_u64x2_extmul_low_u32x4(
      wasm_v32x4_shuffle(a, a, 0, 2, 0, 2),
      wasm_v32x4_shuffle(b, b, 0, 2, 0, 2));
}