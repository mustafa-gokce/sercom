#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "../app/include/Messages.h"
#include "../core/include/Definitions.h"
#include "../core/include/Frame.h"
#include "../core/include/FrameDecoder.h"
#include "../host/include/HostUtils.h"

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
     * Listen for a received frame and print its message counter and its payload text.
     */
    void onReceivedFrame(const sercom::Frame::Frame &frame) final
    {
        using namespace sercom;

        // print the message counter and the payload text
        const std::string line = "  handler: frame found, message counter "
                                 + std::to_string(frame.getMessageCounter()) + ", payload "
                                 + HostUtils::toEscapedString(frame.getPayload(),
                                                              frame.getPayloadLength());
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
 * Add the bytes to the end of the stream and print them under their name.
 */
void addBytes(std::string_view name, const uint8_t *data, size_t size, std::vector<uint8_t> &stream);

/*
 * Create a frame with an echo request that carries the text.
 */
[[nodiscard]] sercom::Frame::Frame createTextFrame(uint16_t messageCounter, std::string_view text);

/*
 * Print a report line with the name and the value of a decoder statistic.
 */
void printReportLine(std::string_view name, uint64_t value);

/*
 * Main entry point of the executable.
 */
int main(int argc, char *argv[]);

int main([[maybe_unused]] int argc, [[maybe_unused]] char *argv[])
{
    using namespace sercom;
    using namespace std::chrono_literals;

    // create the garbage and the frames
    const std::vector<uint8_t> firstGarbage = {0x00, 0x11, 0xA5, 0x22, 0xFF};
    const std::vector<uint8_t> secondGarbage = {0x55, 0xA5, 0xA5};
    static constexpr uint16_t firstMessageCounter = 1;
    static constexpr uint16_t secondMessageCounter = 2;
    const Frame::Frame firstFrame = createTextFrame(firstMessageCounter, "first");
    const Frame::Frame secondFrame = createTextFrame(secondMessageCounter, "second");

    // put garbage and frames into one buffer
    std::vector<uint8_t> stream;
    addBytes("garbage", firstGarbage.data(), firstGarbage.size(), stream);
    addBytes("frame 1", firstFrame.getData(), firstFrame.getSize(), stream);
    addBytes("garbage", secondGarbage.data(), secondGarbage.size(), stream);
    addBytes("frame 2", secondFrame.getData(), secondFrame.getSize(), stream);

    // decode the whole buffer in one call without the inter byte timeout
    HostUtils::printLine("decoding the buffer:");
    MockDecoderHandler mockDecoderHandler;
    FrameDecoder::FrameDecoder frameDecoder(mockDecoderHandler, 0ms);
    frameDecoder.onReceivedData(stream.data(), stream.size(), 0ms);

    // print the decoder statistics
    const FrameDecoder::DecoderStatistics decoderStatistics = frameDecoder.getStatistics();
    HostUtils::printLine("decoder statistics:");
    printReportLine("incoming bytes", decoderStatistics.incomingByteCounter);
    printReportLine("discarded bytes", decoderStatistics.discardedByteCounter);
    printReportLine("incoming frames", decoderStatistics.incomingFrameCounter);
    printReportLine("CRC mismatches", decoderStatistics.crcMismatchCounter);

    // decoder must find both frames
    static constexpr uint64_t frameCount = 2;
    if (decoderStatistics.incomingFrameCounter != frameCount) {
        HostUtils::logWarning("The decoder did not find both frames");
        return 1;
    }

    // decoder must discard exactly the garbage bytes
    const size_t garbageSize = firstGarbage.size() + secondGarbage.size();
    if (decoderStatistics.discardedByteCounter != garbageSize) {
        HostUtils::logWarning("The decoder did not discard exactly the garbage bytes");
        return 1;
    }

    // example finished successfully
    return 0;
}

void addBytes(const std::string_view name,
              const uint8_t *data,
              const size_t size,
              std::vector<uint8_t> &stream)
{
    using namespace sercom;

    // add the bytes to the stream
    stream.insert(stream.end(), data, data + size);

    // print the bytes under their name
    static constexpr size_t nameWidth = 8;
    const std::string line = HostUtils::getLeftJustifiedText(name, nameWidth) + ": "
                             + HostUtils::toHexString(data, size);
    HostUtils::printLine(line);
}

sercom::Frame::Frame createTextFrame(const uint16_t messageCounter, const std::string_view text)
{
    using namespace sercom;

    // create the frame with the text as payload
    const std::vector<uint8_t> payload(text.cbegin(), text.cend());
    const Frame::Frame frame(0,
                             messageCounter,
                             Messages::fromMessageType(Messages::MessageType::ECHO_REQUEST),
                             payload.data(),
                             payload.size());

    // expose the frame
    return frame;
}

void printReportLine(const std::string_view name, const uint64_t value)
{
    using namespace sercom;

    // print the name and the value in aligned columns
    HostUtils::printLine(HostUtils::getReportLine(name, std::to_string(value)));
}
