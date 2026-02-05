/**
 * Parallel search with OpenMP.
 */

#include "hyper_goliath.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <stdatomic.h>

#ifdef _OPENMP
#include <omp.h>
#endif

/**
 * Initialize search results structure.
 */
void results_init(SearchResults *results) {
  memset(results, 0, sizeof(SearchResults));

  /* Pre-allocate space for hits */
  results->hits_capacity = 64;
  results->hits = (BealHit *)malloc(results->hits_capacity * sizeof(BealHit));
  results->hits_count = 0;
}

/**
 * Free search results structure.
 */
void results_free(SearchResults *results) {
  if (results->hits) {
    free(results->hits);
    results->hits = NULL;
  }
}

/**
 * Add a hit to results.
 * Thread-safe when called within an OpenMP critical section.
 */
void results_add_hit(SearchResults *results, const BealHit *hit) {
  if (results->hits_count >= results->hits_capacity) {
    results->hits_capacity *= 2;
    results->hits = (BealHit *)realloc(results->hits, results->hits_capacity *
                                                          sizeof(BealHit));
  }

  results->hits[results->hits_count++] = *hit;
}

/**
 * Main parallel search function.
 */
void search_parallel(const SearchParams *params, SearchResults *results) {
  results_init(results);

  /* Determine number of threads */
  int num_threads = params->num_threads;
#ifdef _OPENMP
  if (num_threads <= 0) {
    num_threads = omp_get_max_threads();
  }
  omp_set_num_threads(num_threads);
#else
  num_threads = 1;
#endif

  printf("Hyper-Goliath Search Engine\n");
  printf("===========================\n");
  printf("Signature: (%u, %u, %u)\n", params->x, params->y, params->z);
  printf("Range: A[%" PRIu64 "-%" PRIu64 "] B[%" PRIu64 "-%" PRIu64
         "] C_max=%" PRIu64 "\n",
         params->A_start, params->A_max, params->B_start, params->B_max,
         params->C_max);
  printf("Threads: %d\n", num_threads);
  printf("\n");

  /* Precompute residue data */
  printf("Precomputing residue tables...\n");
  clock_t precompute_start = clock();

  PrecomputedData *data = precompute_create(params->x, params->y, params->z,
                                            params->A_max, params->B_max);
  if (!data) {
    fprintf(stderr, "ERROR: Precomputation failed\n");
    return;
  }

  double precompute_time =
      (double)(clock() - precompute_start) / CLOCKS_PER_SEC;
  printf("Precomputation complete (%.2f seconds)\n\n", precompute_time);

  /* Log start */
  uint64_t run_id = (uint64_t)time(NULL);
  log_start(params->log_path, params, num_threads);

  uint64_t A_start = params->A_start;
  uint64_t A_max = params->A_max;
  uint64_t B_start = params->B_start;
  uint64_t B_max = params->B_max;
  uint64_t C_max = params->C_max;

  uint64_t expected_pairs = (A_max - A_start + 1) * (B_max - B_start + 1);
  printf("Starting search (%" PRIu64 " pairs)...\n", expected_pairs);

/* Timing */
#ifdef _OPENMP
  double start_time = omp_get_wtime();
  double last_report_time = start_time;
#else
  clock_t start_time = clock();
  double last_report_time = (double)start_time / CLOCKS_PER_SEC;
#endif

  /* Global counters for live UI. We use atomic to avoid reduction silos. */
  _Atomic uint64_t global_tested = 0;
  _Atomic uint64_t global_gcd_skips = 0;
  _Atomic uint64_t global_mod_skips = 0;
  _Atomic uint64_t global_exact_checks = 0;

/* Parallel search loop */
#ifdef _OPENMP
#pragma omp parallel
  {
#endif
    /* Thread-local hit buffer */
    BealHit local_hits[64];
    int local_hit_count = 0;

#ifdef _OPENMP
#pragma omp for schedule(dynamic, 1)
#endif
    for (uint64_t A = A_start; A <= A_max; A++) {
      uint64_t a_tested = 0;
      uint64_t a_gcd = 0;
      uint64_t a_mod = 0;
      uint64_t a_exact = 0;

#ifdef HAVE_AVX2
      for (uint64_t B = B_start; B <= B_max; B += 8) {
        uint8_t survivors = sieve_survives_avx2_8(A, B, data);

        for (int lane = 0; lane < 8 && B + lane <= B_max; lane++) {
          uint64_t B_val = B + lane;
          a_tested++;

          if (gcd64(A, B_val) > 1) {
            a_gcd++;
            continue;
          }

          if (!(survivors & (1 << lane))) {
            a_mod++;
            continue;
          }

          a_exact++;
          uint64_t C, g;
          if (check_beal_hit_gmp(A, B_val, params->x, params->y, params->z,
                                 C_max, &C, &g)) {
            BealHit hit = {A, B_val, C, g, params->x, params->y, params->z};
            if (local_hit_count < 64) {
              local_hits[local_hit_count++] = hit;
            } else {
              /* Critical dump if local hit buffer overflows */
#ifdef _OPENMP
#pragma omp critical
#endif
              {
                for (int i = 0; i < local_hit_count; i++) {
                  results_add_hit(results, &local_hits[i]);
                  log_hit(params->log_path, &local_hits[i]);
                }
              }
              local_hit_count = 0;
              local_hits[local_hit_count++] = hit;
            }

            if (g == 1) {
#ifdef _OPENMP
#pragma omp critical
#endif
              {
                printf("\n🚨 COUNTEREXAMPLE: %" PRIu64 "^%u + %" PRIu64
                       "^%u = %" PRIu64 "^%u (gcd=1)\n",
                       A, params->x, B_val, params->y, C, params->z);
              }
            }
          }
        }
      }
#else
    for (uint64_t B = B_start; B <= B_max; B++) {
      a_tested++;
      if (gcd64(A, B) > 1) {
        a_gcd++;
        continue;
      }
      if (!sieve_survives_scalar(A, B, data)) {
        a_mod++;
        continue;
      }
      a_exact++;
      uint64_t C, g;
      if (check_beal_hit_gmp(A, B, params->x, params->y, params->z, C_max, &C,
                             &g)) {
        BealHit hit = {A, B, C, g, params->x, params->y, params->z};
        if (local_hit_count < 64)
          local_hits[local_hit_count++] = hit;
        if (g == 1) {
          printf("\n🚨 COUNTEREXAMPLE: %" PRIu64 "^%u + %" PRIu64
                 "^%u = %" PRIu64 "^%u (gcd=1)\n",
                 A, params->x, B, params->y, C, params->z);
        }
      }
    }
#endif

      /* Update global stats atomically after each A iteration */
      atomic_fetch_add(&global_tested, a_tested);
      atomic_fetch_add(&global_gcd_skips, a_gcd);
      atomic_fetch_add(&global_mod_skips, a_mod);
      atomic_fetch_add(&global_exact_checks, a_exact);

      /* Progress Report (Throttled to ~1.0s) */
#ifdef _OPENMP
      double now = omp_get_wtime();
#else
    double now = (double)clock() / CLOCKS_PER_SEC;
#endif
      if (now - last_report_time > 1.0) {
#ifdef _OPENMP
#pragma omp critical(report)
#endif
        {
          if (now - last_report_time > 1.0) {
            last_report_time = now;
            double dt = now - start_time;
            uint64_t tested = atomic_load(&global_tested);
            double pct = 100.0 * tested / expected_pairs;
            double rate = dt > 0 ? (double)tested / dt / 1e6 : 0;
            uint64_t checks = atomic_load(&global_exact_checks);

            printf("\r[GOLIATH] Progress: %5.2f%% | A: %-7" PRIu64
                   " | Rate: %6.1fM/s | GMP Checks: %" PRIu64,
                   pct, A, rate, checks);
            fflush(stdout);

            /* Log live checkpoint */
            log_checkpoint(params->log_path, run_id, tested, expected_pairs,
                           atomic_load(&global_gcd_skips),
                           atomic_load(&global_mod_skips), dt,
                           (int)(A - A_start), (int)(A_max - A_start));
          }
        }
      }
    }

    /* Thread finishing: Merge remaining hits */
    if (local_hit_count > 0) {
#ifdef _OPENMP
#pragma omp critical
#endif
      {
        for (int i = 0; i < local_hit_count; i++) {
          results_add_hit(results, &local_hits[i]);
          log_hit(params->log_path, &local_hits[i]);
        }
      }
    }

#ifdef _OPENMP
  }
#endif

/* Calculate final timing */
#ifdef _OPENMP
  double elapsed = omp_get_wtime() - start_time;
#else
  double elapsed = (double)(clock() - start_time) / CLOCKS_PER_SEC;
#endif

  results->total_pairs = atomic_load(&global_tested);
  results->gcd_filtered = atomic_load(&global_gcd_skips);
  results->mod_filtered = atomic_load(&global_mod_skips);
  results->exact_checks = atomic_load(&global_exact_checks);
  results->runtime_seconds = elapsed;
  results->rate_pairs_per_sec =
      elapsed > 0 ? results->total_pairs / elapsed : 0;

  results->power_hits = results->hits_count;
  results->primitive_hits = 0;
  for (size_t i = 0; i < results->hits_count; i++) {
    if (results->hits[i].gcd == 1)
      results->primitive_hits++;
  }

  log_complete(params->log_path, run_id, params, results);

  printf("\n\nSearch Complete!\n================\n");
  printf("Total pairs:     %" PRIu64 "\n", results->total_pairs);
  printf("GCD filtered:    %" PRIu64 " (%.2f%%)\n", results->gcd_filtered,
         100.0 * results->gcd_filtered / results->total_pairs);
  printf("Sieve filtered:  %" PRIu64 " (%.2f%%)\n", results->mod_filtered,
         100.0 * results->mod_filtered / results->total_pairs);
  printf("Exact checks:    %" PRIu64 " (%.6f%%)\n", results->exact_checks,
         100.0 * results->exact_checks /
             (results->total_pairs ? results->total_pairs : 1));
  printf("Power hits:      %" PRIu64 "\n", results->power_hits);
  printf("Primitive hits:  %" PRIu64 "\n\n", results->primitive_hits);
  printf("Runtime:         %.2f seconds\n", results->runtime_seconds);
  printf("Throughput:      %.0f pairs/sec\n", results->rate_pairs_per_sec);

  if (results->primitive_hits > 0) {
    printf("\n*** COUNTEREXAMPLES FOUND! ***\n");
    for (size_t i = 0; i < results->hits_count; i++) {
      if (results->hits[i].gcd == 1) {
        BealHit *h = &results->hits[i];
        printf("  %" PRIu64 "^%u + %" PRIu64 "^%u = %" PRIu64 "^%u\n", h->A,
               h->x, h->B, h->y, h->C, h->z);
      }
    }
  } else {
    printf("\nResult: CLEAR - No counterexamples found.\n");
  }

  precompute_free(data);
}
