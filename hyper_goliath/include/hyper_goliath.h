/**
 * Hyper-Goliath: High-Performance Beal Conjecture Search Engine
 *
 * C/AVX2 port of the Python Beal Residue Engine.
 * Mathematical logic is IDENTICAL to the Python implementation.
 */

#ifndef HYPER_GOLIATH_H
#define HYPER_GOLIATH_H

#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* ============================================================================
 * CONSTANTS - THE SACRED 20 PRIMES
 * These MUST match the Python implementation exactly.
 * ============================================================================
 */

#define NUM_SIEVE_PRIMES 20

static const uint8_t SIEVE_PRIMES[NUM_SIEVE_PRIMES] = {
    2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41, 43, 47, 53, 59, 61, 67, 71};

/* Maximum prime value (for array sizing) */
#define MAX_PRIME 71

/* ============================================================================
 * DATA STRUCTURES
 * ============================================================================
 */

/**
 * Precomputed residue data for a signature (x, y, z).
 * This allows O(1) lookup during the hot sieve loop.
 */
typedef struct {
  uint32_t x, y, z; /* Signature exponents */

  /* Residue sets: for each prime p, which residues are z-th powers mod p?
   * Stored as 128-bit bitmask (2x64) to support primes up to 127.
   */
  uint64_t residue_masks[NUM_SIEVE_PRIMES][2];

  /* Precomputed A^x mod p and B^y mod p for all A, B in search range.
   * ax_mod is A-major: [A][prime_idx] (efficient for fixed A)
   * by_mod is Prime-major: [prime_idx][B] (efficient for SIMD B-sweeps)
   */
  uint8_t **ax_mod; /* ax_mod[A][prime_idx] */
  uint8_t **by_mod; /* by_mod[prime_idx][B] */

  uint64_t A_max, B_max; /* Search bounds */
} PrecomputedData;

/**
 * A hit found during search (power match).
 */
typedef struct {
  uint64_t A, B, C;
  uint64_t gcd; /* gcd(A, B, C) - if 1, this is a COUNTEREXAMPLE */
  uint32_t x, y, z;
} BealHit;

/**
 * Search results and statistics.
 */
typedef struct {
  uint64_t total_pairs;    /* Total pairs tested */
  uint64_t gcd_filtered;   /* Pairs skipped due to gcd(A,B) > 1 */
  uint64_t mod_filtered;   /* Pairs killed by sieve */
  uint64_t exact_checks;   /* Pairs that survived sieve (verified with GMP) */
  uint64_t power_hits;     /* Pairs where A^x + B^y = C^z exactly */
  uint64_t primitive_hits; /* Hits where gcd(A,B,C) = 1 (COUNTEREXAMPLES!) */

  double runtime_seconds;
  double rate_pairs_per_sec;

  BealHit *hits; /* Array of hits (if any) */
  size_t hits_capacity;
  size_t hits_count;
} SearchResults;

/**
 * Search parameters.
 */
typedef struct {
  uint32_t x, y, z; /* Signature */
  uint64_t A_start, A_max;
  uint64_t B_start, B_max;
  uint64_t C_max;

  int num_threads;       /* 0 = auto-detect */
  int progress_interval; /* Print progress every N pairs (0 = disabled) */

  const char *log_path; /* Path to JSONL log file */
} SearchParams;

/* ============================================================================
 * PRECOMPUTE FUNCTIONS (precompute.c)
 * ============================================================================
 */

/**
 * Precompute all residue data for a signature.
 * This is done once at startup before the search loop.
 */
PrecomputedData *precompute_create(uint32_t x, uint32_t y, uint32_t z,
                                   uint64_t A_max, uint64_t B_max);

/**
 * Free precomputed data.
 */
void precompute_free(PrecomputedData *data);

/**
 * Compute z-th power residue set for a prime p.
 * Fills a 128-bit mask (2x64).
 */
void compute_residue_mask128(uint32_t p, uint32_t z, uint64_t mask[2]);

/* ============================================================================
 * SIEVE FUNCTIONS (sieve.c)
 * ============================================================================
 */

