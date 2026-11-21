#include "../include/UartTransport.h"

namespace sercom::UartTransport {

UartTransport::UartTransport(HardwareSerial &serial)
    : serial(serial)
{}

UartTransport::~UartTransport() {}

bool UartTransport::sendData(const uint8_t *data, const size_t size)
{
    // serial port must be open
    if (!static_cast<bool>(this->serial)) {
        return false;
    }

    // every byte must be written into the driver transmit buffer
    const size_t writtenSize = this->serial.write(data, size);
    if (writtenSize != size) {
        return false;
    }

    // data sent successfully
    return true;
}

} // namespace sercom::UartTransport
