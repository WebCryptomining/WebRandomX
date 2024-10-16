#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>

#include "randomx.h"
#include "web_randomx.h"

#define HASH_SIZE 32

typedef struct rx_state {
  char rs_hash[HASH_SIZE];
  uint64_t  rs_height;
  randomx_cache *rs_cache;
} rx_state;

static rx_state rx_s[2] = {{{0},0,0},{{0},0,0}};

static randomx_dataset *rx_dataset;
static uint64_t rx_dataset_height;
static randomx_vm *rx_vm = NULL;

static inline int disabled_flags(void) {
  static int flags = -1;

  if (flags != -1) {
    return flags;
  }

  const char *env = getenv("MONERO_RANDOMX_UMASK");
  if (!env) {
    flags = 0;
  } else {
    char* endptr;
    long int value = strtol(env, &endptr, 0);
    if (endptr != env && value >= 0 && value < INT_MAX) {
      flags = value;
    } else {
      flags = 0;
    }
  }

  return flags;
}

static inline int enabled_flags(void) {
  static int flags = -1;

  if (flags != -1) {
    return flags;
  }

  flags = randomx_get_flags();

  return flags;
}

#define SEEDHASH_EPOCH_BLOCKS	2048
#define SEEDHASH_EPOCH_LAG		64

uint64_t rx_seedheight(const uint64_t height) {
  return (height <= SEEDHASH_EPOCH_BLOCKS+SEEDHASH_EPOCH_LAG) ? 0 : (height - SEEDHASH_EPOCH_LAG - 1) & ~(SEEDHASH_EPOCH_BLOCKS-1);
}

typedef struct seedinfo {
  randomx_cache *si_cache;
  unsigned long si_start;
  unsigned long si_count;
} seedinfo;

EM_PORT_API(int) randomx_hash(const uint64_t mainheight, const uint64_t seedheight, const char *seedhash, const void *data, size_t length,
  char *hash) {
  uint64_t s_height = rx_seedheight(mainheight);
  int toggle = (s_height & SEEDHASH_EPOCH_BLOCKS) != 0;
  randomx_flags flags = randomx_flags(enabled_flags() & ~disabled_flags());
  rx_state *rx_sp;
  randomx_cache *cache;

  // RPC could request an earlier block on mainchain
  if (s_height != seedheight) {
    toggle ^= 1;
  }

  rx_sp = &rx_s[toggle];

  cache = rx_sp->rs_cache;
  if (cache == NULL) {
    if (cache == NULL) {
      cache = randomx_alloc_cache(flags | RANDOMX_FLAG_LARGE_PAGES);
      if (cache == NULL) {
        cache = randomx_alloc_cache(flags);
      }
      if (cache == NULL) {
        _exit(1);
      }
    }
  }
  if (rx_sp->rs_height != seedheight || rx_sp->rs_cache == NULL || memcmp(seedhash, rx_sp->rs_hash, HASH_SIZE)) {
    randomx_init_cache(cache, seedhash, HASH_SIZE);
    rx_sp->rs_cache = cache;
    rx_sp->rs_height = seedheight;
    memcpy(rx_sp->rs_hash, seedhash, HASH_SIZE);
  }
  if (rx_vm == NULL) {
      rx_vm = randomx_create_vm(flags, rx_sp->rs_cache, rx_dataset);
    if(rx_vm == NULL) {
      flags = RANDOMX_FLAG_DEFAULT;
      rx_vm = randomx_create_vm(flags, rx_sp->rs_cache, rx_dataset);
    }
    if (rx_vm == NULL) {
       _exit(1);
    }
  } else {
    randomx_vm_set_cache(rx_vm, rx_sp->rs_cache);
  }
  return randomx_calculate_hash(rx_vm, data, length, hash);
}