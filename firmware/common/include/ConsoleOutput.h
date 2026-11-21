#ifndef SERCOM_CONSOLE_OUTPUT_H
#define SERCOM_CONSOLE_OUTPUT_H

#include <cstddef>

namespace sercom::ConsoleOutput {

class ConsoleOutput
{
public:
    /*
     * Sink of the console text, for example a serial port behind a buffer, implemented once per target.
     */
    ConsoleOutput() = default;
    virtual ~ConsoleOutput();

    // delete copy constructor and copy assignment operator
    ConsoleOutput(const ConsoleOutput &) = delete;
    ConsoleOutput &operator=(const ConsoleOutput &) = delete;

    // delete move constructor and move assignment operator
    ConsoleOutput(ConsoleOutput &&) = delete;
    ConsoleOutput &operator=(ConsoleOutput &&) = delete;

public:
    /*
     * Get the number of characters that the console output takes now without waiting.
     */
    [[nodiscard]] virtual size_t getAvailableSize() const = 0;

    /*
     * Write characters that fit into the available size.
     */
    virtual void writeText(const char *text, size_t size) = 0;
};

} // namespace sercom::ConsoleOutput

#endif // SERCOM_CONSOLE_OUTPUT_H
