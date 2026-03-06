#ifndef RX_VEC_I128_H
#define RX_VEC_I128_H

#include <stdint.h>
#include <string.h>

typedef union {
    uint8_t  u8[16];
    uint32_t u32[4];
    uint64_t u64[2];
    int32_t  i32[4];
    double   f64[2];
} rx_vec_i128;

typedef rx_vec_i128 rx_vec_f128;

static inline rx_vec_i128 rx_xor_vec_i128(rx_vec_i128 a, rx_vec_i128 b) {
    rx_vec_i128 r;
    r.u64[0] = a.u64[0] ^ b.u64[0];
    r.u64[1] = a.u64[1] ^ b.u64[1];
    return r;
}

static inline rx_vec_i128 rx_load_vec_i128(const void* addr) {
    rx_vec_i128 r;
    memcpy(&r, addr, 16);
    return r;
}

static inline void rx_store_vec_i128(void* addr, rx_vec_i128 a) {
    memcpy(addr, &a, 16);
}

#endif /* RX_VEC_I128_H */
