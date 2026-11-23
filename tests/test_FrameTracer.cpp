#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string_view>
#include <vector>

#include <catch2/catch_all.hpp>

#include "../core/include/Clock.h"
#include "../core/include/Definitions.h"
#include "../core/include/Frame.h"
#include "../core/include/FrameTracer.h"
#include "../core/include/Link.h"
#include "../core/include/Settings.h"
#include "../core/include/TraceTransport.h"
#include "../core/include/Transport.h"

using namespace std::chrono_literals;
using namespace sercom;

namespace {

/*
 * Frame or unframed bytes that a mock frame tracer handler saw.
 */
struct TraceEvent
{
    FrameTracer::TraceDirectionType traceDirectionType
        = FrameTracer::TraceDirectionType::UNKNOWN; // direction of the traced bytes
    bool framed = false;                            // event is a traced frame
    std::vector<uint8_t> data;                      // bytes of the frame or the unframed bytes
    std::chrono::milliseconds timestamp = 0ms;      // time of the event in milliseconds
};

class MockClock final : public Clock::Clock
{
public:
    /*
     * Mock clock that only moves when the test moves it.
     */
    MockClock()
        : currentTime(0)
    {}
    ~MockClock() final {}

    // delete copy constructor and copy assignment operator
    MockClock(const MockClock &) = delete;
    MockClock &operator=(const MockClock &) = delete;

    // delete move constructor and move assignment operator
    MockClock(MockClock &&) = delete;
    MockClock &operator=(MockClock &&) = delete;

public:
    /*
     * Get the current time in milliseconds.
     */
    [[nodiscard]] std::chrono::milliseconds getTime() const final
    {
        // expose the current time
        return this->currentTime;
    }

    /*
     * Move the current time forward.
     */
    void advanceTime(const std::chrono::milliseconds duration)
    {
        // move the current time forward
        this->currentTime += duration;
    }

private:
    std::chrono::milliseconds currentTime; // current time in milliseconds
};

class MockTransport final : public Transport::Transport
{
public:
    /*
     * Mock transport that records the sent data unless it is broken.
     */
    MockTransport()
        : broken(false)
    {}
    ~MockTransport() final {}

    // delete copy constructor and copy assignment operator
    MockTransport(const MockTransport &) = delete;
    MockTransport &operator=(const MockTransport &) = delete;

    // delete move constructor and move assignment operator
    MockTransport(MockTransport &&) = delete;
    MockTransport &operator=(MockTransport &&) = delete;

public:
    /*
     * Send data into the record unless the transport is broken.
     */
    [[nodiscard]] bool sendData(const uint8_t *data, const size_t size) final
    {
        // transport must not be broken
        if (this->broken) {
            return false;
        }

        // record the sent data
        this->sentData.insert(this->sentData.end(), data, data + size);

        // data sent successfully
        return true;
    }

public:
    /*
     * Get every sent byte in sending order.
     */
    [[nodiscard]] std::vector<uint8_t> getSentData() const
    {
        // expose the sent data
        return this->sentData;
    }

    /*
     * Set whether the transport fails every send.
     */
    void setBroken(const bool broken)
    {
        // set whether the transport is broken
        this->broken = broken;
    }

private:
    std::vector<uint8_t> sentData; // every sent byte
    bool broken;                   // transport fails every send
};

class MockFrameTracerHandler final : public FrameTracer::FrameTracerHandler
{
public:
    /*
     * Mock frame tracer handler that records every traced frame and every unframed data.
     */
    MockFrameTracerHandler() = default;
    ~MockFrameTracerHandler() final {}

    // delete copy constructor and copy assignment operator
    MockFrameTracerHandler(const MockFrameTracerHandler &) = delete;
    MockFrameTracerHandler &operator=(const MockFrameTracerHandler &) = delete;

    // delete move constructor and move assignment operator
    MockFrameTracerHandler(MockFrameTracerHandler &&) = delete;
    MockFrameTracerHandler &operator=(MockFrameTracerHandler &&) = delete;

public:
    /*
     * Listen for a traced frame and record it.
     */
    void onReceivedTracedFrame(const FrameTracer::TraceDirectionType traceDirectionType,
                               const Frame::Frame &frame,
                               const std::chrono::milliseconds currentTime) final
    {
        // record the traced frame
        TraceEvent traceEvent;
        traceEvent.traceDirectionType = traceDirectionType;
        traceEvent.framed = true;
        traceEvent.data = std::vector<uint8_t>(frame.getData(), frame.getData() + frame.getSize());
        traceEvent.timestamp = currentTime;
        this->traceEvents.push_back(traceEvent);
    }