/**
 * Check if a pair (A, B) survives the 20-prime sieve.
 * Returns true if the pair survives (needs exact GMP verification).
 * Returns false if the pair is killed (impossibility proven by residues).
 *
 * Scalar implementation for reference/fallback.
 */
bool sieve_survives_scalar(uint64_t A, uint64_t B, const PrecomputedData *data);

/**
 * Count survivors in a range (for validation).
 */
uint64_t count_sieve_survivors(uint64_t A_start, uint64_t A_end,
                               uint64_t B_start, uint64_t B_end,
                               const PrecomputedData *data);

#ifdef HAVE_AVX2
/**
 * AVX2-optimized sieve check for 8 B values at once.
 * Returns a bitmask where bit i is set iff B_start+i survives.
 */
uint8_t sieve_survives_avx2_8(uint64_t A, uint64_t B_start,
                              const PrecomputedData *data);
#endif

/* ============================================================================
 * GMP VERIFICATION (gmp_verify.c)
 * ============================================================================
 */

/**
 * Check if A^x + B^y = C^z for some integer C with C <= C_max.
 * Uses GMP for arbitrary-precision arithmetic.
 *
 * Returns true if a power match is found.
 * If true, *out_C is set to the found C, and *out_gcd to gcd(A, B, C).
 */
bool check_beal_hit_gmp(uint64_t A, uint64_t B, uint32_t x, uint32_t y,
                        uint32_t z, uint64_t C_max, uint64_t *out_C,
                        uint64_t *out_gcd);

/**
 * Binary GCD for 64-bit integers.
 * Identical to Python's math.gcd behavior.
 */
uint64_t gcd64(uint64_t a, uint64_t b);

/* ============================================================================
 * PARALLEL SEARCH (parallel.c)
 * ============================================================================
 */

/**
 * Main search function with OpenMP parallelization.
 * This is the entry point for the exhaustive search.
 */
void search_parallel(const SearchParams *params, SearchResults *results);

/**
 * Initialize search results structure.
 */
void results_init(SearchResults *results);

/**
 * Free search results structure.
 */
void results_free(SearchResults *results);

/**
 * Add a hit to results (thread-safe).
 */
void results_add_hit(SearchResults *results, const BealHit *hit);

/* ============================================================================
 * LOGGING (logging.c)
 * ============================================================================
 */

/**
 * JSONL logging functions matching Python engine format.
 */

void log_start(const char *path, const SearchParams *params, int num_workers);
void log_checkpoint(const char *path, uint64_t run_id, uint64_t pairs_completed,
                    uint64_t pairs_expected, uint64_t gcd_skips,
                    uint64_t mod_skips, double elapsed_seconds, int chunks_done,
                    int chunks_total);
void log_complete(const char *path, uint64_t run_id, const SearchParams *params,
                  const SearchResults *results);
void log_hit(const char *path, const BealHit *hit);

/**
 * Get current UTC timestamp in ISO 8601 format.
 */
void get_timestamp_iso(char *buf, size_t len);

/* ============================================================================
 * UTILITY FUNCTIONS
 * ============================================================================
 */

/**
 * Modular exponentiation: compute base^exp mod m.
 * Uses binary exponentiation for efficiency.
 */
static inline uint64_t powmod(uint64_t base, uint32_t exp, uint64_t m) {
  uint64_t result = 1;
  base %= m;
  while (exp > 0) {
    if (exp & 1) {
      result = (result * base) % m;
    }
    exp >>= 1;
    base = (base * base) % m;
  }
  return result;
}

/**
 * Get bit from 128-bit mask.
 */
static inline bool get_bit128(const uint64_t mask[2], uint32_t bit) {
  if (bit < 64)
    return (mask[0] & (1ULL << bit)) != 0;
  return (mask[1] & (1ULL << (bit - 64))) != 0;
}

/**
 * Set bit in 128-bit mask.
 */
static inline void set_bit128(uint64_t mask[2], uint32_t bit) {
  if (bit < 64)
    mask[0] |= (1ULL << bit);
  else
    mask[1] |= (1ULL << (bit - 64));
}

#endif /* HYPER_GOLIATH_H */
