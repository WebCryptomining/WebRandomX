// This file is based on files from package softfloat
// issued with the following license:

/*============================================================================

This C source file is part of the SoftFloat IEEE Floating-Point Arithmetic
Package, Release 3c, by John R. Hauser.

Copyright 2011, 2012, 2013, 2014, 2015, 2016, 2017 The Regents of the
University of California.  All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

 1. Redistributions of source code must retain the above copyright notice,
    this list of conditions, and the following disclaimer.

 2. Redistributions in binary form must reproduce the above copyright notice,
    this list of conditions, and the following disclaimer in the documentation
    and/or other materials provided with the distribution.

 3. Neither the name of the University nor the names of its contributors may
    be used to endorse or promote products derived from this software without
    specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS "AS IS", AND ANY
EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE, ARE
DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

=============================================================================*/

#include "softfloat.hpp"

uint_fast8_t globalDetectTininess = tininess_afterRounding;

uint_fast8_t globalRoundingMode = round_min;

enum {
  flag_inexact = 1,
  flag_underflow = 2,
  flag_overflow = 4,
  flag_infinite = 8,
  flag_invalid = 16
};

// Disabled to make softfloat code stateless
// May be changed in the future for better error handling
static inline void raiseFlags(uint_fast8_t /* flags */) {
  // exceptionFlags |= flags;
}

/*----------------------------------------------------------------------------
 *----------------------------------------------------------------------------*/
#define signF64UI(a) (((uint64_t)(a) >> 63) != 0)
#define expF64UI(a) ((int_fast16_t)((a) >> 52) & 0x7FF)
#define fracF64UI(a) ((a)&UINT64_C(0x000FFFFFFFFFFFFF))
#define packToF64UI(sign, exp, sig)                                          \
  ((uint64_t)(((uint_fast64_t)(sign) << 63) + ((uint_fast64_t)(exp) << 52) + \
              (sig)))
#define isNaNF64UI(a)                            \
  (((~(a)&UINT64_C(0x7FF0000000000000)) == 0) && \
   ((a)&UINT64_C(0x000FFFFFFFFFFFFF)))

// Type used to pass 64-bit floating-point arguments and results
// to/from functions
typedef softdouble float64_t;

// Integer-to-floating-point conversion routines
static float64_t ui32_to_f64(uint32_t);
static float64_t ui64_to_f64(uint64_t);
static float64_t i32_to_f64(int32_t);
static float64_t i64_to_f64(int64_t);

// Floating-point operations
static int_fast32_t f64_to_i32(float64_t, uint_fast8_t, bool);
static int_fast64_t f64_to_i64(float64_t, uint_fast8_t, bool);
static int_fast32_t f64_to_i32_r_minMag(float64_t, bool);
static float64_t f64_roundToInt(float64_t, uint_fast8_t, bool);
static float64_t f64_add(float64_t, float64_t);
static float64_t f64_sub(float64_t, float64_t);
static float64_t f64_mul(float64_t, float64_t);
static float64_t f64_div(float64_t, float64_t);
static float64_t f64_rem(float64_t, float64_t);
static float64_t f64_sqrt(float64_t);
static bool f64_eq(float64_t, float64_t);
static bool f64_le(float64_t, float64_t);
static bool f64_lt(float64_t, float64_t);

// Softdouble methods and members
softdouble::softdouble(const uint32_t a) { *this = ui32_to_f64(a); }
softdouble::softdouble(const uint64_t a) { *this = ui64_to_f64(a); }
softdouble::softdouble(const int32_t a) { *this = i32_to_f64(a); }
softdouble::softdouble(const int64_t a) { *this = i64_to_f64(a); }

softdouble softdouble::operator+(const softdouble& a) const {
  return f64_add(*this, a);
}
softdouble softdouble::operator-(const softdouble& a) const {
  return f64_sub(*this, a);
}
softdouble softdouble::operator*(const softdouble& a) const {
  return f64_mul(*this, a);
}
softdouble softdouble::operator/(const softdouble& a) const {
  return f64_div(*this, a);
}
softdouble softdouble::operator%(const softdouble& a) const {
  return f64_rem(*this, a);
}

bool softdouble::operator==(const softdouble& a) const {
  return f64_eq(*this, a);
}
bool softdouble::operator!=(const softdouble& a) const {
  return !f64_eq(*this, a);
}
bool softdouble::operator>(const softdouble& a) const {
  return f64_lt(a, *this);
}
bool softdouble::operator>=(const softdouble& a) const {
  return f64_le(a, *this);
}
bool softdouble::operator<(const softdouble& a) const {
  return f64_lt(*this, a);
}
bool softdouble::operator<=(const softdouble& a) const {
  return f64_le(*this, a);
}

// Overloads for math function(s)
softdouble sqrt(const softdouble& a) { return f64_sqrt(a); }

/*----------------------------------------------------------------------------
 *----------------------------------------------------------------------------*/
#define i32_fromPosOverflow 0x7FFFFFFF
#define i32_fromNegOverflow (-0x7FFFFFFF - 1)
#define i32_fromNaN 0x7FFFFFFF

#define i64_fromPosOverflow UINT64_C(0x7FFFFFFFFFFFFFFF)
// fixed unsigned unary minus: -x == ~x + 1
#define i64_fromNegOverflow (~UINT64_C(0x7FFFFFFFFFFFFFFF) + 1 - 1)
#define i64_fromNaN UINT64_C(0x7FFFFFFFFFFFFFFF)

/*----------------------------------------------------------------------------
 *----------------------------------------------------------------------------*/
#define defaultNaNF64UI UINT64_C(0xFFF8000000000000)
#define softfloat_isSigNaNF64UI(uiA)                                         \
  ((((uiA)&UINT64_C(0x7FF8000000000000)) == UINT64_C(0x7FF0000000000000)) && \
   ((uiA)&UINT64_C(0x0007FFFFFFFFFFFF)))
static uint_fast64_t softfloat_propagateNaNF64UI(uint_fast64_t uiA,
                                                 uint_fast64_t uiB);

/*----------------------------------------------------------------------------
 *----------------------------------------------------------------------------*/
// Little endian
struct uint128 {
  uint64_t v0, v64;
};
struct uint64_extra {
  uint64_t extra, v;
};
struct uint128_extra {
  uint64_t extra;
  struct uint128 v;
};

/*----------------------------------------------------------------------------
 *----------------------------------------------------------------------------*/
static int_fast32_t softfloat_roundToI32(bool, uint_fast64_t, uint_fast8_t,
                                         bool);
static int_fast64_t softfloat_roundToI64(bool, uint_fast64_t, uint_fast64_t,
                                         uint_fast8_t, bool);

