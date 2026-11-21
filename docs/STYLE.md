# Coding Style

This document describes the coding and commenting style of sercom. Every C++ file in
`core/`, `app/`, `host/`, `firmware/common/`, `firmware/esp32/`, `firmware/stm32f4/`,
`tests/`, `examples/` and `fuzz/` follows it, and so does every change. The code is plain
C++17 with the standard library; the ESP32 firmware adds the Arduino and FreeRTOS calls
it needs, the STM32F4 firmware the CMSIS registers and intrinsics, and nothing else.

Three tools check what a program can count:

1. `tools/analysis/check_format.sh` compares every C++ file with the output of
   clang-format 18 and `tools/analysis/clang-format.yaml`.
2. `python3 tools/analysis/check_style.py` checks the comments and the countable idioms.
   Every finding names the rule number used below, for example `R-084`.
3. `tools/run_static_analysis.sh build` runs both of them, cppcheck 2.22 and clang-tidy 18.

The other rules are checked by reading. When a case is not covered here, copy what
`core/` does.

## 1. Layout

- A `.h` or `.cpp` file is exactly the output of
  `clang-format --style=file:tools/analysis/clang-format.yaml -i <file>` with clang-format 18:
  four spaces, 100 columns, braces on their own line only for classes, structs and
  functions, one argument per line once a call or a declaration wraps (R-001).
- `*` and `&` bind to the name: `const uint8_t *data`, `Transport::Transport &transport` (R-002).
- One blank line between top-level items, never two; no blank line right after `{` or
  right before `}` inside a body; one blank line inside the namespace braces (R-003).
- Files are committed with LF line ends (R-004).

Format first, then build: the formatter lays out broken code too.

## 2. Files and includes

- Each library keeps `include/` and `src/` side by side: `core/include/Frame.h` pairs with
  `core/src/Frame.cpp`. Every header has its source file; directories are lowercase, files
  are named after their class in PascalCase (R-005, R-006).
- The guard is `SERCOM_<STEM>_H`, the stem in upper snake case, closed with its name.
  Nothing comes before it, no file comment and no `#pragma once` (R-007, R-008).
- A source file opens with its own header alone, followed by a blank line (R-009).
- Includes form groups in this order, one blank line between them: the standard library,
  the platform (inside its `#if defined(...)` together with the standard headers only that
  platform needs), third-party code, the other libraries of sercom, the own library. Each
  group is sorted (R-010, R-011).
- sercom headers are included by a path relative to the including file, across libraries
  too; CMake sets no include directory (R-012).

```cpp
#include "../include/SerialPort.h"

#if defined(__linux__)
#include <cerrno>
#include <fcntl.h>
#include <poll.h>
#include <termios.h>
#include <unistd.h>
#endif

#include "../include/HostSettings.h"
```

- Conditional compilation tests definedness with `#if defined(X)` or `#if not defined(X)`
  and values with a plain `#if`; `#ifndef` appears only in the guard (R-019).
- A header looks the same in every build: no `#if` on a build option inside it (R-022).

## 3. Namespaces and program files

- A library file lives in `namespace sercom::<FileStem> {` and closes with
  `} // namespace sercom::<FileStem>`; the contents are not indented and the main class is
  named like the file, so a call reads `Frame::Frame`, `Definitions::DecodeErrorType` (R-014).
- A forward declaration is used only where the other header would include this one (R-015).
- `using namespace std::chrono_literals;` is the only using-directive in a header, and only
  the settings headers have it; a function may open with `using namespace sercom;`. There
  are no `using X::Y;` declarations (R-016).
- The `.cpp` defines the members in the order the header declares them (R-017).
- File-local helpers of a library source sit in one unnamed `namespace {` right after the
  file namespace opens, each under its block comment (R-018).
- A source outside `tests/` defines no class or struct, except the small `Mock<Thing>`
  doubles that an example or a fuzz target keeps in its one file; program classes are
  header and source pairs in `host/` (R-250).
