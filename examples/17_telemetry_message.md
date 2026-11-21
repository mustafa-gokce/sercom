# 17 Telemetry Message

This example builds a telemetry report response, prints how each field sits in the payload and
turns the received numbers back into seconds, kibibytes and degrees Celsius. The device sends
this message on its own, again and again, so the host can draw how the device is doing.

## What you will learn

- How an uptime, a free heap size and a temperature travel as plain integers.
- Why the temperature is sent in steps of 0.01 degree and how a negative value looks on the wire.
- How the host scales the received integers back to units a person can read.

## Before you start

1. Watch lecture L63 "Message classes on GenericMessage" (section S10). In the code a message class writes its bytes in its field constructor and reads them in its frame constructor (`Messages::TelemetryReportResponse`), there is no `serialize()` function.
2. Install the build tools and Qt 5.12.12 once: `sudo apt install build-essential cmake ninja-build git` and the aqtinstall steps of README.md, section "Building the project", which also has the steps for Windows.
3. Open a terminal and go to the source directory of sercom, the directory that holds `CMakeLists.txt`.
4. If you cloned the repository just now, fetch its submodules once: `git submodule update --init --recursive`
5. Configure the build: `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH=$HOME/Qt/5.12.12/gcc_64`, on Windows with `-DCMAKE_PREFIX_PATH=C:/Qt/5.12.12/mingw73_64`
6. Build every program: `cmake --build build`
7. The program of this example is `build/sercom_example_17_telemetry_message`.

To build only this example, run `cmake --build build --target sercom_example_17_telemetry_message`.

## Step 1: Read how the response is built

Open `examples/17_telemetry_message.cpp` and find the comment
`// create the telemetry report response of the device` in `main()`. The four values are:

- the *sequence* 42, a number that grows by one with every report, so the host sees a lost report,
- the *uptime*, the time since the device started, `3723500ms`, that is 1 h 2 min 3.5 s,
- the *free heap size*, the memory the device has left, `176 * 1024` bytes,
- the *temperature* `-1234`, which means -12.34 degrees Celsius.

The temperature is an `int16_t`, a whole number with a sign. The message has no room for a
fraction, so the device multiplies the temperature by 100 before it sends it. One *step* is
0.01 degree.

## Step 2: Read the field by field print

Find `// print the whole payload` and `// print every field after the fields in front of it`.
The payload has 12 bytes (`Messages::TELEMETRY_REPORT_RESPONSE_SIZE`): 2 for the sequence, 4 for
the uptime, 4 for the free heap size and 2 for the temperature. `sizeof(uint16_t)` is 2 and
`sizeof(uint32_t)` is 4, so `offset` moves by the size of each field. `printField()` prints the
name, the offset, the size and the bytes.

## Step 3: Read the host side

Find `// receive the frame bytes like the host`. The response travels in a frame without flags,
because telemetry is *fire and forget*: the device does not wait for an answer, the next report
replaces a lost one. The host builds a new frame from the bytes and a new
`Messages::TelemetryReportResponse` from that frame.

Find `// scale the received values back to their units`. The uptime becomes seconds with
`std::chrono::duration<double>`, the free heap size is divided by 1024 to get *kibibytes* (KiB,
units of 1024 bytes) and the temperature is divided by 100 to get degrees Celsius.

## Step 4: Run the program

Run `build/sercom_example_17_telemetry_message`. You get:

```
sent: sequence 42, uptime 3723500 ms, free heap 180224 bytes, temperature -1234
payload          : 2A 00 EC D0 38 00 00 C0 02 00 2E FB
SEQUENCE         offset  0 size  2 : 2A 00
UPTIME           offset  2 size  4 : EC D0 38 00
FREE_HEAP_SIZE   offset  6 size  4 : 00 C0 02 00
TEMPERATURE      offset 10 size  2 : 2E FB
received sequence         : 42
received uptime           : 3723500 ms = 3723.500 s
received free heap size   : 180224 bytes = 176.0 KiB
received temperature      : -1234 steps = -12.34 degrees Celsius
every value arrived unchanged
```

Then run `echo $?` to see the exit code. It prints `0`.

## Step 5: Understand the output

- `sent:` the four values as the device has them. One hour, two minutes and 3.5 seconds are
  3723500 ms.
- `payload`: the 12 bytes in the order of the fields.
- `SEQUENCE ... 2A 00`: 42 is `0x002A`, sent lowest byte first (*little-endian*).
- `UPTIME ... EC D0 38 00`: 3723500 is `0x0038D0EC`, again lowest byte first.
- `FREE_HEAP_SIZE ... 00 C0 02 00`: 180224 is `0x0002C000`.
- `TEMPERATURE ... 2E FB`: a negative number is stored in *two's complement*: -1234 becomes
  65536 - 1234 = 64302, which is `0xFB2E`.
- The `received` lines: the host read every field back and scaled it to its unit.
- `every value arrived unchanged`: the last check compared every value with the sent one.

## Try it yourself

1. Make it warm. Set `temperature` to `2537`. The field becomes `E9 09` and the host prints
   `received temperature      : 2537 steps = 25.37 degrees Celsius`.
2. Change the free heap size. Set `freeHeapSize` to `64 * 1024`. The field becomes `00 00 01 00`
   and the host reads `65536 bytes = 64.0 KiB`.
3. Let the device run for 50 days (this breaks the program on purpose). Set `uptime` to
   `50 * 24 * 60 * 60 * 1000ms`. The uptime field has 32 bits and counts milliseconds, so it holds at most
   4294967295 ms, about 49.7 days. The field becomes `00 F8 7D 01`, the host reads
   `25032704 ms = 25032.704 s`, the program prints a line ending in
   `WARNING A received value differs from the sent value` and the exit code is `1`.

## Where this is used

- `app/include/Messages.h` and `app/src/Messages.cpp`: `TelemetryReportResponse` and
  `TELEMETRY_REPORT_RESPONSE_SIZE`.
- `app/src/DeviceApplication.cpp`: the device sends the report once per telemetry period.
- `host/src/CliApplication.cpp`: the `telemetry` command of `sercom_cli` prints the reports.
- `docs/PROTOCOL.md`: the message table with `TELEMETRY_REPORT_RESPONSE`.
- Lectures: L63 "Message classes on GenericMessage" and L66 "Validating payloads"; the byte order
  is L25 "Endianness and byte-wise serialization"; the ESP32 measures the heap and the
  temperature in L85 "Hardware adapters: transport, clock and platform".
