#!/bin/bash

#
# run_emulator.sh
#
# Start a firmware build in QEMU with its serial ports as pseudo terminals, or stop it again.
#
# The esp32 target boots a merged flash image of an ESP32 build directory on Espressif's QEMU machine esp32.
# The stm32f4 target boots the firmware.elf of an STM32F4 build directory on the QEMU machine netduinoplus2.
# Every serial port becomes a link to its pseudo terminal in the run directory: uart0 and uart2, or usart1, usart2 and
# usart6, the secondary port of a router.
# QEMU serves each port on a unix socket and tools/uart_bridge.py gives it a pseudo terminal like a USB bridge.
# Both run inside the run directory and name the socket relative to it, because a socket path holds at most 107 bytes.
# QEMU also writes the output of every port into <port>.log of the run directory, from the first byte of the chip on.
# The port named by --log writes only into its log file and gets no pseudo terminal.
# start returns once a port log holds the start line of the firmware, so the first command finds a running chip.
# The emulated ESP32 watchdogs are off, because the emulation is slower than the chip and would trip them.
# QEMU and every bridge write their process ids into the run directory and stop ends only those processes.
#
# usage: tools/run_emulator.sh start <esp32|stm32f4> <build directory> <run directory> [--log PORT]
#        tools/run_emulator.sh stop <run directory>
#

set -euo pipefail

# settings with overridable defaults
SOURCE_DIRECTORY="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
readonly SOURCE_DIRECTORY
readonly ESP32_QEMU="${ESP32_QEMU:-qemu-system-xtensa}"
readonly ARM_QEMU="${ARM_QEMU:-qemu-system-arm}"
readonly PLATFORMIO_PACKAGES="${PLATFORMIO_PACKAGES:-${HOME}/.platformio/packages}"
readonly PYTHON="${PYTHON:-python3}"
readonly START_TIMEOUT_SECONDS="${START_TIMEOUT_SECONDS:-10}"
readonly BOOT_TIMEOUT_SECONDS="${BOOT_TIMEOUT_SECONDS:-60}"
readonly STARTED_TEXT="${STARTED_TEXT:-Started the }"

# print the usage text
print_usage() {
    echo "usage: tools/run_emulator.sh start <esp32|stm32f4> <build directory> <run directory> [--log PORT]"
    echo "       tools/run_emulator.sh stop <run directory>"
    echo "  build directory  firmware/esp32/.pio/build/<environment> or firmware/stm32f4/.pio/build/<environment>"
    echo "  run directory    directory of the port links, the log files, the flash image and the process ids"
    echo "  --log PORT       give the port uart0, uart2, usart1, usart2 or usart6 no pseudo terminal, only"
    echo "                   <run directory>/PORT.log"
    echo "  ESP32_QEMU selects qemu-system-xtensa (default ${ESP32_QEMU}), ARM_QEMU qemu-system-arm"
    echo "  (default ${ARM_QEMU}), PLATFORMIO_PACKAGES the PlatformIO packages (default ${PLATFORMIO_PACKAGES})"
    echo "  BOOT_TIMEOUT_SECONDS sets the longest wait for the start line of the firmware, 0 skips the wait"
    echo "  (default ${BOOT_TIMEOUT_SECONDS})"
}

# append the QEMU options of a serial port to serial_options, a socket for its bridge or the log file alone
add_serial_options() {
    local port="$1"
    rm -f "${RUN_DIRECTORY:?}/${port:?}.log"
    port_logs+=("${RUN_DIRECTORY}/${port}.log")
    if [[ "${port}" == "${LOG_PORT}" ]]; then
        serial_options+=(-serial "file:${port}.log")
        return
    fi
    rm -f "${RUN_DIRECTORY}/${port}.sock"
    serial_options+=(-chardev "socket,id=${port},path=${port}.sock,server=on,wait=off,logfile=${port}.log")
    serial_options+=(-serial "chardev:${port}")
    bridged_ports+=("${port}")
}

# start a bridge for every socket port and wait for the links of their pseudo terminals
start_bridges() {
    local port
    local attempt
    for port in "${bridged_ports[@]}"; do
        # wait for QEMU to serve the socket
        for ((attempt = 0; attempt < START_TIMEOUT_SECONDS * 10; ++attempt)); do
            [[ -S "${RUN_DIRECTORY}/${port}.sock" ]] && break
            sleep 0.1
        done
        if [[ ! -S "${RUN_DIRECTORY}/${port}.sock" ]]; then
            echo "QEMU serves no socket for ${port}, see ${RUN_DIRECTORY}/qemu.log" >&2
            exit 1
        fi

        # give the port its pseudo terminal
        rm -f "${RUN_DIRECTORY}/${port}"
        (cd "${RUN_DIRECTORY}" && exec "${PYTHON}" -I "${SOURCE_DIRECTORY}/tools/uart_bridge.py" "${port}.sock" \
            "${port}" > "bridge-${port}.log" 2>&1 < /dev/null) &
        echo $! > "${RUN_DIRECTORY}/bridge-${port}.pid"
        for ((attempt = 0; attempt < START_TIMEOUT_SECONDS * 10; ++attempt)); do
            [[ -L "${RUN_DIRECTORY}/${port}" ]] && break
            sleep 0.1
        done
        echo "${port}: $(readlink "${RUN_DIRECTORY}/${port}")"
    done
}