struct exp16_sig64 {
  int_fast16_t exp;
  uint_fast64_t sig;
};
static struct exp16_sig64 softfloat_normSubnormalF64Sig(uint_fast64_t);

static float64_t softfloat_roundPackToF64(bool, int_fast16_t, uint_fast64_t);
static float64_t softfloat_normRoundPackToF64(bool, int_fast16_t,
                                              uint_fast64_t);

static float64_t softfloat_addMagsF64(uint_fast64_t, uint_fast64_t, bool);
static float64_t softfloat_subMagsF64(uint_fast64_t, uint_fast64_t, bool);

/*----------------------------------------------------------------------------
 *----------------------------------------------------------------------------*/
static uint32_t softfloat_approxRecipSqrt32_1(unsigned int oddExpA, uint32_t a);

static const uint16_t softfloat_approxRecipSqrt_1k0s[16] = {
    0xB4C9, 0xFFAB, 0xAA7D, 0xF11C, 0xA1C5, 0xE4C7, 0x9A43, 0xDA29,
    0x93B5, 0xD0E5, 0x8DED, 0xC8B7, 0x88C6, 0xC16D, 0x8424, 0xBAE1};
static const uint16_t softfloat_approxRecipSqrt_1k1s[16] = {
    0xA5A5, 0xEA42, 0x8C21, 0xC62D, 0x788F, 0xAA7F, 0x6928, 0x94B6,
    0x5CC7, 0x8335, 0x52A6, 0x74E2, 0x4A3E, 0x68FE, 0x432B, 0x5EFD};

/*----------------------------------------------------------------------------
 *----------------------------------------------------------------------------*/
static inline uint64_t softfloat_shortShiftRightJam64(uint64_t a,
                                                      uint_fast8_t dist) {
  return a >> dist | ((a & (((uint_fast64_t)1 << dist) - 1)) != 0);
}

static inline uint64_t softfloat_shiftRightJam64(uint64_t a,
                                                 uint_fast32_t dist) {
  // fixed unsigned unary minus: -x == ~x + 1
  return (dist < 63) ? a >> dist | ((uint64_t)(a << ((~dist + 1) & 63)) != 0)
                     : (a != 0);
}

/*----------------------------------------------------------------------------
 *----------------------------------------------------------------------------*/
