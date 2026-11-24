#!/bin/bash

#
# run_consumers.sh
#
# Build and run the consumer projects of examples 28 and 29 against this checkout of sercom.
#
# Example 28 adds the checkout with add_subdirectory and is configured with Qt disabled, so sercom must not ask for Qt.
# CMake then warns that the two variables that disable Qt were not used, no find_package of Qt ran.
# Example 29 takes the checkout through FETCHCONTENT_SOURCE_DIR_SERCOM in place of the download and builds with the Qt
# of CMAKE_PREFIX_PATH, Qt 5.12 or newer or Qt 6.2 or newer, while the device simulator stays on Qt 5.12.12.
# A third consumer builds the source of example 28 without a C++ standard of its own, sercom must require C++17 of it.
# Both builds export their compile commands, and no command may hold -Werror or -Wold-style-cast of sercom.
# No build may print a compiler warning, so sercom must build without one also on a newer Qt.
# Example 29 runs on a socat pseudo terminal pair against the device simulator and is skipped when socat is missing.
# CMAKE_PREFIX_PATH names the Qt, CXX the compiler and CMAKE_GENERATOR the generator, as CMake reads them.
#
# usage: tools/run_consumers.sh <sercom_device_sim> [--build-dir DIR]
#

set -euo pipefail

# settings with overridable defaults
SOURCE_DIRECTORY="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
readonly SOURCE_DIRECTORY
BUILD_DIRECTORY="${BUILD_DIRECTORY:-${SOURCE_DIRECTORY}/build-consumers}"
readonly BUILD_TYPE="${BUILD_TYPE:-Debug}"
readonly TIMEOUT_SECONDS="${TIMEOUT_SECONDS:-60}"
readonly ADD_SUBDIRECTORY_PROJECT="28_consume_with_add_subdirectory"
readonly FETCH_CONTENT_PROJECT="29_consume_with_fetch_content"
readonly STANDARD_PROBE_PROJECT="consume_without_cxx_standard"

# print the usage text
print_usage() {
    echo "usage: tools/run_consumers.sh <sercom_device_sim> [--build-dir DIR]"
    echo "  sercom_device_sim  device simulator of the main build, example 29 pings it"
    echo "  --build-dir DIR    build directory of the projects (default ${BUILD_DIRECTORY})"
    echo "  BUILD_TYPE sets the build type (default ${BUILD_TYPE}), TIMEOUT_SECONDS stops a program after that"
    echo "  time (default ${TIMEOUT_SECONDS}), CMAKE_PREFIX_PATH names the Qt of example 29, Qt 5.12 or newer or"
    echo "  Qt 6.2 or newer"
}

# parse the arguments
SIMULATOR=""
while [[ $# -gt 0 ]]; do
    case "$1" in
        --build-dir) BUILD_DIRECTORY="$2"; shift 2 ;;
        --help) print_usage; exit 0 ;;
        -*) echo "unknown option: $1" >&2; print_usage >&2; exit 64 ;;
        *)
            if [[ -n "${SIMULATOR}" ]]; then
                echo "unexpected argument: $1" >&2
                print_usage >&2
                exit 64
            fi
            SIMULATOR="$1"
            shift
            ;;
    esac
done
readonly BUILD_DIRECTORY

# device simulator must be given
if [[ -z "${SIMULATOR}" ]]; then
    echo "missing argument: <sercom_device_sim> is required" >&2
    print_usage >&2
    exit 64
fi
readonly SIMULATOR
if [[ ! -x "${SIMULATOR}" ]]; then
    echo "${SIMULATOR} is not found or not executable" >&2
    exit 66
fi
WORK_DIRECTORY="$(mktemp -d)"
readonly WORK_DIRECTORY

# stop every background process and remove the work directory on exit
process_ids=()
clean_up() {
    kill "${process_ids[@]}" 2>/dev/null || true
    wait 2>/dev/null || true
    rm -rf "${WORK_DIRECTORY}"
}
trap clean_up EXIT