# wait until a port log holds the start line of the firmware, as long as QEMU runs
wait_for_firmware() {
    local attempt
    if [[ "${BOOT_TIMEOUT_SECONDS}" -eq 0 ]]; then
        return
    fi
    for ((attempt = 0; attempt < BOOT_TIMEOUT_SECONDS * 10; ++attempt)); do
        grep -aqF -- "${STARTED_TEXT}" "${port_logs[@]}" 2>/dev/null && return
        if ! kill -0 "$(cat "${RUN_DIRECTORY}/qemu.pid" 2>/dev/null)" 2>/dev/null; then
            echo "QEMU stopped before the firmware started, see ${RUN_DIRECTORY}/qemu.log" >&2
            exit 1
        fi
        sleep 0.1
    done
    echo "no port log in ${RUN_DIRECTORY} holds the start line of the firmware after ${BOOT_TIMEOUT_SECONDS} seconds" >&2
    exit 1
}

# boot an ESP32 build directory on Espressif's QEMU
start_esp32() {
    # merge the bootloader, the partition table, the boot selector and the application into one flash image
    "${PYTHON}" "${PLATFORMIO_PACKAGES}/tool-esptoolpy/esptool.py" --chip esp32 merge_bin \
        -o "${RUN_DIRECTORY}/flash.bin" --flash_mode dio --flash_size 4MB --fill-flash-size 4MB \
        0x1000 "${BUILD_DIRECTORY}/bootloader.bin" 0x8000 "${BUILD_DIRECTORY}/partitions.bin" \
        0xe000 "${PLATFORMIO_PACKAGES}/framework-arduinoespressif32/tools/partitions/boot_app0.bin" \
        0x10000 "${BUILD_DIRECTORY}/firmware.bin" > "${RUN_DIRECTORY}/merge.log"

    # boot the flash image with UART0, UART1 and UART2 in this order, UART1 stays unconnected
    add_serial_options uart0
    serial_options+=(-serial null)
    add_serial_options uart2
    (cd "${RUN_DIRECTORY}" && exec "${ESP32_QEMU}" -nographic -machine esp32 -monitor none \
        -drive file=flash.bin,if=mtd,format=raw \
        -global driver=timer.esp32.timg,property=wdt_disable,value=true \
        "${serial_options[@]}" -pidfile qemu.pid > qemu.log 2>&1 < /dev/null) &
}

# boot an STM32F4 build directory on the QEMU machine netduinoplus2
start_stm32f4() {
    # boot the image with USART1, USART2, the three unconnected ports USART3, UART4 and UART5, then USART6
    add_serial_options usart1
    add_serial_options usart2
    serial_options+=(-serial null -serial null -serial null)
    add_serial_options usart6
    (cd "${RUN_DIRECTORY}" && exec "${ARM_QEMU}" -nographic -machine netduinoplus2 -monitor none \
        -kernel "${BUILD_DIRECTORY}/firmware.elf" \
        "${serial_options[@]}" -pidfile qemu.pid > qemu.log 2>&1 < /dev/null) &
}

# end a process of a process id file of the run directory
stop_process() {
    local process_id_file="$1"
    if [[ -f "${process_id_file}" ]]; then
        kill "$(cat "${process_id_file}")" 2>/dev/null || true
        rm -f "${process_id_file}"
    fi
}

# check the arguments
if [[ $# -ge 1 && "$1" == "--help" ]]; then
    print_usage
    exit 0
fi
if [[ $# -eq 2 && "$1" == "stop" ]]; then
    # stop QEMU and the bridges of the run directory
    stop_process "$2/qemu.pid"
    for bridge_process_id_file in "$2"/bridge-*.pid; do
        stop_process "${bridge_process_id_file}"
    done
    exit 0
fi
if [[ $# -lt 4 || "$1" != "start" ]]; then
    echo "missing argument: start <target> <build directory> <run directory> or stop <run directory>" >&2
    print_usage >&2
    exit 64
fi

# settings of the arguments
readonly TARGET="$2"
BUILD_DIRECTORY="$(cd "$3" && pwd)"
readonly BUILD_DIRECTORY
mkdir -p "$4"
RUN_DIRECTORY="$(cd "$4" && pwd)"
readonly RUN_DIRECTORY
shift 4
LOG_PORT=""
while [[ $# -gt 0 ]]; do
    case "$1" in
        --log) LOG_PORT="$2"; shift 2 ;;
        *) echo "unknown option: $1" >&2; print_usage >&2; exit 64 ;;
    esac
done
readonly LOG_PORT

# an emulator of an earlier start must be stopped first
if [[ -f "${RUN_DIRECTORY}/qemu.pid" ]] && kill -0 "$(cat "${RUN_DIRECTORY}/qemu.pid")" 2>/dev/null; then
    echo "QEMU still runs for ${RUN_DIRECTORY}, stop it first" >&2
    exit 1
fi

# boot the firmware of the target
serial_options=()
bridged_ports=()
port_logs=()
case "${TARGET}" in
    esp32) start_esp32 ;;
    stm32f4) start_stm32f4 ;;
    *) echo "unknown target: ${TARGET}" >&2; print_usage >&2; exit 64 ;;
esac

# give the ports their pseudo terminals and wait for the firmware
start_bridges
wait_for_firmware
echo "QEMU runs ${TARGET} with process id $(cat "${RUN_DIRECTORY}/qemu.pid") in ${RUN_DIRECTORY}"
