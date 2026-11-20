#!/bin/bash

#
# run_sanitizer_test.sh
#
# Build the test executable with AddressSanitizer and UndefinedBehaviorSanitizer and run every test against it.
#
# The sanitizer flags reach CMake from outside through CMAKE_CXX_FLAGS, CXX selects the compiler.
# The first sanitizer report stops the test executable with a failure.
#
# usage: tools/run_sanitizer_test.sh [--build-dir DIR]
#

set -euo pipefail

# settings with overridable defaults
SOURCE_DIRECTORY="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
readonly SOURCE_DIRECTORY
BUILD_DIRECTORY="${BUILD_DIRECTORY:-${SOURCE_DIRECTORY}/build-sanitizers}"
readonly COMPILER="${CXX:-clang++}"
readonly SANITIZER_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all"

# print the usage text
print_usage() {
    echo "usage: tools/run_sanitizer_test.sh [--build-dir DIR]"
    echo "  --build-dir DIR  build directory (default ${BUILD_DIRECTORY})"
    echo "  CXX selects the compiler (default ${COMPILER})"
}

# parse the arguments
while [[ $# -gt 0 ]]; do
    case "$1" in
        --build-dir) BUILD_DIRECTORY="$2"; shift 2 ;;
        --help) print_usage; exit 0 ;;
        *) echo "unknown option: $1" >&2; print_usage >&2; exit 64 ;;
    esac
done
readonly BUILD_DIRECTORY

# configure the tests with the sanitizer flags
cmake -S "${SOURCE_DIRECTORY}" -B "${BUILD_DIRECTORY}" -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_CXX_COMPILER="${COMPILER}" -DCMAKE_CXX_FLAGS="${SANITIZER_FLAGS} -Wno-unknown-warning-option"

# build the test executable
cmake --build "${BUILD_DIRECTORY}" --target sercom_test --parallel

# run every test in declaration order
"${BUILD_DIRECTORY}/sercom_test" --order decl --reporter console
