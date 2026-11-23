# Hardware Bring-Up

This document takes an ESP32 DevKit board from the box to a traced conversation with
the host tools, on Linux and in section 10 on Windows. Sections 11 to 14 do the same for
the STM32F4 Nucleo boards, wire two boards to each other, run both targets in QEMU
without a board and chain three boards with version 2 of the protocol. Sections 1 to 9
cover the device environments of
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
| only `unframed data` lines in hex, no frame, `Device did not answer within 3000 ms` | wrong baud rate on one side | both sides use 115200 (`SERIAL_BAUD_RATE` in `firmware/common/include/CommonSettings.h`, `--baud` of the tools) |
| boot text appears as noise in the statistics (`decoder bytes discarded`) | the ROM bootloader prints on UART0 before the firmware starts | expected; the decoder drops it and `--trace` shows it as text |
| `esp32dev-console`: nothing or garbage on the adapter port | no common ground, TX and RX swapped, or a 5 V adapter | GND to GND, adapter TX to GPIO16, adapter RX to GPIO17, 3.3 V adapter |
| `esp32dev-console`: the console stays empty | the console is the USB port of the board, not the adapter | `pio device monitor ... -p` with the board's port at 115200 baud |
| `Device did not answer within 3000 ms` with the right baud rate | firmware not flashed, board held in reset or in download mode, wrong port | press EN, flash again, try the other `/dev/ttyUSB` device (`ls -l /dev/serial/by-id/` names them) |
| `N console lines dropped` on the console | the console prints more than 115200 baud carries | expected during a stress test; the protocol keeps its speed |

## 10. Windows

The host tools take the COM port name of the board instead of a device path, for
example `build/sercom_cli.exe COM3 --trace ping 3`. Every step above and the Nucleo
boards of section 11 work the same way with that name; `pio run ... --upload-port COM3`
and `pio device monitor ... -p COM3` take it too. The QEMU steps of section 13 need a
Linux host.

1. Install the driver of the USB bridge once: Windows 10 and 11 install the CP2102
   driver of Silicon Labs (`CP210x Universal Windows Driver`) and the CH340 driver of
   WCH (`CH341SER.EXE`) by themselves through Windows Update on most machines; install
   the driver of the vendor when the board shows up without a COM port. A Nucleo board
   needs the ST-LINK driver of STMicroelectronics (`STSW-LINK009`), PlatformIO flashes
   through it and its virtual COM port shows up as
   `STMicroelectronics STLink Virtual COM Port (COM5)`.
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

## 11. STM32F4 Nucleo boards

`firmware/stm32f4` is a bare metal PlatformIO project: the CMSIS headers and start-up
code of `ststm32@19.7.1`, no HAL, no RTOS, no heap and no exceptions. It runs the same
device role of `firmware/common` as the ESP32 in a superloop. Its environments:

| Environment | Board | Protocol | Console | Host tools open |
|---|---|---|---|---|
| `nucleo_f401re` | Nucleo-F401RE | USART2, the ST-LINK virtual COM port | none | the virtual COM port, for example `/dev/ttyACM0` or `COM5` on Windows |
| `nucleo_f411re` | Nucleo-F411RE | USART2, the ST-LINK virtual COM port | none | the virtual COM port, for example `/dev/ttyACM0` or `COM5` on Windows |
| `nucleo_f401re-console` | Nucleo-F401RE | USART1, PA9 transmits (D8), PA10 receives (D2) | USART2, the virtual COM port | a 3.3 V USB-serial adapter on PA9 and PA10, for example `/dev/ttyUSB0` or `COM4` on Windows |
| `nucleo_f401re-controller` | Nucleo-F401RE | controller of a device on USART1 | USART2, the virtual COM port | none, the controller talks to a device (section 12) |
| `netduinoplus2-qemu` | STM32F405 of the QEMU machine `netduinoplus2` | USART2 | none | the link `usart2` of `tools/run_emulator.sh` on Linux (section 13) |
| `netduinoplus2-qemu-controller` | STM32F405 of the QEMU machine `netduinoplus2` | controller on USART1 | USART2 | none, the controller talks to a device (section 13) |
| `nucleo_f401re-router` | Nucleo-F401RE | router node 3 on USART1 and USART6 | USART2, the virtual COM port | none, the host talks to node 3 through node 2 (section 14) |
| `netduinoplus2-qemu-router` | STM32F405 of the QEMU machine `netduinoplus2` | router node 3 on USART1 and USART6 | USART2 | none, the host talks to node 3 through node 2 (section 14) |

