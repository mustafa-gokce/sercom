# sercom

Packet Based Binary Serial Protocol for Qt Hosts on Linux and Windows, ESP32 and STM32F4 Devices

## Building the project
1. Clone the project with: `git clone https://github.com/mustafa-gokce/sercom.git`
2. Go to the source directory.
3. Populate dependencies: `git submodule update --init --recursive`

### Linux
1. Open a terminal.
2. Install the tools: `sudo apt install build-essential cmake ninja-build python3 python3-venv socat gdb git`
3. Install aqtinstall into its own environment: `python3 -m venv ~/aqt && ~/aqt/bin/pip install aqtinstall`
4. Install Qt 5.12.12, the only Qt the host code builds with: `~/aqt/bin/aqt install-qt linux desktop 5.12.12 gcc_64 -O ~/Qt`
5. Go to the source directory.
6. `mkdir build && cd build`
7. `cmake -S .. -B . -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_PREFIX_PATH=$HOME/Qt/5.12.12/gcc_64`
8. `cmake --build .`

### Windows
1. Install Git, CMake, Python 3 and PowerShell 7 (`winget install Git.Git Kitware.CMake Python.Python.3.12 Microsoft.PowerShell`).
2. Open PowerShell 7 (`pwsh`).
3. Install aqtinstall: `python -m pip install aqtinstall pyserial`
4. Install Qt 5.12.12 for MinGW 7.3.0 64-bit: `python -m aqt install-qt windows desktop 5.12.12 win64_mingw73 -O C:\Qt`
5. Install MinGW 7.3.0 64-bit, the compiler of this Qt and the only Windows compiler of the project: `python -m aqt install-tool windows desktop tools_mingw qt.tools.win64_mingw730 -O C:\Qt`
6. Install Ninja: `python -m aqt install-tool windows desktop tools_ninja qt.tools.ninja -O C:\Qt`
7. Put MinGW, Ninja and Qt on the path of the prompt: `$env:PATH = "C:\Qt\Tools\mingw730_64\bin;C:\Qt\Tools\Ninja;C:\Qt\5.12.12\mingw73_64\bin;$env:PATH"`
8. Go to the source directory.
9. `mkdir build; cd build`
10. `cmake -S .. -B . -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH=C:/Qt/5.12.12/mingw73_64`
11. `cmake --build .`
12. The build runs `windeployqt` on `sercom_cli.exe`: the Qt Core, Qt Serial Port and MinGW DLLs that it needs land next to the programs, and every program but `sercom_test.exe` then starts from any prompt. `sercom_test.exe` also needs Qt Test, which `windeployqt` does not deploy, and starts only with the Qt `bin` directory of step 7 on the path. Qt is linked dynamically under the LGPL.

### ESP32
1. Open a terminal, on Windows PowerShell 7.
2. Install PlatformIO: `pip install platformio`
3. Check that PlatformIO runs: `pio --version`; when PowerShell does not find `pio` on Windows, put the `Scripts` directory that pip names in its warning on the path or call `python -m platformio` in place of `pio`
4. Go to the source directory.
5. Build both firmware environments: `pio run -d firmware/esp32 -e esp32dev -e esp32dev-console`
6. Build the controller firmware: `pio run -d firmware/esp32 -e esp32dev-controller`
7. Build the router and the end of a daisy chain: `pio run -d firmware/esp32 -e esp32dev-router -e esp32dev-chain`

### STM32F4
1. Open a terminal, on Windows PowerShell 7.
2. Install PlatformIO: `pip install platformio`
3. Check that PlatformIO runs: `pio --version`; when PowerShell does not find `pio` on Windows, put the `Scripts` directory that pip names in its warning on the path or call `python -m platformio` in place of `pio`
4. Go to the source directory.
5. Build every firmware environment: `pio run -d firmware/stm32f4 -e nucleo_f401re -e nucleo_f411re -e nucleo_f401re-console -e nucleo_f401re-controller -e netduinoplus2-qemu -e netduinoplus2-qemu-controller -e nucleo_f401re-router -e netduinoplus2-qemu-router`

