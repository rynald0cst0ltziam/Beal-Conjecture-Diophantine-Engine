/**
 * GMP-based exact verification.
 *
 * For pairs that survive the modular sieve, we must verify exactly
 * whether A^x + B^y = C^z for some integer C.
 *
 * This uses GMP (GNU Multiple Precision Arithmetic Library) to handle
 * arbitrarily large numbers without floating-point errors.
 */

#include "hyper_goliath.h"
#include <gmp.h>

/**
 * Check if A^x + B^y is a perfect z-th power.
 *
 * If it is, and C <= C_max, returns true and sets *out_C and *out_gcd.
 *
 * This is equivalent to Python's:
 *   N = A**x + B**y
 *   root, exact = sympy.integer_nthroot(N, z)
 *   if exact and root <= C_max: ...
 */
bool check_beal_hit_gmp(uint64_t A, uint64_t B, uint32_t x, uint32_t y,
                        uint32_t z, uint64_t C_max, uint64_t *out_C,
                        uint64_t *out_gcd) {
  mpz_t ax, by, sum, root;
  mpz_inits(ax, by, sum, root, NULL);

  /* Compute A^x using GMP */
  mpz_ui_pow_ui(ax, (unsigned long)A, (unsigned long)x);

  /* Compute B^y using GMP */
  mpz_ui_pow_ui(by, (unsigned long)B, (unsigned long)y);

  /* sum = A^x + B^y */
  mpz_add(sum, ax, by);

  /* Check if sum is a perfect z-th power */
  /* mpz_root returns non-zero iff 'sum' is a perfect z-th power */
  int is_perfect = mpz_root(root, sum, (unsigned long)z);

  bool result = false;

  if (is_perfect) {
    /* Verify: We found C such that C^z = A^x + B^y */
    /* Check if C fits in uint64_t and is <= C_max */
    if (mpz_fits_ulong_p(root)) {
      uint64_t C = mpz_get_ui(root);

      if (C <= C_max && C > 0) {
        *out_C = C;

        /* Compute gcd(A, gcd(B, C)) */
        uint64_t g1 = gcd64(B, C);
        *out_gcd = gcd64(A, g1);

        result = true;
      }
    }
  }

  mpz_clears(ax, by, sum, root, NULL);
  return result;
}

/**
 * Verify a claimed solution.
 *
 * Given A, B, C, x, y, z, verify that A^x + B^y = C^z exactly.
 * This is used for self-testing.
 */
bool verify_beal_equation(uint64_t A, uint64_t B, uint64_t C, uint32_t x,
                          uint32_t y, uint32_t z) {
  mpz_t ax, by, cz, sum;
  mpz_inits(ax, by, cz, sum, NULL);

  mpz_ui_pow_ui(ax, A, x);
  mpz_ui_pow_ui(by, B, y);
  mpz_ui_pow_ui(cz, C, z);

  mpz_add(sum, ax, by);

  int result = (mpz_cmp(sum, cz) == 0);

  mpz_clears(ax, by, cz, sum, NULL);
  return result;
}
