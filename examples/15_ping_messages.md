# 15 Ping Messages

This example builds a ping request and a ping response, prints their payload bytes, reads them
back from frames and calculates the round trip time. A ping is the simplest way to check that a
device is alive and to measure how long one exchange takes.

## What you will learn

- How a message class turns its fields into payload bytes and back.
- Why the ping response returns the request timestamp unchanged.
- How the host calculates the round trip time from that timestamp.

## Before you start

1. Watch lecture L60 "Message structs with serialize and deserialize" (section S10). In the code a message class writes its bytes in its field constructor and reads them in its frame constructor (`Messages::PingRequest`), there is no `serialize()` function.
2. Install the build tools once: `sudo apt install build-essential cmake ninja-build git` (README.md, section "Building the project").
3. Open a terminal and go to the source directory of sercom, the directory that holds `CMakeLists.txt`.
4. If you cloned the repository just now, fetch its submodules once: `git submodule update --init --recursive`
5. Configure the build: `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug`
6. Build every program: `cmake --build build`
7. The program of this example is `build/sercom_example_15_ping_messages`.

To build only this example, run `cmake --build build --target sercom_example_15_ping_messages`.

## Step 1: Read the ping request

Open `examples/15_ping_messages.cpp` and find the comment
`// create the ping request of the host at one second of host time` in `main()`.
`Messages::PingRequest` has one field, the *request timestamp*: the time of the host when it
sends the request, here 1000 ms. The constructor writes the field into the payload, and the
frame carries the payload with the message type PING_REQUEST.

## Step 2: Read the device side

Find `// device must receive a valid ping request`. The device checks the message type of the
frame, then builds `Messages::PingRequest` from the frame and asks `getLikelyValid()` whether the
payload holds every field.

Find `// create the ping response of the device with the request timestamp and its uptime`. The
response copies the request timestamp and adds the *uptime*, the time since the device started,
here 5000 ms. The device does not need to know the time of the host: it only sends the
timestamp back.

## Step 3: Read the host side

Find `// host must receive a valid ping response` and
`// calculate the round trip time when the response arrives at the host`. The *round trip time*
is the time from sending the request to receiving the response. The host subtracts the
returned request timestamp from its current time. In this example the response arrives at the
fixed time 1012 ms, so the output is the same on every run; the real host reads its clock.

## Step 4: Run the program

Run `build/sercom_example_15_ping_messages`. You get:

```
host sends a ping request at 1000 ms
  payload           : E8 03 00 00
  request timestamp : E8 03 00 00 = 1000 ms
  frame             : A5 C3 01 00 01 00 01 00 04 E8 03 00 00 95 00 08 FF
device answers with its uptime of 5000 ms
  payload           : E8 03 00 00 88 13 00 00
  request timestamp : E8 03 00 00 = 1000 ms
  uptime            : 88 13 00 00 = 5000 ms
  frame             : A5 C3 01 00 01 00 02 00 08 E8 03 00 00 88 13 00 00 E7 BD 5A 07
host receives the ping response at 1012 ms
  request timestamp : 1000 ms
  uptime            : 5000 ms
  round trip time   : 1012 ms - 1000 ms = 12 ms
```

Then run `echo $?` to see the exit code. It prints `0`.

## Step 5: Understand the output

- `payload : E8 03 00 00`: the 4 bytes of the request timestamp. 1000 is `0x000003E8`, and the
  protocol sends the lowest byte first, `E8`. This byte order is called *little endian*.
- `frame`: the 13 bytes of frame overhead around the payload. `01 00` after the counter is the
  message type PING_REQUEST, `04` is the payload length.
- `device answers ...` and `payload : E8 03 00 00 88 13 00 00`: the request timestamp unchanged,
  then the uptime 5000, which is `0x00001388`.
- `uptime : 88 13 00 00 = 5000 ms`: the second field starts 4 bytes into the payload.
- In the response frame, `02 00` is the message type PING_RESPONSE and `08` the payload length.
- `host receives ...`: the host read both fields back from the frame.
- `round trip time : 1012 ms - 1000 ms = 12 ms`: the response came 12 ms after the request.

The exit code `0` means both sides got a valid message of the right type.

## Try it yourself

1. Make the line slower. Set `responseTimestamp` to `1250ms`. The last line becomes
   `round trip time   : 1250 ms - 1000 ms = 250 ms`.
2. Send the request later. Set `requestTimestamp` to `70 * 1000ms` and `responseTimestamp` to
   `70012ms`. The timestamp bytes become `70 11 01 00` (70000 is `0x00011170`), the CRC32 bytes
   of both frames change, and the round trip time stays `12 ms`.
3. Send the wrong message type (this breaks the program on purpose). In the response frame,
   replace `Messages::MessageType::PING_RESPONSE` with `Messages::MessageType::PING_REQUEST`.
   The response frame shows `01 00` as its type, the host check fails, the program prints a line
   ending in `WARNING The host did not receive a valid ping response` and the exit code is `1`.

## Where this is used

- `app/include/Messages.h` and `app/src/Messages.cpp`: `PingRequest` and `PingResponse`.
- `docs/PROTOCOL.md`: the message table with `PING_REQUEST` and `PING_RESPONSE`.
- Lectures: L59 "Message types and enum converters", L60 "Message structs with serialize and
  deserialize" and L63 "Validating payloads"; the host side is L75 "DeviceClient: requests and
  replies".
