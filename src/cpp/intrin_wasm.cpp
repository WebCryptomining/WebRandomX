#include "intrin_wasm.h"

#include <cfenv>
#include <cmath>

#include "common.hpp"
#include "softfloat.hpp"

#if defined(__SIZEOF_INT128__)
typedef unsigned __int128 uint128_t;
typedef __int128 int128_t;
uint64_t mulh(uint64_t a, uint64_t b) { return ((uint128_t)a * b) >> 64; }
int64_t smulh(int64_t a, int64_t b) { return ((int128_t)a * b) >> 64; }
#define HAVE_MULH
#define HAVE_SMULH
#endif

#ifndef HAVE_SETROUNDMODE_IMPL
static void setRoundMode_(uint_fast8_t mode) {
  globalRoundingMode = mode;
  // fesetround(mode);
}
#endif

#ifndef HAVE_ROTR
uint64_t rotr(uint64_t a, unsigned int b) {
  return (a >> b) | (a << (-b & 63));
}
#define HAVE_ROTR
#endif

#ifndef HAVE_ROTL
uint64_t rotl(uint64_t a, unsigned int b) {
  return (a << b) | (a >> (-b & 63));
}
#define HAVE_ROTL
#endif

#ifndef HAVE_MULH
#define LO(x) ((x)&0xffffffff)
#define HI(x) ((x) >> 32)
uint64_t mulh(uint64_t a, uint64_t b) {
  uint64_t ah = HI(a), al = LO(a);
  uint64_t bh = HI(b), bl = LO(b);
  uint64_t x00 = al * bl;
  uint64_t x01 = al * bh;
  uint64_t x10 = ah * bl;
  uint64_t x11 = ah * bh;
  uint64_t m1 = LO(x10) + LO(x01) + HI(x00);
  uint64_t m2 = HI(x10) + HI(x01) + LO(x11) + HI(m1);
  uint64_t m3 = HI(x11) + HI(m2);

  return (m3 << 32) + LO(m2);
}
#define HAVE_MULH
#endif

#ifndef HAVE_SMULH
int64_t smulh(int64_t a, int64_t b) {
  int64_t hi = mulh(a, b);
  if (a < 0LL) hi -= b;
  if (b < 0LL) hi -= a;
  return hi;
}
#define HAVE_SMULH
#endif

#ifdef RANDOMX_DEFAULT_FENV

void rx_reset_float_state() {
  setRoundMode_(FE_TONEAREST);
  // rx_set_double_precision();
}

void rx_set_rounding_mode(uint32_t mode) {
  switch (mode & 3) {
    case RoundDown:
      setRoundMode_(round_min);
      break;
    case RoundUp:
      setRoundMode_(round_max);
      break;
    case RoundToZero:
      setRoundMode_(round_minMag);
      break;
    case RoundToNearest:
      setRoundMode_(round_near_even);
      break;
    default:
      UNREACHABLE;
  }
}

#endif