    /*
     * Listen for traced bytes outside a frame and record them.
     */
    void onReceivedUnframedData(const FrameTracer::TraceDirectionType traceDirectionType,
                                const uint8_t *data,
                                const size_t size,
                                const std::chrono::milliseconds currentTime) final
    {
        // record the unframed data
        TraceEvent traceEvent;
        traceEvent.traceDirectionType = traceDirectionType;
        traceEvent.framed = false;
        traceEvent.data = std::vector<uint8_t>(data, data + size);
        traceEvent.timestamp = currentTime;
        this->traceEvents.push_back(traceEvent);
    }

public:
    /*
     * Get the trace events in arrival order.
     */
    [[nodiscard]] std::vector<TraceEvent> getTraceEvents() const
    {
        // expose the trace events
        return this->traceEvents;
    }

private:
    std::vector<TraceEvent> traceEvents; // trace events in arrival order
};

class MockLinkHandler final : public Link::LinkHandler
{
public:
    /*
     * Mock link handler that accepts every message and ignores every delivery result.
     */
    MockLinkHandler() = default;
    ~MockLinkHandler() final {}

    // delete copy constructor and copy assignment operator
    MockLinkHandler(const MockLinkHandler &) = delete;
    MockLinkHandler &operator=(const MockLinkHandler &) = delete;

    // delete move constructor and move assignment operator
    MockLinkHandler(MockLinkHandler &&) = delete;
    MockLinkHandler &operator=(MockLinkHandler &&) = delete;

public:
    /*
     * Listen for a received message and accept it.
     */
    [[nodiscard]] Definitions::MessageStatusType onReceivedMessage(
        [[maybe_unused]] const Frame::Frame &frame) final
    {
        // accept the message
        return Definitions::MessageStatusType::ACCEPTED;
    }

    /*
     * Listen for a confirmed delivery and ignore it.
     */
    void onDeliveryConfirmed([[maybe_unused]] const uint16_t messageCounter,
                             [[maybe_unused]] const uint16_t messageType) final
    {}

    /*
     * Listen for a failed delivery and ignore it.
     */
    void onDeliveryFailed(
        [[maybe_unused]] const uint16_t messageCounter,
        [[maybe_unused]] const uint16_t messageType,
        [[maybe_unused]] const Definitions::DeliveryFailureType deliveryFailureType,
        [[maybe_unused]] const Definitions::MessageStatusType remoteMessageStatusType) final
    {}
};

} // namespace

class TestFrameTracer
{
public:
    /*
     * Frame tracer test fixture.
     */
    TestFrameTracer()
    {
        // initialize frame tracer
        this->frameTracer.reset(
            new FrameTracer::FrameTracer(this->handler,
                                         this->clock,
                                         FrameTracer::TraceDirectionType::HOST_TO_DEVICE));

        // initialize trace transport
        this->traceTransport.reset(
            new TraceTransport::TraceTransport(this->transport, *this->frameTracer));
    }
    ~TestFrameTracer()
    {
        // close trace transport
        this->traceTransport.reset(nullptr);

        // close frame tracer
        this->frameTracer.reset(nullptr);
    }

    /*
     * Get the bytes of a frame built from its fields.
     */
    [[nodiscard]] static std::vector<uint8_t> getFrameBytes(const uint8_t flags,
                                                            const uint16_t messageCounter,
                                                            const uint16_t messageType,
                                                            const std::vector<uint8_t> &payload)
    {
        // build the frame
        const Frame::Frame frame(flags, messageCounter, messageType, payload.data(), payload.size());

        // expose a copy of the frame bytes
        return std::vector<uint8_t>(frame.getData(), frame.getData() + frame.getSize());
    }

