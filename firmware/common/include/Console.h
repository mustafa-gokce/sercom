#ifndef SERCOM_CONSOLE_H
#define SERCOM_CONSOLE_H

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include "../../../app/include/Messages.h"
#include "../../../core/include/Clock.h"
#include "../../../core/include/Frame.h"
#include "../../../core/include/FrameTracer.h"

#include "../include/ConsoleOutput.h"

namespace sercom::Console {

class Console final : public FrameTracer::FrameTracerHandler
{
public:
    /*
     * Readable console with the traced frames and the log lines in the columns of the host trace.
     * A console that is not printed never touches its console output.
     * A line without room in the console output is dropped and counted.
     */
    explicit Console(ConsoleOutput::ConsoleOutput &consoleOutput,
                     const Clock::Clock &clock,
                     bool printed);
    ~Console() final;

    // delete copy constructor and copy assignment operator
    Console(const Console &) = delete;
    Console &operator=(const Console &) = delete;

    // delete move constructor and move assignment operator
    Console(Console &&) = delete;
    Console &operator=(Console &&) = delete;

public:
    /*
     * Print a log line with its level and time.
     */
    void printLog(Messages::LogLevelType logLevelType, std::string_view text);

public:
    /*
     * Get whether the console is printed.
     */
    [[nodiscard]] bool getPrinted() const;

private:
    /*
     * Listen for a traced frame and print it on one line.
     */
    void onReceivedTracedFrame(FrameTracer::TraceDirectionType traceDirectionType,
                               const Frame::Frame &frame,
                               std::chrono::milliseconds currentTime) final;

    /*
     * Listen for traced bytes outside a frame and print them on one line.
     * Text stands escaped in quotes and other bytes stand in hex.
     */
    void onReceivedUnframedData(FrameTracer::TraceDirectionType traceDirectionType,
                                const uint8_t *data,
                                size_t size,
                                std::chrono::milliseconds currentTime) final;

private:
    /*
     * Write a formatted line when the console output has room and count it as dropped otherwise.
     * The number of dropped lines is printed before the next line that fits.
     */
    void printLine(const char *line, int lineLength);

private:
    ConsoleOutput::ConsoleOutput &consoleOutput; // reference for console output
    const Clock::Clock &clock;                   // reference for clock
    const bool printed;                          // console is printed
    uint32_t droppedLineCount; // number of lines without room in the console output
};

} // namespace sercom::Console

#endif // SERCOM_CONSOLE_H
