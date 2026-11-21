# Overview

A robust, packet-based binary protocol for serial links, written in modern C++17,
with a Linux host toolset, firmware for the ESP32 (Arduino framework on FreeRTOS)
and for the STM32F4 (bare metal on the CMSIS headers), and a test suite that includes
noise injection, sanitizers, fuzzing and both firmware targets in QEMU.

This repository is the companion code of the course
**Packet-Based Binary Serial Communication with C++**.

```
A5 C3 | VERSION | FLAGS | MESSAGE_COUNTER | MESSAGE_TYPE | PAYLOAD_LENGTH | PAYLOAD | CRC32
 2 B      1 B      1 B        2 B              2 B             1 B          N B      4 B
```

All multi-byte fields are little-endian, payloads carry up to 255 bytes, and every
frame costs 13 bytes of overhead. CRC32 is the IEEE 802.3 / zlib CRC over
`VERSION` through the end of `PAYLOAD`. The full specification, including the
link layer (acknowledgements, retransmission, duplicate suppression), is in
[PROTOCOL.md](PROTOCOL.md).

## Layout

Every library keeps its headers in `include/` and the matching sources in `src/`,
and every file includes the others by a path relative to itself.

| Directory | Contents |
|---|---|
| `core/` | The protocol: CRC32, byte codec, frame, resynchronizing decoder, link layer and the frame tracer that decodes both directions for display. No OS calls, no heap, no exceptions; builds for PCs and microcontrollers. |
| `app/` | Demo device protocol on top of `core`: message types, `DeviceApplication` (device side) and `DeviceClient` (host side). Shared by the firmware and the host tools. |
| `firmware/common/` | The firmware of every target without platform calls, built in place by both PlatformIO projects and by CMake for the tests: `DeviceRole` (the device application with its frame tracer and activity LED), `ControllerRole` (the `DeviceClient` querying another device step by step), the readable `Console` behind a `ConsoleOutput`, `ActivityLed`, the `RingBuffer` of interrupt handlers, `CommonSettings` and the `build_settings.py` of both projects. |
| `host/` | Linux `SerialPort` (termios, RAII, `poll()`), utilities, the classes of the tools and, in `host/apps/`, the tools `sercom_cli`, `sercom_device_sim`, `sercom_dump`. |
| `firmware/esp32/` | PlatformIO project for ESP32 DevKit boards using `core`, `app` and `firmware/common` in place, with the FreeRTOS tasks, the UART, clock, platform and console adapters and the environments `esp32dev` (protocol on the USB port), `esp32dev-console` (protocol on UART2, readable console on the USB port) and `esp32dev-controller` (controller of a device on UART2, console on the USB port). |
| `firmware/stm32f4/` | Bare metal PlatformIO project for Nucleo-F401RE and Nucleo-F411RE boards: a superloop, SysTick clock, interrupt driven USARTs and the platform adapter, with device, console and controller environments and `netduinoplus2-qemu` for the STM32F405 of QEMU. |
| `examples/` | 25 small standalone programs in course order, from raw serial I/O to a ping over a serial port, each with a step-by-step guide; the index is [examples/README.md](../examples/README.md). |
| `tests/` | Catch2 unit and integration tests with their runner `tests/test_main.cpp`, among them the controller role against the device role over in-memory transports (`tests/test_ControllerRole.cpp`), the noise checklist, an end-to-end test over a noisy virtual link with a fake device for the boot wait (`tests/e2e/late_ping_device.py`) and the emulator test of both firmware targets (`tests/e2e/emulator_test.sh`). |
| `fuzz/` | libFuzzer targets for the decoder and the link layer with their runner `fuzz/run_fuzzers.sh`. |
| `tools/` | `noise.py` noise injector, `noisy_link.sh` virtual link, `sercom_frame.py` Python reference implementation, `check_versions.py` version comparison, `run_small_limit_test.sh` small payload limit run, `run_sanitizer_test.sh` sanitizer run, `run_examples.sh` example run, `run_emulator.sh` with `uart_bridge.py` for a firmware build in QEMU, `run_static_analysis.sh` with the layout and style checks, the analyzer configurations and the cppcheck installer in `tools/analysis/`. |
| `dep/` | Catch2 as a git submodule. |
| `docs/` | Protocol specification, the hardware bring-up of both targets in [HARDWARE.md](HARDWARE.md), the gateway design note [GATEWAY.md](GATEWAY.md), the coding style in [STYLE.md](STYLE.md) and this overview. |

## Build values

The protocol limits and the version numbers are the `set()` lines at the top of
`CMakeLists.txt`. They reach every target through the `sercom_settings` target and
are read once in `core/include/Settings.h` and `app/src/AppSettings.cpp`. The
firmware builds read the same `set()` lines through `firmware/common/build_settings.py`,
and the limits must match on both peers. Both builds take the build number from
`GITHUB_RUN_NUMBER` and the git hash from `GITHUB_SHA` or `git rev-parse HEAD`.

## Quality gates

| Command | What it checks |
|---|---|
| `build/sercom_test` | Every Catch2 test case in declaration order |
| `tools/run_small_limit_test.sh` | The same tests built with a 64 byte payload limit |
| `python3 tools/sercom_frame.py self-test` | The Python reference implementation |
| `tests/e2e/noisy_link_test.sh build/sercom_cli build/sercom_device_sim` | The tools against the device simulator over a noisy virtual link |
| `tools/run_examples.sh build` | Every example program exits with 0, examples 01 and 24 over socat pseudo terminals |
| `tools/run_sanitizer_test.sh` | The same tests with AddressSanitizer and UndefinedBehaviorSanitizer, the flags are passed from outside |
| `fuzz/run_fuzzers.sh` | libFuzzer targets `sercom_fuzz_frame_decoder` and `sercom_fuzz_link` for one minute each (Clang), the flags are passed from outside |
| `python3 tools/check_versions.py` | The version numbers of CMakeLists.txt and the three library.json files |
| `tools/run_static_analysis.sh build` | Layout, comments, countable idioms, cppcheck 2.22 (`tools/analysis/install_cppcheck.sh`) and clang-tidy 18 |
| `pio run -d firmware/esp32 -e esp32dev -e esp32dev-console -e esp32dev-controller` | The ESP32 environments with the same core, app and common firmware libraries |
| `pio run -d firmware/stm32f4 -e nucleo_f401re -e nucleo_f411re -e nucleo_f401re-console -e nucleo_f401re-controller -e netduinoplus2-qemu -e netduinoplus2-qemu-controller` | The STM32F4 environments with the same libraries and the house warnings with `-Werror` |
| `tests/e2e/emulator_test.sh build/sercom_cli firmware/esp32/.pio/build firmware/stm32f4/.pio/build` | Both targets in QEMU against the tools, the ESP32 controller against the STM32F4 device on a clean and a noisy link, and the STM32F4 controller against the ESP32 device |

All of them run in GitHub Actions, see [ci.yml](../.github/workflows/ci.yml).

## Noise tools

`tools/noise.py` and the socat wiring are documented in
[NOISE.md](../tools/NOISE.md) and [SOCAT.md](../tools/SOCAT.md).
