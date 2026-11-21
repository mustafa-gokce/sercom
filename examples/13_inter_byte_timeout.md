# 13 Inter Byte Timeout

This example sends half a frame, lets a mock clock move past the inter byte timeout and shows
that the decoder drops the half frame and accepts the next frame. Without the timeout, a frame
that stopped in the middle would swallow the frames that come after it.

## What you will learn

- What the inter byte timeout is and when the decoder drops a partial frame.
- How `checkTimeout()` and the time argument of `onReceivedData()` work together.
- How a mock clock makes time-dependent code run the same way every time.

## Before you start

1. Watch lecture L51 "Inter-byte timeouts and time beyond 32 bits" (section S08).
2. Install the build tools and Qt 5.12.12 once: `sudo apt install build-essential cmake ninja-build git` and the aqtinstall steps of README.md, section "Building the project", which also has the steps for Windows.
3. Open a terminal and go to the source directory of sercom, the directory that holds `CMakeLists.txt`.
4. If you cloned the repository just now, fetch its submodules once: `git submodule update --init --recursive`
5. Configure the build: `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH=$HOME/Qt/5.12.12/gcc_64`, on Windows with `-DCMAKE_PREFIX_PATH=C:/Qt/5.12.12/mingw73_64`
6. Build every program: `cmake --build build`
7. The program of this example is `build/sercom_example_13_inter_byte_timeout`.

To build only this example, run `cmake --build build --target sercom_example_13_inter_byte_timeout`.

## Step 1: Read the mock clock

Open `examples/13_inter_byte_timeout.cpp` and read the class `MockClock` at the top. A real
clock moves by itself, so a program that reads it gives other numbers on every run. The mock
clock implements the same interface, `Clock::Clock`, but its time only moves when the program
calls `advanceTime()`. The tests of the link layer use the same trick.

The class `MockDecoderHandler` below it prints a `handler:` line for every frame and every
decode error.

## Step 2: Read the decoder setup

In `main()`, find `// create the decoder with the default inter byte timeout`. The *inter byte
timeout* is the longest silence allowed between two bytes of one frame.
`Settings::DEFAULT_INTER_BYTE_TIMEOUT_MSEC` in `core/include/Settings.h` sets it to 50 ms. A
*partial frame* is the start of a frame whose last bytes have not arrived yet.

## Step 3: Read the four steps

- `// receive only the first half of the first frame`: the decoder gets 8 of the 17 bytes, at
  the time of the mock clock, 0 ms.
- `// wait exactly the inter byte timeout`: the clock moves 50 ms, then `checkTimeout()` asks the
  decoder to drop a partial frame that waited too long.
- `// wait one millisecond longer than the inter byte timeout`: the clock moves 1 ms more and
  `checkTimeout()` runs again.
- `// receive the whole second frame`: the decoder gets a complete frame.

`printStep()` prints the time, the step and `frameDecoder.getBufferedSize()` after each step.

## Step 4: Run the program

Run `build/sercom_example_13_inter_byte_timeout`. You get:

```
inter byte timeout 50 ms
time   0 ms: received the first half of frame 1  decoder holds 8 bytes
time  50 ms: checked the timeout                 decoder holds 8 bytes
  handler: decode error INTER_BYTE_TIMEOUT
time  51 ms: checked the timeout                 decoder holds 0 bytes
  handler: frame received, message counter 2
time  51 ms: received the whole frame 2          decoder holds 0 bytes
```

Then run `echo $?` to see the exit code. It prints `0`.

## Step 5: Understand the output

- `time 0 ms`: the decoder holds the 8 bytes of the half frame and waits for the rest.
- `time 50 ms`: the line was silent for exactly 50 ms. That is not longer than the timeout, so
  the decoder still waits.
- `handler: decode error INTER_BYTE_TIMEOUT` and `time 51 ms ... decoder holds 0 bytes`: 51 ms
  is longer than the timeout, so the decoder dropped the 8 stale bytes. The handler line comes
  first because the decoder calls the handler from inside `checkTimeout()`.
- `handler: frame received, message counter 2`: the next frame decodes normally.

The exit code `0` means the decoder counted one timeout and discarded the 8 bytes
(`// decoder must drop the half frame`), and accepted frame 2 (`// decoder must accept the second
frame`).

## Try it yourself

1. Remove the two lines `frameDecoder.checkTimeout(mockClock.getTime());`. The steps still print
   `checked the timeout`, because it is fixed text, but at 51 ms the decoder holds 8 bytes. The
   `INTER_BYTE_TIMEOUT` line now comes right before `frame received`: `onReceivedByte()` checks
   the timeout itself before it takes the first new byte. The exit code is still `0`.
   `checkTimeout()` is needed only to drop a partial frame while no byte arrives.
2. Turn the timeout off (this breaks the program on purpose). Replace
   `Settings::DEFAULT_INTER_BYTE_TIMEOUT_MSEC` in the decoder line with `0ms`. The first line
   still prints 50 ms, because it prints the setting, but the decoder never drops the half
   frame. The 9th header byte is now the first byte of frame 2, `A5`, read as a payload length of
   165, so the decoder waits for a long frame: the last line says `decoder holds 25 bytes` and
   frame 2 is never received. The program prints a line ending in
   `WARNING The decoder did not drop the half frame` and the exit code is `1`.

## Where this is used

- `core/src/FrameDecoder.cpp`: `checkTimeout()` and `onReceivedByte()`.
- `core/include/Settings.h`: `DEFAULT_INTER_BYTE_TIMEOUT_MSEC`.
- `core/src/Link.cpp`: `maintain()` calls `checkTimeout()` with the time of its clock.
- `core/include/Clock.h`: the clock interface that the mock clock implements.
- Lectures: L51 "Inter-byte timeouts and time beyond 32 bits"; the injected clock is L74
  "Injecting the clock".
