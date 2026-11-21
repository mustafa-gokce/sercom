# 18 Link Loopback

This example joins two links, one for the host and one for the device, in memory. The host
sends a message that asks for an acknowledgement, and you watch the acknowledgement come back
and the delivery being confirmed. It is the smallest picture of how the link layer makes sure
that a message arrived.

## What you will learn

- What a *link* is: the layer that numbers messages and waits for their acknowledgements.
- How an *acknowledgement* (ACK) frame echoes the message counter and the message type.
- What a *pending* message is: a sent message that still waits for its answer.

## Before you start

1. Watch lecture L70 "ACK and NACK frames" (section S11).
2. Install the build tools and Qt 5.12.12 once: `sudo apt install build-essential cmake ninja-build git` and the aqtinstall steps of README.md, section "Building the project", which also has the steps for Windows.
3. Open a terminal and go to the source directory of sercom, the directory that holds `CMakeLists.txt`.
4. If you cloned the repository just now, fetch its submodules once: `git submodule update --init --recursive`
5. Configure the build: `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH=$HOME/Qt/5.12.12/gcc_64`, on Windows with `-DCMAKE_PREFIX_PATH=C:/Qt/5.12.12/mingw73_64`
6. Build every program: `cmake --build build`
7. The program of this example is `build/sercom_example_18_link_loopback`.

To build only this example, run `cmake --build build --target sercom_example_18_link_loopback`.

## Step 1: Read the two helpers

Open `examples/18_link_loopback.cpp`. At the top are two small *test doubles*, classes that stand
in for real parts so the example needs no cable:

- `MockTransport` takes the bytes a link sends and keeps them in memory. It prints one line per
  frame it carries. `deliverData()` hands the kept bytes to the link on the other side.
- `MockLinkHandler` is the application above a link. It prints what the link reports: a received
  message, a confirmed delivery or a failed delivery.

You do not need to read every line of them. Their names start with `Mock` so you know they are
only for the example.

## Step 2: Read how the two sides are joined

Find `// join both links in memory` in `main()`. Each side has its own transport and its own
handler. `hostLink` writes into `hostTransport`, `deviceLink` writes into `deviceTransport`. The
link also needs a clock; `HostUtils::SteadyClock` is the normal clock of the host.

## Step 3: Read the conversation

Find `// set LED request must be sent`. The host sends a set LED request with the constant
`acknowledged`, which is `true`, as the `acknowledged` argument of `sendMessage()`, so the frame
carries the `ACK_REQUEST` flag. The link puts the message
into a *pending slot* and keeps a copy, in case it must send it again.

Find `// hand the frame of the host to the device`. The device link decodes the frame, hands the
message to its handler and answers with an ACK frame. Then
`// hand the acknowledgement of the device to the host` gives that ACK to the host link, which
frees the pending slot and tells its handler that the delivery is confirmed.

## Step 4: Run the program

Run `build/sercom_example_18_link_loopback`. You get:

```
host sends a set LED request and requests an acknowledgement
  host -> device  #0  SET_LED_REQUEST  ACK_REQUEST
host pending messages: 1
device received message #0 and answers ACCEPTED
  device -> host  #0  SET_LED_REQUEST  ACK
host got the acknowledgement of message #0
host pending messages: 0
```

Then run `echo $?` to see the exit code. It prints `0`.

## Step 5: Understand the output

- The indented lines are frames on the wire, printed by `MockTransport`: the direction, the
  *message counter* (`#0`, the number the link gave the message), the message type and the flags.
- `host pending messages: 1`: the message waits for its answer.
- `device received message #0 and answers ACCEPTED`: the device application took the message.
- `device -> host  #0  SET_LED_REQUEST  ACK`: the ACK frame has the same counter and the same
  type as the message it confirms, and no payload of its own.
- `host got the acknowledgement of message #0`: the host link matched the ACK to its pending
  message.
- `host pending messages: 0`: the slot is free again for the next message.

The exit code `0` means the device got the message once and the host got its acknowledgement.

## Try it yourself

1. Send without an acknowledgement. Change `acknowledged` from `true` to `false`. The first line
   stays the same, because it is fixed text. The frame shows the flags `NONE`, no ACK comes back,
   `host pending messages` is `0` at once, and the program ends with
   `WARNING The set LED request was not acknowledged` and exit code `1`, because this example
   expects an ACK.
2. Let the device say no. In `MockLinkHandler::onReceivedMessage()` change
   `MessageStatusType::ACCEPTED` to `MessageStatusType::BUSY`. The device prints
   `answers BUSY`, the frame back is a `NACK` (a *negative acknowledgement*), the host prints
   `host gave up message #0 (REJECTED)` and the exit code is `1`.
3. Lose the answer (this breaks the program on purpose). Delete the line
   `deviceTransport.deliverData(hostLink);`. The line `device -> host  #0  SET_LED_REQUEST  ACK`
   still prints, because `MockTransport` prints a frame when it is sent, not when it arrives. The
   host keeps `host pending messages: 1` and the exit code is `1`.

## Where this is used

- `core/include/Link.h` and `core/src/Link.cpp`: `Link::sendMessage()`, the pending slots and
  `sendAcknowledgement()`.
- `core/include/Transport.h`: the interface that `MockTransport` and the serial port implement.
- `tests/test_Link.cpp`: the same two links in memory, with many more cases.
- Lectures: L68 "Message counters and 16-bit wraparound", L70 "ACK and NACK frames" and L75
  "Testing the link layer in memory".
