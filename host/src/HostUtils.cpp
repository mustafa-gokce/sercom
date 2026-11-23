#include "../include/HostUtils.h"

#include <algorithm>
#include <csignal>
#include <cstdio>
#include <iomanip>
#include <limits>
#include <sstream>

#include <QDebug>
#include <QString>
#include <QTextStream>

#include "../../app/include/Messages.h"
#include "../../core/include/Settings.h"

#include "../include/SerialPort.h"

namespace sercom::HostUtils {

namespace {

/*
 * Running flag that the stop handler clears.
 */
volatile std::sig_atomic_t running = 1;

/*
 * Clear the running flag, which is the only work a signal handler may do here.
 */
void handleStopSignal([[maybe_unused]] const int signalNumber)
{
    // clear the running flag
    running = 0;
}

/*
 * Get the time since the first log line as timestamp text.
 */
[[nodiscard]] std::string getTimestampText()
{
    // expose the time since the first log line as timestamp text
    return getTimeText(getElapsedTime());
}

} // namespace

std::string toHexString(const uint8_t *data, const size_t size)
{
    // iterate over the bytes
    std::string dataAsHexString;
    std::for_each(data, data + size, [&dataAsHexString](const uint8_t byte) {
        // separate the bytes by a space
        if (!dataAsHexString.empty()) {
            dataAsHexString.push_back(' ');
        }

        // add the two hex digits of the byte
        dataAsHexString += getHexText(byte, 2);
    });

    // expose the hex string
    return dataAsHexString;
}

std::string toEscapedString(const uint8_t *data, const size_t size)
{
    // open the quotes
    std::string dataAsEscapedString = "\"";

    // iterate over the bytes
    std::for_each(data, data + size, [&dataAsEscapedString](const uint8_t byte) {
        // escape a carriage return by its name
        if (byte == '\r') {
            dataAsEscapedString += "\\r";
            return;
        }

        // escape a line feed by its name
        if (byte == '\n') {
            dataAsEscapedString += "\\n";
            return;
        }

        // escape a tab by its name
        if (byte == '\t') {
            dataAsEscapedString += "\\t";
            return;
        }

        // escape a quote or a backslash by a backslash
        if (byte == '"' || byte == '\\') {
            dataAsEscapedString.push_back('\\');
            dataAsEscapedString.push_back(static_cast<char>(byte));
            return;
        }

        // escape a byte outside the printable characters by its hex value
        static constexpr size_t byteDigitCount = 2;
        if (byte < ' ' || byte > '~') {
            dataAsEscapedString += "\\x" + getHexText(byte, byteDigitCount);
            return;
        }

        // add a printable character as it is
        dataAsEscapedString.push_back(static_cast<char>(byte));
    });

    // close the quotes
    dataAsEscapedString.push_back('"');

    // expose the escaped string
    return dataAsEscapedString;
}

std::string getHexText(const uint32_t value, const size_t digitCount)
{
    // digit count must fit the value
    static constexpr size_t maximumDigitCount = 2 * sizeof(uint32_t);
    if (digitCount > maximumDigitCount) {
        return {};
    }

    // add the digits from the highest one
    static constexpr std::string_view hexDigits = "0123456789ABCDEF";
    std::string valueAsHexText;
    for (size_t index = digitCount; index > 0; --index) {
        const uint32_t digit = (value >> (4 * (index - 1))) & 0xF;
        valueAsHexText.push_back(hexDigits[digit]);
    }

    // expose the hex text
    return valueAsHexText;
}

std::string getDecimalText(const double value, const int decimalCount)
{
    // write the value with a fixed number of decimals
    std::ostringstream valueAsStream;
    valueAsStream << std::fixed << std::setprecision(decimalCount) << value;

    // expose the decimal text
    return valueAsStream.str();
}

std::string getTemperatureText(const int16_t temperature)
{
    // temperature must have a reading
    if (temperature == Messages::UNAVAILABLE_TEMPERATURE) {
        return "no reading";
    }

    // expose the temperature in degrees Celsius with two decimals
    static constexpr double temperatureStepsPerDegree = 100.0;
    static constexpr int decimalCount = 2;
    const double temperatureAsDegrees = static_cast<double>(temperature)
                                        / temperatureStepsPerDegree;
    return getDecimalText(temperatureAsDegrees, decimalCount) + " C";
}

std::chrono::milliseconds getElapsedTime()
{
    // expose the time since the first call
    static const SteadyClock steadyClock;
    return steadyClock.getTime();
}

std::string getTimeText(const std::chrono::milliseconds time)
{
    // get the seconds with millisecond decimals
    const double seconds = static_cast<double>(time.count()) / 1000.0;

    // expose the seconds in brackets
    static constexpr int decimalCount = 3;
    static constexpr size_t timeWidth = 10;
    return "[" + getRightJustifiedText(getDecimalText(seconds, decimalCount), timeWidth) + "]";
}

std::string getLeftJustifiedText(const std::string_view text, const size_t width)
{
    // pad the text on the right
    std::string justifiedText(text);
    justifiedText.append(width - std::min(width, justifiedText.size()), ' ');

    // expose the justified text
    return justifiedText;
}

std::string getRightJustifiedText(const std::string_view text, const size_t width)
{
    // pad the text on the left
    std::string justifiedText(text);
    justifiedText.insert(0, width - std::min(width, justifiedText.size()), ' ');

    // expose the justified text
    return justifiedText;
}

std::string getReportLine(const std::string_view name, const std::string_view value)
{
    // expose the name and the value in their columns
    static constexpr size_t nameWidth = 28;
    static constexpr size_t valueWidth = 10;
    return "  " + getLeftJustifiedText(name, nameWidth) + " "
           + getRightJustifiedText(value, valueWidth);
}

std::string getSerialPortText(const std::string_view portName, const uint32_t baudRate)
{
    // expose the serial port name and the baud rate
    return std::string(portName) + ", " + std::to_string(baudRate);
}

bool parseUint32(const std::string_view text, const uint32_t maximum, uint32_t &value)
{
    // get the digits and their base
    const bool hexadecimal = text.size() > 2 && text[0] == '0'
                             && (text[1] == 'x' || text[1] == 'X');
    const std::string_view digits = hexadecimal ? text.substr(2) : text;
    const int base = hexadecimal ? 16 : 10;

    // digits must be digits of the base only
    static constexpr std::string_view decimalDigits = "0123456789";
    static constexpr std::string_view hexadecimalDigits = "0123456789ABCDEFabcdef";
    const std::string_view baseDigits = hexadecimal ? hexadecimalDigits : decimalDigits;
    if (digits.empty() || digits.find_first_not_of(baseDigits) != std::string_view::npos) {
        return false;
    }

    // digits must form one number of 64 bits
    bool converted = false;
    const uint64_t textAsNumber = QString::fromUtf8(digits.data(), static_cast<int>(digits.size()))
                                      .toULongLong(&converted, base);
    if (!converted) {
        return false;
    }

    // number must not exceed the maximum
    if (textAsNumber > maximum) {
        return false;
    }

    // set the value
    value = static_cast<uint32_t>(textAsNumber);

    // text parsed successfully
    return true;
}

bool parseArgument(const std::string_view argument,
                   const std::string_view argumentName,
                   const uint32_t minimum,
                   const uint32_t maximum,
                   uint32_t &value)
{
    // argument must be a number within the limits
    uint32_t argumentAsNumber = 0;
    if (!parseUint32(argument, maximum, argumentAsNumber) || argumentAsNumber < minimum) {
        // warn the user
        const std::string warningMessage = "Invalid " + std::string(argumentName) + " argument "
                                           + std::string(argument) + ", expected a number from "
                                           + std::to_string(minimum) + " to "
                                           + std::to_string(maximum);
        logWarning(warningMessage);

        // do not proceed further
        return false;
    }

    // set the value
    value = argumentAsNumber;

    // argument parsed successfully
    return true;
}

bool parseDurationArgument(const std::string_view argument,
                           const std::string_view argumentName,
                           const std::chrono::milliseconds unit,
                           const std::chrono::milliseconds maximum,
                           std::chrono::milliseconds &duration)
{
    // argument must count the units up to the maximum
    uint32_t unitCount = 0;
    if (!parseArgument(argument, argumentName, 0, static_cast<uint32_t>(maximum / unit), unitCount)) {
        return false;
    }

    // set the duration
    duration = unitCount * unit;

    // argument parsed successfully
    return true;
}

bool parseBaudRateArgument(const std::string_view argument, uint32_t &baudRate)
{
    // argument must be a baud rate that the serial port supports
    uint32_t argumentAsNumber = 0;
    if (!parseUint32(argument, std::numeric_limits<uint32_t>::max(), argumentAsNumber)
        || !SerialPort::isBaudRateSupported(argumentAsNumber)) {
        // warn the user
        const std::string warningMessage = "Invalid baud rate argument " + std::string(argument)
                                           + ", expected a baud rate that the serial port supports";
        logWarning(warningMessage);

        // do not proceed further
        return false;
    }

    // set the baud rate
    baudRate = argumentAsNumber;

    // argument parsed successfully
    return true;
}

std::vector<std::string> getLinkStatisticsLines(const Link::LinkStatistics &linkStatistics)
{
    // get a report line of a counter
    const auto getCounterLine = [](const std::string_view name, const uint64_t value) {
        return getReportLine(name, std::to_string(value));
    };

    // add the link counters
    std::vector<std::string> linkStatisticsLines;
    linkStatisticsLines.push_back(
        getCounterLine("frames sent", linkStatistics.outgoingFrameCounter));
    linkStatisticsLines.push_back(
        getCounterLine("frames received", linkStatistics.incomingFrameCounter));
    linkStatisticsLines.push_back(
        getCounterLine("messages delivered", linkStatistics.deliveredMessageCounter));
    linkStatisticsLines.push_back(
        getCounterLine("acknowledgements sent", linkStatistics.outgoingAcknowledgementCounter));
    linkStatisticsLines.push_back(
        getCounterLine("acknowledgements received", linkStatistics.incomingAcknowledgementCounter));
    linkStatisticsLines.push_back(
        getCounterLine("rejections sent", linkStatistics.outgoingRejectionCounter));
    linkStatisticsLines.push_back(
        getCounterLine("rejections received", linkStatistics.incomingRejectionCounter));
    linkStatisticsLines.push_back(
        getCounterLine("retransmissions", linkStatistics.retransmissionCounter));
    linkStatisticsLines.push_back(
        getCounterLine("deliveries failed", linkStatistics.failedDeliveryCounter));
    linkStatisticsLines.push_back(
        getCounterLine("duplicates dropped", linkStatistics.duplicateFrameCounter));
    linkStatisticsLines.push_back(getCounterLine("unexpected acknowledgements",
                                                 linkStatistics.unexpectedAcknowledgementCounter));
    linkStatisticsLines.push_back(
        getCounterLine("transport errors", linkStatistics.transportErrorCounter));

    // add the decoder counters
    const FrameDecoder::DecoderStatistics &decoderStatistics = linkStatistics.decoderStatistics;
    linkStatisticsLines.push_back(
        getCounterLine("decoder bytes received", decoderStatistics.incomingByteCounter));
    linkStatisticsLines.push_back(
        getCounterLine("decoder bytes discarded", decoderStatistics.discardedByteCounter));
    linkStatisticsLines.push_back(
        getCounterLine("decoder frames decoded", decoderStatistics.incomingFrameCounter));
    linkStatisticsLines.push_back(
        getCounterLine("decoder invalid versions", decoderStatistics.invalidVersionCounter));
    linkStatisticsLines.push_back(
        getCounterLine("decoder payloads too long", decoderStatistics.payloadTooLongCounter));
    linkStatisticsLines.push_back(
        getCounterLine("decoder CRC mismatches", decoderStatistics.crcMismatchCounter));
    linkStatisticsLines.push_back(
        getCounterLine("decoder inter byte timeouts", decoderStatistics.interByteTimeoutCounter));

    // expose the link statistics lines
    return linkStatisticsLines;
}

std::string_view getMessageStatusTypeText(const Definitions::MessageStatusType messageStatusType)
{
    switch (messageStatusType) {
    case Definitions::MessageStatusType::ACCEPTED: {
        return {"ACCEPTED"};
    }
    case Definitions::MessageStatusType::UNSUPPORTED_MESSAGE_TYPE: {
        return {"UNSUPPORTED_MESSAGE_TYPE"};
    }
    case Definitions::MessageStatusType::MALFORMED_PAYLOAD: {
        return {"MALFORMED_PAYLOAD"};
    }
    case Definitions::MessageStatusType::BUSY: {
        return {"BUSY"};
    }
    case Definitions::MessageStatusType::INVALID_ARGUMENT: {
        return {"INVALID_ARGUMENT"};
    }
    case Definitions::MessageStatusType::UNREACHABLE: {
        return {"UNREACHABLE"};
    }
    case Definitions::MessageStatusType::EXPIRED: {
        return {"EXPIRED"};
    }
    case Definitions::MessageStatusType::UNKNOWN:
    default: {
        return {"UNKNOWN"};
    }
    }
}

std::string getFrameFlagsText(const uint8_t flags)
{
    // add the name of every set flag
    std::string flagsAsText;
    const auto addFlagName = [flags, &flagsAsText](const Definitions::FrameFlagType frameFlagType) {
        // flag must be set
        if (!Definitions::isFrameFlagSet(flags, frameFlagType)) {
            return;
        }

        // separate the flag names by a bar
        if (!flagsAsText.empty()) {
            flagsAsText += "|";
        }

        // add the flag name
        flagsAsText += Definitions::getFrameFlagTypeText(frameFlagType);
    };
    addFlagName(Definitions::FrameFlagType::ACK_REQUEST);
    addFlagName(Definitions::FrameFlagType::ACK);
    addFlagName(Definitions::FrameFlagType::NACK);
    addFlagName(Definitions::FrameFlagType::RETRANSMISSION);
    addFlagName(Definitions::FrameFlagType::ROUTE_CAPABLE);

    // name a FLAGS field without a set flag
    if (flagsAsText.empty()) {
        return "NONE";
    }

    // expose the flag names
    return flagsAsText;
}

std::string getTracedFrameLine(const FrameTracer::TraceDirectionType traceDirectionType,
                               const Frame::Frame &frame,
                               const std::chrono::milliseconds currentTime)
{
    // get the message type by its name
    const Messages::MessageType messageType = Messages::toMessageType(frame.getMessageType());
    std::string messageTypeAsText(Messages::getMessageTypeText(messageType));

    // show an unknown message type by its number
    static constexpr size_t messageTypeDigitCount = 2 * Definitions::MESSAGE_TYPE_SIZE;
    if (messageType == Messages::MessageType::UNKNOWN) {
        messageTypeAsText = "0x" + getHexText(frame.getMessageType(), messageTypeDigitCount);
    }

    // get the payload up to the traced payload limit
    const size_t payloadLength = frame.getPayloadLength();
    const size_t tracedPayloadSize = std::min(payloadLength, Settings::MAXIMUM_TRACED_PAYLOAD_SIZE);
    std::string payloadAsText = toHexString(frame.getPayload(), tracedPayloadSize);
    if (tracedPayloadSize < payloadLength) {
        payloadAsText += " ..";
    }

    // get the fields as text in their columns
    std::string line
        = getTimeText(currentTime) + " "
          + std::string(FrameTracer::getTraceDirectionTypeText(traceDirectionType)) + " #"
          + getLeftJustifiedText(std::to_string(frame.getMessageCounter()),
                                 Settings::TRACED_MESSAGE_COUNTER_WIDTH)
          + " " + getLeftJustifiedText(messageTypeAsText, Settings::TRACED_MESSAGE_TYPE_WIDTH) + " "
          + getLeftJustifiedText(getFrameFlagsText(frame.getFlags()), Settings::TRACED_FLAGS_WIDTH)
          + " len "
          + getRightJustifiedText(std::to_string(payloadLength),
                                  Settings::TRACED_PAYLOAD_LENGTH_WIDTH);

    // add the payload of a frame with a payload
    if (!payloadAsText.empty()) {
        line += "  " + payloadAsText;
    }

    // add the result of the CRC32 check
    line += frame.getCRCValid() ? "  CRC ok" : "  CRC bad";

    // add the route of a routed frame
    if (frame.getRouted()) {
        line += "  route " + std::to_string(frame.getSourceAddress()) + ">"
                + std::to_string(frame.getDestinationAddress()) + " ttl "
                + std::to_string(frame.getTimeToLive());
    }

    // expose the line
    return line;
}

std::string getUnframedDataLine(const FrameTracer::TraceDirectionType traceDirectionType,
                                const uint8_t *data,
                                const size_t size,
                                const std::chrono::milliseconds currentTime)
{
    // get text as escaped text and other bytes as hex
    const std::string dataAsText = FrameTracer::isTextData(data, size) ? toEscapedString(data, size)
                                                                       : toHexString(data, size);

    // get the size with its unit in the singular for one byte
    const std::string sizeAsText = std::to_string(size) + (size == 1 ? " byte" : " bytes");

    // expose the bytes behind the time and the direction
    return getTimeText(currentTime) + " "
           + std::string(FrameTracer::getTraceDirectionTypeText(traceDirectionType))
           + " unframed data, " + sizeAsText + ": " + dataAsText;
}

void printLine(const std::string &line)
{
    // print the line at once
    QTextStream(stdout) << QString::fromStdString(line) << endl;
}

void logDebug(const std::string &debugMessage)
{
    // print the debug message with its timestamp
    const QString debugLine = QString::fromStdString(getTimestampText() + " " + debugMessage);
    qDebug().noquote() << debugLine;
}

void logWarning(const std::string &warningMessage)
{
    // print the warning message with its timestamp
    const QString warningLine = QString::fromStdString(getTimestampText() + " WARNING "
                                                       + warningMessage);
    qWarning().noquote() << warningLine;
}

void installStopHandlers()
{
    // install the interrupt handler
    if (std::signal(SIGINT, handleStopSignal) == SIG_ERR) {
        logWarning("Can not install the interrupt handler");
    }

    // NOTE: no other process can send the terminate signal on Windows
    if (std::signal(SIGTERM, handleStopSignal) == SIG_ERR) {
        logWarning("Can not install the terminate handler");
    }
}

bool getRunning()
{
    // expose whether no stop signal has arrived
    return running != 0;
}

SteadyClock::SteadyClock()
    : elapsedTimer()
{
    // start the time at zero
    this->elapsedTimer.start();
}

SteadyClock::~SteadyClock() {}

std::chrono::milliseconds SteadyClock::getTime() const
{
    // expose the time since construction
    return std::chrono::milliseconds(this->elapsedTimer.elapsed());
}

} // namespace sercom::HostUtils
