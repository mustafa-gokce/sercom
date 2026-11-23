#!/bin/bash

#
# emulator_chain_test.sh
#
# Run the command line tool against a daisy chain of three emulated chips in QEMU.
#
# Node 2 is the ESP32 router of esp32dev-router: the host on UART0, the next node on UART2.
# Node 3 is the STM32F4 router of netduinoplus2-qemu-router: node 2 on USART1, node 4 on USART6, the console on USART2
# and the proxy of node 4.
# Node 4 is the ESP32 end node of esp32dev-chain: node 3 on UART2, the console on UART0.
# tools/noise.py --duplex adds the noise of the realistic profile with a fixed seed between node 2 and node 3, socat
# joins node 3 and node 4.
# discover must name every node, and --to 4 must reach node 4 for info, ping and a stress test without a loss.
# A ping of a missing node must fail as REJECTED / UNREACHABLE and one with two links to live as REJECTED / EXPIRED.
# A route failure report is fire and forget and the noise may drop it, then the host only sees a timeout.
# So these two pings run again while their report is lost, at most REPORT_ATTEMPTS times.
# A ping without --to must reach node 2.
# sercom_dump decodes the port logs of QEMU: the requests of the host leave node 2 with a time to live of 7 and node 3
# with 6, the responses of node 4 leave node 4 with 8, node 3 with 7 and node 2 with 6.
# Node 4 replaced by the device firmware esp32dev-console without an address must answer --to 4 info and --to 4 ping
# through the proxy of node 3.
# END_DEVICE_BUILD_DIRECTORY may name another build for the replaced node 4, for example one of version 1.
# Only the link of node 2 and node 3 carries noise and neither of them restarts, so a duplicate history never meets the
# message counters of an earlier session there. Every command of the host and the replaced node 4 start new sessions
# on the clean links to node 2 and to node 3, which lose no frame, so no retransmission of them can collide with the
# history of the session before.
#
# usage: tests/e2e/emulator_chain_test.sh <sercom_cli> <sercom_dump> <ESP32 build directory> <STM32F4 build directory>
#

set -euo pipefail

# settings with overridable defaults
readonly SEED="${SEED:-1000}"
readonly COUNT="${COUNT:-300}"
readonly PAYLOAD_SIZE="${PAYLOAD_SIZE:-32}"
readonly WINDOW_SIZE="${WINDOW_SIZE:-2}"
readonly REPORT_ATTEMPTS="${REPORT_ATTEMPTS:-5}"
readonly LINK_TIMEOUT_SECONDS="${LINK_TIMEOUT_SECONDS:-10}"
readonly STOP_TIMEOUT_SECONDS="${STOP_TIMEOUT_SECONDS:-10}"
readonly KEPT_WORK_DIRECTORY="${WORK_DIRECTORY:-}"
SOURCE_DIRECTORY="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
readonly SOURCE_DIRECTORY

# print the usage text
print_usage() {
    echo "usage: tests/e2e/emulator_chain_test.sh <sercom_cli> <sercom_dump> <ESP32 build directory>"
    echo "       <STM32F4 build directory>"
    echo "  ESP32 build directory    firmware/esp32/.pio/build with esp32dev-router, esp32dev-chain and"
    echo "                           esp32dev-console"
    echo "  STM32F4 build directory  firmware/stm32f4/.pio/build with netduinoplus2-qemu-router"
    echo "  SEED sets the noise seed (default ${SEED}), COUNT the echo requests of the stress test (default ${COUNT}),"
    echo "  PAYLOAD_SIZE the echo payload size in bytes (default ${PAYLOAD_SIZE}), WINDOW_SIZE the echo requests in"
    echo "  flight (default ${WINDOW_SIZE}), REPORT_ATTEMPTS the runs of a ping whose route failure report is lost"
    echo "  (default ${REPORT_ATTEMPTS}), END_DEVICE_BUILD_DIRECTORY the build of the replaced node 4 (default"
    echo "  <ESP32 build directory>/esp32dev-console), LINK_TIMEOUT_SECONDS the longest wait for a link process to"
    echo "  open its ports (default ${LINK_TIMEOUT_SECONDS}), STOP_TIMEOUT_SECONDS the longest wait for the stopped"
    echo "  processes (default ${STOP_TIMEOUT_SECONDS}), WORK_DIRECTORY keeps the logs in that directory, ESP32_QEMU"
    echo "  and ARM_QEMU select the emulators of tools/run_emulator.sh"
}

