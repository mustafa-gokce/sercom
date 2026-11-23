#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "../app/include/Messages.h"
#include "../core/include/Definitions.h"
#include "../core/include/Frame.h"
#include "../core/include/Settings.h"
#include "../host/include/HostUtils.h"

/*
 * Get one field of a frame with its offset, its size and its bytes.
 */
[[nodiscard]] std::string getFieldText(const sercom::Frame::Frame &frame,
                                       size_t offset,
                                       size_t size);

/*
 * Print one field of the direct and of the routed frame side by side.
 */
void printField(std::string_view fieldName,
                const std::string &directFieldText,
                const std::string &routedFieldText);

/*
 * Print the frame that a frame becomes when it is read back from its bytes.
 * Get whether its CRC32 is valid.
 */
[[nodiscard]] bool printReadBackFrame(std::string_view frameName, const sercom::Frame::Frame &frame);

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

    // create the direct frame of version 1 that requests an acknowledgement
    static constexpr uint16_t messageCounter = 7;
    const uint16_t messageType = Messages::fromMessageType(Messages::MessageType::PING_REQUEST);
    const Frame::Frame directFrame(Definitions::fromFrameFlagType(
                                       Definitions::FrameFlagType::ACK_REQUEST),
                                   messageCounter,
                                   messageType,
                                   pingRequest.getPayload(),
                                   pingRequest.getPayloadLength());

    // create the routed frame of version 2 from the host to node 3
    static constexpr uint8_t destinationAddress = 3;
    Frame::RouteHeader routeHeader;
    routeHeader.sourceAddress = Definitions::HOST_ADDRESS;
    routeHeader.destinationAddress = destinationAddress;
    routeHeader.timeToLive = Settings::DEFAULT_TIME_TO_LIVE;
    const uint8_t routedFlags = Definitions::fromFrameFlagType(
                                    Definitions::FrameFlagType::ACK_REQUEST)
                                | Definitions::fromFrameFlagType(
                                    Definitions::FrameFlagType::ROUTE_CAPABLE);
    const Frame::Frame routedFrame(routedFlags,
                                   messageCounter,
                                   messageType,
                                   routeHeader,
                                   pingRequest.getPayload(),
                                   pingRequest.getPayloadLength());

    // both frames must be valid
    if (!directFrame.getLikelyValid() || !routedFrame.getLikelyValid()) {
        HostUtils::logWarning("Can not build the ping request frames");
        return 1;
    }

    // print both whole frames
    const std::string directFrameLine = "direct frame    : "
                                        + HostUtils::toHexString(directFrame.getData(),
                                                                 directFrame.getSize());
    HostUtils::printLine(directFrameLine);
    const std::string routedFrameLine = "routed frame    : "
                                        + HostUtils::toHexString(routedFrame.getData(),
                                                                 routedFrame.getSize());
    HostUtils::printLine(routedFrameLine);

    // print the fields of the header that both versions share at the same offsets
    printField("FIELD", "direct", "routed");
    size_t offset = 0;
    const auto printSharedField =
        [&directFrame, &routedFrame, &offset](const std::string_view fieldName, const size_t size) {
            // print the field at the same offset of both frames
            printField(fieldName,
                       getFieldText(directFrame, offset, size),
                       getFieldText(routedFrame, offset, size));
            offset += size;
        };
    printSharedField("SYNC", Definitions::SYNC_SIZE);
    printSharedField("VERSION", Definitions::VERSION_SIZE);
    printSharedField("FLAGS", Definitions::FLAGS_SIZE);
    printSharedField("MESSAGE_COUNTER", Definitions::MESSAGE_COUNTER_SIZE);
    printSharedField("MESSAGE_TYPE", Definitions::MESSAGE_TYPE_SIZE);
    printSharedField("PAYLOAD_LENGTH", Definitions::PAYLOAD_LENGTH_SIZE);

    // print the route header that only the routed frame holds, a dash for the direct frame
    const std::string missingFieldText = "-";
    printField("SOURCE_ADDRESS",
               missingFieldText,
               getFieldText(routedFrame, offset, Definitions::SOURCE_ADDRESS_SIZE));
    offset += Definitions::SOURCE_ADDRESS_SIZE;
    printField("DESTINATION_ADDRESS",
               missingFieldText,
               getFieldText(routedFrame, offset, Definitions::DESTINATION_ADDRESS_SIZE));
    offset += Definitions::DESTINATION_ADDRESS_SIZE;
    printField("TTL",
               missingFieldText,
               getFieldText(routedFrame, offset, Definitions::TIME_TO_LIVE_SIZE));

    // print the payload and the CRC32 behind the header of each version
    const size_t payloadLength = pingRequest.getPayloadLength();
    const size_t directPayloadOffset = Definitions::HEADER_SIZE;
    const size_t routedPayloadOffset = Definitions::ROUTED_HEADER_SIZE;
    printField("PAYLOAD",
               getFieldText(directFrame, directPayloadOffset, payloadLength),
               getFieldText(routedFrame, routedPayloadOffset, payloadLength));
    printField("CRC32",
               getFieldText(directFrame, directPayloadOffset + payloadLength, Definitions::CRC_SIZE),
               getFieldText(routedFrame,
                            routedPayloadOffset + payloadLength,
                            Definitions::CRC_SIZE));
    printField("overhead",
               std::to_string(Definitions::FRAME_OVERHEAD) + " bytes",
               std::to_string(Definitions::ROUTED_FRAME_OVERHEAD) + " bytes");

    // create the copy that node 2 forwards on its next link with that link's counter
    static constexpr uint16_t forwardedMessageCounter = 2;
    Frame::RouteHeader forwardedRouteHeader = routedFrame.getRouteHeader();
    --forwardedRouteHeader.timeToLive;
    const Frame::Frame forwardedFrame(routedFrame.getFlags(),
                                      forwardedMessageCounter,
                                      routedFrame.getMessageType(),
                                      forwardedRouteHeader,
                                      routedFrame.getPayload(),
                                      routedFrame.getPayloadLength());

    // print the forwarded copy and its new CRC32
    static constexpr size_t crc32DigitCount = 2 * Definitions::CRC_SIZE;
    const std::string forwardedFrameLine = "forwarded frame : "
                                           + HostUtils::toHexString(forwardedFrame.getData(),
                                                                    forwardedFrame.getSize());
    HostUtils::printLine(forwardedFrameLine);
    const std::string forwardedLine = "CRC32 0x"
                                      + HostUtils::getHexText(routedFrame.getCRC32(),
                                                              crc32DigitCount)
                                      + " of TTL " + std::to_string(routedFrame.getTimeToLive())
                                      + " became 0x"
                                      + HostUtils::getHexText(forwardedFrame.getCRC32(),
                                                              crc32DigitCount)
                                      + " of TTL " + std::to_string(forwardedFrame.getTimeToLive());
    HostUtils::printLine(forwardedLine);

    // damage the time to live of the routed frame on the wire
    static constexpr size_t timeToLiveOffset = Definitions::HEADER_SIZE
                                               + Definitions::SOURCE_ADDRESS_SIZE
                                               + Definitions::DESTINATION_ADDRESS_SIZE;
    std::vector<uint8_t> damagedData(routedFrame.getData(),
                                     routedFrame.getData() + routedFrame.getSize());
    ++damagedData.at(timeToLiveOffset);
    const Frame::Frame damagedFrame(damagedData.data(), damagedData.size());

    // print the damaged frame and its CRC32 check
    const bool damagedFrameValid = damagedFrame.getCRCValid();
    const std::string damagedFrameLine = "damaged TTL     : "
                                         + HostUtils::toHexString(damagedData.data(),
                                                                  damagedData.size())
                                         + " -> CRC32 " + (damagedFrameValid ? "valid" : "INVALID");
    HostUtils::printLine(damagedFrameLine);

    // read every frame back from its bytes
    const bool directFrameValid = printReadBackFrame("direct",
                                                     Frame::Frame(directFrame.getData(),
                                                                  directFrame.getSize()));
    const bool routedFrameValid = printReadBackFrame("routed",
                                                     Frame::Frame(routedFrame.getData(),
                                                                  routedFrame.getSize()));
    const bool forwardedFrameValid = printReadBackFrame("forwarded",
                                                        Frame::Frame(forwardedFrame.getData(),
                                                                     forwardedFrame.getSize()));

    // every intact frame must be valid and the damaged frame must not
    if (!directFrameValid || !routedFrameValid || !forwardedFrameValid || damagedFrameValid) {
        return 1;
    }

    // frames verified successfully
    return 0;
}

