# sercom

Packet Based Binary Serial Protocol for Linux Hosts, ESP32 and STM32F4 Devices

## Building the project
1. Clone the project with: `git clone https://github.com/mustafa-gokce/sercom.git`
2. Go to the source directory.
3. Populate dependencies: `git submodule update --init --recursive`

### Linux
1. Open a terminal.
2. Install the tools: `sudo apt install build-essential cmake ninja-build python3 socat gdb git`
3. Go to the source directory.
4. `mkdir build && cd build`
5. `cmake -S .. -B . -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON`
6. `cmake --build .`

### ESP32
1. Open a terminal.
2. Install PlatformIO: `pip install platformio`
3. Go to the source directory.
4. Build both firmware environments: `pio run -d firmware/esp32 -e esp32dev -e esp32dev-console`
5. Build the controller firmware: `pio run -d firmware/esp32 -e esp32dev-controller`

### STM32F4
1. Open a terminal.
2. Install PlatformIO: `pip install platformio`
3. Go to the source directory.
4. Build every firmware environment: `pio run -d firmware/stm32f4 -e nucleo_f401re -e nucleo_f411re -e nucleo_f401re-console -e nucleo_f401re-controller -e netduinoplus2-qemu -e netduinoplus2-qemu-controller`

## Run tests
### Linux
1. Open a terminal.
2. Go to the build directory.
3. `./sercom_test --order decl --reporter JUnit::out=test_results_junit.xml --reporter xml::out=test_results.xml --reporter console`
4. Go to the source directory.
5. Run the tests with a 64 byte payload limit: `tools/run_small_limit_test.sh`
6. Run the Python reference self test: `python3 tools/sercom_frame.py self-test`
7. Run the end to end test over a noisy virtual link: `tests/e2e/noisy_link_test.sh build/sercom_cli build/sercom_device_sim`
8. Compare the version numbers of the build files: `python3 tools/check_versions.py`
9. Install Clang, clang-format 18 and clang-tidy 18 (the versions of Ubuntu 24.04): `sudo apt install clang clang-format clang-tidy`
10. Build and install cppcheck 2.22 into `~/.local/cppcheck-2.22.0`: `tools/analysis/install_cppcheck.sh`
11. Run the layout, comment, idiom, cppcheck and clang-tidy gates: `CPPCHECK=~/.local/cppcheck-2.22.0/bin/cppcheck tools/run_static_analysis.sh build`
12. Run the tests with AddressSanitizer and UndefinedBehaviorSanitizer in `build-sanitizers`: `tools/run_sanitizer_test.sh`
13. Fuzz the frame decoder and the link layer for one minute each in `build-fuzz`: `fuzz/run_fuzzers.sh`

### ESP32
1. Build the firmware as above, the protocol code is tested on Linux.

### STM32F4
1. Build the firmware as above, the protocol code and `firmware/common` are tested on Linux.

### Both targets in QEMU
1. Install socat, xPack QEMU Arm and Espressif QEMU as in [docs/HARDWARE.md](docs/HARDWARE.md) section 12.
2. Name both emulators: `export ARM_QEMU=<path>/qemu-system-arm ESP32_QEMU=<path>/qemu-system-xtensa`
3. Run both targets against the tools and against each other: `tests/e2e/emulator_test.sh build/sercom_cli firmware/esp32/.pio/build firmware/stm32f4/.pio/build`

