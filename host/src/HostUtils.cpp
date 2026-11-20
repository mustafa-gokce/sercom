#include "../include/HostUtils.h"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace sercom::HostUtils {

namespace {

/*
 * Get the steady time of the host.
 */
[[nodiscard]] std::chrono::milliseconds getSteadyTime()
{
    // expose the steady time in milliseconds
    const std::chrono::steady_clock::duration steadyTime = std::chrono::steady_clock::now()
                                                               .time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(steadyTime);
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

std::chrono::milliseconds getElapsedTime()
{
    // expose the time since the first call
    static const std::chrono::milliseconds startTimestamp = getSteadyTime();
    return getSteadyTime() - startTimestamp;
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

std::string getRightJustifiedText(const std::string_view text, const size_t width)
{
    // pad the text on the left
    std::string justifiedText(text);
    justifiedText.insert(0, width - std::min(width, justifiedText.size()), ' ');

    // expose the justified text
    return justifiedText;
}

void printLine(const std::string &line)
{
    // print the line at once
    std::cout << line << '\n' << std::flush;
}

void logDebug(const std::string &debugMessage)
{
    // print the debug message with its timestamp
    std::clog << getTimestampText() << ' ' << debugMessage << '\n';
}

void logWarning(const std::string &warningMessage)
{
    // print the warning message with its timestamp
    std::clog << getTimestampText() << " WARNING " << warningMessage << '\n';
}

} // namespace sercom::HostUtils
