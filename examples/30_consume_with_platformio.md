# 30 Consume with PlatformIO

This example is an ESP32 firmware of its own that uses sercom as a library. Its PlatformIO
project links the `core`, `app` and common firmware libraries of a sercom checkout, brings its
own serial port and board classes and runs the device role of sercom on them, so it answers
`sercom_cli` like the firmware of `firmware/esp32`. In the repository the checkout is the sercom
directory two levels up; in a project of your own it is the git submodule `dep/sercom`.

## What you will learn

- How a PlatformIO project links the three sercom libraries in place with `symlink://`, and why
  not with a git URL.
- How `build_settings.py` and the project option `custom_sercom_directory` give the firmware the
  protocol limits of the `CMakeLists.txt` of sercom.
- Which two classes a board of its own needs for the device role: a serial port and a platform
  with a clock.

## Before you start

1. Watch lecture L116 "sercom in PlatformIO projects, in CI and as release 2.0" (section S16), and before it L83 "PlatformIO project layout and shared libraries" and L92 "A generic device layer: extracting firmware/common".
2. Install PlatformIO once as README.md shows in the section "Building the project", "ESP32": `pip install platformio`, then check that `pio --version` runs.
3. Open a terminal and go to the directory of this project: `cd examples/30_consume_with_platformio` in the source directory of sercom. On Windows open PowerShell 7.
4. Build the firmware: `pio run`
5. The firmware of this example is `.pio/build/esp32dev/firmware.bin`; `pio run -t upload` flashes it to an ESP32 board on the USB port.

The first build downloads the ESP32 platform and the Arduino framework. PlatformIO builds the
project on Linux and on Windows; the CI of sercom builds it on Linux, like the firmware projects
of the repository, whose Windows steps `docs/HARDWARE.md` describes in section 10.

## Step 1: Lay out a project of your own

In a project of your own, sercom is a git submodule in `dep/`, next to the sources of the
firmware:

```
my_firmware/
    platformio.ini
    include/
        ConsumerPlatform.h
        ConsumerPort.h
        main.h
    src/
        ConsumerPlatform.cpp
        ConsumerPort.cpp
        main.cpp
    dep/
        sercom/        the git submodule
```

1. Add the submodule: `git submodule add https://github.com/mustafa-gokce/sercom.git dep/sercom`
2. Pin it to the release: `git -C dep/sercom checkout v2.0.0`, then commit `dep/sercom` together
   with `.gitmodules`.
3. A fresh clone of your project fetches it with `git submodule update --init`. sercom's own
   submodule Catch2 is not needed, because the firmware builds no tests.

Copy `platformio.ini`, `include/` and `src/` of this directory into `my_firmware/` and change the
paths: `../..` becomes `dep/sercom` in the five lines of Step 2, and `"../../../` becomes
`"../dep/sercom/` in the includes of the sources.

## Step 2: Read the project file

Open `examples/30_consume_with_platformio/platformio.ini`.

- `[env]` takes the platform `espressif32@6.9.0` and the Arduino framework, the same pins as
  `firmware/esp32/platformio.ini`.
- `custom_sercom_directory = ../..` names the sercom checkout relative to the project directory,
  `dep/sercom` in your own project. PlatformIO keeps every option that starts with `custom_` for
  the scripts of the project.
- `lib_deps` links three libraries in place with `symlink://`: the manifests `library.json` of
  `core/`, `app/` and `firmware/common/`, named `sercom-core`, `sercom-app` and
  `sercom-firmware`. PlatformIO builds them with the flags of their manifests. They include each
  other by relative path, so they must stay together in the tree of one checkout; a git URL in
  `lib_deps` would install each library as a package of its own and break those paths.
- `extra_scripts = pre:../../firmware/common/build_settings.py` runs the script of sercom before
  the build. It reads `custom_sercom_directory`, finds the `CMakeLists.txt` of sercom there and
  passes `MAXIMUM_PAYLOAD_SIZE`, `MAXIMUM_PENDING_FRAMES`, `DEFAULT_DUPLICATE_HISTORY_SIZE` and the
  version numbers to every source. The protocol limits are the contract with the peer, so the
  firmware and the host tools build with the same values. Without the option the script looks two
  levels up, where the firmware projects of sercom find the file.
- `build_unflags` and `build_flags` replace the `gnu++11` of the framework by C++17;
  `build_src_flags` turns on the house warnings with `-Werror` for the sources in `src/`.

## Step 3: Read the serial port

Open `examples/30_consume_with_platformio/include/ConsumerPort.h` and its source in `src/`.

- `ConsumerPort` is the `Transport::Transport` of the protocol: `sendData()` writes every byte of
  a frame to `Serial`, UART0 on the USB bridge of the board.
- It is also the `ConsoleOutput::ConsoleOutput` that the console of the device role needs.
  `CONSOLE_PRINTED` is `false`: the protocol keeps UART0 to itself, and a console that is not
  printed never writes to its output.
