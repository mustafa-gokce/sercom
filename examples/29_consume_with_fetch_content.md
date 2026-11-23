# 29 Consume with FetchContent

This example is a host tool of its own that uses sercom as a library. Its CMake project fetches
sercom with `FetchContent`, pinned to a tag, turns on sercom's host library and links it together
with Qt. The program opens a serial port, waits until the device answers a ping and prints the
round trip time; with `--to ADDRESS` it pings the node of that address through a chain. In the
repository the build takes this checkout of sercom; in a project of your own CMake downloads the
tag from GitHub.

## What you will learn

- How `FetchContent` fetches sercom at a tag, and why without its submodules.
- How a project turns on the host library of sercom and what it needs for it: Qt 5.12.12.
- How a project includes `"core/include/Frame.h"` through the include directory of
  `sercom::settings`.
- How a host tool of its own reaches a node address with the router of `DeviceClient`.

## Before you start

1. Watch lecture L115 "sercom as a CMake dependency: add_subdirectory, FetchContent, aliases" (section S16) and run example 24 first, the same ping inside the main project.
2. Install the build tools and Qt 5.12.12 once, as README.md shows in the section "Building the project" for Linux and Windows, and `socat` on Linux: `sudo apt install socat`. On Windows create the com0com pair `COM10` and `COM11` of `tools/COM0COM.md`.
3. Build the device simulator of the main project as README.md shows, in `build` of the source directory of sercom: this guide runs `build/sercom_device_sim` on the far end of the cable.
4. Open a terminal and go to the directory of this project: `cd examples/29_consume_with_fetch_content` in the source directory of sercom. On Windows open PowerShell 7 with the path of the README.
5. Configure the build with this checkout of sercom: `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH=$HOME/Qt/5.12.12/gcc_64 -DFETCHCONTENT_SOURCE_DIR_SERCOM=../..`, on Windows with `-DCMAKE_PREFIX_PATH=C:/Qt/5.12.12/mingw73_64`
6. Build the program: `cmake --build build`
7. The program of this example is `build/consume_with_fetch_content`, `build/consume_with_fetch_content.exe` on Windows.

`FETCHCONTENT_SOURCE_DIR_SERCOM` makes `FetchContent` take a local checkout in place of the
download, here the sercom directory two levels up. Without it, the configure step clones the tag
of Step 2 into `build/_deps/sercom-src`.

## Step 1: Lay out a project of your own

A project that fetches sercom needs no submodule and no copy of sercom:

```
my_host_tool/
    CMakeLists.txt
    my_host_tool.cpp
```

Copy `CMakeLists.txt` and `29_consume_with_fetch_content.cpp` of this directory into
`my_host_tool/`. The includes stay as they are, and you configure without
`FETCHCONTENT_SOURCE_DIR_SERCOM`, so that CMake downloads the tag. Use the variable again when you
change sercom itself and want your tool to build against your checkout.

## Step 2: Read the project file

Open `examples/29_consume_with_fetch_content/CMakeLists.txt`.

- `set(SERCOM_GIT_REPOSITORY ...)` and `set(SERCOM_GIT_TAG L115)` stand above the banner: the tag
  is the pin of this project. A lecture tag fits the course; a tool of your own pins a release.
- `set(SERCOM_BUILD_HOST ON)` before `FetchContent_MakeAvailable` turns on the host library,
  which a subproject does not build by default. It is a normal variable, and the `option()` of
  sercom keeps its value: `cmake_minimum_required(VERSION 3.16)` makes the policy CMP0077 new in
  sercom's `CMakeLists.txt`.
- `FetchContent_Declare(sercom ...)` names the repository and the tag. `GIT_SHALLOW ON` clones
  only the commit of the tag. `GIT_SUBMODULES ""` clones no submodule: sercom's `dep/Catch2` is
  for its own tests only. The empty value means "none" since CMake 3.16 (policy CMP0097).
- `FetchContent_MakeAvailable(sercom)` clones the tag, or takes the checkout of
  `FETCHCONTENT_SOURCE_DIR_SERCOM`, and adds it with `add_subdirectory`. sercom then finds Qt for
  its host library and stops when the version is not 5.12.12.
- `find_package(Qt5 ...)` finds Qt for this project too: `main()` uses `QCoreApplication`, the
  event loop and the signals of `SerialPort` itself.
- `target_link_libraries` links `sercom::settings`, the three libraries the program uses with
  `sercom::host` last, and the two Qt modules.

## Step 3: Read the includes

Open `examples/29_consume_with_fetch_content/29_consume_with_fetch_content.cpp`. The sercom headers
are included as `"core/include/Clock.h"` and `"host/include/SerialPort.h"`. The fetched sources lie
in the build directory, so a path relative to this file would depend on where you build.
`sercom::settings` therefore carries the top directory of sercom as its include directory, and
every target that links it can include from there. sercom's own files keep including each other by
relative path.

## Step 4: Read the program

The program is example 24 with one wait and an optional destination.

- `parseCommandLine()` takes the serial port and an optional `--to ADDRESS`, which
  `HostUtils::parseAddressArgument()` checks to be a node address from 2 to 254, the same check
  as `sercom_cli --to`.
- `// serial port must open` opens the port at 115200 baud, 8N1, and
  `// create the device client on the serial port, a destination makes its requests routed` puts
  a `DeviceClient` on it. `getRouterConfiguration()` gives the client the host address only when
  a destination is given, and `setDestinationAddress()` sends every request as a routed frame to
  it. Without `--to` the client has no address and sends the direct frames of version 1.
- `// hand the received data to the device client` connects the signal `receivedData` of the port
  to the client, `// stop the wait with a failure when the serial port fails` connects
  `errorHappened` to a warning.
