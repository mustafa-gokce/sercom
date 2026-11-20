# 11 Resync After Garbage

This example puts garbage bytes, a frame, more garbage and a second frame into one buffer and
lets the decoder find both frames. On a real line you get garbage after a cable is plugged in,
after a device reset or from electrical noise, and the decoder must find the next frame anyway.

## What you will learn

- How the decoder skips bytes that do not start a frame.
- Why the decoder drops only one byte when a sync word does not match.
- How to read the decoder statistics, above all the discarded bytes.

## Before you start

1. Watch lecture L44 "Finding the sync word" (section S08).
2. Install the build tools once: `sudo apt install build-essential cmake ninja-build git` (README.md, section "Building the project").
3. Open a terminal and go to the source directory of sercom, the directory that holds `CMakeLists.txt`.
4. If you cloned the repository just now, fetch its submodules once: `git submodule update --init --recursive`
5. Configure the build: `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug`
6. Build every program: `cmake --build build`
7. The program of this example is `build/sercom_example_11_resync_after_garbage`.

To build only this example, run `cmake --build build --target sercom_example_11_resync_after_garbage`.

## Step 1: Read the garbage and the frames

Open `examples/11_resync_after_garbage.cpp` and find the comment
`// create the garbage and the frames` in `main()`. *Garbage* means bytes that are not part of
any frame. The first garbage holds a lone `A5`, the first byte of the sync word, followed by
`22` instead of `C3`. The second garbage ends in `A5 A5`, right before the `A5 C3` of the second
frame. `createTextFrame()` builds a frame whose payload is a short text.

Find `// put garbage and frames into one buffer`. `addBytes()` appends each part to `stream`
and prints it, so you can see the whole buffer.

## Step 2: Read the decoding

Find `// decode the whole buffer in one call without the inter byte timeout`. The decoder gets
all 45 bytes in one call of `onReceivedData()`. The mock handler at the top of the file prints a
`handler:` line for every frame it gets.

Find `// print the decoder statistics`. `frameDecoder.getStatistics()` returns a
`DecoderStatistics` struct with counters, and `printReportLine()` prints one of them per line.

## Step 3: Run the program

Run `build/sercom_example_11_resync_after_garbage`. You get:

```
garbage : 00 11 A5 22 FF
frame 1 : A5 C3 01 00 01 00 06 00 05 66 69 72 73 74 40 60 43 CD
garbage : 55 A5 A5
frame 2 : A5 C3 01 00 02 00 06 00 06 73 65 63 6F 6E 64 BC DA 99 9D
decoding the buffer:
  handler: frame found, message counter 1, payload "first"
  handler: frame found, message counter 2, payload "second"
decoder statistics:
  incoming bytes                       45
  discarded bytes                       8
  incoming frames                       2
  CRC mismatches                        0
```

Then run `echo $?` to see the exit code. It prints `0`.

## Step 4: Understand the output

- The four lines `garbage` and `frame` show the buffer in order, 5 + 18 + 3 + 19 bytes.
- `decoding the buffer:` and the two `handler:` lines: the decoder found both frames, with their
  texts `"first"` and `"second"`.
- `incoming bytes 45`: every byte reached the decoder.
- `discarded bytes 8`: the 5 + 3 garbage bytes. *Discarded* means dropped because the byte is
  not part of a valid frame. Not one byte of a frame was lost.
- `incoming frames 2`: both frames passed every check.
- `CRC mismatches 0`: no candidate failed the CRC32 check.

How did the decoder get through `A5 A5 A5 C3`? At the first `A5` it sees that the next byte is
not `C3`, so it drops only that one `A5` and looks again. The same happens to the second `A5`.
The third `A5` is followed by `C3`, and that is the real frame. A decoder that dropped two bytes
at a time would have lost the second frame. *Resynchronizing* (resync) is this search for the
next sync word after an error.

The exit code `0` means the decoder found both frames and discarded exactly the garbage.

## Try it yourself

1. Add more garbage. Put three more `0x00` bytes at the front of `firstGarbage`. You see
   `incoming bytes 48` and `discarded bytes 11`, and still both frames.
2. Put a false sync word into the garbage. Change `firstGarbage` to
   `{0x00, 0xA5, 0xC3, 0x22, 0xFF}`. Now the decoder takes `A5 C3` for the start of a frame and
   reads the header behind it. The version byte is `22`, not `01`, so you see the new line
   `handler: decode error INVALID_VERSION`. The decoder drops one byte, scans again and still
   finds both frames, with `discarded bytes 8`.

## Where this is used

- `core/src/FrameDecoder.cpp`: `processBuffer()` (the sync word checks) and `rejectCandidate()`.
- `core/include/FrameDecoder.h`: `DecoderStatistics`.
- `host/apps/sercom_dump.cpp`: decodes captured traffic and prints the same statistics.
- `docs/PROTOCOL.md`, section "4. Receiver rules (frame layer)", rules 1, 2 and 5.
- Lectures: L44 "Finding the sync word", L47 "Resynchronization: drop one byte, rescan the
  rest" and L50 "Handler interface and decoder statistics".
