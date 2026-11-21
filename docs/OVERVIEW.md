# Overview

A robust, packet-based binary protocol for serial links, written in modern C++17,
with a Qt 5.12.12 host toolset for Linux and Windows, ESP32 firmware (Arduino
framework on FreeRTOS) and a test suite that includes noise injection, sanitizers
and fuzzing.

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
| `host/` | The Qt 5.12.12 host: `SerialPort` on `QSerialPort` with the received data as a signal, utilities with the `QElapsedTimer` clock and the Qt log, the classes of the tools on the `QCoreApplication` event loop and, in `host/apps/`, the tools `sercom_cli`, `sercom_device_sim`, `sercom_dump`. The same code builds on Linux with GCC and on Windows with the MinGW 7.3.0 of Qt, without a platform branch. |
| `firmware/esp32/` | PlatformIO project for ESP32 DevKit boards using `core` and `app` in place, with the environments `esp32dev` (protocol on the USB port) and `esp32dev-console` (protocol on UART2, readable console on the USB port). |
| `examples/` | 25 small standalone programs in course order, from raw serial I/O on `QSerialPort` to a ping over a serial port, each with a step-by-step guide; the index is [examples/README.md](../examples/README.md). |
| `tests/` | Catch2 unit and integration tests with their runner `tests/test_main.cpp` inside a `QCoreApplication`, the noise checklist and an end-to-end test over a noisy virtual link with a fake device for the boot wait (`tests/e2e/late_ping_device.py`), for Linux in Bash and for Windows in PowerShell. |
| `fuzz/` | libFuzzer targets for the decoder and the link layer with their runner `fuzz/run_fuzzers.sh`. |
| `tools/` | `noise.py` noise injector for pseudo terminals and COM ports, `noisy_link.sh` and `noisy_link.ps1` virtual links, `sercom_frame.py` Python reference implementation, `check_versions.py` version comparison, `run_small_limit_test.sh` small payload limit run, `run_sanitizer_test.sh` sanitizer run, `run_examples.sh` and `run_examples.ps1` example runs, `run_static_analysis.sh` with the layout and style checks, the analyzer configurations and the cppcheck installer in `tools/analysis/`. |
| `dep/` | Catch2 as a git submodule. Qt 5.12.12 is installed apart, for example with aqtinstall as the README shows. |
| `docs/` | Protocol specification, the hardware bring-up in [HARDWARE.md](HARDWARE.md), the coding style in [STYLE.md](STYLE.md) and this overview. |

## Qt and the platforms

The host targets need Qt 5.12.12 exactly, with QtCore, QtSerialPort and, for the
tests, QtTest. CMake finds it through `CMAKE_PREFIX_PATH` and stops with a message
when it finds another version. `core/` and `app/` never see Qt, because they also
run on the microcontrollers. The same `CMakeLists.txt` builds on Linux and on Windows;
its only platform check runs `windeployqt` after the Windows build, which copies the
Qt and MinGW DLLs next to the programs (dynamic Qt under the LGPL). The serial port
names are `/dev/ttyUSB0` or a pseudo terminal such as `/tmp/ttyV1` on Linux and
`COM3` on Windows; `QSerialPort` takes both. On Linux a name without a leading `/` or
`./` names a device in `/dev`, so `ttyUSB0` works and a pseudo terminal link in the
current directory needs `./`.

## Build values

The protocol limits and the version numbers are the `set()` lines at the top of
`CMakeLists.txt`. They reach every target through the `sercom_settings` target and
are read once in `core/include/Settings.h` and `app/src/AppSettings.cpp`. The
firmware build reads the same `set()` lines through `firmware/esp32/build_settings.py`,
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
| `tools/run_examples.ps1 build` | The same on Windows, examples 01 and 24 over a com0com pair |
| `tests/e2e/noisy_link_test.ps1 build/sercom_cli.exe build/sercom_device_sim.exe` | The end to end test on Windows over two com0com pairs |
| `tools/run_sanitizer_test.sh` | The same tests with AddressSanitizer and UndefinedBehaviorSanitizer, the flags are passed from outside |
| `fuzz/run_fuzzers.sh` | libFuzzer targets `sercom_fuzz_frame_decoder` and `sercom_fuzz_link` for one minute each (Clang), the flags are passed from outside |
| `python3 tools/check_versions.py` | The version numbers of CMakeLists.txt and both library.json files |
| `tools/run_static_analysis.sh build` | Layout, comments, countable idioms, cppcheck 2.22 with its Qt library (`tools/analysis/install_cppcheck.sh`) and clang-tidy 18 |
| `pio run -d firmware/esp32 -e esp32dev -e esp32dev-console` | Both firmware environments with the same core library |

All of them run in GitHub Actions, see [ci.yml](../.github/workflows/ci.yml). The Linux
jobs install Qt 5.12.12 for GCC and Clang; the `windows` job builds Debug and Release
with the MinGW 7.3.0 of Qt 5.12.12 and `-Werror`, runs `sercom_test` and `--help` of
every program from a path without Qt and MinGW, then installs com0com with the pairs of
[COM0COM.md](../tools/COM0COM.md) and runs every example and the end to end test over
them.

## Noise tools

`tools/noise.py` and the socat wiring are documented in
[NOISE.md](../tools/NOISE.md) and [SOCAT.md](../tools/SOCAT.md), the com0com pairs
of Windows in [COM0COM.md](../tools/COM0COM.md).
