# 08 Read a Frame

This example takes the 17 bytes of a known frame, a ping request, builds a `Frame::Frame` from them and prints every
field through its getter. Then it damages the bytes in two ways and shows which check finds which damage:
`getLikelyValid` or `getCRCValid`.

## What you will learn

- How the bytes of a frame map to its fields: version, flags, message counter, message type, payload length,
  payload and CRC32.
- How to build a frame from received bytes and read each field with its getter.
- The difference between the quick structure check `getLikelyValid` and the full check `getCRCValid`.

## Before you start

1. Watch lectures L27 "Anatomy of a frame" and L29 "Version, flags, counter and type" first. Example 02 builds the same frame from its fields; this example goes the other way.
2. Install the build tools once: `sudo apt install build-essential cmake ninja-build git` (README.md, section "Building the project").
3. Open a terminal and go to the source directory of sercom, the directory that holds `CMakeLists.txt`.
4. If you cloned the repository just now, fetch its submodules once: `git submodule update --init --recursive`
5. Configure the build: `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug`
6. Build every program: `cmake --build build`
7. The program of this example is `build/sercom_example_08_read_a_frame`.

To build only this example, run `cmake --build build --target sercom_example_08_read_a_frame`.

## Step 1: Get the bytes of the frame

Open `examples/08_read_a_frame.cpp` and find the comment
`// get the header bytes of a ping request frame as they arrive from the wire`.

These are the bytes of the example frame in `docs/PROTOCOL.md`, section 2.2. The *header* is the first 9 bytes:
the sync word `A5 C3`, the version `01`, the flags `01`, the message counter `07 00`, the message type `01 00` and
the payload length `04`.

Find `// add the payload bytes and the CRC32 bytes behind the header`. The *payload* `E8 03 00 00` is the data of the
message, here a request timestamp of 1000 ms. The last four bytes `21 12 D0 63` are the CRC32.

## Step 2: Build the frame and read the fields

Find `// build a frame from the bytes and print it`. The constructor `Frame::Frame(data, size)` copies the bytes into
the frame. It only refuses more bytes than the largest frame has; the real checks come later.

The function `printFields` at the end of the file calls one getter per field: `getVersion`, `getFlags`,
`getMessageCounter`, `getMessageType`, `getPayloadLength`, `getPayload` and `getCRC32`. A *getter* is a function
that returns one value of an object. The getters read the fields from the bytes. A field of two or four bytes is
read least significant byte first.

## Step 3: Run the program

```
build/sercom_example_08_read_a_frame
```

You see:

```
frame as received
  getVersion()        : 1
  getFlags()          : 0x01 ACK_REQUEST
  getMessageCounter() : 7
  getMessageType()    : 0x0001 PING_REQUEST
  getPayloadLength()  : 4
  getPayload()        : E8 03 00 00
  getCRC32()          : 0x63D01221
  bytes               : A5 C3 01 01 07 00 01 00 04 E8 03 00 00 21 12 D0 63
  getLikelyValid()    : yes
  getCRCValid()       : yes
frame with one payload bit flipped
  bytes               : A5 C3 01 01 07 00 01 00 04 E9 03 00 00 21 12 D0 63
  getLikelyValid()    : yes
  getCRCValid()       : no
frame with the last byte lost
  bytes               : A5 C3 01 01 07 00 01 00 04 E8 03 00 00 21 12 D0
  getLikelyValid()    : no
  getCRCValid()       : no
```

The first block is the frame as received:

- `getVersion()` is 1, the version of the frame format.
- `getFlags()` is `0x01`. Bit 0 is the flag `ACK_REQUEST`: the sender asks for an acknowledgement.
- `getMessageCounter()` is 7. The bytes `07 00` are read as `0x0007`.
- `getMessageType()` is `0x0001`, the type `PING_REQUEST`.
- `getPayloadLength()` is 4, so four payload bytes follow the header.
- `getPayload()` shows the four payload bytes.
- `getCRC32()` is `0x63D01221`, read from the bytes `21 12 D0 63`.
- `bytes` shows all 17 bytes of the frame.
- `getLikelyValid()` is yes and `getCRCValid()` is yes: the frame is good.

## Step 4: Find the damage

`getLikelyValid` is a quick check of the structure. It checks that the frame starts with the sync word, that the
version is 1, that the payload length is allowed, and that the frame has exactly 13 bytes plus the payload length.
It does not look at the payload.

`getCRCValid` runs `getLikelyValid` first. Then it calculates the CRC32 of the bytes from the version to the last
payload byte and compares it with the CRC32 field.

Find `// flip the lowest bit of the first payload byte`. The second block shows the frame with `E8` changed to `E9`.
The structure is still right, so `getLikelyValid()` is yes. The CRC32 no longer matches, so `getCRCValid()` is no.

Find `// lose the last byte`. The third block shows the frame with 16 bytes. The payload length says 4, so the frame
should have 17 bytes. `getLikelyValid()` is no, and so is `getCRCValid()`.

Find `// checks must pass the frame as received`, `// only the CRC32 check must find the flipped bit` and
`// both checks must find the lost byte`. The program exits with 0 only when all three frames give these answers.
Print the exit code with `echo $?` and you see `0`.

## Try it yourself

1. Break it on purpose. Change the message counter bytes `0x07, 0x00` in the header line to `0x08, 0x00`. The first
   block now shows `getMessageCounter() : 8`, `getLikelyValid() : yes` and `getCRCValid() : no`, and the program
   ends with this line and the exit code 1:

   ```
   [     0.000] WARNING The checks did not pass the frame as received
   ```

   The CRC32 covers the header too, not only the payload. The number in brackets is the time in seconds since the
   first log line.

2. Repair the frame of the first change with the Python reference implementation. It prints the bytes of the frame with the
   counter 8:

   ```
   python3 tools/sercom_frame.py encode --type 0x0001 --counter 8 --flags 0x01 --payload E8030000
   ```

   ```
   A5 C3 01 01 08 00 01 00 04 E8 03 00 00 F0 DD 69 BC
   ```

   Put the new CRC32 bytes `0xF0, 0xDD, 0x69, 0xBC` into `crcBytes`. Now `getCRC32()` shows `0xBC69DDF0`, both
   checks say yes, and the program exits with 0 again.

## Where this is used

- `core/include/Frame.h` and `core/src/Frame.cpp` define the frame, its getters, `getLikelyValid` and `getCRCValid`.
- `core/src/FrameDecoder.cpp` builds a `Frame::Frame` from the received bytes and checks it with `getCRCValid`
  before it hands the frame on.
- `docs/PROTOCOL.md` describes the frame format and the same example frame.
- Lectures: L27 "Anatomy of a frame", L29 "Version, flags, counter and type" and L46 "CRC check and copying the
  frame out".