### As a dependency
1. A CMake project of your own uses sercom as a library with CMake 3.16 or newer and a C++17 compiler; `sercom::settings` requires C++17 of every target that links it, also without `set(CMAKE_CXX_STANDARD 17)` in your project
2. Add sercom as a git submodule and pin it to a tag: `git submodule add https://github.com/mustafa-gokce/sercom.git dep/sercom` and `git -C dep/sercom checkout <tag>`
3. Add the submodule in the `CMakeLists.txt` of your project: `add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/dep/sercom)`
4. Or fetch sercom at a tag without a submodule: `include(FetchContent)`, `FetchContent_Declare(sercom GIT_REPOSITORY https://github.com/mustafa-gokce/sercom.git GIT_TAG <tag> GIT_SHALLOW ON GIT_SUBMODULES "")` and `FetchContent_MakeAvailable(sercom)`
5. Build a fetched sercom from a local checkout in place of the download: `-DFETCHCONTENT_SOURCE_DIR_SERCOM=<checkout>`
6. Link `sercom::settings` and every library the target uses: `target_link_libraries(my_tool PRIVATE sercom::settings sercom::core sercom::app)`, the other libraries are `sercom::firmware` and `sercom::host`
7. Include the headers by relative path, `"dep/sercom/app/include/DeviceClient.h"`, or through the include directory of `sercom::settings`, `"app/include/DeviceClient.h"`, the form for a fetched sercom.
8. As a subproject sercom builds no tests, no programs and no host library and asks for no Qt; its libraries build with the flags of your project, without the warning flags of sercom.
9. Turn on the host library with `set(SERCOM_BUILD_HOST ON)` before `add_subdirectory` or `FetchContent_MakeAvailable`; it needs Qt 5.12.12 on `CMAKE_PREFIX_PATH`, and a target that links `sercom::host` also links `Qt5::Core` and `Qt5::SerialPort`.
10. The protocol limits `MAXIMUM_PAYLOAD_SIZE`, `MAXIMUM_PENDING_FRAMES` and `DEFAULT_DUPLICATE_HISTORY_SIZE` stay the `set()` lines of the `CMakeLists.txt` of sercom: they are the contract with the peer, so both ends build with the same values.
11. sercom has no install step and no `find_package(sercom)`, a project takes it as a submodule or with `FetchContent`.
12. Examples 28 and 29 are two such projects with their guides, [examples/README.md](examples/README.md) lists them under "Consumer projects".
13. Build and run both against this checkout: `tools/run_consumers.sh build/sercom_device_sim`, on Windows `tools/run_consumers.ps1 build/sercom_device_sim.exe`

## Run tests
### Linux
1. Open a terminal.
2. Go to the build directory.
3. `./sercom_test --order decl --reporter JUnit::out=test_results_junit.xml --reporter xml::out=test_results.xml --reporter console`
4. Go to the source directory.
5. Point the helper scripts at Qt 5.12.12: `export CMAKE_PREFIX_PATH=$HOME/Qt/5.12.12/gcc_64`
6. Run the tests with a 64 byte payload limit: `tools/run_small_limit_test.sh`
7. Run the Python reference self test: `python3 tools/sercom_frame.py self-test`
8. Run the end to end test over a noisy virtual link: `tests/e2e/noisy_link_test.sh build/sercom_cli build/sercom_device_sim`
9. Run every example of the build directory, examples 01 and 24 over socat pseudo terminals: `tools/run_examples.sh build`
10. Compare the version numbers of the build files: `python3 tools/check_versions.py`
11. Install Clang, clang-format 18 and clang-tidy 18 (the versions of Ubuntu 24.04): `sudo apt install clang clang-format clang-tidy`
12. Build and install cppcheck 2.22 into `~/.local/cppcheck-2.22.0`: `tools/analysis/install_cppcheck.sh`
13. Run the layout, comment, idiom, cppcheck and clang-tidy gates: `CPPCHECK=~/.local/cppcheck-2.22.0/bin/cppcheck tools/run_static_analysis.sh build`
14. Run the tests with AddressSanitizer and UndefinedBehaviorSanitizer in `build-sanitizers`: `tools/run_sanitizer_test.sh`
15. Fuzz the frame decoder, the link layer and the router for one minute each in `build-fuzz`: `fuzz/run_fuzzers.sh`

### Windows
1. Open PowerShell 7 with the path of the build steps.
2. Go to the build directory.
3. `./sercom_test.exe --order decl --reporter JUnit::out=test_results_junit.xml --reporter xml::out=test_results.xml --reporter console`
4. Go to the source directory.
5. Run the Python reference self test: `python tools/sercom_frame.py self-test`
6. Run every example of the build directory, examples 01 and 24 on the com0com pair `COM10` and `COM11` of [tools/COM0COM.md](tools/COM0COM.md): `tools/run_examples.ps1 build`
7. Run the end to end test over a noisy link of the com0com pairs `COM10` to `COM13`: `tests/e2e/noisy_link_test.ps1 build/sercom_cli.exe build/sercom_device_sim.exe`

### ESP32
1. Build the firmware as above, the protocol code is tested on Linux and Windows.

### STM32F4
1. Build the firmware as above, the protocol code and `firmware/common` are tested on Linux and Windows.

