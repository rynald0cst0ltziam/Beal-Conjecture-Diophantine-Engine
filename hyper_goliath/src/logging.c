/**
 * JSONL Logging - matches Python engine format exactly.
 */

#include "hyper_goliath.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <sys/utsname.h>
#include <time.h>
#include <unistd.h>

/**
 * Get current UTC timestamp in ISO 8601 format.
 */
void get_timestamp_iso(char *buf, size_t len) {
  time_t now = time(NULL);
  struct tm *tm_info = gmtime(&now);
  strftime(buf, len, "%Y-%m-%dT%H:%M:%SZ", tm_info);
}

/**
 * Log the START event.
 */
void log_start(const char *path, const SearchParams *params, int num_workers) {
  if (!path)
    return;
  FILE *f = fopen(path, "w");
  if (!f)
    return;

  char ts[32];
  get_timestamp_iso(ts, sizeof(ts));

  char hostname[256] = "unknown";
  gethostname(hostname, sizeof(hostname));

  struct utsname uname_info;
  uname(&uname_info);

  uint64_t expected_pairs = (params->A_max - params->A_start + 1) *
                            (params->B_max - params->B_start + 1);

  fprintf(f,
          "{\"ts\":\"%s\",\"event\":\"START\",\"run_id\":%" PRIu64 ","
          "\"mode\":\"search\",\"signature\":[%u,%u,%u],"
          "\"Astart\":%" PRIu64 ",\"Amax\":%" PRIu64 ",\"Bstart\":%" PRIu64
          ",\"Bmax\":%" PRIu64 ","
          "\"Cmax\":%" PRIu64 ",\"expected_pairs\":%" PRIu64 ","
          "\"system\":{\"hostname\":\"%s\",\"platform\":\"%s %s\","
          "\"cpu_count\":%d,\"engine\":\"hyper_goliath_c\"},"
          "\"sieve_primes\":[2,3,5,7,11,13,17,19,23,29,31,37,41,43,47,53,59,61,"
          "67,71]}\n",
          ts, (uint64_t)time(NULL), params->x, params->y, params->z,
          params->A_start, params->A_max, params->B_start, params->B_max,
          params->C_max, expected_pairs, hostname, uname_info.sysname,
          uname_info.release, num_workers);

  fclose(f);
}

/**
 * Log a CHECKPOINT event.
 */
void log_checkpoint(const char *path, uint64_t run_id, uint64_t pairs_completed,
                    uint64_t pairs_expected, uint64_t gcd_skips,
                    uint64_t mod_skips, double elapsed_seconds, int chunks_done,
                    int chunks_total) {
  if (!path)
    return;
  FILE *f = fopen(path, "a");
  if (!f)
    return;

  char ts[32];
  get_timestamp_iso(ts, sizeof(ts));

  double pct = pairs_expected > 0
                   ? (double)pairs_completed / pairs_expected * 100.0
                   : 0.0;
  uint64_t exact_checks = pairs_completed > (gcd_skips + mod_skips)
                              ? pairs_completed - gcd_skips - mod_skips
                              : 0;
  double rate = elapsed_seconds > 0 ? pairs_completed / elapsed_seconds : 0;

  fprintf(f,
          "{\"ts\":\"%s\",\"event\":\"CHECKPOINT\",\"run_id\":%" PRIu64 ","
          "\"pairs_completed\":%" PRIu64 ",\"pairs_expected\":%" PRIu64 ","
          "\"percent_complete\":%.4f,\"gcd_skips\":%" PRIu64
          ",\"mod_skips\":%" PRIu64 ","
          "\"exact_checks\":%" PRIu64 ",\"elapsed_seconds\":%.2f,"
          "\"rate_pairs_per_sec\":%.0f,"
          "\"chunks_done\":%d,\"chunks_total\":%d}\n",
          ts, run_id, pairs_completed, pairs_expected, pct, gcd_skips,
          mod_skips, exact_checks, elapsed_seconds, rate, chunks_done,
          chunks_total);

  fclose(f);
}

