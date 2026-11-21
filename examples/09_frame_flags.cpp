#include <bitset>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "../app/include/Messages.h"
#include "../core/include/Definitions.h"
#include "../core/include/Frame.h"
#include "../host/include/HostUtils.h"

/*
 * Create a frame with a set LED request and the flags.
 */
[[nodiscard]] sercom::Frame::Frame createFrame(uint8_t flags);

/*
 * Print a FLAGS byte after its name as hex, as binary and with the names of the set flags.
 */
void printFlagsLine(std::string_view name, uint8_t flags);

/*
 * Print whether a flag is set in the frame.
 */
void printFlagState(const sercom::Frame::Frame &frame,
                    sercom::Definitions::FrameFlagType frameFlagType);

/*
 * Main entry point of the executable.
 */
int main(int argc, char *argv[]);

int main([[maybe_unused]] int argc, [[maybe_unused]] char *argv[])
{
    using namespace sercom;

    // get the value of every flag
    const uint8_t ackRequest = Definitions::fromFrameFlagType(
        Definitions::FrameFlagType::ACK_REQUEST);
    const uint8_t ack = Definitions::fromFrameFlagType(Definitions::FrameFlagType::ACK);
    const uint8_t nack = Definitions::fromFrameFlagType(Definitions::FrameFlagType::NACK);
    const uint8_t retransmission = Definitions::fromFrameFlagType(
        Definitions::FrameFlagType::RETRANSMISSION);

    // print the bit of every flag
    HostUtils::printLine("every flag is one bit of the FLAGS byte:");
    printFlagsLine("bit 0", ackRequest);
    printFlagsLine("bit 1", ack);
    printFlagsLine("bit 2", nack);
    printFlagsLine("bit 3", retransmission);

    // create one frame per flag combination that the link layer sends
    const uint8_t retransmittedAckRequest = static_cast<uint8_t>(ackRequest | retransmission);
    const Frame::Frame plainFrame = createFrame(0);
    const Frame::Frame requestFrame = createFrame(ackRequest);
    const Frame::Frame retransmittedFrame = createFrame(retransmittedAckRequest);
    const Frame::Frame acknowledgementFrame = createFrame(ack);
    const Frame::Frame rejectionFrame = createFrame(nack);

    // print the FLAGS byte of every frame behind the sync word and the version
    static constexpr size_t flagsOffset = Definitions::SYNC_SIZE + Definitions::VERSION_SIZE;
    HostUtils::printLine("frames with different flags:");
    printFlagsLine("plain message", plainFrame.getData()[flagsOffset]);
    printFlagsLine("request", requestFrame.getData()[flagsOffset]);
    printFlagsLine("retransmitted request", retransmittedFrame.getData()[flagsOffset]);
    printFlagsLine("acknowledgement", acknowledgementFrame.getData()[flagsOffset]);
    printFlagsLine("rejection", rejectionFrame.getData()[flagsOffset]);

    // check the flags of the retransmitted request one by one
    HostUtils::printLine("flags of the retransmitted request one by one:");
    printFlagState(retransmittedFrame, Definitions::FrameFlagType::ACK_REQUEST);
    printFlagState(retransmittedFrame, Definitions::FrameFlagType::ACK);
    printFlagState(retransmittedFrame, Definitions::FrameFlagType::NACK);
    printFlagState(retransmittedFrame, Definitions::FrameFlagType::RETRANSMISSION);

    // retransmitted request must keep both of its flags
    if (retransmittedFrame.getFlags() != retransmittedAckRequest) {
        HostUtils::logWarning("The retransmitted request did not keep its flags");
        return 1;
    }

    // example finished successfully
    return 0;
}

sercom::Frame::Frame createFrame(const uint8_t flags)
{
    using namespace sercom;

    // create the frame with a set LED request
    static constexpr uint16_t messageCounter = 1;
    const Messages::SetLedRequest setLedRequest(true);
    const Frame::Frame frame(flags,
                             messageCounter,
                             Messages::fromMessageType(Messages::MessageType::SET_LED_REQUEST),
                             setLedRequest.getPayload(),
                             setLedRequest.getPayloadLength());

    // expose the frame
    return frame;
}

void printFlagsLine(const std::string_view name, const uint8_t flags)
{
    using namespace sercom;

    // get the flags as hex and as binary text
    static constexpr size_t flagsDigitCount = 2 * Definitions::FLAGS_SIZE;
    static constexpr size_t flagsBitCount = 8 * Definitions::FLAGS_SIZE;
    const std::string flagsAsText = "0x" + HostUtils::getHexText(flags, flagsDigitCount) + " = "
                                    + std::bitset<flagsBitCount>(flags).to_string();

    // print the flags after their name with the names of the set flags
    static constexpr size_t nameWidth = 23;
    const std::string line = "  " + HostUtils::getLeftJustifiedText(name, nameWidth) + flagsAsText
                             + "  " + HostUtils::getFrameFlagsText(flags);
    HostUtils::printLine(line);
}

void printFlagState(const sercom::Frame::Frame &frame,
                    const sercom::Definitions::FrameFlagType frameFlagType)
{
    using namespace sercom;

    // print whether the flag is set
    static constexpr size_t flagNameWidth = 16;
    const std::string_view flagName = Definitions::getFrameFlagTypeText(frameFlagType);
    const std::string line = "  " + HostUtils::getLeftJustifiedText(flagName, flagNameWidth)
                             + (frame.getFlag(frameFlagType) ? "set" : "not set");
    HostUtils::printLine(line);
}