std::string getFieldText(const sercom::Frame::Frame &frame, const size_t offset, const size_t size)
{
    using namespace sercom;

    // expose the offset, the size and the bytes of the field
    static constexpr size_t numberWidth = 2;
    return "offset " + HostUtils::getRightJustifiedText(std::to_string(offset), numberWidth)
           + " size " + std::to_string(size) + " : "
           + HostUtils::toHexString(frame.getData() + offset, size);
}

void printField(const std::string_view fieldName,
                const std::string &directFieldText,
                const std::string &routedFieldText)
{
    using namespace sercom;

    // print the field of both frames in their columns
    static constexpr size_t fieldNameWidth = 20;
    static constexpr size_t fieldTextWidth = 32;
    const std::string line = HostUtils::getLeftJustifiedText(fieldName, fieldNameWidth) + " "
                             + HostUtils::getLeftJustifiedText(directFieldText, fieldTextWidth)
                             + " " + routedFieldText;
    HostUtils::printLine(line);
}

bool printReadBackFrame(const std::string_view frameName, const sercom::Frame::Frame &frame)
{
    using namespace sercom;

    // print the version, the route and the CRC32 check of the frame
    static constexpr size_t frameNameWidth = 10;
    const bool frameValid = frame.getCRCValid();
    const std::string line = "read back "
                             + HostUtils::getLeftJustifiedText(frameName, frameNameWidth)
                             + ": version " + std::to_string(frame.getVersion()) + ", routed "
                             + (frame.getRouted() ? "yes" : "no") + ", source "
                             + std::to_string(frame.getSourceAddress()) + ", destination "
                             + std::to_string(frame.getDestinationAddress()) + ", TTL "
                             + std::to_string(frame.getTimeToLive()) + ", CRC32 "
                             + (frameValid ? "valid" : "INVALID");
    HostUtils::printLine(line);

    // expose whether the CRC32 is valid
    return frameValid;
}
