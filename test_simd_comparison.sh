#!/usr/bin/env bash
#
# test_simd_comparison.sh — Build & compare SIMD vs no-SIMD WebRandomX
#
# Produces:
#   1. WASM feature audit (SIMD/bulk-memory/sign-ext instruction counts)
#   2. Functional correctness: both builds must pass identical hash tests
#   3. Hash equivalence check
#   4. Benchmark: ms/hash comparison (N configurable)
#
# Usage:
#   ./test_simd_comparison.sh [--nonces N]   (default: 100)

set -euo pipefail
cd "$(dirname "$0")"

NONCES=100
while [[ $# -gt 0 ]]; do
  case "$1" in
    --nonces) NONCES="$2"; shift 2 ;;
    *) echo "Unknown option: $1"; exit 1 ;;
  esac
done

BUILD_SIMD="build-simd-test"
BUILD_NOSCIMD="build-noscimd-test"

SEP="$(printf '=%.0s' {1..70})"

log()  { echo -e "\n${SEP}\n  $1\n${SEP}"; }
pass() { echo "  [PASS] $1"; }
fail() { echo "  [FAIL] $1"; FAILURES=$((FAILURES+1)); }

FAILURES=0

# ─────────────────────────────────────────────────────────────────────
# 1. Build both variants (with tests + benchmark)
# ─────────────────────────────────────────────────────────────────────
log "Building SIMD variant"
rm -rf "$BUILD_SIMD"
mkdir -p "$BUILD_SIMD" && cd "$BUILD_SIMD"
emcmake cmake -DARCH=native -DTESTS=true .. > /dev/null 2>&1
make -j$(nproc 2>/dev/null || sysctl -n hw.ncpu) > /dev/null 2>&1
cd ..

log "Building no-SIMD variant"
rm -rf "$BUILD_NOSCIMD"
mkdir -p "$BUILD_NOSCIMD" && cd "$BUILD_NOSCIMD"
emcmake cmake -DARCH=native -DRANDOMX_NO_SIMD=ON -DTESTS=true .. > /dev/null 2>&1
make -j$(nproc 2>/dev/null || sysctl -n hw.ncpu) > /dev/null 2>&1
cd ..

# ─────────────────────────────────────────────────────────────────────
# 2. WASM feature audit
# ─────────────────────────────────────────────────────────────────────
log "WASM feature audit"

count_instructions() {
  local wasm_file="$1"
  local pattern="$2"
  wasm-objdump -d "$wasm_file" 2>/dev/null | grep -ci "$pattern" || echo 0
}

echo ""
printf "  %-20s %10s %10s\n" "Feature" "SIMD build" "no-SIMD"
printf "  %-20s %10s %10s\n" "-------" "----------" "-------"

for feature_label_pattern in \
  "SIMD128:v128\|i8x16\|i16x8\|i32x4\|i64x2\|f32x4\|f64x2" \
  "bulk-memory:memory.copy\|memory.fill\|memory.init\|data.drop" \
  "sign-ext:i32.extend8_s\|i32.extend16_s\|i64.extend8_s\|i64.extend16_s\|i64.extend32_s" \
  "nontrapping-fptoint:i32.trunc_sat\|i64.trunc_sat"; do

  label="${feature_label_pattern%%:*}"
  pattern="${feature_label_pattern#*:}"
  simd_count=$(count_instructions "$BUILD_SIMD/web-randomx-tests.wasm" "$pattern" | tr -d '[:space:]')
  noscimd_count=$(count_instructions "$BUILD_NOSCIMD/web-randomx-tests.wasm" "$pattern" | tr -d '[:space:]')
  printf "  %-20s %10s %10s\n" "$label" "$simd_count" "$noscimd_count"

  if [[ "$noscimd_count" -eq 0 ]]; then
    pass "$label absent in no-SIMD build"
  else
    fail "$label found $noscimd_count instructions in no-SIMD build"
  fi
done

# ─────────────────────────────────────────────────────────────────────
# 3. Functional correctness tests
# ─────────────────────────────────────────────────────────────────────
log "Functional correctness: SIMD"
SIMD_TEST_OUT=$(node "$BUILD_SIMD/web-randomx-tests.js" 2>&1) || true
if echo "$SIMD_TEST_OUT" | grep -q "All tests PASSED"; then
  pass "SIMD tests passed"