/**
 * Log the COMPLETE event.
 */
void log_complete(const char *path, uint64_t run_id, const SearchParams *params,
                  const SearchResults *results) {
  if (!path)
    return;
  FILE *f = fopen(path, "a");
  if (!f)
    return;

  char ts[32];
  get_timestamp_iso(ts, sizeof(ts));

  /* FNV-1a hash for stronger proof-of-work verification */
  uint64_t hash = 14695981039346656037ULL; /* FNV offset basis */
  const uint64_t FNV_PRIME = 1099511628211ULL;

  /* Include all search parameters */
  hash ^= params->x;
  hash *= FNV_PRIME;
  hash ^= params->y;
  hash *= FNV_PRIME;
  hash ^= params->z;
  hash *= FNV_PRIME;
  hash ^= params->A_start;
  hash *= FNV_PRIME;
  hash ^= params->A_max;
  hash *= FNV_PRIME;
  hash ^= params->B_start;
  hash *= FNV_PRIME;
  hash ^= params->B_max;
  hash *= FNV_PRIME;
  hash ^= params->C_max;
  hash *= FNV_PRIME;

  /* Include all results for verification */
  hash ^= results->total_pairs;
  hash *= FNV_PRIME;
  hash ^= results->gcd_filtered;
  hash *= FNV_PRIME;
  hash ^= results->mod_filtered;
  hash *= FNV_PRIME;
  hash ^= results->exact_checks;
  hash *= FNV_PRIME;
  hash ^= results->power_hits;
  hash *= FNV_PRIME;
  hash ^= results->primitive_hits;
  hash *= FNV_PRIME;

  const char *status =
      results->primitive_hits > 0 ? "COUNTEREXAMPLE_FOUND" : "CLEAR";

  fprintf(
      f,
      "{\"ts\":\"%s\",\"event\":\"COMPLETE\",\"run_id\":%" PRIu64 ","
      "\"signature\":[%u,%u,%u],"
      "\"search_bounds\":{\"A\":[%" PRIu64 ",%" PRIu64 "],\"B\":[%" PRIu64
      ",%" PRIu64 "],\"C\":[1,%" PRIu64 "]},"
      "\"results\":{\"total_pairs\":%" PRIu64 ",\"gcd_filtered\":%" PRIu64 ","
      "\"mod_filtered\":%" PRIu64 ",\"exact_checks\":%" PRIu64 ","
      "\"power_hits\":%" PRIu64 ",\"primitive_counterexamples\":%" PRIu64 "},"
      "\"performance\":{\"runtime_seconds\":%.2f,"
      "\"avg_rate_pairs_per_sec\":%.0f,\"workers_used\":%d},"
      "\"verification\":{\"status\":\"%s\",\"integrity_hash\":\"%016" PRIx64
      "\"}}\n",
      ts, run_id, params->x, params->y, params->z, params->A_start,
      params->A_max, params->B_start, params->B_max, params->C_max,
      results->total_pairs, results->gcd_filtered, results->mod_filtered,
      results->exact_checks, results->power_hits, results->primitive_hits,
      results->runtime_seconds, results->rate_pairs_per_sec,
      params->num_threads > 0 ? params->num_threads : 1, status, hash);

  fclose(f);
}

/**
 * Log a power hit.
 */
void log_hit(const char *path, const BealHit *hit) {
  if (!path)
    return;
  FILE *f = fopen(path, "a");
  if (!f)
    return;

  char ts[32];
  get_timestamp_iso(ts, sizeof(ts));

  fprintf(f,
          "{\"ts\":\"%s\",\"event\":\"POWER_HIT\","
          "\"A\":%" PRIu64 ",\"B\":%" PRIu64 ",\"C\":%" PRIu64
          ",\"gcd\":%" PRIu64 ","
          "\"x\":%u,\"y\":%u,\"z\":%u}\n",
          ts, hit->A, hit->B, hit->C, hit->gcd, hit->x, hit->y, hit->z);

  fclose(f);
}