static const uint_least8_t softfloat_countLeadingZeros8[256] = {
    8, 7, 6, 6, 5, 5, 5, 5, 4, 4, 4, 4, 4, 4, 4, 4, 3, 3, 3, 3, 3, 3, 3, 3,
    3, 3, 3, 3, 3, 3, 3, 3, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

static inline uint_fast8_t softfloat_countLeadingZeros32(uint32_t a) {
  uint_fast8_t count = 0;
  if (a < 0x10000) {
    count = 16;
    a <<= 16;
  }
  if (a < 0x1000000) {
    count += 8;
    a <<= 8;
  }
  count += softfloat_countLeadingZeros8[a >> 24];
  return count;
}

static uint_fast8_t softfloat_countLeadingZeros64(uint64_t a);

/*----------------------------------------------------------------------------
 *----------------------------------------------------------------------------*/
#define softfloat_approxRecip32_1(a) \
  ((uint32_t)(UINT64_C(0x7FFFFFFFFFFFFFFF) / (uint32_t)(a)))

/*----------------------------------------------------------------------------
 *----------------------------------------------------------------------------*/
static struct uint128 softfloat_mul64To128(uint64_t a, uint64_t b);

/*----------------------------------------------------------------------------
 *----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------
 *----------------------------------------------------------------------------*/
static float64_t ui32_to_f64(uint32_t a) {
  uint_fast64_t uiZ;
  int_fast8_t shiftDist;

  if (!a) {
    uiZ = 0;
  } else {
    shiftDist = softfloat_countLeadingZeros32(a) + 21;
    uiZ = packToF64UI(0, 0x432 - shiftDist, (uint_fast64_t)a << shiftDist);
  }
  return float64_t::fromRaw(uiZ);
}

static float64_t ui64_to_f64(uint64_t a) {
  if (!a) {
    return float64_t::fromRaw(0);
  }
  if (a & UINT64_C(0x8000000000000000)) {
    return softfloat_roundPackToF64(0, 0x43D,
                                    softfloat_shortShiftRightJam64(a, 1));
  } else {
    return softfloat_normRoundPackToF64(0, 0x43C, a);
  }
}

static float64_t i32_to_f64(int32_t a) {
  uint_fast64_t uiZ;
  bool sign;
  uint_fast32_t absA;
  int_fast8_t shiftDist;

  if (!a) {
    uiZ = 0;
  } else {
    sign = (a < 0);
    // fixed unsigned unary minus: -x == ~x + 1
    absA = sign ? (~(uint_fast32_t)a + 1) : (uint_fast32_t)a;
    shiftDist = softfloat_countLeadingZeros32(absA) + 21;
    uiZ =
        packToF64UI(sign, 0x432 - shiftDist, (uint_fast64_t)absA << shiftDist);
  }
  return float64_t::fromRaw(uiZ);
}

static float64_t i64_to_f64(int64_t a) {
  bool sign;
  uint_fast64_t absA;

  sign = (a < 0);
  if (!(a & UINT64_C(0x7FFFFFFFFFFFFFFF))) {
    return float64_t::fromRaw(sign ? packToF64UI(1, 0x43E, 0) : 0);
  }
  // fixed unsigned unary minus: -x == ~x + 1
  absA = sign ? (~(uint_fast64_t)a + 1) : (uint_fast64_t)a;
  return softfloat_normRoundPackToF64(sign, 0x43C, absA);
}

static int_fast32_t f64_to_i32(float64_t a, uint_fast8_t roundingMode,
                               bool exact) {
  uint_fast64_t uiA;
  bool sign;
  int_fast16_t exp;
  uint_fast64_t sig;
  int_fast16_t shiftDist;

  /*------------------------------------------------------------------------
   *------------------------------------------------------------------------*/
  uiA = a.v;
  sign = signF64UI(uiA);
  exp = expF64UI(uiA);
  sig = fracF64UI(uiA);
  /*------------------------------------------------------------------------
   *------------------------------------------------------------------------*/
#if (i32_fromNaN != i32_fromPosOverflow) || (i32_fromNaN != i32_fromNegOverflow)
  if ((exp == 0x7FF) && sig) {
#if (i32_fromNaN == i32_fromPosOverflow)
    sign = 0;
#elif (i32_fromNaN == i32_fromNegOverflow)
    sign = 1;
#else
    raiseFlags(flag_invalid);
    return i32_fromNaN;
#endif
  }
#endif
  /*------------------------------------------------------------------------
   *------------------------------------------------------------------------*/
  if (exp) sig |= UINT64_C(0x0010000000000000);
  shiftDist = 0x427 - exp;
  if (0 < shiftDist) sig = softfloat_shiftRightJam64(sig, shiftDist);
  return softfloat_roundToI32(sign, sig, roundingMode, exact);
}

static int_fast64_t f64_to_i64(float64_t a, uint_fast8_t roundingMode,
                               bool exact) {
  uint_fast64_t uiA;
  bool sign;
  int_fast16_t exp;
  uint_fast64_t sig;
  int_fast16_t shiftDist;

  /*------------------------------------------------------------------------
   *------------------------------------------------------------------------*/
  uiA = a.v;
  sign = signF64UI(uiA);
  exp = expF64UI(uiA);
  sig = fracF64UI(uiA);
  /*------------------------------------------------------------------------
   *------------------------------------------------------------------------*/
#if (i64_fromNaN != i64_fromPosOverflow) || (i64_fromNaN != i64_fromNegOverflow)
  if ((exp == 0x7FF) && sig) {
#if (i64_fromNaN == i64_fromPosOverflow)
    sign = 0;
#elif (i64_fromNaN == i64_fromNegOverflow)
    sign = 1;
#else
    raiseFlags(flag_invalid);
    return i64_fromNaN;
#endif
  }
#endif
  /*------------------------------------------------------------------------
   *------------------------------------------------------------------------*/
  if (exp) sig |= UINT64_C(0x0010000000000000);
  shiftDist = 0x433 - exp;
  if (shiftDist <= 0) {
    uint_fast64_t z = sig << -shiftDist;
    if ((shiftDist < -11) || (z & UINT64_C(0x8000000000000000))) {
      raiseFlags(flag_invalid);
      return sign ? i64_fromNegOverflow : i64_fromPosOverflow;
    }
    return sign ? -(int_fast64_t)z : (int_fast64_t)z;
  } else {
    if (shiftDist < 64)
      return softfloat_roundToI64(sign, sig >> shiftDist,
                                  sig << (-shiftDist & 63), roundingMode,
                                  exact);
    else
      return softfloat_roundToI64(sign, 0, (shiftDist == 64) ? sig : (sig != 0),
                                  roundingMode, exact);
  }
}

static int_fast32_t f64_to_i32_r_minMag(float64_t a, bool exact) {
  uint_fast64_t uiA;
  int_fast16_t exp;
  uint_fast64_t sig;
  int_fast16_t shiftDist;
  bool sign;
  int_fast32_t absZ;

  /*------------------------------------------------------------------------
   *------------------------------------------------------------------------*/
  uiA = a.v;
  exp = expF64UI(uiA);
  sig = fracF64UI(uiA);
  /*------------------------------------------------------------------------
   *------------------------------------------------------------------------*/
  shiftDist = 0x433 - exp;
  if (53 <= shiftDist) {
    if (exact && (exp | sig)) {
      raiseFlags(flag_inexact);
    }
    return 0;
  }
  /*------------------------------------------------------------------------
   *------------------------------------------------------------------------*/
  sign = signF64UI(uiA);
  if (shiftDist < 22) {
    if (sign && (exp == 0x41E) && (sig < UINT64_C(0x0000000000200000))) {
      if (exact && sig) {
        raiseFlags(flag_inexact);
      }
      return -0x7FFFFFFF - 1;
    }
    raiseFlags(flag_invalid);
    return (exp == 0x7FF) && sig
               ? i32_fromNaN
               : sign ? i32_fromNegOverflow : i32_fromPosOverflow;
  }
  /*------------------------------------------------------------------------
   *------------------------------------------------------------------------*/
  sig |= UINT64_C(0x0010000000000000);
  absZ = (int_fast32_t)(sig >> shiftDist);  // fixed warning on type cast
  if (exact && ((uint_fast64_t)(uint_fast32_t)absZ << shiftDist != sig)) {
    raiseFlags(flag_inexact);
  }
  return sign ? -absZ : absZ;
}

static float64_t f64_roundToInt(float64_t a, uint_fast8_t roundingMode,
                                bool exact) {
  uint_fast64_t uiA;
  int_fast16_t exp;
  uint_fast64_t uiZ, lastBitMask, roundBitsMask;

  /*------------------------------------------------------------------------
   *------------------------------------------------------------------------*/
  uiA = a.v;
  exp = expF64UI(uiA);
  /*------------------------------------------------------------------------
   *------------------------------------------------------------------------*/
  if (exp <= 0x3FE) {
    if (!(uiA & UINT64_C(0x7FFFFFFFFFFFFFFF))) return a;
    if (exact) raiseFlags(flag_inexact);
    uiZ = uiA & packToF64UI(1, 0, 0);
    switch (roundingMode) {
      case round_near_even:
        if (!fracF64UI(uiA)) break;
        /* fallthrough */
      case round_near_maxMag:
        if (exp == 0x3FE) uiZ |= packToF64UI(0, 0x3FF, 0);
        break;
      case round_min:
        if (uiZ) uiZ = packToF64UI(1, 0x3FF, 0);
        break;
      case round_max:
        if (!uiZ) uiZ = packToF64UI(0, 0x3FF, 0);
        break;
    }
    goto uiZ;
  }
  /*------------------------------------------------------------------------
   *------------------------------------------------------------------------*/
  if (0x433 <= exp) {
    if ((exp == 0x7FF) && fracF64UI(uiA)) {
      uiZ = softfloat_propagateNaNF64UI(uiA, 0);
      goto uiZ;
    }
    return a;
  }
  /*------------------------------------------------------------------------
   *------------------------------------------------------------------------*/
  uiZ = uiA;
  lastBitMask = (uint_fast64_t)1 << (0x433 - exp);
  roundBitsMask = lastBitMask - 1;
  if (roundingMode == round_near_maxMag) {
    uiZ += lastBitMask >> 1;
  } else if (roundingMode == round_near_even) {
    uiZ += lastBitMask >> 1;
    if (!(uiZ & roundBitsMask)) uiZ &= ~lastBitMask;
  } else if (roundingMode == (signF64UI(uiZ) ? round_min : round_max)) {
    uiZ += roundBitsMask;
  }
  uiZ &= ~roundBitsMask;
  if (exact && (uiZ != uiA)) {
    raiseFlags(flag_inexact);
  }
uiZ:
  return float64_t::fromRaw(uiZ);
}

static float64_t f64_add(float64_t a, float64_t b) {
  uint_fast64_t uiA;
  bool signA;
  uint_fast64_t uiB;
  bool signB;

  uiA = a.v;
  signA = signF64UI(uiA);
  uiB = b.v;
  signB = signF64UI(uiB);
  if (signA == signB) {
    return softfloat_addMagsF64(uiA, uiB, signA);
  } else {
    return softfloat_subMagsF64(uiA, uiB, signA);
  }
}

static float64_t f64_sub(float64_t a, float64_t b) {
  uint_fast64_t uiA;
  bool signA;
  uint_fast64_t uiB;
  bool signB;

  uiA = a.v;
  signA = signF64UI(uiA);
  uiB = b.v;
  signB = signF64UI(uiB);

  if (signA == signB) {
    return softfloat_subMagsF64(uiA, uiB, signA);
  } else {
    return softfloat_addMagsF64(uiA, uiB, signA);
  }
}

static float64_t f64_mul(float64_t a, float64_t b) {
  uint_fast64_t uiA;
  bool signA;
  int_fast16_t expA;
  uint_fast64_t sigA;
  uint_fast64_t uiB;
  bool signB;
  int_fast16_t expB;
  uint_fast64_t sigB;
  bool signZ;
  uint_fast64_t magBits;
  struct exp16_sig64 normExpSig;
  int_fast16_t expZ;
  struct uint128 sig128Z;
  uint_fast64_t sigZ, uiZ;

  /*------------------------------------------------------------------------
   *------------------------------------------------------------------------*/
  uiA = a.v;
  signA = signF64UI(uiA);
  expA = expF64UI(uiA);
  sigA = fracF64UI(uiA);
  uiB = b.v;
  signB = signF64UI(uiB);
  expB = expF64UI(uiB);
  sigB = fracF64UI(uiB);
  signZ = signA ^ signB;
  /*------------------------------------------------------------------------
   *------------------------------------------------------------------------*/
  if (expA == 0x7FF) {
    if (sigA || ((expB == 0x7FF) && sigB)) goto propagateNaN;
    magBits = expB | sigB;
    goto infArg;
  }
  if (expB == 0x7FF) {
    if (sigB) goto propagateNaN;
    magBits = expA | sigA;
    goto infArg;
  }
  /*------------------------------------------------------------------------
   *------------------------------------------------------------------------*/
  if (!expA) {
    if (!sigA) goto zero;
    normExpSig = softfloat_normSubnormalF64Sig(sigA);
    expA = normExpSig.exp;
    sigA = normExpSig.sig;
  }
  if (!expB) {
    if (!sigB) goto zero;
    normExpSig = softfloat_normSubnormalF64Sig(sigB);
    expB = normExpSig.exp;
    sigB = normExpSig.sig;
  }
  /*------------------------------------------------------------------------
   *------------------------------------------------------------------------*/
  expZ = expA + expB - 0x3FF;
  sigA = (sigA | UINT64_C(0x0010000000000000)) << 10;
  sigB = (sigB | UINT64_C(0x0010000000000000)) << 11;
  sig128Z = softfloat_mul64To128(sigA, sigB);
  sigZ = sig128Z.v64 | (sig128Z.v0 != 0);

  if (sigZ < UINT64_C(0x4000000000000000)) {
    --expZ;
    sigZ <<= 1;
  }
  return softfloat_roundPackToF64(signZ, expZ, sigZ);
  /*------------------------------------------------------------------------
   *------------------------------------------------------------------------*/
propagateNaN:
  uiZ = softfloat_propagateNaNF64UI(uiA, uiB);
  goto uiZ;
  /*------------------------------------------------------------------------
   *------------------------------------------------------------------------*/
infArg:
  if (!magBits) {
    raiseFlags(flag_invalid);
    uiZ = defaultNaNF64UI;
  } else {
    uiZ = packToF64UI(signZ, 0x7FF, 0);
  }
  goto uiZ;
  /*------------------------------------------------------------------------
   *------------------------------------------------------------------------*/
zero:
  uiZ = packToF64UI(signZ, 0, 0);
uiZ:
  return float64_t::fromRaw(uiZ);
}

static float64_t f64_div(float64_t a, float64_t b) {
  uint_fast64_t uiA;
  bool signA;
  int_fast16_t expA;
  uint_fast64_t sigA;
  uint_fast64_t uiB;
  bool signB;
  int_fast16_t expB;
  uint_fast64_t sigB;
  bool signZ;
  struct exp16_sig64 normExpSig;
  int_fast16_t expZ;
  uint32_t recip32, sig32Z, doubleTerm;
  uint_fast64_t rem;
  uint32_t q;
  uint_fast64_t sigZ;
  uint_fast64_t uiZ;

  /*------------------------------------------------------------------------
   *------------------------------------------------------------------------*/
  uiA = a.v;
  signA = signF64UI(uiA);
  expA = expF64UI(uiA);
  sigA = fracF64UI(uiA);
  uiB = b.v;
  signB = signF64UI(uiB);
  expB = expF64UI(uiB);
  sigB = fracF64UI(uiB);
  signZ = signA ^ signB;
  /*------------------------------------------------------------------------
   *------------------------------------------------------------------------*/
  if (expA == 0x7FF) {
    if (sigA) goto propagateNaN;
    if (expB == 0x7FF) {
      if (sigB) goto propagateNaN;
      goto invalid;
    }
    goto infinity;
  }
  if (expB == 0x7FF) {
    if (sigB) goto propagateNaN;
    goto zero;
  }
  /*------------------------------------------------------------------------
   *------------------------------------------------------------------------*/
  if (!expB) {
    if (!sigB) {
      if (!(expA | sigA)) goto invalid;
      raiseFlags(flag_infinite);
      goto infinity;
    }
    normExpSig = softfloat_normSubnormalF64Sig(sigB);
    expB = normExpSig.exp;
    sigB = normExpSig.sig;
  }
  if (!expA) {
    if (!sigA) goto zero;
    normExpSig = softfloat_normSubnormalF64Sig(sigA);
    expA = normExpSig.exp;
    sigA = normExpSig.sig;
  }
  /*------------------------------------------------------------------------
   *------------------------------------------------------------------------*/
  expZ = expA - expB + 0x3FE;
  sigA |= UINT64_C(0x0010000000000000);
  sigB |= UINT64_C(0x0010000000000000);
  if (sigA < sigB) {
    --expZ;
    sigA <<= 11;
  } else {
    sigA <<= 10;
  }
  sigB <<= 11;
  recip32 = softfloat_approxRecip32_1(sigB >> 32) - 2;
  sig32Z = ((uint32_t)(sigA >> 32) * (uint_fast64_t)recip32) >> 32;
  doubleTerm = sig32Z << 1;
  rem = ((sigA - (uint_fast64_t)doubleTerm * (uint32_t)(sigB >> 32)) << 28) -
        (uint_fast64_t)doubleTerm * ((uint32_t)sigB >> 4);
  q = (((uint32_t)(rem >> 32) * (uint_fast64_t)recip32) >> 32) + 4;
  sigZ = ((uint_fast64_t)sig32Z << 32) + ((uint_fast64_t)q << 4);
  /*------------------------------------------------------------------------
   *------------------------------------------------------------------------*/
  if ((sigZ & 0x1FF) < 4 << 4) {
    q &= ~7;
    sigZ &= ~(uint_fast64_t)0x7F;
    doubleTerm = q << 1;
    rem = ((rem - (uint_fast64_t)doubleTerm * (uint32_t)(sigB >> 32)) << 28) -
          (uint_fast64_t)doubleTerm * ((uint32_t)sigB >> 4);
    if (rem & UINT64_C(0x8000000000000000)) {
      sigZ -= 1 << 7;
    } else {
      if (rem) sigZ |= 1;
    }
  }
  return softfloat_roundPackToF64(signZ, expZ, sigZ);
  /*------------------------------------------------------------------------
   *------------------------------------------------------------------------*/
propagateNaN:
  uiZ = softfloat_propagateNaNF64UI(uiA, uiB);
  goto uiZ;
  /*------------------------------------------------------------------------
   *------------------------------------------------------------------------*/
invalid:
  raiseFlags(flag_invalid);
  uiZ = defaultNaNF64UI;
  goto uiZ;
  /*------------------------------------------------------------------------
   *------------------------------------------------------------------------*/
infinity:
  uiZ = packToF64UI(signZ, 0x7FF, 0);
  goto uiZ;
  /*------------------------------------------------------------------------
   *------------------------------------------------------------------------*/
zero:
  uiZ = packToF64UI(signZ, 0, 0);
uiZ:
  return float64_t::fromRaw(uiZ);
}

static float64_t f64_rem(float64_t a, float64_t b) {
  uint_fast64_t uiA;
  bool signA;
  int_fast16_t expA;
  uint_fast64_t sigA;
  uint_fast64_t uiB;
  int_fast16_t expB;
  uint_fast64_t sigB;
  struct exp16_sig64 normExpSig;
  uint64_t rem;
  int_fast16_t expDiff;
  uint32_t q, recip32;
  uint_fast64_t q64;
  uint64_t altRem, meanRem;
  bool signRem;
  uint_fast64_t uiZ;

  /*------------------------------------------------------------------------
   *------------------------------------------------------------------------*/
  uiA = a.v;
  signA = signF64UI(uiA);
  expA = expF64UI(uiA);
  sigA = fracF64UI(uiA);
  uiB = b.v;
  expB = expF64UI(uiB);
  sigB = fracF64UI(uiB);
  /*------------------------------------------------------------------------
   *------------------------------------------------------------------------*/
  if (expA == 0x7FF) {
    if (sigA || ((expB == 0x7FF) && sigB)) goto propagateNaN;
    goto invalid;
  }
  if (expB == 0x7FF) {
    if (sigB) goto propagateNaN;
    return a;
  }
  /*------------------------------------------------------------------------
   *------------------------------------------------------------------------*/
  if (expA < expB - 1) return a;
  /*------------------------------------------------------------------------
   *------------------------------------------------------------------------*/
  if (!expB) {
    if (!sigB) goto invalid;
    normExpSig = softfloat_normSubnormalF64Sig(sigB);
    expB = normExpSig.exp;
    sigB = normExpSig.sig;
  }
  if (!expA) {
    if (!sigA) return a;
    normExpSig = softfloat_normSubnormalF64Sig(sigA);
    expA = normExpSig.exp;
    sigA = normExpSig.sig;
  }
  /*------------------------------------------------------------------------
   *------------------------------------------------------------------------*/
  rem = sigA | UINT64_C(0x0010000000000000);
  sigB |= UINT64_C(0x0010000000000000);
  expDiff = expA - expB;
  if (expDiff < 1) {
    if (expDiff < -1) return a;
    sigB <<= 9;
    if (expDiff) {
      rem <<= 8;
      q = 0;
    } else {
      rem <<= 9;
      q = (sigB <= rem);
      if (q) rem -= sigB;
    }
  } else {
    recip32 = softfloat_approxRecip32_1(sigB >> 21);
    rem <<= 9;
    expDiff -= 30;
    // The scale of `sigB' affects how many bits are obtained during eachcycle
    // of the loop Currently this is 29 bits per loop iteration, the maximum
    // possible
    sigB <<= 9;
    for (;;) {
      q64 = (uint32_t)(rem >> 32) * (uint_fast64_t)recip32;
      if (expDiff < 0) break;
      q = (q64 + 0x80000000) >> 32;
      rem <<= 29;
      rem -= q * (uint64_t)sigB;
      if (rem & UINT64_C(0x8000000000000000)) rem += sigB;
      expDiff -= 29;
    }
    // `expDiff` cannot be less than -29 here
    q = (uint32_t)(q64 >> 32) >> (~expDiff & 31);
    rem = (rem << (expDiff + 30)) - q * (uint64_t)sigB;
    if (rem & UINT64_C(0x8000000000000000)) {
      altRem = rem + sigB;
      goto selectRem;
    }
  }
  /*------------------------------------------------------------------------
   *------------------------------------------------------------------------*/
  do {
    altRem = rem;
    ++q;
    rem -= sigB;
  } while (!(rem & UINT64_C(0x8000000000000000)));
selectRem:
  meanRem = rem + altRem;
  if ((meanRem & UINT64_C(0x8000000000000000)) || (!meanRem && (q & 1))) {
    rem = altRem;
  }
  signRem = signA;
  if (rem & UINT64_C(0x8000000000000000)) {
    signRem = !signRem;
    // fixed unsigned unary minus: -x == ~x + 1
    rem = ~rem + 1;
  }
  return softfloat_normRoundPackToF64(signRem, expB, rem);
  /*------------------------------------------------------------------------
   *------------------------------------------------------------------------*/
propagateNaN:
  uiZ = softfloat_propagateNaNF64UI(uiA, uiB);
  goto uiZ;
invalid:
  raiseFlags(flag_invalid);
  uiZ = defaultNaNF64UI;
uiZ:
  return float64_t::fromRaw(uiZ);
}

static float64_t f64_sqrt(float64_t a) {
  uint_fast64_t uiA;
  bool signA;
  int_fast16_t expA;
  uint_fast64_t sigA, uiZ;
  struct exp16_sig64 normExpSig;
  int_fast16_t expZ;
  uint32_t sig32A, recipSqrt32, sig32Z;
  uint_fast64_t rem;
  uint32_t q;
  uint_fast64_t sigZ, shiftedSigZ;

  /*------------------------------------------------------------------------
   *------------------------------------------------------------------------*/
  uiA = a.v;
  signA = signF64UI(uiA);
  expA = expF64UI(uiA);
  sigA = fracF64UI(uiA);
  /*------------------------------------------------------------------------
   *------------------------------------------------------------------------*/
  if (expA == 0x7FF) {
    if (sigA) {
      uiZ = softfloat_propagateNaNF64UI(uiA, 0);
      goto uiZ;
    }
    if (!signA) return a;
    goto invalid;
  }
  /*------------------------------------------------------------------------
   *------------------------------------------------------------------------*/
  if (signA) {
    if (!(expA | sigA)) return a;
    goto invalid;
  }
  /*------------------------------------------------------------------------
   *------------------------------------------------------------------------*/
  if (!expA) {
    if (!sigA) return a;
    normExpSig = softfloat_normSubnormalF64Sig(sigA);
    expA = normExpSig.exp;
    sigA = normExpSig.sig;
  }
  // `sig32Z` is guaranteed to be a lower bound on the square root of `sig32A`
  // which makes `sig32Z' also a lower bound on the square root of `sigA`
  expZ = ((expA - 0x3FF) >> 1) + 0x3FE;
  expA &= 1;
  sigA |= UINT64_C(0x0010000000000000);
  sig32A = (uint32_t)(sigA >> 21);  // fixed warning on type cast
  recipSqrt32 = softfloat_approxRecipSqrt32_1(expA, sig32A);
  sig32Z = ((uint_fast64_t)sig32A * recipSqrt32) >> 32;
  if (expA) {
    sigA <<= 8;
    sig32Z >>= 1;
  } else {
    sigA <<= 9;
  }
  rem = sigA - (uint_fast64_t)sig32Z * sig32Z;
  q = ((uint32_t)(rem >> 2) * (uint_fast64_t)recipSqrt32) >> 32;
  sigZ = ((uint_fast64_t)sig32Z << 32 | 1 << 5) + ((uint_fast64_t)q << 3);
  /*------------------------------------------------------------------------
   *------------------------------------------------------------------------*/
  if ((sigZ & 0x1FF) < 0x22) {
    sigZ &= ~(uint_fast64_t)0x3F;
    shiftedSigZ = sigZ >> 6;
    rem = (sigA << 52) - shiftedSigZ * shiftedSigZ;
    if (rem & UINT64_C(0x8000000000000000)) {
      --sigZ;
    } else {
      if (rem) sigZ |= 1;
    }
  }
  return softfloat_roundPackToF64(0, expZ, sigZ);
  /*------------------------------------------------------------------------
   *------------------------------------------------------------------------*/
invalid:
  raiseFlags(flag_invalid);
  uiZ = defaultNaNF64UI;
uiZ:
  return float64_t::fromRaw(uiZ);
}

static bool f64_eq(float64_t a, float64_t b) {
  uint_fast64_t uiA;
  uint_fast64_t uiB;

  uiA = a.v;
  uiB = b.v;
  if (isNaNF64UI(uiA) || isNaNF64UI(uiB)) {
    if (softfloat_isSigNaNF64UI(uiA) || softfloat_isSigNaNF64UI(uiB))
      raiseFlags(flag_invalid);
    return false;
  }
  return (uiA == uiB) || !((uiA | uiB) & UINT64_C(0x7FFFFFFFFFFFFFFF));
}

static bool f64_le(float64_t a, float64_t b) {
  uint_fast64_t uiA;
  uint_fast64_t uiB;
  bool signA, signB;

  uiA = a.v;
  uiB = b.v;
  if (isNaNF64UI(uiA) || isNaNF64UI(uiB)) {
    raiseFlags(flag_invalid);
    return false;
  }
  signA = signF64UI(uiA);
  signB = signF64UI(uiB);
  return (signA != signB)
             ? signA || !((uiA | uiB) & UINT64_C(0x7FFFFFFFFFFFFFFF))
             : (uiA == uiB) || (signA ^ (uiA < uiB));
}

static bool f64_lt(float64_t a, float64_t b) {
  uint_fast64_t uiA;
  uint_fast64_t uiB;
  bool signA, signB;

  uiA = a.v;
  uiB = b.v;
  if (isNaNF64UI(uiA) || isNaNF64UI(uiB)) {
    raiseFlags(flag_invalid);
    return false;
  }
  signA = signF64UI(uiA);
  signB = signF64UI(uiB);
  return (signA != signB)
             ? signA && ((uiA | uiB) & UINT64_C(0x7FFFFFFFFFFFFFFF))
             : (uiA != uiB) && (signA ^ (uiA < uiB));
}

static uint_fast64_t softfloat_propagateNaNF64UI(uint_fast64_t uiA,
                                                 uint_fast64_t uiB) {
  bool isSigNaNA;

  isSigNaNA = softfloat_isSigNaNF64UI(uiA);
  if (isSigNaNA || softfloat_isSigNaNF64UI(uiB)) {
    raiseFlags(flag_invalid);
    if (isSigNaNA) return uiA | UINT64_C(0x0008000000000000);
  }
  return (isNaNF64UI(uiA) ? uiA : uiB) | UINT64_C(0x0008000000000000);
}

static float64_t softfloat_roundPackToF64(bool sign, int_fast16_t exp,
                                          uint_fast64_t sig) {
  uint_fast8_t roundingMode;
  bool roundNearEven;
  uint_fast16_t roundIncrement, roundBits;
  bool isTiny;
  uint_fast64_t uiZ;

  /*------------------------------------------------------------------------
   *------------------------------------------------------------------------*/
  roundingMode = globalRoundingMode;
  roundNearEven = (roundingMode == round_near_even);
  roundIncrement = 0x200;
  if (!roundNearEven && (roundingMode != round_near_maxMag)) {
    roundIncrement =
        (roundingMode == (sign ? round_min : round_max)) ? 0x3FF : 0;
  }
  roundBits = sig & 0x3FF;
  /*------------------------------------------------------------------------
   *------------------------------------------------------------------------*/
  if (0x7FD <= (uint16_t)exp) {
    if (exp < 0) {
      /*----------------------------------------------------------------
       *----------------------------------------------------------------*/
      isTiny = (globalDetectTininess == tininess_beforeRounding) ||
               (exp < -1) ||
               (sig + roundIncrement < UINT64_C(0x8000000000000000));
      sig = softfloat_shiftRightJam64(sig, -exp);
      exp = 0;
      roundBits = sig & 0x3FF;
      if (isTiny && roundBits) {
        raiseFlags(flag_underflow);
      }
    } else if ((0x7FD < exp) ||
               (UINT64_C(0x8000000000000000) <= sig + roundIncrement)) {
      /*----------------------------------------------------------------
       *----------------------------------------------------------------*/
      raiseFlags(flag_overflow | flag_inexact);
      uiZ = packToF64UI(sign, 0x7FF, 0) - !roundIncrement;
      goto uiZ;
    }
  }
  /*------------------------------------------------------------------------
   *------------------------------------------------------------------------*/
  sig = (sig + roundIncrement) >> 10;
  if (roundBits) {
    raiseFlags(flag_inexact);
    if (roundingMode == round_odd) {
      sig |= 1;
      goto packReturn;
    }
  }
  sig &= ~(uint_fast64_t)(!(roundBits ^ 0x200) & roundNearEven);
  if (!sig) exp = 0;
  /*------------------------------------------------------------------------
   *------------------------------------------------------------------------*/
packReturn:
  uiZ = packToF64UI(sign, exp, sig);
uiZ:
  return float64_t::fromRaw(uiZ);
}

static int_fast32_t softfloat_roundToI32(bool sign, uint_fast64_t sig,
                                         uint_fast8_t roundingMode,
                                         bool exact) {
  bool roundNearEven;
  uint_fast16_t roundIncrement, roundBits;
  uint_fast32_t sig32;
  union {
    uint32_t ui;
    int32_t i;
  } uZ;
  int_fast32_t z;

  /*------------------------------------------------------------------------
   *------------------------------------------------------------------------*/
  roundNearEven = (roundingMode == round_near_even);
  roundIncrement = 0x800;
  if (!roundNearEven && (roundingMode != round_near_maxMag)) {
    roundIncrement =
        (roundingMode == (sign ? round_min : round_max)) ? 0xFFF : 0;
  }
  roundBits = sig & 0xFFF;
  sig += roundIncrement;
  if (sig & UINT64_C(0xFFFFF00000000000)) goto invalid;
  sig32 = (uint_fast32_t)(sig >> 12);  // fixed warning on type cast
  sig32 &= ~(uint_fast32_t)(!(roundBits ^ 0x800) & roundNearEven);
  // fixed unsigned unary minus: -x == ~x + 1
  uZ.ui = sign ? (~sig32 + 1) : sig32;
  z = uZ.i;
  if (z && ((z < 0) ^ sign)) goto invalid;
  if (exact && roundBits) {
    raiseFlags(flag_inexact);
  }
  return z;
  /*------------------------------------------------------------------------
   *------------------------------------------------------------------------*/
invalid:
  raiseFlags(flag_invalid);
  return sign ? i32_fromNegOverflow : i32_fromPosOverflow;
}

static int_fast64_t softfloat_roundToI64(bool sign, uint_fast64_t sig,
                                         uint_fast64_t sigExtra,
                                         uint_fast8_t roundingMode,
                                         bool exact) {
  bool roundNearEven, doIncrement;
  union {
    uint64_t ui;
    int64_t i;
  } uZ;
  int_fast64_t z;

  /*------------------------------------------------------------------------
   *------------------------------------------------------------------------*/
  roundNearEven = (roundingMode == round_near_even);
  doIncrement = (UINT64_C(0x8000000000000000) <= sigExtra);
  if (!roundNearEven && (roundingMode != round_near_maxMag)) {
    doIncrement = (roundingMode == (sign ? round_min : round_max)) && sigExtra;
  }
  if (doIncrement) {
    ++sig;
    if (!sig) goto invalid;
    sig &= ~(uint_fast64_t)(!(sigExtra & UINT64_C(0x7FFFFFFFFFFFFFFF)) &
                            roundNearEven);
  }
  uZ.ui = sign ? (~sig + 1) : sig;
  z = uZ.i;
  if (z && ((z < 0) ^ sign)) goto invalid;
  if (exact && sigExtra) {
    raiseFlags(flag_inexact);
  }
  return z;
  /*------------------------------------------------------------------------
   *------------------------------------------------------------------------*/
invalid:
  raiseFlags(flag_invalid);
  return sign ? i64_fromNegOverflow : i64_fromPosOverflow;
}

static struct exp16_sig64 softfloat_normSubnormalF64Sig(uint_fast64_t sig) {
  int_fast8_t shiftDist;
  struct exp16_sig64 z;

  shiftDist = softfloat_countLeadingZeros64(sig) - 11;
  z.exp = 1 - shiftDist;
  z.sig = sig << shiftDist;
  return z;
}

static float64_t softfloat_normRoundPackToF64(bool sign, int_fast16_t exp,
                                              uint_fast64_t sig) {
  int_fast8_t shiftDist;

  shiftDist = softfloat_countLeadingZeros64(sig) - 1;
  exp -= shiftDist;
  if ((10 <= shiftDist) && ((unsigned int)exp < 0x7FD)) {
    return float64_t::fromRaw(
        packToF64UI(sign, sig ? exp : 0, sig << (shiftDist - 10)));
  } else {
    return softfloat_roundPackToF64(sign, exp, sig << shiftDist);
  }
}

static float64_t softfloat_addMagsF64(uint_fast64_t uiA, uint_fast64_t uiB,
                                      bool signZ) {
  int_fast16_t expA;
  uint_fast64_t sigA;
  int_fast16_t expB;
  uint_fast64_t sigB;
  int_fast16_t expDiff;
  uint_fast64_t uiZ;
  int_fast16_t expZ;
  uint_fast64_t sigZ;

  /*------------------------------------------------------------------------
   *------------------------------------------------------------------------*/
  expA = expF64UI(uiA);
  sigA = fracF64UI(uiA);
  expB = expF64UI(uiB);
  sigB = fracF64UI(uiB);
  /*------------------------------------------------------------------------
   *------------------------------------------------------------------------*/
  expDiff = expA - expB;
  if (!expDiff) {
    /*--------------------------------------------------------------------
     *--------------------------------------------------------------------*/
    if (!expA) {
      uiZ = uiA + sigB;
      goto uiZ;
    }
    if (expA == 0x7FF) {
      if (sigA | sigB) goto propagateNaN;
      uiZ = uiA;
      goto uiZ;
    }
    expZ = expA;
    sigZ = UINT64_C(0x0020000000000000) + sigA + sigB;
    sigZ <<= 9;
  } else {
    /*--------------------------------------------------------------------
     *--------------------------------------------------------------------*/
    sigA <<= 9;
    sigB <<= 9;
    if (expDiff < 0) {
      if (expB == 0x7FF) {
        if (sigB) goto propagateNaN;
        uiZ = packToF64UI(signZ, 0x7FF, 0);
        goto uiZ;
      }
      expZ = expB;
      if (expA) {
        sigA += UINT64_C(0x2000000000000000);
      } else {
        sigA <<= 1;
      }
      sigA = softfloat_shiftRightJam64(sigA, -expDiff);
    } else {
      if (expA == 0x7FF) {
        if (sigA) goto propagateNaN;
        uiZ = uiA;
        goto uiZ;
      }
      expZ = expA;
      if (expB) {
        sigB += UINT64_C(0x2000000000000000);
      } else {
        sigB <<= 1;
      }
      sigB = softfloat_shiftRightJam64(sigB, expDiff);
    }
    sigZ = UINT64_C(0x2000000000000000) + sigA + sigB;
    if (sigZ < UINT64_C(0x4000000000000000)) {
      --expZ;
      sigZ <<= 1;
    }
  }
  return softfloat_roundPackToF64(signZ, expZ, sigZ);
  /*------------------------------------------------------------------------
   *------------------------------------------------------------------------*/
propagateNaN:
  uiZ = softfloat_propagateNaNF64UI(uiA, uiB);
uiZ:
  return float64_t::fromRaw(uiZ);
}

static float64_t softfloat_subMagsF64(uint_fast64_t uiA, uint_fast64_t uiB,
                                      bool signZ) {
  int_fast16_t expA;
  uint_fast64_t sigA;
  int_fast16_t expB;
  uint_fast64_t sigB;
  int_fast16_t expDiff;
  uint_fast64_t uiZ;
  int_fast64_t sigDiff;
  int_fast8_t shiftDist;
  int_fast16_t expZ;
  uint_fast64_t sigZ;

  /*------------------------------------------------------------------------
   *------------------------------------------------------------------------*/
  expA = expF64UI(uiA);
  sigA = fracF64UI(uiA);
  expB = expF64UI(uiB);
  sigB = fracF64UI(uiB);
  /*------------------------------------------------------------------------
   *------------------------------------------------------------------------*/
  expDiff = expA - expB;
  if (!expDiff) {
    /*--------------------------------------------------------------------
     *--------------------------------------------------------------------*/
    if (expA == 0x7FF) {
      if (sigA | sigB) goto propagateNaN;
      raiseFlags(flag_invalid);
      uiZ = defaultNaNF64UI;
      goto uiZ;
    }
    sigDiff = sigA - sigB;
    if (!sigDiff) {
      uiZ = packToF64UI((globalRoundingMode == round_min), 0, 0);
      goto uiZ;
    }
    if (expA) --expA;
    if (sigDiff < 0) {
      signZ = !signZ;
      sigDiff = -sigDiff;
    }
    shiftDist = softfloat_countLeadingZeros64(sigDiff) - 11;
    expZ = expA - shiftDist;
    if (expZ < 0) {
      shiftDist = (int_fast8_t)expA;  // fixed type cast
      expZ = 0;
    }
    uiZ = packToF64UI(signZ, expZ, sigDiff << shiftDist);
    goto uiZ;
  } else {
    /*--------------------------------------------------------------------
     *--------------------------------------------------------------------*/
    sigA <<= 10;
    sigB <<= 10;
    if (expDiff < 0) {
      /*----------------------------------------------------------------
       *----------------------------------------------------------------*/
      signZ = !signZ;
      if (expB == 0x7FF) {
        if (sigB) goto propagateNaN;
        uiZ = packToF64UI(signZ, 0x7FF, 0);
        goto uiZ;
      }
      sigA += expA ? UINT64_C(0x4000000000000000) : sigA;
      sigA = softfloat_shiftRightJam64(sigA, -expDiff);
      sigB |= UINT64_C(0x4000000000000000);
      expZ = expB;
      sigZ = sigB - sigA;
    } else {
      /*----------------------------------------------------------------
       *----------------------------------------------------------------*/
      if (expA == 0x7FF) {
        if (sigA) goto propagateNaN;
        uiZ = uiA;
        goto uiZ;
      }
      sigB += expB ? UINT64_C(0x4000000000000000) : sigB;
      sigB = softfloat_shiftRightJam64(sigB, expDiff);
      sigA |= UINT64_C(0x4000000000000000);
      expZ = expA;
      sigZ = sigA - sigB;
    }
    return softfloat_normRoundPackToF64(signZ, expZ - 1, sigZ);
  }
  /*------------------------------------------------------------------------
   *------------------------------------------------------------------------*/
propagateNaN:
  uiZ = softfloat_propagateNaNF64UI(uiA, uiB);
uiZ:
  return float64_t::fromRaw(uiZ);
}

static uint32_t softfloat_approxRecipSqrt32_1(unsigned int oddExpA,
                                              uint32_t a) {
  int index;
  uint16_t eps, r0;
  uint_fast32_t ESqrR0;
  uint32_t sigma0;
  uint_fast32_t r;
  uint32_t sqrSigma0;

  index = (a >> 27 & 0xE) + oddExpA;
  eps = (uint16_t)(a >> 12);
  r0 = softfloat_approxRecipSqrt_1k0s[index] -
       ((softfloat_approxRecipSqrt_1k1s[index] * (uint_fast32_t)eps) >> 20);
  ESqrR0 = (uint_fast32_t)r0 * r0;
  if (!oddExpA) ESqrR0 <<= 1;
  sigma0 = ~(uint_fast32_t)(((uint32_t)ESqrR0 * (uint_fast64_t)a) >> 23);
  r = (uint_fast32_t)(
      ((uint_fast32_t)r0 << 16) +
      ((r0 * (uint_fast64_t)sigma0) >> 25));  // fixed warning on type cast
  sqrSigma0 = ((uint_fast64_t)sigma0 * sigma0) >> 32;
  r += ((uint32_t)((r >> 1) + (r >> 3) - ((uint_fast32_t)r0 << 14)) *
        (uint_fast64_t)sqrSigma0) >>
       48;
  if (!(r & 0x80000000)) r = 0x80000000;
  return r;
}

static uint_fast8_t softfloat_countLeadingZeros64(uint64_t a) {
  uint_fast8_t count;
  uint32_t a32;

  count = 0;
  a32 = a >> 32;
  if (!a32) {
    count = 32;
    a32 = (uint32_t)a;  // fixed warning on type cast
  }
  // From here, result is current count + count leading zeros of `a32'.
  if (a32 < 0x10000) {
    count += 16;
    a32 <<= 16;
  }
  if (a32 < 0x1000000) {
    count += 8;
    a32 <<= 8;
  }
  count += softfloat_countLeadingZeros8[a32 >> 24];
  return count;
}

static struct uint128 softfloat_mul64To128(uint64_t a, uint64_t b) {
  uint32_t a32, a0, b32, b0;
  struct uint128 z;
  uint64_t mid1, mid;

  a32 = a >> 32;
  a0 = (uint32_t)a;  // fixed warning on type cast
  b32 = b >> 32;
  b0 = (uint32_t)b;  // fixed warning on type cast
  z.v0 = (uint_fast64_t)a0 * b0;
  mid1 = (uint_fast64_t)a32 * b0;
  mid = mid1 + (uint_fast64_t)a0 * b32;
  z.v64 = (uint_fast64_t)a32 * b32;
  z.v64 += (uint_fast64_t)(mid < mid1) << 32 | mid >> 32;
  mid <<= 32;
  z.v0 += mid;
  z.v64 += (z.v0 < mid);
  return z;
}
