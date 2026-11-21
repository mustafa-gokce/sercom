#ifndef SERCOM_TRACE_TRANSPORT_H
#define SERCOM_TRACE_TRANSPORT_H

#include <cstddef>
#include <cstdint>

#include "../include/FrameTracer.h"
#include "../include/Transport.h"

namespace sercom::TraceTransport {

class TraceTransport final : public Transport::Transport
{
public:
    /*
     * Transport that sends through another transport and hands every sent byte to a frame tracer.
     */
    explicit TraceTransport(sercom::Transport::Transport &transport,
                            FrameTracer::FrameTracer &frameTracer);
    ~TraceTransport() final;

    // delete copy constructor and copy assignment operator
    TraceTransport(const TraceTransport &) = delete;
    TraceTransport &operator=(const TraceTransport &) = delete;

    // delete move constructor and move assignment operator
    TraceTransport(TraceTransport &&) = delete;
    TraceTransport &operator=(TraceTransport &&) = delete;

public:
    /*
     * Send every byte of the data through the transport and trace the data that was sent.
     */
    [[nodiscard]] bool sendData(const uint8_t *data, size_t size) final;

private:
    sercom::Transport::Transport &transport; // reference for transport
    FrameTracer::FrameTracer &frameTracer;   // reference for frame tracer
};

} // namespace sercom::TraceTransport

#endif // SERCOM_TRACE_TRANSPORT_H
