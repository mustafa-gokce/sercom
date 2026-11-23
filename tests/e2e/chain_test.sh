#!/bin/bash

#
# chain_test.sh
#
# Run the command line tool against a daisy chain of three device simulators over virtual serial links.
#
# Node 2 and node 3 are routers, node 4 is the end of the chain and node 3 stands in for it as its proxy.
# socat pseudo terminal pairs join the links, tools/noisy_link.sh adds noise with a fixed seed between node 2 and 3.
# discover must name every node, and --to 4 must reach node 4 for info, ping and a stress test without a loss.
# A ping of a missing node must fail as REJECTED / UNREACHABLE and one with two links to live as REJECTED / EXPIRED.
# A route failure report is fire and forget and the noise may drop it, then the host only sees a timeout.
# So these two pings run again while their report is lost, at most REPORT_ATTEMPTS times.
# A ping without --to must reach node 2, a wrong destination or time to live must fail with exit code 64.
# Node 4 restarted without an address must answer --to 4 info through the proxy of node 3.
# END_DEVICE_SIMULATOR may name another simulator for the restarted node 4, for example one of version 1.
#
# usage: tests/e2e/chain_test.sh <sercom_cli> <sercom_device_sim> [profile] [count]
#

set -euo pipefail

# settings with overridable defaults
readonly SEED="${SEED:-1000}"
readonly PAYLOAD_SIZE="${PAYLOAD_SIZE:-32}"
readonly WINDOW_SIZE="${WINDOW_SIZE:-2}"
readonly REPORT_ATTEMPTS="${REPORT_ATTEMPTS:-5}"
SOURCE_DIRECTORY="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
readonly SOURCE_DIRECTORY

# print the usage text
print_usage() {
    echo "usage: tests/e2e/chain_test.sh <sercom_cli> <sercom_device_sim> [profile] [count]"
    echo "  profile  noise profile of tools/noisy_link.sh between node 2 and node 3 (default realistic)"
    echo "  count    number of echo requests of the stress test (default 300)"
    echo "  SEED sets the noise seed (default ${SEED}), PAYLOAD_SIZE the echo payload size in bytes"
    echo "  (default ${PAYLOAD_SIZE}), WINDOW_SIZE the echo requests in flight (default ${WINDOW_SIZE}),"
    echo "  REPORT_ATTEMPTS the runs of a ping whose route failure report is lost (default ${REPORT_ATTEMPTS}) and"
    echo "  END_DEVICE_SIMULATOR the simulator of the restarted node 4 (default <sercom_device_sim>)"
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
readonly END_DEVICE_SIMULATOR="${END_DEVICE_SIMULATOR:-${SIMULATOR}}"
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

# require a line of a file, print the file and fail otherwise
require_line() {
    local file="$1"
    local pattern="$2"
    grep -qE -- "${pattern}" "${file}" && return 0
    echo "${pattern} did not appear in ${file}:" >&2
    cat "${file}" >&2
    return 1
}

# require that no line of a file matches, print the file and fail otherwise
require_no_line() {
    local file="$1"
    local pattern="$2"
    ! grep -qE -- "${pattern}" "${file}" && return 0
    echo "${pattern} appeared in ${file}:" >&2
    cat "${file}" >&2
    return 1
}

# start a socat pseudo terminal pair that connects two port links
start_port_pair() {
    local first_port="$1"
    local second_port="$2"
    socat PTY,raw,echo=0,link="${first_port}" PTY,raw,echo=0,link="${second_port}" &
    process_ids+=($!)
    for _ in $(seq 1 100); do
        [[ -e "${first_port}" && -e "${second_port}" ]] && return 0
        sleep 0.05
    done
    echo "${first_port} and ${second_port} did not appear" >&2
    return 1
}

# start a device simulator in the background and wait until it is started
start_simulator() {
    local simulator="$1"
    local log_file="$2"
    shift 2
    "${simulator}" "$@" > "${log_file}" 2>&1 &
    process_ids+=($!)
    simulator_process_id=$!
    wait_for_line "${log_file}" "Started the DeviceSimulator"
}

# start the links of the chain, the link between node 2 and node 3 with noise
readonly HOST_PORT="${WORK_DIRECTORY}/host"
readonly NODE2_PORT="${WORK_DIRECTORY}/node2"
readonly NODE3_DOWNSTREAM_PORT="${WORK_DIRECTORY}/node3_downstream"
readonly NODE4_PORT="${WORK_DIRECTORY}/node4"
start_port_pair "${HOST_PORT}" "${NODE2_PORT}"
start_port_pair "${NODE3_DOWNSTREAM_PORT}" "${NODE4_PORT}"
mkdir -p "${WORK_DIRECTORY}/noisy"
"${SOURCE_DIRECTORY}/tools/noisy_link.sh" --profile "${PROFILE}" --dir "${WORK_DIRECTORY}/noisy" \
    --seed "${SEED}" > "${WORK_DIRECTORY}/link.log" 2>&1 &
process_ids+=($!)
wait_for_line "${WORK_DIRECTORY}/link.log" "Noisy link ready"

# start the three nodes from the end of the chain up to node 2
start_simulator "${SIMULATOR}" "${WORK_DIRECTORY}/node4.log" "${NODE4_PORT}" --name node4 --address 4
node4_process_id="${simulator_process_id}"
start_simulator "${SIMULATOR}" "${WORK_DIRECTORY}/node3.log" "${WORK_DIRECTORY}/noisy/ttyV2" --name node3 \
    --address 3 --downstream "${NODE3_DOWNSTREAM_PORT}" --proxy 4
start_simulator "${SIMULATOR}" "${WORK_DIRECTORY}/node2.log" "${NODE2_PORT}" --name node2 --address 2 \
    --downstream "${WORK_DIRECTORY}/noisy/ttyV1"

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
        grep -qF -- "${report}" "${WORK_DIRECTORY}/${name}.err" && return 0
        require_line "${WORK_DIRECTORY}/${name}.err" 'Ping request timed out'
        echo "the route failure report was lost in attempt ${attempt} of ${REPORT_ATTEMPTS}"
    done
    echo "${report} did not arrive in ${REPORT_ATTEMPTS} attempts" >&2
    return 1
}

