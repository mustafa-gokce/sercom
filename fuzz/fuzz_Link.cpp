#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>

#include "../core/include/Clock.h"
#include "../core/include/Definitions.h"
#include "../core/include/Frame.h"
#include "../core/include/Link.h"
#include "../core/include/Settings.h"
#include "../core/include/Transport.h"

class MockClock final : public sercom::Clock::Clock
{
public:
    /*
     * Mock clock that only moves when the fuzz input advances it.
     */
    explicit MockClock(const std::chrono::milliseconds currentTime)
        : currentTime(currentTime)
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
     * Get the current time of the mock clock.
     */
    [[nodiscard]] std::chrono::milliseconds getTime() const final
    {
        // expose the current time
        return this->currentTime;
    }

    /*
     * Advance the mock clock by the duration.
     */
    void advance(const std::chrono::milliseconds duration)
    {
        // advance the current time
        this->currentTime += duration;
    }

private:
    std::chrono::milliseconds currentTime; // current time of the mock clock
};

class MockTransport final : public sercom::Transport::Transport
{
public:
    /*
     * Mock transport that accepts every byte the link sends.
     */
    MockTransport() = default;
    ~MockTransport() final {}

    // delete copy constructor and copy assignment operator
    MockTransport(const MockTransport &) = delete;
    MockTransport &operator=(const MockTransport &) = delete;

    // delete move constructor and move assignment operator
    MockTransport(MockTransport &&) = delete;
    MockTransport &operator=(MockTransport &&) = delete;

public:
    /*
     * Send the data by accepting every byte.
     */
    [[nodiscard]] bool sendData([[maybe_unused]] const uint8_t *data,
                                [[maybe_unused]] const size_t size) final
    {
        // data sent successfully
        return true;
    }
};

class MockLinkHandler final : public sercom::Link::LinkHandler
{
public:
    /*
     * Mock link handler that answers every received message with a status taken from the frame.
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
     * Listen for a received message and get a message status type from its message type.
     */
    [[nodiscard]] sercom::Definitions::MessageStatusType onReceivedMessage(
        const sercom::Frame::Frame &frame) final
    {
        // expose a message status type taken from the message type
        static constexpr uint16_t messageStatusTypeCount = 6;
        return sercom::Definitions::toMessageStatusType(
            static_cast<uint8_t>(frame.getMessageType() % messageStatusTypeCount));
    }

    /*
     * Listen for a confirmed delivery that needs no work.
     */
    void onDeliveryConfirmed([[maybe_unused]] const uint16_t messageCounter,
                             [[maybe_unused]] const uint16_t messageType) final
    {}

    /*
     * Listen for a failed delivery that needs no work.
     */
    void onDeliveryFailed(
        [[maybe_unused]] const uint16_t messageCounter,
        [[maybe_unused]] const uint16_t messageType,
        [[maybe_unused]] const sercom::Definitions::DeliveryFailureType deliveryFailureType,
        [[maybe_unused]] const sercom::Definitions::MessageStatusType remoteMessageStatusType) final
    {}
};

/*
 * Fuzz entry point of the link layer.
 */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, const size_t size)
{
    using namespace sercom;
    using namespace std::chrono_literals;

    // initialize the link near the 32 bit limit
    static constexpr std::chrono::milliseconds startTime = 0xFFFFF000ms;
    MockClock mockClock(startTime);
    MockTransport mockTransport;
    MockLinkHandler mockLinkHandler;
    Link::Link link(mockTransport, mockLinkHandler, mockClock);

    /*
     * The input is a little program of opcodes.
     * An opcode feeds bytes, advances the time or sends a message.
     */
    static constexpr uint8_t firstAdvanceOpcode = 0x80;
    static constexpr uint8_t firstSendOpcode = 0xC0;
    static constexpr size_t sendPayloadSize = 3;
    static constexpr std::chrono::milliseconds advanceStep = 10ms;
    size_t position = 0;
    while (position < size) {
        // get the next opcode
        const uint8_t opcode = data[position];
        ++position;

        // feed up to opcode plus one bytes
        if (opcode < firstAdvanceOpcode) {
            const size_t receivedSize = std::min(static_cast<size_t>(opcode) + 1, size - position);
            link.onReceivedData(data + position, receivedSize);
            position += receivedSize;
        }

        // advance the time and maintain the link
        else if (opcode < firstSendOpcode) {
            mockClock.advance((opcode - firstAdvanceOpcode) * advanceStep);
            link.maintain();
        }

        // send a message for a send opcode
        else {
            const std::array<uint8_t, sendPayloadSize> payload = {opcode, 0x00, 0xFF};
            const bool acknowledged = (opcode & 1) != 0;
            uint16_t messageCounter = 0;
            [[maybe_unused]] const Definitions::SendStatusType sendStatusType = link.sendMessage(
                opcode, payload.data(), payload.size(), acknowledged, messageCounter);
        }

        // window must not hold more messages than its slots
        if (link.getPendingCount() > Settings::MaximumPendingFrames) {
            std::abort();
        }
    }

    // input processed successfully
    return 0;
}