### Both targets in QEMU
1. Build the host with Qt 5.12.12 as the Linux steps show and the firmware as above, the emulator tools need a Linux host.
2. Install socat, xPack QEMU Arm and Espressif QEMU as in [docs/HARDWARE.md](docs/HARDWARE.md) section 13.
3. Name both emulators: `export ARM_QEMU=<path>/qemu-system-arm ESP32_QEMU=<path>/qemu-system-xtensa`
4. Run both targets against the tools and against each other: `tests/e2e/emulator_test.sh build/sercom_cli firmware/esp32/.pio/build firmware/stm32f4/.pio/build`

### Daisy chain
1. Build the host as the Linux steps show, the test needs `socat` and Python 3 on Linux.
2. Go to the source directory.
3. Run the chain of three device simulators with noise between node 2 and node 3: `tests/e2e/chain_test.sh build/sercom_cli build/sercom_device_sim`
4. Run it with a device simulator of version 1 as the restarted node 4 behind the proxy: `END_DEVICE_SIMULATOR=<old build>/sercom_device_sim tests/e2e/chain_test.sh build/sercom_cli build/sercom_device_sim`
5. On Windows create the com0com pairs `COM10` with `COM11`, `COM12` with `COM13`, `COM14` with `COM15` and `COM16` with `COM17` as [tools/COM0COM.md](tools/COM0COM.md) shows and run the PowerShell twin: `tests/e2e/chain_test.ps1 build/sercom_cli.exe build/sercom_device_sim.exe`

### Daisy chain in QEMU
1. Build the host with Qt 5.12.12 as the Linux steps show, the emulator tools need a Linux host.
2. Install socat, xPack QEMU Arm and Espressif QEMU and name both emulators as in "Both targets in QEMU".
3. Build the firmware of the chain: `pio run -d firmware/esp32 -e esp32dev-router -e esp32dev-chain -e esp32dev-console` and `pio run -d firmware/stm32f4 -e netduinoplus2-qemu-router`
4. Run the ESP32 router node 2, the STM32F4 router node 3 and the ESP32 node 4 with noise between node 2 and node 3, then node 4 without an address behind the proxy of node 3: `tests/e2e/emulator_chain_test.sh build/sercom_cli build/sercom_dump firmware/esp32/.pio/build firmware/stm32f4/.pio/build`
5. Run it with a firmware of version 1 as the replaced node 4: `END_DEVICE_BUILD_DIRECTORY=<old tree>/firmware/esp32/.pio/build/esp32dev-console tests/e2e/emulator_chain_test.sh build/sercom_cli build/sercom_dump firmware/esp32/.pio/build firmware/stm32f4/.pio/build`
6. Follow the steps of [docs/HARDWARE.md](docs/HARDWARE.md) section 14 to start the three chips one by one and decode their port logs with `sercom_dump`.

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

### Windows
1. Create the com0com pairs `COM10` with `COM11` and `COM12` with `COM13` as [tools/COM0COM.md](tools/COM0COM.md) shows.
2. Open PowerShell 7 with the path of the build steps.
3. Go to the source directory.
4. Start a noisy link between `COM10` and `COM13`: `Start-Process pwsh -ArgumentList '-File', 'tools/noisy_link.ps1', '--profile', 'realistic', '--seed', '1'`
5. Start the device simulator: `Start-Process build/sercom_device_sim.exe -ArgumentList 'COM13'`
6. Measure the round trip time: `build/sercom_cli.exe COM10 ping 3`
7. Print every frame on the wire in both directions: `build/sercom_cli.exe COM10 --trace ping 1`
8. Run the other commands of the Linux steps the same way, with `COM10` in place of `/tmp/ttyV1`.
9. Stop the device simulator and the noisy link with Ctrl+C in their windows.
10. A port name from `COM1` to `COM256` works as it is, `\\.\COM10` works too.

### ESP32
1. Connect the board over USB.
2. Close every serial monitor, the protocol uses the same USB port.
3. Open a terminal.
4. Go to the source directory.
5. Flash the firmware with the protocol on the USB port: `pio run -d firmware/esp32 -e esp32dev -t upload`
6. Print the device information and every frame on the wire: `build/sercom_cli /dev/ttyUSB0 --trace info`, on Windows with the COM port of the board: `build/sercom_cli.exe COM3 --trace info`
7. Measure one ping on the board: `build/sercom_example_24_serial_ping /dev/ttyUSB0`
8. Follow [docs/HARDWARE.md](docs/HARDWARE.md) for the wiring, the console firmware `esp32dev-console` with the protocol on UART2, the COM ports of Windows and the troubleshooting list.

