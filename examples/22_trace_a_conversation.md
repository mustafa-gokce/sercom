# 22 Trace a Conversation

This example puts the frame tracer on the host side of an in-memory conversation and prints
every frame in both directions: a ping and a device info request, their responses and all
acknowledgements. It is the same trace that `sercom_cli --trace` and `sercom_device_sim --trace`
print, so after this example you can read those lines.

## What you will learn

- How `FrameTracer` turns the sent and the received bytes into one line per frame.
- How `TraceTransport` sits between the device client and its transport to see every sent byte.
- How to read a trace line: time, direction, message counter, type, flags, length, payload, CRC.

## Before you start

1. Watch lecture L79 "The sercom_cli tool on Linux and Windows" (section S12) and run example 21 first.
2. Install the build tools and Qt 5.12.12 once: `sudo apt install build-essential cmake ninja-build git` and the aqtinstall steps of README.md, section "Building the project", which also has the steps for Windows.
3. Open a terminal and go to the source directory of sercom, the directory that holds `CMakeLists.txt`.
4. If you cloned the repository just now, fetch its submodules once: `git submodule update --init --recursive`
5. Configure the build: `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH=$HOME/Qt/5.12.12/gcc_64`, on Windows with `-DCMAKE_PREFIX_PATH=C:/Qt/5.12.12/mingw73_64`
6. Build every program: `cmake --build build`
7. The program of this example is `build/sercom_example_22_trace_a_conversation`.

To build only this example, run `cmake --build build --target sercom_example_22_trace_a_conversation`.

## Step 1: Read the trace handler

Open `examples/22_trace_a_conversation.cpp`. `MockClock` and `MockTransport` are the ones of
example 21. `MockFrameTracerHandler` is new and short: the tracer calls
`onReceivedTracedFrame()` for every frame and `onReceivedUnframedData()` for bytes that are not
part of a frame, for example the boot text of a board. Both print one line with the helpers
`HostUtils::getTracedFrameLine()` and `HostUtils::getUnframedDataLine()`.

`MockClientHandler` prints only the ping and the device info response; the trace shows the rest.
The interface asks for a callback per message, so the class ends with a second `public:` section
of empty callbacks: skip the empty callbacks.

## Step 2: Read how the tracer is put in

Find `// create the simulated device`. The device uses `SimulatedPlatform` from the host library,
the platform of the device simulator.

Find `// create the tracing host`. Three objects are chained:

- `frameTracer` decodes bytes for display. `TraceDirectionType::HOST_TO_DEVICE` says that the
  bytes this side sends travel from the host to the device.
- `traceTransport` is a transport that passes every byte on to `hostTransport` and also hands it
  to `frameTracer.onSentData()`.
- `deviceClient` sends through `traceTransport`, so it does not know that it is traced.

Now read `exchangeData()`: `// hand the device data to the tracer and the host` gives the bytes
of the device first to `frameTracer.onReceivedData()` and then to the client. This is exactly how
`sercom_cli` builds its trace in `host/src/CliApplication.cpp`.

## Step 3: Run the program

Run `build/sercom_example_22_trace_a_conversation`. You get:

```
[     1.000] >> #0     PING_REQUEST                 ACK_REQUEST                len   4  E8 03 00 00  CRC ok
[     1.002] << #0     PING_RESPONSE                ACK_REQUEST                len   8  E8 03 00 00 E9 03 00 00  CRC ok
[     1.002] << #0     PING_REQUEST                 ACK                        len   0  CRC ok
host received a ping response: round trip 2 ms, device uptime 1001 ms
[     1.002] >> #0     PING_RESPONSE                ACK                        len   0  CRC ok
[     1.003] >> #1     DEVICE_INFO_REQUEST          ACK_REQUEST                len   0  CRC ok
[     1.005] << #1     DEVICE_INFO_RESPONSE         ACK_REQUEST                len  25  01 00 EC 03 00 00 20 BF 02 00 0E 65 78 61 6D 70 ..  CRC ok
[     1.005] << #1     DEVICE_INFO_REQUEST          ACK                        len   0  CRC ok
host received a device info response: example-device, firmware 1.0
[     1.005] >> #1     DEVICE_INFO_RESPONSE         ACK                        len   0  CRC ok
```

