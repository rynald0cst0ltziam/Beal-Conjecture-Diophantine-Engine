/**
 * Precomputation of residue sets and modular powers.
 */

#include "hyper_goliath.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * Compute the z-th power residue set for a prime p.
 */
void compute_residue_mask128(uint32_t p, uint32_t z, uint64_t mask[2]) {
  mask[0] = 0;
  mask[1] = 0;

  for (uint32_t r = 0; r < p; r++) {
    uint64_t rz = powmod(r, z, p);
    set_bit128(mask, (uint32_t)rz);
  }
}

/**
 * Create and populate precomputed data for a signature.
 */
PrecomputedData *precompute_create(uint32_t x, uint32_t y, uint32_t z,
                                   uint64_t A_max, uint64_t B_max) {
  PrecomputedData *data = (PrecomputedData *)malloc(sizeof(PrecomputedData));
  if (!data) {
    fprintf(stderr, "ERROR: Failed to allocate PrecomputedData\n");
    return NULL;
  }

  data->x = x;
  data->y = y;
  data->z = z;
  data->A_max = A_max;
  data->B_max = B_max;

  /* Compute residue masks for each prime */
  for (int i = 0; i < NUM_SIEVE_PRIMES; i++) {
    uint32_t p = SIEVE_PRIMES[i];
    compute_residue_mask128(p, z, data->residue_masks[i]);
  }

  /* Allocate and compute ax_mod (A-major) */
  data->ax_mod = (uint8_t **)malloc((A_max + 1) * sizeof(uint8_t *));
  if (!data->ax_mod) {
    free(data);
    return NULL;
  }

  for (uint64_t A = 0; A <= A_max; A++) {
    data->ax_mod[A] = (uint8_t *)malloc(NUM_SIEVE_PRIMES * sizeof(uint8_t));
    for (int i = 0; i < NUM_SIEVE_PRIMES; i++) {
      data->ax_mod[A][i] = (uint8_t)powmod(A, x, SIEVE_PRIMES[i]);
    }
  }

  /* Allocate and compute by_mod (Prime-major for SIMD optimization) */
  data->by_mod = (uint8_t **)malloc(NUM_SIEVE_PRIMES * sizeof(uint8_t *));
  if (!data->by_mod) {
    /* Cleanup data->ax_mod first... */
    return NULL;
  }

  for (int i = 0; i < NUM_SIEVE_PRIMES; i++) {
    uint32_t p = SIEVE_PRIMES[i];
    data->by_mod[i] = (uint8_t *)malloc((B_max + 1) * sizeof(uint8_t));
    for (uint64_t B = 0; B <= B_max; B++) {
      data->by_mod[i][B] = (uint8_t)powmod(B, y, p);
    }
  }

  return data;
}

/**
 * Free all precomputed data.
 */
void precompute_free(PrecomputedData *data) {
  if (!data)
    return;

  if (data->ax_mod) {
    for (uint64_t A = 0; A <= data->A_max; A++) {
      free(data->ax_mod[A]);
    }
    free(data->ax_mod);
  }

  if (data->by_mod) {
    for (int i = 0; i < NUM_SIEVE_PRIMES; i++) {
      free(data->by_mod[i]);
    }
    free(data->by_mod);
  }

  free(data);
}
