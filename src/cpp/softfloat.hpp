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

#pragma once

#include <cstdint>

enum {
  tininess_beforeRounding = 0,
  tininess_afterRounding = 1
};
extern uint_fast8_t globalDetectTininess;

enum {
  round_near_even = 0,  // round to nearest, with ties to even
  round_minMag = 1,     // round to minimum magnitude (toward zero)
  round_min = 2,        // round to minimum (down)
  round_max = 3,        // round to maximum (up)
  round_near_maxMag =
      4,  // round to nearest, with ties to maximum magnitude (away from zero)
  round_odd = 5  // round to odd (jamming)
};
extern uint_fast8_t globalRoundingMode;

// For faster memory accessing
typedef union suf64 {
  int64_t i;
  uint64_t u;
  double f;
} suf64;

struct softdouble {
 public:
  // constructors
  softdouble() { v = 0; }
  softdouble(const softdouble& s) { v = s.v; }
  softdouble& operator=(const softdouble& s) {
    if (&s != this) v = s.v;
    return *this;
  }

  // Construct from raw binary representation
  static const softdouble fromRaw(const uint64_t a) {
    softdouble x;
    x.v = a;
    return x;
  }

  // Construct from integer
  explicit softdouble(const uint32_t);
  explicit softdouble(const uint64_t);
  explicit softdouble(const int32_t);
  explicit softdouble(const int64_t);

  // Construct from double
  explicit softdouble(const double a) {
    suf64 s;
    s.f = a;
    v = s.u;
  }

  // Type cast
  operator double() const {
    suf64 s;
    s.u = v;
    return s.f;
  }

  // Basic arithmetics
  softdouble operator+(const softdouble&) const;
  softdouble operator-(const softdouble&) const;
  softdouble operator*(const softdouble&) const;
  softdouble operator/(const softdouble&) const;
  softdouble operator%(const softdouble&) const;
  softdouble operator-() const {
    softdouble x;
    x.v = v ^ (1ULL << 63);
    return x;
  }

  softdouble& operator+=(const softdouble& a) {
    *this = *this + a;
    return *this;
  }
  softdouble& operator-=(const softdouble& a) {
    *this = *this - a;
    return *this;
  }
  softdouble& operator*=(const softdouble& a) {
    *this = *this * a;
    return *this;
  }
  softdouble& operator/=(const softdouble& a) {
    *this = *this / a;
    return *this;
  }
  softdouble& operator%=(const softdouble& a) {
    *this = *this % a;
    return *this;
  }

  // Comparison operations
  // Any operation with NaN produces false. The only exception is when x is NaN:
  // x != y for any y. Positive and negative zeros are equal.
  bool operator==(const softdouble&) const;
  bool operator!=(const softdouble&) const;
  bool operator>(const softdouble&) const;
  bool operator>=(const softdouble&) const;
  bool operator<(const softdouble&) const;
  bool operator<=(const softdouble&) const;

  // Indicators
  inline bool isNaN() const {
    return (v & 0x7fffffffffffffff) > 0x7ff0000000000000;
  }
  inline bool isInf() const {
    return (v & 0x7fffffffffffffff) == 0x7ff0000000000000;
  }
  inline bool isSubnormal() const { return ((v >> 52) & 0x7FF) == 0; }

  // Sign bit
  inline bool getSign() const { return (v >> 63) != 0; }
  inline softdouble setSign(bool sign) const {
    softdouble x;
    x.v = (v & ((1ULL << 63) - 1)) | ((uint_fast64_t)(sign) << 63);
    return x;
  }

  // 0-based exponent
  inline int getExp() const { return ((v >> 52) & 0x7FF) - 1023; }
  inline softdouble setExp(int e) const {
    softdouble x;
    x.v =
        (v & 0x800FFFFFFFFFFFFF) | ((uint_fast64_t)((e + 1023) & 0x7FF) << 52);
    return x;
  }

  // Fraction part
  inline softdouble getFrac() const {
    uint_fast64_t vv = (v & 0x000FFFFFFFFFFFFF) | ((uint_fast64_t)(1023) << 52);
    return softdouble::fromRaw(vv);
  }
  inline softdouble setFrac(const softdouble& s) const {
    softdouble x;
    x.v = (v & 0xFFF0000000000000) | (s.v & 0x000FFFFFFFFFFFFF);
    return x;
  }

  // Default constants
  static softdouble zero() { return softdouble::fromRaw(0); }
  static softdouble inf() {
    return softdouble::fromRaw((uint_fast64_t)(0x7FF) << 52);
  }
  static softdouble nan() {
    return softdouble::fromRaw((uint_fast64_t)(0x7FFFFFFFFFFFFFFF));
  }
  static softdouble one() {
    return softdouble::fromRaw((uint_fast64_t)(1023) << 52);
  }
  static softdouble min() {
    return softdouble::fromRaw((uint_fast64_t)(0x01) << 52);
  }
  static softdouble max() {
    return softdouble::fromRaw(((uint_fast64_t)(0x7FF) << 52) - 1);
  }
  static softdouble eps() {
    return softdouble::fromRaw((uint_fast64_t)(1023 - 52) << 52);
  }

  uint64_t v;
};

// Arithmetics
softdouble sqrt(const softdouble& a);

// Other functions
inline softdouble min(const softdouble& a, const softdouble& b) {
  return (a > b) ? b : a;
}
inline softdouble max(const softdouble& a, const softdouble& b) {
  return (a > b) ? a : b;
}
inline softdouble abs(softdouble a) {
  softdouble x;
  x.v = a.v & ((1ULL << 63) - 1);
  return x;
}
