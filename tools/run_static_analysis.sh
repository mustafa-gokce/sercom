#!/bin/bash

#
# run_static_analysis.sh
#
# Run the analysis gates of the project: layout, comments and idioms, cppcheck and clang-tidy.
#
# The build directory must hold the compile database of a CMake configuration with the binaries.
# cppcheck runs at its most thorough level, an argument stored in the member of its name is the accepted shadowMember.
# cppcheck reads the Qt macros of the host code, signals, slots and Q_OBJECT, through its qt library.
# The gates need clang-format 18, cppcheck 2.22 and clang-tidy 18, tools/analysis/install_cppcheck.sh builds cppcheck.
# Every gate runs, the exit code is 1 when one of them has a finding.
#
# usage: tools/run_static_analysis.sh [build directory]
#

set -euo pipefail

# settings with overridable defaults
SOURCE_DIRECTORY="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
readonly SOURCE_DIRECTORY
readonly CLANG_FORMAT="${CLANG_FORMAT:-clang-format}"
export CLANG_FORMAT
readonly CPPCHECK="${CPPCHECK:-cppcheck}"
readonly CPPCHECK_VERSION="2.22"
readonly CLANG_TIDY="${CLANG_TIDY:-clang-tidy}"
readonly CLANG_TIDY_VERSION="18"
readonly JOBS="${JOBS:-$(nproc)}"

# print the usage text
print_usage() {
    echo "usage: tools/run_static_analysis.sh [build directory]"
    echo "  build directory  CMake build directory with compile_commands.json (default build)"
    echo "  CLANG_FORMAT selects clang-format 18, CPPCHECK cppcheck ${CPPCHECK_VERSION}, CLANG_TIDY clang-tidy ${CLANG_TIDY_VERSION}"
    echo "  JOBS sets the number of jobs, tools/analysis/install_cppcheck.sh installs cppcheck ${CPPCHECK_VERSION}"
}

# get whether a shadowMember finding is an argument stored in the member of its name
get_accepted_shadow_member() {
    local finding="$1"
    local pattern="^([^:]+):([0-9]+):[0-9]+: [a-z,]+: shadowMember: "
    pattern+="Argument '([A-Za-z0-9_]+)' shadows outer member"
    if [[ ! "${finding}" =~ ${pattern} ]]; then
        return 1
    fi
    local file="${BASH_REMATCH[1]}"
    local line="${BASH_REMATCH[2]}"
    local name="${BASH_REMATCH[3]}"
    awk -v first_line="${line}" -v assignment="this->${name} = ${name};" \
        'NR >= first_line { if (index($0, assignment) > 0) { found = 1; exit } if (NR > first_line && /^}/) { exit } }
         END { exit found ? 0 : 1 }' "${file}"
}

# check the arguments
if [[ $# -ge 1 && "$1" == "--help" ]]; then
    print_usage
    exit 0
fi
if [[ $# -gt 1 ]]; then
    echo "unexpected argument: $2" >&2
    print_usage >&2
    exit 64
fi

# check the analyzer versions
if ! cppcheck_version_text="$("${CPPCHECK}" --version 2>/dev/null)"; then
    echo "${CPPCHECK} is not found, CPPCHECK selects the binary" >&2
    exit 69
fi
if [[ ! "${cppcheck_version_text}" =~ ^Cppcheck\ "${CPPCHECK_VERSION}"(\.|$) ]]; then
    echo "cppcheck ${CPPCHECK_VERSION} is needed, found: ${cppcheck_version_text}" >&2
    exit 69
fi
if ! clang_tidy_version_text="$("${CLANG_TIDY}" --version 2>/dev/null)"; then
    echo "${CLANG_TIDY} is not found, CLANG_TIDY selects the binary" >&2
    exit 69
fi
if [[ ! "${clang_tidy_version_text}" =~ version\ ${CLANG_TIDY_VERSION}\. ]]; then
    echo "clang-tidy ${CLANG_TIDY_VERSION} is needed, found: ${clang_tidy_version_text}" >&2
    exit 69
fi

# select the build directory
BUILD_DIRECTORY="$(cd "${1:-${SOURCE_DIRECTORY}/build}" && pwd)"
readonly BUILD_DIRECTORY
if [[ ! -f "${BUILD_DIRECTORY}/compile_commands.json" ]]; then
    echo "${BUILD_DIRECTORY} holds no compile_commands.json, configure with -DCMAKE_EXPORT_COMPILE_COMMANDS=ON" >&2
    exit 66
fi

# run every gate from the source directory and collect the failed ones
cd "${SOURCE_DIRECTORY}"
failed_gates=()

# check the layout against the formatter
echo "== layout"
tools/analysis/check_format.sh || failed_gates+=("layout")

# check the comments and the countable idioms
echo "== comments and idioms"
python3 tools/analysis/check_style.py || failed_gates+=("comments and idioms")

# check every own source with cppcheck
echo "== cppcheck"
mkdir -p "${BUILD_DIRECTORY}/cppcheck"
cppcheck_status=0
"${CPPCHECK}" --enable=all --inconclusive --check-level=exhaustive --std=c++17 --library=qt \
    --inline-suppr --suppress=missingIncludeSystem --suppress=missingInclude \
    --suppress=unmatchedSuppression --suppress=checkersReport --suppress=unusedFunction \
    --quiet -j "${JOBS}" --cppcheck-build-dir="${BUILD_DIRECTORY}/cppcheck" \
    --template='{file}:{line}:{column}: {severity}{inconclusive:,inconclusive}: {id}: {message}' \
    --output-file="${BUILD_DIRECTORY}/cppcheck.log" \
    core/src app/src host/src host/apps examples fuzz firmware/esp32/src || cppcheck_status=$?

# print the cppcheck findings without the accepted ones
cppcheck_finding_count=0
while IFS= read -r finding; do
    if get_accepted_shadow_member "${finding}"; then
        continue
    fi
    echo "${finding}"
    cppcheck_finding_count=$((cppcheck_finding_count + 1))
done < "${BUILD_DIRECTORY}/cppcheck.log"
if [[ "${cppcheck_status}" != "0" || "${cppcheck_finding_count}" != "0" ]]; then
    failed_gates+=("cppcheck")
fi

# check the sources of the compile database with clang-tidy, without the counts of suppressed warnings
echo "== clang-tidy"
clang_tidy_status=0
printf '%s\n' core/src/*.cpp app/src/*.cpp host/src/*.cpp host/apps/*.cpp examples/*.cpp \
    | xargs -P "${JOBS}" -n 1 "${CLANG_TIDY}" -p "${BUILD_DIRECTORY}" \
        --config-file=tools/analysis/clang-tidy.yaml --quiet --warnings-as-errors='*' \
        --extra-arg=-Wno-unknown-warning-option > "${BUILD_DIRECTORY}/clang-tidy.log" 2>&1 \
    || clang_tidy_status=$?
grep -vE '^[0-9]+ warnings?( and [0-9]+ errors?)? generated\.$' "${BUILD_DIRECTORY}/clang-tidy.log" || true
if [[ "${clang_tidy_status}" != "0" ]]; then
    failed_gates+=("clang-tidy")
fi

# notify the user
if [[ ${#failed_gates[@]} -gt 0 ]]; then
    echo "static analysis failed: $(IFS=,; echo "${failed_gates[*]}")" >&2
    exit 1
fi
echo "static analysis passed"