# discover every node of the chain
run discover discover
require_line "${WORK_DIRECTORY}/discover.out" '^node 2 answered after [0-9]+ ms$'
require_line "${WORK_DIRECTORY}/discover.out" '^node 3 answered after [0-9]+ ms$'
require_line "${WORK_DIRECTORY}/discover.out" '^node 4 answered after [0-9]+ ms$'
require_line "${WORK_DIRECTORY}/discover.out" '^discovered 3 nodes$'

# reach node 4 over three links for info, ping and the stress test
run info --to 4 info
require_line "${WORK_DIRECTORY}/info.out" '^device: node4$'
require_line "${WORK_DIRECTORY}/info.out" '^protocol version: 2$'
require_line "${WORK_DIRECTORY}/info.out" '^node address: 4$'
run ping --to 4 ping 5
require_line "${WORK_DIRECTORY}/ping.out" '^ping response from node 4: round trip [0-9]+ ms'
run stress --to 4 stress "${COUNT}" "${PAYLOAD_SIZE}" "${WINDOW_SIZE}"
require_line "${WORK_DIRECTORY}/stress.out" "^  verified replies +${COUNT}$"

# check the failure reports of a missing node and of a frame without enough time to live
run_reported unreachable 'Request PING_REQUEST failed: REJECTED / UNREACHABLE' --to 9 ping 1
run_reported expired 'Request PING_REQUEST failed: REJECTED / EXPIRED' --to 4 --ttl 2 ping 1

# reach node 2 without a destination and refuse wrong values
run direct ping 1
require_line "${WORK_DIRECTORY}/direct.out" '^ping response: round trip [0-9]+ ms'
run_failing host_destination 64 --to 1 ping 1
run_failing broadcast_destination 64 --to 255 ping 1
run_failing no_time_to_live 64 --to 4 --ttl 0 ping 1

# restart node 4 without an address behind the proxy of node 3
kill "${node4_process_id}"
wait "${node4_process_id}" 2>/dev/null || true
start_simulator "${END_DEVICE_SIMULATOR}" "${WORK_DIRECTORY}/node4_direct.log" "${NODE4_PORT}" --name node4-direct
run proxy --to 4 info
require_line "${WORK_DIRECTORY}/proxy.out" '^device: node4-direct$'
require_no_line "${WORK_DIRECTORY}/proxy.out" '^protocol version: '

# stop every process and check that none is left behind
clean_up
trap - EXIT
for process_id in "${process_ids[@]}"; do
    if kill -0 "${process_id}" 2>/dev/null; then
        echo "process ${process_id} is left behind" >&2
        exit 1
    fi
done

# notify the user
echo "chain (${PROFILE}) passed"
