#include <chrono>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_all.hpp>

#include "../app/include/Messages.h"
#include "../core/include/Clock.h"
#include "../core/include/Definitions.h"
#include "../core/include/Frame.h"
#include "../core/include/FrameTracer.h"
#include "../firmware/common/include/Console.h"
#include "../firmware/common/include/ConsoleOutput.h"

using namespace std::chrono_literals;
using namespace sercom;

namespace {

class MockClock final : public Clock::Clock
{
public:
    /*
     * Mock clock that only moves when the test moves it.
     */
    MockClock()
        : currentTime(0)
    {}
    ~MockClock() final {}

    // delete copy constructor and copy assignment operator
    MockClock(const MockClock &) = delete;
    MockClock &operator=(const MockClock &) = delete;

    // delete move constructor and move assignment operator
    MockClock(MockClock &&) = delete;
    MockClock &operator=(MockClock &&) = delete;

public:
    /*
     * Get the current time in milliseconds.
     */
    [[nodiscard]] std::chrono::milliseconds getTime() const final
    {
        // expose the current time
        return this->currentTime;
    }

    /*
     * Set the current time in milliseconds.
     */
    void setTime(const std::chrono::milliseconds time)
    {
        // set the current time
        this->currentTime = time;
    }

private:
    std::chrono::milliseconds currentTime; // current time in milliseconds
};

class MockConsoleOutput final : public ConsoleOutput::ConsoleOutput
{
public:
    /*
     * Mock console output that keeps every written character and takes as many as the test allows.
     */
    MockConsoleOutput()
        : availableSize(std::numeric_limits<size_t>::max())
        , writeCount(0)
    {}
    ~MockConsoleOutput() final {}

    // delete copy constructor and copy assignment operator
    MockConsoleOutput(const MockConsoleOutput &) = delete;
    MockConsoleOutput &operator=(const MockConsoleOutput &) = delete;

    // delete move constructor and move assignment operator
    MockConsoleOutput(MockConsoleOutput &&) = delete;
    MockConsoleOutput &operator=(MockConsoleOutput &&) = delete;

public:
    /*
     * Get the number of characters that the console output takes now.
     */
    [[nodiscard]] size_t getAvailableSize() const final
    {
        // expose the room the test allows
        return this->availableSize;
    }

    /*
     * Write characters into the kept text.
     */
    void writeText(const char *text, const size_t size) final
    {
        // keep the characters
        this->text.append(text, size);

        // keep track of writes
        ++this->writeCount;
    }

public:
    /*
     * Get every written character.
     */
    [[nodiscard]] std::string getText() const
    {
        // expose the written text
        return this->text;
    }

    /*
     * Get the number of writes.
     */
    [[nodiscard]] size_t getWriteCount() const
    {
        // expose the number of writes
        return this->writeCount;
    }

    /*
     * Set the number of characters that the console output takes.
     */
    void setAvailableSize(const size_t availableSize)
    {
        // set the room of the console output
        this->availableSize = availableSize;
    }

    /*
     * Drop the kept text.
     */
    void clear()
    {
        // drop the written characters
        this->text.clear();
    }

private:
    std::string text;     // written characters
    size_t availableSize; // characters the console output takes
    size_t writeCount;    // number of writes
};

} // namespace

class TestConsole
{
public:
    /*
     * Console test fixture.
     */
    TestConsole()
    {
        // initialize clock
        static constexpr std::chrono::milliseconds startTime = 1234ms;
        this->clock.setTime(startTime);

        // initialize printed console and console that is not printed
        this->console.reset(new Console::Console(this->consoleOutput, this->clock, true));
        this->silentConsole.reset(
            new Console::Console(this->silentConsoleOutput, this->clock, false));
    }
    ~TestConsole()
    {
        // close printed console and console that is not printed
        this->silentConsole.reset(nullptr);
        this->console.reset(nullptr);
    }

    /*
     * Get the bytes of a text.
     */
    [[nodiscard]] static std::vector<uint8_t> getTextBytes(const std::string_view text)
    {
        // expose a copy of the characters
        return std::vector<uint8_t>(text.cbegin(), text.cend());
    }

    // delete copy and move operators
    TestConsole(const TestConsole &other) = delete;
    TestConsole &operator=(const TestConsole &other) = delete;
    TestConsole(TestConsole &&other) noexcept = delete;
    TestConsole &operator=(TestConsole &&other) noexcept = delete;

protected:
    MockClock clock;                                 // clock of both consoles
    MockConsoleOutput consoleOutput;                 // text of the printed console
    MockConsoleOutput silentConsoleOutput;           // text of the console that is not printed
    std::unique_ptr<Console::Console> console;       // printed console under test
    std::unique_ptr<Console::Console> silentConsole; // console that is not printed
};

TEST_CASE_METHOD(TestConsole,
                 "A log line is printed with its time and its level",
                 "[Console][testLogLine]")
{
    // print a debug and a warning log line
    console->printLog(Messages::LogLevelType::DEBUG, "Starting the DeviceApplication");
    console->printLog(Messages::LogLevelType::WARNING, "Heartbeat");

    // check both lines
    REQUIRE(consoleOutput.getText()
            == "[     1.234] log DEBUG: Starting the DeviceApplication\r\n"
               "[     1.234] log WARNING: Heartbeat\r\n");
    REQUIRE(console->getPrinted());
}

