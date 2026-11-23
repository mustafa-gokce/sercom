# 28 Consume with add_subdirectory

This example is a CMake project of its own that uses sercom as a library. It adds a checkout of
sercom with `add_subdirectory`, links the `core` and `app` libraries without Qt and runs a device
and a client in memory: the client with the host address pings the device as node 2 and prints
the answer. In the repository the checkout is the sercom directory two levels up; in a project of
your own it is the git submodule `dep/sercom`.

## What you will learn

- How a project adds sercom with `add_subdirectory` and which targets it links.
- What sercom builds as a subproject: no tests, no programs, no host library, no Qt and none of
  its own warning flags.
- How the headers are included by relative path, as in sercom itself.
- How a client with the host address sends a routed request to the address of a node.

## Before you start

1. Watch lecture L115 "sercom as a CMake dependency: add_subdirectory, FetchContent, aliases" (section S16) and run example 21 first, the same device and client in the main project.
2. Install the build tools once: `sudo apt install build-essential cmake ninja-build git`. This project needs no Qt. On Windows install CMake, MinGW 7.3.0 and Ninja as README.md shows in the section "Building the project", steps 1, 5 and 6, and put MinGW and Ninja on the path of the prompt as in its step 7.
3. Open a terminal and go to the directory of this project: `cd examples/28_consume_with_add_subdirectory` in the source directory of sercom. On Windows open PowerShell 7.
4. Configure the build: `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug`
5. Build the program: `cmake --build build`
6. The program of this example is `build/consume_with_add_subdirectory`, `build/consume_with_add_subdirectory.exe` on Windows.

There is no `CMAKE_PREFIX_PATH`: nothing in this build asks for Qt.

## Step 1: Lay out a project of your own

In a project of your own, sercom is a git submodule in `dep/`, the way sercom keeps Catch2:

```
my_project/
    CMakeLists.txt
    my_project.cpp
    dep/
        sercom/        the git submodule
```

1. Add the submodule: `git submodule add https://github.com/mustafa-gokce/sercom.git dep/sercom`
2. Pin it to a tag: `git -C dep/sercom checkout L115`, then commit `dep/sercom` together with
   `.gitmodules`.
3. A fresh clone of your project fetches it with `git submodule update --init`. sercom's own
   submodule Catch2 is not needed, because sercom builds no tests as a subproject.

Copy `CMakeLists.txt` and `28_consume_with_add_subdirectory.cpp` of this directory into
`my_project/` and change two things: the sercom directory of Step 2 and the include paths of
Step 3.

## Step 2: Read the project file

Open `examples/28_consume_with_add_subdirectory/CMakeLists.txt`.

- `set(SERCOM_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../..)` stands above the banner, because it is
  the value you change: `${CMAKE_CURRENT_SOURCE_DIR}/dep/sercom` in your own project.
- `# use C++ 2017` sets the language level of this project, as every project of the house style
  does. `CMAKE_CXX_STANDARD` is a variable of a directory: sercom's own C++17 reaches sercom's
  targets, not yours. The sercom headers need C++17, so `sercom::settings` requires `cxx_std_17`
  of every target that links it, and a project without its own C++17 still builds them.
- `# add sercom without its tests, binaries and host library` adds the checkout. The second
  argument names the build directory of sercom; it is needed because `../..` lies outside this
  project, and it does no harm for `dep/sercom`. `EXCLUDE_FROM_ALL` builds only the libraries
  that a target of this project links.
- `target_link_libraries` links three targets by their namespaced names. `sercom::settings`
  carries the protocol limits, such as `MAXIMUM_PAYLOAD_SIZE=255`, that every sercom header
  reads. `sercom::core` and `sercom::app` are the two libraries this program uses. sercom links
  nothing `PUBLIC`, so a project lists every library it uses, as sercom's own programs do.

The other names are `sercom::firmware` and, with the host library of example 29, `sercom::host`.

## Step 3: Read the includes

Open `examples/28_consume_with_add_subdirectory/28_consume_with_add_subdirectory.cpp`. The sercom
headers are included by a path relative to this file, `"../../app/include/DeviceClient.h"`, the way
sercom includes its own headers. In `my_project/my_project.cpp` the same header is
`"dep/sercom/app/include/DeviceClient.h"`.

## Step 4: Read the program

