# 14 Two Frames in One Read

This example hands the decoder two frames in one read, and then one frame split over two reads.
A serial port read returns whatever bytes have arrived so far. That can be two frames at once,
or a frame cut in the middle, and the decoder must handle both.

## What you will learn

- That read boundaries and frame boundaries are independent.
- How the decoder finds several frames in one call of `onReceivedData()`.
- How the decoder keeps the start of a frame until the next read brings the rest.

## Before you start

1. Watch lecture L52 "Buffer management and reentrancy" (section S08).
2. Install the build tools and Qt 5.12.12 once: `sudo apt install build-essential cmake ninja-build git` and the aqtinstall steps of README.md, section "Building the project", which also has the steps for Windows.
3. Open a terminal and go to the source directory of sercom, the directory that holds `CMakeLists.txt`.
4. If you cloned the repository just now, fetch its submodules once: `git submodule update --init --recursive`
5. Configure the build: `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH=$HOME/Qt/5.12.12/gcc_64`, on Windows with `-DCMAKE_PREFIX_PATH=C:/Qt/5.12.12/mingw73_64`
6. Build every program: `cmake --build build`
7. The program of this example is `build/sercom_example_14_two_frames_one_read`.

To build only this example, run `cmake --build build --target sercom_example_14_two_frames_one_read`.

## Step 1: Read the helper functions

Open `examples/14_two_frames_one_read.cpp`. `createPingFrame()` builds a frame with a ping
request and the message counter you give it. `receiveRead()` plays the part of one serial port
*read*: one call that hands a block of bytes to the decoder. It prints the size of the read,
calls `frameDecoder.onReceivedData()` and prints how many bytes the decoder holds afterwards.
The mock handler at the top of the file prints a `handler:` line for every frame.

## Step 2: Read the two cases

In `main()`, find `// receive the first two frames back to back in one read`. `firstRead` holds
frame 1 and frame 2 one after the other, *back to back*, with no gap between them.

Find `// receive the third frame split over two reads`. `secondRead` holds the first 5 bytes of
frame 3, `splitOffset`, and `thirdRead` holds the other 12 bytes. The cut lies inside the message
counter field, so even a two-byte field arrives in two pieces.

## Step 3: Run the program

Run `build/sercom_example_14_two_frames_one_read`. You get:

```
two frames in one read:
read 1: 34 bytes
  handler: frame received, message counter 1
  handler: frame received, message counter 2
  decoder holds 0 bytes after read 1
one frame in two reads:
read 2: 5 bytes
  decoder holds 5 bytes after read 2
read 3: 12 bytes
  handler: frame received, message counter 3
  decoder holds 0 bytes after read 3
```

Then run `echo $?` to see the exit code. It prints `0`.

## Step 4: Understand the output

- `read 1: 34 bytes`: two frames of 17 bytes.
- The two `handler:` lines during read 1: one call of `onReceivedData()` gave both frames. The
  decoder does not stop after the first frame, it goes on until the buffer is used up.
- `decoder holds 0 bytes after read 1`: no byte of read 1 was left over.
- `read 2: 5 bytes` and `decoder holds 5 bytes after read 2`: no frame yet. The decoder keeps
  the 5 bytes in its buffer, the memory where it collects the bytes of a frame.
- `read 3: 12 bytes` and the `handler:` line: the rest arrived, frame 3 is complete.
- `decoder holds 0 bytes after read 3`: the buffer is empty again.

The exit code `0` means the decoder found all three frames and discarded no byte.

## Try it yourself

1. Cut after the first byte. Set `splitOffset` to `1`. You see `read 2: 1 bytes`,
   `decoder holds 1 bytes after read 2` and `read 3: 16 bytes`, and frame 3 still arrives.
2. Put all three frames into read 1. Add `thirdFrame` to `firstRead` the same way as
   `secondFrame`, and delete the two `receiveRead()` lines of read 2 and read 3. You see three
   `handler:` lines during read 1.
3. Leave out the last read (this breaks the program on purpose). Delete the line
   `receiveRead(frameDecoder, "read 3", thirdRead);`. Frame 3 never becomes complete, the
   decoder still holds 5 bytes, the program prints a line ending in
   `WARNING The decoder did not find all three frames` and the exit code is `1`.

## Where this is used

- `core/src/FrameDecoder.cpp`: `onReceivedData()`, `appendByte()` and the loop in
  `processBuffer()`.
- `host/src/CliApplication.cpp` and `host/src/DeviceSimulator.cpp`: hand every serial port read,
  whatever its size, to the link layer, which passes it on to its decoder.
- Lectures: L52 "Buffer management and reentrancy" and L46 "Decoder design options"; the read
  loop of the host is L77 "The Qt event loop: one thread, signals and a timer".
