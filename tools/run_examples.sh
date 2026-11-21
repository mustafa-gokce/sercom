#!/bin/bash

#
# run_examples.sh
#
# Run every example program of a build directory and fail when one of them exits with a code other than 0.
#
# The script builds nothing, the build directory must hold every example program and sercom_device_sim.
# Example 01 runs on a socat pseudo terminal pair, must print a received text and stops on an interrupt signal.
# Example 24 runs on a socat pseudo terminal pair against the device simulator on the far end.
# Both are skipped with a message when socat is missing, every example runs before the exit code is set.
#
# usage: tools/run_examples.sh [build directory]
#

set -euo pipefail

# settings with overridable defaults
SOURCE_DIRECTORY="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
readonly SOURCE_DIRECTORY
readonly TIMEOUT_SECONDS="${TIMEOUT_SECONDS:-60}"
readonly RAW_SERIAL_SECONDS="${RAW_SERIAL_SECONDS:-2}"

# print the usage text
print_usage() {
    echo "usage: tools/run_examples.sh [build directory]"
    echo "  build directory  CMake build directory with the example programs (default build)"
    echo "  TIMEOUT_SECONDS stops an example after that time (default ${TIMEOUT_SECONDS})"
    echo "  RAW_SERIAL_SECONDS sets the run time of example 01 before the interrupt (default ${RAW_SERIAL_SECONDS})"
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

# select the build directory
BUILD_DIRECTORY="$(cd "${1:-${SOURCE_DIRECTORY}/build}" && pwd)"
readonly BUILD_DIRECTORY
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

# start a socat pseudo terminal pair that connects two port links
start_port_pair() {
    local first_port="$1"
    local second_port="$2"
    socat PTY,raw,echo=0,link="${first_port}" PTY,raw,echo=0,link="${second_port}" &
    process_ids+=($!)
    wait_for_file "${first_port}" && wait_for_file "${second_port}"
}

# run an example that needs no serial port
run_example() {
    local program="$1"
    local log_file="$2"
    timeout "${TIMEOUT_SECONDS}" "${program}" > "${log_file}" 2>&1
}

# run the raw serial example on a port pair, send it a text and stop it with an interrupt signal
run_raw_serial_example() {
    local program="$1"
    local log_file="$2"
    local first_port="${WORK_DIRECTORY}/raw_serial_ttyV1"
    local second_port="${WORK_DIRECTORY}/raw_serial_ttyV2"
    start_port_pair "${first_port}" "${second_port}" || return 1
    "${program}" "${first_port}" > "${log_file}" 2>&1 &
    local example_process_id=$!
    process_ids+=("${example_process_id}")
    wait_for_file "${log_file}" "Listening on" || return 1
    printf 'hello' > "${second_port}"
    sleep "${RAW_SERIAL_SECONDS}"
    kill -INT "${example_process_id}"
    wait "${example_process_id}" || return $?

    # raw serial example must print the received bytes
    grep -q "Received: 0x68 'h'" "${log_file}" || return 1
}

# run the serial ping example on a port pair against the device simulator
run_serial_ping_example() {
    local program="$1"
    local log_file="$2"
    local first_port="${WORK_DIRECTORY}/serial_ping_ttyV1"
    local second_port="${WORK_DIRECTORY}/serial_ping_ttyV2"
    local simulator_log_file="${WORK_DIRECTORY}/serial_ping_simulator.log"
    start_port_pair "${first_port}" "${second_port}" || return 1
    "${BUILD_DIRECTORY}/sercom_device_sim" "${second_port}" > "${simulator_log_file}" 2>&1 &
    process_ids+=($!)
    wait_for_file "${simulator_log_file}" "Started the DeviceSimulator" || return 1
    timeout "${TIMEOUT_SECONDS}" "${program}" "${first_port}" > "${log_file}" 2>&1
}

# check whether socat is there for the examples with a serial port
socat_found=1
if ! command -v socat > /dev/null; then
    socat_found=0
fi

# run every example of the examples directory in the order of its number
failed_examples=()
skipped_examples=()
passed_count=0
for source_file in "${SOURCE_DIRECTORY}"/examples/[0-9][0-9]_*.cpp; do
    example_name="$(basename "${source_file}" .cpp)"
    program_name="sercom_example_${example_name}"
    program="${BUILD_DIRECTORY}/${program_name}"
    log_file="${WORK_DIRECTORY}/${program_name}.log"

    # program must be built
    if [[ ! -x "${program}" ]]; then
        echo "failed   ${program_name} (not found in ${BUILD_DIRECTORY})" >&2
        failed_examples+=("${program_name}")
        continue
    fi

    # select the runner of the example, the serial port examples need socat
    case "${example_name}" in
        01_raw_serial) runner=run_raw_serial_example ;;
        24_serial_ping) runner=run_serial_ping_example ;;
        *) runner=run_example ;;
    esac
    if [[ "${runner}" != "run_example" && "${socat_found}" == "0" ]]; then
        echo "skipped  ${program_name} (socat is not found)"
        skipped_examples+=("${program_name}")
        continue
    fi

    # run the example and keep its exit code
    exit_code=0
    "${runner}" "${program}" "${log_file}" || exit_code=$?
    if [[ "${exit_code}" != "0" ]]; then
        cat "${log_file}" >&2 2>/dev/null || true
        echo "failed   ${program_name} (exit code ${exit_code})" >&2
        failed_examples+=("${program_name}")
        continue
    fi
    echo "passed   ${program_name}"
    passed_count=$((passed_count + 1))
done

# notify the user
if [[ ${#failed_examples[@]} -gt 0 ]]; then
    echo "examples failed: $(IFS=,; echo "${failed_examples[*]}")" >&2
    exit 1
fi
echo "examples passed: ${passed_count}, skipped: ${#skipped_examples[@]}"
