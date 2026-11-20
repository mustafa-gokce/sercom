#ifndef SERCOM_HOST_UTILS_H
#define SERCOM_HOST_UTILS_H

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "../../core/include/Clock.h"

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
     * Steady clock of the host that starts at zero on construction.
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
    const std::chrono::milliseconds startTimestamp; // steady time at construction
};

} // namespace sercom::HostUtils

#endif // SERCOM_HOST_UTILS_H
