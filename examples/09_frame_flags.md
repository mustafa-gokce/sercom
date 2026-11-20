# 09 Frame Flags

This example builds the same frame with different flags and prints the FLAGS byte in hex and in
binary. Flags tell the receiver what to do with a frame: answer it, take it as an answer, or
treat it as a repeat. You need to read them in every byte dump.

## What you will learn

- Every flag is one bit of the FLAGS byte, and one frame can carry several flags.
- How to combine flags with `|` and how to test one flag with `Frame::getFlag()`.
- Where the FLAGS byte sits in a frame.

## Before you start

1. Watch lecture L29 "Version, flags, counter and type" (section S05).
2. Install the build tools once: `sudo apt install build-essential cmake ninja-build git` (README.md, section "Building the project").
3. Open a terminal and go to the source directory of sercom, the directory that holds `CMakeLists.txt`.
4. If you cloned the repository just now, fetch its submodules once: `git submodule update --init --recursive`
5. Configure the build: `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug`
6. Build every program: `cmake --build build`
7. The program of this example is `build/sercom_example_09_frame_flags`.

To build only this example, run `cmake --build build --target sercom_example_09_frame_flags`.

## Step 1: Read the flag values

Open `examples/09_frame_flags.cpp` and find the comment `// get the value of every flag` in
`main()`. `Definitions::fromFrameFlagType()` turns a flag into its number. Then
`// print the bit of every flag` calls `printFlagsLine()` once for every flag.

The flags are the enum `Definitions::FrameFlagType` in `core/include/Definitions.h`. A *bit* is
one binary digit, 0 or 1. A byte has 8 bits, numbered from 0 on the right to 7 on the left.
`ACK_REQUEST = 1 << 0` means "the number 1 moved 0 places to the left", that is bit 0.
`RETRANSMISSION = 1 << 3` is bit 3, the number 8.

Now read `printFlagsLine()`. It prints a byte twice: as hex with `HostUtils::getHexText()`, and
as binary with `std::bitset`. *Binary* means written with the digits 0 and 1 only, one digit
per bit. At the end of the line `HostUtils::getFrameFlagsText()` names the flags that are set.

## Step 2: Combine flags

Find the comment `// create one frame per flag combination that the link layer sends`. The
operator `|` (bitwise OR) sets every bit that is set in either number: `0x01 | 0x08` is `0x09`.

These are the combinations that the link layer sends:

- a request that wants an answer carries ACK_REQUEST,
- the same request sent again, because no answer came in time, carries ACK_REQUEST and RETRANSMISSION,
- the answer carries ACK (accepted) or NACK (rejected).

`createFrame()` builds a frame with a set LED request and the flags you give it. In the real
link layer an ACK frame has no payload and a NACK frame carries a status byte. Here every frame
carries the same LED request, so only the flags differ.

## Step 3: Read the FLAGS byte from the frame bytes

Find `// print the FLAGS byte of every frame behind the sync word and the version`. A frame
starts with the sync word (2 bytes) and the version (1 byte), so FLAGS is the byte at offset 3
(`flagsOffset`). The *offset* is the position of a byte, counted from 0. The example reads that
byte by hand from `getData()` to show where it sits; `frame.getFlags()`, which the last check
uses, returns the same byte.

Read `printFlagState()`. `frame.getFlag()` tests one bit. Inside, it uses the operator `&`
(bitwise AND), which keeps only the bits that are set in both numbers: `0x09 & 0x08` is `0x08`,
so RETRANSMISSION is set, and `0x09 & 0x02` is `0`, so ACK is not set.

## Step 4: Run the program

Run `build/sercom_example_09_frame_flags`. You get:

```
every flag is one bit of the FLAGS byte:
  bit 0                  0x01 = 00000001  ACK_REQUEST
  bit 1                  0x02 = 00000010  ACK
  bit 2                  0x04 = 00000100  NACK
  bit 3                  0x08 = 00001000  RETRANSMISSION
frames with different flags:
  plain message          0x00 = 00000000  NONE
  request                0x01 = 00000001  ACK_REQUEST
  retransmitted request  0x09 = 00001001  ACK_REQUEST|RETRANSMISSION
  acknowledgement        0x02 = 00000010  ACK
  rejection              0x04 = 00000100  NACK
flags of the retransmitted request one by one:
  ACK_REQUEST     set
  ACK             not set
  NACK            not set
  RETRANSMISSION  set
```

Then run `echo $?` to see the exit code. It prints `0`.

## Step 5: Understand the output

- `every flag is one bit of the FLAGS byte:` and the four lines below it: each flag has
  exactly one `1` in its binary text, at the place of its bit.
- `plain message 0x00 = 00000000 NONE`: no bit is set. The receiver does not answer this frame.
- `request 0x01 = 00000001 ACK_REQUEST`: bit 0 is set. The receiver must answer.
- `retransmitted request 0x09 = 00001001 ACK_REQUEST|RETRANSMISSION`: bits 0 and 3 are set.
  `0x09` is `0x01 + 0x08`, because each flag adds its own bit.
- `acknowledgement 0x02` and `rejection 0x04`: one answer flag each.
- `flags of the retransmitted request one by one:` and the four lines below it: the program
  asks `getFlag()` for every flag of `0x09`. Only ACK_REQUEST and RETRANSMISSION are set.

The exit code `0` means the retransmitted frame kept both of its flags.

## Try it yourself

1. Add a repeated rejection. After the line
   `printFlagsLine("rejection", rejectionFrame.getData()[flagsOffset]);` add
   `printFlagsLine("repeated rejection", static_cast<uint8_t>(nack | retransmission));`.
   A new line appears: `repeated rejection     0x0C = 00001100  NACK|RETRANSMISSION`.
2. Set a reserved bit (this breaks the program on purpose). Bits 4 to 7 are *reserved*: they
   have no meaning yet, senders must send 0 and receivers must ignore them. In `createFrame()`,
   replace `const Frame::Frame frame(flags,` with
   `const Frame::Frame frame(static_cast<uint8_t>(flags | 0x80),`. Every FLAGS byte now starts
   with a 1, for example `0x89 = 10001001  ACK_REQUEST|RETRANSMISSION`, but the names stay the
   same, because the reserved bit has no name. The last check sees `0x89` instead of `0x09`,
   prints a line ending in `WARNING The retransmitted request did not keep its flags` and the
   exit code is `1`.

## Where this is used

- `core/include/Definitions.h`: `FrameFlagType`, `fromFrameFlagType()` and `isFrameFlagSet()`.
- `core/include/Frame.h`: `Frame::getFlags()` and `Frame::getFlag()`.
- `host/include/HostUtils.h`: `getFrameFlagsText()`.
- `docs/PROTOCOL.md`, section "3. Flags".
- Lectures: L29 "Version, flags, counter and type" and L27 "Anatomy of a frame"; the flags come
  back in L67 "ACK and NACK frames" and L68 "Retransmission timers and the RETRANSMISSION flag".