## Run application
### Linux
1. Open a terminal.
2. Go to the source directory.
3. Start a noisy virtual link between `/tmp/ttyV1` and `/tmp/ttyV2`: `tools/noisy_link.sh --profile realistic --seed 1 &`
4. Start the device simulator: `build/sercom_device_sim /tmp/ttyV2 &`
5. Measure the round trip time: `build/sercom_cli /tmp/ttyV1 ping 3`
6. Print every frame on the wire in both directions: `build/sercom_cli /tmp/ttyV1 --trace ping 1`
7. Print the device information: `build/sercom_cli /tmp/ttyV1 info`
8. Switch the LED: `build/sercom_cli /tmp/ttyV1 led on`
9. Print the telemetry: `build/sercom_cli /tmp/ttyV1 telemetry 200 3`
10. Print the telemetry and the device logs for five seconds: `build/sercom_cli /tmp/ttyV1 monitor 5`
11. Run the echo stress test: `build/sercom_cli /tmp/ttyV1 stress 300 32 2`
12. Stop the device simulator and the noisy link: `kill %2 %1`
13. Write one frame into a capture file: `python3 tools/sercom_frame.py encode --type 0x0001 --counter 7 --flags 0x01 --payload E8030000 --binary > /tmp/ping.bin`
14. Decode the capture file: `build/sercom_dump /tmp/ping.bin`
15. Pick an example from the index of the 25 examples with their step-by-step guides: [examples/README.md](examples/README.md)
16. Run one example: `build/sercom_example_02_build_frame`
17. Run every example of the build directory: `tools/run_examples.sh build`

### ESP32
1. Connect the board over USB.
2. Close every serial monitor, the protocol uses the same USB port.
3. Open a terminal.
4. Go to the source directory.
5. Flash the firmware with the protocol on the USB port: `pio run -d firmware/esp32 -e esp32dev -t upload`
6. Print the device information and every frame on the wire: `build/sercom_cli /dev/ttyUSB0 --trace info`
7. Measure one ping on the board: `build/sercom_example_24_serial_ping /dev/ttyUSB0`
8. Follow [docs/HARDWARE.md](docs/HARDWARE.md) for the wiring, the console firmware `esp32dev-console` with the protocol on UART2 and the troubleshooting list.

### STM32F4
1. Connect the ST-LINK USB port of a Nucleo-F401RE board.
2. Open a terminal.
3. Go to the source directory.
4. Flash the firmware with the protocol on the virtual COM port of the ST-LINK: `pio run -d firmware/stm32f4 -e nucleo_f401re -t upload`
5. Print the device information and every frame on the wire: `build/sercom_cli /dev/ttyACM0 --trace info`
6. Run the echo stress test: `build/sercom_cli /dev/ttyACM0 stress 300 32 2`

### Device to device
1. Flash `esp32dev-controller` to an ESP32 and `nucleo_f401re-console` to a Nucleo-F401RE board.
2. Wire ESP32 GPIO17 to PA10, GPIO16 to PA9 and GND to GND, 3.3 V levels only, as in [docs/HARDWARE.md](docs/HARDWARE.md) section 11.
3. Open the console of the controller on the USB port of the ESP32: `pio device monitor -d firmware/esp32 -e esp32dev-controller -p /dev/ttyUSB0`
4. Wait for `Controller passed every step`, the reset button of the ESP32 runs every step again.

## Contribution guidelines
Contributions follow the rules below, the layout of the repository and the tools are described in [docs/OVERVIEW.md](docs/OVERVIEW.md).
1. Fork the project.
2. Clone the fork.
3. Create a branch named `username-YYYY.MM.DD-NN`, never commit on the main branch.
4. Group the commits by file and name them as: `FileName: A small comment for the commit` (a header and its source share the name).
5. Codes must not contain any warnings or errors.
6. Every contribution comes with its tests.
7. A contribution must not break the auto tests.
8. Codes must be formatted with: `clang-format --style=file:tools/analysis/clang-format.yaml -i <file>`
9. Codes must be checked with `cppcheck` and `clang-tidy` through `tools/run_static_analysis.sh` and must not contain any warnings.
10. Codes must follow the coding and commenting style of [docs/STYLE.md](docs/STYLE.md), `python3 tools/analysis/check_style.py` checks its countable rules.
11. Push the branch to the fork.
12. Open a pull request.
13. Wait for the tests to pass (changes of timing need at least 10 consecutive passing runs).
14. Wait for the approval.

## Contributors
### [mustafa-gokce](https://github.com/mustafa-gokce) - Maintainer, Developer