    /*
     * Get the bytes of a routed frame from the host to node 3 built from its fields.
     */
    [[nodiscard]] static std::vector<uint8_t> getRoutedFrameBytes(const uint16_t messageCounter,
                                                                  const uint16_t messageType,
                                                                  const std::vector<uint8_t> &payload)
    {
        // build the routed frame
        static constexpr uint8_t destinationAddress = 3;
        Frame::RouteHeader routeHeader;
        routeHeader.sourceAddress = Definitions::HOST_ADDRESS;
        routeHeader.destinationAddress = destinationAddress;
        routeHeader.timeToLive = Settings::DEFAULT_TIME_TO_LIVE;
        const Frame::Frame frame(0x11,
                                 messageCounter,
                                 messageType,
                                 routeHeader,
                                 payload.data(),
                                 payload.size());

        // expose a copy of the frame bytes
        return std::vector<uint8_t>(frame.getData(), frame.getData() + frame.getSize());
    }

    /*
     * Get the bytes of a text.
     */
    [[nodiscard]] static std::vector<uint8_t> getTextBytes(const std::string_view text)
    {
        // expose a copy of the characters
        return std::vector<uint8_t>(text.cbegin(), text.cend());
    }

    /*
     * Get the bytes of two byte sequences one after the other.
     */
    [[nodiscard]] static std::vector<uint8_t> getJoinedBytes(const std::vector<uint8_t> &firstBytes,
                                                             const std::vector<uint8_t> &secondBytes)
    {
        // append the second bytes to the first bytes
        std::vector<uint8_t> joinedBytes = firstBytes;
        joinedBytes.insert(joinedBytes.end(), secondBytes.cbegin(), secondBytes.cend());

        // expose the joined bytes
        return joinedBytes;
    }

    /*
     * Let the line stay silent for longer than the inter byte timeout and maintain the tracer.
     */
    void advanceSilence()
    {
        // let twice the inter byte timeout pass
        this->clock.advanceTime(2 * Settings::DEFAULT_INTER_BYTE_TIMEOUT_MSEC);

        // maintain the frame tracer
        this->frameTracer->maintain();
    }

    // delete copy and move operators
    TestFrameTracer(const TestFrameTracer &other) = delete;
    TestFrameTracer &operator=(const TestFrameTracer &other) = delete;
    TestFrameTracer(TestFrameTracer &&other) noexcept = delete;
    TestFrameTracer &operator=(TestFrameTracer &&other) noexcept = delete;

protected:
    MockClock clock;                                                // clock of the frame tracer
    MockFrameTracerHandler handler;                                 // events of the frame tracer
    MockTransport transport;                                        // data of the trace transport
    std::unique_ptr<FrameTracer::FrameTracer> frameTracer;          // tracer of the host side
    std::unique_ptr<TraceTransport::TraceTransport> traceTransport; // transport under test
};

TEST_CASE_METHOD(TestFrameTracer,
                 "Trace direction type can be converted to string",
                 "[FrameTracer][fromTraceDirectionType]")
{
    REQUIRE(FrameTracer::fromTraceDirectionType(FrameTracer::TraceDirectionType::UNKNOWN)
            == "UNKNOWN");
    REQUIRE(FrameTracer::fromTraceDirectionType(FrameTracer::TraceDirectionType::HOST_TO_DEVICE)
            == "HOST_TO_DEVICE");
    REQUIRE(FrameTracer::fromTraceDirectionType(FrameTracer::TraceDirectionType::DEVICE_TO_HOST)
            == "DEVICE_TO_HOST");
}

TEST_CASE_METHOD(TestFrameTracer,
                 "Trace direction type can be parsed from string",
                 "[FrameTracer][toTraceDirectionType]")
{
    REQUIRE(FrameTracer::toTraceDirectionType("UNKNOWN")
            == FrameTracer::TraceDirectionType::UNKNOWN);
    REQUIRE(FrameTracer::toTraceDirectionType("HOST_TO_DEVICE")
            == FrameTracer::TraceDirectionType::HOST_TO_DEVICE);
    REQUIRE(FrameTracer::toTraceDirectionType("DEVICE_TO_HOST")
            == FrameTracer::TraceDirectionType::DEVICE_TO_HOST);
    REQUIRE(FrameTracer::toTraceDirectionType("LOREM_IPSUM")
            == FrameTracer::TraceDirectionType::UNKNOWN);
    REQUIRE(FrameTracer::toTraceDirectionType("host_to_device")
            == FrameTracer::TraceDirectionType::UNKNOWN);
    REQUIRE(FrameTracer::toTraceDirectionType("HOST_TO_DEVICE ")
            == FrameTracer::TraceDirectionType::UNKNOWN);
}