- A program file declares its functions first, each under a one-sentence block comment,
  `main` included, and takes `char *argv[]` (R-251).

```cpp
/*
 * Main entry point of the executable.
 */
int main(int argc, char *argv[]);
```

## 4. Declarations

- A header declares; every body lives in the `.cpp`. `= default` and `= delete` are the
  only bodies a header holds (R-020).
- Library code has no templates, no typedefs and no constexpr functions (R-021). The CRC32
  lookup table is built once at run time by a lambda (R-137).
- Every function, enum and struct, and the first constructor of a class, has a block
  comment directly above it. Overloads and further constructors share the first one (R-023).
- `[[nodiscard]]` comes first on every getter and helper that returns a value; never on a
  `void` function and not on an enum converter (R-024).
- A by-value parameter has no `const` in the declaration and has it in the definition (R-025).
- Class types are passed as `const T &`; scalars, enums, `std::string_view` and chrono
  durations by value. There is no `std::move` sink and no `const_cast` (R-026).
- A pointer never carries a top-level `const`: `const uint8_t *data`, never
  `uint8_t *const data` (R-027).
- A second result goes through a non-const reference parameter; a function that can fail
  returns a `bool` or a status enum. Pointer parameters are for objects and byte buffers only,
  never for an optional output (R-028).
- Every parameter has a name; a setter's parameter is named after its property (R-029).
- A default argument lives in the declaration as an explicit empty value (`= 0`,
  `= LinkConfiguration()`); an empty argument at a call site is `std::vector<uint8_t>()` or
  `std::string()`, never `{}` (R-030).
- A header holds, in this order: constants, each enum with its converters, free functions,
  structs, then classes with a base before the classes derived from it (R-031).
- A setter follows its getter, each with its comment; free functions are plain
  declarations without `static`, `inline` or `extern` (R-032).
- `virtual` stands in the base class only. A redefinition says `final` in a leaf class and
  `override` only where other classes still derive (R-033).

```cpp
/*
 * Listen for a received frame.
 */
void onReceivedFrame(const Frame::Frame &frame) final;
```

## 5. Classes and members

- No comment stands above `class X`. The block comment of the first constructor describes
  the class; the first `public:` section holds the constructors and the destructor (R-034,
  R-035).
- The copy and move members follow as two commented pairs, `delete` in every class that is
  not a value class (interfaces, handlers and classes that own or reference something) and
  `default` in the value classes, `Frame` and the messages (R-036).
- Every constructor with parameters is `explicit`; a constructor without parameters is
  never `explicit`: `X() = default;` when it sets nothing, otherwise `X();` with its
  initializer list in the `.cpp` (R-037).
- The destructor follows the constructors without a comment. A non-copyable class defines
  it in the `.cpp` (`Link::~Link() {}`); it says `final` when the base destructor is virtual
  (`~Link() final;`). A destructor with work calls the teardown under
  `// invoke clean up procedure` before `this->cleanUp();`, or under `// close the device`
  before `this->close();` in a class with an open and close interface (R-038).
- Every class that nothing derives from is `final` (R-039).

```cpp
class Link final : public FrameDecoder::FrameDecoderHandler
{
public:
    /*
     * Link layer with message counters, acknowledgements, retransmissions and duplicate detection.
     */
    explicit Link(Transport::Transport &transport,
                  LinkHandler &handler,
                  const Clock::Clock &clock,
                  const LinkConfiguration &configuration = LinkConfiguration());
    ~Link() final;

    // delete copy constructor and copy assignment operator
    Link(const Link &) = delete;
    Link &operator=(const Link &) = delete;

    // delete move constructor and move assignment operator
    Link(Link &&) = delete;
    Link &operator=(Link &&) = delete;
```

- Access sections repeat per group of members; the data members form the last section
  of their own (R-040).
- Every data member has an aligned trailing lowercase comment without a full stop. A
  reference to the owner says `// reference to <owner>`, a reference to a service it uses
  `// reference for <service>` (R-041).

