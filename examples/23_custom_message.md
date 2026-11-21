# 23 Custom Message

This example adds a message type of your own, an add numbers request with two numbers and an add
numbers response with their sum, and sends it over two links in memory. Nothing in the libraries
changes: the link layer carries any message type, and the meaning of a type lives in the
application on both ends.

## What you will learn

- How to choose a message type value that is free.
- How to write a payload with `ByteWriter` and read it back with `ByteReader`.
- How the receiver answers a bad request with a message status instead of a response.

## Before you start

1. Watch lectures L59 "Message types and enum converters" and L62 "Dispatching messages" (section S10), and run example 18 first.
2. Install the build tools and Qt 5.12.12 once: `sudo apt install build-essential cmake ninja-build git` and the aqtinstall steps of README.md, section "Building the project", which also has the steps for Windows.
3. Open a terminal and go to the source directory of sercom, the directory that holds `CMakeLists.txt`.
4. If you cloned the repository just now, fetch its submodules once: `git submodule update --init --recursive`
5. Configure the build: `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH=$HOME/Qt/5.12.12/gcc_64`, on Windows with `-DCMAKE_PREFIX_PATH=C:/Qt/5.12.12/mingw73_64`
6. Build every program: `cmake --build build`
7. The program of this example is `build/sercom_example_23_custom_message`.

To build only this example, run `cmake --build build --target sercom_example_23_custom_message`.

## Step 1: Choose the message type values

Open `examples/23_custom_message.cpp`. At the top is the enum `CustomMessageType` with
`ADD_NUMBERS_REQUEST = 0x0200` and `ADD_NUMBERS_RESPONSE = 0x0201`, followed by its two
*converters*, the functions that turn the enum into the number on the wire and back.

The message type field has 16 bits. `docs/PROTOCOL.md` divides it into ranges:

- `0x0000` is invalid and never sent,
- `0x0001` to `0x00FF` are *system messages* such as ping and device info,
- `0x0100` to `0xFFFF` are *application messages*; the demo protocol uses `0x0100` to `0x0103`.

So pick a value in the application range that nobody uses yet. Starting a new block at `0x0200`
leaves room for the demo protocol to grow, and a request and its response sit next to each other.
Find `// print the custom message types` in `main()`: the program prints both values first.

## Step 2: Read the device

Find the class `MockDevice`. It owns its link, like `DeviceApplication` does, so it can send a
response while it handles a request. Its constructor passes `*this` to the link before the device
is fully built; that is safe, because `Link` only keeps the reference and calls the handler later,
when data arrives. Read `onReceivedMessage()` from top to bottom. Each check is
a *guard*: it returns at once when something is wrong.

- `// message must be an add numbers request`: any other type gets `UNSUPPORTED_MESSAGE_TYPE`.
- `// payload must hold both numbers`: `ByteReader` reads two `int32_t` values; a short payload
  gets `MALFORMED_PAYLOAD`.
- `// add the numbers in 64 bits`: two `int32_t` values never overflow a 64 bit sum.
- `// add numbers response must be sent`: `ByteWriter` writes the sum into the 32 bits of its
  field, the link sends it.

The value it returns is the answer of the link: `ACCEPTED` becomes an ACK, any other status a
NACK. The extra bytes of a longer payload are ignored, so a newer host may add a field later.

## Step 3: Read the host

Find `MockHostHandler`. It keeps the sum of an add numbers response and prints a failed request.
In `main()`, `// write both numbers into the payload of the request` writes 40 and 2 with
`ByteWriter`, and `// add numbers request must be sent` sends them with an acknowledgement
request. `exchangeData()` passes the bytes between both sides until no bytes wait.

In the library a new message becomes a pair of classes in `app/include/Messages.h` and
`app/src/Messages.cpp`, like `PingRequest` and `PingResponse`. The example writes the payload with
`ByteWriter`, because an example file holds no classes besides its mocks.

## Step 4: Run the program

Run `build/sercom_example_23_custom_message`. You get:

```
custom message types: ADD_NUMBERS_REQUEST 0x0200, ADD_NUMBERS_RESPONSE 0x0201
host sends ADD_NUMBERS_REQUEST with payload 28 00 00 00 02 00 00 00
device adds 40 and 2
host received the sum 42
```

Then run `echo $?` to see the exit code. It prints `0`.

## Step 5: Understand the output

- `custom message types`: the two values of step 1.
- `payload 28 00 00 00 02 00 00 00`: 40 is `0x28` and 2 is `0x02`, each in four bytes, lowest
  byte first.
- `device adds 40 and 2`: the device read both numbers.
- `host received the sum 42`: the response came back and the host read the sum.

The exit code `0` means the host received the right sum.

## Try it yourself

1. Make the sum too big. Set `firstNumber` to `2147483647`, the largest `int32_t`. The sum
   2147483649 does not fit into the 32 bits of its field, the device sends only its lowest 32
   bits, the host prints `host received the sum -2147483647`, then
   `WARNING The host did not receive the sum of both numbers`, and the exit code is `1`. Now let
   the device reject such a request: add `#include <limits>` below `#include <cstdint>` and replace
   the block `// add the numbers in 64 bits` with this guard:

   ```
           // sum must fit into its field
           const int64_t sum = static_cast<int64_t>(firstNumber) + secondNumber;
           if (sum < std::numeric_limits<int32_t>::min() || sum > std::numeric_limits<int32_t>::max()) {
               return Definitions::MessageStatusType::INVALID_ARGUMENT;
           }
   ```

   The device answers `INVALID_ARGUMENT`, and the host prints
   `host request failed: REJECTED, INVALID_ARGUMENT` and the same warning.
2. Send a short payload. Delete the line `byteWriter.writeInt32(secondNumber);`. The payload is
   `28 00 00 00`, the host prints `host request failed: REJECTED, MALFORMED_PAYLOAD` and the exit
   code is `1`.
3. Take a used value (this breaks the program on purpose). Change `ADD_NUMBERS_REQUEST = 0x0200,`
   to `ADD_NUMBERS_REQUEST = 0x0100,`, the value of `SET_LED_REQUEST`. The example still prints
   `host received the sum 42`, because both ends are mocks of this file, but a device running the
   demo firmware would switch its LED instead of adding numbers. Let the program catch it: add
   `#include "../app/include/Messages.h"` above `#include "../core/include/ByteCodec.h"` and this
   guard above `// join the host and the device in memory`:

   ```
       // custom message type must be unknown to the demo protocol
       if (Messages::toMessageType(requestMessageType) != Messages::MessageType::UNKNOWN) {
           HostUtils::logWarning("The custom message type is taken by the demo protocol");
           return 1;
       }
   ```

   `Messages::toMessageType()` of the library returns `UNKNOWN` for every value it does not know.
   The program now stops with `WARNING The custom message type is taken by the demo protocol` and
   exit code `1`.

## Where this is used

- `app/include/Messages.h`: the demo `MessageType` values and `toMessageType()`.
- `app/src/DeviceApplication.cpp`: the dispatch of the demo device, which answers an unknown type
  with `UNSUPPORTED_MESSAGE_TYPE`.
- `core/include/ByteCodec.h`: `ByteWriter` and `ByteReader`.
- `docs/PROTOCOL.md`: section 6, the message type ranges.
- In your own project, write `AddNumbersRequest` and `AddNumbersResponse` classes like the ones in
  `app/include/Messages.h` and `app/src/Messages.cpp`.
- Lectures: L59 "Message types and enum converters", L61 "Evolving messages without breaking
  peers", L62 "Dispatching messages" and L63 "Validating payloads".
