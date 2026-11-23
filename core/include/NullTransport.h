#ifndef SERCOM_NULL_TRANSPORT_H
#define SERCOM_NULL_TRANSPORT_H

#include <cstddef>
#include <cstdint>

#include "../include/Transport.h"

namespace sercom::NullTransport {

class NullTransport final : public Transport::Transport
{
public:
    /*
     * Transport of a port without a cable that takes no byte.
     */
    NullTransport() = default;
    ~NullTransport() final;

    // delete copy constructor and copy assignment operator
    NullTransport(const NullTransport &) = delete;
    NullTransport &operator=(const NullTransport &) = delete;

    // delete move constructor and move assignment operator
    NullTransport(NullTransport &&) = delete;
    NullTransport &operator=(NullTransport &&) = delete;

public:
    /*
     * Send no byte of the data and get false.
     */
    [[nodiscard]] bool sendData(const uint8_t *data, size_t size) final;
};

} // namespace sercom::NullTransport

#endif // SERCOM_NULL_TRANSPORT_H