```cpp
private:
    Transport::Transport &transport;         // reference for transport
    LinkHandler &handler;                    // reference to handler
    const Clock::Clock &clock;               // reference for clock
    const LinkConfiguration configuration;   // link configuration
```

- A member fixed at construction is `const` and set in the initializer list; a class has
  no default member initializers (R-042).
- The initializer list names one member per line, `: a(...)` first and `, b(...)` after,
  with parentheses; every scalar and enum is set, an array is zeroed with `, buffer()`, and
  the body is `{}` (R-043).
- An object created once is a plain member. A `std::unique_ptr` holds only an object that
  is created again at run time or released in order by the destructor; it is created with
  `this->x.reset(new X(...));` under a comment and released with `this->x.reset(nullptr);`,
  never with `std::make_unique` or `.reset()` (R-044, R-242).
- Only a lock is `mutable` (R-045).
- A struct is a commented aggregate at namespace scope with commented fields, every scalar
  initialized, no member functions, declared before the class that uses it (R-046, R-047).
- Every member and own method is reached through `this->` (R-048).
- A new member, flag or wire field comes only when the existing state cannot express the
  case; shared helpers are called, not copied (R-049).
- A hand-written copy assignment checks for itself, copies the members in declaration
  order and returns `*this` (R-246).

## 6. Comments

- A block comment opens with `/*`, holds one complete sentence per line as ` * Sentence.`
  and closes with ` */`; no empty ` *` lines, lists or tags (R-050).
- A `//` comment is one line; two `//` lines never follow each other (R-051).
- A declaration comment starts with the verb of its kind: `Get ...`, `Get whether ...`,
  `Set ...`, `Listen for ...`, `Send ...`, `Maintain ...`, `Convert ... to ...`, and
  `Find ...` for a lookup by key. An enum converter says `<Enum in words> to string.` and
  `String to <enum in words>.`, or `<Enum in words> to integer.` and
  `Integer to <enum in words>.` for a wire value; a text helper for display says
  `Get <enum in words> as text.` and sits next to its enum when the firmware console
  prints the text too (`Messages::getMessageTypeText`, `Definitions::getFrameFlagTypeText`),
  in `HostUtils` when only the host tools print it (`getMessageStatusTypeText`)
  (R-052, R-121).
- Destructors, special members, overloads, includes, namespace lines, access specifiers
  and aliases carry no comment of their own. The free-standing `//` lines of a header are
  the special member pairs, the forward declarations, a group heading inside a large enum
  (`// system messages` in `app/include/Messages.h`) and `// check ...` above the
  `static_assert` lines (R-053, R-054).
- Every block of a function body opens with one lowercase `//` line that says what the
  block does: every block after a blank line, the first block of every body (a body of one
  block included) and every one-statement accessor (`// expose <thing>`, `// set <thing>`).
  Only the bodies of converters and of a hand-written copy assignment stay bare (R-055,
  R-056, R-122, R-246).
- The comment of a short conditional stands above the `if`, not inside its block (R-057).
- A body comment is lowercase, short, without closing punctuation or a comma clause, and
  says what, not why (R-058).
- Comments are imperative; a guard states its requirement (`// payload must fit into a
  frame of this build`) or names its check (`// check for an empty write`). Fixed phrases:
  `// notify the user`, `// warn the user`, `// expose <thing>`,
  `// notify listeners about <thing>`, `// keep track of <things>`,
  `// do not proceed further`, `// iterate over ...`, and `// <thing> <done> successfully`
  (`// frame sent successfully`) above a final `return true;` (R-059).
- A comment line follows a blank line or opens a block; it never follows code and is never
  followed by a blank line (R-060).
- `// NOTE: <lowercase clause>` marks a real trap and stays rare; there is no TODO or
  FIXME (R-061).
- More than one sentence inside a body is a block comment. Statements carry no trailing
  comments (R-062, R-063).
- A comment changes together with its code; it says "directory", never "folder" (R-064).
- A platform limit is noted in one line where the code depends on it (R-065).
- Analyzer findings are fixed in the code; there are no suppressions beyond a bare trailing
  `// NOLINT` (R-066).

