# Hardware Bring-Up

This document takes an ESP32 DevKit board from the box to a traced conversation with
the host tools, on Linux and in section 10 on Windows.
Sections 1 to 9 cover the device environments of
`firmware/esp32/platformio.ini`:

| Environment | Protocol | USB port of the board shows | Host tools open |
|---|---|---|---|
| `esp32dev` | UART0, the USB bridge of the board, 115200 baud | the binary frames | the USB port of the board, for example `/dev/ttyUSB0` or `COM3` on Windows |
| `esp32dev-console` | UART2, GPIO16 receives and GPIO17 transmits, 115200 baud | a readable console at 115200 baud | a 3.3 V USB-serial adapter on GPIO16 and GPIO17, for example `/dev/ttyUSB1` or `COM4` on Windows |

`esp32dev` needs nothing but the board. `esp32dev-console` keeps the USB port free
for text, so the boot messages, the log lines and one line per frame can be read
while the host talks to the device. The protocol code is the same in both; only the
UART and the console differ. The switch is `-DLET_PRINT_CONSOLE` in the environment's
`build_src_flags`, read once in `firmware/esp32/src/FirmwareSettings.cpp`.

## 1. What you need

- An ESP32 DevKit board (`esp32dev`) with a CP2102 or CH340 USB bridge and a USB
  cable with data lines.
- The activity LED is GPIO2 (`LED_PIN`), the blue LED of DOIT-style DevKit V1 boards.
  An Espressif ESP32-DevKitC has only a power LED; connect an LED with a 330 ohm
  resistor from GPIO2 to GND.
- For `esp32dev-console`: a USB-serial adapter set to 3.3 V (CP2102, CH340 or FT232)
  and three jumper wires.
- A Linux or Windows host with the build of `README.md` (`build/sercom_cli`) and
  PlatformIO (`pip install platformio`); check it with `pio --version`, and on Windows
  see section 10 when PowerShell does not find `pio`.
- On Linux your user in the `dialout` group (section 9), on Windows the driver of the
  USB bridge (section 10).

## 2. Wiring of the console environment

| USB-serial adapter | ESP32 DevKit | Purpose |
|---|---|---|
| TX | GPIO16 (RX2) | host to device |
| RX | GPIO17 (TX2) | device to host |
| GND | GND | common ground |
| VCC or 5V | not connected | the board is powered over its own USB port |

Use 3.3 V signal levels only. A 5 V adapter drives the ESP32 inputs above their
limit. The adapter's DTR and RTS lines stay unconnected, so opening the adapter does
not reset the board. The pins are `UART2_RECEIVE_PIN` and `UART2_TRANSMIT_PIN` in
`firmware/esp32/include/FirmwareSettings.h`. Boards with an ESP32-WROVER module use
GPIO16 and GPIO17 for their PSRAM; use a board with an ESP32-WROOM module.

## 3. Flashing

1. Plug the board in and find its port: `ls /dev/ttyUSB* /dev/ttyACM*` and
   `dmesg | tail` (a CP2102 shows `cp210x converter now attached to ttyUSB0`).
   The number depends on the plug order; `ls -l /dev/serial/by-id/` names the USB
   bridge behind each `/dev/ttyUSB` device and tells the board from the adapter.
2. Close every program that holds the port (serial monitors, `sercom_cli`).
3. Build both environments: `pio run -d firmware/esp32 -e esp32dev -e esp32dev-console`.
4. Flash one environment and name it; without `-e` PlatformIO flashes `esp32dev`:
   `pio run -d firmware/esp32 -e esp32dev -t upload --upload-port /dev/ttyUSB0`
   or
   `pio run -d firmware/esp32 -e esp32dev-console -t upload --upload-port /dev/ttyUSB0`.
5. If the upload stops at `Connecting...`, hold the BOOT button until the transfer starts.

## 4. First ping with `esp32dev`

The protocol runs on the USB port, so no serial monitor may be open.

```bash
build/sercom_cli /dev/ttyUSB0 --trace ping 3
```

