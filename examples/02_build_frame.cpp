#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "../app/include/Messages.h"
#include "../core/include/CRC32.h"
#include "../core/include/Definitions.h"
#include "../core/include/Frame.h"
#include "../host/include/HostUtils.h"

/*
 * Print one field of the frame with its offset and its size.
 */
void printField(std::string_view fieldName, const uint8_t *data, size_t offset, size_t size);

/*
 * Main entry point of the executable.
 */
int main(int argc, char *argv[]);

int main([[maybe_unused]] int argc, [[maybe_unused]] char *argv[])
{
    using namespace sercom;
    using namespace std::chrono_literals;

    // create a ping request sent at one second
    static constexpr std::chrono::milliseconds requestTimestamp = 1 * 1000ms;
    const Messages::PingRequest pingRequest(requestTimestamp);

    // create a frame that requests an acknowledgement
    static constexpr uint16_t messageCounter = 7;
    const Frame::Frame frame(Definitions::fromFrameFlagType(Definitions::FrameFlagType::ACK_REQUEST),
                             messageCounter,
                             Messages::fromMessageType(Messages::MessageType::PING_REQUEST),
                             pingRequest.getPayload(),
                             pingRequest.getPayloadLength());

    // frame must be valid
    if (!frame.getLikelyValid()) {
        HostUtils::logWarning("Can not build the ping request frame");
        return 1;
    }

    // print the whole frame
    const std::string frameAsHexString = HostUtils::toHexString(frame.getData(), frame.getSize());
    const std::string frameLine = "whole frame      : " + frameAsHexString;
    HostUtils::printLine(frameLine);

    // print every field after the fields in front of it
    size_t offset = 0;
    printField("SYNC", frame.getData(), offset, Definitions::SYNC_SIZE);
    offset += Definitions::SYNC_SIZE;
    printField("VERSION", frame.getData(), offset, Definitions::VERSION_SIZE);
    offset += Definitions::VERSION_SIZE;
    printField("FLAGS", frame.getData(), offset, Definitions::FLAGS_SIZE);
    offset += Definitions::FLAGS_SIZE;
    printField("MESSAGE_COUNTER", frame.getData(), offset, Definitions::MESSAGE_COUNTER_SIZE);
    offset += Definitions::MESSAGE_COUNTER_SIZE;
    printField("MESSAGE_TYPE", frame.getData(), offset, Definitions::MESSAGE_TYPE_SIZE);
    offset += Definitions::MESSAGE_TYPE_SIZE;
    printField("PAYLOAD_LENGTH", frame.getData(), offset, Definitions::PAYLOAD_LENGTH_SIZE);
    offset += Definitions::PAYLOAD_LENGTH_SIZE;
    printField("PAYLOAD", frame.getData(), offset, pingRequest.getPayloadLength());
    offset += pingRequest.getPayloadLength();
    printField("CRC32", frame.getData(), offset, Definitions::CRC_SIZE);

    // calculate the CRC32 like a receiver
    const uint32_t calculatedCRC32 = CRC32::calculateCRC32(frame.getData() + Definitions::SYNC_SIZE,
                                                           offset - Definitions::SYNC_SIZE);
    const uint32_t receivedCRC32 = frame.getCRC32();
    const bool crc32Valid = calculatedCRC32 == receivedCRC32;

    // print the calculated and the received CRC32
    static constexpr size_t crc32DigitCount = 2 * Definitions::CRC_SIZE;
    const std::string line = "CRC32 calculated 0x"
                             + HostUtils::getHexText(calculatedCRC32, crc32DigitCount)
                             + " received 0x"
                             + HostUtils::getHexText(receivedCRC32, crc32DigitCount) + " -> "
                             + (crc32Valid ? "valid" : "INVALID");
    HostUtils::printLine(line);

    // CRC32 must be valid
    if (!crc32Valid) {
        return 1;
    }

    // frame verified successfully
    return 0;
}

void printField(const std::string_view fieldName,
                const uint8_t *data,
                const size_t offset,
                const size_t size)
{
    using namespace sercom;

    // get the field columns as text
    static constexpr size_t fieldNameWidth = 16;
    static constexpr size_t numberWidth = 2;
    const std::string fieldAsText = HostUtils::getLeftJustifiedText(fieldName, fieldNameWidth)
                                    + " offset "
                                    + HostUtils::getRightJustifiedText(std::to_string(offset),
                                                                       numberWidth)
                                    + " size "
                                    + HostUtils::getRightJustifiedText(std::to_string(size),
                                                                       numberWidth);

    // print the field with its bytes
    const std::string line = fieldAsText + " : " + HostUtils::toHexString(data + offset, size);
    HostUtils::printLine(line);
}
