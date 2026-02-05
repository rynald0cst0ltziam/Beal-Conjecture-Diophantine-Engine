# 🏎️ Hyper-Goliath: C/AVX2 Beal Conjecture Search Engine

High-performance C port of the Python Beal Residue Engine with AVX2 SIMD optimization.

**Target:** 100M+ pairs/second (20-50x faster than Python)

## Prerequisites

- macOS or Linux
- GCC/Clang with C11 support
- CMake 3.16+
- GMP library (GNU Multiple Precision Arithmetic)
- OpenMP support (optional, for parallelization)

### macOS Installation

```bash
brew install cmake gmp libomp
```

### Ubuntu Installation

```bash
sudo apt install build-essential cmake libgmp-dev libomp-dev
```

## Building

```bash
# From hyper_goliath directory
chmod +x scripts/build.sh
./scripts/build.sh
```

This creates:
- `build/hyper_goliath` - Main search engine
- `build/test_sieve` - Sieve validation
- `build/export_survivors` - Cross-validation export

## Self-Validation

Run built-in tests to verify correctness:

```bash
./build/hyper_goliath --validate
```

Expected output:
```
Hyper-Goliath Self-Validation
=============================

[1] Testing residue mask computation...
    PASS: Cubes mod 7 = {0, 1, 6}
    PASS: 5th powers mod 11 = {0, 1, 10}

[2] Testing GCD function...
    PASS: All GCD tests passed

[3] Testing modular exponentiation...
    PASS: Modular exponentiation correct

[4] Testing GMP exact verification...
    PASS: 2^6 + 2^6 = 2^7 (gcd=2, non-primitive)
    PASS: 2^3 + 3^3 = 35 correctly rejected (not a cube)

[5] Testing sieve on small range...
    Survivors in [1,100]x[1,100] for (3,4,5): 0
    PASS: Survivor count is reasonable

=============================
All validation tests PASSED!
```

## Cross-Validation Against Python

To verify the C engine produces **identical** results to the Python engine:

```bash
python3 scripts/cross_validate.py
```

## Usage

### Basic Search

```bash
./build/hyper_goliath --x 4 --y 5 --z 6 --Amax 10000 --Bmax 10000
```

### Full Goliath Run (300,000 bases)

```bash
./build/hyper_goliath \
    --x 4 --y 5 --z 6 \
    --Amax 300000 --Bmax 300000 \
    --Cmax 100000000 \
    --log goliath_4_5_6.jsonl
```

### All Options

```
--x <N>          Exponent x (must be > 2)
--y <N>          Exponent y (must be > 2)
--z <N>          Exponent z (must be > 2)
--Amax <N>       Maximum A value (default: 1000)
--Bmax <N>       Maximum B value (default: 1000)
--Cmax <N>       Maximum C value (default: 10000000)
--Astart <N>     Starting A value (default: 1)
--Bstart <N>     Starting B value (default: 1)
--threads <N>    Number of threads (default: auto)
--log <file>     JSONL log file path
--validate       Run self-validation tests
--help           Show help
```

## Log Format

Logs are in JSONL format, compatible with Python engine tools:

```json
{"ts":"2026-02-04T10:00:00Z","event":"START",...}
{"ts":"2026-02-04T10:00:10Z","event":"CHECKPOINT",...}
{"ts":"2026-02-04T10:30:00Z","event":"COMPLETE",...}
```

## Architecture

```
┌─────────────────────────────────────────────────────┐
│                 HYPER-GOLIATH                       │
├─────────────────────────────────────────────────────┤
│  Precompute     →    Sieve (AVX2)    →    GMP      │
│  (startup)           (hot path)          (verify)   │
├─────────────────────────────────────────────────────┤
│              OpenMP Thread Pool                     │
│           Core 0 │ Core 1 │ ... │ Core N            │
├─────────────────────────────────────────────────────┤
│              JSONL Logger (async)                   │
└─────────────────────────────────────────────────────┘
```

## Performance

| Configuration | Pairs/sec | Notes |
|---------------|-----------|-------|
| Python (8 cores) | 4.5M | Current baseline |
| C Scalar (1 core) | 10-15M | 2-3x improvement |
| C OpenMP (8 cores) | 50-80M | 10-15x improvement |
| C AVX2 + OpenMP | 100M+ | Target performance |

## Mathematical Integrity

The C engine maintains **byte-for-byte equivalence** with Python:

1. **Same 20 primes:** {2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41, 43, 47, 53, 59, 61, 67, 71}
2. **Same residue computation:** R_z(p) = {r^z mod p | r ∈ [0, p-1]}
3. **Same GCD logic:** Skip pairs where gcd(A,B) > 1
4. **Same sieve logic:** Kill pair iff (A^x + B^y) mod p ∉ R_z(p) for any prime p
5. **Same verification:** GMP mpz_root for exact integer n-th root

## License

MIT License - Part of Project Goliath
