#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "../app/include/Messages.h"
#include "../core/include/Definitions.h"
#include "../core/include/Frame.h"
#include "../core/include/FrameDecoder.h"
#include "../host/include/HostUtils.h"

class MockDecoderHandler final : public sercom::FrameDecoder::FrameDecoderHandler
{
public:
    /*
     * Mock decoder handler that prints every complete frame and every decode error.
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
     * Listen for a complete frame and print its header fields.
     */
    void onReceivedFrame(const sercom::Frame::Frame &frame) final
    {
        using namespace sercom;

        // print the header fields of the frame
        static constexpr size_t messageTypeDigitCount = 2 * Definitions::MESSAGE_TYPE_SIZE;
        const std::string line = "  handler: frame complete, message counter "
                                 + std::to_string(frame.getMessageCounter()) + ", message type 0x"
                                 + HostUtils::getHexText(frame.getMessageType(),
                                                         messageTypeDigitCount)
                                 + ", payload " + std::to_string(frame.getPayloadLength())
                                 + " bytes";
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
 * Get the name of the frame field that holds the byte at the offset.
 */
[[nodiscard]] std::string_view getFieldName(size_t offset, size_t payloadLength);

/*
 * Main entry point of the executable.
 */
int main(int argc, char *argv[]);

int main([[maybe_unused]] int argc, [[maybe_unused]] char *argv[])
{
    using namespace sercom;
    using namespace std::chrono_literals;

    // create a frame with a ping request sent at one second
    static constexpr std::chrono::milliseconds requestTimestamp = 1 * 1000ms;
    static constexpr uint16_t messageCounter = 1;
    const Messages::PingRequest pingRequest(requestTimestamp);
    const Frame::Frame frame(0,
                             messageCounter,
                             Messages::fromMessageType(Messages::MessageType::PING_REQUEST),
                             pingRequest.getPayload(),
                             pingRequest.getPayloadLength());

    // print the size of the frame
    const std::string sizeLine = "frame of " + std::to_string(frame.getSize())
                                 + " bytes, fed to the decoder one byte at a time";
    HostUtils::printLine(sizeLine);

    // create the decoder without the inter byte timeout
    MockDecoderHandler mockDecoderHandler;
    FrameDecoder::FrameDecoder frameDecoder(mockDecoderHandler, 0ms);

    // feed the frame one byte at a time
    static constexpr size_t numberWidth = 2;
    static constexpr size_t byteDigitCount = 2;
    static constexpr size_t fieldNameWidth = 16;
    for (size_t offset = 0; offset < frame.getSize(); ++offset) {
        // feed the byte
        const uint8_t byte = frame.getData()[offset];
        frameDecoder.onReceivedByte(byte, 0ms);

        // print the byte with its field and the buffered size
        const std::string_view fieldName = getFieldName(offset, frame.getPayloadLength());
        const std::string line
            = "byte " + HostUtils::getRightJustifiedText(std::to_string(offset + 1), numberWidth)
              + ": " + HostUtils::getHexText(byte, byteDigitCount) + "  "
              + HostUtils::getLeftJustifiedText(fieldName, fieldNameWidth) + " buffered "
              + HostUtils::getRightJustifiedText(std::to_string(frameDecoder.getBufferedSize()),
                                                 numberWidth);
        HostUtils::printLine(line);
    }

    // decoder must find the frame without discarding a byte
    const FrameDecoder::DecoderStatistics decoderStatistics = frameDecoder.getStatistics();
    if (decoderStatistics.incomingFrameCounter != 1 || decoderStatistics.discardedByteCounter != 0) {
        HostUtils::logWarning("The decoder did not find the frame");
        return 1;
    }

    // example finished successfully
    return 0;
}

std::string_view getFieldName(const size_t offset, const size_t payloadLength)
{
    using namespace sercom;

    // sync word opens the frame
    static constexpr size_t versionOffset = Definitions::SYNC_SIZE;
    if (offset < versionOffset) {
        return {"SYNC"};
    }

    // version follows the sync word
    static constexpr size_t flagsOffset = versionOffset + Definitions::VERSION_SIZE;
    if (offset < flagsOffset) {
        return {"VERSION"};
    }

    // flags follow the version
    static constexpr size_t messageCounterOffset = flagsOffset + Definitions::FLAGS_SIZE;
    if (offset < messageCounterOffset) {
        return {"FLAGS"};
    }

    // message counter follows the flags
    static constexpr size_t messageTypeOffset = messageCounterOffset
                                                + Definitions::MESSAGE_COUNTER_SIZE;
    if (offset < messageTypeOffset) {
        return {"MESSAGE_COUNTER"};
    }

    // message type follows the message counter
    static constexpr size_t payloadLengthOffset = messageTypeOffset
                                                  + Definitions::MESSAGE_TYPE_SIZE;
    if (offset < payloadLengthOffset) {
        return {"MESSAGE_TYPE"};
    }

    // payload length closes the header
    if (offset < Definitions::HEADER_SIZE) {
        return {"PAYLOAD_LENGTH"};
    }

    // payload follows the header
    if (offset < Definitions::HEADER_SIZE + payloadLength) {
        return {"PAYLOAD"};
    }

    // CRC32 closes the frame
    return {"CRC32"};
}
