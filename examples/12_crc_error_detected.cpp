#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "../app/include/Messages.h"
#include "../core/include/CRC32.h"
#include "../core/include/Definitions.h"
#include "../core/include/Frame.h"
#include "../core/include/FrameDecoder.h"
#include "../host/include/HostUtils.h"

class MockDecoderHandler final : public sercom::FrameDecoder::FrameDecoderHandler
{
public:
    /*
     * Mock decoder handler that prints every received LED request and every decode error.
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
     * Listen for a received frame and print the LED state that it asks for.
     */
    void onReceivedFrame(const sercom::Frame::Frame &frame) final
    {
        using namespace sercom;

        // print the message counter and the LED state
        const Messages::SetLedRequest setLedRequest(frame);
        const std::string line = "  handler: frame received, message counter "
                                 + std::to_string(frame.getMessageCounter()) + ", LED "
                                 + (setLedRequest.getLedOn() ? "on" : "off");
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
 * Create a frame with a set LED request that switches the LED on.
 */
[[nodiscard]] sercom::Frame::Frame createLedOnFrame(uint16_t messageCounter);

/*
 * Main entry point of the executable.
 */
int main(int argc, char *argv[]);

int main([[maybe_unused]] int argc, [[maybe_unused]] char *argv[])
{
    using namespace sercom;
    using namespace std::chrono_literals;

    // create two frames that switch the LED on
    static constexpr uint16_t firstMessageCounter = 1;
    static constexpr uint16_t secondMessageCounter = 2;
    const Frame::Frame firstFrame = createLedOnFrame(firstMessageCounter);
    const Frame::Frame secondFrame = createLedOnFrame(secondMessageCounter);

    // flip the lowest bit of the first payload byte of the first frame
    static constexpr uint8_t flippedBit = 0x01;
    std::vector<uint8_t> damagedBytes(firstFrame.getData(),
                                      firstFrame.getData() + firstFrame.getSize());
    damagedBytes[Definitions::HEADER_SIZE] ^= flippedBit;

    // print the frame before and after the damage
    const std::string sentLine = "sent     : "
                                 + HostUtils::toHexString(firstFrame.getData(),
                                                          firstFrame.getSize());
    HostUtils::printLine(sentLine);
    const std::string receivedLine = "received : "
                                     + HostUtils::toHexString(damagedBytes.data(),
                                                              damagedBytes.size());
    HostUtils::printLine(receivedLine);

    // calculate the CRC32 of the damaged bytes like the decoder
    const Frame::Frame damagedFrame(damagedBytes.data(), damagedBytes.size());
    const size_t crcOffset = damagedBytes.size() - Definitions::CRC_SIZE;
    const uint32_t calculatedCRC32 = CRC32::calculateCRC32(damagedBytes.data()
                                                               + Definitions::SYNC_SIZE,
                                                           crcOffset - Definitions::SYNC_SIZE);

    // print the received and the calculated CRC32
    static constexpr size_t crc32DigitCount = 2 * Definitions::CRC_SIZE;
    const std::string crcLine = "CRC32 in the frame 0x"
                                + HostUtils::getHexText(damagedFrame.getCRC32(), crc32DigitCount)
                                + ", CRC32 of the received bytes 0x"
                                + HostUtils::getHexText(calculatedCRC32, crc32DigitCount);
    HostUtils::printLine(crcLine);

    // decode the damaged frame followed by the second frame
    HostUtils::printLine("decoding the damaged frame and the second frame:");
    std::vector<uint8_t> receivedBytes(damagedBytes);
    receivedBytes.insert(receivedBytes.end(),
                         secondFrame.getData(),
                         secondFrame.getData() + secondFrame.getSize());
    MockDecoderHandler mockDecoderHandler;
    FrameDecoder::FrameDecoder frameDecoder(mockDecoderHandler, 0ms);
    frameDecoder.onReceivedData(receivedBytes.data(), receivedBytes.size(), 0ms);

    // print the decoder statistics
    const FrameDecoder::DecoderStatistics decoderStatistics = frameDecoder.getStatistics();
    const std::string statisticsLine = "CRC mismatches "
                                       + std::to_string(decoderStatistics.crcMismatchCounter)
                                       + ", incoming frames "
                                       + std::to_string(decoderStatistics.incomingFrameCounter)
                                       + ", discarded bytes "
                                       + std::to_string(decoderStatistics.discardedByteCounter);
    HostUtils::printLine(statisticsLine);

    // decoder must reject the damaged frame
    if (decoderStatistics.crcMismatchCounter != 1) {
        HostUtils::logWarning("The decoder did not reject the damaged frame");
        return 1;
    }

    // decoder must accept the good frame
    if (decoderStatistics.incomingFrameCounter != 1) {
        HostUtils::logWarning("The decoder did not accept the good frame");
        return 1;
    }

    // example finished successfully
    return 0;
}

sercom::Frame::Frame createLedOnFrame(const uint16_t messageCounter)
{
    using namespace sercom;

    // create the frame with the set LED request
    const Messages::SetLedRequest setLedRequest(true);
    const Frame::Frame frame(0,
                             messageCounter,
                             Messages::fromMessageType(Messages::MessageType::SET_LED_REQUEST),
                             setLedRequest.getPayload(),
                             setLedRequest.getPayloadLength());

    // expose the frame
    return frame;
}
