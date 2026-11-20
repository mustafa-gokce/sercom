#!/bin/bash

#
# install_cppcheck.sh
#
# Build cppcheck 2.22.0 from its source tag and install it into a prefix directory.
#
# The cppcheck gate of tools/run_static_analysis.sh needs this version, other versions report other findings.
# An installation that already reports the version is kept, so a cached prefix costs no build.
#
# usage: tools/analysis/install_cppcheck.sh [--prefix DIRECTORY]
#

set -euo pipefail

# settings with overridable defaults
readonly CPPCHECK_VERSION="2.22.0"
readonly CPPCHECK_REPOSITORY="${CPPCHECK_REPOSITORY:-https://github.com/danmar/cppcheck.git}"
CPPCHECK_PREFIX="${CPPCHECK_PREFIX:-${HOME}/.local/cppcheck-${CPPCHECK_VERSION}}"
readonly JOBS="${JOBS:-$(nproc)}"

# print the usage text
print_usage() {
    echo "usage: tools/analysis/install_cppcheck.sh [--prefix DIRECTORY]"
    echo "  --prefix DIRECTORY  installation directory, cppcheck lands in DIRECTORY/bin (default ${CPPCHECK_PREFIX})"
    echo "  CPPCHECK_REPOSITORY selects the source repository, JOBS the number of build jobs"
}

# parse the arguments
while [[ $# -gt 0 ]]; do
    case "$1" in
        --prefix) CPPCHECK_PREFIX="$2"; shift 2 ;;
        --help) print_usage; exit 0 ;;
        *) echo "unknown option: $1" >&2; print_usage >&2; exit 64 ;;
    esac
done
readonly CPPCHECK_PREFIX

# keep an installation of the version
if version_text="$("${CPPCHECK_PREFIX}/bin/cppcheck" --version 2>/dev/null)" \
    && [[ "${version_text}" == "Cppcheck ${CPPCHECK_VERSION}" ]]; then
    echo "${version_text} is installed in ${CPPCHECK_PREFIX}"
    exit 0
fi

# create the work directory and remove it on exit
WORK_DIRECTORY="$(mktemp -d)"
readonly WORK_DIRECTORY
clean_up() {
    rm -rf "${WORK_DIRECTORY}"
}
trap clean_up EXIT

# get the source of the version
git clone --quiet --depth 1 --branch "${CPPCHECK_VERSION}" "${CPPCHECK_REPOSITORY}" "${WORK_DIRECTORY}/source"

# build and install the release build
cmake -S "${WORK_DIRECTORY}/source" -B "${WORK_DIRECTORY}/build" -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="${CPPCHECK_PREFIX}"
cmake --build "${WORK_DIRECTORY}/build" --parallel "${JOBS}"
cmake --install "${WORK_DIRECTORY}/build"

# notify the user
echo "$("${CPPCHECK_PREFIX}/bin/cppcheck" --version) is installed in ${CPPCHECK_PREFIX}"