# wait until a file exists or holds a line, fail after five seconds
wait_for_file() {
    local file="$1"
    local text="${2:-}"
    for _ in $(seq 1 100); do
        if [[ -e "${file}" ]] && { [[ -z "${text}" ]] || grep -q "${text}" "${file}"; }; then
            return 0
        fi
        sleep 0.05
    done
    echo "${file} did not appear or did not show ${text}" >&2
    return 1
}

# configure and build a consumer project with its compile commands and a build log
build_project() {
    local project="$1"
    shift
    echo "build    ${project}"
    cmake -S "${SOURCE_DIRECTORY}/examples/${project}" -B "${BUILD_DIRECTORY}/${project}" \
        -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON "$@"
    cmake --build "${BUILD_DIRECTORY}/${project}" --parallel 2>&1 | tee "${BUILD_DIRECTORY}/${project}.build.log"
}

# write, configure and build a consumer of the example 28 source without a C++ standard of its own
build_standard_probe() {
    local probe_directory="${BUILD_DIRECTORY}/${STANDARD_PROBE_PROJECT}"
    echo "build    ${STANDARD_PROBE_PROJECT}"
    mkdir -p "${probe_directory}"
    cat > "${probe_directory}/CMakeLists.txt" << EOF
cmake_minimum_required(VERSION 3.16)
project(${STANDARD_PROBE_PROJECT} LANGUAGES CXX)
add_subdirectory("${SOURCE_DIRECTORY}" sercom EXCLUDE_FROM_ALL)
add_executable(${STANDARD_PROBE_PROJECT}
    "${SOURCE_DIRECTORY}/examples/${ADD_SUBDIRECTORY_PROJECT}/${ADD_SUBDIRECTORY_PROJECT}.cpp"
)
target_link_libraries(${STANDARD_PROBE_PROJECT} PRIVATE
    sercom::settings
    sercom::core
    sercom::app
)
EOF
    cmake -S "${probe_directory}" -B "${probe_directory}/build" -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
        -DCMAKE_DISABLE_FIND_PACKAGE_QT=ON -DCMAKE_DISABLE_FIND_PACKAGE_Qt5=ON
    cmake --build "${probe_directory}/build" --parallel 2>&1 \
        | tee "${BUILD_DIRECTORY}/${STANDARD_PROBE_PROJECT}.build.log"
    echo "passed   ${STANDARD_PROBE_PROJECT}: the sercom headers build with the C++17 of sercom::settings"
}

# fail when a compile command of a consumer project holds a warning flag of sercom
check_compile_commands() {
    local project="$1"
    local compile_commands="${BUILD_DIRECTORY}/${project}/compile_commands.json"
    local command_count
    local flagged_count
    command_count="$(grep -c '"command"' "${compile_commands}" || true)"
    flagged_count="$(grep '"command"' "${compile_commands}" | grep -cE -- '-Werror|-Wold-style-cast' || true)"
    if [[ "${command_count}" == "0" || "${flagged_count}" != "0" ]]; then
        echo "failed   ${project}: ${flagged_count} of ${command_count} compile commands hold -Werror or" \
            "-Wold-style-cast" >&2
        return 1
    fi
    echo "passed   ${project}: none of ${command_count} compile commands holds -Werror or -Wold-style-cast"
}

# fail when the build of a consumer project printed a compiler warning
check_build_output() {
    local project="$1"
    local warning_count
    warning_count="$(grep -c -i 'warning:' "${BUILD_DIRECTORY}/${project}.build.log" || true)"
    if [[ "${warning_count}" != "0" ]]; then
        echo "failed   ${project}: its build printed ${warning_count} warnings" >&2
        return 1
    fi
    echo "passed   ${project}: its build printed no warning"
}

