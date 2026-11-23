# 26 Routed Frame

This example builds the golden ping request twice, as a direct frame of version 1 and as a routed frame of version 2,
and prints every field of both frames side by side. Then it forwards the routed frame like node 2 of a chain, damages
its time to live on the wire and reads all frames back from their bytes.

## What you will learn

- How the routed frame keeps the nine header bytes of the direct frame and adds the source address, the destination
  address and the time to live (TTL) behind `PAYLOAD_LENGTH`.
- Why the overhead grows from 13 to 16 bytes and the payload moves from offset 9 to offset 12.
- How a forwarding node builds a new frame with `TTL - 1` and the message counter of its next link, and why that
  frame gets a new CRC32.
- Why a damaged TTL is a CRC mismatch and not a silent routing error.

## Before you start

1. Watch lecture L110 "The version 2 frame: source, destination and TTL" first. L109 "Version 2: addresses, hops and
   the end-to-end argument" explains why the frame needs addresses, and L45 "Example: a frame field by field" shows
   the direct frame alone in example 02.
2. Install the build tools and Qt 5.12.12 once: `sudo apt install build-essential cmake ninja-build git` and the aqtinstall steps of README.md, section "Building the project", which also has the steps for Windows.
3. Open a terminal and go to the source directory of sercom, the directory that holds `CMakeLists.txt`.
4. If you cloned the repository just now, fetch its submodules once: `git submodule update --init --recursive`
5. Configure the build: `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH=$HOME/Qt/5.12.12/gcc_64`, on Windows with `-DCMAKE_PREFIX_PATH=C:/Qt/5.12.12/mingw73_64`
6. Build every program: `cmake --build build`
7. The program of this example is `build/sercom_example_26_routed_frame`.

To build only this example, run `cmake --build build --target sercom_example_26_routed_frame`.

## Step 1: Build both frames

Open `examples/26_routed_frame.cpp` and find the comment `// create the direct frame of version 1 that requests an
acknowledgement`. This is the golden frame of example 02: a ping request with message counter 7 and the flag
`ACK_REQUEST`.

Find `// create the routed frame of version 2 from the host to node 3`. A `Frame::RouteHeader` holds the three new
fields: the source address `HOST_ADDRESS` (1), the destination address 3 and the time to live
`Settings::DEFAULT_TIME_TO_LIVE` (8). The constructor of `Frame::Frame` that takes a route header writes version `0x02`
and the three bytes. The flags add `ROUTE_CAPABLE` (`0x10`): a node with an address sets it on every frame it sends, so
its peer learns that it reads routed frames.

## Step 2: Run the program

```
build/sercom_example_26_routed_frame
```

You see:

```
direct frame    : A5 C3 01 01 07 00 01 00 04 E8 03 00 00 21 12 D0 63
routed frame    : A5 C3 02 11 07 00 01 00 04 01 03 08 E8 03 00 00 2B AB 4A 47
FIELD                direct                           routed
SYNC                 offset  0 size 2 : A5 C3         offset  0 size 2 : A5 C3
VERSION              offset  2 size 1 : 01            offset  2 size 1 : 02
FLAGS                offset  3 size 1 : 01            offset  3 size 1 : 11
MESSAGE_COUNTER      offset  4 size 2 : 07 00         offset  4 size 2 : 07 00
MESSAGE_TYPE         offset  6 size 2 : 01 00         offset  6 size 2 : 01 00
PAYLOAD_LENGTH       offset  8 size 1 : 04            offset  8 size 1 : 04
SOURCE_ADDRESS       -                                offset  9 size 1 : 01
DESTINATION_ADDRESS  -                                offset 10 size 1 : 03
TTL                  -                                offset 11 size 1 : 08
PAYLOAD              offset  9 size 4 : E8 03 00 00   offset 12 size 4 : E8 03 00 00
CRC32                offset 13 size 4 : 21 12 D0 63   offset 16 size 4 : 2B AB 4A 47
overhead             13 bytes                         16 bytes
forwarded frame : A5 C3 02 11 02 00 01 00 04 01 03 07 E8 03 00 00 EA 4B B9 5D
CRC32 0x474AAB2B of TTL 8 became 0x5DB94BEA of TTL 7
damaged TTL     : A5 C3 02 11 07 00 01 00 04 01 03 09 E8 03 00 00 2B AB 4A 47 -> CRC32 INVALID
read back direct    : version 1, routed no, source 0, destination 0, TTL 0, CRC32 valid
read back routed    : version 2, routed yes, source 1, destination 3, TTL 8, CRC32 valid
read back forwarded : version 2, routed yes, source 1, destination 3, TTL 7, CRC32 valid
```

