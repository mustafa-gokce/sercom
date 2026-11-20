#!/bin/bash

#
# run_small_limit_test.sh
#
# Build the test executable with a small payload limit and run every test against it.
#
# The limit reaches CMake through the SERCOM_TEST_MAXIMUM_PAYLOAD_SIZE environment variable.
# The other build values of CMakeLists.txt stay as they are.
#
# usage: tools/run_small_limit_test.sh [--build-dir DIR] [--limit BYTES]
#

set -euo pipefail

# settings with overridable defaults
SOURCE_DIRECTORY="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
readonly SOURCE_DIRECTORY
BUILD_DIRECTORY="${BUILD_DIRECTORY:-${SOURCE_DIRECTORY}/build-small-limit}"
PAYLOAD_LIMIT="${PAYLOAD_LIMIT:-64}"

# print the usage text
print_usage() {
    echo "usage: tools/run_small_limit_test.sh [--build-dir DIR] [--limit BYTES]"
    echo "  --build-dir DIR  build directory (default ${BUILD_DIRECTORY})"
    echo "  --limit BYTES    payload limit of the build (default ${PAYLOAD_LIMIT})"
}

# parse the arguments
while [[ $# -gt 0 ]]; do
    case "$1" in
        --build-dir) BUILD_DIRECTORY="$2"; shift 2 ;;
        --limit) PAYLOAD_LIMIT="$2"; shift 2 ;;
        --help) print_usage; exit 0 ;;
        *) echo "unknown option: $1" >&2; print_usage >&2; exit 64 ;;
    esac
done
readonly BUILD_DIRECTORY
readonly PAYLOAD_LIMIT

# configure the tests only, with the small payload limit
SERCOM_TEST_MAXIMUM_PAYLOAD_SIZE="${PAYLOAD_LIMIT}" cmake -S "${SOURCE_DIRECTORY}" \
    -B "${BUILD_DIRECTORY}" -DCMAKE_BUILD_TYPE=Debug \
    -DSERCOM_BUILD_BINARIES=OFF -DSERCOM_BUILD_TESTS=ON

# build the test executable
cmake --build "${BUILD_DIRECTORY}" --target sercom_test --parallel

# run every test in declaration order
"${BUILD_DIRECTORY}/sercom_test" --order decl --reporter console
