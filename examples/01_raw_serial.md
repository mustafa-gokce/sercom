# 01 Raw Serial

This example opens a serial port, prints every byte that arrives and sends a short text once per second. It has no
frames yet: it shows the plain byte stream that the rest of the course builds on.

## What you will learn

- How to open a serial port with `SerialPort::SerialPort`, read from it with a timeout and send bytes.
- How to make a virtual serial cable with `socat`, so you need no hardware.
- Why raw bytes are not messages: nothing marks where a message starts or ends, and nothing shows a damaged byte.

## Before you start

1. Watch lectures L09 "Streams are not messages", L11 "A first serial program: raw bytes in and out" and L17 "Pseudo terminals and socat" first. You need three terminals in the source directory.
2. Install the build tools once: `sudo apt install build-essential cmake ninja-build git socat` (README.md, section "Building the project").
3. Open a terminal and go to the source directory of sercom, the directory that holds `CMakeLists.txt`.
4. If you cloned the repository just now, fetch its submodules once: `git submodule update --init --recursive`
5. Configure the build: `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug`
6. Build every program: `cmake --build build`
7. The program of this example is `build/sercom_example_01_raw_serial`.

To build only this example, run `cmake --build build --target sercom_example_01_raw_serial`.

## Step 1: Read the command line

Open `examples/01_raw_serial.cpp` and find the comment `// command line must be valid` in `main`, then the function
`parseCommandLine`. The program needs exactly one argument: the name of the serial port. Run it without one:

```
build/sercom_example_01_raw_serial
```

You see:

```
[     0.000] WARNING Missing serial port argument
Usage: build/sercom_example_01_raw_serial <serial-port>
Prints every byte of the serial port at 115200 8N1 and sends a text once per second.
```

- The first line is a warning. The number in brackets is the time in seconds since the first log line.
- The usage text says what the program expects. `115200 8N1` means 115200 bits per second, 8 data bits, no parity
  bit and 1 stop bit (lecture L06).

Print the exit code with `echo $?`. You see `64`, the value of `HostSettings::INVALID_COMMAND_LINE_EXIT_CODE` for a
wrong command line.

## Step 2: Make a virtual serial cable

A *pseudo terminal* is a serial port that the operating system makes in software. `socat` joins two of them, like a
cable with a plug at each end. In the second terminal run:

```
socat PTY,raw,echo=0,link=/tmp/ttyV1 PTY,raw,echo=0,link=/tmp/ttyV2
```

It prints nothing and keeps running. Bytes written into `/tmp/ttyV1` come out of `/tmp/ttyV2`, and the other way
round. `raw` passes every byte unchanged, and `echo=0` sends nothing back by itself. `tools/SOCAT.md` has more.

## Step 3: Open the port and listen

Find `// serial port must open` and `// read and send until a stop signal arrives`. The program opens the port at
`HostSettings::DEFAULT_BAUD_RATE`. In the loop, `serialPort.read` waits up to 100 ms (`readTimeout`) for bytes. The
status `TIMED_OUT` is no error: it only means that no byte came. Once per second (`messagePeriod`) the program sends
the text `a=1;b=2;c=3;` and a line feed. When a read or a send fails, the program warns the user and exits with 1.

In the first terminal run:

```
build/sercom_example_01_raw_serial /tmp/ttyV1
```

You see:

```
Listening on /tmp/ttyV1...
Press Ctrl+C to exit.
```

The program now waits for bytes.

## Step 4: Send bytes to the program

In the third terminal run:

```
printf 'Hi\n' > /tmp/ttyV2
```

The first terminal shows one line per byte:

```
Received: 0x48 'H'
Received: 0x69 'i'
Received: 0x0A '.'
```

Each line shows the byte in hex and as a character. `0x0A` is the line feed (`\n`). It is no printable character, so
`printReceivedByte` shows a dot instead.

## Step 5: Read what the program sends

In the third terminal run:

```
timeout 3 cat /tmp/ttyV2
```

First every line that waited in the cable since Step 3 appears at once, then one more line per second. You see at
least three lines like these:

```
a=1;b=2;c=3;
a=1;b=2;c=3;
a=1;b=2;c=3;
```

`cat` prints what arrives, and `timeout` stops it after three seconds. The program sends one line per second, so the
number of lines depends on how long the program ran before. The text has no start mark, no length and no check. If a
byte got lost on the way, the receiver could not tell. The frames of example 02 add all three.

## Step 6: Stop the program

Press Ctrl+C in the first terminal. The whole output of the first terminal is:

```
Listening on /tmp/ttyV1...
Press Ctrl+C to exit.
Received: 0x48 'H'
Received: 0x69 'i'
Received: 0x0A '.'

Serial device closed.
```

Your terminal shows `^C` on the empty line: that is your key press. Find `// install the stop handlers` and
`// close the serial port`. Ctrl+C sends the signal `SIGINT`. The stop handler clears the running flag, the loop ends
and the program closes the port. Print the exit code with `echo $?` and you see `0`. Stop `socat` with Ctrl+C in the
second terminal.

## Try it yourself

1. Send bytes that are no text: `printf 'A\x00\xff' > /tmp/ttyV2`. The first terminal shows:

   ```
   Received: 0x41 'A'
   Received: 0x00 '.'
   Received: 0xFF '.'
   ```

   A serial port carries every byte value from `0x00` to `0xFF`, not only text.

2. Change the text `message` from `"a=1;b=2;c=3;\n"` to `"hello\n"`, build and run again. `timeout 3 cat /tmp/ttyV2`
   now shows `hello` once per second.

3. Break it on purpose. Open a port that does not exist: `build/sercom_example_01_raw_serial /tmp/ttyV9`. The
   program prints this line and exits with 1:

   ```
   [     0.000] WARNING Can not open the serial port /tmp/ttyV9 (OPEN_FAILED)
   ```

   `OPEN_FAILED` is the status of `SerialPort::open` as text, from `SerialPort::fromSerialPortStatusType`.

4. Pull the cable. Start `socat` and the program on `/tmp/ttyV1` again, then stop `socat` with Ctrl+C in the second
   terminal. The read fails, the program prints this line and exits with 1:

   ```
   [     0.000] WARNING Can not read the serial port /tmp/ttyV1 (DISCONNECTED)
   ```

## Where this is used

- `host/include/SerialPort.h` and `host/src/SerialPort.cpp` define `open`, `read`, `sendData` and `close`.
- `host/src/HostUtils.cpp` defines `installStopHandlers`, `getRunning`, `SteadyClock`, `printLine` and `logWarning`.
- `host/include/HostSettings.h` holds `DEFAULT_BAUD_RATE`, `READ_BUFFER_SIZE` and the exit codes.
- `tools/SOCAT.md` explains virtual serial links.
- Lectures: L09 "Streams are not messages", L11 "A first serial program: raw bytes in and out", L16 "The port is
  complete: clocks, signals and a first run" and L17 "Pseudo terminals and socat".