TEST_CASE_METHOD(TestFrameTracer,
                 "Trace direction type is shown as an arrow",
                 "[FrameTracer][getTraceDirectionTypeText]")
{
    REQUIRE(FrameTracer::getTraceDirectionTypeText(FrameTracer::TraceDirectionType::UNKNOWN)
            == "??");
    REQUIRE(FrameTracer::getTraceDirectionTypeText(FrameTracer::TraceDirectionType::HOST_TO_DEVICE)
            == ">>");
    REQUIRE(FrameTracer::getTraceDirectionTypeText(FrameTracer::TraceDirectionType::DEVICE_TO_HOST)
            == "<<");
}

TEST_CASE_METHOD(TestFrameTracer,
                 "Only printable characters and line ends form text data",
                 "[FrameTracer][isTextData]")
{
    // check printable characters and the line end characters
    const std::vector<uint8_t> text = getTextBytes(" ok~\r\n\t");
    REQUIRE(FrameTracer::isTextData(text.data(), text.size()));

    // check bytes outside the printable characters
    const std::vector<uint8_t> controlByteText = {'o', 0x00, 'k'};
    const std::vector<uint8_t> deleteByteText = {'o', 0x7F, 'k'};
    const std::vector<uint8_t> highByteText = {'o', 0xA5, 'k'};
    REQUIRE_FALSE(FrameTracer::isTextData(controlByteText.data(), controlByteText.size()));
    REQUIRE_FALSE(FrameTracer::isTextData(deleteByteText.data(), deleteByteText.size()));
    REQUIRE_FALSE(FrameTracer::isTextData(highByteText.data(), highByteText.size()));

    // check missing data
    REQUIRE_FALSE(FrameTracer::isTextData(nullptr, 0));
}

TEST_CASE_METHOD(TestFrameTracer,
                 "Sent and received frames are traced in their directions",
                 "[FrameTracer][testDirections]")
{
    // trace a sent request and a received response
    static constexpr std::chrono::milliseconds responseDelay = 5ms;
    const std::vector<uint8_t> request
        = getFrameBytes(0x01, 7, 0x0001, std::vector<uint8_t>({0xE8, 0x03, 0x00, 0x00}));
    const std::vector<uint8_t> response = getFrameBytes(0x02, 7, 0x0001, std::vector<uint8_t>());
    frameTracer->onSentData(request.data(), request.size());
    clock.advanceTime(responseDelay);
    frameTracer->onReceivedData(response.data(), response.size());

    // check both frames with their directions and times
    const std::vector<TraceEvent> traceEvents = handler.getTraceEvents();
    REQUIRE(traceEvents.size() == static_cast<size_t>(2));
    REQUIRE(traceEvents.at(0).framed);
    REQUIRE(traceEvents.at(0).traceDirectionType == FrameTracer::TraceDirectionType::HOST_TO_DEVICE);
    REQUIRE(traceEvents.at(0).data == request);
    REQUIRE(traceEvents.at(0).timestamp == 0ms);
    REQUIRE(traceEvents.at(1).framed);
    REQUIRE(traceEvents.at(1).traceDirectionType == FrameTracer::TraceDirectionType::DEVICE_TO_HOST);
    REQUIRE(traceEvents.at(1).data == response);
    REQUIRE(traceEvents.at(1).timestamp == responseDelay);
}

TEST_CASE_METHOD(TestFrameTracer,
                 "A tracer of the device side swaps the directions",
                 "[FrameTracer][testDeviceDirections]")
{
    // trace a received request and a sent response on the device side
    FrameTracer::FrameTracer deviceFrameTracer(handler,
                                               clock,
                                               FrameTracer::TraceDirectionType::DEVICE_TO_HOST);
    const std::vector<uint8_t> request = getFrameBytes(0x01, 3, 0x0003, std::vector<uint8_t>());
    const std::vector<uint8_t> response = getFrameBytes(0x02, 3, 0x0003, std::vector<uint8_t>());
    deviceFrameTracer.onReceivedData(request.data(), request.size());
    deviceFrameTracer.onSentData(response.data(), response.size());

    // check the directions
    const std::vector<TraceEvent> traceEvents = handler.getTraceEvents();
    REQUIRE(traceEvents.size() == static_cast<size_t>(2));
    REQUIRE(traceEvents.at(0).traceDirectionType == FrameTracer::TraceDirectionType::HOST_TO_DEVICE);
    REQUIRE(traceEvents.at(0).data == request);
    REQUIRE(traceEvents.at(1).traceDirectionType == FrameTracer::TraceDirectionType::DEVICE_TO_HOST);
    REQUIRE(traceEvents.at(1).data == response);
}

