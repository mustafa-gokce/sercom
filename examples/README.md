# sercom examples

Small Standalone Programs with Step-by-Step Guides

Every example is one short program in the `examples/` directory with a guide of the same name next to it. The guide says which
lectures to watch first, builds and runs the program, walks through the source step by step, explains the output
and ends with changes to try. The examples use only the public API of the `core`, `app` and `host` libraries.
Examples 01 and 24 open a serial port, a virtual serial cable made with `socat` stands in for the hardware. The others
run in memory with fixed inputs and print the same output on every run.

An example exits with 0 when its result is as expected, with 1 when it is not, and with 64 for a wrong command line.

## Examples

Work through the table from the top, not through the files by number. The rows follow the order of the course: the
first two columns say when an example is due, the section and the lectures to watch first, the main lecture first. The
numbers therefore jump, for example example 02 comes after the CRC examples 06 and 07. Example 23 sits in section S11,
because its messages travel over the link layer of example 18.

| Section | Watch first | No. | Title | What it shows | Guide and source |
|---|---|---|---|---|---|
| S01 | L09, L11, L16, L17 | 01 | Raw Serial | Prints every byte of a serial port and sends a text once per second: a byte stream without frames | [guide](01_raw_serial.md), [source](01_raw_serial.cpp) |
| S04 | L24, L25, L26 | 05 | Write and Read Fields | Writes and reads integer and float fields with `ByteWriter` and `ByteReader`, then the sticky error of a record cut short | [guide](05_write_and_read_fields.md), [source](05_write_and_read_fields.cpp) |
| S05 | L27, L29 (also L46) | 08 | Read a Frame | Builds a frame from its 17 bytes, prints every getter and shows which check finds a flipped bit and a lost byte | [guide](08_read_a_frame.md), [source](08_read_a_frame.cpp) |
| S06 | L37, L38 (also L35) | 06 | CRC32 of a Text | The CRC32 of `123456789` equals the check value `0xCBF43926`, and every flipped bit changes it | [guide](06_crc32_of_text.md), [source](06_crc32_of_text.cpp) |
| S08 | L44, L47 (also L50) | 11 | Resync After Garbage | Finds two frames between garbage bytes, the `A5 A5 C3` trap included, and prints the decoder statistics | [guide](11_resync_after_garbage.md), [source](11_resync_after_garbage.cpp) |
| S09 | L55 (also L18, L47, L50) | 03 | Decode a Noisy Stream | Damages 1000 frames with seeded random noise and decodes the frames that arrived intact | [guide](03_decode_noisy_stream.md), [source](03_decode_noisy_stream.cpp) |

## Building the examples
1. Open a terminal.
2. Install the build tools once: `sudo apt install build-essential cmake ninja-build git socat`
3. Go to the source directory, the directory that holds `CMakeLists.txt`.
4. Populate dependencies once: `git submodule update --init --recursive`
5. Configure the build: `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug`
6. Build every program: `cmake --build build`
7. The program of example NN is `build/sercom_example_NN_name`, for example `build/sercom_example_02_build_frame`.
8. Build one example only with its target: `cmake --build build --target sercom_example_02_build_frame`

## Run one example
1. Go to the source directory.
2. Run the program of the example: `build/sercom_example_02_build_frame`
3. Print its exit code: `echo $?`
4. Compare the output with the guide of the example, `examples/02_build_frame.md`.
5. Examples 01 and 24 take a serial port as their argument, their guides show how to make one with `socat`.

## Run all examples
1. Install `socat` for examples 01 and 24: `sudo apt install socat`
2. Go to the source directory.
3. Run every example of the build directory: `tools/run_examples.sh build`
4. The script builds nothing, it runs each program and prints `passed` or `failed` with its name.
5. It runs example 01 on a `socat` pseudo terminal pair, sends it `hello`, stops it after two seconds with an interrupt signal and checks that it printed the received bytes.
6. It runs example 24 on a `socat` pseudo terminal pair against `build/sercom_device_sim`.
7. Without `socat` it skips examples 01 and 24 with a message.
8. The exit code is 0 when every example exited with 0, otherwise 1.
