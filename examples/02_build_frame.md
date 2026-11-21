# 02 Build a Frame

This example builds the frame of a ping request from its fields and prints every field with its offset and its size.
Then it checks the CRC32 of the frame the way a receiver does.

## What you will learn

- How a message gives the payload and how `Frame::Frame` builds the frame around it.
- Where each field sits in the frame: its offset and its size.
- How a receiver checks the CRC32 of a frame.

## Before you start

1. Watch lectures L27 "Anatomy of a frame" and L42 "Example: a frame field by field" first. L41 "Golden vectors and encoder tests" uses the same frame.
2. Install the build tools and Qt 5.12.12 once: `sudo apt install build-essential cmake ninja-build git` and the aqtinstall steps of README.md, section "Building the project", which also has the steps for Windows.
3. Open a terminal and go to the source directory of sercom, the directory that holds `CMakeLists.txt`.
4. If you cloned the repository just now, fetch its submodules once: `git submodule update --init --recursive`
5. Configure the build: `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH=$HOME/Qt/5.12.12/gcc_64`, on Windows with `-DCMAKE_PREFIX_PATH=C:/Qt/5.12.12/mingw73_64`
6. Build every program: `cmake --build build`
7. The program of this example is `build/sercom_example_02_build_frame`.

To build only this example, run `cmake --build build --target sercom_example_02_build_frame`.

## Step 1: Create the message

Open `examples/02_build_frame.cpp` and find the comment `// create a ping request sent at one second`.

A *message* is the data that the application wants to send. `Messages::PingRequest` holds one field, the request
timestamp, here one second. The message keeps its bytes: `getPayload()` returns the first byte and
`getPayloadLength()` the number of bytes, here 4. The *payload* is the part of a frame that carries the message.

## Step 2: Create the frame

Find `// create a frame that requests an acknowledgement`. The constructor of `Frame::Frame` takes five values:

- the flags: `ACK_REQUEST` asks the receiver for an acknowledgement, and `fromFrameFlagType` turns it into the
  number `0x01`,
- the message counter 7, the number of this message,
- the message type of a ping request, `0x0001` from `fromMessageType`,
- the payload and the payload length of the message.

The constructor writes the sync word, the version, these fields, the payload and the CRC32. Find
`// frame must be valid`: a payload above the limit would leave the frame empty, and `getLikelyValid()` would return
false.

## Step 3: Run the program

```
build/sercom_example_02_build_frame
```

You see:

```
whole frame      : A5 C3 01 01 07 00 01 00 04 E8 03 00 00 21 12 D0 63
SYNC             offset  0 size  2 : A5 C3
VERSION          offset  2 size  1 : 01
FLAGS            offset  3 size  1 : 01
MESSAGE_COUNTER  offset  4 size  2 : 07 00
MESSAGE_TYPE     offset  6 size  2 : 01 00
PAYLOAD_LENGTH   offset  8 size  1 : 04
PAYLOAD          offset  9 size  4 : E8 03 00 00
CRC32            offset 13 size  4 : 21 12 D0 63
CRC32 calculated 0x63D01221 received 0x63D01221 -> valid
```

- `whole frame` shows all 17 bytes.
- Each field line shows the name of the field, its *offset* (the number of bytes in front of it), its size in bytes
  and its bytes. The offset of each field is the offset of the field before it plus that field's size.
- `SYNC` is the sync word `A5 C3`, the fixed start of every frame.
- `MESSAGE_COUNTER` `07 00` and `MESSAGE_TYPE` `01 00` are stored least significant byte first (see example 04).
- `PAYLOAD` `E8 03 00 00` is the timestamp 1000 ms.
- `CRC32` `21 12 D0 63` is the CRC32 `0x63D01221`, least significant byte first.

## Step 4: Check the CRC32 like a receiver

Find `// calculate the CRC32 like a receiver`. The CRC32 covers the bytes from the version (offset 2) to the last
payload byte (offset 12). The program calculates it with `CRC32::calculateCRC32` and compares it with the CRC32 field
from `frame.getCRC32()`.

The last line shows both numbers and `valid`, because they are equal. Find `// CRC32 must be valid`: the program
exits with 1 when they differ. Print the exit code with `echo $?` and you see `0`.

## Try it yourself

1. Change `messageCounter` from 7 to 8, build and run again. Only two fields and the CRC32 change:

   ```
   whole frame      : A5 C3 01 01 08 00 01 00 04 E8 03 00 00 F0 DD 69 BC
   MESSAGE_COUNTER  offset  4 size  2 : 08 00
   CRC32            offset 13 size  4 : F0 DD 69 BC
   CRC32 calculated 0xBC69DDF0 received 0xBC69DDF0 -> valid
   ```

   A change in any covered byte gives another CRC32.

2. Change `requestTimestamp` from `1 * 1000ms` to `2 * 1000ms`. The payload becomes `D0 07 00 00` (2000 is `0x07D0`)
   and the CRC32 becomes `B3 6A 46 51`.

3. Build the same frame with the Python reference implementation and compare the bytes with the `whole frame` line:

   ```
   python3 tools/sercom_frame.py encode --type 0x0001 --counter 7 --flags 0x01 --payload E8030000
   ```

   ```
   A5 C3 01 01 07 00 01 00 04 E8 03 00 00 21 12 D0 63
   ```

   Two separate implementations give the same bytes, so both follow the same specification.

## Where this is used

- `core/include/Frame.h` and `core/src/Frame.cpp` define the field constructor that writes the frame.
- `app/include/Messages.h` and `app/src/Messages.cpp` define `Messages::PingRequest` and the message types.
- `core/include/Definitions.h` holds the sync bytes, the field sizes and the frame flags.
- `core/src/CRC32.cpp` calculates the CRC32.
- `docs/PROTOCOL.md` describes the same frame in section 2.2, and `tools/sercom_frame.py` builds it in Python.
- Lectures: L27 "Anatomy of a frame", L41 "Golden vectors and encoder tests" and L42 "Example: a frame field by
  field".
