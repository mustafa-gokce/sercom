#ifndef SERCOM_CONSOLE_H
#define SERCOM_CONSOLE_H

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/stream_buffer.h>

#include "../../../app/include/Messages.h"
#include "../../../core/include/Clock.h"
#include "../../../core/include/Frame.h"
#include "../../../core/include/FrameTracer.h"

namespace sercom::Console {

class Console final : public FrameTracer::FrameTracerHandler
{
public:
    /*
     * Readable console on a serial port with the boot banner, the traced frames and the log lines.
     * A console that is not printed never touches its serial port.
     * The lines wait in a console buffer until the console task writes them to the serial port.
     */
    explicit Console(HardwareSerial &serial, const Clock::Clock &clock, bool printed);
    ~Console() final;

    // delete copy constructor and copy assignment operator
    Console(const Console &) = delete;
    Console &operator=(const Console &) = delete;

    // delete move constructor and move assignment operator
    Console(Console &&) = delete;
    Console &operator=(Console &&) = delete;

public:
    /*
     * Open the serial port and the console buffer of a printed console and print the boot banner.
     * Return whether the console is open, which a console that is not printed never is.
     */
    [[nodiscard]] bool begin();

    /*
     * Print a log line with its level and time.
     */
    void printLog(Messages::LogLevelType logLevelType, std::string_view text);

    /*
     * Wait for the next bytes in the console buffer and write them to the serial port.
     */
    void processConsole();

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
     * Queue a formatted line when the console buffer has room and count it as dropped otherwise.
     * The number of dropped lines is printed before the next line that fits.
     */
    void printLine(const char *line, int lineLength);

private:
    HardwareSerial &serial;            // reference for serial port
    const Clock::Clock &clock;         // reference for clock
    const bool printed;                // console is printed
    StreamBufferHandle_t streamBuffer; // console buffer of the lines
    uint32_t droppedLineCount;         // number of lines without room in the console buffer
};

} // namespace sercom::Console

#endif // SERCOM_CONSOLE_H
