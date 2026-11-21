# 16 Device Info Message

This example builds a device info response with a firmware version, an uptime, a free heap size
and a device name, prints its payload field by field and reads it back from a frame. The host
asks for this message to learn which device and which firmware is on the other end of the cable.

## What you will learn

- How fixed-size fields and a text field share one payload.
- How a text travels as a length byte followed by its characters.
- How the receiving side reads every field back with the getters of the message.

## Before you start

1. Watch lecture L63 "Message classes on GenericMessage" (section S10). In the code a message class writes its bytes in its field constructor and reads them in its frame constructor (`Messages::DeviceInfoResponse`), there is no `serialize()` function.
2. Install the build tools and Qt 5.12.12 once: `sudo apt install build-essential cmake ninja-build git` and the aqtinstall steps of README.md, section "Building the project", which also has the steps for Windows.
3. Open a terminal and go to the source directory of sercom, the directory that holds `CMakeLists.txt`.
4. If you cloned the repository just now, fetch its submodules once: `git submodule update --init --recursive`
5. Configure the build: `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH=$HOME/Qt/5.12.12/gcc_64`, on Windows with `-DCMAKE_PREFIX_PATH=C:/Qt/5.12.12/mingw73_64`
6. Build every program: `cmake --build build`
7. The program of this example is `build/sercom_example_16_device_info_message`.

To build only this example, run `cmake --build build --target sercom_example_16_device_info_message`.

## Step 1: Read how the response is built

Open `examples/16_device_info_message.cpp` and find the comment
`// create the device info response of a device that runs for one and a half minutes` in
`main()`. The five values are:

- the firmware version 1.2, as a major number 1 and a minor number 2,
- the uptime, the time since the device started, 90 seconds,
- the free heap size, the memory the device has left, 180 * 1024 bytes,
- the device name, `HostSettings::DEFAULT_DEVICE_NAME`.

Find `// response must hold every field and the device name`. A name above 31 characters
(`Messages::MAXIMUM_DEVICE_NAME_LENGTH`) leaves the response empty, and `getLikelyValid()`
returns false.

## Step 2: Read the field by field print

Find `// print every field after the fields in front of it`. Each field starts where the field
in front of it ends, so `offset` grows by the size of each field. `sizeof(uint8_t)` is 1 byte,
`sizeof(uint32_t)` is 4 bytes. `printField()` prints the name, the offset, the size, the bytes
and the value that the getter of the response returns.

The device name is a *string field*: one length byte, then the characters, without a 0 at the
end. The length byte tells the receiver where the name ends.

## Step 3: Read the host side

Find `// put the response into a frame and read it back like the host`. The host builds a new
`Messages::DeviceInfoResponse` from the frame and reads every field with its getter. The last
check compares every field with the value that went in.

## Step 4: Run the program

Run `build/sercom_example_16_device_info_message`. You get:

```
payload of 28 bytes: 01 02 90 5F 01 00 00 D0 02 00 11 73 65 72 63 6F 6D 2D 64 65 76 69 63 65 2D 73 69 6D
FIRMWARE_VERSION_MAJOR offset  0 size  1 : 01 = 1
FIRMWARE_VERSION_MINOR offset  1 size  1 : 02 = 2
UPTIME                 offset  2 size  4 : 90 5F 01 00 = 90000 ms
FREE_HEAP_SIZE         offset  6 size  4 : 00 D0 02 00 = 184320 bytes
DEVICE_NAME_LENGTH     offset 10 size  1 : 11 = 17
DEVICE_NAME            offset 11 size 17 : 73 65 72 63 6F 6D 2D 64 65 76 69 63 65 2D 73 69 6D = "sercom-device-sim"
host reads firmware version 1.2, device name sercom-device-sim
host reads uptime 90000 ms, free heap size 184320 bytes
```

Then run `echo $?` to see the exit code. It prints `0`.

## Step 5: Understand the output

- `payload of 28 bytes`: 11 bytes of fixed fields plus the 17 characters of the name.
- `FIRMWARE_VERSION_MAJOR` and `FIRMWARE_VERSION_MINOR`: one byte each, `01` and `02`.
- `UPTIME ... 90 5F 01 00 = 90000 ms`: 90000 is `0x00015F90`, sent lowest byte first.
- `FREE_HEAP_SIZE ... 00 D0 02 00 = 184320 bytes`: 180 * 1024 is `0x0002D000`.
- `DEVICE_NAME_LENGTH ... 11 = 17`: `0x11` is 17, the number of characters.
- `DEVICE_NAME ... = "sercom-device-sim"`: the characters as ASCII codes, `73` is `s`, `65` is `e`.
- The two `host reads` lines: the host got every field back from the frame.

The exit code `0` means every field arrived unchanged.

## Try it yourself

1. Use a name of your own. Set `deviceName` to `"my-esp32"`. The payload has 19 bytes, the length
   byte is `08` and the last line of the field list is
   `DEVICE_NAME            offset 11 size  8 : 6D 79 2D 65 73 70 33 32 = "my-esp32"`.
2. Change the free heap size. Set `freeHeapSize` to `64 * 1024`. The field becomes
   `00 00 01 00 = 65536 bytes`.
3. Use a name that is too long (this breaks the program on purpose). Set `deviceName` to
   `"a-device-name-that-is-far-too-long"`, which has 34 characters. The response stays empty,
   the program prints a line ending in `WARNING Can not build the device info response` and the
   exit code is `1`.

## Where this is used

- `app/include/Messages.h` and `app/src/Messages.cpp`: `DeviceInfoResponse` and
  `MAXIMUM_DEVICE_NAME_LENGTH`.
- `core/src/ByteCodec.cpp`: `ByteWriter::writeString()` writes the length byte and the
  characters.
- `docs/PROTOCOL.md`: the message table with `DEVICE_INFO_RESPONSE`.
- Lectures: L63 "Message classes on GenericMessage", L64 "Evolving messages without breaking
  peers" and L66 "Validating payloads"; the `info` command is in L79 "The sercom_cli tool on
  Linux and Windows".
