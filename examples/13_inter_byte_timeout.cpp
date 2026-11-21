#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "../app/include/Messages.h"
#include "../core/include/Clock.h"
#include "../core/include/Definitions.h"
#include "../core/include/Frame.h"
#include "../core/include/FrameDecoder.h"
#include "../core/include/Settings.h"
#include "../host/include/HostUtils.h"

class MockClock final : public sercom::Clock::Clock
{
public:
    /*
     * Mock clock that only moves when the example advances it.
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
     * Get the current time of the mock clock.
     */
    [[nodiscard]] std::chrono::milliseconds getTime() const final
    {
        // expose the current time
        return this->currentTime;
    }

    /*
     * Move the current time forward.
     */
    void advanceTime(const std::chrono::milliseconds duration)
    {
        // move the current time forward
        this->currentTime += duration;
    }

private:
    std::chrono::milliseconds currentTime; // current time of the mock clock
};

class MockDecoderHandler final : public sercom::FrameDecoder::FrameDecoderHandler
{
public:
    /*
     * Mock decoder handler that prints every received frame and every decode error.
     */
    MockDecoderHandler() = default;
    ~MockDecoderHandler() final {}

    // delete copy constructor and copy assignment operator
    MockDecoderHandler(const MockDecoderHandler &) = delete;
    MockDecoderHandler &operator=(const MockDecoderHandler &) = delete;

    // delete move constructor and move assignment operator
    MockDecoderHandler(MockDecoderHandler &&) = delete;
    MockDecoderHandler &operator=(MockDecoderHandler &&) = delete;

public:
    /*
     * Listen for a received frame and print its message counter.
     */
    void onReceivedFrame(const sercom::Frame::Frame &frame) final
    {
        using namespace sercom;

        // print the message counter
        const std::string line = "  handler: frame received, message counter "
                                 + std::to_string(frame.getMessageCounter());
        HostUtils::printLine(line);
    }

    /*
     * Listen for a decode error and print its type.
     */
    void onDecodeError(const sercom::Definitions::DecodeErrorType decodeErrorType) final
    {
        using namespace sercom;

        // print the decode error
        const std::string line = "  handler: decode error "
                                 + std::string(Definitions::fromDecodeErrorType(decodeErrorType));
        HostUtils::printLine(line);
    }
};

/*
 * Print the time of the mock clock with the step and the number of bytes that the decoder holds.
 */
void printStep(const MockClock &mockClock,
               std::string_view step,
               const sercom::FrameDecoder::FrameDecoder &frameDecoder);

/*
 * Main entry point of the executable.
 */
int main(int argc, char *argv[]);

int main([[maybe_unused]] int argc, [[maybe_unused]] char *argv[])
{
    using namespace sercom;
    using namespace std::chrono_literals;

    // create two frames with a ping request
    static constexpr uint16_t firstMessageCounter = 1;
    static constexpr uint16_t secondMessageCounter = 2;
    const Messages::PingRequest pingRequest(0ms);
    const uint16_t messageType = Messages::fromMessageType(Messages::MessageType::PING_REQUEST);
    const Frame::Frame firstFrame(0,
                                  firstMessageCounter,
                                  messageType,
                                  pingRequest.getPayload(),
                                  pingRequest.getPayloadLength());
    const Frame::Frame secondFrame(0,
                                   secondMessageCounter,
                                   messageType,
                                   pingRequest.getPayload(),
                                   pingRequest.getPayloadLength());

    // create the decoder with the default inter byte timeout
    MockClock mockClock;
    MockDecoderHandler mockDecoderHandler;
    FrameDecoder::FrameDecoder frameDecoder(mockDecoderHandler,
                                            Settings::DEFAULT_INTER_BYTE_TIMEOUT_MSEC);

    // print the inter byte timeout
    const std::string timeoutLine = "inter byte timeout "
                                    + std::to_string(
                                        Settings::DEFAULT_INTER_BYTE_TIMEOUT_MSEC.count())
                                    + " ms";
    HostUtils::printLine(timeoutLine);

    // receive only the first half of the first frame
    const size_t halfSize = firstFrame.getSize() / 2;
    frameDecoder.onReceivedData(firstFrame.getData(), halfSize, mockClock.getTime());
    printStep(mockClock, "received the first half of frame 1", frameDecoder);

    // wait exactly the inter byte timeout
    mockClock.advanceTime(Settings::DEFAULT_INTER_BYTE_TIMEOUT_MSEC);
    frameDecoder.checkTimeout(mockClock.getTime());
    printStep(mockClock, "checked the timeout", frameDecoder);

    // wait one millisecond longer than the inter byte timeout
    static constexpr std::chrono::milliseconds oneMillisecond = 1ms;
    mockClock.advanceTime(oneMillisecond);
    frameDecoder.checkTimeout(mockClock.getTime());
    printStep(mockClock, "checked the timeout", frameDecoder);

    // receive the whole second frame
    frameDecoder.onReceivedData(secondFrame.getData(), secondFrame.getSize(), mockClock.getTime());
    printStep(mockClock, "received the whole frame 2", frameDecoder);

    // decoder must drop the half frame
    const FrameDecoder::DecoderStatistics decoderStatistics = frameDecoder.getStatistics();
    if (decoderStatistics.interByteTimeoutCounter != 1
        || decoderStatistics.discardedByteCounter != halfSize) {
        HostUtils::logWarning("The decoder did not drop the half frame");
        return 1;
    }

    // decoder must accept the second frame
    if (decoderStatistics.incomingFrameCounter != 1) {
        HostUtils::logWarning("The decoder did not accept the second frame");
        return 1;
    }

    // example finished successfully
    return 0;
}

void printStep(const MockClock &mockClock,
               const std::string_view step,
               const sercom::FrameDecoder::FrameDecoder &frameDecoder)
{
    using namespace sercom;

    // print the time in milliseconds with the step and the buffered size
    static constexpr size_t timeWidth = 3;
    static constexpr size_t stepWidth = 36;
    const std::string timeAsText = std::to_string(mockClock.getTime().count());
    const std::string line = "time " + HostUtils::getRightJustifiedText(timeAsText, timeWidth)
                             + " ms: " + HostUtils::getLeftJustifiedText(step, stepWidth)
                             + "decoder holds " + std::to_string(frameDecoder.getBufferedSize())
                             + " bytes";
    HostUtils::printLine(line);
}
