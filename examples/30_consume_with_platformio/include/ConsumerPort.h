#ifndef SERCOM_CONSUMER_PORT_H
#define SERCOM_CONSUMER_PORT_H

#include <cstddef>
#include <cstdint>
#include <string_view>

#include <Arduino.h>

#include "../../../core/include/Transport.h"
#include "../../../firmware/common/include/ConsoleOutput.h"

namespace sercom::ConsumerPort {

constexpr std::string_view PORT_NAME = "UART0";         // name of the serial port on the USB bridge
constexpr size_t SERIAL_RECEIVE_BUFFER_SIZE = 1 * 1024; // driver receive buffer size in bytes
constexpr bool CONSOLE_PRINTED = false;                 // console printed on the serial port

class ConsumerPort final : public Transport::Transport, public ConsoleOutput::ConsoleOutput
{
public:
    /*
     * Serial port of the consumer firmware, the transport of the protocol and the output of its console.
     * The protocol keeps the serial port to itself, so the console is not printed and never writes to it.
     */
    explicit ConsumerPort(HardwareSerial &serial);
    ~ConsumerPort() final;

    // delete copy constructor and copy assignment operator
    ConsumerPort(const ConsumerPort &) = delete;
    ConsumerPort &operator=(const ConsumerPort &) = delete;

    // delete move constructor and move assignment operator
    ConsumerPort(ConsumerPort &&) = delete;
    ConsumerPort &operator=(ConsumerPort &&) = delete;

public:
    /*
     * Open the serial port 8N1 without flow control at the baud rate of the protocol.
     */
    void begin();

    /*
     * Read the waiting bytes of the serial port into the data, at most its size.
     * Get the number of bytes read, 0 when none waits.
     */
    [[nodiscard]] size_t receiveData(uint8_t *data, size_t size);

public:
    /*
     * Send every byte of the data and block while the driver transmit buffer is full.
     */
    [[nodiscard]] bool sendData(const uint8_t *data, size_t size) final;

    /*
     * Get the number of characters that the driver transmit buffer takes now without waiting.
     */
    [[nodiscard]] size_t getAvailableSize() const final;

    /*
     * Write characters into the driver transmit buffer.
     */
    void writeText(const char *text, size_t size) final;

private:
    HardwareSerial &serial; // reference for serial port
};

} // namespace sercom::ConsumerPort

#endif // SERCOM_CONSUMER_PORT_H
