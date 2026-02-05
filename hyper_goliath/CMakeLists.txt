cmake_minimum_required(VERSION 3.16)
project(hyper_goliath VERSION 1.0.0 LANGUAGES C)

# C11 Standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Build type defaults to Release
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Compiler flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wpedantic")
set(CMAKE_C_FLAGS_DEBUG "-g -O0 -DDEBUG")
set(CMAKE_C_FLAGS_RELEASE "-O3 -DNDEBUG")

# Check for AVX2 support
include(CheckCCompilerFlag)
check_c_compiler_flag("-mavx2" COMPILER_SUPPORTS_AVX2)

if(COMPILER_SUPPORTS_AVX2)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mavx2")
    add_definitions(-DHAVE_AVX2)
    message(STATUS "AVX2 support enabled")
else()
    message(WARNING "AVX2 not supported - falling back to scalar implementation")
endif()

# Find OpenMP
find_package(OpenMP)
if(OpenMP_C_FOUND)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
    message(STATUS "OpenMP support enabled")
else()
    message(WARNING "OpenMP not found - running single-threaded")
endif()

# Find GMP library
find_library(GMP_LIBRARY gmp)
find_path(GMP_INCLUDE_DIR gmp.h)

if(GMP_LIBRARY AND GMP_INCLUDE_DIR)
    message(STATUS "Found GMP: ${GMP_LIBRARY}")
    include_directories(${GMP_INCLUDE_DIR})
else()
    message(FATAL_ERROR "GMP library not found. Install with: brew install gmp")
endif()

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)

# Source files
set(SOURCES
    src/main.c
    src/precompute.c
    src/sieve.c
    src/gmp_verify.c
    src/logging.c
    src/parallel.c
    src/utils.c
)

# Main executable
add_executable(hyper_goliath ${SOURCES})
target_link_libraries(hyper_goliath ${GMP_LIBRARY} m)

if(OpenMP_C_FOUND)
    target_link_libraries(hyper_goliath OpenMP::OpenMP_C)
endif()

# Test executable
add_executable(test_sieve tests/test_sieve.c src/precompute.c src/sieve.c src/utils.c)
target_link_libraries(test_sieve ${GMP_LIBRARY} m)

# Cross-validation export tool
add_executable(export_survivors tests/export_survivors.c src/precompute.c src/sieve.c src/gmp_verify.c src/utils.c)
target_link_libraries(export_survivors ${GMP_LIBRARY} m)

if(OpenMP_C_FOUND)
    target_link_libraries(export_survivors OpenMP::OpenMP_C)
endif()

# Install
install(TARGETS hyper_goliath DESTINATION bin)

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C flags: ${CMAKE_C_FLAGS}")