`--trace` prints one line per frame on the wire, both directions, as it happens, and
the bytes that are not part of a frame. Opening the port resets most DevKit boards,
because the DTR and RTS lines of the USB bridge drive the EN and GPIO0 pins. The tool
therefore waits up to `DEVICE_BOOT_TIMEOUT_MSEC` (3000 ms, `host/include/HostSettings.h`)
for the first frame of the device before it sends the first request of the command.
It sends one ping request at the start of the wait for a device that is already
running and also waits until that ping request is acknowledged or has failed, so its
answer never mixes with the command. On the board the output has this form:

```text
[     0.000] Waiting for the device on /dev/ttyUSB0, 115200 for at most 3000 ms
[     0.000] >> #0     PING_REQUEST                 ACK_REQUEST                len   4  00 00 00 00  CRC ok
[     0.031] << unframed data, 26 bytes: "ets Jun  8 2016 00:22:57\r\n"
[     0.032] << unframed data, 2 bytes: "\r\n"
[     0.035] << unframed data, 57 bytes: "rst:0x1 (POWERON_RESET),boot:0x13 (SPI_FAST_FLASH_BOOT)\r\n"
...
[     0.060] << unframed data, 18 bytes: "entry 0x400805e4\r\n"
[     0.250] >> #0     PING_REQUEST                 ACK_REQUEST|RETRANSMISSION len   4  00 00 00 00  CRC ok
[     0.4xx] << #0     LOG_REPORT_RESPONSE          NONE                       len  49  01 2F 53 74 61 72 74 69 6E 67 20 74 68 65 20 44 ..  CRC ok
[     0.4xx] << #1     LOG_REPORT_RESPONSE          NONE                       len  90  01 58 53 74 61 72 74 65 64 20 74 68 65 20 44 65 ..  CRC ok
[     0.500] >> #0     PING_REQUEST                 ACK_REQUEST|RETRANSMISSION len   4  00 00 00 00  CRC ok
[     0.5xx] << #2     PING_RESPONSE                ACK_REQUEST                len   8  00 00 00 00 xx xx xx xx  CRC ok
[     0.5xx] << #0     PING_REQUEST                 ACK                        len   0  CRC ok
[     0.5xx] >> #2     PING_RESPONSE                ACK                        len   0  CRC ok
[     0.5xx] Device answered after 5xx ms
```

The exact boot text depends on the board and on the ROM version, and the values
marked `xx` depend on the boot time of the board. The ping request sent while the chip
boots is lost and goes out again with the `RETRANSMISSION` flag every 250 ms. The first
frames of the device are the two start-up log lines `Starting the DeviceApplication on
UART0, 115200` and `Started the DeviceApplication on UART0, 115200 for the device
sercom-esp32 version 1.0.0`; the next retransmission of the ping request is answered.
The same command against the device simulator (section 8) prints the frames without
the boot text.

### What a trace line means

```text
[     0.003] >> #1     PING_REQUEST                 ACK_REQUEST                len   4  03 00 00 00  CRC ok
```

| Field | Example | Meaning |
|---|---|---|
| time | `[     0.003]` | seconds since the first log line of the tool, the time of the log lines too, with millisecond decimals |
| direction | `>>` | `>>` host to device, `<<` device to host |
| message counter | `#1` | the MESSAGE_COUNTER field; an ACK or NACK repeats the counter and the message type of the message it confirms: `<< #0 PING_REQUEST ACK` is the device's acknowledgement of ping request `#0` |
| message type | `PING_REQUEST` | the name of MESSAGE_TYPE, or its number such as `0xBEEF` for an unknown type; an ACK or NACK shows the type of the message it confirms |
| flags | `ACK_REQUEST` | the names of the set FLAGS bits joined by `\|`, `NONE` without a set bit |
| payload length | `len   4` | the PAYLOAD_LENGTH field in bytes |
| payload | `03 00 00 00` | the payload in hex, the first 16 bytes followed by `..` for a longer payload |
| CRC result | `CRC ok` | `CRC bad` for a frame candidate whose CRC32 does not match |

