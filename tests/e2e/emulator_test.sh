#!/bin/bash

#
# emulator_test.sh
#
# Run the command line tool against both firmware targets in QEMU and let the two emulated chips talk to each other.
#
# The STM32F4 device of netduinoplus2-qemu and the ESP32 device of esp32dev must answer info, ping, LED, telemetry and
# an echo stress test, the stress test must verify every echo without a CRC mismatch and without a retransmission.
# A command line tool killed while it holds the port must leave the port free for the next one, as on a board.
# The ESP32 controller of esp32dev-controller must pass every step against the emulated STM32F4 device, once over a
# clean link and once through tools/noise.py with the realistic profile of tools/noisy_link.sh.
# The STM32F4 controller of netduinoplus2-qemu-controller must pass every step against the ESP32 device of
# esp32dev-console.
# tools/run_emulator.sh starts each chip, socat or two tools/noise.py processes wire their serial ports together.
#
# usage: tests/e2e/emulator_test.sh <sercom_cli> <ESP32 build directory> <STM32F4 build directory>
#

set -euo pipefail

# settings with overridable defaults
readonly SEED="${SEED:-1000}"
readonly CONTROLLER_TIMEOUT_SECONDS="${CONTROLLER_TIMEOUT_SECONDS:-90}"
readonly KILLED_TIMEOUT_SECONDS="${KILLED_TIMEOUT_SECONDS:-10}"
readonly KEPT_WORK_DIRECTORY="${WORK_DIRECTORY:-}"
SOURCE_DIRECTORY="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
readonly SOURCE_DIRECTORY

# print the usage text
print_usage() {
    echo "usage: tests/e2e/emulator_test.sh <sercom_cli> <ESP32 build directory> <STM32F4 build directory>"
    echo "  ESP32 build directory    firmware/esp32/.pio/build with esp32dev, esp32dev-console and"
    echo "                           esp32dev-controller"
    echo "  STM32F4 build directory  firmware/stm32f4/.pio/build with netduinoplus2-qemu and"
    echo "                           netduinoplus2-qemu-controller"
    echo "  SEED sets the noise seed (default ${SEED}), CONTROLLER_TIMEOUT_SECONDS the longest controller run"
    echo "  (default ${CONTROLLER_TIMEOUT_SECONDS}), KILLED_TIMEOUT_SECONDS the longest wait for the first answer of the"
    echo "  killed tool (default ${KILLED_TIMEOUT_SECONDS}), WORK_DIRECTORY keeps the logs in that directory, ESP32_QEMU"
    echo "  and ARM_QEMU select the emulators of tools/run_emulator.sh"
}

