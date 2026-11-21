#ifndef SERCOM_TRANSPORT_H
#define SERCOM_TRANSPORT_H

#include <cstddef>
#include <cstdint>

namespace sercom::Transport {

class Transport
{
public:
    /*
     * Byte sink that takes the frames of the link layer, for example a serial port.
     */
    Transport() = default;
    virtual ~Transport();

    // delete copy constructor and copy assignment operator
    Transport(const Transport &) = delete;
    Transport &operator=(const Transport &) = delete;

    // delete move constructor and move assignment operator
    Transport(Transport &&) = delete;
    Transport &operator=(Transport &&) = delete;

public:
    /*
     * Send every byte of the data or none.
     * Get false when the data could not be sent.
     */
    [[nodiscard]] virtual bool sendData(const uint8_t *data, size_t size) = 0;
};

} // namespace sercom::Transport

#endif // SERCOM_TRANSPORT_H