An `unframed data` line holds bytes that were not part of a valid frame: boot text,
noise, a partial frame dropped after the inter byte timeout. Text, made of printable
characters, `\r`, `\n` and `\t` only, stands in quotes with `\r`, `\n`, `\t`, `\"` and
`\\` by name: `<< unframed data, 26 bytes: "ets Jun  8 2016 00:22:57\r\n"`. Any other
bytes stand in hex like a payload: `<< unframed data, 3 bytes: A5 C3 0A`. The bytes are
printed at the line feed of a text line, at 80 bytes (`MAXIMUM_UNFRAMED_DATA_SIZE`),
before the next frame and after 50 ms of silence; a line feed inside binary bytes does
not end them.

One request produces four frames: the request with `ACK_REQUEST`, the response with
`ACK_REQUEST` and its own counter, the device's `ACK` of the request and the host's
`ACK` of the response. A retransmission shows as the same counter and type with the
`RETRANSMISSION` flag, exactly as it goes on the wire.

## 5. The monitor command

```bash
build/sercom_cli /dev/ttyUSB0 monitor 20
```

The monitor command prints every log report of the device with its level, for example
`[     0.412] Device log DEBUG: Starting the DeviceApplication on UART0, 115200`, the
second start-up line with the device name and version and every 5 seconds the
heartbeat `Device log DEBUG: Heartbeat: free heap ... bytes, protocol stack headroom
... bytes`. A warning of the device is printed as `WARNING Device log WARNING: ...`.
To see telemetry as well, set a period first: `build/sercom_cli /dev/ttyUSB0 telemetry
1000 10` prints `telemetry #0: uptime ... ms, free heap ... bytes, temperature ... C`
once per second for 10 seconds.

## 6. The console environment

1. Flash `esp32dev-console` (section 3) and wire the adapter (section 2).
2. Open the console on the USB port of the board:
   `pio device monitor -d firmware/esp32 -e esp32dev-console -p /dev/ttyUSB0`.
   Opening it resets the board, so the boot text comes first.
3. Run the host tools on the adapter port: `build/sercom_cli /dev/ttyUSB1 --trace ping 3`.
   `ls -l /dev/serial/by-id/` names the USB bridge of the board and of the adapter if
   the numbers are not clear. The board does not reset and answers the ping request of
   the boot wait at once; the tool prints `Device answered after N ms`, where N is the
   time of that first exchange over the adapter.

The console shows the ROM boot text, a boot banner, the log lines and one line per
frame in both directions, in the columns of the host trace. A ping at 12 seconds after
the two heartbeats of 5 and 10 seconds has this form:

```text
sercom-esp32 version 1.0.0 git <hash of the build>
protocol on UART2 at 115200 baud, receive pin GPIO16, transmit pin GPIO17
console on UART0 at 115200 baud
[     0.312] log DEBUG: Starting the DeviceApplication on UART2, 115200
[     0.313] << #0     LOG_REPORT_RESPONSE          NONE                       len  49  01 2F 53 74 61 72 74 69 6E 67 20 74 68 65 20 44 ..  CRC ok
[     0.313] log DEBUG: Started the DeviceApplication on UART2, 115200 for the device sercom-esp32 version 1.0.0
[     0.314] << #1     LOG_REPORT_RESPONSE          NONE                       len  90  01 58 53 74 61 72 74 65 64 20 74 68 65 20 44 65 ..  CRC ok
[     5.312] log DEBUG: Heartbeat: free heap ... bytes, protocol stack headroom ... bytes
[     5.312] << #2     LOG_REPORT_RESPONSE          NONE                       len  xx  01 xx 48 65 61 72 74 62 65 61 74 3A 20 66 72 65 65 ..  CRC ok
[    10.312] log DEBUG: Heartbeat: free heap ... bytes, protocol stack headroom ... bytes
[    10.312] << #3     LOG_REPORT_RESPONSE          NONE                       len  xx  01 xx 48 65 61 72 74 62 65 61 74 3A 20 66 72 65 65 ..  CRC ok
[    12.034] >> #0     PING_REQUEST                 ACK_REQUEST                len   4  00 00 00 00  CRC ok
[    12.035] << #4     PING_RESPONSE                ACK_REQUEST                len   8  00 00 00 00 xx xx xx xx  CRC ok
[    12.035] << #0     PING_REQUEST                 ACK                        len   0  CRC ok
[    12.036] >> #4     PING_RESPONSE                ACK                        len   0  CRC ok
...
```

