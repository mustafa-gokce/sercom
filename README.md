# sercom

Packet Based Binary Serial Protocol for Linux Hosts and ESP32 Devices

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

## Run tests
### Linux
1. Open a terminal.
2. Go to the build directory.
3. `./sercom_test --order decl --reporter JUnit::out=test_results_junit.xml --reporter xml::out=test_results.xml --reporter console`

### ESP32
1. Build the firmware as above, the protocol code is tested on Linux.

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
