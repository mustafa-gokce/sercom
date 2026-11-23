#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>

#include "../core/include/Clock.h"
#include "../core/include/Definitions.h"
#include "../core/include/Frame.h"
#include "../core/include/Link.h"
#include "../core/include/Router.h"
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
     * Mock transport that accepts every byte the router sends on a port.
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
     * Mock link handler of the node that accepts every received message.
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
    [[nodiscard]] sercom::Definitions::MessageStatusType onReceivedMessage(
        [[maybe_unused]] const sercom::Frame::Frame &frame) final
    {
        // accept the message
        return sercom::Definitions::MessageStatusType::ACCEPTED;
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
 * Fuzz entry point of the router.
 */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, const size_t size)
{
    using namespace sercom;
    using namespace std::chrono_literals;

    // input must hold the port selector
    if (size == 0) {
        return 0;
    }

    // initialize node 3 with both ports and a proxy for node 4 near the 32 bit limit
    static constexpr std::chrono::milliseconds startTime = 0xFFFFF000ms;
    static constexpr uint8_t nodeAddress = 3;
    static constexpr uint8_t proxyAddress = 4;
    MockClock mockClock(startTime);
    MockTransport primaryTransport;
    MockTransport secondaryTransport;
    MockLinkHandler mockLinkHandler;
    Router::RouterConfiguration routerConfiguration;
    routerConfiguration.nodeAddress = nodeAddress;
    routerConfiguration.proxyAddress = proxyAddress;
    Router::Router router(primaryTransport,
                          &secondaryTransport,
                          mockLinkHandler,
                          mockClock,
                          Link::LinkConfiguration(),
                          routerConfiguration);

    /*
     * The bits of the first byte select the port of each chunk in turn.
     * Every chunk advances the time, and every fourth chunk sends an own message to its first byte.
     */
    const uint8_t portSelector = data[0];
    static constexpr size_t chunkSize = 16;
    static constexpr size_t selectorBitCount = 8;
    static constexpr size_t ownMessagePeriod = 4;
    static constexpr std::chrono::milliseconds advanceStep = 50ms;
    size_t position = 1;
    size_t chunkIndex = 0;
    while (position < size) {
        // hand the next chunk to the port of its selector bit
        const size_t receivedSize = std::min(chunkSize, size - position);
        const bool secondary = ((portSelector >> (chunkIndex % selectorBitCount)) & 1) != 0;
        router.onReceivedData(secondary ? Definitions::PortType::SECONDARY
                                        : Definitions::PortType::PRIMARY,
                              data + position,
                              receivedSize);

        // send an own message to the address of the first chunk byte
        if (chunkIndex % ownMessagePeriod == ownMessagePeriod - 1) {
            uint16_t messageCounter = 0;
            [[maybe_unused]] const Definitions::SendStatusType sendStatusType
                = router.sendMessage(data[position],
                                     Definitions::ROUTE_FAILURE_REPORT_MESSAGE_TYPE + 1,
                                     data + position,
                                     receivedSize,
                                     true,
                                     messageCounter);
        }

        // advance the time and maintain both ports
        mockClock.advance(advanceStep);
        router.maintain();
        position += receivedSize;
        ++chunkIndex;

        // window must not hold more messages than its slots
        if (router.getPendingCount() > Settings::MaximumPendingFrames) {
            std::abort();
        }
    }

    // input processed successfully
    return 0;
}
