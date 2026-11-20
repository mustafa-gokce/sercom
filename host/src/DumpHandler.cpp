#include "../include/DumpHandler.h"

#include <string>
#include <string_view>

#include "../../app/include/Messages.h"

#include "../include/HostUtils.h"

namespace sercom::DumpHandler {

DumpHandler::~DumpHandler() {}

void DumpHandler::onReceivedFrame(const Frame::Frame &frame)
{
    // get the names of the set flags
    const std::string flagsAsText = HostUtils::getFrameFlagsText(frame.getFlags());

    // get the fields as text in their columns
    static constexpr size_t messageCounterWidth = 5;
    static constexpr size_t messageTypeDigitCount = 2 * Definitions::MESSAGE_TYPE_SIZE;
    static constexpr size_t messageTypeWidth = 28;
    static constexpr size_t flagsWidth = 32;
    static constexpr size_t payloadLengthWidth = 4;
    const std::string messageCounterAsText
        = HostUtils::getLeftJustifiedText(std::to_string(frame.getMessageCounter()),
                                          messageCounterWidth);
    const std::string messageTypeAsText
        = HostUtils::getHexText(frame.getMessageType(), messageTypeDigitCount) + " "
          + HostUtils::getLeftJustifiedText(Messages::getMessageTypeText(
                                                Messages::toMessageType(frame.getMessageType())),
                                            messageTypeWidth);
    const std::string payloadLengthAsText
        = HostUtils::getLeftJustifiedText(std::to_string(frame.getPayloadLength()),
                                          payloadLengthWidth);

    // print the frame
    const std::string line = "FRAME counter=" + messageCounterAsText + " type=0x"
                             + messageTypeAsText
                             + " flags=" + HostUtils::getLeftJustifiedText(flagsAsText, flagsWidth)
                             + " length=" + payloadLengthAsText + " payload="
                             + HostUtils::toHexString(frame.getPayload(), frame.getPayloadLength());
    HostUtils::printLine(line);
}

void DumpHandler::onDecodeError(const Definitions::DecodeErrorType decodeErrorType)
{
    // print the decode error
    const std::string line = "ERROR "
                             + std::string(Definitions::fromDecodeErrorType(decodeErrorType));
    HostUtils::printLine(line);
}

} // namespace sercom::DumpHandler