Then run `echo $?` to see the exit code. It prints `0`.

## Step 4: Read one trace line

Take the first line:

- `[     1.000]`: the time of the mock clock in seconds.
- `>>`: host to device. `<<` is device to host.
- `#0`: the message counter.
- `PING_REQUEST`: the message type.
- `ACK_REQUEST`: the flags; the sender wants an acknowledgement.
- `len   4`: the payload length in bytes.
- `E8 03 00 00`: the payload, here the request time 1000 ms (`0x000003E8`), lowest byte first.
- `CRC ok`: the CRC32 at the end of the frame matches.

## Step 5: Understand the conversation

- The ping response carries `E8 03 00 00`, the request time copied back, and `E9 03 00 00`, the
  device uptime 1001 ms.
- The device sends its response before the ACK of the request, because it answers while its link
  is still handling the request. The ACK lines have the counter and the type of the frame they
  confirm and `len   0`.
- `>> #0 PING_RESPONSE ACK`: the host confirms the response, because the response asked for an
  acknowledgement.
- `host received ...` stands between the trace lines, because the tracer sees the bytes before the
  client handles them.
- The device info response has 25 payload bytes. The trace prints the first 16
  (`Settings::MAXIMUM_TRACED_PAYLOAD_SIZE`) and `..` for the rest. You can see the version `01 00`,
  the uptime `EC 03 00 00` (1004 ms), the free heap `20 BF 02 00` (180000 bytes), the name length
  `0E` (14) and the first characters of `example-device`.

## Try it yourself

1. Show text from the device. Before `// ping request must be sent` add:

   ```
       // pretend the device prints a boot line
       const std::string bootText = "booting\n";
       frameTracer.onReceivedData(reinterpret_cast<const uint8_t *>(bootText.data()), bootText.size());
   ```

   The first line of the output becomes `[     1.000] << unframed data, 8 bytes: "booting\n"`.
   This is how the boot text of an ESP32 shows up in a real trace.
2. Trace from the device side. Change `TraceDirectionType::HOST_TO_DEVICE` to
   `TraceDirectionType::DEVICE_TO_HOST`. Every arrow turns around, for example the first line
   starts with `[     1.000] << #0     PING_REQUEST`. The tracer only knows which way the bytes
   it sends travel, so the direction must match the side it sits on.
3. Switch the LED as well. Before `// host must have received a response to every request` add:

   ```
       // set LED request must be sent
       if (deviceClient.sendSetLedRequest(true, messageCounter) != Definitions::SendStatusType::SENT) {
           HostUtils::logWarning("Can not send the set LED request");
           return 1;
       }

       // hand the set LED request and its response over
       exchangeData(clock, hostTransport, deviceTransport, frameTracer, deviceClient, deviceApplication);
   ```

   Then change `requestCount` to `3`. Four more trace lines appear, for example
   `>> #2     SET_LED_REQUEST ... len   1  01  CRC ok`, and the simulated platform writes the log
   line `[     0.000] LED switched on` to standard error. The log line shows the real time since the
   first log line, not the mock clock, so it stands between trace lines stamped `1.006` and
   `1.008`.

## Where this is used

- `core/include/FrameTracer.h` and `core/src/FrameTracer.cpp`: `FrameTracer` and `TraceChannel`.
- `core/include/TraceTransport.h`: the transport that traces what it sends.
- `host/src/HostUtils.cpp`: `getTracedFrameLine()` and `getUnframedDataLine()`.
- `host/src/CliApplication.cpp` and `host/src/DeviceSimulator.cpp`: the `--trace` option.
- Lectures: L79 "The sercom_cli tool on Linux and Windows", L82 "Decoding captured traffic with
  sercom_dump" and L91 "Debugging protocol problems on the device".