```cpp
Frame::Frame(const uint8_t *data, const size_t size)
    : frame()
    , frameSize(0)
{
    // data must exist
    if (data == nullptr) {
        return;
    }

    // data must fit into a frame of this build
    if (size > this->frame.size()) {
        return;
    }

    // copy the received bytes
    std::copy_n(data, size, this->frame.begin());
    this->frameSize = size;
}
```

## 7. Function bodies

- Guards come first, one requirement each under its comment, with an early return and no
  `else` after them; a computed value and its check share one block (R-067, R-068).
- A commented branch is split as `}`, blank line, `// comment`, `else if (...) {`; a short
  two-way choice without comments is `} else {` (R-069).
- Every local that is not a pointer and does not change is `const` (R-070).
- One declaration per line, `=` for values and `()` for constructor arguments, scalars
  initialized, no `T x{}` (R-071).
- A member is used directly; a snapshot gets its own name, `current<X>` for a value or a
  time and `my<X>` for a shared object (R-072).
- Casts are `static_cast`; `reinterpret_cast` only views a byte buffer as characters and
  back (R-073).
- Integer literals carry no suffix, except `U` on CRC register constants; hex digits are
  upper case; no digit separators (R-074).
- Pointers are compared with `nullptr`, emptiness is `.empty()`, a bool is tested bare and
  an integer against an explicit value (R-075).
- A ternary only picks a value; it never acts and never nests (R-076).
- An empty value is returned as `return {};`; success is a `bool` (R-077).
- A result kept on purpose but not used is `[[maybe_unused]] const T name = call();` under a
  comment; an unused parameter is `[[maybe_unused]]` in the definition and keeps its name;
  never a cast to `void` (R-078, R-079).
- Increments and decrements are prefix: `++index`, never `index++` or `+= 1` (R-080).
- Every `case X: {` is braced with its `break;` inside; `case X::UNKNOWN:` stands on top of a
  braced `default: {` (R-081).
- No magic numbers: a value comes from a settings or definitions constant or from a named
  function-local `static constexpr` (R-082).
- Containers and `std::string`, no local C arrays with C string functions (R-083).
- A factory reads every field into `const` locals first, builds once, then exposes the
  result (R-244); a step that needs locals of its own is a bare `{ ... }` block under
  `// <step>` (R-247).
- A search result is checked by a not-found guard before it is exposed (R-249).

```cpp
// received message must be found
const std::array<ReceivedMessage, Settings::DefaultDuplicateHistorySize>::const_iterator &iterator
    = std::find_if(this->receivedMessages.cbegin(),
                   this->receivedMessages.cend(),
                   [messageCounter, messageType](const ReceivedMessage &receivedMessage) {
                       return receivedMessage.used
                              && receivedMessage.messageCounter == messageCounter
                              && receivedMessage.messageType == messageType;
                   });
if (iterator == this->receivedMessages.cend()) {
    return nullptr;
}

// expose the received message
return &*iterator;
```

## 8. Loops and algorithms

- There is no range-based `for`. The algorithm says what the loop does: `std::for_each`,
  `std::any_of`, `std::all_of`, `std::find_if`, `std::count_if`, `std::accumulate`,
  `std::transform`. A `continue` becomes `return;` inside the lambda, a `break` becomes
  `std::find_if` or `std::any_of` (R-084).
- An index `for` stays where the index is the point (bit steps, table builds); a `while`
  drives input and parse loops with an explicit advance; no loop has an empty body (R-085,
  R-086).
- Read-only algorithms take `cbegin()` and `cend()`, `begin()` and `end()` only when they
  write (R-087).
- `std::accumulate` starts with a value of the result type (R-088).
- A lambda names its captures (never `[=]` or `[&]`), always has `()`, takes class types by
  const reference and names its parameter after the element; a stored lambda is
  `const auto name = ...` (R-089).
