# 06 CRC32 of a Text

This example calculates the CRC32 of the text `123456789` and compares it with the published check value
`0xCBF43926`. Then it flips one bit of the text at a time and prints how much the CRC32 changes.

## What you will learn

- How to call `CRC32::calculateCRC32` on a few bytes.
- What a check value is and why it proves that a CRC32 implementation is right.
- That a single flipped bit changes about half of the 32 bits of the CRC32.

## Before you start

1. Watch lectures L37 "Verifying a CRC implementation" and L38 "What CRC32 guarantees and what it does not" first.
2. Install the build tools once: `sudo apt install build-essential cmake ninja-build git` (README.md, section "Building the project").
3. Open a terminal and go to the source directory of sercom, the directory that holds `CMakeLists.txt`.
4. If you cloned the repository just now, fetch its submodules once: `git submodule update --init --recursive`
5. Configure the build: `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug`
6. Build every program: `cmake --build build`
7. The program of this example is `build/sercom_example_06_crc32_of_text`.

To build only this example, run `cmake --build build --target sercom_example_06_crc32_of_text`.

## Step 1: Calculate the CRC32 of the check text

Open `examples/06_crc32_of_text.cpp` and find the comment `// calculate the CRC32 of the check text`.

A *CRC32* is a 32 bit number calculated from a block of bytes. The sender puts it behind the bytes. The receiver
calculates it again from the bytes it got. When the two numbers differ, the bytes were damaged on the way.

There are many CRC32 variants. sercom uses CRC-32/ISO-HDLC, the one of Ethernet, zip and Python's
`binascii.crc32`. The catalogue of CRC variants gives each one a *check value*: the CRC32 of the nine ASCII
characters `123456789`. When your code gives the check value for this text, it uses the right variant.

The example copies the characters of the text into a vector of bytes and calls `CRC32::calculateCRC32` with the
first byte and the number of bytes.

## Step 2: Run the program

```
build/sercom_example_06_crc32_of_text
```

You see:

```
"123456789" -> CRC32 0xCBF43926, check value 0xCBF43926
flip one bit of the last character
  bit 0 "123456788" -> CRC32 0xBCF309B0, 15 of 32 bits changed
  bit 1 "12345678;" -> CRC32 0x25FA580A, 15 of 32 bits changed
  bit 2 "12345678=" -> CRC32 0xCC99FD3F, 14 of 32 bits changed
  bit 3 "123456781" -> CRC32 0xC52FB114, 14 of 32 bits changed
  bit 4 "12345678)" -> CRC32 0xD6432942, 14 of 32 bits changed
  bit 5 "12345678\x19" -> CRC32 0xF09A19EE, 14 of 32 bits changed
  bit 6 "12345678y" -> CRC32 0xBD2878B6, 14 of 32 bits changed
  bit 7 "12345678\xB9" -> CRC32 0x264CBA06, 14 of 32 bits changed
```

The first line shows the text in quotes, its CRC32 and the check value. They are the same, so the implementation is
right. Find `// CRC32 must be the check value`: the program stops with the exit code 1 when they differ.

## Step 3: Flip one bit at a time

Find `// flip each bit of the last character in a copy of the text`. The loop runs eight times, once for every bit of
the last character `9`. *Flipping* a bit turns a 0 into a 1 or a 1 into a 0. The code does it with XOR (`^`) and a
mask that has only that bit set (`1 << bit`). This is what noise on a cable does to a byte.

Each line of the second block shows:

- the bit number, from 0 (the lowest bit) to 7 (the highest bit),
- the changed text: `9` is `0x39`, flipping bit 0 gives `0x38`, the character `8`. Bytes that are no printable
  character are shown as `\x` and two hex digits, for example `\x19`.
- the new CRC32 and how many of its 32 bits differ from the CRC32 of the original text.

One flipped bit in the text changes 14 or 15 bits of the CRC32. A small change in the data gives a very different
CRC32, so the receiver cannot miss it.

Find `// count a flip that the CRC32 misses` and `// CRC32 must detect every flipped bit`. A CRC32 detects every
single flipped bit, in any block of bytes, so the counter stays at zero. Print the exit code with `echo $?` and you
see `0`.

## Try it yourself

1. Check a second known text. Change the text to `"The quick brown fox jumps over the lazy dog"` and the check value
   to `0x414FA339U`. The first line becomes:

   ```
   "The quick brown fox jumps over the lazy dog" -> CRC32 0x414FA339, check value 0x414FA339
   ```

   You can compare any text with Python:
   `python3 -c "import binascii; print(hex(binascii.crc32(b'123456789')))"` prints `0xcbf43926`.

2. Flip the bits of the first character instead of the last one: change both `flippedBytes.back()` to
   `flippedBytes.front()`. The texts now start with the changed character, and the first line of the block becomes:

   ```
     bit 0 "023456789" -> CRC32 0xDC8F2D65, 15 of 32 bits changed
   ```

   The heading still says "last character", because it is fixed text.

3. Break it on purpose. Change the check value to `0xCBF43927U`. The program prints the first line, then this line,
   and exits with 1:

   ```
   [     0.000] WARNING CRC32 of the check text is not the check value
   ```

   The number in brackets is the time in seconds since the first log line.

## Where this is used

- `core/include/CRC32.h` and `core/src/CRC32.cpp` define `calculateCRC32`.
- `core/include/Definitions.h` holds the parameters of the variant: `CRC32_POLYNOMIAL`, `CRC32_INITIAL_VALUE` and
  `CRC32_FINAL_XOR_VALUE`.
- Lectures: L37 "Verifying a CRC implementation" and L38 "What CRC32 guarantees and what it does not". L35 "A
  bitwise CRC-32 implementation" shows how the number is calculated.
