# 12 CRC Error Detected

This example flips one bit in the payload of a frame and lets the decoder check it. The decoder
reports a CRC mismatch, drops the damaged frame and still accepts the good frame behind it.
Without the CRC check, the damaged frame would switch the LED off instead of on.

## What you will learn

- How one flipped bit changes the CRC32 of a frame completely.
- How the decoder reports a damaged frame with `onDecodeError()` and `CRC_MISMATCH`.
- That the decoder recovers by itself and decodes the next good frame.

## Before you start

1. Watch lecture L49 "CRC check and copying the frame out" (section S08).
2. Install the build tools and Qt 5.12.12 once: `sudo apt install build-essential cmake ninja-build git` and the aqtinstall steps of README.md, section "Building the project", which also has the steps for Windows.
3. Open a terminal and go to the source directory of sercom, the directory that holds `CMakeLists.txt`.
4. If you cloned the repository just now, fetch its submodules once: `git submodule update --init --recursive`
5. Configure the build: `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH=$HOME/Qt/5.12.12/gcc_64`, on Windows with `-DCMAKE_PREFIX_PATH=C:/Qt/5.12.12/mingw73_64`
6. Build every program: `cmake --build build`
7. The program of this example is `build/sercom_example_12_crc_error_detected`.

To build only this example, run `cmake --build build --target sercom_example_12_crc_error_detected`.

## Step 1: Read how the frame is damaged

Open `examples/12_crc_error_detected.cpp` and find the comment
`// create two frames that switch the LED on` in `main()`. Both frames carry a set LED request
with one payload byte, `01` for "on". `createLedOnFrame()` builds them.

Find `// flip the lowest bit of the first payload byte of the first frame`. The program copies
the bytes of the first frame into `damagedBytes`. The payload starts right after the header, at
offset `Definitions::HEADER_SIZE` (9). The operator `^=` (XOR) flips the bits that are set in
`flippedBit`, so `01` becomes `00`.

## Step 2: Read the CRC32 calculation

Find `// calculate the CRC32 of the damaged bytes like the decoder`. *CRC32* is a 32 bit check
value. The sender calculates it over every byte after the sync word and puts it at the end of
the frame. The receiver calculates it again over the bytes it got. Both values must match.
`damagedFrame.getCRC32()` reads the value that the sender wrote, and
`CRC32::calculateCRC32()` calculates the value of the damaged bytes.

## Step 3: Read the decoding

Find `// decode the damaged frame followed by the second frame`. The good second frame is added
behind the damaged bytes and the decoder gets everything in one call. The mock handler at the
top of the file prints a `handler:` line for every frame and every decode error.

## Step 4: Run the program

Run `build/sercom_example_12_crc_error_detected`. You get:

```
sent     : A5 C3 01 00 01 00 00 01 01 01 B2 67 0A 0D
received : A5 C3 01 00 01 00 00 01 01 00 B2 67 0A 0D
CRC32 in the frame 0x0D0A67B2, CRC32 of the received bytes 0x7A0D5724
decoding the damaged frame and the second frame:
  handler: decode error CRC_MISMATCH
  handler: frame received, message counter 2, LED on
CRC mismatches 1, incoming frames 1, discarded bytes 14
```

Then run `echo $?` to see the exit code. It prints `0`.

## Step 5: Understand the output

- `sent` and `received`: the two frames differ in one byte only, the payload `01` became `00`.
- `CRC32 in the frame 0x0D0A67B2`: the value of the sender. On the wire it stands as
  `B2 67 0A 0D`, lowest byte first.
- `CRC32 of the received bytes 0x7A0D5724`: one flipped bit changed the whole value.
- `handler: decode error CRC_MISMATCH`: the decoder saw that the two values differ and rejected
  the frame. The LED request inside was never delivered.
- `handler: frame received, message counter 2, LED on`: the good frame behind it arrived intact.
- `CRC mismatches 1, incoming frames 1`: one frame rejected, one accepted.
- `discarded bytes 14`: all 14 bytes of the damaged frame were dropped. After a mismatch the
  decoder drops one byte and scans the rest again, because a real frame could start inside the
  rejected one.

The exit code `0` means the decoder rejected the damaged frame and accepted the good one.

## Try it yourself

1. Flip another bit. Set `flippedBit` to `0x80`. The received payload byte becomes `81` and the
   calculated CRC32 becomes `0xE0B2E492`. The decoder still reports `CRC_MISMATCH`.
2. Damage the CRC32 itself. Replace `damagedBytes[Definitions::HEADER_SIZE] ^= flippedBit;` with
   `damagedBytes[damagedBytes.size() - 1] ^= flippedBit;`. Now the payload is right but the
   stored value is wrong: `CRC32 in the frame 0x0C0A67B2, CRC32 of the received bytes 0x0D0A67B2`.
   This is a CRC mismatch too.
3. Damage the sync word (this breaks the program on purpose). Use `damagedBytes[1]` as the byte
   to flip. The frame starts with `A5 C2` now, so the decoder never takes it for a frame: there is
   no `CRC_MISMATCH` line, only `CRC mismatches 0, incoming frames 1, discarded bytes 14`. The
   program prints a line ending in `WARNING The decoder did not reject the damaged frame`
   and the exit code is `1`. Not every damage is a CRC error.

## Where this is used

- `core/src/CRC32.cpp`: `calculateCRC32()`.
- `core/src/Frame.cpp`: `Frame::getCRCValid()`, the check that the decoder calls.
- `core/src/FrameDecoder.cpp`: `processBuffer()` counts `crcMismatchCounter` and calls
  `rejectCandidate()`.
- `tools/NOISE.md`: the test checklist for damaged frames.
- Lectures: L49 "CRC check and copying the frame out" and L50 "Resynchronization: drop one
  byte, rescan the rest"; what the CRC32 can find is L41 "What CRC32 guarantees and what it
  does not".