The ST-LINK of a Nucleo-64 board carries USART2 (PA2 transmits, PA3 receives) to the
host as a virtual COM port, `/dev/ttyACM0` on Linux and a COM port such as `COM5` on
Windows (section 10). Opening it does not reset the chip and the chip prints no boot
text, so the first frame of the host is answered at once. The user LED LD2 is PA5
(`LED_PIN` in `firmware/stm32f4/include/FirmwareSettings.h`).

1. Build every environment: `pio run -d firmware/stm32f4 -e nucleo_f401re -e nucleo_f411re -e nucleo_f401re-console -e nucleo_f401re-controller -e netduinoplus2-qemu -e netduinoplus2-qemu-controller -e nucleo_f401re-router -e netduinoplus2-qemu-router`.
2. Connect the ST-LINK USB connector of the board.
3. Flash the device firmware through the ST-LINK: `pio run -d firmware/stm32f4 -e nucleo_f401re -t upload`.
4. Print the device information with every frame: `build/sercom_cli /dev/ttyACM0 --trace info`, on Windows `build/sercom_cli.exe COM5 --trace info`.
5. Run the other commands of section 7 on `/dev/ttyACM0` or `COM5`.

How the firmware uses the chip:

| Part | Choice | Why |
|---|---|---|
| Clock | HSI at 16 MHz, the clock after reset, without the PLL | nothing in the protocol needs more, and no clock-ready flag must be waited for |
| Time | SysTick interrupt every millisecond counts a 64 bit `Stm32Clock` | the clock of `core/include/Clock.h` must not wrap; a read masks the interrupt for the two loads |
| Baud rate | BRR 139 at 16 MHz, 115108 baud, 0.08 % below 115200 | far inside the tolerance of a UART |
| Receive | RXNE interrupt into a 2048 byte ring buffer (`RING_BUFFER_SIZE`) | the superloop may be busy, the ring holds 178 ms of a full line |
| Transmit | TXE interrupt from a second 2048 byte ring buffer | `sendData` returns at once and waits only when the ring is full, the contract of the ESP32 UART driver; the same `UsartPort` is the console output, which drops a line instead of stalling |
| Loop | receive, heartbeat every 5 s, `maintain()`, then `__WFI()` | the SysTick interrupt wakes the loop at least once per millisecond |
| Free heap field | bytes between the end of the static data and the deepest stack use, found with a fill pattern written at the start | the firmware has no heap: `_sbrk` refuses every request, so this is the memory still free |
| Temperature | internal sensor on ADC1 channel 18 (16 on the F405) with the factory readings at 30 and 110 °C | a conversion that does not end within 2 ms reports `-32768` (-327.68 °C), the protocol value of no reading |

The heartbeat log line of the STM32F4 is `Heartbeat: free memory <bytes> bytes, dropped
received bytes <count>`.

## 12. Device to device

The controller role of `firmware/common/include/ControllerRole.h` runs the
`DeviceClient` on a microcontroller: it waits for the device, sends 3 ping requests,
asks for the device information, collects 3 telemetry reports at 200 ms, runs an echo
stress test of 300 requests with 32 byte payloads and 2 requests in flight, and prints
every result on its console. A request answered with NACK `BUSY` is sent again, any
other failed request fails the step. The last two lines are the result:

```text
[     2.746] log DEBUG: Controller passed every step
[     2.746] log DEBUG: Controller report: pings 3, info 1, telemetry 3, echoes 300 of 300, busy 0, failed 0
```

Wire the protocol ports of two boards to each other:

| Controller | Device | Purpose |
|---|---|---|
| ESP32 `esp32dev-controller`, GPIO17 (TX2) | Nucleo `nucleo_f401re-console`, PA10 (D2, USART1 RX) | controller to device |
| ESP32 `esp32dev-controller`, GPIO16 (RX2) | Nucleo `nucleo_f401re-console`, PA9 (D8, USART1 TX) | device to controller |
| GND | GND | common ground |

or the other way round:

| Controller | Device | Purpose |
|---|---|---|
| Nucleo `nucleo_f401re-controller`, PA9 (D8, USART1 TX) | ESP32 `esp32dev-console`, GPIO16 (RX2) | controller to device |
| Nucleo `nucleo_f401re-controller`, PA10 (D2, USART1 RX) | ESP32 `esp32dev-console`, GPIO17 (TX2) | device to controller |
| GND | GND | common ground |

