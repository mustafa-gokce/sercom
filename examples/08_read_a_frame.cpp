#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "../app/include/Messages.h"
#include "../core/include/Definitions.h"
#include "../core/include/Frame.h"
#include "../host/include/HostUtils.h"

/*
 * Print every field of a frame through its getter.
 */
void printFields(const sercom::Frame::Frame &frame);

/*
 * Print the bytes of a frame with both of its checks.
 */
void printChecks(const sercom::Frame::Frame &frame);

/*
 * Get a field value as hex text with two digits per field byte.
 */
[[nodiscard]] std::string getHexFieldText(uint32_t value, size_t fieldSize);

/*
 * Print one result line with its name in an aligned column.
 */
void printResult(std::string_view resultName, const std::string &result);

/*
 * Main entry point of the executable.
 */
int main(int argc, char *argv[]);

int main([[maybe_unused]] int argc, [[maybe_unused]] char *argv[])
{
    using namespace sercom;

    // get the header bytes of a ping request frame as they arrive from the wire
    std::vector<uint8_t> frameBytes = {0xA5, 0xC3, 0x01, 0x01, 0x07, 0x00, 0x01, 0x00, 0x04};

    // add the payload bytes and the CRC32 bytes behind the header
    const std::vector<uint8_t> payloadBytes = {0xE8, 0x03, 0x00, 0x00};
    const std::vector<uint8_t> crcBytes = {0x21, 0x12, 0xD0, 0x63};
    frameBytes.insert(frameBytes.end(), payloadBytes.cbegin(), payloadBytes.cend());
    frameBytes.insert(frameBytes.end(), crcBytes.cbegin(), crcBytes.cend());

    // build a frame from the bytes and print it
    const Frame::Frame frame(frameBytes.data(), frameBytes.size());
    HostUtils::printLine("frame as received");
    printFields(frame);
    printChecks(frame);

    // flip the lowest bit of the first payload byte
    std::vector<uint8_t> flippedBytes = frameBytes;
    flippedBytes.at(Definitions::HEADER_SIZE) = static_cast<uint8_t>(
        flippedBytes.at(Definitions::HEADER_SIZE) ^ 0x01);
    const Frame::Frame flippedFrame(flippedBytes.data(), flippedBytes.size());
    HostUtils::printLine("frame with one payload bit flipped");
    printChecks(flippedFrame);

    // lose the last byte
    const Frame::Frame cutFrame(frameBytes.data(), frameBytes.size() - 1);
    HostUtils::printLine("frame with the last byte lost");
    printChecks(cutFrame);

    // checks must pass the frame as received
    if (!frame.getLikelyValid() || !frame.getCRCValid()) {
        HostUtils::logWarning("The checks did not pass the frame as received");
        return 1;
    }

    // only the CRC32 check must find the flipped bit
    if (!flippedFrame.getLikelyValid() || flippedFrame.getCRCValid()) {
        HostUtils::logWarning("The CRC32 check alone did not find the flipped bit");
        return 1;
    }

    // both checks must find the lost byte
    if (cutFrame.getLikelyValid() || cutFrame.getCRCValid()) {
        HostUtils::logWarning("The checks did not both find the lost byte");
        return 1;
    }

    // example finished successfully
    return 0;
}

void printFields(const sercom::Frame::Frame &frame)
{
    using namespace sercom;

    // print the header fields
    printResult("getVersion()", std::to_string(frame.getVersion()));
    printResult("getFlags()",
                getHexFieldText(frame.getFlags(), Definitions::FLAGS_SIZE) + " "
                    + HostUtils::getFrameFlagsText(frame.getFlags()));
    printResult("getMessageCounter()", std::to_string(frame.getMessageCounter()));
    const Messages::MessageType messageType = Messages::toMessageType(frame.getMessageType());
    printResult("getMessageType()",
                getHexFieldText(frame.getMessageType(), Definitions::MESSAGE_TYPE_SIZE) + " "
                    + std::string(Messages::getMessageTypeText(messageType)));
    printResult("getPayloadLength()", std::to_string(frame.getPayloadLength()));

    // print the payload and the CRC32
    printResult("getPayload()",
                HostUtils::toHexString(frame.getPayload(), frame.getPayloadLength()));
    printResult("getCRC32()", getHexFieldText(frame.getCRC32(), Definitions::CRC_SIZE));
}

void printChecks(const sercom::Frame::Frame &frame)
{
    using namespace sercom;

    // print the bytes with both checks
    printResult("bytes", HostUtils::toHexString(frame.getData(), frame.getSize()));
    printResult("getLikelyValid()", frame.getLikelyValid() ? "yes" : "no");
    printResult("getCRCValid()", frame.getCRCValid() ? "yes" : "no");
}

std::string getHexFieldText(const uint32_t value, const size_t fieldSize)
{
    using namespace sercom;

    // expose two hex digits per field byte
    return "0x" + HostUtils::getHexText(value, 2 * fieldSize);
}

void printResult(const std::string_view resultName, const std::string &result)
{
    using namespace sercom;

    // print the result after its name
    static constexpr size_t resultNameWidth = 19;
    const std::string line = "  " + HostUtils::getLeftJustifiedText(resultName, resultNameWidth)
                             + " : " + result;
    HostUtils::printLine(line);
}