The program is example 21 without its host library: the same four mocks (`MockClock`,
`MockTransport`, `MockDevicePlatform` and `MockClientHandler`), the same `exchangeData()` with
1 ms per hand over, and one ping request. The device and the client have addresses, so the ping
travels as a routed frame of version 2.

- `printLine()` and `printWarning()` at the top write to the standard output and the standard
  error. The host library has `HostUtils::printLine()`, but it needs Qt, so this project brings
  its own two lines.
- `MockClientHandler::onReceivedPingResponse()` keeps that a ping response arrived and prints
  it with the source address of the response, the node that answered, and its round trip time.
- `getRouterConfiguration()` fills a `Router::RouterConfiguration` with a node address; the time
  to live keeps its default of 8.
- `// join the device of node 2 and the client of the host in memory` in `main()` creates the
  device application with the node address 2 on one mock transport and the device client with
  the host address on the other. `setDestinationAddress(2)` makes every request of the client a
  routed frame to node 2. Without the two router configurations both send the direct frames of
  version 1, as in example 21.
- `// device must have answered the ping request` ends the program with exit code 1 when no
  ping response arrived.

## Step 5: Run it

Run `build/consume_with_add_subdirectory`. You get:

```
Consumer sends a ping request to node 2 in memory
Ping answered by node 2 after 2 ms
```

Then run `echo $?` to see the exit code, in PowerShell `$LASTEXITCODE`. It prints `0`.

## Step 6: Understand the output

- `Ping answered by node 2`: the device application of node 2 answered the routed ping request
  of the client with a routed ping response to the host address, through the link layer of
  sercom in both directions.
- `after 2 ms`: 1 ms there and 1 ms back on the mock clock, the same on every run.

The configure step of Before you start also printed sercom's status block:

```
-- >>> PROJECT_NAME        : sercom (sub)
```

`(sub)` says that sercom found another project around it: `SERCOM_IS_MAIN_PROJECT` is off, and
with it the default of `SERCOM_BUILD_TESTS`, `SERCOM_BUILD_BINARIES` and `SERCOM_BUILD_HOST`. As a
subproject sercom also adds none of its warning flags, no `-Werror` and no optimizer flags: its
libraries build with the flags of this project. There is no `QT_VERSION` line, because no Qt was
searched.

## Try it yourself

1. Prove that sercom asks for no Qt. Configure again with Qt forbidden:
   `cmake -S . -B build -DCMAKE_DISABLE_FIND_PACKAGE_QT=ON -DCMAKE_DISABLE_FIND_PACKAGE_Qt5=ON`.
   The configure step succeeds, where a `find_package(... REQUIRED)` of Qt would stop it with
   `A REQUIRED package cannot be disabled`. CMake also warns that the two variables were not used
   by the project: no `find_package` of Qt ran at all.
2. Look for sercom's warning flags. Configure with `-DCMAKE_EXPORT_COMPILE_COMMANDS=ON` and run
   `grep -c -- -Werror build/compile_commands.json`. It prints `0`: neither your file nor the sercom
   sources get `-Werror`.
3. Ask for the tests of sercom. Configure a new build directory:
   `cmake -S . -B build-tests -DSERCOM_BUILD_TESTS=ON`. The configure step stops with
   `The binaries and the tests need the host library, turn SERCOM_BUILD_HOST on`: the tests need
   the host library, and the host library needs Qt.
4. Build everything. Remove `EXCLUDE_FROM_ALL` from the `add_subdirectory` line and build again.
   The build now also compiles `sercom_firmware_lib`, which no target of this project links.

## Where this is used

- `CMakeLists.txt` of sercom: the main project check, the option `SERCOM_BUILD_HOST`, the C++17
  of the consumers, the namespaced aliases, and the warnings and optimizers of the main project
  only.
- `examples/21_device_in_one_process.cpp`: the same device and client inside the main build.
- `examples/27_chain_in_one_process.cpp`: a client, a router and a device with addresses inside
  the main build.
- `README.md`, section "Building the project", "As a dependency": the steps of a project that uses
  sercom.
- Lectures: L54 "Catch2 v3 with CMake" (a submodule in `dep/` added with `add_subdirectory`), L81
  "A device simulator: firmware logic without hardware", L113 "Addressed applications: replies,
  reports and the --to option" and L115 "sercom as a CMake dependency: add_subdirectory,
  FetchContent, aliases".