TEST_CASE_METHOD(TestFrameTracer,
                 "A frame split over several reads is traced once",
                 "[FrameTracer][testSplitFrame]")
{
    // trace a frame in two parts within the inter byte timeout
    static constexpr std::chrono::milliseconds partDelay = 1ms;
    static constexpr size_t firstPartSize = 5;
    const std::vector<uint8_t> frame = getFrameBytes(0x00, 1, 0x0005, getTextBytes("boot"));
    frameTracer->onReceivedData(frame.data(), firstPartSize);
    clock.advanceTime(partDelay);
    frameTracer->onReceivedData(frame.data() + firstPartSize, frame.size() - firstPartSize);

    // check the frame and the time of its last byte
    const std::vector<TraceEvent> traceEvents = handler.getTraceEvents();
    REQUIRE(traceEvents.size() == static_cast<size_t>(1));
    REQUIRE(traceEvents.at(0).framed);
    REQUIRE(traceEvents.at(0).data == frame);
    REQUIRE(traceEvents.at(0).timestamp == partDelay);
}

TEST_CASE_METHOD(TestFrameTracer,
                 "Boot text is reported as unframed data line by line",
                 "[FrameTracer][testBootText]")
{
    // trace two lines of boot text
    const std::vector<uint8_t> bootText = getTextBytes("ets Jun  8 2016 00:22:57\r\nrst:0x1\r\n");
    frameTracer->onReceivedData(bootText.data(), bootText.size());

    // check that every line is reported at its line end
    const std::vector<TraceEvent> traceEvents = handler.getTraceEvents();
    REQUIRE(traceEvents.size() == static_cast<size_t>(2));
    REQUIRE_FALSE(traceEvents.at(0).framed);
    REQUIRE(traceEvents.at(0).traceDirectionType == FrameTracer::TraceDirectionType::DEVICE_TO_HOST);
    REQUIRE(traceEvents.at(0).data == getTextBytes("ets Jun  8 2016 00:22:57\r\n"));
    REQUIRE_FALSE(traceEvents.at(1).framed);
    REQUIRE(traceEvents.at(1).data == getTextBytes("rst:0x1\r\n"));
}

TEST_CASE_METHOD(TestFrameTracer,
                 "Unframed data before a frame is reported before the frame",
                 "[FrameTracer][testUnframedDataBeforeFrame]")
{
    // trace text without a line end and a frame right after it
    const std::vector<uint8_t> frame = getFrameBytes(0x00, 2, 0x0005, getTextBytes("ready"));
    const std::vector<uint8_t> data = getJoinedBytes(getTextBytes("entry 0x40080400"), frame);
    frameTracer->onReceivedData(data.data(), data.size());

    // check the order of the unframed data and the frame
    const std::vector<TraceEvent> traceEvents = handler.getTraceEvents();
    REQUIRE(traceEvents.size() == static_cast<size_t>(2));
    REQUIRE_FALSE(traceEvents.at(0).framed);
    REQUIRE(traceEvents.at(0).data == getTextBytes("entry 0x40080400"));
    REQUIRE(traceEvents.at(1).framed);
    REQUIRE(traceEvents.at(1).data == frame);
}

TEST_CASE_METHOD(TestFrameTracer,
                 "Unframed data is reported after a silent line",
                 "[FrameTracer][testSilentLine]")
{
    // trace text without a line end
    const std::vector<uint8_t> text = getTextBytes("mode:DIO");
    frameTracer->onReceivedData(text.data(), text.size());
    frameTracer->maintain();
    REQUIRE(handler.getTraceEvents().empty());

    // check the text after the silence with its arrival time
    advanceSilence();
    const std::vector<TraceEvent> traceEvents = handler.getTraceEvents();
    REQUIRE(traceEvents.size() == static_cast<size_t>(1));
    REQUIRE_FALSE(traceEvents.at(0).framed);
    REQUIRE(traceEvents.at(0).data == text);
    REQUIRE(traceEvents.at(0).timestamp == 0ms);
}

