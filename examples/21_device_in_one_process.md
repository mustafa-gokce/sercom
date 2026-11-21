# 21 Device in One Process

This example runs the real device application and the real host client in one program, joined in
memory. The host sends a ping and switches the LED on, with no serial port and no board. It shows
that the protocol logic of the device does not depend on the hardware.

## What you will learn

- How `DeviceApplication` (the device side) and `DeviceClient` (the host side) talk to each other.
- What the `DevicePlatform` interface is: the only place where the device touches its hardware.
- How a mock clock makes the round trip time and the uptime the same on every run.

## Before you start

1. Watch lecture L81 "A device simulator: firmware logic without hardware" (section S12) and run example 18 first.
2. Install the build tools and Qt 5.12.12 once: `sudo apt install build-essential cmake ninja-build git` and the aqtinstall steps of README.md, section "Building the project", which also has the steps for Windows.
3. Open a terminal and go to the source directory of sercom, the directory that holds `CMakeLists.txt`.
4. If you cloned the repository just now, fetch its submodules once: `git submodule update --init --recursive`
5. Configure the build: `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH=$HOME/Qt/5.12.12/gcc_64`, on Windows with `-DCMAKE_PREFIX_PATH=C:/Qt/5.12.12/mingw73_64`
6. Build every program: `cmake --build build`
7. The program of this example is `build/sercom_example_21_device_in_one_process`.

To build only this example, run `cmake --build build --target sercom_example_21_device_in_one_process`.

## Step 1: Read the helpers

Open `examples/21_device_in_one_process.cpp`. There are four small helpers, called *mocks*
because they stand in for real parts:

- `MockClock`: a clock that moves only when the program moves it.
- `MockTransport`: keeps the sent bytes in memory until `receiveData()` takes them.
- `MockDevicePlatform`: the "hardware" of the device. It has an LED that prints when it switches,
  a fixed free heap size, a fixed temperature and the name `example-device`. On the ESP32 the same
  interface reads the real heap and switches the real LED.
- `MockClientHandler`: the host application. It prints the ping and the set LED response and
  every request that failed. The interface asks for a callback per message, so the class ends
  with a second `public:` section of empty callbacks for the messages this example does not ask
  for: skip the empty callbacks.

## Step 2: Read how the two sides are joined

Find `// join the device and the host in memory` in `main()`. The device application writes into
`deviceTransport`, the device client writes into `hostTransport`. Find
`// let the device run for one second`: the device started at 0 ms and the host starts at
1000 ms, so the device uptime is about one second.

Now read `exchangeData()` at the end of the file. It takes the waiting bytes of one side and
gives them to the other side, in both directions, until no bytes wait any more. Each hand over
moves the clock by 1 ms, the time the bytes spend on the wire in this example.

## Step 3: Read the two requests

Find `// ping request must be sent` and `// set LED request must be sent`. Each request goes out
with one call of the device client, and each `exchangeData()` call lets the device answer it.
`DeviceClient` sends every request with an acknowledgement request, and `DeviceApplication`
answers with a response that also asks for an acknowledgement. The link layer of example 18 does
that work under both of them.

At the end, `// host must have received a response to every request` checks that the host link
delivered two messages, and `// LED of the device must be on` asks the mock platform. Example 22
adds the device info request and shows every frame of the conversation.

## Step 4: Run the program

Run `build/sercom_example_21_device_in_one_process`. You get:

```
host sends a ping request
host received a ping response: round trip 2 ms, device uptime 1001 ms
host sends a set LED request
  device platform switches the LED on
host received a set LED response: LED on
```

Then run `echo $?` to see the exit code. It prints `0`.

## Step 5: Understand the output

- `round trip 2 ms`: the *round trip time* is the time from sending the ping request to receiving
  its response, 1 ms there and 1 ms back. `DeviceClient` measures it from the time stamp that the
  device copies from the request into the response.
- `device uptime 1001 ms`: the device got the request at 1001 ms, one second plus 1 ms on the
  wire.
- `device platform switches the LED on`: `DeviceApplication` called `setLedOn(true)` on the
  platform. On a board this line would light the LED.
- `LED on`: the set LED response reports what the platform says after switching.

The exit code `0` means two responses arrived and the LED is on.

## Try it yourself

1. Switch the LED off. Change `sendSetLedRequest(true, messageCounter)` to
   `sendSetLedRequest(false, messageCounter)`. The platform prints
   `device platform switches the LED off`, the response says `LED off`, and the program ends with
   `WARNING The LED of the device is not on` and exit code `1`.
2. Make the wire slower. Change `wireDelay = 1ms;` in `exchangeData()` to `wireDelay = 10ms;`.
   The ping line becomes `round trip 20 ms, device uptime 1010 ms`.
3. Ask for something the device refuses. Before `// host must have received a response to every request`
   add a set telemetry period request of 5 ms, which is below
   `AppSettings::MINIMUM_TELEMETRY_PERIOD_MSEC` (20 ms):

   ```
       // print the set telemetry period request
       HostUtils::printLine("host sends a set telemetry period request");

       // set telemetry period request must be sent
       static constexpr std::chrono::milliseconds telemetryPeriod = 5ms;
       if (deviceClient.sendSetTelemetryPeriodRequest(telemetryPeriod, messageCounter)
           != Definitions::SendStatusType::SENT) {
           HostUtils::logWarning("Can not send the set telemetry period request");
           return 1;
       }

       // hand the set telemetry period request and its answer over
       exchangeData(clock, hostTransport, deviceTransport, deviceClient, deviceApplication);
   ```

   The device answers with a NACK and the handler prints
   `host request #2 SET_TELEMETRY_PERIOD_REQUEST failed: REJECTED, INVALID_ARGUMENT`. The exit
   code stays `0`, because a NACK delivers no message to the host.

## Where this is used

- `app/include/DeviceApplication.h` and `app/src/DeviceApplication.cpp`: the device side and the
  `DevicePlatform` interface.
- `app/include/DeviceClient.h` and `app/src/DeviceClient.cpp`: the host side and the round trip
  time.
- `host/src/SimulatedPlatform.cpp`: the platform of the device simulator, and
  `firmware/esp32/src/Esp32Platform.cpp`: the platform of the ESP32.
- `tests/test_DeviceApplication.cpp`: the same pair in memory, with many more cases.
- Lectures: L65 "Dispatching messages", L78 "DeviceClient: requests and replies" and L81 "A device
  simulator: firmware logic without hardware".