The time of a console line is the time since the board booted, `xx` stands for the
values that depend on the board. Every log line is followed by the frame that carries
it to the host. The ping request `#0` is the one of the boot wait; the three pings of
the command follow as `#1` to `#3` with four frames each. The lines wait in a 4 KB
console buffer (`CONSOLE_TRANSMIT_BUFFER_SIZE`) that a console task of its own writes
to the USB port. When the console cannot keep up (a stress test sends more lines than
115200 baud carries), lines are dropped instead of slowing the protocol down, and the
next printed line is preceded by `N console lines dropped`.

In both environments the LED blinks for 30 ms (`ACTIVITY_BLINK_DURATION_MSEC`) after a
valid received frame, at most once per 100 ms (`ACTIVITY_BLINK_PERIOD_MSEC`). The blink
shows the opposite of the LED state and then restores the state that the last
`SET_LED_REQUEST` asked for, so `led on` keeps the LED on with short dark blinks. On
`esp32dev` each command resets the board, so the LED starts off again; to watch the
dark blinks, use `esp32dev-console`: `led on`, then `ping 3` on the adapter port.

## 7. Further commands

1. Device information: `build/sercom_cli /dev/ttyUSB0 info`.
2. The LED: `build/sercom_cli /dev/ttyUSB0 led on`, then `led off`.
3. A short stress test with every frame: `build/sercom_cli /dev/ttyUSB0 --trace stress 20 16 2`.
4. A long stress test without the trace: `build/sercom_cli /dev/ttyUSB0 stress 1000 64 4`.
5. The stack headroom of the protocol task on `esp32dev-console`: after
   `build/sercom_cli /dev/ttyUSB1 --trace stress 20 16 2`, `build/sercom_cli /dev/ttyUSB1 monitor 10`
   must show a `protocol stack headroom` above 1024 bytes in both heartbeat lines.

`--trace` works with every command and may stand anywhere on the command line.

## 8. Rehearsal with the device simulator

Every step above can be rehearsed without a board, see `tools/SOCAT.md`. The device
simulator takes `--trace` too. These steps in one terminal, from the source directory
of a Release build, made the captures below; the times differ from run to run:

1. Start a clean virtual link between `/tmp/ttyV1` and `/tmp/ttyV2`: `tools/noisy_link.sh --profile clean &`
2. Start the simulator with a heartbeat every second: `build/sercom_device_sim /tmp/ttyV2 --log-period 1000 &`
3. Let the simulator run for a second: `sleep 1`
4. Ping with the trace: `build/sercom_cli /tmp/ttyV1 --trace ping 1`
5. Watch the device for two seconds with the trace: `build/sercom_cli /tmp/ttyV1 --trace monitor 2`
6. Stop the simulator and the link: `kill %2 %1`

```text
$ build/sercom_cli /tmp/ttyV1 --trace ping 1
[     0.000] Starting the CliApplication on /tmp/ttyV1, 115200
[     0.000] CliApplication trying to connect to the serial port
[     0.000] CliApplication connected to serial port from /tmp/ttyV1, 115200
[     0.000] Started the CliApplication on /tmp/ttyV1, 115200
[     0.000] Waiting for the device on /tmp/ttyV1, 115200 for at most 3000 ms
[     0.000] >> #0     PING_REQUEST                 ACK_REQUEST                len   4  00 00 00 00  CRC ok
[     0.000] << #0     LOG_REPORT_RESPONSE          NONE                       len  13  01 0B 68 65 61 72 74 62 65 61 74 20 31  CRC ok
[     0.000] << #1     PING_RESPONSE                ACK_REQUEST                len   8  00 00 00 00 E9 03 00 00  CRC ok
[     0.000] << #0     PING_REQUEST                 ACK                        len   0  CRC ok
[     0.000] >> #1     PING_RESPONSE                ACK                        len   0  CRC ok
[     0.000] Device answered after 0 ms
[     0.003] >> #1     PING_REQUEST                 ACK_REQUEST                len   4  03 00 00 00  CRC ok
[     0.003] << #2     PING_RESPONSE                ACK_REQUEST                len   8  03 00 00 00 EC 03 00 00  CRC ok
[     0.003] << #1     PING_REQUEST                 ACK                        len   0  CRC ok
ping response: round trip 0 ms, device uptime 1004 ms
[     0.003] >> #2     PING_RESPONSE                ACK                        len   0  CRC ok
[     0.003] Closing the CliApplication on /tmp/ttyV1, 115200
[     0.003] Closed the CliApplication on /tmp/ttyV1, 115200
```

