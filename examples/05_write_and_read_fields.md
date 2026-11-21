# 05 Write and Read Fields

This example writes the four fields of a small status record into a buffer with `ByteCodec::ByteWriter` and reads
them back with `ByteCodec::ByteReader`. Then it reads a record that was cut short and shows how the reader becomes
invalid when it reads past the end.

## What you will learn

- How a `ByteWriter` writes fields of different types one after the other.
- How a `ByteReader` reads them back in the same order.
- Why one check at the end is enough: after the first failed read, every later read fails too.

## Before you start

1. Watch lectures L27 "ByteWriter and the sticky error pattern" and L28 "ByteReader: parsing untrusted bytes" first. L29 "Signed values, floats and strings" explains how negative numbers and floats look as bytes.
2. Install the build tools and Qt 5.12.12 once: `sudo apt install build-essential cmake ninja-build git` and the aqtinstall steps of README.md, section "Building the project", which also has the steps for Windows.
3. Open a terminal and go to the source directory of sercom, the directory that holds `CMakeLists.txt`.
4. If you cloned the repository just now, fetch its submodules once: `git submodule update --init --recursive`
5. Configure the build: `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH=$HOME/Qt/5.12.12/gcc_64`, on Windows with `-DCMAKE_PREFIX_PATH=C:/Qt/5.12.12/mingw73_64`
6. Build every program: `cmake --build build`
7. The program of this example is `build/sercom_example_05_write_and_read_fields`.

To build only this example, run `cmake --build build --target sercom_example_05_write_and_read_fields`.

## Step 1: Write the fields

Open `examples/05_write_and_read_fields.cpp` and find the comment `// write the fields of a small status record`.
The record has four *fields*, values with a fixed type and a fixed place:

- the LED state, a `uint8_t` with the value 1 (one byte),
- the boot count, a `uint16_t` with the value 513 (two bytes),
- the temperature, an `int32_t` with the value -1234 in hundredths of a degree, so -12.34 degrees (four bytes),
- the voltage, a `float` with the value 3.3 (four bytes).

A *buffer* is a block of memory that the writer fills, here a vector of 16 bytes. The writer remembers its
*position*, the index of the next free byte. Every write puts its bytes at the position and moves the position on.

Now find `// every write must fit`. When a field does not fit into the rest of the buffer, the writer becomes
*invalid*: `getValid()` returns false and the writer ignores every later write. So one check after all the writes is
enough.

## Step 2: Run the program

```
build/sercom_example_05_write_and_read_fields
```

You see:

```
write the status record
  bytes              : 01 01 02 2E FB FF FF 33 33 53 40
  size in bytes      : 11 written, 5 free
read the status record
  LED state          : 1
  boot count         : 513
  temperature        : -1234
  voltage            : 3.30
read a record cut after five bytes
  uint8 LED state    : ok, reader valid, position 1, remaining 4
  uint16 boot count  : ok, reader valid, position 3, remaining 2
  int32 temperature  : failed, reader invalid, position 3, remaining 2
  uint8 next byte    : failed, reader invalid, position 3, remaining 2
```

The first block shows the written bytes. Read them field by field:

- `01` is the LED state.
- `01 02` is the boot count 513, that is `0x0201`, least significant byte first.
- `2E FB FF FF` is the temperature -1234. Negative numbers are stored in *two's complement*, the usual form in every
  computer: -1234 as a 32 bit number is `0xFFFFFB2E`.
- `33 33 53 40` is the voltage 3.3. A float travels as its *IEEE 754* bit pattern, the standard layout of floating
  point numbers: 3.3 is the pattern `0x40533333`.
- `size in bytes` says that 11 bytes are written (1 + 2 + 4 + 4) and 5 of the 16 bytes are still free.

The second block shows the four fields that the reader got back. They are the values that were written. The voltage
is printed with two decimals.

## Step 3: Compare the fields

Find `// every field must be read back in the same order`. Each read returns true when it found enough bytes, and the
`&&` chain stops at the first false. The guard right below the chain stops the program when a read failed. Then find
`// fields must come back unchanged`: the program compares every read value with the written one.

Floats are compared with a margin, the largest difference you accept. Find
`// float read back from the same bytes must be exact`: the margin is zero here, because the reader returns exactly
the bit pattern that the writer stored.

## Step 4: Read a record that was cut short

Find `// read the same fields from a record cut after five bytes`. A second reader sees only the first five bytes,
like a payload that lost its end on the way, and reads into its own `cut...` variables. The third block shows each
read with the state of the reader after it:

- `uint8 LED state` reads one byte. The position is 1 and 4 bytes remain.
- `uint16 boot count` reads two bytes. The position is 3 and 2 bytes remain.
- `int32 temperature` needs four bytes, but only two are left. The read fails and the reader becomes invalid. The
  position stays at 3: a failed read takes no bytes.
- `uint8 next byte` fails as well, although two bytes are still there. Once the reader is invalid, every later read
  fails. This is called a *sticky error*, because the error sticks to the reader.

Find `// reader must stay invalid after the read past the end`. The program exits with 0 only when the reader behaved
like this. Print the exit code with `echo $?` and you see `0`.

## Try it yourself

1. Change the voltage `3.3F` to `5.0F`, build and run again. The bytes line becomes
   `  bytes              : 01 01 02 2E FB FF FF 00 00 A0 40`, and the voltage reads back as `5.00`.

2. Break it on purpose. In the `&&` chain under `// every field must be read back in the same order`, swap the
   first two reads, so that `readUint16(readBootCount)` comes before `readUint8(readLedState)`. Every read still
   succeeds, but the values are wrong: the LED state is `2` and the boot count is `257`. The program ends with this
   line and the exit code 1:

   ```
   [     0.000] WARNING Fields of the status record changed on the way back
   ```

   The bytes do not say which field they belong to. The reader must read the fields in the order of the writer.
   The temperature is still right, because the first two fields take three bytes in both orders.

3. Change `bufferSize` from 16 to 10. The float needs four bytes, but only three are left after the first three
   fields, so the writer becomes invalid. The program prints only this line and exits with 1:

   ```
   [     0.000] WARNING Can not write the status record
   ```

## Where this is used

- `core/include/ByteCodec.h` and `core/src/ByteCodec.cpp` define `ByteWriter` and `ByteReader`. The functions
  `reserve` and `consume` hold the bounds checks and make the writer and the reader invalid.
- `core/src/Frame.cpp` writes the header of every frame with a `ByteWriter`.
- Lectures: L27 "ByteWriter and the sticky error pattern", L28 "ByteReader: parsing untrusted bytes" and L29
  "Signed values, floats and strings".
