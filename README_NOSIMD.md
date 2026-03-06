# WebRandomX — PoC no-SIMD build

## Quick start

Prerequisites: `emcc, cmake, make`

```shell
make -f Makefile.noscimd
```

Or manually:

```shell
mkdir build-noscimd && cd build-noscimd
emcmake cmake -DARCH=native -DRANDOMX_NO_SIMD=ON ..
make
```

This produces a WebAssembly binary without SIMD128, bulk-memory, sign-extension, or non-trapping float-to-int instructions. The output is functionally equivalent (bit-identical hashes) to the standard SIMD build.

## Test: SIMD vs no-SIMD comparison

To verify functional equivalence and measure performance:

```shell
./test_simd_comparison.sh [--nonces N]   # default: 100
```

The script builds both variants (with tests and benchmarks enabled) and runs:

1. **WASM feature audit** — counts WASM extension instructions via `wasm-objdump`
2. **Functional correctness** — runs the test suite (known hash vectors) on both builds
3. **Hash equivalence** — compares test outputs between SIMD and no-SIMD
4. **Benchmark** — measures ms/hash on both builds and computes the slowdown factor

---

## Why a no-SIMD build?

### The problem: WASM extensions limit portability

Modern WebAssembly toolchains (Emscripten >= 3.x) emit binaries that use **non-baseline WASM extensions** enabled by default:

| Extension                 | Emscripten flag | Instructions emitted                    |
| ------------------------- | --------------- | --------------------------------------- |
| SIMD128                   | `-msimd128`     | `v128.*`, `i32x4.*`, `f64x2.*`, etc.    |
| Bulk memory               | (default ON)    | `memory.copy`, `memory.fill`            |
| Sign extension            | (default ON)    | `i32.extend8_s`, `i64.extend32_s`, etc. |
| Non-trapping float-to-int | (default ON)    | `i32.trunc_sat_*`, `i64.trunc_sat_*`    |

WebRandomX explicitly uses SIMD128 for its Argon2 memory-hard function (`argon2_simd.c`, `blamka-round-simd.h`) and AES emulation (`intrin_wasm.h`). The other three extensions are injected implicitly by the compiler backend.

These extensions are not supported in several environments:

- **Dynamic analysis frameworks**
- **Lightweight/IoT runtimes**
- **Older browsers and embedded WebView**

This PoC explores whether WebRandomX can be compiled using only baseline WASM instructions, making it portable to the widest possible range of environments.

### Functional equivalence guarantee

The no-SIMD build replaces SIMD intrinsics with **semantically identical scalar operations**. The 128-bit `v128_t` type is replaced by a union of scalar fields (`uint64_t u64[2]`, `uint32_t u32[4]`, `double f64[2]`), and each SIMD intrinsic is replaced by equivalent element-wise operations.

This is verified empirically: given identical inputs, both builds produce **bit-identical hash outputs** across the entire RandomX test vector suite.

---

## Implementation details

### Build system (`CMakeLists.txt`)

A CMake option `RANDOMX_NO_SIMD` (default OFF) controls the build variant:

```cmake
option(RANDOMX_NO_SIMD "Build without WASM SIMD128 instructions" OFF)

if(NOT RANDOMX_NO_SIMD)
  set_source_files_properties(${randomx_sources} COMPILE_FLAGS -msimd128)
else()
  add_definitions(-DRANDOMX_NO_SIMD)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mno-bulk-memory -mno-sign-ext -mno-nontrapping-fptoint")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mno-bulk-memory -mno-sign-ext -mno-nontrapping-fptoint")
endif()
```