The heartbeat log of the simulator (`<< #0 LOG_REPORT_RESPONSE`) arrives during the boot
wait. The ping command prints no device log; the monitor command prints every one as a
`Device log` line after its frame:

```text
$ build/sercom_cli /tmp/ttyV1 --trace monitor 2
...
[     0.001] Device answered after 1 ms
[     0.997] << #4     LOG_REPORT_RESPONSE          NONE                       len  13  01 0B 68 65 61 72 74 62 65 61 74 20 32  CRC ok
[     0.998] Device log DEBUG: heartbeat 2
[     1.000] >> #1     PING_REQUEST                 ACK_REQUEST                len   4  E8 03 00 00  CRC ok
[     1.000] << #5     PING_RESPONSE                ACK_REQUEST                len   8  E8 03 00 00 D5 07 00 00  CRC ok
[     1.000] << #1     PING_REQUEST                 ACK                        len   0  CRC ok
[     1.000] >> #5     PING_RESPONSE                ACK                        len   0  CRC ok
[     1.998] << #6     LOG_REPORT_RESPONSE          NONE                       len  13  01 0B 68 65 61 72 74 62 65 61 74 20 33  CRC ok
[     1.998] Device log DEBUG: heartbeat 3
[     2.000] >> #2     PING_REQUEST                 ACK_REQUEST                len   4  D0 07 00 00  CRC ok
[     2.000] << #7     PING_RESPONSE                ACK_REQUEST                len   8  D0 07 00 00 BD 0B 00 00  CRC ok
[     2.000] << #2     PING_REQUEST                 ACK                        len   0  CRC ok
[     2.000] >> #7     PING_RESPONSE                ACK                        len   0  CRC ok
...
```

The trace lines go to standard output together with the command results, the log
lines go to standard error. `2>/dev/null` keeps only the frames and the results.

## 9. Troubleshooting

| What you see | Cause | Remedy |
|---|---|---|
| `can not connect to serial port on /dev/ttyUSB0, 115200 (OPEN_FAILED)`, `ls -l /dev/ttyUSB0` shows group `dialout` | your user may not open the port | `sudo usermod -aG dialout $USER`, then log out and in again; `groups` must list `dialout` |
| A CH340 board appears and disappears again, `dmesg` shows `usbfs: interface 0 claimed by ch341 while 'brltty' sets config` | the braille display service of Ubuntu takes CH340 adapters | `sudo apt remove brltty`, plug the board in again |
| `OPEN_FAILED` although the permissions are right, or the frames arrive in parts | another program holds the port: a serial monitor, `screen`, `minicom`, a second `sercom_cli`, the PlatformIO upload | `fuser -v /dev/ttyUSB0` names the program; close it |
| `unframed data` lines with boot text right after the start, the uptime of `info` is always small | the board resets when the port opens | expected on DevKit boards; the boot wait covers it; `esp32dev-console` avoids the reset |
| only `unframed data` lines in hex, no frame, `Device did not answer within 3000 ms` | wrong baud rate on one side | both sides use 115200 (`SERIAL_BAUD_RATE` in `FirmwareSettings.h`, `--baud` of the tools) |
| boot text appears as noise in the statistics (`decoder bytes discarded`) | the ROM bootloader prints on UART0 before the firmware starts | expected; the decoder drops it and `--trace` shows it as text |
| `esp32dev-console`: nothing or garbage on the adapter port | no common ground, TX and RX swapped, or a 5 V adapter | GND to GND, adapter TX to GPIO16, adapter RX to GPIO17, 3.3 V adapter |
| `esp32dev-console`: the console stays empty | the console is the USB port of the board, not the adapter | `pio device monitor ... -p` with the board's port at 115200 baud |
| `Device did not answer within 3000 ms` with the right baud rate | firmware not flashed, board held in reset or in download mode, wrong port | press EN, flash again, try the other `/dev/ttyUSB` device (`ls -l /dev/serial/by-id/` names them) |
| `N console lines dropped` on the console | the console prints more than 115200 baud carries | expected during a stress test; the protocol keeps its speed |

