#include "../include/ConsumerPort.h"

#include <algorithm>

#include "../../../firmware/common/include/CommonSettings.h"

namespace sercom::ConsumerPort {

ConsumerPort::ConsumerPort(HardwareSerial &serial)
    : serial(serial)
{}

ConsumerPort::~ConsumerPort() {}

void ConsumerPort::begin()
{
    // set the driver receive buffer size before the serial port begins
    this->serial.setRxBufferSize(SERIAL_RECEIVE_BUFFER_SIZE);

    // open the serial port 8N1 without flow control
    this->serial.begin(CommonSettings::SERIAL_BAUD_RATE, SERIAL_8N1);
}

size_t ConsumerPort::receiveData(uint8_t *data, const size_t size)
{
    // received bytes must wait
    const int availableSize = this->serial.available();
    if (availableSize <= 0) {
        return 0;
    }

    // expose the bytes of at most the size of the data
    const size_t requestedSize = std::min(static_cast<size_t>(availableSize), size);
    return this->serial.readBytes(data, requestedSize);
}

bool ConsumerPort::sendData(const uint8_t *data, const size_t size)
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

size_t ConsumerPort::getAvailableSize() const
{
    // free transmit space must be reported
    const int availableSize = this->serial.availableForWrite();
    if (availableSize <= 0) {
        return 0;
    }

    // expose the free transmit space
    return static_cast<size_t>(availableSize);
}

void ConsumerPort::writeText(const char *text, const size_t size)
{
    // write the characters into the driver transmit buffer
    [[maybe_unused]] const size_t writtenSize = this->serial.write(text, size);
}

} // namespace sercom::ConsumerPort