TEST_CASE_METHOD(TestFrameTracer,
                 "Unframed data is reported when it fills its buffer",
                 "[FrameTracer][testFullUnframedData]")
{
    // trace more noise than the unframed data buffer holds
    static constexpr size_t extraByteCount = 3;
    const std::vector<uint8_t> noise(Settings::MAXIMUM_UNFRAMED_DATA_SIZE + extraByteCount, 0x55);
    frameTracer->onReceivedData(noise.data(), noise.size());

    // check the full buffer and then the rest after the silence
    REQUIRE(handler.getTraceEvents().size() == static_cast<size_t>(1));
    advanceSilence();
    const std::vector<TraceEvent> traceEvents = handler.getTraceEvents();
    REQUIRE(traceEvents.size() == static_cast<size_t>(2));
    REQUIRE(traceEvents.at(0).data.size() == Settings::MAXIMUM_UNFRAMED_DATA_SIZE);
    REQUIRE(traceEvents.at(1).data.size() == extraByteCount);
}

TEST_CASE_METHOD(TestFrameTracer,
                 "Binary unframed data is not cut at a line feed",
                 "[FrameTracer][testBinaryUnframedData]")
{
    // trace noise with a line feed inside
    const std::vector<uint8_t> noise = {0xA5, 0x0B, 0x0A, 0x0C, 0xFE};
    frameTracer->onReceivedData(noise.data(), noise.size());
    REQUIRE(handler.getTraceEvents().empty());

    // check the noise in one piece after the silence
    advanceSilence();
    const std::vector<TraceEvent> traceEvents = handler.getTraceEvents();
    REQUIRE(traceEvents.size() == static_cast<size_t>(1));
    REQUIRE_FALSE(traceEvents.at(0).framed);
    REQUIRE(traceEvents.at(0).data == noise);
}

TEST_CASE_METHOD(TestFrameTracer,
                 "A frame with a wrong CRC32 is reported once",
                 "[FrameTracer][testCRCMismatch]")
{
    // trace a corrupted frame followed by a valid frame
    std::vector<uint8_t> corruptedFrame = getFrameBytes(0x01,
                                                        4,
                                                        0x0006,
                                                        std::vector<uint8_t>({1, 2, 3, 4}));
    corruptedFrame[Definitions::HEADER_SIZE] ^= 0x10;
    const std::vector<uint8_t> validFrame = getFrameBytes(0x01,
                                                          5,
                                                          0x0006,
                                                          std::vector<uint8_t>({1, 2, 3, 4}));
    const std::vector<uint8_t> data = getJoinedBytes(corruptedFrame, validFrame);
    frameTracer->onReceivedData(data.data(), data.size());
    advanceSilence();

    // check the corrupted frame without its bytes as unframed data
    const std::vector<TraceEvent> traceEvents = handler.getTraceEvents();
    REQUIRE(traceEvents.size() == static_cast<size_t>(2));
    REQUIRE(traceEvents.at(0).framed);
    REQUIRE(traceEvents.at(0).data == corruptedFrame);
    const Frame::Frame tracedCorruptedFrame(traceEvents.at(0).data.data(),
                                            traceEvents.at(0).data.size());
    REQUIRE_FALSE(tracedCorruptedFrame.getCRCValid());
    REQUIRE(traceEvents.at(1).framed);
    REQUIRE(traceEvents.at(1).data == validFrame);
}

TEST_CASE_METHOD(TestFrameTracer,
                 "A frame that starts inside a rejected candidate is still traced",
                 "[FrameTracer][testFrameInsideCandidate]")
{
    // trace a frame that lost its last byte followed by a valid frame
    std::vector<uint8_t> truncatedFrame = getFrameBytes(0x00, 8, 0x0005, getTextBytes("lost"));
    truncatedFrame.pop_back();
    const std::vector<uint8_t> validFrame = getFrameBytes(0x00, 9, 0x0005, getTextBytes("kept"));
    const std::vector<uint8_t> data = getJoinedBytes(truncatedFrame, validFrame);
    frameTracer->onReceivedData(data.data(), data.size());
    advanceSilence();

    // check the rejected candidate and the valid frame without unframed data
    const std::vector<TraceEvent> traceEvents = handler.getTraceEvents();
    REQUIRE(traceEvents.size() == static_cast<size_t>(2));
    REQUIRE(traceEvents.at(0).framed);
    REQUIRE(traceEvents.at(0).data.size() == truncatedFrame.size() + 1);
    REQUIRE(traceEvents.at(1).framed);
    REQUIRE(traceEvents.at(1).data == validFrame);
}