## Step 3: Understand the fields

- The first six field lines have the same offsets in both frames. A receiver reads `VERSION` at offset 2 and
  `PAYLOAD_LENGTH` at offset 8 before it knows which version it got, so these offsets must never move.
- `SOURCE_ADDRESS`, `DESTINATION_ADDRESS` and `TTL` exist in the routed frame only, the direct frame shows a dash.
- The payload `E8 03 00 00` (1000 ms) moves from offset 9 to offset 12 and the CRC32 from offset 13 to offset 16.
- The overhead line shows `Definitions::FRAME_OVERHEAD` and `Definitions::ROUTED_FRAME_OVERHEAD`: three bytes more per
  frame. The largest frame of this build grows from 268 to 271 bytes, which is why `Settings::MAXIMUM_FRAME_SIZE` now
  sizes the buffers for a routed frame and `Settings::MAXIMUM_DIRECT_FRAME_SIZE` keeps the old limit.
- Both CRC32 values cover everything from `VERSION` to the last payload byte, the routing bytes included.

## Step 4: Forward the frame like node 2

Find `// create the copy that node 2 forwards on its next link with that link's counter`. Node 2 does not pass the
bytes on as they came. It builds a new frame for its next link: the same flags, message type, payload, source and
destination, the time to live minus one and the message counter 2 of its own link, because every link counts its
messages on its own. The `forwarded frame` line shows `02 00` as counter and `07` as TTL, and the CRC32 line shows that
the copy has a CRC32 of its own.

## Step 5: Damage the time to live

Find `// damage the time to live of the routed frame on the wire`. The program raises the TTL byte from `08` to `09`
and builds a frame from the damaged bytes. `getCRCValid()` is false, so the receiver drops the frame like any other
corrupted frame. Without the CRC32 over the routing bytes, a damaged TTL or a damaged address would send the frame to
another node or let it travel too far, and nobody would notice.

## Step 6: Read the frames back

Find `// read every frame back from its bytes`. `Frame::Frame(data, size)` takes the received bytes, the getters read
the version and the route. `getRouted()` is true for the routed frames only. A direct frame gives the neutral values
`UNASSIGNED_ADDRESS` (0) and TTL 0, so code that handles both versions needs no special case. Find `// every intact
frame must be valid and the damaged frame must not`: the program exits with 0 when the three intact frames are valid
and the damaged one is not. The decoder of the byte stream learns to accept version 2 in lecture L111.

## Try it yourself

1. Change `destinationAddress` from 3 to 4. The destination byte and both CRC32 values of the routed frames change, the
   direct frame stays the same.
2. Change `routeHeader.timeToLive` to 1 and run again. The forwarded copy has TTL 0, which a sender must never send: a
   node that would have to forward a frame with TTL 1 drops it with the status `EXPIRED` instead.
3. Build both routed frames with the Python reference implementation and compare the bytes:

   ```
   python3 tools/sercom_frame.py encode --type 0x0001 --counter 7 --flags 0x11 --payload E8030000 --version 2 --source 1 --destination 3
   python3 tools/sercom_frame.py encode --type 0x0001 --counter 2 --flags 0x11 --payload E8030000 --version 2 --source 1 --destination 3 --ttl 7
   ```

   ```
   A5 C3 02 11 07 00 01 00 04 01 03 08 E8 03 00 00 2B AB 4A 47
   A5 C3 02 11 02 00 01 00 04 01 03 07 E8 03 00 00 EA 4B B9 5D
   ```

## Where this is used

- `core/include/Frame.h` and `core/src/Frame.cpp` define `Frame::RouteHeader`, the routed field constructor and the
  getters of both versions.
- `core/include/Definitions.h` holds `ROUTED_PROTOCOL_VERSION`, the sizes of the routing fields, `ROUTE_CAPABLE` and the
  addresses, `core/include/Settings.h` the frame size limits and `DEFAULT_TIME_TO_LIVE`.
- `docs/PROTOCOL.md` specifies the routed frame in section 8 and shows both frames in section 8.11.
- `tools/sercom_frame.py` builds and decodes routed frames in Python.
- Lectures: L109 "Version 2: addresses, hops and the end-to-end argument", L110 "The version 2 frame: source,
  destination and TTL" and L45 "Example: a frame field by field".
