#pragma once

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <stdexcept>
#include <wasm_simd128.h>

#include "endian.h"
#include "softfloat.hpp"

constexpr int32_t unsigned32ToSigned2sCompl(uint32_t x) {
  return (-1 == ~0)
             ? (int32_t)x
             : (x > INT32_MAX ? (-(int32_t)(UINT32_MAX - x) - 1) : (int32_t)x);
}

constexpr int64_t unsigned64ToSigned2sCompl(uint64_t x) {
  return (-1 == ~0)
             ? (int64_t)x
             : (x > INT64_MAX ? (-(int64_t)(UINT64_MAX - x) - 1) : (int64_t)x);
}

constexpr uint64_t signExtend2sCompl(uint32_t x) {
  return (-1 == ~0)
             ? (int64_t)(int32_t)(x)
             : (x > INT32_MAX ? (x | 0xffffffff00000000ULL) : (uint64_t)x);
}

constexpr int RoundToNearest = 0;
constexpr int RoundDown = 1;
constexpr int RoundUp = 2;
constexpr int RoundToZero = 3;

#define rx_sqrt sqrt

typedef v128_t rx_vec_i128;
typedef v128_t rx_vec_f128;

#define rx_aligned_alloc(a, b) aligned_alloc(b, a)
#define rx_aligned_free(a) free(a)
#define rx_prefetch_nta(x)
#define rx_prefetch_t0(x)

#define rx_load_vec_f128 wasm_v128_load
#define rx_store_vec_f128 wasm_v128_store

FORCE_INLINE rx_vec_f128 rx_set_vec_f128(uint64_t x1, uint64_t x0) {
  return wasm_i64x2_make(x0, x1);
}

FORCE_INLINE rx_vec_f128 rx_set1_vec_f128(uint64_t x) {
  return wasm_i64x2_make(x, x);
}

FORCE_INLINE rx_vec_f128 rx_swap_vec_f128(rx_vec_f128 a) {
  return wasm_i8x16_shuffle(a, a, 8, 9, 10, 11, 12, 13, 14, 15, 0, 1, 2, 3, 4, 5, 6, 7);
}

FORCE_INLINE double rx_vec_f128_lo(rx_vec_f128 a) {
	return wasm_f64x2_extract_lane(a, 0);
}

FORCE_INLINE double rx_vec_f128_hi(rx_vec_f128 a) {
	rx_vec_f128 tmp = wasm_i8x16_shuffle(a, a, 8, 9, 10, 11, 12, 13, 14, 15, 8, 9, 10, 11, 12, 13, 14, 15);
	return wasm_f64x2_extract_lane(tmp, 0);
}

FORCE_INLINE rx_vec_f128 rx_add_vec_f128(rx_vec_f128 a, rx_vec_f128 b) {
  if (globalRoundingMode == round_near_even) return wasm_f64x2_add(a, b);
  softdouble rlo = softdouble(rx_vec_f128_lo(a)) + softdouble(rx_vec_f128_lo(b));
  softdouble rhi = softdouble(rx_vec_f128_hi(a)) + softdouble(rx_vec_f128_hi(b));
  return wasm_f64x2_make(double(rlo), double(rhi));
}

FORCE_INLINE rx_vec_f128 rx_sub_vec_f128(rx_vec_f128 a, rx_vec_f128 b) {
  if (globalRoundingMode == round_near_even) return wasm_f64x2_sub(a, b);
  softdouble rlo = softdouble(rx_vec_f128_lo(a)) - softdouble(rx_vec_f128_lo(b));
  softdouble rhi = softdouble(rx_vec_f128_hi(a)) - softdouble(rx_vec_f128_hi(b));
  return wasm_f64x2_make(double(rlo), double(rhi));
}

