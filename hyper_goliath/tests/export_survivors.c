/**
 * Export survivors for cross-validation with Python.
 *
 * Outputs a list of all surviving (A, B) pairs for comparison.
 */

#include "../include/hyper_goliath.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
  if (argc < 6) {
    fprintf(stderr, "Usage: %s <x> <y> <z> <A_max> <B_max>\n", argv[0]);
    fprintf(stderr, "Example: %s 4 5 6 1000 1000\n", argv[0]);
    return 1;
  }

  uint32_t x = atoi(argv[1]);
  uint32_t y = atoi(argv[2]);
  uint32_t z = atoi(argv[3]);
  uint64_t A_max = strtoull(argv[4], NULL, 10);
  uint64_t B_max = strtoull(argv[5], NULL, 10);

  /* Precompute */
  PrecomputedData *data = precompute_create(x, y, z, A_max, B_max);
  if (!data) {
    fprintf(stderr, "Precomputation failed\n");
    return 1;
  }

  /* Output header (JSON format for Python parsing) */
  printf("{\"signature\": [%u, %u, %u], \"A_max\": %lu, \"B_max\": %lu, "
         "\"survivors\": [\n",
         x, y, z, A_max, B_max);

  int first = 1;
  uint64_t count = 0;

  for (uint64_t A = 1; A <= A_max; A++) {
    for (uint64_t B = 1; B <= B_max; B++) {
      if (gcd64(A, B) > 1) {
        continue;
      }

      if (!sieve_survives_scalar(A, B, data)) {
        continue;
      }

      /* This pair survives - output it */
      if (!first)
        printf(",\n");
      printf("  [%lu, %lu]", A, B);
      first = 0;
      count++;
    }
  }

  printf("\n], \"count\": %lu}\n", count);

  precompute_free(data);
  return 0;
}
