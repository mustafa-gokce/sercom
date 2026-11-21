#ifndef SERCOM_ESP32_CONSOLE_OUTPUT_H
#define SERCOM_ESP32_CONSOLE_OUTPUT_H

#include <cstddef>

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/stream_buffer.h>

#include "../../common/include/ConsoleOutput.h"

namespace sercom::Esp32ConsoleOutput {

class Esp32ConsoleOutput final : public ConsoleOutput::ConsoleOutput
{
public:
    /*
     * Console output on a serial port with the boot banner and a console buffer of its own.
     * The UART driver overstates its free transmit space and blocks when it is full.
     * The text waits in the console buffer until the console task writes it to the serial port.
     * A console output that is not printed never touches its serial port.
     */
    explicit Esp32ConsoleOutput(HardwareSerial &serial, bool printed);
    ~Esp32ConsoleOutput() final;

    // delete copy constructor and copy assignment operator
    Esp32ConsoleOutput(const Esp32ConsoleOutput &) = delete;
    Esp32ConsoleOutput &operator=(const Esp32ConsoleOutput &) = delete;

    // delete move constructor and move assignment operator
    Esp32ConsoleOutput(Esp32ConsoleOutput &&) = delete;
    Esp32ConsoleOutput &operator=(Esp32ConsoleOutput &&) = delete;

public:
    /*
     * Open the serial port and the console buffer of a printed console output and print the boot banner.
     * Return whether the console output is open, which a console output that is not printed never is.
     */
    [[nodiscard]] bool begin();

    /*
     * Wait for the next bytes in the console buffer and write them to the serial port.
     */
    void processConsole();

public:
    /*
     * Get the free space of the console buffer in characters.
     */
    [[nodiscard]] size_t getAvailableSize() const final;

    /*
     * Write characters into the console buffer for the console task.
     */
    void writeText(const char *text, size_t size) final;

private:
    /*
     * Write a formatted line of the boot banner into the console buffer.
     */
    void writeLine(const char *line, int lineLength);

private:
    HardwareSerial &serial;            // reference for serial port
    const bool printed;                // console output is printed
    StreamBufferHandle_t streamBuffer; // console buffer of the text
};

} // namespace sercom::Esp32ConsoleOutput

#endif // SERCOM_ESP32_CONSOLE_OUTPUT_H
