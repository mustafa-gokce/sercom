#ifndef SERCOM_FRAME_TRACER_H
#define SERCOM_FRAME_TRACER_H

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include "../include/Clock.h"
#include "../include/Definitions.h"
#include "../include/Frame.h"
#include "../include/FrameDecoder.h"
#include "../include/Settings.h"

namespace sercom::FrameTracer {

/*
 * Trace direction type.
 */
enum class TraceDirectionType : uint8_t {
    UNKNOWN = 0,        // unknown trace direction
    HOST_TO_DEVICE = 1, // bytes travel from the host to the device
    DEVICE_TO_HOST = 2, // bytes travel from the device to the host
};

/*
 * Trace direction type to string.
 */
std::string_view fromTraceDirectionType(TraceDirectionType traceDirectionType);

/*
 * String to trace direction type.
 */
TraceDirectionType toTraceDirectionType(std::string_view traceDirectionType);

/*
 * Get trace direction type as text.
 */
[[nodiscard]] std::string_view getTraceDirectionTypeText(TraceDirectionType traceDirectionType);

/*
 * Get whether every byte is a printable character, a carriage return, a line feed or a tab.
 */
[[nodiscard]] bool isTextData(const uint8_t *data, size_t size);

class FrameTracerHandler
{
public:
    /*
     * Receiver of the traced frames and of the bytes outside a frame, implemented by a display.
     */
    FrameTracerHandler() = default;
    virtual ~FrameTracerHandler();

    // delete copy constructor and copy assignment operator
    FrameTracerHandler(const FrameTracerHandler &) = delete;
    FrameTracerHandler &operator=(const FrameTracerHandler &) = delete;

    // delete move constructor and move assignment operator
    FrameTracerHandler(FrameTracerHandler &&) = delete;
    FrameTracerHandler &operator=(FrameTracerHandler &&) = delete;

public:
    /*
     * Listen for a traced frame whose CRC32 matches or not.
     * The frame reference is valid during the call only.
     */
    virtual void onReceivedTracedFrame(TraceDirectionType traceDirectionType,
                                       const Frame::Frame &frame,
                                       std::chrono::milliseconds currentTime)
        = 0;

    /*
     * Listen for traced bytes outside a frame, for example boot text or noise.
     * The data is valid during the call only.
     */
    virtual void onReceivedUnframedData(TraceDirectionType traceDirectionType,
                                        const uint8_t *data,
                                        size_t size,
                                        std::chrono::milliseconds currentTime)
        = 0;
};

class TraceChannel final : public FrameDecoder::FrameDecoderHandler
{
public:
    /*
     * One direction of a frame tracer that decodes its bytes and collects the unframed bytes.
     * Unframed bytes are reported at a line end, when their buffer is full and before a frame.
     * They are also reported when the line was silent for longer than the inter byte timeout.
     * A line feed ends the unframed bytes only when they form text.
     */
    explicit TraceChannel(FrameTracerHandler &handler, TraceDirectionType traceDirectionType);
    ~TraceChannel() final;

    // delete copy constructor and copy assignment operator
    TraceChannel(const TraceChannel &) = delete;
    TraceChannel &operator=(const TraceChannel &) = delete;

    // delete move constructor and move assignment operator
    TraceChannel(TraceChannel &&) = delete;
    TraceChannel &operator=(TraceChannel &&) = delete;

public:
    /*
     * Listen for traced data and notify the handler about every frame and the unframed bytes.
     */
    void onReceivedData(const uint8_t *data, size_t size, std::chrono::milliseconds currentTime);

    /*
     * Check whether the line was silent for longer than the inter byte timeout.
     * A silent line drops a partial frame and reports the unframed bytes.
     */
    void checkTimeout(std::chrono::milliseconds currentTime);

private:
    /*
     * Listen for a decoded frame and report it after the unframed bytes before it.
     */
    void onReceivedFrame(const Frame::Frame &frame) final;

    /*
     * Listen for a decode error and report a frame candidate whose CRC32 does not match.
     */
    void onDecodeError(Definitions::DecodeErrorType decodeErrorType) final;

private:
    /*
     * Get the number of bytes that the decoder discarded and that are not taken yet.
     */
    [[nodiscard]] size_t getDiscardedByteCount() const;

    /*
     * Take discarded bytes from the front of the buffer as unframed bytes.
     * Bytes already reported in a frame candidate are not reported again.
     */
    void takeDiscardedBytes(size_t byteCount);

    /*
     * Remove bytes from the front of the buffer.
     */
    void removeBytes(size_t byteCount);

    /*
     * Append an unframed byte and report the unframed bytes at a text line end or when full.
     */
    void appendUnframedByte(uint8_t byte);

    /*
     * Report the collected unframed bytes.
     */
    void reportUnframedData();

private:
    FrameTracerHandler &handler;                              // reference to handler
    const TraceDirectionType traceDirectionType;              // direction of the traced bytes
    FrameDecoder::FrameDecoder frameDecoder;                  // decoder of the traced bytes
    std::array<uint8_t, Settings::MAXIMUM_FRAME_SIZE> buffer; // bytes the decoder holds or dropped
    size_t bufferedSize;                                      // number of buffered bytes
    uint64_t takenDiscardedByteCounter;                       // discarded bytes taken
    size_t reportedByteCount;                                 // bytes of reported candidates
    std::array<uint8_t, Settings::MAXIMUM_UNFRAMED_DATA_SIZE> unframedData; // unreported bytes
    size_t unframedDataSize;                         // number of unreported unframed bytes
    std::chrono::milliseconds unframedDataTimestamp; // arrival time of the first unframed byte
    std::chrono::milliseconds lastByteTimestamp;     // arrival time of the newest byte
};

class FrameTracer final
{
public:
    /*
     * Tracer that decodes the sent and the received bytes of one side into frames for display.
     * The sent bytes travel in the sent trace direction and the received bytes in the other one.
     */
    explicit FrameTracer(FrameTracerHandler &handler,
                         const Clock::Clock &clock,
                         TraceDirectionType sentTraceDirectionType);
    ~FrameTracer();

    // delete copy constructor and copy assignment operator
    FrameTracer(const FrameTracer &) = delete;
    FrameTracer &operator=(const FrameTracer &) = delete;

    // delete move constructor and move assignment operator
    FrameTracer(FrameTracer &&) = delete;
    FrameTracer &operator=(FrameTracer &&) = delete;

public:
    /*
     * Listen for sent data and trace it.
     */
    void onSentData(const uint8_t *data, size_t size);

    /*
     * Listen for received data and trace it.
     */
    void onReceivedData(const uint8_t *data, size_t size);

    /*
     * Maintain the inter byte timeouts of both directions.
     */
    void maintain();

private:
    const Clock::Clock &clock;         // reference for clock
    TraceChannel sentTraceChannel;     // trace of the sent bytes
    TraceChannel receivedTraceChannel; // trace of the received bytes
};

} // namespace sercom::FrameTracer

#endif // SERCOM_FRAME_TRACER_H