- `std::min`, `std::max` and `std::clamp` with one type, never a hand-written ternary (R-090).
- The result of an algorithm has its iterator type spelled out; `auto` holds only lambdas
  (R-091).

## 9. Names and types

- Names are full words: `message`, `buffer`, `length`, `index`, `receive`; never `msg`,
  `buf`, `len`, `idx`, `rx`, `tx`, and no member prefixes (R-092).
- Limits are `MAXIMUM_`/`MINIMUM_` and `maximum`/`minimum`, never `MAX` or `MIN` (R-093).
- Acronyms: `CRC32`, `USB` and `HTTP` stay upper case; `Id`, `Led`, `Uart` are words; a
  lowerCamel name starts all lower case (`crc`) (R-094).
- A converter's parameter is named after the type it converts,
  `toMessageStatusType(uint8_t messageStatusType)`, never `raw...` (R-095).
- The unit lives in the chrono type, the constant suffix or the comment, never in a
  camelCase name: `retransmitTimeout`, not `retransmitTimeoutMs` (R-096).
- Containers are plural, numbers of things end in `Count`; a search result is `iterator`,
  a log text `debugMessage` or `warningMessage` (R-097, R-098).
- A boolean member is an adjective or participle (`opened`, `used`); accessors are
  `getX() const` and `setX(...)`, booleans too (`getOpened()`); `is` only names a function
  that tests its argument (R-099, R-100).
- A function that writes to the wire is `sendX`; housekeeping is `maintain()`, teardown
  `cleanUp()`; message classes are `<Action>Request` and `<Action>Response` (R-101).
- A notification is answered by `onReceivedX` on a handler interface; a keyed lookup is
  `get<Thing>With<Key>()` and returns `nullptr` when nothing is found (R-102, R-103).
- Fixed-width integers (`uint8_t` to `uint64_t`), sizes and indexes `size_t`, never
  `unsigned`, `long` or `short` (R-104, R-105).
- Durations and time points are `std::chrono::milliseconds` with chrono literals;
  `.count()` appears only where an API needs a number (R-106).
- Text is `std::string` on the host; an enum converter returns `std::string_view` built with
  `return {"NAME"};` (R-107).
- A third-party unscoped enumerator is qualified with its enum (R-243); a local holding a
  converted form is `<thing>As<Form>` (R-245).

## 10. Constants and build values

- A settings header holds one declaration per line, each with a trailing lowercase comment
  that names the thing and its unit, and no other comment lines (R-108).
- A literal constant is `constexpr UPPER_SNAKE` at namespace scope without `static`. A value
  that comes from the build is read once into a PascalCase constant (R-109, R-110, R-115).

```cpp
constexpr size_t MaximumPayloadSize = MAXIMUM_PAYLOAD_SIZE;     // maximum payload size in bytes
constexpr size_t MaximumPendingFrames = MAXIMUM_PENDING_FRAMES; // maximum unacknowledged frames
```

- A duration constant is `constexpr std::chrono::milliseconds NAME_MSEC = 250ms;` with
  `_TIMEOUT_MSEC`, `_PERIOD_MSEC` or `_DELAY_MSEC`; seconds are written as `5 * 1000ms`
  (R-111).
- A constant that follows from another is written by name (`= HEADER_SIZE + CRC_SIZE`) and
  has the type its consumer takes (R-112, R-113).
- A source file has no constants at file scope; a constant of one function is a
  function-local `static constexpr` in camelCase (R-114).
- `static_assert` checks follow the namespace at the end of the header under
  `// check settings` or `// check definitions`, fully qualified, with the message
  `"<Thing> must be <value>"` (R-154).

## 11. Enums and converters

- Every enum is `enum class Name : uint8_t` (wider only when needed) and its name ends in
  `Type`, or in `Key` for a key table (R-116, R-117).
- A value enum has a block comment naming the type, `UNKNOWN = 0` first, every value
  numbered with a trailing lowercase comment and a trailing comma. A flag family is its own
  enum with `1 << n` values (R-118, R-119, R-120).