- Transmit goes to receive in both directions; two transmit pins on one wire drive
  against each other.
- Both boards share ground and nothing else; each one is powered over its own USB port.
- Both chips use 3.3 V levels. Never connect a 5 V signal or an RS-232 port (±12 V)
  to a 3.3 V pin; an RS-232 device needs a level shifter such as a MAX3232.
- The device console runs on USART2, so the Nucleo device uses `nucleo_f401re-console`:
  PA2 and PA3 of `nucleo_f401re` are wired to the ST-LINK by the solder bridges SB13
  and SB14.
- Watch the controller on its console: the USB port of the ESP32 at 115200 baud, or
  the virtual COM port of the Nucleo controller, `/dev/ttyACM0` or `COM5` on Windows.
  Each start runs the steps once; press reset to run them again.

A board that passes the frames of the host on to the next board is a router of version 2
of the protocol, and section 14 chains three boards that way; [GATEWAY.md](GATEWAY.md)
explains why version 1 has no gateway.

## 13. Both targets in QEMU without a board

Both targets also run in QEMU: the ESP32 on Espressif's QEMU, the STM32F4 on the
`netduinoplus2` machine of the xPack QEMU for Arm, which models an STM32F405. The
emulator tools need a Linux host: `tools/run_emulator.sh` is a Bash script and
`tools/uart_bridge.py` creates pseudo terminals.