TEST_CASE_METHOD(TestFrameTracer,
                 "A frame of another version is reported as unframed data",
                 "[FrameTracer][testInvalidVersion]")
{
    // trace a frame with a wrong version
    static constexpr size_t versionOffset = 2;
    std::vector<uint8_t> frame = getFrameBytes(0x00, 1, 0x0005, getTextBytes("v2"));
    frame[versionOffset] = Definitions::ROUTED_PROTOCOL_VERSION + 1;
    frameTracer->onReceivedData(frame.data(), frame.size());
    advanceSilence();

    // check every byte as unframed data
    const std::vector<TraceEvent> traceEvents = handler.getTraceEvents();
    REQUIRE(traceEvents.size() == static_cast<size_t>(1));
    REQUIRE_FALSE(traceEvents.at(0).framed);
    REQUIRE(traceEvents.at(0).data == frame);
}

TEST_CASE_METHOD(TestFrameTracer,
                 "A partial frame is reported as unframed data after a silent line",
                 "[FrameTracer][testPartialFrame]")
{
    // trace the first half of a frame
    const std::vector<uint8_t> frame = getFrameBytes(0x01,
                                                     6,
                                                     0x0001,
                                                     std::vector<uint8_t>({0, 0, 0, 0}));
    const size_t partSize = frame.size() / 2;
    frameTracer->onReceivedData(frame.data(), partSize);
    REQUIRE(handler.getTraceEvents().empty());

    // check the dropped bytes after the silence
    advanceSilence();
    const std::vector<TraceEvent> traceEvents = handler.getTraceEvents();
    REQUIRE(traceEvents.size() == static_cast<size_t>(1));
    REQUIRE_FALSE(traceEvents.at(0).framed);
    REQUIRE(traceEvents.at(0).data == std::vector<uint8_t>(frame.data(), frame.data() + partSize));
}

TEST_CASE_METHOD(TestFrameTracer,
                 "The trace transport sends and traces the data",
                 "[FrameTracer][testTraceTransport]")
{
    // send a frame through the trace transport
    const std::vector<uint8_t> frame = getFrameBytes(0x01, 1, 0x0100, std::vector<uint8_t>({0x01}));
    REQUIRE(traceTransport->sendData(frame.data(), frame.size()));

    // check the frame on the transport and in the trace
    const std::vector<TraceEvent> traceEvents = handler.getTraceEvents();
    REQUIRE(transport.getSentData() == frame);
    REQUIRE(traceEvents.size() == static_cast<size_t>(1));
    REQUIRE(traceEvents.at(0).traceDirectionType == FrameTracer::TraceDirectionType::HOST_TO_DEVICE);
    REQUIRE(traceEvents.at(0).data == frame);
}

TEST_CASE_METHOD(TestFrameTracer,
                 "The trace transport does not trace data it could not send",
                 "[FrameTracer][testBrokenTraceTransport]")
{
    // send a frame through a broken transport
    const std::vector<uint8_t> frame = getFrameBytes(0x01, 1, 0x0100, std::vector<uint8_t>({0x01}));
    transport.setBroken(true);
    REQUIRE_FALSE(traceTransport->sendData(frame.data(), frame.size()));

    // check that nothing is traced
    REQUIRE(transport.getSentData().empty());
    REQUIRE(handler.getTraceEvents().empty());
}

TEST_CASE_METHOD(TestFrameTracer,
                 "A retransmission is traced as it goes on the wire",
                 "[FrameTracer][testRetransmission]")
{
    // send an acknowledged message that stays unanswered
    MockLinkHandler linkHandler;
    Link::Link link(*traceTransport, linkHandler, clock);
    const std::vector<uint8_t> payload = {0x01};
    uint16_t messageCounter = 0;
    REQUIRE(link.sendMessage(0x0100, payload.data(), payload.size(), true, messageCounter)
            == Definitions::SendStatusType::SENT);

    // let the retransmit timeout pass
    clock.advanceTime(Settings::DEFAULT_RETRANSMIT_TIMEOUT_MSEC);
    link.maintain();

    // check the first transmission and the retransmission
    const std::vector<TraceEvent> traceEvents = handler.getTraceEvents();
    REQUIRE(traceEvents.size() == static_cast<size_t>(2));
    const Frame::Frame firstFrame(traceEvents.at(0).data.data(), traceEvents.at(0).data.size());
    const Frame::Frame secondFrame(traceEvents.at(1).data.data(), traceEvents.at(1).data.size());
    REQUIRE(firstFrame.getMessageCounter() == messageCounter);
    REQUIRE_FALSE(firstFrame.getFlag(Definitions::FrameFlagType::RETRANSMISSION));
    REQUIRE(secondFrame.getMessageCounter() == messageCounter);
    REQUIRE(secondFrame.getFlag(Definitions::FrameFlagType::RETRANSMISSION));
    REQUIRE(traceEvents.at(1).timestamp == Settings::DEFAULT_RETRANSMIT_TIMEOUT_MSEC);
}