- After each enum come `from<Enum>` and `to<Enum>`, each with its block comment, the
  parameter named after the type (R-121).
- `from` is a braced switch with `case X::UNKNOWN:` stacked on `default:`; `to` is an `if`
  chain that ends in `UNKNOWN`; there are no comments inside a converter. An integer
  converter is a `static_cast` pair that validates wire values (R-122, R-123).

```cpp
std::string_view fromDecodeErrorType(const DecodeErrorType decodeErrorType)
{
    switch (decodeErrorType) {
    case DecodeErrorType::INVALID_VERSION: {
        return {"INVALID_VERSION"};
    }
    ...
    case DecodeErrorType::UNKNOWN:
    default: {
        return {"UNKNOWN"};
    }
    }
}
```

- Enums of the whole protocol live in `core/include/Definitions.h`, the enums of one family
  in its header, file-local ones at the top of the source; stored keys are enum values,
  never literals at the call site (R-124, R-125, R-126).

## 12. Errors and logging

- There are no exceptions; a failure is a `bool`, an empty value or a `...StatusType` value
  (R-127).
- A log message with arguments is built into a named `const std::string` under
  `// notify the user` or `// warn the user`, then logged; a constant message is logged in
  one call. There are two levels, debug and warning, and output goes only through the log
  functions; the core library does not log (R-128, R-129).
- The object that owns a port logs `Starting the <Class> on <port>, <baud>`,
  `Started ...`, `Closing ...` and `Closed ...`. Its serial connection logs
  `<Class> trying to connect to the serial port`,
  `<Class> connected to serial port from <port>, <baud>` and
  `<Class> disconnected from serial port on <port>, <baud> (<status>)` (R-130).
- Host text is built from strings with `std::to_string`, never with `snprintf`; the firmware
  may format one log line into a local buffer (R-131).
- Every invalid input is rejected with a warning that names the argument and the reason,
  the same way everywhere; a library returns a status instead of clamping silently (R-132).

```cpp
// argument must be a number within the limits
uint32_t argumentAsNumber = 0;
if (!parseUint32(argument, maximum, argumentAsNumber) || argumentAsNumber < minimum) {
    // warn the user
    const std::string warningMessage = "Invalid " + std::string(argumentName) + " argument "
                                       + std::string(argument) + ", expected a number from "
                                       + std::to_string(minimum) + " to "
                                       + std::to_string(maximum);
    logWarning(warningMessage);

    // do not proceed further
    return false;
}
```

## 13. Protocol and device code

- Multi-byte fields are split and joined by hand with shifts, never with `memcpy` of a
  struct (R-133).
- A message class keeps its bytes, is built by a field constructor or from a received
  frame, and has one getter per field; there are no serialize, encode or decode names
  (R-134).
- Wire facts live in `Definitions.h`, tunables in the settings headers; the size is checked
  before a byte is touched (R-135, R-136).
- The checksum is one `calculateCRC32(data, size)` with `updateCRC32` (R-137).
- The stream decoder walks an offset, skips one byte on garbage and stays bounded; the
  receive chain is `onReceivedData`, `processBuffer`, `onReceivedFrame` (R-138, R-139).
- A serial port opens 8N1 without flow control; the host reconnects periodically
  (`attemptReconnect()`) (R-140, R-141).
- Message counters are `uint16_t` that wrap by themselves with `++`; an order between two
  counters is the signed distance `static_cast<int16_t>(a - b) < 0`. Protocol buffers are
  sized by named constants; traffic counters are 64-bit `incoming<What>Counter` and
  `outgoing<What>Counter` (R-143, R-144, R-145).
- Elapsed time is `currentTime - this->last<X>Timestamp` against a chrono period; the clock
  returns `std::chrono::milliseconds` that do not wrap (R-146, R-147, R-148).

