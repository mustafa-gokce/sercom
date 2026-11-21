#ifndef SERCOM_UART_TRANSPORT_H
#define SERCOM_UART_TRANSPORT_H

#include <cstddef>
#include <cstdint>

#include <Arduino.h>

#include "../../../core/include/Transport.h"

namespace sercom::UartTransport {

class UartTransport final : public Transport::Transport
{
public:
    /*
     * Transport writing frames to an Arduino hardware serial port.
     */
    explicit UartTransport(HardwareSerial &serial);
    ~UartTransport() final;

    // delete copy constructor and copy assignment operator
    UartTransport(const UartTransport &) = delete;
    UartTransport &operator=(const UartTransport &) = delete;

    // delete move constructor and move assignment operator
    UartTransport(UartTransport &&) = delete;
    UartTransport &operator=(UartTransport &&) = delete;

public:
    /*
     * Send every byte of the data and block while the driver transmit buffer is full.
     */
    [[nodiscard]] bool sendData(const uint8_t *data, size_t size) final;

private:
    HardwareSerial &serial; // reference for serial port
};

} // namespace sercom::UartTransport

#endif // SERCOM_UART_TRANSPORT_H
