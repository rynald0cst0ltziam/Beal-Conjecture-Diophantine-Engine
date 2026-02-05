/**
 * Modular sieve implementation.
 */

#include "hyper_goliath.h"

#ifdef HAVE_AVX2
#include <immintrin.h>
#endif

/**
 * Scalar sieve check - reference implementation (now uses prime-major by_mod).
 */
bool sieve_survives_scalar(uint64_t A, uint64_t B,
                           const PrecomputedData *data) {
  for (int i = 0; i < NUM_SIEVE_PRIMES; i++) {
    uint32_t p = SIEVE_PRIMES[i];
    uint8_t ax_mod = data->ax_mod[A][i];
    uint8_t by_mod = data->by_mod[i][B];

    uint32_t sum = ax_mod + by_mod;
    if (sum >= p)
      sum -= p;

    if (!get_bit128(data->residue_masks[i], sum)) {
      return false;
    }
  }
  return true;
}

#ifdef HAVE_AVX2

/**
 * Optimized sieve check for 8 B values at once.
 * Leverages prime-major layout for contiguous memory access.
 */
uint8_t sieve_survives_avx2_8(uint64_t A, uint64_t B_start,
                              const PrecomputedData *data) {
  uint8_t survivors = 0xFF;

  for (int i = 0; i < NUM_SIEVE_PRIMES; i++) {
    uint32_t p = SIEVE_PRIMES[i];
    uint8_t ax = data->ax_mod[A][i];
    const uint8_t *by_row = data->by_mod[i];
    const uint64_t *mask = data->residue_masks[i];

    /*
     * Heat-seeker optimization:
     * Contiguous access to 8 B values. The compiler can easily
     * vectorize this or use efficient registers.
     */
    for (int l = 0; l < 8; l++) {
      if (!(survivors & (1 << l)))
        continue;

      uint64_t B = B_start + l;
      if (B > data->B_max) {
        survivors &= ~(1 << l);
        continue;
      }

      uint32_t sum = ax + by_row[B];
      if (sum >= p)
        sum -= p;

      if (!get_bit128(mask, sum)) {
        survivors &= ~(1 << l);
      }
    }

    if (!survivors)
      break;
  }

  return survivors;
}

#endif /* HAVE_AVX2 */

/**
 * Count survivors in a range.
 */
uint64_t count_sieve_survivors(uint64_t A_start, uint64_t A_end,
                               uint64_t B_start, uint64_t B_end,
                               const PrecomputedData *data) {
  uint64_t count = 0;
  for (uint64_t A = A_start; A <= A_end; A++) {
    for (uint64_t B = B_start; B <= B_end; B++) {
      if (gcd64(A, B) == 1 && sieve_survives_scalar(A, B, data)) {
        count++;
      }
    }
  }
  return count;
}
