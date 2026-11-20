#include "../include/HostUtils.h"

#include <algorithm>
#include <csignal>
#include <iomanip>
#include <iostream>
#include <sstream>

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

void installStopHandlers()
{
    // install the interrupt handler
    if (std::signal(SIGINT, handleStopSignal) == SIG_ERR) {
        logWarning("Can not install the interrupt handler");
    }

    // install the terminate handler
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
    : startTimestamp(getSteadyTime())
{}

SteadyClock::~SteadyClock() {}

std::chrono::milliseconds SteadyClock::getTime() const
{
    // expose the time since construction
    return getSteadyTime() - this->startTimestamp;
}

} // namespace sercom::HostUtils
