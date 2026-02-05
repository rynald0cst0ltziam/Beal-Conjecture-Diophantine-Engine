/**
 * Hyper-Goliath: Main Entry Point
 *
 * C/AVX2 High-Performance Beal Conjecture Search Engine
 *
 * Usage:
 *   ./hyper_goliath --x 3 --y 4 --z 5 --Amax 300000 --Bmax 300000 --Cmax
 * 100000000
 */

#include "hyper_goliath.h"
#include <getopt.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Version info */
#define VERSION "1.0.0"

/**
 * Print usage information.
 */
static void print_usage(const char *prog) {
  printf("Hyper-Goliath v%s - High-Performance Beal Conjecture Search\n",
         VERSION);
  printf("\n");
  printf("Usage: %s [options]\n", prog);
  printf("\n");
  printf("Required:\n");
  printf("  --x <N>          Exponent x (must be > 2)\n");
  printf("  --y <N>          Exponent y (must be > 2)\n");
  printf("  --z <N>          Exponent z (must be > 2)\n");
  printf("\n");
  printf("Bounds:\n");
  printf("  --Amax <N>       Maximum A value (default: 1000)\n");
  printf("  --Bmax <N>       Maximum B value (default: 1000)\n");
  printf("  --Cmax <N>       Maximum C value (default: 10000000)\n");
  printf("  --Astart <N>     Starting A value (default: 1)\n");
  printf("  --Bstart <N>     Starting B value (default: 1)\n");
  printf("\n");
  printf("Options:\n");
  printf("  --threads <N>    Number of threads (default: auto)\n");
  printf("  --log <file>     JSONL log file path\n");
  printf("  --progress <N>   Print progress every N pairs (0=disabled)\n");
  printf("  --validate       Run self-validation tests and exit\n");
  printf("  --help           Show this help\n");
  printf("\n");
  printf("Example:\n");
  printf("  %s --x 4 --y 5 --z 6 --Amax 300000 --Bmax 300000\n", prog);
  printf("\n");
}

/**
 * Run self-validation tests.
 */