### STM32F4
1. Connect the ST-LINK USB port of a Nucleo-F401RE board, on Windows with the ST-LINK driver of [docs/HARDWARE.md](docs/HARDWARE.md) section 10.
2. Open a terminal.
3. Go to the source directory.
4. Flash the firmware with the protocol on the virtual COM port of the ST-LINK: `pio run -d firmware/stm32f4 -e nucleo_f401re -t upload`
5. Print the device information and every frame on the wire: `build/sercom_cli /dev/ttyACM0 --trace info`, on Windows with the COM port of the ST-LINK: `build/sercom_cli.exe COM5 --trace info`
6. Run the echo stress test: `build/sercom_cli /dev/ttyACM0 stress 300 32 2`, on Windows `build/sercom_cli.exe COM5 stress 300 32 2`

### Device to device
1. Flash `esp32dev-controller` to an ESP32 and `nucleo_f401re-console` to a Nucleo-F401RE board.
2. Wire ESP32 GPIO17 to PA10, GPIO16 to PA9 and GND to GND, 3.3 V levels only, as in [docs/HARDWARE.md](docs/HARDWARE.md) section 12.
3. Open the console of the controller on the USB port of the ESP32: `pio device monitor -d firmware/esp32 -e esp32dev-controller -p /dev/ttyUSB0`, on Windows with `-p COM3`
4. Wait for `Controller passed every step`, the reset button of the ESP32 runs every step again.

### Daisy chain
1. Open a terminal.
2. Go to the source directory.
3. Join the host with node 2: `socat PTY,raw,echo=0,link=/tmp/ttyH1 PTY,raw,echo=0,link=/tmp/ttyH2 &`
4. Join node 2 with node 3: `socat PTY,raw,echo=0,link=/tmp/ttyN1 PTY,raw,echo=0,link=/tmp/ttyN2 &`
5. Start node 3 at the end of the chain: `build/sercom_device_sim /tmp/ttyN2 --name node3 --address 3 &`
6. Start node 2 with its downstream serial port: `build/sercom_device_sim /tmp/ttyH2 --name node2 --address 2 --downstream /tmp/ttyN1 &`
7. Find every node of the chain: `build/sercom_cli /tmp/ttyH1 discover`
8. Print the device information of node 3 through node 2: `build/sercom_cli /tmp/ttyH1 --to 3 info`
9. Measure the round trip time to node 3: `build/sercom_cli /tmp/ttyH1 --to 3 ping 3`
10. Let a request expire on the way with a time to live of 1: `build/sercom_cli /tmp/ttyH1 --to 3 --ttl 1 ping 1`
11. Talk to node 2 alone, without an address, as to any device: `build/sercom_cli /tmp/ttyH1 ping 1`
12. A node started with `--proxy N` and a downstream device without `--address` stands in for that device as node N.
13. Stop the simulators and the links: `kill %4 %3 %2 %1`
14. On Windows use com0com pairs in place of the `socat` links, as in the steps of "Run tests".

### Daisy chain of boards
1. Flash `esp32dev-router` to an ESP32 as node 2, `nucleo_f401re-router` to a Nucleo-F401RE board as node 3 and `esp32dev-chain` to a second ESP32 as node 4.
2. Wire ESP32 node 2 GPIO17 to PA10 and GPIO16 to PA9 of node 3, PA11 of node 3 to GPIO16 and PA12 to GPIO17 of ESP32 node 4, and GND of every board together, 3.3 V levels only, as in [docs/HARDWARE.md](docs/HARDWARE.md) section 14.
3. Find every node of the chain on the USB port of node 2: `build/sercom_cli /dev/ttyUSB0 discover`, on Windows with the COM port of the board: `build/sercom_cli.exe COM3 discover`
4. Print the device information of node 4 through node 2 and node 3: `build/sercom_cli /dev/ttyUSB0 --to 4 info`
5. Run the echo stress test over three links: `build/sercom_cli /dev/ttyUSB0 --to 4 stress 300 32 2`
6. Watch the console of node 3 on its ST-LINK port: `pio device monitor -d firmware/stm32f4 -e nucleo_f401re-router -p /dev/ttyACM0`, on Windows with `-p COM5`
7. Flash `esp32dev-console` to node 4 to make it a device without an address, which node 3 reaches as the proxy of node 4: `build/sercom_cli /dev/ttyUSB0 --to 4 info`

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
9. Host codes use Qt 5.12.12 and contain no platform branches, `core/`, `app/` and `firmware/common/` stay plain C++ without Qt.
10. Codes must be checked with `cppcheck` and `clang-tidy` through `tools/run_static_analysis.sh` and must not contain any warnings.
11. Codes must follow the coding and commenting style of [docs/STYLE.md](docs/STYLE.md), `python3 tools/analysis/check_style.py` checks its countable rules.
12. Push the branch to the fork.
13. Open a pull request.
14. Wait for the tests to pass on Linux and Windows (changes of timing need at least 10 consecutive passing runs).
15. Wait for the approval.

## Contributors
### [mustafa-gokce](https://github.com/mustafa-gokce) - Maintainer, Developer