```cpp
// attempt to reconnect the serial port periodically
const std::chrono::milliseconds currentTime = this->clock.getTime();
if (currentTime - this->lastReconnectTimestamp
    >= HostSettings::SERIAL_PORT_RECONNECT_PERIOD_MSEC) {
    this->lastReconnectTimestamp = currentTime;
    this->attemptReconnect();
}
```

- Periodic housekeeping is `maintain()`; the ESP32 protocol task and the STM32F4 superloop
  call `firmwareRole.maintain()`; firmware periods are named settings constants, never a
  literal in a delay (R-149, R-150).
- Width helpers spell the type, `readUint16`, `storeUint32`; a reader that can fail returns
  `bool` and fills a reference (R-151).
- The firmware keeps `include/<Module>.h` and `src/<Module>.cpp` pairs; the program state
  of `main.cpp` is `static` at file scope under `Global variables.` (R-155, R-254).
- `core/`, `app/` and `firmware/common/` stay free of platform calls; the code builds
  without a warning on GCC, Clang, the ESP32 toolchain and the Arm toolchain (R-156, R-157).

## 14. Tests

- A feature has `tests/test_<Feature>.cpp` with the fixture `class Test<Feature>`; test
  doubles are `Mock<Thing>` above the fixture, and every file keeps its own mocks and
  helpers, there is no shared test header (R-158, R-159).
- A test file includes only `<catch2/catch_all.hpp>` from Catch2, states
  `using namespace sercom;` after the includes and runs every test case on its fixture with
  `TEST_CASE_METHOD` (R-160, R-161, R-162).
- The fixture constructor is commented `<Feature> test fixture.` and builds its objects in
  `// initialize <thing>` steps; the destructor follows and releases them in reverse under
  `// close <thing>`; then come the commented helpers, the deleted copy and move members
  and the protected members with aligned comments (R-163 to R-167).
- Test case bodies use the fixture members without `this->` (R-168).
- A test name is a capitalized sentence without a full stop; the tags are
  `[<Feature>][test<What>]` (R-169, R-170).
- Every block of a test opens with one short lowercase `//` line; a reason is a short
  parenthesis on that line. A converter test is the exception: its body is a plain list of
  `REQUIRE`s without comments (R-171, R-241).
- Assertions are `REQUIRE` and `REQUIRE_FALSE` only; "is not" is `REQUIRE_FALSE(a == b)`.
  An obtained pointer or iterator is asserted before use; the value under test stands on
  the left (R-172 to R-175).
- A literal has the type of the value it meets (`== static_cast<size_t>(0)`,
  `0xCBF43926U` for a CRC32); floating point is compared with `Catch::Approx` (R-176, R-177).
- `CAPTURE` shows the asserted payload and a loop counter; `WARN` prints measured numbers
  (R-178).
- Test locals are `const`, local constants `static constexpr` in camelCase; one check over
  several inputs walks a `static const` table with `std::for_each` or uses SECTIONs, never
  `GENERATE` (R-179, R-180).

```cpp
// drop every sync byte in turn
static const std::vector<size_t> droppedOffsets = {FirstSyncByteOffset, SecondSyncByteOffset};
std::for_each(droppedOffsets.cbegin(), droppedOffsets.cend(), [this](const size_t droppedOffset) {
    // check the recovery from the dropped sync byte
    CAPTURE(droppedOffset);
    ...
});
```

- Time passes only by advancing the injected clock by the configured period constants,
  never by a bare number or a sleep; a wait for an asynchronous condition is a bounded poll
  that fails when the condition never holds (R-181, R-182).
- `tests/test_main.cpp` runs a configured `Catch::Session` in declaration order; the test
  executable links `Catch2::Catch2` (R-183).
- Test data lives in `tests/test_data/`; test-only switches are `SERCOM_TEST_<WHAT>` (R-184,
  R-185).
- A regression test goes into the feature's own test file; every change comes with tests
  that fail without it; inputs are deterministic with fixed seeds, and no test may fail
  (R-186 to R-190).
- Converter tests have one case per direction and walk every value in declaration order;
  paired objects are `test<Thing>` and `another<Thing>`; vector elements are read with
  `.at(i)` (R-241, R-248, R-252).