FORCE_INLINE rx_vec_f128 rx_mul_vec_f128(rx_vec_f128 a, rx_vec_f128 b) {
  if (globalRoundingMode == round_near_even) return wasm_f64x2_mul(a, b);
  softdouble rlo = softdouble(rx_vec_f128_lo(a)) * softdouble(rx_vec_f128_lo(b));
  softdouble rhi = softdouble(rx_vec_f128_hi(a)) * softdouble(rx_vec_f128_hi(b));
  return wasm_f64x2_make(double(rlo), double(rhi));
}

FORCE_INLINE rx_vec_f128 rx_div_vec_f128(rx_vec_f128 a, rx_vec_f128 b) {
  if (globalRoundingMode == round_near_even) return wasm_f64x2_div(a, b);
  softdouble rlo = softdouble(rx_vec_f128_lo(a)) / softdouble(rx_vec_f128_lo(b));
  softdouble rhi = softdouble(rx_vec_f128_hi(a)) / softdouble(rx_vec_f128_hi(b));
  return wasm_f64x2_make(double(rlo), double(rhi));
}

FORCE_INLINE rx_vec_f128 rx_sqrt_vec_f128(rx_vec_f128 a) {
  if (globalRoundingMode == round_near_even) return wasm_f64x2_sqrt(a);
  softdouble rlo = sqrt(softdouble(rx_vec_f128_lo(a)));
  softdouble rhi = sqrt(softdouble(rx_vec_f128_hi(a)));
  return wasm_f64x2_make(double(rlo), double(rhi));
}

#define rx_xor_vec_f128 wasm_v128_xor
#define rx_and_vec_f128 wasm_v128_and
#define rx_or_vec_f128 wasm_v128_or

FORCE_INLINE int rx_vec_i128_x(rx_vec_i128 a) {
	return wasm_i32x4_extract_lane(a, 0);
}

FORCE_INLINE int rx_vec_i128_y(rx_vec_i128 a) {
  rx_vec_i128 tmp = wasm_i8x16_shuffle(a, a, 4, 5, 6, 7, 4, 5, 6, 7, 4, 5, 6, 7, 4, 5, 6, 7);
	return wasm_i32x4_extract_lane(tmp, 0);
}

FORCE_INLINE int rx_vec_i128_z(rx_vec_i128 a) {
	rx_vec_i128 tmp = wasm_i8x16_shuffle(a, a, 8, 9, 10, 11, 8, 9, 10, 11, 8, 9, 10, 11, 8, 9, 10, 11);
	return wasm_i32x4_extract_lane(tmp, 0);
}

FORCE_INLINE int rx_vec_i128_w(rx_vec_i128 a) {
	rx_vec_i128 tmp = wasm_i8x16_shuffle(a, a, 12, 13, 14, 15, 12, 13, 14, 15, 12, 13, 14, 15, 12, 13, 14, 15);
	return wasm_i32x4_extract_lane(tmp, 0);
}

FORCE_INLINE rx_vec_i128 rx_set_int_vec_i128(int _I3, int _I2, int _I1, int _I0) {
  int v[] = {_I0, _I1, _I2, _I3};
  return wasm_v128_load(v);
}

#define rx_xor_vec_i128 wasm_v128_xor
#define rx_load_vec_i128 wasm_v128_load
#define rx_store_vec_i128 wasm_v128_store

FORCE_INLINE rx_vec_f128 rx_cvt_packed_int_vec_f128(const void* addr) {
  double lo = (double)unsigned32ToSigned2sCompl(load32((uint8_t*)addr + 0));
  double hi = unsigned32ToSigned2sCompl(load32((uint8_t*)addr + 4));
  return wasm_f64x2_make(lo, hi);
}

#define RANDOMX_DEFAULT_FENV

#ifdef RANDOMX_DEFAULT_FENV
void rx_reset_float_state();
void rx_set_rounding_mode(uint32_t mode);
#endif

uint64_t mulh(uint64_t, uint64_t);
int64_t smulh(int64_t, int64_t);
uint64_t rotl(uint64_t, unsigned int);
uint64_t rotr(uint64_t, unsigned int);
