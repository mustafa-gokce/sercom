# 25 Dispatch Messages

This example hands four frames to the frame decoder, and its handler passes every message on to
one function, chosen by the message type: a ping request, a device info request, a set LED request
and a message type that the device does not know. A device works the same way: the message type
of a frame tells it how to read the payload and what to do.

## What you will learn

- How a `switch` over `Messages::toMessageType()` hands every message to a function of its own.
- Why every message type without a function of its own ends in one place, unknown types included.
- How each function reads the fields of its message from the frame.

## Before you start

1. Watch lecture L65 "Dispatching messages" (section S10), and run example 10 first.
2. Install the build tools and Qt 5.12.12 once: `sudo apt install build-essential cmake ninja-build git` and the aqtinstall steps of README.md, section "Building the project", which also has the steps for Windows.
3. Open a terminal and go to the source directory of sercom, the directory that holds `CMakeLists.txt`.
4. If you cloned the repository just now, fetch its submodules once: `git submodule update --init --recursive`
5. Configure the build: `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH=$HOME/Qt/5.12.12/gcc_64`, on Windows with `-DCMAKE_PREFIX_PATH=C:/Qt/5.12.12/mingw73_64`
6. Build every program: `cmake --build build`
7. The program of this example is `build/sercom_example_25_dispatch_messages`.

To build only this example, run `cmake --build build --target sercom_example_25_dispatch_messages`.

## Step 1: Read the switch

Open `examples/25_dispatch_messages.cpp` and read `onReceivedFrame()` in the class
`MockDecoderHandler` at the top. As in example 10, the decoder calls this function for every
frame that passed every check. It prints the message type of the frame, then comes the comment
`// handle the message by its message type` and the `switch`.

`Messages::toMessageType()` turns the 16 bit number of the frame into a `MessageType` value. A
number that the demo protocol does not use becomes `UNKNOWN`, so the `switch` only ever sees
named values. Each `case` does one thing: it calls the function of its message type.

- `PING_REQUEST` calls `handlePingRequest()`.
- `DEVICE_INFO_REQUEST` calls `handleDeviceInfoRequest()`.
- `SET_LED_REQUEST` calls `handleSetLedRequest()`.
- Every other value calls `handleUnsupportedMessage()`: the responses, which travel from the
  device to the host, the echo request and the set telemetry period request, which this example
  has no function for, and `UNKNOWN`. `default:` sits below `UNKNOWN` and closes the `switch`.

The last group names every other message type, so a reader sees that each one was thought
about. The compiler does not check that list: with a `default:` it accepts a missing name too.

## Step 2: Read the functions

Below the first `private:` are the four functions. Each reads what its message holds, prints one line
that starts with its own name and counts the message.

- `handlePingRequest()` builds a `Messages::PingRequest` from the frame and reads its request
  timestamp.
- `handleDeviceInfoRequest()` reads nothing, because a device info request has no fields.
- `handleSetLedRequest()` builds a `Messages::SetLedRequest` from the frame and reads the LED state.
- `handleUnsupportedMessage()` does not read the payload. Without a known message type it can not
  know which fields the payload holds.

The functions count into `handledMessageCounter` and `unsupportedMessageCounter`. They do not
check the payload yet; lecture L66 "Validating payloads" adds that check.

## Step 3: Read how the frames are made

In `main()`, find the comment `// receive a ping request sent at one second`. Each of the four
blocks calls `receiveMessage()` with a message type and a message. `receiveMessage()` at the end
of the file builds a frame from both and hands it to the decoder, which calls the handler before
the call returns. Every frame carries the message counter 1, because the dispatch reads only the
message type.

The fourth block,
`// receive a message without payload of a message type that the device does not know`, sends
`0x0200`, a value that the demo protocol does not use.
`Messages::GenericMessage()` is a message without payload.

Find `// handler must handle the three requests and find the unknown message type unsupported`.
The program checks both counters of the handler.

## Step 4: Run the program

Run `build/sercom_example_25_dispatch_messages`. You get:

```
frame received, message type 0x0001
  handlePingRequest: request timestamp 1000 ms
frame received, message type 0x0003
  handleDeviceInfoRequest: no fields to read
frame received, message type 0x0100
  handleSetLedRequest: LED on
frame received, message type 0x0200
  handleUnsupportedMessage: message type not supported
```

Then run `echo $?` to see the exit code. It prints `0`.

## Step 5: Understand the output

- `message type 0x0001` is `PING_REQUEST`, and `handlePingRequest()` read the request
  timestamp 1000 ms from the payload.
- `message type 0x0003` is `DEVICE_INFO_REQUEST`, a message without fields.
- `message type 0x0100` is `SET_LED_REQUEST`, and its payload asks for the LED on.
- `message type 0x0200` is not a type of the demo protocol: `toMessageType()` makes it `UNKNOWN`,
  and the message ends in `handleUnsupportedMessage()`.
- Every `frame received` line is followed by exactly one function line: one function per message.

The exit code `0` means three messages went to their own function and one went to
`handleUnsupportedMessage()`.

## Try it yourself

1. Switch the LED off. Set `ledOn` to `false`. The third function line becomes
   `handleSetLedRequest: LED off`.
2. Send a known type without a function. Set `unknownMessageType` to `0x0006`, the value of
   `ECHO_REQUEST`. The last frame line shows `message type 0x0006`, and the message still ends in
   `handleUnsupportedMessage()`, because this `switch` lists `ECHO_REQUEST` in the last group. The
   exit code stays `0`.
3. Forget a case (this breaks the program on purpose). Delete the case of the device info
   request, the five lines from `// handle a device info request` to the `}` below its `break;`.
   The program builds without a warning, because `default:` now catches the device info request.
   The second function line becomes `handleUnsupportedMessage: message type not supported`, the
   program prints a line ending in
   `WARNING The handler did not hand every message to its function` and the exit code is `1`.

## Where this is used

- `app/include/Messages.h` and `app/src/Messages.cpp`: `MessageType` and `toMessageType()`.
- `core/include/FrameDecoder.h`: the handler interface `FrameDecoderHandler`.
- Lectures: L65 "Dispatching messages", L62 "Message types and enum converters" and L53 "Handler
  interface and decoder statistics"; the dispatch of the device is L77 "The Qt event loop: one
  thread, signals and a timer" and the host side L78 "DeviceClient: requests and replies".