1. Build `build/sercom_cli` with Qt 5.12.12 as the Linux steps of `README.md` show.
2. Install socat and the libraries of Espressif's QEMU: `sudo apt install socat libgcrypt20 libglib2.0-0t64 libpixman-1-0 libsdl2-2.0-0 libslirp0`
3. Download and unpack [xPack QEMU Arm 9.2.4-1](https://github.com/xpack-dev-tools/qemu-arm-xpack/releases/tag/v9.2.4-1) and [Espressif QEMU esp-develop-9.2.2-20260417](https://github.com/espressif/qemu/releases/tag/esp-develop-9.2.2-20260417), the versions of the CI.
4. Name both emulators: `export ARM_QEMU=<xpack directory>/bin/qemu-system-arm ESP32_QEMU=<espressif directory>/qemu/bin/qemu-system-xtensa`
5. Build the emulated firmware: `pio run -d firmware/stm32f4 -e netduinoplus2-qemu -e netduinoplus2-qemu-controller` and `pio run -d firmware/esp32 -e esp32dev -e esp32dev-console -e esp32dev-controller`
6. Start the STM32F4 device: `tools/run_emulator.sh start stm32f4 firmware/stm32f4/.pio/build/netduinoplus2-qemu /tmp/stm32f4 --log usart1`
7. Talk to it like to a board: `build/sercom_cli /tmp/stm32f4/usart2 --trace info`, `build/sercom_cli /tmp/stm32f4/usart2 stress 300 32 2`; give the link with its directory, `QSerialPort` reads a bare `usart2` as `/dev/usart2`
8. Stop it: `tools/run_emulator.sh stop /tmp/stm32f4`
9. Start the ESP32 device the same way and use `/tmp/esp32/uart0`: `tools/run_emulator.sh start esp32 firmware/esp32/.pio/build/esp32dev /tmp/esp32 --log uart2`
10. Wire an emulated controller to an emulated device and run every check of the CI: `tests/e2e/emulator_test.sh build/sercom_cli firmware/esp32/.pio/build firmware/stm32f4/.pio/build`

`tools/run_emulator.sh` serves each serial port of QEMU on a unix socket and starts
`tools/uart_bridge.py` for it, which links a pseudo terminal of its own into the run
directory. QEMU's own `-serial pty` notices a program that opens the pseudo terminal
only about once a second and holds the bytes of that second back. Measured with the Qt
`sercom_cli` on the STM32F4 in 8 runs of each command, every command on QEMU's own
pseudo terminal sent its first ping request again every 250 ms: `ping 1` showed
`Device answered after` 679 to 1001 ms and `stress 300 32 2` 998 to 1005 ms with 3
retransmissions per stress test; the first command after a pause waits less, the next
ones the whole second. Through the bridge the first answer came after 2 to 4 ms, and
`stress 300 32 2` ran without a retransmission at 1.1 to 1.5 ms average and 3 to 5 ms
maximum latency. Like a USB bridge, it drops the output of the chip while no program
holds the port open.
`QSerialPort` opens a port in exclusive mode and ends the mode when it closes the port;
when a tool is killed before that, the bridge links a new pseudo terminal in place of
the refused one, as the port of a board is free again once the program is gone.

QEMU also writes the output of every port into `<port>.log` of the run directory, from
the first byte of the chip on, and `start` returns only once one of these logs holds the
start line of the firmware, `Started the DeviceApplication ...` or a controller's
`Started the DeviceClient ...`. The first command then finds a running chip, as on a
board that runs before a tool opens its port; the boot wait of the tools stays at
3000 ms for the reset of a board. Measured with `esp32dev` and
`sercom_cli <port> --trace info` right after `start` on a host with 20 cores: at a load
average of 14 to 21, 8 runs without the wait got the answer to the boot wait after 1946
to 2075 ms, 8 runs with it after 4 to 14 ms, `start` took 2.2 to 3.4 s. Beside two
parallel builds, 8 runs without the wait (load average 18 to 33) got no answer within
3000 ms in 2 runs and no answer to `info` in 6; 16 runs with the wait (load average 32
to 46) got the answer after 2 to 23 ms and completed `info` every time, `start` took 3.3
to 22.0 s. On such a busy host the second core of the emulated ESP32 can panic in its
start code (`Cache disabled but cached memory region accessed` in `uart0.log`) and the
chip boots again; `start` waits for that boot too, up to `BOOT_TIMEOUT_SECONDS` (60 by
default).

Two emulated chips are wired with socat between their pseudo terminals:
`socat /tmp/esp32/uart2,raw,echo=0 /tmp/stm32f4/usart2,raw,echo=0`; two
`tools/noise.py` processes in its place, or one with `--duplex`, add the noise of
`tools/noisy_link.sh`.

What differs from a board:

| | QEMU | Board |
|---|---|---|
| STM32F4 chip | STM32F405 with 1 MB flash, of its RAM the firmware uses the 128 KB SRAM; built by `netduinoplus2-qemu` | STM32F401RE (512 KB, 96 KB) or STM32F411RE (512 KB, 128 KB) |
| STM32F4 clock | the machine runs SysTick from a fixed 168 MHz clock, `LET_RUN_ON_QEMU` selects it; the RCC registers read 0 and ignore writes | HSI at 16 MHz after reset |
| STM32F4 serial ports | the baud rate register is ignored, bytes move at the speed of the host | 115108 baud from BRR 139 |
| STM32F4 LED | GPIO is not modelled, the LED state is only remembered | LD2 lights |
| STM32F4 temperature | the ADC conversion never ends, `sercom_cli` and a controller show the temperature as `no reading` | calibrated internal sensor |
| ESP32 | watchdogs off, the temperature reads `-17.78 C`, opening a port does not reset the chip | DTR and RTS reset the chip when a port opens on Linux, on Windows the chip keeps running (section 10) |

## 14. A daisy chain

Version 2 of the protocol ([PROTOCOL.md](PROTOCOL.md#8-version-2-routed-frames)) lets the
host reach a board through the boards in front of it. A router build runs the device role
of `firmware/common` on two serial ports: a frame for another node leaves on the port it
did not arrive on with its time to live one lower, and every link between two boards is a
version 1 link layer of its own with its own counters, acknowledgements and
retransmissions. The chain of three boards:

| Node | Board and environment | Port towards the host | Port away from the host | Console |
|---|---|---|---|---|
| 2 | ESP32 DevKit, `esp32dev-router` | UART0, the USB bridge | UART2, GPIO16 receives, GPIO17 transmits | none, both UARTs carry frames |
| 3 | Nucleo-F401RE, `nucleo_f401re-router`, proxy of node 4 | USART1, PA10 (D2) receives, PA9 (D8) transmits | USART6, PA12 (CN10 pin 12) receives, PA11 (CN10 pin 14) transmits | USART2, the ST-LINK virtual COM port |
| 4 | ESP32 DevKit, `esp32dev-chain` | UART2, GPIO16 receives, GPIO17 transmits | none, the end of the chain | UART0, the USB bridge |

The four environments of the chain:

| Environment | Build values | Node |
|---|---|---|
| `esp32dev-router` | `-DLET_RUN_ROUTER -DNODE_ADDRESS=2` | node 2 |
| `esp32dev-chain` | `-DLET_PRINT_CONSOLE -DNODE_ADDRESS=4` | node 4, the console environment with an address |
| `nucleo_f401re-router` | `-DLET_PRINT_CONSOLE -DLET_RUN_ROUTER -DNODE_ADDRESS=3 -DPROXY_ADDRESS=4` | node 3 |
| `netduinoplus2-qemu-router` | the values of `nucleo_f401re-router` and `-DLET_RUN_ON_QEMU` | node 3 in QEMU |

`FirmwareSettings.cpp` of each target reads the values once. `NODE_ADDRESS` is the address
of the node, 2 to 254; a build without it has no address and sends the bytes of version 1.
`PROXY_ADDRESS` names the neighbour below a router that has no address of its own, and
`LET_RUN_ROUTER` gives the device role its second port. A build stops when a router has no
address, and on the ESP32 when a router prints a console, because UART2 is its second port.
USART6 keeps the ST-LINK port free for the console of node 3 and needs no solder bridge:
PA11 and PA12 are free pins of the morpho connector CN10. They reach USART6 with alternate
function 8, where USART1 and USART2 use 7, and USART6 takes its clock from APB2 like USART1.

Wire the boards one link after the other:

| From | To | Purpose |
|---|---|---|
| ESP32 node 2, GPIO17 (TX2) | Nucleo node 3, PA10 (D2, USART1 RX) | down from node 2 |
| ESP32 node 2, GPIO16 (RX2) | Nucleo node 3, PA9 (D8, USART1 TX) | up to node 2 |
| Nucleo node 3, PA11 (CN10 pin 14, USART6 TX) | ESP32 node 4, GPIO16 (RX2) | down from node 3 |
| Nucleo node 3, PA12 (CN10 pin 12, USART6 RX) | ESP32 node 4, GPIO17 (TX2) | up to node 3 |
| GND of node 2 and of node 4 | GND of node 3 | common ground |

The rules of section 12 hold for both links: transmit goes to receive, the boards share
ground, the levels are 3.3 V only and each board is powered over its own USB port.

1. Build the four environments: `pio run -d firmware/esp32 -e esp32dev-router -e esp32dev-chain` and `pio run -d firmware/stm32f4 -e nucleo_f401re-router -e netduinoplus2-qemu-router`
2. Flash node 2 on its USB port: `pio run -d firmware/esp32 -e esp32dev-router -t upload --upload-port /dev/ttyUSB0`
3. Flash node 4 on its USB port: `pio run -d firmware/esp32 -e esp32dev-chain -t upload --upload-port /dev/ttyUSB1`
4. Flash node 3 through its ST-LINK: `pio run -d firmware/stm32f4 -e nucleo_f401re-router -t upload`
5. Wire the boards as the table shows.
6. Find every node of the chain: `build/sercom_cli /dev/ttyUSB0 discover`
7. Print the device information of node 4 through node 2 and node 3: `build/sercom_cli /dev/ttyUSB0 --to 4 info`
8. Run the echo stress test over three links: `build/sercom_cli /dev/ttyUSB0 --to 4 stress 300 32 2`
9. Watch node 3 on its console: `pio device monitor -d firmware/stm32f4 -e nucleo_f401re-router -p /dev/ttyACM0`, and node 4 on its own: `pio device monitor -d firmware/esp32 -e esp32dev-chain -p /dev/ttyUSB1`
10. Follow the heartbeat of node 2, which has no console: `build/sercom_cli /dev/ttyUSB0 monitor 10`
11. On Windows use the COM ports of section 10 in place of `/dev/ttyUSB0`, `/dev/ttyUSB1` and `/dev/ttyACM0`.

The console of node 3 traces the frames of USART1 with their route, and every 5 s a router
adds a second heartbeat line with its forwarded, expired and unreachable frames:

```text
[     0.016] log DEBUG: Routing as node 3 between USART1 and USART6, proxy address 4
[    11.009] << #77    ECHO_RESPONSE                ACK_REQUEST|ROUTE_CAPABLE  len  32  36 00 00 00 3A 3B 3C 3D 3E 3F 40 41 42 43 44 45 ..  CRC ok  route 4>1 ttl 7
[    30.020] log DEBUG: Heartbeat: forwarded 684, expired 1, unreachable 0, dropped secondary bytes 0
```

Node 4 may also be a device of version 1, for example the `esp32dev-console` firmware of
tag `L108` built in a worktree of its own (`git worktree add ../sercom-l108 L108`), or the
`esp32dev-console` of this tree, which has no address. Node 3 stands in for it as node 4,
its `PROXY_ADDRESS`: it sends the requests for node 4 down as direct frames and passes the
answers up as routed frames from node 4. `--to 4 info` then lists no protocol version and
no node address, and the first request after the swap waits for one timeout of node 3
before the proxy takes over, within the 3000 ms boot wait of `sercom_cli`.

The same chain runs in QEMU on Linux with the emulators of section 13:

1. Build `build/sercom_cli` and `build/sercom_dump` with Qt 5.12.12 as the Linux steps of `README.md` show.
2. Build the emulated chain: `pio run -d firmware/esp32 -e esp32dev-router -e esp32dev-chain -e esp32dev-console` and `pio run -d firmware/stm32f4 -e netduinoplus2-qemu-router`
3. Start node 4: `tools/run_emulator.sh start esp32 firmware/esp32/.pio/build/esp32dev-chain /tmp/node4 --log uart0`
4. Start node 3: `tools/run_emulator.sh start stm32f4 firmware/stm32f4/.pio/build/netduinoplus2-qemu-router /tmp/node3 --log usart2`
5. Join node 3 and node 4: `socat /tmp/node3/usart6,raw,echo=0 /tmp/node4/uart2,raw,echo=0 &`
6. Start node 2: `tools/run_emulator.sh start esp32 firmware/esp32/.pio/build/esp32dev-router /tmp/node2`
7. Join node 2 and node 3 through the noise of the realistic profile: `python3 tools/noise.py --drop 0.001 --flip 0.001 --insert 0.0005 --duplicate 0.0001 --maximum-bit-flips 1 --delay 0.0001 --burst 0.0001 --burst-length 5 --seed 1 --duplex /tmp/node2/uart2 /tmp/node3/usart1 &`
8. Talk to the chain like to the boards: `build/sercom_cli /tmp/node2/uart0 discover`, `build/sercom_cli /tmp/node2/uart0 --to 4 stress 300 32 2`
9. Decode what node 2 sent down, the requests of the host carry `ttl=7`: `build/sercom_dump /tmp/node2/uart2.log`
10. Decode what node 3 sent down, the same requests carry `ttl=6`: `build/sercom_dump /tmp/node3/usart6.log`
11. Read the console of node 3: `tr -d '\r' < /tmp/node3/usart2.log`
12. Stop the links and the chips: `kill %2 %1`, then `tools/run_emulator.sh stop /tmp/node2`, `tools/run_emulator.sh stop /tmp/node3` and `tools/run_emulator.sh stop /tmp/node4`
13. Run every check of the chain, with node 4 replaced by `esp32dev-console` at the end: `tests/e2e/emulator_chain_test.sh build/sercom_cli build/sercom_dump firmware/esp32/.pio/build firmware/stm32f4/.pio/build`; `END_DEVICE_BUILD_DIRECTORY=<L108 worktree>/firmware/esp32/.pio/build/esp32dev-console` makes the replaced node 4 a device of version 1.

Every STM32F4 run of `tools/run_emulator.sh` has a `usart6` link, so any emulated STM32F4
build can be the router of a chain. Measured in three runs of `emulator_chain_test.sh` with
its default seed on a host with 20 cores: `discover` heard node 2 after 1 to 3 ms, node 3
after 11 to 15 ms and node 4 after 11 to 24 ms; `--to 4 stress 300 32 2` verified all 300
echoes in 12.5 to 14.8 s at 83 to 98 ms average latency and 806 ms at most, with 0 busy
retries of the host and 0 overdue retries, because a node that a router refuses as `BUSY`
sends its frame again (PROTOCOL.md §8.7); `--to 4 info` through the proxy answered the boot
wait after 1013 to 1017 ms with the `esp32dev-console` of this tree as node 4 and after
2019 ms with the one of `L108`.

What a chain does not model:

- A per-hop acknowledgement says that the next board holds the frame, not that node 4 has
  it; the response, or a route failure report of the board that lost it, closes a request
  (PROTOCOL.md §8.8 and §8.10).
- Each board has two ports at most and no routing table: a frame that is not for a board
  leaves on its other port. Trees and stars, and answers to requests that come from below,
  are not part of version 2 (PROTOCOL.md §8.12).
- On Linux opening `/dev/ttyUSB0` resets node 2 through DTR and RTS (section 9). Node 2
  then forgets the frames it held for node 3 and the versions of its peers, so the first
  request after the reset may wait for a retransmission; in QEMU and on Windows the chip
  keeps running.
- QEMU moves the bytes of every port at the speed of the host and does not model the
  alternate functions of the STM32F4 pins, so the timing above and the pins of USART6 are
  checked only on the boards.