static int run_validation(void) {
  printf("Hyper-Goliath Self-Validation\n");
  printf("=============================\n\n");

  int errors = 0;

  /* Test 1: Residue mask computation */
  printf("[1] Testing residue mask computation...\n");

  /* Cubes mod 7 should be {0, 1, 6} */
  uint64_t mask_7_3[2];
  compute_residue_mask128(7, 3, mask_7_3);
  if (mask_7_3[0] != ((1ULL << 0) | (1ULL << 1) | (1ULL << 6)) ||
      mask_7_3[1] != 0) {
    printf("    FAIL: Cubes mod 7 = 0x%" PRIx64 ":%" PRIx64 "\n", mask_7_3[1],
           mask_7_3[0]);
    errors++;
  } else {
    printf("    PASS: Cubes mod 7 = {0, 1, 6}\n");
  }

  /* 5th powers mod 11 should be {0, 1, 10} */
  uint64_t mask_11_5[2];
  compute_residue_mask128(11, 5, mask_11_5);
  if (mask_11_5[0] != ((1ULL << 0) | (1ULL << 1) | (1ULL << 10)) ||
      mask_11_5[1] != 0) {
    printf("    FAIL: 5th powers mod 11 = 0x%" PRIx64 ":%" PRIx64 "\n",
           mask_11_5[1], mask_11_5[0]);
    errors++;
  } else {
    printf("    PASS: 5th powers mod 11 = {0, 1, 10}\n");
  }

  /* REGRESSION TEST: Prime > 64 (p=71, cubes mod 71, bit 70 should be set) */
  uint64_t mask_71_3[2];
  compute_residue_mask128(71, 3, mask_71_3);
  if (!get_bit128(mask_71_3, 70)) {
    printf("    FAIL: Bit 70 NOT set for cubes mod 71 (70^3 %% 71 == 70)\n");
    errors++;
  } else {
    printf("    PASS: Bit 70 set for cubes mod 71 (correct 128-bit shift)\n");
  }

  /* Test 2: GCD function */
  printf("\n[2] Testing GCD function...\n");

  struct {
    uint64_t a, b, expected;
  } gcd_tests[] = {
      {12, 8, 4}, {17, 13, 1}, {100, 25, 25}, {0, 5, 5}, {7, 0, 7}, {1, 1, 1},
  };

  for (size_t i = 0; i < sizeof(gcd_tests) / sizeof(gcd_tests[0]); i++) {
    uint64_t result = gcd64(gcd_tests[i].a, gcd_tests[i].b);
    if (result != gcd_tests[i].expected) {
      printf("    FAIL: gcd(%" PRIu64 ", %" PRIu64 ") = %" PRIu64
             ", expected %" PRIu64 "\n",
             gcd_tests[i].a, gcd_tests[i].b, result, gcd_tests[i].expected);
      errors++;
    }
  }
  if (errors == 0) {
    printf("    PASS: All GCD tests passed\n");
  }

  /* Test 3: Power modulo */
  printf("\n[3] Testing modular exponentiation...\n");

  uint64_t pm1 = powmod(2, 10, 1000); /* 1024 mod 1000 = 24 */
  uint64_t pm2 = powmod(3, 4, 7);     /* 81 mod 7 = 4 */
  uint64_t pm3 = powmod(5, 3, 13);    /* 125 mod 13 = 8 */

  if (pm1 != 24 || pm2 != 4 || pm3 != 8) {
    printf("    FAIL: powmod results incorrect\n");
    errors++;
  } else {
    printf("    PASS: Modular exponentiation correct\n");
  }

  /* Test 4: GMP verification */
  printf("\n[4] Testing GMP exact verification...\n");

  /* Known: 2^6 + 2^6 = 128 = 2^7, but gcd = 2 (not primitive) */
  uint64_t C, g;
  bool hit = check_beal_hit_gmp(2, 2, 6, 6, 7, 1000, &C, &g);
  if (!hit || C != 2 || g != 2) {
    printf("    FAIL: 2^6 + 2^6 = 2^7 not detected correctly\n");
    errors++;
  } else {
    printf("    PASS: 2^6 + 2^6 = 2^7 (gcd=2, non-primitive)\n");
  }

  /* Non-hit case: 2^3 + 3^3 = 8 + 27 = 35 (not a perfect cube) */
  hit = check_beal_hit_gmp(2, 3, 3, 3, 3, 1000, &C, &g);
  if (hit) {
    printf("    FAIL: 2^3 + 3^3 incorrectly reported as hit\n");
    errors++;
  } else {
    printf("    PASS: 2^3 + 3^3 = 35 correctly rejected (not a cube)\n");
  }

  /* Test 5: Sieve correctness on small range */
  printf("\n[5] Testing sieve on small range...\n");

  PrecomputedData *data = precompute_create(3, 4, 5, 100, 100);
  if (!data) {
    printf("    FAIL: Precomputation failed\n");
    errors++;
  } else {
    uint64_t survivor_count = count_sieve_survivors(1, 100, 1, 100, data);
    printf("    Survivors in [1,100]x[1,100] for (3,4,5): %" PRIu64 "\n",
           survivor_count);

    /* The Python engine should give the same count */
    /* For (3,4,5) with 20-prime sieve, we expect very few or zero survivors */
    if (survivor_count <= 10) { /* Reasonable upper bound */
      printf("    PASS: Survivor count is reasonable\n");
    } else {
      printf("    WARNING: Survivor count seems high\n");
    }

    precompute_free(data);
  }

  /* Summary */
  printf("\n=============================\n");
  if (errors == 0) {
    printf("All validation tests PASSED!\n");
    return 0;
  } else {
    printf("%d validation test(s) FAILED!\n", errors);
    return 1;
  }
}

/**
 * Main entry point.
 */
