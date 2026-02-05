#!/bin/bash
# Build script for Hyper-Goliath with OpenMP support for macOS

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_DIR/build"

echo "================================"
echo "Building Hyper-Goliath"
echo "================================"
echo ""

# Check for Homebrew on macOS
if [[ "$OSTYPE" == "darwin"* ]]; then
    if command -v brew >/dev/null 2>&1; then
        # Check for GMP
        if ! brew list gmp &>/dev/null; then
            echo "GMP not found. Installing with Homebrew..."
            brew install gmp
        fi

        # Check for libomp (OpenMP)
        if ! brew list libomp &>/dev/null; then
            echo "libomp not found. Installing with Homebrew..."
            brew install libomp
        fi

        LIBOMP_PREFIX=$(brew --prefix libomp)
        GMP_PREFIX=$(brew --prefix gmp)
    else
        echo "Homebrew not found. Assuming dependencies are installed manually."
        LIBOMP_PREFIX="/usr/local/opt/libomp"
        GMP_PREFIX="/usr/local"
    fi
else
    # Linux or other
    echo "Platform: $OSTYPE. Assuming dependencies (libgmp-dev, libomp-dev) are installed."
    LIBOMP_PREFIX="/usr"
    GMP_PREFIX="/usr"
fi

# Create build directory
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Configure with CMake
echo "Configuring with CMake..."
# We pass Homebrew paths to CMake so it finds OpenMP and GMP correctly on macOS
cmake -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_C_FLAGS="-I$GMP_PREFIX/include -I$LIBOMP_PREFIX/include" \
      -DCMAKE_EXE_LINKER_FLAGS="-L$LIBOMP_PREFIX/lib" \
      -DGMP_LIBRARY="$GMP_PREFIX/lib/libgmp.dylib" \
      -DGMP_INCLUDE_DIR="$GMP_PREFIX/include" \
      -DOpenMP_C_FLAGS="-Xpreprocessor -fopenmp -I$LIBOMP_PREFIX/include" \
      -DOpenMP_C_LIB_NAMES="omp" \
      -DOpenMP_omp_LIBRARY="$LIBOMP_PREFIX/lib/libomp.dylib" \
      "$PROJECT_DIR"

# Build
echo ""
echo "Building..."
make -j$(sysctl -n hw.ncpu)

echo ""
echo "================================"
echo "Build complete!"
echo ""
echo "Executables:"
echo "  $BUILD_DIR/hyper_goliath    - Main search engine"
echo "  $BUILD_DIR/test_sieve       - Sieve validation"
echo "  $BUILD_DIR/export_survivors - Cross-validation export"
echo ""
echo "Run validation:"
echo "  $BUILD_DIR/hyper_goliath --validate"
echo ""
echo "Run 100M pairs/sec benchmark:"
echo "  $BUILD_DIR/hyper_goliath --x 3 --y 4 --z 5 --Amax 10000 --Bmax 10000"
echo "================================"
