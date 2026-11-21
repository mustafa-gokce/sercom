#include "../include/Definitions.h"

namespace sercom::Definitions {

uint8_t fromFrameFlagType(const FrameFlagType frameFlagType)
{
    return static_cast<uint8_t>(frameFlagType);
}

FrameFlagType toFrameFlagType(const uint8_t frameFlagType)
{
    return static_cast<FrameFlagType>(frameFlagType);
}

bool isFrameFlagSet(const uint8_t flags, const FrameFlagType frameFlagType)
{
    // expose whether the bit of the flag is set
    return (flags & fromFrameFlagType(frameFlagType)) != 0;
}

std::string_view getFrameFlagTypeText(const FrameFlagType frameFlagType)
{
    switch (frameFlagType) {
    case FrameFlagType::ACK_REQUEST: {
        return {"ACK_REQUEST"};
    }
    case FrameFlagType::ACK: {
        return {"ACK"};
    }
    case FrameFlagType::NACK: {
        return {"NACK"};
    }
    case FrameFlagType::RETRANSMISSION: {
        return {"RETRANSMISSION"};
    }
    default: {
        return {"UNKNOWN"};
    }
    }
}

} // namespace sercom::Definitions
