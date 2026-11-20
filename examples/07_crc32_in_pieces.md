# 07 CRC32 in Pieces

This example calculates the CRC32 of a ping request frame in three pieces with `CRC32::updateCRC32`, the way a
receiver can do it while the bytes arrive in chunks. The result is the same as `CRC32::calculateCRC32` over the whole
buffer at once.

## What you will learn

- What the CRC32 register is, and where its initial value and its final XOR come in.
- How `updateCRC32` folds one piece of data after the other into the register.
- That the size of the pieces does not change the result.

## Before you start

1. Watch lecture L36 about the table-driven CRC32 first. It explains the register, the lookup table and the incremental use. Example 06 shows the CRC32 of a whole buffer.
2. Install the build tools once: `sudo apt install build-essential cmake ninja-build git` (README.md, section "Building the project").
3. Open a terminal and go to the source directory of sercom, the directory that holds `CMakeLists.txt`.
4. If you cloned the repository just now, fetch its submodules once: `git submodule update --init --recursive`
5. Configure the build: `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug`
6. Build every program: `cmake --build build`
7. The program of this example is `build/sercom_example_07_crc32_in_pieces`.

To build only this example, run `cmake --build build --target sercom_example_07_crc32_in_pieces`.

## Step 1: Get the protected bytes

Open `examples/07_crc32_in_pieces.cpp` and find the comment `// create the ping request frame of example 02`. The
example builds the same frame as example 02.

Now find `// get the bytes from the version to the last payload byte`. The CRC32 of a frame covers every byte from
the version field to the last payload byte. These are the *protected bytes*. The sync word is not covered, because
the receiver compares it byte by byte anyway. The CRC32 field itself is not covered either, because it holds the
result.

## Step 2: Fold the pieces into the register

Find `// pieces must cover the protected bytes`. The array `pieceSizes` splits the 11 protected bytes into pieces of
3, 5 and 3 bytes, as three calls of `read()` on a serial port might return them. The check makes sure that the
pieces add up to the protected bytes.

Find `// start the running register`. The *register* is the 32 bit number in which the CRC32 is calculated. It starts
at `Definitions::CRC32_INITIAL_VALUE`, that is `0xFFFFFFFF`.

Find `// fold the pieces into the register as they arrive`. For each piece, `CRC32::updateCRC32` takes the register
and the bytes of the piece and returns the new register. The next piece goes on from there.

Find `// turn the register into the CRC32`. After the last piece the register is XORed with
`Definitions::CRC32_FINAL_XOR_VALUE`, also `0xFFFFFFFF`. *XOR* (`^`) flips every bit of the register where the other
value has a 1, so here it flips all 32 bits. The result is the CRC32.

## Step 3: Run the program

```
build/sercom_example_07_crc32_in_pieces
```

You see:

```
protected bytes : 01 01 07 00 01 00 04 E8 03 00 00 (11 bytes)
start register  : 0xFFFFFFFF
piece 1         : 01 01 07 -> register 0x8603E838
piece 2         : 00 01 00 04 E8 -> register 0x9F04D32F
piece 3         : 03 00 00 -> register 0x9C2FEDDE
final XOR       : 0x9C2FEDDE ^ 0xFFFFFFFF = 0x63D01221
whole buffer    : 0x63D01221
CRC32 field     : 0x63D01221
```

- `protected bytes` are the 11 bytes from the version (`01`) to the last payload byte (`00`).
- `start register` is the initial value.
- `piece 1` to `piece 3` show the bytes of each piece and the register after it.
- `final XOR` turns the register after the last piece into the CRC32 `0x63D01221`.
- `whole buffer` is `calculateCRC32` over all 11 bytes in one call: the same number.
- `CRC32 field` is the CRC32 that the frame carries in its last four bytes (`21 12 D0 63`): the same number again.

`calculateCRC32` itself is just this: it starts the register at the initial value, calls `updateCRC32` once with all
bytes and applies the final XOR.

## Step 4: Check the exit code

Find `// CRC32 of the pieces must match the whole buffer` and
`// CRC32 of the pieces must match the CRC32 field of the frame`. The program exits with 0 only when all three numbers
are equal. Print the exit code with `echo $?` and you see `0`.

sercom's own `FrameDecoder` collects the bytes of a frame in its buffer first and then checks the CRC32 in one call,
through `Frame::getCRCValid`. The pieces show what `updateCRC32` makes possible: a receiver can update the CRC32 while
the bytes arrive and does not need to keep them for the check.

## Try it yourself

1. Change `pieceSizes` to `{1, 1, 9}`, build and run again. The registers after piece 1 and piece 2 change, but the
   register after the last piece and the CRC32 stay the same:

   ```
   piece 1         : 01 -> register 0x5AFA20E4
   piece 2         : 01 -> register 0xD03AECD7
   piece 3         : 07 00 01 00 04 E8 03 00 00 -> register 0x9C2FEDDE
   final XOR       : 0x9C2FEDDE ^ 0xFFFFFFFF = 0x63D01221
   ```

2. Change `pieceSizes` to `{4, 4, 4}`. The pieces add up to 12 bytes, one more than the frame protects. The program
   prints the protected bytes, then this line, and exits with 1:

   ```
   [     0.000] WARNING Pieces do not cover the protected bytes
   ```

   The number in brackets is the time in seconds since the first log line.

3. Break it on purpose. Start the register at `0` instead of `Definitions::CRC32_INITIAL_VALUE`. The final XOR now
   gives `0xF7A85C32`, which is not the CRC32 of the frame, and the program ends with:

   ```
   [     0.000] WARNING CRC32 of the pieces does not match the whole buffer
   ```

   Both ends must use the same initial value and the same final XOR, or every frame fails its CRC32 check.

## Where this is used

- `core/include/CRC32.h` and `core/src/CRC32.cpp` define `updateCRC32` with its lookup table, and `calculateCRC32`
  on top of it.
- `core/include/Definitions.h` holds `CRC32_INITIAL_VALUE` and `CRC32_FINAL_XOR_VALUE`.
- `core/src/Frame.cpp` calculates the CRC32 of the protected bytes when it builds a frame and when `getCRCValid`
  checks one.
- `tests/test_CRC32.cpp` checks that a buffer in two parts gives the same CRC32 as the whole buffer.
- Lectures: L36 about the table-driven CRC32, L37 "Verifying a CRC implementation" and L46 "CRC check and copying the
  frame out".