## 15. CMake

- `CMakeLists.txt` starts with the values a user may change, then the
  `# DO NOT EDIT BELOW #` banner, then the build logic, and ends with the status block
  under `# debug the project` (R-191, R-192, R-212).
- Every block after a blank line opens with one lowercase `#` phrase (R-193).
- Commands are lower case with four spaces; `if (`, `elseif (` and `foreach (` take a
  blank; source and link lists put one item per line with `)` alone (R-194).
- Build values are UPPER_SNAKE `set()` lines (`set(MAXIMUM_PAYLOAD_SIZE 255)`); `option()`
  is used only for `SERCOM_BUILD_BINARIES` and `SERCOM_BUILD_TESTS` (R-195, R-196).
- Build values reach the code only through the `${PROJECT_NAME}_settings` interface target,
  linked `PRIVATE` into every target (R-197, R-198).
- Target names derive from `${PROJECT_NAME}`; sources are listed inline as
  `include/X.h src/X.cpp` per line, without globs or list variables (R-199 to R-203).
- There is one test executable, registered once with the JUnit, XML and console reporters
  (R-205, R-206).
- Dependencies are git submodules in `dep/` and are never changed (R-207, R-208).
- The warning flags are added with `add_compiler_flags` after the targets, `-Werror`
  always; the build type comes from the command line (R-209, R-210).
- Every `target_link_libraries` call uses one keyword, `PRIVATE`, and an executable lists
  every library it uses (R-253).

## 16. Scripts and tools

- A shell script starts with `#!/bin/bash`, a blank line and a framed `#` block: its file
  name, its purpose and its details one sentence per line, the parts separated by bare `#`
  lines, then a `# usage:` line (R-216, R-218).
- A lowercase `#` comment stands above every block; functions and their `local` variables
  are snake_case; top-level settings are UPPER_SNAKE, `readonly`, with defaults the
  environment can override (R-217).
- A Python tool starts with its docstring (the purpose, then the call), has no shebang and
  uses the standard library, single quotes and `%` formatting (R-218, R-219).
- Every helper is a one-line command with `--help` and overridable defaults; nothing is
  hard-coded and nothing is deleted. An unknown option is named in the warning (R-220,
  R-132).

```bash
#!/bin/bash

#
# run_small_limit_test.sh
#
# Build the test executable with a small payload limit and run every test against it.
#
# The limit reaches CMake through the SERCOM_TEST_MAXIMUM_PAYLOAD_SIZE environment variable.
# The other build values of CMakeLists.txt stay as they are.
#
# usage: tools/run_small_limit_test.sh [--build-dir DIR] [--limit BYTES]
#
```

## 17. Gates

- `python3 tools/analysis/check_style.py` reports nothing (R-221).
- cppcheck 2.22 runs at its most thorough level; its findings are fixed in the code. The
  only accepted finding is `shadowMember` for a parameter stored in the member of its name
  (R-222).
- clang-tidy 18 runs with `tools/analysis/clang-tidy.yaml` on the own headers and reports
  nothing (R-223).
- The repository root holds no analyzer configuration and no `cmake/` directory; the
  configurations live in `tools/analysis/` (R-224).
- The sanitizer builds need no suppressions; a suppression would name third-party frames
  only, with its reason (R-225).

## 18. Documents and commits

- The README has the sections Building the project, Run tests, Run application,
  Contribution guidelines and Contributors, with numbered steps right under each heading
  (R-226 to R-229).
- Documents describe the intended behaviour, are written in Markdown with PNG images in
  an `images/` directory, and state results as measured numbers with units and sample sizes
  (R-230, R-231, R-232).
- A commit subject is `FileName: Comment`, capitalized, imperative, without a full stop and
  without a body; one commit per file or header and source pair, each one building; work
  happens on dated branches through pull requests (R-233 to R-238).
- Commits and documents carry no tool attribution (R-239).
- A change is minimal and uses the idioms the code already has (R-240).
