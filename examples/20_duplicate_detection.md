# 20 Duplicate Detection

In this example the device receives a message and answers it, but the wire loses the answer. The
host does not know that the message arrived, so it sends the message again. The device sees the
same message a second time, answers it again and does not hand it to its application twice. This
matters for a message like "add 10 to the counter": done twice, it would be wrong.

## What you will learn

- Why a lost acknowledgement makes the same message arrive twice.
- How the receiving link recognises a *duplicate*, a copy of a message it already delivered.
- What the device link statistics count in this case.

## Before you start

1. Watch lecture L72 "Duplicate suppression and restarted peers" (section S11) and run example 19 first.
2. Install the build tools and Qt 5.12.12 once: `sudo apt install build-essential cmake ninja-build git` and the aqtinstall steps of README.md, section "Building the project", which also has the steps for Windows.
3. Open a terminal and go to the source directory of sercom, the directory that holds `CMakeLists.txt`.
4. If you cloned the repository just now, fetch its submodules once: `git submodule update --init --recursive`
5. Configure the build: `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH=$HOME/Qt/5.12.12/gcc_64`, on Windows with `-DCMAKE_PREFIX_PATH=C:/Qt/5.12.12/mingw73_64`
6. Build every program: `cmake --build build`
7. The program of this example is `build/sercom_example_20_duplicate_detection`.

To build only this example, run `cmake --build build --target sercom_example_20_duplicate_detection`.

## Step 1: Read the setup

Open `examples/20_duplicate_detection.cpp`. The helpers `MockClock`, `MockTransport` and
`MockLinkHandler` are the ones of example 19. Find `// lose the first frame of the device`. This
time the device transport loses its first frame, and the first frame the device sends is the
acknowledgement (ACK).

## Step 2: Read the lost acknowledgement

Find `// hand the frame of the host to the device`. The device receives the set LED request,
delivers it to its handler and sends the ACK, which is lost. Find
`// hand the device frames to the host`: nothing arrives, and the host still has one pending
message.

## Step 3: Read the duplicate

Find `// let the retransmit timeout pass` and `// maintain the host link`. As in example 19 the
host sends the message again with the `RETRANSMISSION` flag. Find
`// hand the retransmission of the host to the device`. The device link keeps a *history* of the
acknowledged messages it received recently (`Settings::DefaultDuplicateHistorySize` entries).
It finds the same message counter and message type there, so it does not call its handler. It
only sends the answer it gave the first time. Find
`// hand the repeated acknowledgement to the host`: now the host confirms the delivery.

Only a frame with the `RETRANSMISSION` flag is checked against the history. A new message never
is, so a device that restarted cannot mistake a new message for an old one.

## Step 4: Run the program

Run `build/sercom_example_20_duplicate_detection`. You get:

```
at   0 ms: host sends a set LED request and requests an acknowledgement
  host -> device  #0  SET_LED_REQUEST  ACK_REQUEST
device received message #0 and answers ACCEPTED
  device -> host  #0  SET_LED_REQUEST  ACK  (lost on the wire)
at   0 ms: host frames received: 0, host pending messages: 1
at 250 ms: retransmit timeout passed, host maintains its link
  host -> device  #0  SET_LED_REQUEST  ACK_REQUEST|RETRANSMISSION
  device -> host  #0  SET_LED_REQUEST  ACK
host got the acknowledgement of message #0
device link statistics:
  frames sent                           2
  frames received                       2
  messages delivered                    1
  acknowledgements sent                 2
  acknowledgements received             0
  rejections sent                       0
  rejections received                   0
  retransmissions                       0
  deliveries failed                     0
  duplicates dropped                    1
  unexpected acknowledgements           0
  transport errors                      0
  decoder bytes received               28
  decoder bytes discarded               0
  decoder frames decoded                2
  decoder invalid versions              0
  decoder payloads too long             0
  decoder CRC mismatches                0
  decoder inter byte timeouts           0
```

Then run `echo $?` to see the exit code. It prints `0`.

## Step 5: Understand the output

- `device received message #0 and answers ACCEPTED` appears once, although the message arrived
  twice. After the retransmission only the ACK frame follows.
- `ACK  (lost on the wire)`: the first answer never reached the host.
- `frames received 2`: the device got the message and its retransmission.
- `messages delivered 1`: its handler saw the message once.
- `duplicates dropped 1`: the retransmission was recognised and not delivered again.
- `acknowledgements sent 2`: the device answered both copies, the lost one and the repeat.
- `decoder bytes received 28`: two frames of 13 bytes overhead and 1 payload byte each.

The exit code `0` means the device delivered the message once, dropped one duplicate and the host
got its acknowledgement.

## Try it yourself

1. Lose the message instead of the answer. Change `deviceTransport.setNextFrameLost(true);` to
   `hostTransport.setNextFrameLost(true);`. Now it is example 19: the retransmission is the first
   copy the device sees, so it is delivered, `duplicates dropped` is `0`, and the program ends with
   `WARNING The device did not drop the duplicate set LED request` and exit code `1`.
2. Lose nothing. Change the line to `deviceTransport.setNextFrameLost(false);`. The ACK arrives at
   0 ms (`host frames received: 1, host pending messages: 0`), the host sends nothing again and
   the exit code is `1` for the same reason.
3. Let the device answer BUSY (this breaks the program on purpose). In
   `MockLinkHandler::onReceivedMessage()` change `MessageStatusType::ACCEPTED` to
   `MessageStatusType::BUSY`. The device remembers its answer, so the retransmission gets the same
   `NACK` again, the host prints `host gave up message #0 (REJECTED)` and the exit code is `1`.

## Where this is used

- `core/src/Link.cpp`: `Link::onReceivedFrame()` with the duplicate check,
  `getReceivedMessageWithMessageCounter()` and `rememberReceivedMessage()`.
- `core/include/Settings.h`: `DefaultDuplicateHistorySize`.
- `tests/test_Link.cpp`: the lost ACK and peer restart tests.
- Lectures: L72 "Duplicate suppression and restarted peers", L73 "The spread limit: a bug found by
  the end-to-end test" and L75 "Testing the link layer in memory".