# check the arguments
if [[ $# -ge 1 && "$1" == "--help" ]]; then
    print_usage
    exit 0
fi
if [[ $# -lt 4 ]]; then
    echo "missing argument: <sercom_cli>, <sercom_dump>, <ESP32 build directory> and <STM32F4 build directory> are" \
        "required" >&2
    print_usage >&2
    exit 64
fi

# settings of the arguments
readonly CLI="$1"
readonly DUMP="$2"
readonly ESP32_BUILD_DIRECTORY="$3"
readonly STM32F4_BUILD_DIRECTORY="$4"
readonly END_DEVICE_BUILD_DIRECTORY="${END_DEVICE_BUILD_DIRECTORY:-${ESP32_BUILD_DIRECTORY}/esp32dev-console}"
if [[ -n "${KEPT_WORK_DIRECTORY}" ]]; then
    mkdir -p "${KEPT_WORK_DIRECTORY}"
    WORK_DIRECTORY="$(cd "${KEPT_WORK_DIRECTORY}" && pwd)"
else
    WORK_DIRECTORY="$(mktemp -d)"
fi
readonly WORK_DIRECTORY

# stop every emulator and link process and remove a temporary work directory on exit
run_directories=()
process_ids=()
clean_up() {
    kill "${process_ids[@]}" 2>/dev/null || true
    local run_directory
    for run_directory in "${run_directories[@]}"; do
        "${SOURCE_DIRECTORY}/tools/run_emulator.sh" stop "${run_directory}"
    done
    wait 2>/dev/null || true
    [[ -n "${KEPT_WORK_DIRECTORY}" ]] || rm -rf "${WORK_DIRECTORY}"
}
trap clean_up EXIT

# require a line of a file, print the file and fail otherwise
require_line() {
    local file="$1"
    local pattern="$2"
    grep -aqE -- "${pattern}" "${file}" && return 0
    echo "${pattern} did not appear in ${file}:" >&2
    cat "${file}" >&2
    return 1
}

# require that no line of a file matches, print the file and fail otherwise
require_no_line() {
    local file="$1"
    local pattern="$2"
    ! grep -aqE -- "${pattern}" "${file}" && return 0
    echo "${pattern} appeared in ${file}:" >&2
    cat "${file}" >&2
    return 1
}

# start a firmware build in QEMU in its own run directory
start_emulator() {
    local target="$1"
    local build_directory="$2"
    local run_directory="$3"
    shift 3
    run_directories+=("${run_directory}")
    "${SOURCE_DIRECTORY}/tools/run_emulator.sh" start "${target}" "${build_directory}" "${run_directory}" "$@"
}

# wait until a link process holds the pseudo terminals of its ports open, the bridges drop the bytes before
wait_for_link() {
    local process_id="$1"
    shift
    local port
    local terminal
    local attempt
    for port in "$@"; do
        terminal="$(readlink -f "${port}")"
        for ((attempt = 0; attempt < LINK_TIMEOUT_SECONDS * 10; ++attempt)); do
            find "/proc/${process_id}/fd" -lname "${terminal}" 2>/dev/null | grep -q . && break
            sleep 0.1
        done
        if ! find "/proc/${process_id}/fd" -lname "${terminal}" 2>/dev/null | grep -q .; then
            echo "process ${process_id} did not open ${port} within ${LINK_TIMEOUT_SECONDS} seconds" >&2
            return 1
        fi
    done
}

# join two port links with socat in the background, keep its process id and wait until it holds both ports
start_link() {
    local first_port="$1"
    local second_port="$2"
    socat "${first_port},raw,echo=0" "${second_port},raw,echo=0" &
    process_ids+=($!)
    link_process_id=$!
    wait_for_link "${link_process_id}" "${first_port}" "${second_port}"
}

# run one command of the command line tool with its output in files and get its exit code
run_command() {
    local name="$1"
    shift
    echo "+ sercom_cli $*"
    "${CLI}" "${HOST_PORT}" "$@" > "${WORK_DIRECTORY}/${name}.out" 2> "${WORK_DIRECTORY}/${name}.err"
}

# run one command that must succeed, print its output and fail otherwise
run() {
    local name="$1"
    run_failing "${name}" 0 "${@:2}"
}

# run a command that must end with an exit code, print its output and fail otherwise
run_failing() {
    local name="$1"
    local expected_status="$2"
    shift 2
    local status=0
    run_command "${name}" "$@" || status=$?
    if [[ "${status}" != "${expected_status}" ]]; then
        echo "sercom_cli $* gave exit code ${status} instead of ${expected_status}:" >&2
        cat "${WORK_DIRECTORY}/${name}.out" "${WORK_DIRECTORY}/${name}.err" >&2
        return 1
    fi
}

# run a ping that must fail with a route failure report, again while the noise drops the report
run_reported() {
    local name="$1"
    local report="$2"
    shift 2
    local attempt
    for attempt in $(seq 1 "${REPORT_ATTEMPTS}"); do
        run_failing "${name}" 1 "$@"
        grep -aqF -- "${report}" "${WORK_DIRECTORY}/${name}.err" && return 0
        require_line "${WORK_DIRECTORY}/${name}.err" 'Ping request timed out'
        echo "the route failure report was lost in attempt ${attempt} of ${REPORT_ATTEMPTS}"
    done
    echo "${report} did not arrive in ${REPORT_ATTEMPTS} attempts" >&2
    return 1
}

# decode the port log of a node and require a frame of a type from a source to a destination with a time to live
require_hop() {
    local node="$1"
    local port="$2"
    local message_type="$3"
    local source_address="$4"
    local destination_address="$5"
    local time_to_live="$6"
    local dump_file="${WORK_DIRECTORY}/${node}-${port}.dump"
    "${DUMP}" "${WORK_DIRECTORY}/${node}/${port}.log" > "${dump_file}"
    local pattern=" ${message_type} .* source=${source_address} destination=${destination_address} ttl=${time_to_live}$"
    require_line "${dump_file}" "${pattern}"
    echo "${node} ${port}: ${message_type} source=${source_address} destination=${destination_address}" \
        "ttl=${time_to_live}"
}

# start the three nodes from the end of the chain up to node 2 and join their ports
readonly HOST_PORT="${WORK_DIRECTORY}/node2/uart0"
start_emulator esp32 "${ESP32_BUILD_DIRECTORY}/esp32dev-chain" "${WORK_DIRECTORY}/node4" --log uart0
start_emulator stm32f4 "${STM32F4_BUILD_DIRECTORY}/netduinoplus2-qemu-router" "${WORK_DIRECTORY}/node3" --log usart2
start_link "${WORK_DIRECTORY}/node3/usart6" "${WORK_DIRECTORY}/node4/uart2"
node4_link_process_id="${link_process_id}"
start_emulator esp32 "${ESP32_BUILD_DIRECTORY}/esp32dev-router" "${WORK_DIRECTORY}/node2"
noise_arguments=(--drop 0.001 --flip 0.001 --insert 0.0005 --duplicate 0.0001 --maximum-bit-flips 1
                 --delay 0.0001 --burst 0.0001 --burst-length 5)
python3 -u "${SOURCE_DIRECTORY}/tools/noise.py" "${noise_arguments[@]}" --seed "${SEED}" --duplex \
    "${WORK_DIRECTORY}/node2/uart2" "${WORK_DIRECTORY}/node3/usart1" > "${WORK_DIRECTORY}/noise.log" 2>&1 &
process_ids+=($!)
wait_for_link $! "${WORK_DIRECTORY}/node2/uart2" "${WORK_DIRECTORY}/node3/usart1"

# discover every node of the chain
run discover discover
require_line "${WORK_DIRECTORY}/discover.out" '^node 2 answered after [0-9]+ ms$'
require_line "${WORK_DIRECTORY}/discover.out" '^node 3 answered after [0-9]+ ms$'
require_line "${WORK_DIRECTORY}/discover.out" '^node 4 answered after [0-9]+ ms$'
require_line "${WORK_DIRECTORY}/discover.out" '^discovered 3 nodes$'
grep -aE '^(node [0-9]+ answered|discovered)' "${WORK_DIRECTORY}/discover.out"

# reach node 4 over three links for info, ping and the stress test
run info --to 4 info
require_line "${WORK_DIRECTORY}/info.out" '^device: sercom-esp32$'
require_line "${WORK_DIRECTORY}/info.out" '^protocol version: 2$'
require_line "${WORK_DIRECTORY}/info.out" '^node address: 4$'
run ping --to 4 ping 5
require_line "${WORK_DIRECTORY}/ping.out" '^ping response from node 4: round trip [0-9]+ ms'
run stress --to 4 stress "${COUNT}" "${PAYLOAD_SIZE}" "${WINDOW_SIZE}"
require_line "${WORK_DIRECTORY}/stress.out" "^  verified replies +${COUNT}$"
grep -aE '^  (verified replies|busy retries|overdue retries|retransmissions) ' "${WORK_DIRECTORY}/stress.out"

# check the failure reports of a missing node and of a frame without enough time to live
run_reported unreachable 'Request PING_REQUEST failed: REJECTED / UNREACHABLE' --to 9 ping 1
run_reported expired 'Request PING_REQUEST failed: REJECTED / EXPIRED' --to 4 --ttl 2 ping 1

# reach node 2 without a destination
run direct ping 1
require_line "${WORK_DIRECTORY}/direct.out" '^ping response: round trip [0-9]+ ms'

# check the time to live of every hop in the port logs of QEMU
require_hop node2 uart2 PING_REQUEST 1 4 7
require_hop node3 usart6 PING_REQUEST 1 4 6
require_hop node4 uart2 PING_RESPONSE 4 1 8
require_hop node3 usart1 PING_RESPONSE 4 1 7
require_hop node2 uart0 PING_RESPONSE 4 1 6

# check the router heartbeat on the console of node 3
readonly NODE3_CONSOLE="${WORK_DIRECTORY}/node3.console"
tr -d '\r' < "${WORK_DIRECTORY}/node3/usart2.log" > "${NODE3_CONSOLE}"
require_line "${NODE3_CONSOLE}" 'log DEBUG: Routing as node 3 between USART1 and USART6, proxy address 4$'
require_line "${NODE3_CONSOLE}" 'log DEBUG: Heartbeat: forwarded [1-9][0-9]*, expired [0-9]+, unreachable [0-9]+'
grep -a 'Heartbeat: forwarded ' "${NODE3_CONSOLE}" | tail -1 > "${WORK_DIRECTORY}/node3.heartbeat"
cat "${WORK_DIRECTORY}/node3.heartbeat"
require_line "${WORK_DIRECTORY}/node3.heartbeat" ', dropped secondary bytes 0$'

# replace node 4 by a device without an address behind the proxy of node 3
kill "${node4_link_process_id}" 2>/dev/null || true
wait "${node4_link_process_id}" 2>/dev/null || true
"${SOURCE_DIRECTORY}/tools/run_emulator.sh" stop "${WORK_DIRECTORY}/node4"
start_emulator esp32 "${END_DEVICE_BUILD_DIRECTORY}" "${WORK_DIRECTORY}/node4-direct" --log uart0
start_link "${WORK_DIRECTORY}/node3/usart6" "${WORK_DIRECTORY}/node4-direct/uart2"
run proxy_info --to 4 info
require_line "${WORK_DIRECTORY}/proxy_info.out" '^device: sercom-esp32$'
require_no_line "${WORK_DIRECTORY}/proxy_info.out" '^protocol version: '
run proxy_ping --to 4 ping 3
require_line "${WORK_DIRECTORY}/proxy_ping.out" '^ping response from node 4: round trip [0-9]+ ms'

# stop every process and check that none is left behind
stopped_process_ids=("${process_ids[@]}")
for run_directory in "${run_directories[@]}"; do
    for process_id_file in "${run_directory}"/*.pid; do
        [[ -f "${process_id_file}" ]] && stopped_process_ids+=("$(cat "${process_id_file}")")
    done
done
clean_up
trap - EXIT
for process_id in "${stopped_process_ids[@]}"; do
    for ((attempt = 0; attempt < STOP_TIMEOUT_SECONDS * 10; ++attempt)); do
        kill -0 "${process_id}" 2>/dev/null || break
        sleep 0.1
    done
    if kill -0 "${process_id}" 2>/dev/null; then
        echo "process ${process_id} is left behind" >&2
        exit 1
    fi
done

# notify the user
echo "emulator chain test passed"
