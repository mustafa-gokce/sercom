# 24 Serial Ping

This is the smallest real host program: it opens the serial port you name on the command line,
waits until the device answers, sends one ping request and prints the round trip time. You run it
against the device simulator over a pair of pseudo terminals on Linux or a com0com pair on Windows,
or against an ESP32 with the sercom firmware.

## What you will learn

- How `SerialPort` and `DeviceClient` fit together in a host program.
- How the Qt event loop hands the received bytes to the client and a timer keeps the
  retransmissions going.
- Why a host waits for the device after it opens the port.

## Before you start

1. Watch lectures L77 "The Qt event loop: one thread, signals and a timer" and L78 "DeviceClient: requests and replies" (section S12). You also need three terminals.
2. Install the build tools and Qt 5.12.12 once: `sudo apt install build-essential cmake ninja-build git socat` and the aqtinstall steps of README.md, section "Building the project", which also has the steps for Windows. On Windows create the com0com pair `COM10` and `COM11` of `tools/COM0COM.md`.
3. Open a terminal and go to the source directory of sercom, the directory that holds `CMakeLists.txt`. On Windows open PowerShell 7 with the path of the README.
4. If you cloned the repository just now, fetch its submodules once: `git submodule update --init --recursive`
5. Configure the build: `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH=$HOME/Qt/5.12.12/gcc_64`, on Windows with `-DCMAKE_PREFIX_PATH=C:/Qt/5.12.12/mingw73_64`
6. Build every program: `cmake --build build`
7. The program of this example is `build/sercom_example_24_serial_ping`, `build/sercom_example_24_serial_ping.exe` on Windows.

To build only this example, run `cmake --build build --target sercom_example_24_serial_ping`.
This guide also uses `build/sercom_device_sim`, which `cmake --build build` builds as well.

## Step 1: Read the command line

Open `examples/24_serial_ping.cpp` and find `// command line must be valid` in `main()`.
`parseCommandLine()` accepts exactly one argument, the serial port, for example `/tmp/ttyV1` or
`/dev/ttyUSB0` on Linux and `COM10` or `COM3` on Windows. Anything else prints the usage text and
exits with code 64
(`HostSettings::INVALID_COMMAND_LINE_EXIT_CODE`).

## Step 2: Read how the port and the client meet

Find `// serial port must open`. The port opens at 115200 baud, 8 data bits, no parity, 1 stop
bit (*8N1*). Find `// create the device client on the serial port`. `SerialPort` is a
`Transport`, so the device client writes its frames straight into the port. `SerialPort` is also a
`QObject`: `// hand the received data to the device client` connects its signal `receivedData` to the
client, and `// stop the wait with a failure when the serial port fails` connects `errorHappened`
to a warning and the end of the event loop.
`HostUtils::SteadyClock` is the clock of the host, and `MockClientHandler` keeps the round trip
time and the device uptime of the latest ping response. The interface asks for a callback per
message, so the class ends with a `public:` section of empty callbacks: skip the empty callbacks.

## Step 3: Read the event loop

Read `waitForPingResponse()` at the end of the file. It runs a `QEventLoop` until a new ping
response arrived. The bytes need no code of their own there: whenever some arrive, the event loop
emits `receivedData` and the connection of Step 2 hands them to the client, which decodes the
frames and calls the handler. A `QTimer` calls its lambda every `HostSettings::MAINTAIN_PERIOD_MSEC`
(2 ms), and one tick does three things:

- `// wait must go on until a new ping response, the timeout or a stop signal`: the loop ends when a
  new ping response arrived, when the timeout passed, or when you press Ctrl+C.
- `// ping request must be sent when no request waits`: a new ping goes out when the client has
  no request waiting for its acknowledgement, at the start and after a ping that timed out.
- `// maintain the retransmissions of the device client`: `maintain()` sends a ping again when its
  answer is late.

This is the *event loop* of L77, without threads; Qt does the waiting for bytes and for the timer.

`main()` calls it twice. `// wait for the device within the boot timeout` waits up to
`HostSettings::DEVICE_BOOT_TIMEOUT_MSEC` (3000 ms): an ESP32 may restart when the port opens and
then needs a moment to boot. `// wait for the response to one more ping request` then measures one
ping on a device that is surely running. After each wait, `// stop signal must not have arrived`
ends the program with 0 when you pressed Ctrl+C, before `// device must answer ...` warns that
the device stayed silent.

## Step 4: Run it against the device simulator

