# 04 Byte Order

This example stores two numbers of a ping request as bytes, the way sercom puts them on the wire, and loads them back.
It also reads the same bytes in the wrong order, so you can see why the sender and the receiver must agree on the
byte order.

## What you will learn

- How `ByteCodec::storeUint16` and `ByteCodec::storeUint32` split a number into bytes, least significant byte first.
- How `ByteCodec::loadUint16` and `ByteCodec::loadUint32` join the bytes back into the number.
- What a receiver gets when it reads the bytes in the other order.

## Before you start

1. Watch lecture L25 "Endianness and byte-wise serialization" first.
2. Install the build tools and Qt 5.12.12 once: `sudo apt install build-essential cmake ninja-build git` and the aqtinstall steps of README.md, section "Building the project", which also has the steps for Windows.
3. Open a terminal and go to the source directory of sercom, the directory that holds `CMakeLists.txt`.
4. If you cloned the repository just now, fetch its submodules once: `git submodule update --init --recursive`
5. Configure the build: `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH=$HOME/Qt/5.12.12/gcc_64`, on Windows with `-DCMAKE_PREFIX_PATH=C:/Qt/5.12.12/mingw73_64`
6. Build every program: `cmake --build build`
7. The program of this example is `build/sercom_example_04_byte_order`.

To build only this example, run `cmake --build build --target sercom_example_04_byte_order`.

## Step 1: Store the message type

Open `examples/04_byte_order.cpp` and find the comment `// store the message type of a ping request`.

A byte holds a number from 0 to 255. A 16 bit number (`uint16_t`) needs two bytes. The *least significant byte* is
the low part of the number, the *most significant byte* is the high part. In hex the number `0x1234` has the most
significant byte `12` and the least significant byte `34`. *Hex* (hexadecimal) writes numbers in base 16; the prefix
`0x` marks a hex number, and every byte is exactly two hex digits.

`ByteCodec::storeUint16` writes the least significant byte first. This order is called *little-endian*. Every field of
a sercom frame that is longer than one byte uses it.

The message type of a ping request is `0x0001`. The example gets it with `Messages::fromMessageType` and stores it
into a vector of two bytes.

## Step 2: Load it back in both orders

Find the comment `// load the message type in both byte orders`.

`ByteCodec::loadUint16` reads the two bytes in the same order and gives back the number.

`loadBigEndianValue` is a small function at the end of the file. It reads the bytes the other way round, most
significant byte first. This order is called *big-endian*. sercom never sends it. The example uses it only to show
what a receiver with the wrong byte order would read.

## Step 3: Run the program

```
build/sercom_example_04_byte_order
```

You see:

```
message type of a ping request
  value                     : 0x0001 PING_REQUEST
  stored as bytes           : 01 00
  loaded in the same order  : 0x0001 PING_REQUEST
  loaded in the wrong order : 0x0100 SET_LED_REQUEST
request timestamp of a ping request
  value                     : 0x000003E8 = 1000 ms
  stored as bytes           : E8 03 00 00
  loaded in the same order  : 0x000003E8 = 1000 ms
  loaded in the wrong order : 0xE8030000 = 3892510720 ms
```

The first block is the message type:

- `value` is the number before it was stored: `0x0001`, the type of a ping request.
- `stored as bytes` shows the two bytes in wire order: the low byte `01` first, then `00`.
- `loaded in the same order` is the number after `loadUint16`: the same value again.
- `loaded in the wrong order` is `0x0100`. That is a real message type too: `SET_LED_REQUEST`. A receiver with the
  wrong byte order would switch an LED instead of answering the ping, and nothing would warn it.

The second block is the request timestamp of the ping request. It is a 32 bit number (`uint32_t`) of milliseconds:

- 1000 ms is `0x000003E8`. It is stored as `E8 03 00 00`, the same four bytes you see as the payload in example 02.
- In the wrong order the receiver reads `0xE8030000`, that is 3892510720 ms: about 45 days instead of one second.

## Step 4: Check the exit code

Find the comments `// values must come back in the same order` and `// values must change in the wrong order`. The
program exits with 0 only when both checks pass. Print the exit code of the last program:

```
echo $?
```

You see `0`.

## Try it yourself

1. Change the timestamp `1 * 1000ms` to `5 * 1000ms`, build and run again. The second block now ends with:

   ```
     value                     : 0x00001388 = 5000 ms
     stored as bytes           : 88 13 00 00
     loaded in the same order  : 0x00001388 = 5000 ms
     loaded in the wrong order : 0x88130000 = 2282946560 ms
   ```

2. Change `Messages::MessageType::PING_REQUEST` to `Messages::MessageType::ECHO_REQUEST`. The bytes become `06 00`
   and the wrong order gives `0x0600 UNKNOWN`: no message type has this number. The heading still says "ping
   request", because it is fixed text.

3. Break it on purpose. Make the "same order" load use the wrong order: replace
   `ByteCodec::loadUint16(messageTypeAsBytes.data())` with
   `static_cast<uint16_t>(loadBigEndianValue(messageTypeAsBytes.data(), messageTypeAsBytes.size()))`. Both load lines
   now show `0x0100 SET_LED_REQUEST`, and the program ends with this line and the exit code 1:

   ```
   [     0.000] WARNING Values changed although they were loaded in the same order
   ```

   The number in brackets is the time in seconds since the first log line. Undo the change afterwards.

## Where this is used

- `core/include/ByteCodec.h` and `core/src/ByteCodec.cpp` define `storeUint16`, `storeUint32`, `loadUint16` and
  `loadUint32`.
- `core/src/Frame.cpp` loads the message counter, the message type and the CRC32 of a frame with `loadUint16` and
  `loadUint32`.
- `app/src/Messages.cpp` loads the request timestamp of a ping request with `loadUint32`.
- Lectures: L25 "Endianness and byte-wise serialization", L24 "Bytes and fixed-width integers" and L30 "Anatomy of a
  frame".
