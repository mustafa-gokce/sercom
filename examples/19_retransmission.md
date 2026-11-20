# 19 Retransmission

In this example the wire loses the first frame. A mock clock moves the time forward, the host
link sees that no answer came in time and sends the message again, and this time the delivery is
confirmed. This is how the link layer turns a line that loses frames into one that delivers them.

## What you will learn

- What a *retransmission* is and when the link sends one.
- What the *retransmit timeout* is and how `maintain()` checks it.
- How a *mock clock* lets a program wait 250 ms without sleeping.

## Before you start

1. Watch lecture L68 "Retransmission timers and the RETRANSMISSION flag" (section S11) and run example 18 first.
2. Install the build tools once: `sudo apt install build-essential cmake ninja-build git` (README.md, section "Building the project").
3. Open a terminal and go to the source directory of sercom, the directory that holds `CMakeLists.txt`.
4. If you cloned the repository just now, fetch its submodules once: `git submodule update --init --recursive`
5. Configure the build: `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug`
6. Build every program: `cmake --build build`
7. The program of this example is `build/sercom_example_19_retransmission`.

To build only this example, run `cmake --build build --target sercom_example_19_retransmission`.

## Step 1: Read the helpers

Open `examples/19_retransmission.cpp`. `MockTransport` and `MockLinkHandler` are the ones of
example 18 with one addition: `MockTransport::setNextFrameLost()` makes the transport lose the
next frame it sends. A lost frame is printed with `(lost on the wire)` and never reaches the other side.
The transport still reports success, because a real serial port also does not know that the cable
dropped the bytes.

`MockClock` is new. Its time only moves when the program calls `advanceTime()`. The link asks
the clock for the time, so the program decides when the timeout has passed.

## Step 2: Read the lost frame

Find `// lose the first frame of the host`. Then the host sends the set LED request as in example
18. Find `// hand the host frames to the device`: there is nothing to hand over, the device has
received no frame, and the message stays pending.

## Step 3: Read the retransmission

Find `// let the retransmit timeout pass`. The clock moves forward by
`Settings::DEFAULT_RETRANSMIT_TIMEOUT_MSEC`, 250 ms. Find `// maintain the host link`.
`maintain()` looks at every pending message. When its last transmission is at least the
retransmit timeout old, the link sends it again with the `RETRANSMISSION` flag. The device
receives it, answers with an ACK and the host confirms the delivery.

At the end, `// print the statistics of the host link` prints the counters of the host link with
`HostUtils::getLinkStatisticsLines()`.

## Step 4: Run the program

Run `build/sercom_example_19_retransmission`. You get:

```
at   0 ms: host sends a set LED request and requests an acknowledgement
  host -> device  #0  SET_LED_REQUEST  ACK_REQUEST  (lost on the wire)
at   0 ms: device frames received: 0, host pending messages: 1
at 250 ms: retransmit timeout passed, host maintains its link
  host -> device  #0  SET_LED_REQUEST  ACK_REQUEST|RETRANSMISSION
device received message #0 and answers ACCEPTED
  device -> host  #0  SET_LED_REQUEST  ACK
host got the acknowledgement of message #0
host link statistics:
  frames sent                           2
  frames received                       1
  messages delivered                    0
  acknowledgements sent                 0
  acknowledgements received             1
  rejections sent                       0
  rejections received                   0
  retransmissions                       1
  deliveries failed                     0
  duplicates dropped                    0
  unexpected acknowledgements           0
  transport errors                      0
  decoder bytes received               13
  decoder bytes discarded               0
  decoder frames decoded                1
  decoder invalid versions              0
  decoder payloads too long             0
  decoder CRC mismatches                0
  decoder inter byte timeouts           0
```

Then run `echo $?` to see the exit code. It prints `0`.

## Step 5: Understand the output

- `at   0 ms` and `at 250 ms`: the time of the mock clock.
- `(lost on the wire)`: the first frame never arrives, so `device frames received: 0`.
- `ACK_REQUEST|RETRANSMISSION`: the same message with the same counter `#0`, now marked as a
  repeat. The device has never seen it, so it delivers it normally.
- `frames sent 2`: the lost frame and the retransmission. The host cannot tell them apart from
  its side, both left through the transport.
- `frames received 1` and `acknowledgements received 1`: the one ACK frame of the device.
- `messages delivered 0`: the host itself received no message, only an ACK.
- `retransmissions 1`: the frame sent again after the timeout.
- `decoder bytes received 13`: an ACK frame has no payload, so it is the 13 bytes of the frame
  overhead (`Definitions::FRAME_OVERHEAD`).

The exit code `0` means the host retransmitted once and the delivery was confirmed.

## Try it yourself

1. Wait a little less. Change the `clock.advanceTime(...)` line to
   `clock.advanceTime(Settings::DEFAULT_RETRANSMIT_TIMEOUT_MSEC - std::chrono::milliseconds(1));`.
   The output shows `at 249 ms`, no frame is sent again, and the program ends with
   `WARNING The host did not retransmit the set LED request once` and exit code `1`.
2. Lose the retransmission too (this breaks the program on purpose). Under
   `// maintain the host link`, add `hostTransport.setNextFrameLost(true);` above
   `hostLink.maintain();`. The retransmission is `(lost on the wire)` too, no ACK arrives and the
   exit code is `1`. The link would try again after another 250 ms, up to
   `Settings::DEFAULT_MAXIMUM_RETRANSMISSIONS` (5) times.
3. Lose nothing. Change the line to `hostTransport.setNextFrameLost(false);`. The first frame
   arrives at once (`device frames received: 1`), but the ACK still waits in `deviceTransport`,
   because this program hands it to the host only after `maintain()`. So the host retransmits
   although nothing was lost, the device answers the repeat once more, and the statistics show
   `acknowledgements received 1` and `unexpected acknowledgements 1`: the second ACK found no
   pending message. The exit code is still `0`. An answer that is slower than the retransmit
   timeout costs extra frames like this; L92 is about choosing the timeout.

## Where this is used

- `core/src/Link.cpp`: `Link::maintain()` with the retransmit timeout and the `RETRANSMISSION`
  flag.
- `core/include/Clock.h`: the clock interface that `MockClock` and `HostUtils::SteadyClock`
  implement.
- `core/include/Settings.h`: `DEFAULT_RETRANSMIT_TIMEOUT_MSEC` and
  `DEFAULT_MAXIMUM_RETRANSMISSIONS`.
- `tests/test_Link.cpp`: the tests of lost data with a mock clock.
- Lectures: L68 "Retransmission timers and the RETRANSMISSION flag", L71 "Injecting the clock"
  and L72 "Testing the link layer in memory".