else
  fail "SIMD tests failed"
fi
echo "$SIMD_TEST_OUT" | head -20

echo ""
log "Functional correctness: no-SIMD"
NOSCIMD_TEST_OUT=$(node "$BUILD_NOSCIMD/web-randomx-tests.js" 2>&1) || true
if echo "$NOSCIMD_TEST_OUT" | grep -q "All tests PASSED"; then
  pass "no-SIMD tests passed"
else
  fail "no-SIMD tests failed"
fi
echo "$NOSCIMD_TEST_OUT" | head -20

# ─────────────────────────────────────────────────────────────────────
# 4. Hash equivalence — extract hash lines and compare
# ─────────────────────────────────────────────────────────────────────
log "Hash equivalence check"

# Extract PASSED test names from both outputs and compare
SIMD_PASSED=$(echo "$SIMD_TEST_OUT" | grep "PASSED" | sort)
NOSCIMD_PASSED=$(echo "$NOSCIMD_TEST_OUT" | grep "PASSED" | sort)

if [[ "$SIMD_PASSED" == "$NOSCIMD_PASSED" ]]; then
  pass "Identical test results between SIMD and no-SIMD"
else
  fail "Test results differ"
  diff <(echo "$SIMD_PASSED") <(echo "$NOSCIMD_PASSED") || true
fi

# ─────────────────────────────────────────────────────────────────────
# 5. Benchmark
# ─────────────────────────────────────────────────────────────────────
log "Benchmark: $NONCES nonces"

echo "  Running SIMD benchmark..."
SIMD_BENCH_OUT=$(node "$BUILD_SIMD/web-randomx-benchmark.js" --nonces "$NONCES" 2>&1)
SIMD_PERF=$(echo "$SIMD_BENCH_OUT" | grep "Performance:" | grep -oE '[0-9]+(\.[0-9]+)? ms per hash')
SIMD_RESULT=$(echo "$SIMD_BENCH_OUT" | grep "Calculated result:")

echo "  Running no-SIMD benchmark..."
NOSCIMD_BENCH_OUT=$(node "$BUILD_NOSCIMD/web-randomx-benchmark.js" --nonces "$NONCES" 2>&1)
NOSCIMD_PERF=$(echo "$NOSCIMD_BENCH_OUT" | grep "Performance:" | grep -oE '[0-9]+(\.[0-9]+)? ms per hash')
NOSCIMD_RESULT=$(echo "$NOSCIMD_BENCH_OUT" | grep "Calculated result:")

echo ""
printf "  %-12s %s\n" "SIMD:" "$SIMD_PERF"
printf "  %-12s %s\n" "no-SIMD:" "$NOSCIMD_PERF"

echo ""
echo "  SIMD hash:    $SIMD_RESULT"
echo "  no-SIMD hash: $NOSCIMD_RESULT"

if [[ "$SIMD_RESULT" == "$NOSCIMD_RESULT" ]]; then
  pass "Benchmark hash output identical"
else
  fail "Benchmark hash output differs (functional divergence!)"
fi

# Extract numeric ms values for ratio
SIMD_MS=$(echo "$SIMD_PERF" | grep -oE '[0-9]+(\.[0-9]+)?')
NOSCIMD_MS=$(echo "$NOSCIMD_PERF" | grep -oE '[0-9]+(\.[0-9]+)?')

if command -v bc &>/dev/null && [[ -n "$SIMD_MS" ]] && [[ -n "$NOSCIMD_MS" ]]; then
  RATIO=$(echo "scale=2; $NOSCIMD_MS / $SIMD_MS" | bc)
  echo ""
  echo "  Slowdown factor (no-SIMD / SIMD): ${RATIO}x"
fi

# ─────────────────────────────────────────────────────────────────────
# Summary
# ─────────────────────────────────────────────────────────────────────
log "Summary"
if [[ $FAILURES -eq 0 ]]; then
  echo "  All checks passed."
else
  echo "  $FAILURES check(s) FAILED."
  exit 1
fi
