/**
 * Utility functions for Hyper-Goliath.
 */

#include "hyper_goliath.h"

/**
 * Binary GCD for 64-bit integers.
 *
 * This is a fast implementation using bit operations.
 * Equivalent to Python's math.gcd().
 */
uint64_t gcd64(uint64_t a, uint64_t b) {
  if (a == 0)
    return b;
  if (b == 0)
    return a;

  /* Find common factors of 2 */
  int shift = __builtin_ctzll(a | b);

  /* Divide a by its factors of 2 */
  a >>= __builtin_ctzll(a);

  do {
    /* Divide b by its factors of 2 */
    b >>= __builtin_ctzll(b);

    /* Ensure a <= b */
    if (a > b) {
      uint64_t t = a;
      a = b;
      b = t;
    }

    /* Subtract (b is now >= a) */
    b -= a;
  } while (b);

  return a << shift;
}
