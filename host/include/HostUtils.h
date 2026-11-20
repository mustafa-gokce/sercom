#ifndef SERCOM_HOST_UTILS_H
#define SERCOM_HOST_UTILS_H

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace sercom::HostUtils {

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
 * Get the text padded with spaces on the left up to the width.
 */
[[nodiscard]] std::string getRightJustifiedText(std::string_view text, size_t width);

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

} // namespace sercom::HostUtils

#endif // SERCOM_HOST_UTILS_H