- `waitForPingResponse()` at the end of the file runs a `QEventLoop`. Every
  `HostSettings::MAINTAIN_PERIOD_MSEC` (2 ms) it maintains the retransmissions, and once per
  `HostSettings::DEVICE_PERIODIC_REQUEST_PERIOD_MSEC` (1000 ms) it sends a ping request when none
  waits, as `sercom_cli` does while it waits for a device. It stops when a ping response arrived,
  a node rejected a ping request, the timeout passed or you pressed Ctrl+C.
- `MockClientHandler::onRequestFailed()` keeps a ping request that a node rejected with another
  status than `BUSY`, because every new ping would get the same answer. After a timeout or a
  `BUSY` the wait sends the next ping in the next period.
- `// wait for the ping response within the boot timeout` waits up to
  `HostSettings::DEVICE_BOOT_TIMEOUT_MSEC` (3000 ms), long enough for an ESP32 that restarts when
  the port opens.
- `// ping request must not be rejected by the node or by a node on the way to it` prints the
  failure as `sercom_cli` does, for example `Request PING_REQUEST failed: REJECTED / UNREACHABLE`,
  and ends with exit code `1`.
- `// print the answered ping with the node that answered it` prints the round trip time that
  `MockClientHandler` kept, and the source address of the response when it was a routed frame.

## Step 5: Run it against the device simulator

1. Start a virtual cable in the first terminal:
   `socat PTY,raw,echo=0,link=/tmp/ttyV1 PTY,raw,echo=0,link=/tmp/ttyV2`.
2. Start the simulator of the main project on one end in the second terminal:
   `../../build/sercom_device_sim /tmp/ttyV2`
3. Run the program on the other end in the third terminal: `build/consume_with_fetch_content /tmp/ttyV1`

On Windows the com0com pair is the cable: start the simulator with
`../../build/sercom_device_sim.exe COM11` and run `build/consume_with_fetch_content.exe COM10`.

You get:

```
Waiting for the device on /tmp/ttyV1, 115200 for at most 3000 ms
Ping answered by the device after 0 ms
```

Then run `echo $?` to see the exit code, in PowerShell `$LASTEXITCODE`. It prints `0`. The round
trip comes from a real clock, usually 0 or 1 ms on a pseudo terminal.

## Step 6: Ping a node address

1. Stop the simulator with Ctrl+C and start it again as node 2: `../../build/sercom_device_sim /tmp/ttyV2 --address 2`
2. Run the program with a destination: `build/consume_with_fetch_content /tmp/ttyV1 --to 2`

You get:

```
Waiting for the device on /tmp/ttyV1, 115200 for at most 3000 ms
Ping answered by node 2 after 0 ms
```

The request and the response were routed frames of version 2: the program sent from the host
address 1 to node 2, and the response came back from node 2 with its address as the source. The
daisy chain of `tests/e2e/chain_test.sh` takes the same `--to 4` to a node two links further.

## Step 7: Understand the output

- `Waiting for the device on /tmp/ttyV1, 115200 ...`: the port is open, the wait starts.
- `Ping answered by the device after 0 ms`: the first ping that got its response, with the time
  from sending it to receiving the response.
- `Ping answered by node 2 after 0 ms`: the same for a routed ping, with the node that answered.

The configure step printed sercom's status block with the Qt of the host library:

```
-- >>> PROJECT_NAME        : sercom (sub)
-- >>> QT_VERSION          : 5.12.12
```

## Try it yourself

1. Leave out the host library. Put a `#` in front of `set(SERCOM_BUILD_HOST ON)` and configure a
   new build directory. CMake stops with `Target "consume_with_fetch_content" links to:
   sercom::host but the target was not found`: without the option, sercom builds no host library
   and finds no Qt.
2. Look for sercom's warning flags. Configure with `-DCMAKE_EXPORT_COMPILE_COMMANDS=ON` and run
   `grep -c -- -Werror build/compile_commands.json`. It prints `0`.
3. Run it with no device. Stop the simulator with Ctrl+C and run the program again. It waits three
   seconds and prints a line ending in `WARNING The device did not answer within the boot timeout`.
   The exit code is `1`.
4. Ping a node that is not there. Run `build/consume_with_fetch_content /tmp/ttyV1 --to 3` against
   node 2. Node 2 has no next node, so it rejects the ping as `UNREACHABLE` at once. The program
   prints a line ending in `WARNING Request PING_REQUEST failed: REJECTED / UNREACHABLE` after this
   one ping and ends with exit code `1`, where a new ping every 2 ms would only flood the link.
5. Download sercom for real. Configure a new build directory without
   `FETCHCONTENT_SOURCE_DIR_SERCOM`: `cmake -S . -B build-download -G Ninja -DCMAKE_PREFIX_PATH=$HOME/Qt/5.12.12/gcc_64`.
   CMake clones the tag `L115` from GitHub into `build-download/_deps/sercom-src`, without
   `dep/Catch2`.

Stop the simulator and the cable with Ctrl+C when you are done.

## Where this is used

- `CMakeLists.txt` of sercom: the option `SERCOM_BUILD_HOST`, the include directory of
  `sercom::settings` and the namespaced aliases.
- `examples/24_serial_ping.cpp`: the same ping inside the main build, with a second measured ping.
- `docs/STYLE.md`: this project holds the one `FetchContent` of the repository, sercom itself keeps
  its dependencies as git submodules.
- Lectures: L77 "The Qt event loop: one thread, signals and a timer", L78 "DeviceClient: requests
  and replies", L113 "Addressed applications: replies, reports and the --to option" and L115
  "sercom as a CMake dependency: add_subdirectory, FetchContent, aliases".
