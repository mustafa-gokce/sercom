#!/bin/bash

#
# check_format.sh
#
# Check that every own C++ file equals the output of clang-format 18 with the house style file.
#
# The style file is tools/analysis/clang-format.yaml, other clang-format versions lay out differently.
# Every file that differs is listed with its number of changed lines, --fix rewrites those files.
#
# usage: tools/analysis/check_format.sh [--fix]
#

set -euo pipefail

# settings with overridable defaults
SOURCE_DIRECTORY="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
readonly SOURCE_DIRECTORY
readonly STYLE_FILE="${SOURCE_DIRECTORY}/tools/analysis/clang-format.yaml"
readonly CLANG_FORMAT="${CLANG_FORMAT:-clang-format}"
readonly CLANG_FORMAT_VERSION="18"

# print the usage text
print_usage() {
    echo "usage: tools/analysis/check_format.sh [--fix]"
    echo "  --fix  rewrite the files that differ from the formatter output"
    echo "  CLANG_FORMAT selects the clang-format ${CLANG_FORMAT_VERSION} binary (default clang-format)"
}

# parse the arguments
FIX="false"
while [[ $# -gt 0 ]]; do
    case "$1" in
        --fix) FIX="true"; shift ;;
        --help) print_usage; exit 0 ;;
        *) echo "unknown option: $1" >&2; print_usage >&2; exit 64 ;;
    esac
done
readonly FIX

# check the formatter version
if ! version_text="$("${CLANG_FORMAT}" --version 2>/dev/null)"; then
    echo "${CLANG_FORMAT} is not found, CLANG_FORMAT selects the binary" >&2
    exit 69
fi
if [[ ! "${version_text}" =~ version\ ${CLANG_FORMAT_VERSION}\. ]]; then
    echo "clang-format ${CLANG_FORMAT_VERSION} is needed, found: ${version_text}" >&2
    exit 69
fi

# list the own C++ files
cd "${SOURCE_DIRECTORY}"
mapfile -t FILES < <(find core app host firmware/common/include firmware/common/src firmware/esp32/include \
    firmware/esp32/src firmware/stm32f4/include firmware/stm32f4/src tests examples fuzz \
    -name '.*' -prune -o -type f \( -name '*.h' -o -name '*.cpp' \) -print | sort)
readonly FILES

# compare every file with the formatter output, from its first differing line on
deviation_count=0
for file in "${FILES[@]}"; do
    difference="$("${CLANG_FORMAT}" --style=file:"${STYLE_FILE}" "${file}" | diff "${file}" - || true)"
    if [[ -z "${difference}" ]]; then
        continue
    fi
    deviation_count=$((deviation_count + 1))
    first_line="$(head -1 <<< "${difference}" | grep -oE '^[0-9]+')"
    changed_line_count="$(grep -c '^>' <<< "${difference}" || true)"
    if [[ "${changed_line_count}" == "0" ]]; then
        changed_line_count="$(grep -c '^<' <<< "${difference}" || true)"
    fi
    echo "${file}:${first_line}: ${changed_line_count} lines differ from the formatter output"
    if [[ "${FIX}" == "true" ]]; then
        "${CLANG_FORMAT}" --style=file:"${STYLE_FILE}" -i "${file}"
    fi
done

# notify the user
if [[ "${FIX}" == "true" ]]; then
    echo "layout: ${deviation_count} of ${#FILES[@]} files formatted"
    exit 0
fi
echo "layout: ${deviation_count} of ${#FILES[@]} files differ"
if [[ "${deviation_count}" != "0" ]]; then
    exit 1
fi