1. Start a virtual cable in the first terminal, as in example 01:
   `socat PTY,raw,echo=0,link=/tmp/ttyV1 PTY,raw,echo=0,link=/tmp/ttyV2`.
   It links `/tmp/ttyV1` and `/tmp/ttyV2`, prints nothing and keeps running.
2. Start the simulator on one end in the second terminal: `build/sercom_device_sim /tmp/ttyV2`
3. Run the example on the other end in the third terminal:
   `build/sercom_example_24_serial_ping /tmp/ttyV1`

On Windows the com0com pair is the cable: start the simulator with `build/sercom_device_sim.exe COM11`
in the second terminal and run `build/sercom_example_24_serial_ping.exe COM10` in the third one.

You get:

```
waiting for the device on /tmp/ttyV1, 115200 for at most 3000 ms
device answered, sending one ping request
ping response: round trip 1 ms, device uptime 1003 ms
```

Then run `echo $?` to see the exit code, in PowerShell `$LASTEXITCODE`. It prints `0`. The two numbers come from real clocks, so
your run shows other values: the round trip is usually 0 or 1 ms on a pseudo terminal, and the
uptime is the time since you started the simulator. Keep the simulator and the cable running for
the changes under "Try it yourself".

## Step 5: Understand the output

- `waiting for the device on /tmp/ttyV1, 115200 ...`: the port is open, the boot wait starts.
- `device answered`: the first ping got its response.
- `round trip 1 ms`: the time from sending the second ping request to receiving its response.
- `device uptime 1003 ms`: the time since the device started, as the device reports it.

## Step 6: Run it against an ESP32

You need a board with the sercom firmware. Flash the `esp32dev` environment, which puts the
protocol on the USB port: `pio run -d firmware/esp32 -e esp32dev -t upload`.
Close every serial monitor first: only one program may use the port.

1. Plug the board in and find its port, usually `/dev/ttyUSB0`, on Windows `COM3` or the number
   the Device Manager shows.
2. Run `build/sercom_example_24_serial_ping /dev/ttyUSB0`.

On Linux opening the port restarts the board. Its boot text arrives first; the decoder skips it,
because it is no frame. On Windows the board usually keeps running.
Either way the device answers within the boot wait and the program prints the
same three lines as above, with a round trip of a few milliseconds over the USB serial adapter.

## Try it yourself

1. Run it with no device. Stop the simulator with Ctrl+C and run the example again. It waits three
   seconds and prints a line ending in `WARNING The device did not answer within the boot timeout`.
   The exit code is `1`.
2. Start the device late. Stop the simulator, run the example first and start the simulator one
   to two seconds later with `build/sercom_device_sim /tmp/ttyV2 --trace`. The example still
   prints `device answered`. The first ping in the trace usually carries the `RETRANSMISSION`
   flag, because the simulator sees only what arrives after it opened the port. Its counter is
   `#0` when the simulator started within about 1.5 s, and `#1` when it started later: ping `#0`
   gives up after 250 ms and five retransmissions, and a new ping goes out.
3. Watch the frames. Start the simulator with its trace: `build/sercom_device_sim /tmp/ttyV2 --trace`.
   It prints eight trace lines per run: the ping `#0` of the boot wait and the measured ping `#1`,
   each with its response and two acknowledgements, for example
   `[     1.002] >> #0     PING_REQUEST                 ACK_REQUEST                len   4  00 00 00 00  CRC ok`.

Stop the simulator and the cable with Ctrl+C when you are done.

## Where this is used

- `host/include/SerialPort.h` and `host/src/SerialPort.cpp`: opening the port on `QSerialPort`,
  the signals `receivedData` and `errorHappened`, and writing.
- `app/include/DeviceClient.h` and `app/src/DeviceClient.cpp`: the ping request and the round trip
  time.
- `host/src/CliApplication.cpp`: `waitForDevice()` and the `ping` command of `sercom_cli`, the full
  version of this program.
- `tools/SOCAT.md`: the virtual cable; `tools/noisy_link.sh` adds noise to it. `tools/COM0COM.md`
  and `tools/noisy_link.ps1` do the same on Windows.
- Lectures: L77 "The Qt event loop: one thread, signals and a timer", L78 "DeviceClient: requests
  and replies", L79 "The sercom_cli tool on Linux and Windows" and L89 "Flashing and the first
  ping"; the virtual cable is L19 "Pseudo terminals and socat", on Windows L20 "Virtual COM port
  pairs on Windows: com0com".