# check the arguments
if [[ $# -ge 1 && "$1" == "--help" ]]; then
    print_usage
    exit 0
fi
if [[ $# -lt 3 ]]; then
    echo "missing argument: <sercom_cli>, <ESP32 build directory> and <STM32F4 build directory> are required" >&2
    print_usage >&2
    exit 64
fi

# settings of the arguments
readonly CLI="$1"
readonly ESP32_BUILD_DIRECTORY="$2"
readonly STM32F4_BUILD_DIRECTORY="$3"
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

# start a firmware build in QEMU in its own run directory
start_emulator() {
    local target="$1"
    local build_directory="$2"
    local run_directory="$3"
    shift 3
    run_directories+=("${run_directory}")
    "${SOURCE_DIRECTORY}/tools/run_emulator.sh" start "${target}" "${build_directory}" "${run_directory}" "$@"
}

# stop the emulators and the link processes of one check
stop_check() {
    kill "${process_ids[@]}" 2>/dev/null || true
    process_ids=()
    local run_directory
    for run_directory in "${run_directories[@]}"; do
        "${SOURCE_DIRECTORY}/tools/run_emulator.sh" stop "${run_directory}"
    done
    run_directories=()
}

# run the commands of the command line tool against an emulated device and check their results
check_device() {
    local name="$1"
    local port="$2"
    local device_name="$3"
    local output="${WORK_DIRECTORY}/${name}.out"
    local killed_output="${WORK_DIRECTORY}/${name}-killed.out"
    : > "${output}"

    # run the commands, any failure stops the test
    local command
    local command_arguments
    for command in "info" "ping 3" "led on" "telemetry 200 1" "stress 300 32 2"; do
        echo "+ sercom_cli ${name} ${command}" | tee -a "${output}"
        read -r -a command_arguments <<< "${command}"
        "${CLI}" "${port}" "${command_arguments[@]}" >> "${output}" 2>&1
    done

    # kill a tool after its first answer, the next tool must open the port the killed one held
    echo "+ sercom_cli ${name} ping 100, killed after the first answer" | tee -a "${output}"
    "${CLI}" "${port}" ping 100 > "${killed_output}" 2>&1 &
    local killed_process_id=$!
    local attempt
    for ((attempt = 0; attempt < KILLED_TIMEOUT_SECONDS * 10; ++attempt)); do
        grep -aqE '^ping response: ' "${killed_output}" && break
        sleep 0.1
    done
    kill -KILL "${killed_process_id}"
    wait "${killed_process_id}" 2>/dev/null || true
    require_line "${killed_output}" '^ping response: '
    echo "+ sercom_cli ${name} ping 1" | tee -a "${output}"
    "${CLI}" "${port}" ping 1 >> "${output}" 2>&1

    # check the answers of the device
    require_line "${output}" "^device: ${device_name}$"
    require_line "${output}" '^ping response: round trip [0-9]+ ms'
    require_line "${output}" '^led: on$'
    require_line "${output}" '^telemetry #[0-9]+: '
    require_line "${output}" '^  verified replies +300$'
    require_line "${output}" '^  retransmissions +0$'
    require_line "${output}" '^  decoder CRC mismatches +0$'
    echo "${name} passed"
}

# wait until the console of a controller reports its result and require that every step passed
check_controller() {
    local name="$1"
    local console_file="$2"
    local second
    for ((second = 0; second < CONTROLLER_TIMEOUT_SECONDS; ++second)); do
        grep -aqE 'Controller (passed every step|failed in step)' "${console_file}" 2>/dev/null && break
        sleep 1
    done
    tr -d '\r' < "${console_file}" > "${WORK_DIRECTORY}/${name}.console"
    require_line "${WORK_DIRECTORY}/${name}.console" 'log DEBUG: Controller passed every step$'
    require_line "${WORK_DIRECTORY}/${name}.console" 'log DEBUG: Controller report: '
    grep -a 'Controller report: ' "${WORK_DIRECTORY}/${name}.console"
    echo "${name} passed"
}

# check the STM32F4 device and the ESP32 device with the command line tool
start_emulator stm32f4 "${STM32F4_BUILD_DIRECTORY}/netduinoplus2-qemu" "${WORK_DIRECTORY}/stm32f4-device" \
    --log usart1
check_device stm32f4-device "${WORK_DIRECTORY}/stm32f4-device/usart2" sercom-stm32f4
stop_check
start_emulator esp32 "${ESP32_BUILD_DIRECTORY}/esp32dev" "${WORK_DIRECTORY}/esp32-device" --log uart2
check_device esp32-device "${WORK_DIRECTORY}/esp32-device/uart0" sercom-esp32
stop_check

# check the ESP32 controller against the STM32F4 device over a clean link
start_emulator stm32f4 "${STM32F4_BUILD_DIRECTORY}/netduinoplus2-qemu" "${WORK_DIRECTORY}/clean-device" \
    --log usart1
start_emulator esp32 "${ESP32_BUILD_DIRECTORY}/esp32dev-controller" "${WORK_DIRECTORY}/clean-controller" \
    --log uart0
socat "${WORK_DIRECTORY}/clean-controller/uart2,raw,echo=0" "${WORK_DIRECTORY}/clean-device/usart2,raw,echo=0" &
process_ids+=($!)
check_controller esp32-controller-clean "${WORK_DIRECTORY}/clean-controller/uart0.log"
stop_check

# check the ESP32 controller against the STM32F4 device through the noise of both directions
start_emulator stm32f4 "${STM32F4_BUILD_DIRECTORY}/netduinoplus2-qemu" "${WORK_DIRECTORY}/noisy-device" \
    --log usart1
start_emulator esp32 "${ESP32_BUILD_DIRECTORY}/esp32dev-controller" "${WORK_DIRECTORY}/noisy-controller" \
    --log uart0
noise_arguments=(--drop 0.001 --flip 0.001 --insert 0.0005 --duplicate 0.0001 --maximum-bit-flips 1
                 --delay 0.0001 --burst 0.0001 --burst-length 5)
python3 -u "${SOURCE_DIRECTORY}/tools/noise.py" "${noise_arguments[@]}" --seed "${SEED}" \
    "${WORK_DIRECTORY}/noisy-controller/uart2" "${WORK_DIRECTORY}/noisy-device/usart2" &
process_ids+=($!)
python3 -u "${SOURCE_DIRECTORY}/tools/noise.py" "${noise_arguments[@]}" --seed "$((SEED + 1))" \
    "${WORK_DIRECTORY}/noisy-device/usart2" "${WORK_DIRECTORY}/noisy-controller/uart2" &
process_ids+=($!)
check_controller esp32-controller-noisy "${WORK_DIRECTORY}/noisy-controller/uart0.log"
stop_check

# check the STM32F4 controller against the ESP32 device over a clean link
start_emulator esp32 "${ESP32_BUILD_DIRECTORY}/esp32dev-console" "${WORK_DIRECTORY}/reverse-device" --log uart0
start_emulator stm32f4 "${STM32F4_BUILD_DIRECTORY}/netduinoplus2-qemu-controller" \
    "${WORK_DIRECTORY}/reverse-controller" --log usart2
socat "${WORK_DIRECTORY}/reverse-controller/usart1,raw,echo=0" "${WORK_DIRECTORY}/reverse-device/uart2,raw,echo=0" &
process_ids+=($!)
check_controller stm32f4-controller-clean "${WORK_DIRECTORY}/reverse-controller/usart2.log"
stop_check

# notify the user
echo "emulator test passed"