- `begin()` sets the receive buffer of the driver to 1 KB before the serial port begins, room for
  the largest frame of 271 bytes, and opens the port at 115200 baud, 8N1.
- `receiveData()` reads the waiting bytes, at most one chunk, and gets 0 when none waits.

## Step 4: Read the board

Open `examples/30_consume_with_platformio/include/ConsumerPlatform.h` and its source.

- `ConsumerPlatform` is the `FirmwarePlatform::FirmwarePlatform` of the device role: the LED on
  GPIO 2, the free heap of the ESP32, the device name `consumer-esp32` and `showLed()` for the
  activity blink.
- `getTemperature()` gives `Messages::UNAVAILABLE_TEMPERATURE`, because this board reads no
  sensor; `sercom_cli telemetry` shows it as `no reading`.
- It is also the `Clock::Clock` of the device role: `getTime()` gives `millis()` of the framework.

## Step 5: Read the program

Open `examples/30_consume_with_platformio/src/main.cpp`.

- `// initialize device` creates four objects at file scope: the serial port, the board, a
  console that is not printed, and the device role of `firmware/common`. The device role takes
  the constructor without a router configuration: an end node without an address, which sends
  the bytes of version 1.
- `setup()` opens the serial port, configures the board and reports the line `Started the
  DeviceRole on UART0, 115200 ...` through the device role, a log report to the host.
- `loop()` feeds the waiting bytes to the device role and maintains it on every pass of the
  framework. One loop replaces the receive, protocol, console and heartbeat tasks of
  `firmware/esp32`, which this small firmware does not need.

## Step 6: Run it in QEMU

The emulator of README.md, "Run tests", "Both targets in QEMU", runs the firmware without a board.
From the source directory of sercom, with the host tools built in `build`:

1. Name the emulator: `export ESP32_QEMU=<path>/qemu-system-xtensa`
2. Start the firmware: `tools/run_emulator.sh start esp32 examples/30_consume_with_platformio/.pio/build/esp32dev /tmp/consumer`
3. Ping it three times: `build/sercom_cli /tmp/consumer/uart0 ping 3`
4. Ask for its device information: `build/sercom_cli /tmp/consumer/uart0 info`
5. Stop it: `tools/run_emulator.sh stop /tmp/consumer`

You get, as emulator output:

```
ping response: round trip 13 ms, device uptime 1428 ms
ping response: round trip 7 ms, device uptime 1628 ms
ping response: round trip 9 ms, device uptime 1829 ms
```

and from `info`:

```
device: consumer-esp32
firmware: 2.0
```

followed by the uptime and the free heap. On a board run `pio run -t upload` and then
`build/sercom_cli /dev/ttyUSB0 ping 3`, on Windows with the COM port of the board.

## Step 7: Understand the output

- `ping response: round trip 13 ms`: the device role of sercom answered on the serial port of
  this project, through the link layer of the `core` library in both directions.
- `device uptime 1428 ms`: `millis()` of `ConsumerPlatform` at the answer, 200 ms apart for the
  three pings of `PING_PERIOD_MSEC`.
- `device: consumer-esp32`: the name of `ConsumerPlatform`. `firmware: 2.0` comes from the
  `set()` lines of the `CMakeLists.txt` of sercom, which `build_settings.py` passed to the build.
- The round trip times vary from run to run; QEMU is slower than the chip.

## Try it yourself

1. Give the node an address. Pass a `Router::RouterConfiguration` with `nodeAddress = 2` as the
   fifth argument of the device role, build and run it again, and ask
   `build/sercom_cli /tmp/consumer/uart0 --to 2 info`. It also prints `protocol version: 2` and
   `node address: 2`.
2. Switch the LED. Run `build/sercom_cli /tmp/consumer/uart0 led on`; on a board the LED on
   GPIO 2 lights.
3. Lose the build values. In a project of your own, put a `;` in front of
   `custom_sercom_directory`. The build stops in `build_settings.py`, which finds no
   `CMakeLists.txt` two levels above the project.
4. Read the telemetry. Run `build/sercom_cli /tmp/consumer/uart0 telemetry 500 2`: every line
   ends with `temperature no reading`, the unavailable temperature of `ConsumerPlatform`.

## Where this is used

- `firmware/common/build_settings.py`: the project option `custom_sercom_directory`.
- `firmware/esp32/`: the firmware of sercom for the same board, with its tasks, its console and
  the router role.
- `.github/workflows/ci.yml`: the `firmware` job builds this project, the `emulator` job pings it
  in QEMU.
- Lectures: L83 "PlatformIO project layout and shared libraries", L85 "Hardware adapters:
  transport, clock and platform", L92 "A generic device layer: extracting firmware/common" and
  L116 "sercom in PlatformIO projects, in CI and as release 2.0".