TEST_CASE_METHOD(TestFrameTracer,
                 "Routed frames are traced between direct frames and boot text",
                 "[FrameTracer][testRoutedFrames]")
{
    // trace boot text, a routed frame, a direct frame and a routed frame
    const std::vector<uint8_t> bootText = getTextBytes("boot\r\n");
    const std::vector<uint8_t> firstRoutedFrame
        = getRoutedFrameBytes(1, 0x0001, std::vector<uint8_t>({0xE8, 0x03, 0x00, 0x00}));
    const std::vector<uint8_t> directFrame = getFrameBytes(0x02, 1, 0x0001, std::vector<uint8_t>());
    const std::vector<uint8_t> secondRoutedFrame = getRoutedFrameBytes(2,
                                                                       0x0003,
                                                                       std::vector<uint8_t>());
    const std::vector<uint8_t> data = getJoinedBytes(getJoinedBytes(bootText, firstRoutedFrame),
                                                     getJoinedBytes(directFrame, secondRoutedFrame));
    frameTracer->onReceivedData(data.data(), data.size());
    advanceSilence();

    // check the text and every frame with its version
    const std::vector<TraceEvent> traceEvents = handler.getTraceEvents();
    REQUIRE(traceEvents.size() == static_cast<size_t>(4));
    REQUIRE_FALSE(traceEvents.at(0).framed);
    REQUIRE(traceEvents.at(0).data == bootText);
    REQUIRE(traceEvents.at(1).framed);
    REQUIRE(traceEvents.at(1).data == firstRoutedFrame);
    REQUIRE(traceEvents.at(2).framed);
    REQUIRE(traceEvents.at(2).data == directFrame);
    REQUIRE(traceEvents.at(3).framed);
    REQUIRE(traceEvents.at(3).data == secondRoutedFrame);
    const Frame::Frame tracedFrame(traceEvents.at(3).data.data(), traceEvents.at(3).data.size());
    REQUIRE(tracedFrame.getRouted());
    REQUIRE(tracedFrame.getCRCValid());
}

TEST_CASE_METHOD(TestFrameTracer,
                 "A routed frame with a wrong CRC32 is reported once with all its bytes",
                 "[FrameTracer][testRoutedCRCMismatch]")
{
    // trace a routed frame with a damaged time to live followed by a valid routed frame
    static constexpr size_t timeToLiveOffset = Definitions::ROUTED_HEADER_SIZE - 1;
    std::vector<uint8_t> corruptedFrame = getRoutedFrameBytes(4,
                                                              0x0006,
                                                              std::vector<uint8_t>({1, 2, 3, 4}));
    corruptedFrame[timeToLiveOffset] ^= 0x01;
    const std::vector<uint8_t> validFrame = getRoutedFrameBytes(5,
                                                                0x0006,
                                                                std::vector<uint8_t>({1, 2, 3, 4}));
    const std::vector<uint8_t> data = getJoinedBytes(corruptedFrame, validFrame);
    frameTracer->onReceivedData(data.data(), data.size());
    advanceSilence();

    // check the corrupted routed frame without its bytes as unframed data
    const std::vector<TraceEvent> traceEvents = handler.getTraceEvents();
    REQUIRE(traceEvents.size() == static_cast<size_t>(2));
    REQUIRE(traceEvents.at(0).framed);
    REQUIRE(traceEvents.at(0).data == corruptedFrame);
    const Frame::Frame tracedCorruptedFrame(traceEvents.at(0).data.data(),
                                            traceEvents.at(0).data.size());
    REQUIRE_FALSE(tracedCorruptedFrame.getCRCValid());
    REQUIRE(traceEvents.at(1).framed);
    REQUIRE(traceEvents.at(1).data == validFrame);
}
