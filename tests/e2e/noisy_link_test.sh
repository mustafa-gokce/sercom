#!/bin/bash

#
# noisy_link_test.sh
#
# Run the command line tool against the device simulator over a noisy virtual serial link.
#
# The link comes from tools/noisy_link.sh with a fixed seed, so a failing run can be replayed.
# Ping, info, LED and an echo stress test must succeed, the stress test must verify every echo.
# Each command meets a fresh simulator, because version 1 has no sessions (docs/PROTOCOL.md, section 7):
# a simulator that served an earlier tool process could take a retransmission for a duplicate.
# A traced ping must show the frames of both directions in the tool and in the simulator.
# Only its simulator traces, because a traced simulator writes every frame to its log on its one thread:
# a slow disk under the log would slow the device down, and the stress test must not measure the disk.
# A late answer to the boot ping of tests/e2e/late_ping_device.py must not count for the ping command,
# and a failed boot ping must not fail it.
# An info command on a serial port that does not exist must fail with exit code 1.
#
# usage: tests/e2e/noisy_link_test.sh <sercom_cli> <sercom_device_sim> [profile] [count]
#

set -euo pipefail

# settings with overridable defaults
readonly SEED="${SEED:-1000}"
readonly PAYLOAD_SIZE="${PAYLOAD_SIZE:-32}"
readonly WINDOW_SIZE="${WINDOW_SIZE:-2}"
SOURCE_DIRECTORY="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
readonly SOURCE_DIRECTORY

# print the usage text
print_usage() {
    echo "usage: tests/e2e/noisy_link_test.sh <sercom_cli> <sercom_device_sim> [profile] [count]"
    echo "  profile  noise profile of tools/noisy_link.sh (default realistic)"
    echo "  count    number of echo requests of the stress test (default 300)"
    echo "  SEED sets the noise seed (default ${SEED}), PAYLOAD_SIZE the echo payload size in bytes"
    echo "  (default ${PAYLOAD_SIZE}) and WINDOW_SIZE the echo requests in flight (default ${WINDOW_SIZE})"
}