## 10. Windows

The host tools take the COM port name of the board instead of a device path, for
example `build/sercom_cli.exe COM3 --trace ping 3`. Every step above
works the same way with that name; `pio run ... --upload-port COM3`
and `pio device monitor ... -p COM3` take it too.

1. Install the driver of the USB bridge once: Windows 10 and 11 install the CP2102
   driver of Silicon Labs (`CP210x Universal Windows Driver`) and the CH340 driver of
   WCH (`CH341SER.EXE`) by themselves through Windows Update on most machines; install
   the driver of the vendor when the board shows up without a COM port.
2. Plug the board in and find its port: the Device Manager lists it under
   `Ports (COM & LPT)` as `Silicon Labs CP210x USB to UART Bridge (COM3)` or
   `USB-SERIAL CH340 (COM3)`, and `[System.IO.Ports.SerialPort]::GetPortNames()` in
   PowerShell prints every COM port. Windows remembers the number it gave a bridge; the
   advanced port settings of the Device Manager change it.
3. Write the name in capitals: `QSerialPort` puts the `\\.\` prefix that `COM10` and
   above need in front of a name that starts with `COM`; `\\.\COM10` works as well.
4. A COM port opens only once: a second program, a serial monitor or a PlatformIO
   upload that holds the port makes the tools fail with `OPEN_FAILED`.
5. Check that PlatformIO runs after `pip install platformio`: `pio --version`. When
   PowerShell does not find `pio`, pip put it into a `Scripts` directory that is not on
   the path and names that directory in its warning; put the directory on the path
   (`$env:PATH = "<Scripts directory>;$env:PATH"`) or call `python -m platformio` in
   place of `pio`.

`QSerialPort` switches RTS off when it opens a port on Windows and leaves DTR as the
driver had it, while Linux raises both. On most DevKit boards RTS drives EN, so the
board keeps running when a tool opens the port on Windows and the boot text of
section 4 only shows after a press of EN; the boot wait of the tools covers both cases.

| What you see | Cause | Remedy |
|---|---|---|
| `can not connect to serial port on COM3, 115200 (OPEN_FAILED)` | the port does not exist or another program holds it | check the number in the Device Manager, close serial monitors, PlatformIO and other tools |
| The board shows up as `CP2102 USB to UART Bridge Controller` with a yellow mark and no COM port | the driver of the USB bridge is missing | install the driver of Silicon Labs or WCH, then plug the board in again |
| `sercom_cli.exe` does not start: `Qt5Cored.dll was not found`, `Qt5SerialPortd.dll was not found` or the same for a MinGW DLL such as `libstdc++-6.dll`; a Release build names `Qt5Core.dll` and `Qt5SerialPort.dll` | the program runs outside the build directory without its DLLs | run it from the build directory, where `windeployqt` put the DLLs, or put `C:\Qt\5.12.12\mingw73_64\bin` on the path |
| `sercom_test.exe` does not start: `Qt5Testd.dll was not found`, in a Release build `Qt5Test.dll` | `windeployqt` deploys only the DLLs of `sercom_cli.exe`, and Qt Test is not one of them | put `C:\Qt\5.12.12\mingw73_64\bin` on the path, as step 7 of the Windows build in `README.md` does |
| No log lines in a file, only the results | the program runs without a console window, Qt then logs to the debugger | set `QT_FORCE_STDERR_LOGGING=1`, the PowerShell scripts set it themselves |
