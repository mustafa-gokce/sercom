# 01 Raw Serial

This example opens a serial port with `QSerialPort`, prints every byte that arrives and sends a short text once per
second. It has no frames yet: it shows the plain byte stream that the rest of the course builds on. The same program
runs on Linux and on Windows.

## What you will learn

- How to open a serial port with `QSerialPort` in raw 8N1 mode, take its bytes on `readyRead` and send a text from a
  `QTimer` on the Qt event loop.
- How to make a virtual serial cable without hardware: `socat` on Linux, a com0com pair on Windows.
- Why raw bytes are not messages: nothing marks where a message starts or ends, and nothing shows a damaged byte.

## Before you start

1. Watch lectures L09 "Streams are not messages", L11 "A first serial program: raw bytes in and out" and L17 "Pseudo terminals and socat" first. You need three terminals in the source directory.
2. Install the build tools and Qt 5.12.12 once: `sudo apt install build-essential cmake ninja-build git socat` and the aqtinstall steps of README.md, section "Building the project", which also has the steps for Windows. On Windows create the com0com pair `COM10` and `COM11` of `tools/COM0COM.md`.
3. Open a terminal and go to the source directory of sercom, the directory that holds `CMakeLists.txt`. On Windows open PowerShell 7 with the path of the README.
4. If you cloned the repository just now, fetch its submodules once: `git submodule update --init --recursive`
5. Configure the build: `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH=$HOME/Qt/5.12.12/gcc_64`, on Windows with `-DCMAKE_PREFIX_PATH=C:/Qt/5.12.12/mingw73_64`
6. Build every program: `cmake --build build`
7. The program of this example is `build/sercom_example_01_raw_serial`, `build/sercom_example_01_raw_serial.exe` on Windows.

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

Print the exit code with `echo $?`, in PowerShell with `$LASTEXITCODE`. You see `64`, the value of
`HostSettings::INVALID_COMMAND_LINE_EXIT_CODE` for a wrong command line.

## Step 2: Make a virtual serial cable

A virtual serial cable is a pair of serial ports that the operating system makes in software, like a cable with a
plug at each end.

On Linux, `socat` joins two *pseudo terminals*. In the second terminal run:

```
socat PTY,raw,echo=0,link=/tmp/ttyV1 PTY,raw,echo=0,link=/tmp/ttyV2
```

It prints nothing and keeps running. Bytes written into `/tmp/ttyV1` come out of `/tmp/ttyV2`, and the other way
round. `raw` passes every byte unchanged, and `echo=0` sends nothing back by itself. `tools/SOCAT.md` has more.

On Windows, the com0com pair of `tools/COM0COM.md` is the cable: bytes written into `COM10` come out of `COM11`, and
the other way round. It stays there until you remove it, so the second terminal is free.

## Step 3: Open the port and listen

Find `// serial port must open in raw 8N1 mode without flow control`. The program sets the port name, the baud rate
`HostSettings::DEFAULT_BAUD_RATE`, 8 data bits, no parity, one stop bit and no flow control on a `QSerialPort`, then
opens it for reading and writing. `QSerialPort` turns these settings into termios calls on Linux and into a `DCB` on
Windows, so the program has no code of its own for either system.

Then find `// print every byte that arrives` and `// send a text once per second and stop when a stop signal arrives`.
Nothing reads in a loop: `QCoreApplication::exec()` runs the Qt event loop, which calls the `readyRead` lambda as
soon as bytes arrive. A `QTimer` calls its lambda every 100 ms (`timerPeriod`); once per second (`messagePeriod`) it
sends the text `a=1;b=2;c=3;` and a line feed. When the port fails or a send fails, the program warns the user and
leaves the event loop with 1.

In the first terminal run:

```
build/sercom_example_01_raw_serial /tmp/ttyV1
```

On Windows run `build/sercom_example_01_raw_serial.exe COM10`. You see:

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

On Windows send the same bytes from PowerShell:

```
$port = New-Object System.IO.Ports.SerialPort COM11, 115200; $port.Open(); $port.Write("Hi`n"); $port.Close()
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

On Windows read the other port for three seconds:

```
$port = New-Object System.IO.Ports.SerialPort COM11, 115200; $port.Open(); Start-Sleep -Seconds 3; $port.ReadExisting(); $port.Close()
```

You see lines like these, one per second:

```
a=1;b=2;c=3;
a=1;b=2;c=3;
a=1;b=2;c=3;
```

The program sends one line per second; on Linux the lines that waited in the cable since Step 3 come first, so the
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

On Linux your terminal shows `^C` on the empty line: that is your key press. Find `// install the stop handlers`,
`// stop signal must not have arrived` and `// close the serial port`. Ctrl+C sends the signal `SIGINT` on both
systems. The stop handler only clears the running flag; the next tick of the timer sees it and quits the event loop,
`QCoreApplication::exec()` returns and the program closes the port. Print the exit code and you see `0`. Stop `socat`
with Ctrl+C in the second terminal.

## Try it yourself

1. Send bytes that are no text: `printf 'A\x00\xff' > /tmp/ttyV2`, on Windows
   `$port.Write([byte[]](0x41, 0x00, 0xFF), 0, 3)` with the port of Step 4. The first terminal shows:

   ```
   Received: 0x41 'A'
   Received: 0x00 '.'
   Received: 0xFF '.'
   ```

   A serial port carries every byte value from `0x00` to `0xFF`, not only text.

2. Change the text `message` from `"a=1;b=2;c=3;\n"` to `"hello\n"`, build and run again. Step 5 now shows `hello`
   once per second.

3. Break it on purpose. Open a port that does not exist: `build/sercom_example_01_raw_serial /tmp/ttyV9`. The
   program prints this line and exits with 1:

   ```
   [     0.000] WARNING Can not open the serial port /tmp/ttyV9 (No such file or directory)
   ```

   The text in parentheses is `QSerialPort::errorString()`, the error text of the system; on Windows `COM99` gives the
   Windows text for a missing file.

4. Pull the cable on Linux. Start `socat` and the program on `/tmp/ttyV1` again, then stop `socat` with Ctrl+C in the
   second terminal. The port fails, the program prints a line like this one and exits with 1:

   ```
   [     0.000] WARNING Can not read the serial port /tmp/ttyV1 (Inappropriate ioctl for device)
   ```

   A com0com pair has no cable to pull: it keeps both ports until you remove it. Unplug a USB-serial adapter instead.

## Where this is used

- `host/include/SerialPort.h` and `host/src/SerialPort.cpp` wrap the same `QSerialPort` calls for the tools, with the
  received bytes as the signal `receivedData`.
- `host/src/HostUtils.cpp` defines `installStopHandlers`, `getRunning`, `SteadyClock`, `printLine` and `logWarning`.
- `host/include/HostSettings.h` holds `DEFAULT_BAUD_RATE` and the exit codes.
- `tools/SOCAT.md` explains virtual serial links on Linux, `tools/COM0COM.md` the com0com pairs of Windows.
- Lectures: L09 "Streams are not messages", L11 "A first serial program: raw bytes in and out", L16 "The port is
  complete: clocks, signals and a first run" and L17 "Pseudo terminals and socat".