int main(int argc, char *argv[]) {
  /* Default parameters */
  SearchParams params = {.x = 0,
                         .y = 0,
                         .z = 0,
                         .A_start = 1,
                         .A_max = 1000,
                         .B_start = 1,
                         .B_max = 1000,
                         .C_max = 10000000,
                         .num_threads = 0,
                         .progress_interval = 0,
                         .log_path = NULL};

  int do_validate = 0;
  char *log_path_buf = NULL;

  /* Long options */
  static struct option long_options[] = {
      {"x", required_argument, 0, 'x'},
      {"y", required_argument, 0, 'y'},
      {"z", required_argument, 0, 'z'},
      {"Amax", required_argument, 0, 'A'},
      {"Bmax", required_argument, 0, 'B'},
      {"Cmax", required_argument, 0, 'C'},
      {"Astart", required_argument, 0, 'a'},
      {"Bstart", required_argument, 0, 'b'},
      {"threads", required_argument, 0, 't'},
      {"log", required_argument, 0, 'l'},
      {"progress", required_argument, 0, 'p'},
      {"validate", no_argument, 0, 'v'},
      {"help", no_argument, 0, 'h'},
      {0, 0, 0, 0}};

  int opt;
  int option_index = 0;

  while ((opt = getopt_long(argc, argv, "x:y:z:A:B:C:a:b:t:l:p:vh",
                            long_options, &option_index)) != -1) {
    switch (opt) {
    case 'x':
      params.x = atoi(optarg);
      break;
    case 'y':
      params.y = atoi(optarg);
      break;
    case 'z':
      params.z = atoi(optarg);
      break;
    case 'A':
      params.A_max = strtoull(optarg, NULL, 10);
      break;
    case 'B':
      params.B_max = strtoull(optarg, NULL, 10);
      break;
    case 'C':
      params.C_max = strtoull(optarg, NULL, 10);
      break;
    case 'a':
      params.A_start = strtoull(optarg, NULL, 10);
      break;
    case 'b':
      params.B_start = strtoull(optarg, NULL, 10);
      break;
    case 't':
      params.num_threads = atoi(optarg);
      break;
    case 'l':
      log_path_buf = strdup(optarg);
      params.log_path = log_path_buf;
      break;
    case 'p':
      params.progress_interval = atoi(optarg);
      break;
    case 'v':
      do_validate = 1;
      break;
    case 'h':
    default:
      print_usage(argv[0]);
      return (opt == 'h') ? 0 : 1;
    }
  }

  /* Validation mode */
  if (do_validate) {
    return run_validation();
  }

  /* Validate required parameters */
  if (params.x < 3 || params.y < 3 || params.z < 3) {
    fprintf(stderr, "Error: Exponents x, y, z must all be > 2\n");
    fprintf(stderr, "       (Beal Conjecture requires exponents >= 3)\n");
    print_usage(argv[0]);
    return 1;
  }

  if (params.A_start < 1 || params.B_start < 1) {
    fprintf(stderr, "Error: A_start and B_start must be >= 1\n");
    return 1;
  }

  if (params.A_max < params.A_start || params.B_max < params.B_start) {
    fprintf(stderr, "Error: Max values must be >= start values\n");
    return 1;
  }

  /* Generate default log path if not specified */
  if (!params.log_path) {
    log_path_buf = malloc(256);
    snprintf(log_path_buf, 256, "search_%u_%u_%u_%lu.jsonl", params.x, params.y,
             params.z, (unsigned long)time(NULL));
    params.log_path = log_path_buf;
  }

  /* Run the search */
  SearchResults results;
  search_parallel(&params, &results);

  /* Print log location */
  printf("\nLog file: %s\n", params.log_path);

  /* Cleanup */
  results_free(&results);
  if (log_path_buf)
    free(log_path_buf);

  /* Return 0 if no counterexamples, 42 if counterexample found */
  return results.primitive_hits > 0 ? 42 : 0;
}