# run the consumer project of example 28, its device and client run in memory
run_add_subdirectory_project() {
    local log_file="${WORK_DIRECTORY}/${ADD_SUBDIRECTORY_PROJECT}.log"
    timeout "${TIMEOUT_SECONDS}" "${BUILD_DIRECTORY}/${ADD_SUBDIRECTORY_PROJECT}/consume_with_add_subdirectory" \
        > "${log_file}" 2>&1 || { cat "${log_file}" >&2; return 1; }
    cat "${log_file}"

    # device must have answered the ping
    grep -q '^Ping answered by ' "${log_file}"
}

# run the consumer project of example 29 on a port pair against the device simulator
run_fetch_content_project() {
    local log_file="${WORK_DIRECTORY}/${FETCH_CONTENT_PROJECT}.log"
    local first_port="${WORK_DIRECTORY}/ttyV1"
    local second_port="${WORK_DIRECTORY}/ttyV2"
    local simulator_log_file="${WORK_DIRECTORY}/simulator.log"
    socat PTY,raw,echo=0,link="${first_port}" PTY,raw,echo=0,link="${second_port}" &
    process_ids+=($!)
    { wait_for_file "${first_port}" && wait_for_file "${second_port}"; } || return 1
    "${SIMULATOR}" "${second_port}" > "${simulator_log_file}" 2>&1 &
    process_ids+=($!)
    wait_for_file "${simulator_log_file}" "Started the DeviceSimulator" || return 1
    timeout "${TIMEOUT_SECONDS}" "${BUILD_DIRECTORY}/${FETCH_CONTENT_PROJECT}/consume_with_fetch_content" \
        "${first_port}" > "${log_file}" 2>&1 || { cat "${log_file}" >&2; return 1; }
    cat "${log_file}"

    # device must have answered the ping
    grep -q '^Ping answered by ' "${log_file}"
}

# build example 28 with Qt disabled, sercom must configure without it
build_project "${ADD_SUBDIRECTORY_PROJECT}" -DCMAKE_DISABLE_FIND_PACKAGE_QT=ON -DCMAKE_DISABLE_FIND_PACKAGE_Qt5=ON

# build example 29 with this checkout in place of the download
build_project "${FETCH_CONTENT_PROJECT}" -DFETCHCONTENT_SOURCE_DIR_SERCOM="${SOURCE_DIRECTORY}"

# build a consumer without a C++ standard of its own
build_standard_probe

# check the compile commands of both projects
failed_steps=()
check_compile_commands "${ADD_SUBDIRECTORY_PROJECT}" || failed_steps+=("${ADD_SUBDIRECTORY_PROJECT} compile commands")
check_compile_commands "${FETCH_CONTENT_PROJECT}" || failed_steps+=("${FETCH_CONTENT_PROJECT} compile commands")

# check the build output of the three consumers
for project in "${ADD_SUBDIRECTORY_PROJECT}" "${FETCH_CONTENT_PROJECT}" "${STANDARD_PROBE_PROJECT}"; do
    check_build_output "${project}" || failed_steps+=("${project} warnings")
done

# run example 28
skipped_count=0
if run_add_subdirectory_project; then
    echo "passed   ${ADD_SUBDIRECTORY_PROJECT}"
else
    echo "failed   ${ADD_SUBDIRECTORY_PROJECT}" >&2
    failed_steps+=("${ADD_SUBDIRECTORY_PROJECT}")
fi

# run example 29 when socat is there
if ! command -v socat > /dev/null; then
    echo "skipped  ${FETCH_CONTENT_PROJECT} (socat is not found)"
    skipped_count=1
elif run_fetch_content_project; then
    echo "passed   ${FETCH_CONTENT_PROJECT}"
else
    echo "failed   ${FETCH_CONTENT_PROJECT}" >&2
    failed_steps+=("${FETCH_CONTENT_PROJECT}")
fi

# notify the user
if [[ ${#failed_steps[@]} -gt 0 ]]; then
    echo "consumers failed: $(IFS=,; echo "${failed_steps[*]}")" >&2
    exit 1
fi
echo "consumers passed: $((3 - skipped_count)), skipped: ${skipped_count}"
