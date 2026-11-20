#include "../include/TraceTransport.h"

namespace sercom::TraceTransport {

TraceTransport::TraceTransport(sercom::Transport::Transport &transport,
                               FrameTracer::FrameTracer &frameTracer)
    : transport(transport)
    , frameTracer(frameTracer)
{}

TraceTransport::~TraceTransport() {}

bool TraceTransport::sendData(const uint8_t *data, const size_t size)
{
    // data must be sent through the transport
    if (!this->transport.sendData(data, size)) {
        return false;
    }

    // trace the sent data
    this->frameTracer.onSentData(data, size);

    // data sent successfully
    return true;
}

} // namespace sercom::TraceTransport
