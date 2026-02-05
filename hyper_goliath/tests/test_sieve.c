/**
 * Sieve Validation Test
 *
 * Validates that the C sieve produces identical results to Python.
 */

#include "../include/hyper_goliath.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
  printf("Sieve Validation Test\n");
  printf("=====================\n\n");

  /* Test parameters - should match Python cross-validation */
  uint32_t x = 4, y = 5, z = 6;
  uint64_t A_max = 1000, B_max = 1000;

  if (argc > 3) {
    x = atoi(argv[1]);
    y = atoi(argv[2]);
    z = atoi(argv[3]);
  }
  if (argc > 5) {
    A_max = strtoull(argv[4], NULL, 10);
    B_max = strtoull(argv[5], NULL, 10);
  }

  printf("Signature: (%u, %u, %u)\n", x, y, z);
  printf("Range: A,B <= %" PRIu64 "\n\n", A_max);

  /* Precompute */
  printf("Precomputing...\n");
  PrecomputedData *data = precompute_create(x, y, z, A_max, B_max);
  if (!data) {
    fprintf(stderr, "Precomputation failed\n");
    return 1;
  }

  /* Print residue masks for verification */
  printf("\nResidue masks (z-th powers mod p):\n");
  for (int i = 0; i < NUM_SIEVE_PRIMES; i++) {
    uint32_t p = SIEVE_PRIMES[i];

    printf("  p=%2u: {", p);
    int first = 1;
    for (uint32_t r = 0; r < p; r++) {
      if (get_bit128(data->residue_masks[i], r)) {
        if (!first)
          printf(",");
        printf("%u", r);
        first = 0;
      }
    }
    printf("}\n");
  }

  /* Count survivors */
  printf("\nCounting survivors...\n");

  uint64_t tested = 0;
  uint64_t gcd_filtered = 0;
  uint64_t sieve_filtered = 0;
  uint64_t survivors = 0;

  for (uint64_t A = 1; A <= A_max; A++) {
    for (uint64_t B = 1; B <= B_max; B++) {
      tested++;

      if (gcd64(A, B) > 1) {
        gcd_filtered++;
        continue;
      }

      if (!sieve_survives_scalar(A, B, data)) {
        sieve_filtered++;
        continue;
      }

      survivors++;

      /* Print first few survivors for verification */
      if (survivors <= 10) {
        printf("  Survivor: A=%" PRIu64 ", B=%" PRIu64 "\n", A, B);
      }
    }
  }

  printf("\nResults:\n");
  printf("  Total pairs:     %" PRIu64 "\n", tested);
  printf("  GCD filtered:    %" PRIu64 "\n", gcd_filtered);
  printf("  Sieve filtered:  %" PRIu64 "\n", sieve_filtered);
  printf("  Survivors:       %" PRIu64 "\n", survivors);

  printf("\n== CROSS-VALIDATION OUTPUT ==\n");
  printf("signature=%u_%u_%u\n", x, y, z);
  printf("A_max=%" PRIu64 "\n", A_max);
  printf("B_max=%" PRIu64 "\n", B_max);
  printf("survivors=%" PRIu64 "\n", survivors);
  printf("gcd_filtered=%" PRIu64 "\n", gcd_filtered);
  printf("sieve_filtered=%" PRIu64 "\n", sieve_filtered);

  precompute_free(data);

  printf("\nTest complete.\n");
  return 0;
}