# check the arguments
if [[ $# -ge 1 && "$1" == "--help" ]]; then
    print_usage
    exit 0
fi
if [[ $# -lt 2 ]]; then
    echo "missing argument: <sercom_cli> and <sercom_device_sim> are required" >&2
    print_usage >&2
    exit 64
fi

# settings of the arguments
readonly CLI="$1"
readonly SIMULATOR="$2"
readonly PROFILE="${3:-realistic}"
readonly COUNT="${4:-300}"
WORK_DIRECTORY="$(mktemp -d)"
readonly WORK_DIRECTORY

# stop every background process, the running simulator too, and remove the work directory on exit
process_ids=()
simulator_id=""
clean_up() {
    kill "${process_ids[@]}" 2>/dev/null || true
    if [[ -n "${simulator_id}" ]]; then
        kill "${simulator_id}" 2>/dev/null || true
    fi
    wait 2>/dev/null || true
    rm -rf "${WORK_DIRECTORY}"
}
trap clean_up EXIT

# wait until a log file holds a line, fail after five seconds
wait_for_line() {
    local log_file="$1"
    local text="$2"
    for _ in $(seq 1 100); do
        grep -q "${text}" "${log_file}" 2>/dev/null && return 0
        sleep 0.05
    done
    echo "${text} did not appear in ${log_file}" >&2
    return 1
}

# start the noisy link and wait until it is ready
"${SOURCE_DIRECTORY}/tools/noisy_link.sh" --profile "${PROFILE}" --dir "${WORK_DIRECTORY}" \
    --seed "${SEED}" > "${WORK_DIRECTORY}/link.log" 2>&1 &
process_ids+=($!)
wait_for_line "${WORK_DIRECTORY}/link.log" "Noisy link ready"

# require a line of a file, print the file and fail otherwise
require_line() {
    local file="$1"
    local pattern="$2"
    grep -qE -- "${pattern}" "${file}" && return 0
    echo "${pattern} did not appear in ${file}:" >&2
    cat "${file}" >&2
    return 1
}

# require a number of lines of a file, print the file and fail otherwise
require_line_count() {
    local file="$1"
    local pattern="$2"
    local count="$3"
    local found_count
    found_count="$(grep -cE -- "${pattern}" "${file}" || true)"
    [[ "${found_count}" == "${count}" ]] && return 0
    echo "${pattern} appeared ${found_count} times instead of ${count} in ${file}:" >&2
    cat "${file}" >&2
    return 1
}

# start a fresh device simulator with the given options and a log file of its own and wait until it is started
simulator_count=0
start_simulator() {
    simulator_count=$((simulator_count + 1))
    simulator_log_file="${WORK_DIRECTORY}/sim${simulator_count}.log"
    "${SIMULATOR}" "${WORK_DIRECTORY}/ttyV2" --name e2e-device "$@" > "${simulator_log_file}" 2>&1 &
    simulator_id=$!
    wait_for_line "${simulator_log_file}" "Started the DeviceSimulator"
}

# stop the device simulator and wait until it has closed its port
stop_simulator() {
    kill "${simulator_id}"
    wait "${simulator_id}"
    simulator_id=""
}

# run one command of the command line tool against a fresh device simulator
run() {
    start_simulator
    echo "+ sercom_cli $*"
    "${CLI}" "${WORK_DIRECTORY}/ttyV1" "$@"
    stop_simulator
}

# run the commands, any failure stops the test
run ping 3
run info
run led on
run led off
run stress "${COUNT}" "${PAYLOAD_SIZE}" "${WINDOW_SIZE}"

# run a traced ping against a fresh simulator and check the frames, the boot wait and the single ping response
start_simulator --trace
echo "+ sercom_cli --trace ping 1"
"${CLI}" "${WORK_DIRECTORY}/ttyV1" --trace ping 1 > "${WORK_DIRECTORY}/trace.out" 2> "${WORK_DIRECTORY}/trace.err"
stop_simulator
require_line "${WORK_DIRECTORY}/trace.out" '^\[ *[0-9]+\.[0-9]{3}\] >> #[0-9]+ +PING_REQUEST .*CRC ok$'
require_line "${WORK_DIRECTORY}/trace.out" '^\[ *[0-9]+\.[0-9]{3}\] << #[0-9]+ +PING_RESPONSE .*CRC ok$'
require_line_count "${WORK_DIRECTORY}/trace.out" '^ping response: ' 1
require_line "${WORK_DIRECTORY}/trace.err" 'Device answered after [0-9]+ ms'

# check the same frames in the opposite directions in the log of its simulator
require_line "${simulator_log_file}" '^\[ *[0-9]+\.[0-9]{3}\] >> #[0-9]+ +PING_REQUEST .*CRC ok$'
require_line "${simulator_log_file}" '^\[ *[0-9]+\.[0-9]{3}\] << #[0-9]+ +PING_RESPONSE .*CRC ok$'

# check in both modes of the late ping device that only the ping of the command counts
for mode in late failed; do
    python3 "${SOURCE_DIRECTORY}/tests/e2e/late_ping_device.py" "${WORK_DIRECTORY}/tty_${mode}" \
        --mode "${mode}" > "${WORK_DIRECTORY}/${mode}.log" 2>&1 &
    process_ids+=($!)
    wait_for_line "${WORK_DIRECTORY}/${mode}.log" "Late ping device ready"
    echo "+ sercom_cli late_ping_device --mode ${mode} ping 1"
    "${CLI}" "${WORK_DIRECTORY}/tty_${mode}" ping 1 > "${WORK_DIRECTORY}/${mode}.out"
    require_line "${WORK_DIRECTORY}/${mode}.out" '^ping response: .*device uptime 2222 ms$'
    require_line_count "${WORK_DIRECTORY}/${mode}.out" '^ping response: ' 1
done

# check that a missing serial port fails
echo "+ sercom_cli missing_port info"
missing_port_status=0
"${CLI}" "${WORK_DIRECTORY}/missing_port" info || missing_port_status=$?
if [[ "${missing_port_status}" != "1" ]]; then
    echo "a missing serial port gave exit code ${missing_port_status} instead of 1" >&2
    exit 1
fi

# notify the user
echo "e2e (${PROFILE}) passed"
