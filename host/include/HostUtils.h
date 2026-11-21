#ifndef SERCOM_HOST_UTILS_H
#define SERCOM_HOST_UTILS_H

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include <QElapsedTimer>

#include "../../core/include/Clock.h"
#include "../../core/include/Definitions.h"
#include "../../core/include/Frame.h"
#include "../../core/include/FrameTracer.h"
#include "../../core/include/Link.h"

namespace sercom::HostUtils {

/*
 * Convert bytes to space separated upper case hex text.
 */
[[nodiscard]] std::string toHexString(const uint8_t *data, size_t size);

/*
 * Convert bytes to printable text in quotes with escapes for the other bytes.
 */
[[nodiscard]] std::string toEscapedString(const uint8_t *data, size_t size);

/*
 * Get a value as upper case hex text with the digit count.
 */
[[nodiscard]] std::string getHexText(uint32_t value, size_t digitCount);

/*
 * Get a value as decimal text with the decimal count.
 */
[[nodiscard]] std::string getDecimalText(double value, int decimalCount);

/*
 * Get the time since the first log line in milliseconds, the time base of every printed time.
 * A call before the first log line starts the time base itself.
 */
[[nodiscard]] std::chrono::milliseconds getElapsedTime();

/*
 * Get a time as seconds with millisecond decimals in brackets.
 */
[[nodiscard]] std::string getTimeText(std::chrono::milliseconds time);

/*
 * Get the text padded with spaces on the right up to the width.
 */
[[nodiscard]] std::string getLeftJustifiedText(std::string_view text, size_t width);

/*
 * Get the text padded with spaces on the left up to the width.
 */
[[nodiscard]] std::string getRightJustifiedText(std::string_view text, size_t width);

/*
 * Get a report line with the name and the value in aligned columns.
 */
[[nodiscard]] std::string getReportLine(std::string_view name, std::string_view value);

/*
 * Get the serial port name and the baud rate as text.
 */
[[nodiscard]] std::string getSerialPortText(std::string_view portName, uint32_t baudRate);

/*
 * Parse a decimal or 0x prefixed hexadecimal text to a 32 bit integer up to the maximum.
 * Get false when the text is no such number.
 */
[[nodiscard]] bool parseUint32(std::string_view text, uint32_t maximum, uint32_t &value);

/*
 * Parse a command line argument to a 32 bit integer within the minimum and the maximum.
 * Warn the user and get false when the argument is no such number.
 */
[[nodiscard]] bool parseArgument(std::string_view argument,
                                 std::string_view argumentName,
                                 uint32_t minimum,
                                 uint32_t maximum,
                                 uint32_t &value);

/*
 * Parse a command line argument that counts units to a duration up to the maximum.
 * Warn the user and get false when the argument is no such duration.
 */
[[nodiscard]] bool parseDurationArgument(std::string_view argument,
                                         std::string_view argumentName,
                                         std::chrono::milliseconds unit,
                                         std::chrono::milliseconds maximum,
                                         std::chrono::milliseconds &duration);

/*
 * Parse a command line argument to a baud rate that the serial port supports.
 * Warn the user and get false when the argument is no such baud rate.
 */
[[nodiscard]] bool parseBaudRateArgument(std::string_view argument, uint32_t &baudRate);

/*
 * Get the link and decoder statistics as report lines.
 */
[[nodiscard]] std::vector<std::string> getLinkStatisticsLines(
    const Link::LinkStatistics &linkStatistics);

/*
 * Get message status type as text.
 */
[[nodiscard]] std::string_view getMessageStatusTypeText(
    Definitions::MessageStatusType messageStatusType);

/*
 * Get the names of the set flags of a FLAGS field as text.
 */
[[nodiscard]] std::string getFrameFlagsText(uint8_t flags);

/*
 * Get a traced frame as one line of text.
 */
[[nodiscard]] std::string getTracedFrameLine(FrameTracer::TraceDirectionType traceDirectionType,
                                             const Frame::Frame &frame,
                                             std::chrono::milliseconds currentTime);

/*
 * Get traced bytes outside a frame as one line of text.
 * Text stands escaped in quotes and other bytes stand in hex.
 */
[[nodiscard]] std::string getUnframedDataLine(FrameTracer::TraceDirectionType traceDirectionType,
                                              const uint8_t *data,
                                              size_t size,
                                              std::chrono::milliseconds currentTime);

/*
 * Print a result line to standard output.
 */
void printLine(const std::string &line);

/*
 * Print a debug message with a timestamp to standard error.
 */
void logDebug(const std::string &debugMessage);

/*
 * Print a warning message with a timestamp to standard error.
 */
void logWarning(const std::string &warningMessage);

/*
 * Install the interrupt and terminate handlers that clear the running flag.
 */
void installStopHandlers();

/*
 * Get whether no stop signal has arrived yet.
 */
[[nodiscard]] bool getRunning();

class SteadyClock final : public Clock::Clock
{
public:
    /*
     * Steady clock of the host on a QElapsedTimer that starts at zero on construction.
     */
    SteadyClock();
    ~SteadyClock() final;

    // delete copy constructor and copy assignment operator
    SteadyClock(const SteadyClock &) = delete;
    SteadyClock &operator=(const SteadyClock &) = delete;

    // delete move constructor and move assignment operator
    SteadyClock(SteadyClock &&) = delete;
    SteadyClock &operator=(SteadyClock &&) = delete;

public:
    /*
     * Get the time since construction in milliseconds.
     */
    [[nodiscard]] std::chrono::milliseconds getTime() const final;

private:
    QElapsedTimer elapsedTimer; // monotonic timer started at construction
};

} // namespace sercom::HostUtils

#endif // SERCOM_HOST_UTILS_H
