#include "../include/NullTransport.h"

namespace sercom::NullTransport {

NullTransport::~NullTransport() {}

bool NullTransport::sendData([[maybe_unused]] const uint8_t *data,
                             [[maybe_unused]] const size_t size)
{
    // take no byte
    return false;
}

} // namespace sercom::NullTransport
