#!/bin/bash

#
# run_fuzzers.sh
#
# Build the libFuzzer executables of the frame decoder, the link layer and the router and run each of them for a while.
#
# The fuzzer flags reach CMake from outside through CMAKE_CXX_FLAGS, CXX selects the Clang compiler.
# The frame decoder and the router start from their own copies of fuzz/corpus in the build directory, the link layer
# from nothing.
#
# usage: fuzz/run_fuzzers.sh [--build-dir DIR] [--seconds N]
#

set -euo pipefail

# settings with overridable defaults
SOURCE_DIRECTORY="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
readonly SOURCE_DIRECTORY
BUILD_DIRECTORY="${BUILD_DIRECTORY:-${SOURCE_DIRECTORY}/build-fuzz}"
FUZZ_SECONDS="${FUZZ_SECONDS:-60}"
readonly COMPILER="${CXX:-clang++}"
readonly FUZZER_FLAGS="-fsanitize=fuzzer-no-link,address,undefined"

# print the usage text
print_usage() {
    echo "usage: fuzz/run_fuzzers.sh [--build-dir DIR] [--seconds N]"
    echo "  --build-dir DIR  build directory (default ${BUILD_DIRECTORY})"
    echo "  --seconds N      run time of each fuzzer in seconds (default ${FUZZ_SECONDS})"
    echo "  CXX selects the Clang compiler (default ${COMPILER})"
}

# parse the arguments
while [[ $# -gt 0 ]]; do
    case "$1" in
        --build-dir) BUILD_DIRECTORY="$2"; shift 2 ;;
        --seconds) FUZZ_SECONDS="$2"; shift 2 ;;
        --help) print_usage; exit 0 ;;
        *) echo "unknown option: $1" >&2; print_usage >&2; exit 64 ;;
    esac
done
readonly BUILD_DIRECTORY
readonly FUZZ_SECONDS

# configure the fuzz executables only, with the fuzzer flags
cmake -S "${SOURCE_DIRECTORY}" -B "${BUILD_DIRECTORY}" -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_CXX_COMPILER="${COMPILER}" -DSERCOM_BUILD_BINARIES=OFF -DSERCOM_BUILD_TESTS=OFF \
    -DCMAKE_CXX_FLAGS="${FUZZER_FLAGS} -Wno-unknown-warning-option"

# build the fuzz executables
cmake --build "${BUILD_DIRECTORY}" --parallel

# copy the seed corpus for the frame decoder and the router, each fuzzer adds its findings to its copy
mkdir -p "${BUILD_DIRECTORY}/corpus" "${BUILD_DIRECTORY}/corpus-router"
cp "${SOURCE_DIRECTORY}"/fuzz/corpus/* "${BUILD_DIRECTORY}/corpus/"
cp "${SOURCE_DIRECTORY}"/fuzz/corpus/* "${BUILD_DIRECTORY}/corpus-router/"

# run each fuzzer for its time
"${BUILD_DIRECTORY}/sercom_fuzz_frame_decoder" -max_total_time="${FUZZ_SECONDS}" "${BUILD_DIRECTORY}/corpus"
"${BUILD_DIRECTORY}/sercom_fuzz_link" -max_total_time="${FUZZ_SECONDS}"
"${BUILD_DIRECTORY}/sercom_fuzz_router" -max_total_time="${FUZZ_SECONDS}" "${BUILD_DIRECTORY}/corpus-router"
