# 10 Decode Byte by Byte

This example hands one frame to the frame decoder one byte at a time and prints what the
decoder holds after each byte. A serial port can deliver a frame in pieces as small as one
byte, so the decoder must collect bytes until a whole frame is there.

## What you will learn

- How the decoder buffers the bytes of a frame until the frame is complete.
- Which frame field every byte belongs to.
- How a handler gets the complete frame: the decoder calls it from inside `onReceivedByte()`.

## Before you start

1. Watch lecture L46 "Decoder design options" (section S08).
2. Install the build tools and Qt 5.12.12 once: `sudo apt install build-essential cmake ninja-build git` and the aqtinstall steps of README.md, section "Building the project", which also has the steps for Windows.
3. Open a terminal and go to the source directory of sercom, the directory that holds `CMakeLists.txt`.
4. If you cloned the repository just now, fetch its submodules once: `git submodule update --init --recursive`
5. Configure the build: `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH=$HOME/Qt/5.12.12/gcc_64`, on Windows with `-DCMAKE_PREFIX_PATH=C:/Qt/5.12.12/mingw73_64`
6. Build every program: `cmake --build build`
7. The program of this example is `build/sercom_example_10_decode_byte_by_byte`.

To build only this example, run `cmake --build build --target sercom_example_10_decode_byte_by_byte`.

## Step 1: Read the mock handler

Open `examples/10_decode_byte_by_byte.cpp` and read the class `MockDecoderHandler` at the top.

The *decoder* (`FrameDecoder::FrameDecoder`) turns a stream of bytes into frames. It does not
know what to do with a frame, so it calls a *handler*: an object with the two functions of the
interface `FrameDecoderHandler`. `onReceivedFrame()` is called for every complete frame,
`onDecodeError()` for every rejected one. A *mock* is a small stand-in object made for a test or
an example. This mock only prints a line, starting with `handler:`, when it is called.

## Step 2: Read how the bytes are fed

In `main()`, find the comment `// create a frame with a ping request sent at one second`. The
frame carries a ping request with 4 payload bytes.

Find `// create the decoder without the inter byte timeout`. The second argument `0ms` turns the
timeout check off; example 13 shows the timeout.

Find `// feed the frame one byte at a time`. The loop hands every byte to
`frameDecoder.onReceivedByte()`. The second argument is the time of arrival; with the timeout
off it does not matter. After each byte the loop prints the byte, the field that it belongs to
(from `getFieldName()`) and `frameDecoder.getBufferedSize()`, the number of bytes that the
decoder keeps while it waits for the rest of the frame.

Read `getFieldName()` at the end of the file. It walks the fields in the order of the frame and
returns the name of the field that covers the offset.

## Step 3: Run the program

Run `build/sercom_example_10_decode_byte_by_byte`. You get:

```
frame of 17 bytes, fed to the decoder one byte at a time
byte  1: A5  SYNC             buffered  1
byte  2: C3  SYNC             buffered  2
byte  3: 01  VERSION          buffered  3
byte  4: 00  FLAGS            buffered  4
byte  5: 01  MESSAGE_COUNTER  buffered  5
byte  6: 00  MESSAGE_COUNTER  buffered  6
byte  7: 01  MESSAGE_TYPE     buffered  7
byte  8: 00  MESSAGE_TYPE     buffered  8
byte  9: 04  PAYLOAD_LENGTH   buffered  9
byte 10: E8  PAYLOAD          buffered 10
byte 11: 03  PAYLOAD          buffered 11
byte 12: 00  PAYLOAD          buffered 12
byte 13: 00  PAYLOAD          buffered 13
byte 14: 95  CRC32            buffered 14
byte 15: 00  CRC32            buffered 15
byte 16: 08  CRC32            buffered 16
  handler: frame complete, message counter 1, message type 0x0001, payload 4 bytes
byte 17: FF  CRC32            buffered  0
```

Then run `echo $?` to see the exit code. It prints `0`.

## Step 4: Understand the output

- `frame of 17 bytes`: 13 bytes of frame overhead plus 4 payload bytes.
- `byte 1` and `byte 2`: `A5 C3` is the sync word that starts every frame.
- `byte 3` to `byte 9`: the rest of the header. `01 00` after FLAGS is the message counter 1,
  `01 00` is the message type PING_REQUEST, `04` is the payload length. Multi-byte fields send
  the lowest byte first.
- `byte 10` to `byte 13`: the payload `E8 03 00 00`, the number 1000 (the request time).
- `byte 14` to `byte 17`: the CRC32, a check value over everything after the sync word.
- `buffered` grows by one with every byte: the decoder keeps the bytes.
- The `handler:` line comes before the line of byte 17. The decoder calls the handler from
  inside `onReceivedByte()`, before the loop prints the line for byte 17.
- `buffered  0` after byte 17: the frame left the decoder, nothing waits any more.

The exit code `0` means the decoder found exactly one frame and dropped no byte.

## Try it yourself

1. Change the payload. Set `requestTimestamp` to `2 * 1000ms`. The payload bytes become
   `D0 07 00 00`, and the four CRC32 bytes change too, to `07 78 9E CD`. The frame is still
   complete at byte 17.
2. Use a message without a payload. Replace the line
   `const Messages::PingRequest pingRequest(requestTimestamp);` with
   `const Messages::DeviceInfoRequest deviceInfoRequest;`, rename `pingRequest` to
   `deviceInfoRequest` in the two lines below, delete the `requestTimestamp` line and change
   `PING_REQUEST` to `DEVICE_INFO_REQUEST`. The frame has 13 bytes, there is no PAYLOAD line, and
   the handler prints `message type 0x0003, payload 0 bytes` before byte 13.
3. Stop one byte early (this breaks the program on purpose). Change the loop condition to
   `offset + 1 < frame.getSize()`. The last line is `byte 16: 08  CRC32            buffered 16`,
   there is no `handler:` line, the program prints a line ending in
   `WARNING The decoder did not find the frame` and the exit code is `1`.

## Where this is used

- `core/include/FrameDecoder.h` and `core/src/FrameDecoder.cpp`: `onReceivedByte()`,
  `processBuffer()` and the handler interface.
- `core/src/Link.cpp`: the link layer feeds every received read into its decoder.
- `docs/PROTOCOL.md`, section "4. Receiver rules (frame layer)".
- Lectures: L46 "Decoder design options", L49 "CRC check and copying the frame out" and L53
  "Handler interface and decoder statistics".