TEST_CASE_METHOD(TestConsole,
                 "A traced frame is printed in the columns of the host trace",
                 "[Console][testTracedFrame]")
{
    // trace a received ping request and a sent ping response with a long payload
    static constexpr uint16_t messageCounter = 7;
    const std::vector<uint8_t> pingPayload = {0xE8, 0x03, 0x00, 0x00};
    const std::vector<uint8_t> longPayload(20, 0xAB);
    const Frame::Frame pingFrame(Definitions::fromFrameFlagType(
                                     Definitions::FrameFlagType::ACK_REQUEST),
                                 messageCounter,
                                 Messages::fromMessageType(Messages::MessageType::PING_REQUEST),
                                 pingPayload.data(),
                                 pingPayload.size());
    const Frame::Frame longFrame(static_cast<uint8_t>(
                                     Definitions::fromFrameFlagType(
                                         Definitions::FrameFlagType::ACK_REQUEST)
                                     | Definitions::fromFrameFlagType(
                                         Definitions::FrameFlagType::RETRANSMISSION)),
                                 messageCounter,
                                 Messages::fromMessageType(Messages::MessageType::ECHO_RESPONSE),
                                 longPayload.data(),
                                 longPayload.size());
    FrameTracer::FrameTracerHandler &frameTracerHandler = *console;
    frameTracerHandler.onReceivedTracedFrame(FrameTracer::TraceDirectionType::HOST_TO_DEVICE,
                                             pingFrame,
                                             clock.getTime());
    frameTracerHandler.onReceivedTracedFrame(FrameTracer::TraceDirectionType::DEVICE_TO_HOST,
                                             longFrame,
                                             clock.getTime());

    // check both lines
    REQUIRE(consoleOutput.getText()
            == "[     1.234] >> #7     PING_REQUEST                 ACK_REQUEST                "
               "len   4  E8 03 00 00  CRC ok\r\n"
               "[     1.234] << #7     ECHO_RESPONSE                ACK_REQUEST|RETRANSMISSION "
               "len  20  AB AB AB AB AB AB AB AB AB AB AB AB AB AB AB AB ..  CRC ok\r\n");
}

TEST_CASE_METHOD(TestConsole,
                 "Unframed bytes are printed as escaped text or in hex",
                 "[Console][testUnframedData]")
{
    // trace boot text and noise
    const std::vector<uint8_t> bootText = getTextBytes("ets \"boot\"\r\n");
    const std::vector<uint8_t> noise = {0xA5, 0xC3, 0x0A};
    FrameTracer::FrameTracerHandler &frameTracerHandler = *console;
    frameTracerHandler.onReceivedUnframedData(FrameTracer::TraceDirectionType::DEVICE_TO_HOST,
                                              bootText.data(),
                                              bootText.size(),
                                              clock.getTime());
    frameTracerHandler.onReceivedUnframedData(FrameTracer::TraceDirectionType::HOST_TO_DEVICE,
                                              noise.data(),
                                              noise.size(),
                                              clock.getTime());

    // check both lines
    REQUIRE(consoleOutput.getText()
            == "[     1.234] << unframed data, 12 bytes: \"ets \\\"boot\\\"\\r\\n\"\r\n"
               "[     1.234] >> unframed data, 3 bytes: A5 C3 0A\r\n");
}

TEST_CASE_METHOD(TestConsole,
                 "Lines without room are dropped and counted before the next line",
                 "[Console][testDroppedLines]")
{
    // print two lines without room in the console output
    consoleOutput.setAvailableSize(0);
    console->printLog(Messages::LogLevelType::DEBUG, "first");
    console->printLog(Messages::LogLevelType::DEBUG, "second");
    REQUIRE(consoleOutput.getText().empty());

    // print a line with room and check the notice before it
    consoleOutput.setAvailableSize(std::numeric_limits<size_t>::max());
    console->printLog(Messages::LogLevelType::DEBUG, "third");
    REQUIRE(consoleOutput.getText()
            == "2 console lines dropped\r\n[     1.234] log DEBUG: third\r\n");

    // print one more line and check that the notice is gone
    consoleOutput.clear();
    console->printLog(Messages::LogLevelType::DEBUG, "fourth");
    REQUIRE(consoleOutput.getText() == "[     1.234] log DEBUG: fourth\r\n");
}

TEST_CASE_METHOD(TestConsole,
                 "A console that is not printed never writes to its console output",
                 "[Console][testNotPrinted]")
{
    // print a log line and a traced frame on the console that is not printed
    const Frame::Frame frame(0,
                             0,
                             Messages::fromMessageType(Messages::MessageType::PING_REQUEST),
                             nullptr,
                             0);
    silentConsole->printLog(Messages::LogLevelType::DEBUG, "hidden");
    FrameTracer::FrameTracerHandler &frameTracerHandler = *silentConsole;
    frameTracerHandler.onReceivedTracedFrame(FrameTracer::TraceDirectionType::HOST_TO_DEVICE,
                                             frame,
                                             clock.getTime());

    // check that nothing was written
    REQUIRE_FALSE(silentConsole->getPrinted());
    REQUIRE(silentConsoleOutput.getWriteCount() == static_cast<size_t>(0));
}
