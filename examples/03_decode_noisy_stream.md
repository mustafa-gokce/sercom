# 03 Decode a Noisy Stream

This example builds a stream of 1000 frames, damages it with a seeded random generator and decodes it with
`FrameDecoder::FrameDecoder`. It shows that the decoder finds the frames that arrived intact and skips the damage.

## What you will learn

- How noise damages a byte stream: dropped bytes, flipped bits, doubled bytes and extra bytes.
- How the decoder statistics count the decoded frames, the skipped bytes and each kind of error.
- Why a fixed seed makes a random test give the same result in every run.

## Before you start

1. Watch lectures L47 "Resynchronization: drop one byte, rescan the rest", L50 "Handler interface and decoder statistics" and L55 "Soak tests with seeded random damage" first. L18 "noise.py: modelling a bad cable" explains the kinds of noise.
2. Install the build tools and Qt 5.12.12 once: `sudo apt install build-essential cmake ninja-build git` and the aqtinstall steps of README.md, section "Building the project", which also has the steps for Windows.
3. Open a terminal and go to the source directory of sercom, the directory that holds `CMakeLists.txt`.
4. If you cloned the repository just now, fetch its submodules once: `git submodule update --init --recursive`
5. Configure the build: `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH=$HOME/Qt/5.12.12/gcc_64`, on Windows with `-DCMAKE_PREFIX_PATH=C:/Qt/5.12.12/mingw73_64`
6. Build every program: `cmake --build build`
7. The program of this example is `build/sercom_example_03_decode_noisy_stream`.

To build only this example, run `cmake --build build --target sercom_example_03_decode_noisy_stream`.

## Step 1: Read the arguments

Open `examples/03_decode_noisy_stream.cpp` and find the comment `// command line must be valid` in `main`, then the
function `parseCommandLine`. Both arguments are optional:

- the *seed*, the start value of the random generator, 1 by default,
- the *error rate*, the chance of each kind of damage per byte, 0.005 (0.5 percent) by default.

## Step 2: Build the clean stream

Find `// build the clean stream of numbered frames`. The loop builds 1000 echo request frames
(`Messages::MessageType::ECHO_REQUEST`) with a payload of 32 bytes each. Every frame has 13 bytes of overhead plus the
payload, so 45 bytes, and the stream has 45000 bytes.

## Step 3: Damage the stream

Find `// damage the stream with the seeded random generator`. `std::mt19937` is the random generator of the C++
standard library, started with the seed. `addNoise` at the end of the file draws from it: for every byte the lambda
under `// iterate over the bytes to damage them by chance` has four chances of damage, each with the error rate:

- `// byte must not be dropped`: the byte is lost,
- `// flip one bit of the byte by chance`: one bit of the byte changes,
- `// duplicate the byte by chance`: the byte arrives twice,
- `// follow the byte by garbage by chance`: a random byte comes after it.

The same seed gives the same damage in every run, so a failure can be repeated.

## Step 4: Decode the stream

Find `// decode the noisy stream without the inter byte timeout`. A *handler* is the object that the decoder calls for
every decoded frame and every error. `MockDecoderHandler` at the top of the file does nothing, because the decoder
statistics already count everything. A *mock* is a small stand-in object for a test or an example. The inter byte
timeout `0ms` switches the timeout off, because all bytes arrive in one call of `onReceivedData`.

## Step 5: Run the program

```
build/sercom_example_03_decode_noisy_stream
```

You see:

```
seed 1, error rate 0.0050
frames sent 1000, frames decoded 397 (39.7%)
bytes 45000 -> 45267, discarded 27267, CRC errors 553, length errors 0, version errors 19
```

- The first line repeats the seed and the error rate.
- 397 of the 1000 frames were decoded. With four chances of 0.5 percent for each of its 45 bytes, a frame stays
  intact with a chance of about 0.995 to the power of 180, that is about 41 percent. The decoder finds these frames.
- The stream grew from 45000 to 45267 bytes, because doubled and extra bytes are more than the dropped ones.
- `discarded` counts the bytes that did not end up in a valid frame: mostly the bytes of damaged frames and the
  garbage between frames.
- `CRC errors` counts frame candidates whose CRC32 did not match. A *candidate* is a byte sequence that starts with
  the sync word and looks like a frame until the decoder checks it.
- `length errors` stays 0 in this build: the limit is 255 bytes, so every payload length byte is allowed (lecture
  L45).
- `version errors` counts candidates whose version byte was not 1.

The numbers come from GCC's standard library. The C++ standard fixes the generator `std::mt19937` but not the
distributions that turn its numbers into chances and bytes, so another standard library may print other numbers for
the same seed. The program always exits with 0: it reports the numbers and does not judge them.

## Try it yourself

1. Use another seed: `build/sercom_example_03_decode_noisy_stream 2`. The damage is different, so are the numbers:

   ```
   seed 2, error rate 0.0050
   frames sent 1000, frames decoded 387 (38.7%)
   bytes 45000 -> 45252, discarded 27837, CRC errors 572, length errors 0, version errors 11
   ```

2. Switch the noise off: `build/sercom_example_03_decode_noisy_stream 1 0`. Every frame arrives:

   ```
   seed 1, error rate 0.0000
   frames sent 1000, frames decoded 1000 (100.0%)
   bytes 45000 -> 45000, discarded 0, CRC errors 0, length errors 0, version errors 0
   ```

   Then try a lighter noise: `build/sercom_example_03_decode_noisy_stream 1 0.001` decodes `820 (82.0%)` frames.

3. Break it on purpose. Give an error rate that is no probability:
   `build/sercom_example_03_decode_noisy_stream 1 2`. The program prints:

   ```
   [     0.000] WARNING Invalid error rate argument 2, expected a number from 0 to 1
   Usage: build/sercom_example_03_decode_noisy_stream [seed] [error-rate]
   Damages a stream of frames with a seeded random generator and decodes it, the error rate is per byte.
   ```

   `echo $?` shows `64`, the exit code of a wrong command line.

## Where this is used

- `core/include/FrameDecoder.h` and `core/src/FrameDecoder.cpp` define the decoder, its handler interface and
  `DecoderStatistics`. `rejectCandidate` drops one byte of a bad candidate and scans on.
- `core/src/Frame.cpp` checks each candidate with `getCRCValid`.
- `tools/noise.py` and `tools/NOISE.md` put the same kinds of noise on a virtual serial link.
- `tests/test_FrameDecoder.cpp` checks with seeded random damage that a damaged stream only yields frames that were
  sent intact.
- Lectures: L47 "Resynchronization: drop one byte, rescan the rest", L50 "Handler interface and decoder statistics",
  L55 "Soak tests with seeded random damage" and L18 "noise.py: modelling a bad cable".
