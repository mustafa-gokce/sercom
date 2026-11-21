#include "../include/Console.h"

#include <algorithm>
#include <array>
#include <cinttypes>
#include <cstdio>

#include "../../../core/include/Definitions.h"
#include "../../../core/include/Settings.h"

#include "../include/CommonSettings.h"

namespace sercom::Console {

namespace {

/*
 * Write a time in brackets as seconds with millisecond decimals.
 */
void writeTimeText(const std::chrono::milliseconds time,
                   std::array<char, CommonSettings::CONSOLE_FIELD_SIZE> &timeAsText)
{
    // split the time into whole seconds and milliseconds
    const std::chrono::seconds seconds = std::chrono::duration_cast<std::chrono::seconds>(time);
    const std::chrono::milliseconds milliseconds = time - seconds;

    // write the seconds and the milliseconds in their columns
    static constexpr int secondsWidth = 6;
    static constexpr int millisecondsWidth = 3;
    [[maybe_unused]] const int timeAsTextLength
        = std::snprintf(timeAsText.data(),
                        timeAsText.size(),
                        "[%*" PRIu32 ".%0*" PRIu32 "]",
                        secondsWidth,
                        static_cast<uint32_t>(seconds.count()),
                        millisecondsWidth,
                        static_cast<uint32_t>(milliseconds.count()));
}

/*
 * Append the name of a flag that is set in the FLAGS field behind a bar.
 */
void appendFlagName(const uint8_t flags,
                    const Definitions::FrameFlagType frameFlagType,
                    std::array<char, CommonSettings::CONSOLE_FIELD_SIZE> &flagsAsText,
                    size_t &flagsAsTextLength)
{
    // flag must be set
    if (!Definitions::isFrameFlagSet(flags, frameFlagType)) {
        return;
    }

    // append the flag name behind a bar
    const std::string_view flagName = Definitions::getFrameFlagTypeText(frameFlagType);
    const int flagNameLength = std::snprintf(flagsAsText.data() + flagsAsTextLength,
                                             flagsAsText.size() - flagsAsTextLength,
                                             "%s%.*s",
                                             flagsAsTextLength == 0 ? "" : "|",
                                             static_cast<int>(flagName.size()),
                                             flagName.data());
    flagsAsTextLength = std::min(flagsAsTextLength
                                     + static_cast<size_t>(std::max(flagNameLength, 0)),
                                 flagsAsText.size() - 1);
}

/*
 * Append bytes as upper case hex text separated by spaces as far as the text has room.
 */
void appendHexText(
    const uint8_t *data, const size_t size, char *text, const size_t textSize, size_t &textLength)
{
    // iterate over the bytes
    std::for_each(data, data + size, [text, textSize, &textLength](const uint8_t byte) {
        // byte in hex must fit into the text
        if (textLength + CommonSettings::CONSOLE_HEX_BYTE_LENGTH >= textSize) {
            return;
        }

        // add the byte in hex behind a space after the first byte
        const int byteAsTextLength = std::snprintf(text + textLength,
                                                   textSize - textLength,
                                                   "%s%02" PRIX32,
                                                   textLength == 0 ? "" : " ",
                                                   static_cast<uint32_t>(byte));
        textLength += static_cast<size_t>(std::max(byteAsTextLength, 0));
    });
}

/*
 * Get the escape of a byte with a named escape or an empty text for every other byte.
 */
[[nodiscard]] std::string_view getNamedEscape(const uint8_t byte)
{
    // escape the special characters by name
    if (byte == '\r') {
        return {"\\r"};
    }
    if (byte == '\n') {
        return {"\\n"};
    }
    if (byte == '\t') {
        return {"\\t"};
    }
    if (byte == '"') {
        return {"\\\""};
    }
    if (byte == '\\') {
        return {"\\\\"};
    }

    // expose no escape for every other byte
    return {};
}

/*
 * Append text bytes with a named escape for the bytes that are not printed as they are.
 */
void appendEscapedText(
    const uint8_t *data, const size_t size, char *text, const size_t textSize, size_t &textLength)
{
    // iterate over the bytes
    std::for_each(data, data + size, [text, textSize, &textLength](const uint8_t byte) {
        // escaped byte must fit into the text
        if (textLength + CommonSettings::CONSOLE_MAXIMUM_ESCAPE_LENGTH >= textSize) {
            return;
        }

        // add a byte with a named escape
        const std::string_view namedEscape = getNamedEscape(byte);
        if (!namedEscape.empty()) {
            textLength += namedEscape.copy(text + textLength, namedEscape.size());
            return;
        }

        // add a printable byte as it is
        text[textLength] = static_cast<char>(byte);
        ++textLength;
    });
}

} // namespace

Console::Console(ConsoleOutput::ConsoleOutput &consoleOutput,
                 const Clock::Clock &clock,
                 const bool printed)
    : consoleOutput(consoleOutput)
    , clock(clock)
    , printed(printed)
    , droppedLineCount(0)
{}

Console::~Console() {}

void Console::printLog(const Messages::LogLevelType logLevelType, const std::string_view text)
{
    // console must be printed
    if (!this->printed) {
        return;
    }

    // get the time and the level of the log line
    std::array<char, CommonSettings::CONSOLE_FIELD_SIZE> timeAsText
        = std::array<char, CommonSettings::CONSOLE_FIELD_SIZE>();
    writeTimeText(this->clock.getTime(), timeAsText);
    const std::string_view logLevelTypeAsText = Messages::getLogLevelTypeText(logLevelType);

    // print the log line with its time and level
    std::array<char, CommonSettings::CONSOLE_LINE_SIZE> line
        = std::array<char, CommonSettings::CONSOLE_LINE_SIZE>();
    const int lineLength = std::snprintf(line.data(),
                                         line.size(),
                                         "%s log %.*s: %.*s\r\n",
                                         timeAsText.data(),
                                         static_cast<int>(logLevelTypeAsText.size()),
                                         logLevelTypeAsText.data(),
                                         static_cast<int>(text.size()),
                                         text.data());
    this->printLine(line.data(), lineLength);
}

bool Console::getPrinted() const
{
    // expose whether the console is printed
    return this->printed;
}

void Console::onReceivedTracedFrame(const FrameTracer::TraceDirectionType traceDirectionType,
                                    const Frame::Frame &frame,
                                    const std::chrono::milliseconds currentTime)
{
    // console must be printed
    if (!this->printed) {
        return;
    }

    // get the message type by its name
    const Messages::MessageType messageType = Messages::toMessageType(frame.getMessageType());
    const std::string_view messageTypeAsName = Messages::getMessageTypeText(messageType);
    std::array<char, CommonSettings::CONSOLE_FIELD_SIZE> messageTypeAsText
        = std::array<char, CommonSettings::CONSOLE_FIELD_SIZE>();
    if (messageType == Messages::MessageType::UNKNOWN) {
        [[maybe_unused]] const int messageTypeAsTextLength
            = std::snprintf(messageTypeAsText.data(),
                            messageTypeAsText.size(),
                            "0x%04" PRIX32,
                            static_cast<uint32_t>(frame.getMessageType()));
    } else {
        [[maybe_unused]] const int messageTypeAsTextLength
            = std::snprintf(messageTypeAsText.data(),
                            messageTypeAsText.size(),
                            "%.*s",
                            static_cast<int>(messageTypeAsName.size()),
                            messageTypeAsName.data());
    }

    // get the names of the set flags
    std::array<char, CommonSettings::CONSOLE_FIELD_SIZE> flagsAsText
        = std::array<char, CommonSettings::CONSOLE_FIELD_SIZE>();
    size_t flagsAsTextLength = 0;
    const uint8_t flags = frame.getFlags();
    appendFlagName(flags, Definitions::FrameFlagType::ACK_REQUEST, flagsAsText, flagsAsTextLength);
    appendFlagName(flags, Definitions::FrameFlagType::ACK, flagsAsText, flagsAsTextLength);
    appendFlagName(flags, Definitions::FrameFlagType::NACK, flagsAsText, flagsAsTextLength);
    appendFlagName(flags,
                   Definitions::FrameFlagType::RETRANSMISSION,
                   flagsAsText,
                   flagsAsTextLength);

    // name a FLAGS field without a set flag
    if (flagsAsTextLength == 0) {
        [[maybe_unused]] const int noneAsTextLength = std::snprintf(flagsAsText.data(),
                                                                    flagsAsText.size(),
                                                                    "NONE");
    }

    // get the payload up to the traced payload limit
    const size_t payloadLength = frame.getPayloadLength();
    const size_t tracedPayloadSize = std::min(payloadLength, Settings::MAXIMUM_TRACED_PAYLOAD_SIZE);
    std::array<char, CommonSettings::CONSOLE_FIELD_SIZE> payloadAsText
        = std::array<char, CommonSettings::CONSOLE_FIELD_SIZE>();
    size_t payloadAsTextLength = 0;
    appendHexText(frame.getPayload(),
                  tracedPayloadSize,
                  payloadAsText.data(),
                  payloadAsText.size(),
                  payloadAsTextLength);

    // get the time and the direction of the frame
    std::array<char, CommonSettings::CONSOLE_FIELD_SIZE> timeAsText
        = std::array<char, CommonSettings::CONSOLE_FIELD_SIZE>();
    writeTimeText(currentTime, timeAsText);
    const std::string_view traceDirectionTypeAsText = FrameTracer::getTraceDirectionTypeText(
        traceDirectionType);

    // print the frame in the columns of the host trace
    std::array<char, CommonSettings::CONSOLE_LINE_SIZE> line
        = std::array<char, CommonSettings::CONSOLE_LINE_SIZE>();
    const int lineLength = std::snprintf(line.data(),
                                         line.size(),
                                         "%s %.*s #%-*" PRIu32 " %-*s %-*s len %*" PRIu32
                                         "%s%s%s  CRC %s\r\n",
                                         timeAsText.data(),
                                         static_cast<int>(traceDirectionTypeAsText.size()),
                                         traceDirectionTypeAsText.data(),
                                         static_cast<int>(Settings::TRACED_MESSAGE_COUNTER_WIDTH),
                                         static_cast<uint32_t>(frame.getMessageCounter()),
                                         static_cast<int>(Settings::TRACED_MESSAGE_TYPE_WIDTH),
                                         messageTypeAsText.data(),
                                         static_cast<int>(Settings::TRACED_FLAGS_WIDTH),
                                         flagsAsText.data(),
                                         static_cast<int>(Settings::TRACED_PAYLOAD_LENGTH_WIDTH),
                                         static_cast<uint32_t>(payloadLength),
                                         payloadLength > 0 ? "  " : "",
                                         payloadAsText.data(),
                                         tracedPayloadSize < payloadLength ? " .." : "",
                                         frame.getCRCValid() ? "ok" : "bad");
    this->printLine(line.data(), lineLength);
}

void Console::onReceivedUnframedData(const FrameTracer::TraceDirectionType traceDirectionType,
                                     const uint8_t *data,
                                     const size_t size,
                                     const std::chrono::milliseconds currentTime)
{
    // console must be printed
    if (!this->printed) {
        return;
    }

    // get text as escaped text and other bytes as hex
    std::array<char, CommonSettings::CONSOLE_LINE_SIZE> dataAsText
        = std::array<char, CommonSettings::CONSOLE_LINE_SIZE>();
    size_t dataAsTextLength = 0;
    const bool textData = FrameTracer::isTextData(data, size);
    if (textData) {
        appendEscapedText(data, size, dataAsText.data(), dataAsText.size(), dataAsTextLength);
    } else {
        appendHexText(data, size, dataAsText.data(), dataAsText.size(), dataAsTextLength);
    }

    // get the time and the direction of the bytes
    std::array<char, CommonSettings::CONSOLE_FIELD_SIZE> timeAsText
        = std::array<char, CommonSettings::CONSOLE_FIELD_SIZE>();
    writeTimeText(currentTime, timeAsText);
    const std::string_view traceDirectionTypeAsText = FrameTracer::getTraceDirectionTypeText(
        traceDirectionType);

    // print the bytes with their time and direction
    std::array<char, CommonSettings::CONSOLE_LINE_SIZE> line
        = std::array<char, CommonSettings::CONSOLE_LINE_SIZE>();
    const int lineLength = std::snprintf(line.data(),
                                         line.size(),
                                         "%s %.*s unframed data, %" PRIu32 " %s: %s%.*s%s\r\n",
                                         timeAsText.data(),
                                         static_cast<int>(traceDirectionTypeAsText.size()),
                                         traceDirectionTypeAsText.data(),
                                         static_cast<uint32_t>(size),
                                         size == 1 ? "byte" : "bytes",
                                         textData ? "\"" : "",
                                         static_cast<int>(dataAsTextLength),
                                         dataAsText.data(),
                                         textData ? "\"" : "");
    this->printLine(line.data(), lineLength);
}

void Console::printLine(const char *line, const int lineLength)
{
    // line must be formatted
    if (lineLength <= 0) {
        return;
    }

    // get the notice of the dropped lines
    std::array<char, CommonSettings::CONSOLE_FIELD_SIZE> droppedLineCountAsText
        = std::array<char, CommonSettings::CONSOLE_FIELD_SIZE>();
    size_t droppedLineCountAsTextSize = 0;
    if (this->droppedLineCount > 0) {
        const int droppedLineCountAsTextLength = std::snprintf(droppedLineCountAsText.data(),
                                                               droppedLineCountAsText.size(),
                                                               "%" PRIu32 " console %s dropped\r\n",
                                                               this->droppedLineCount,
                                                               this->droppedLineCount == 1
                                                                   ? "line"
                                                                   : "lines");
        droppedLineCountAsTextSize = std::min(static_cast<size_t>(
                                                  std::max(droppedLineCountAsTextLength, 0)),
                                              droppedLineCountAsText.size() - 1);
    }

    // notice and line must fit into the console output
    const size_t lineSize = std::min(static_cast<size_t>(lineLength),
                                     CommonSettings::CONSOLE_LINE_SIZE - 1);
    if (this->consoleOutput.getAvailableSize() < droppedLineCountAsTextSize + lineSize) {
        ++this->droppedLineCount;
        return;
    }

    // write the notice of the dropped lines
    if (droppedLineCountAsTextSize > 0) {
        this->consoleOutput.writeText(droppedLineCountAsText.data(), droppedLineCountAsTextSize);
    }

    // write the line
    this->consoleOutput.writeText(line, lineSize);
    this->droppedLineCount = 0;
}

} // namespace sercom::Console
