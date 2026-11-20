#ifndef SERCOM_DEVICE_CLIENT_H
#define SERCOM_DEVICE_CLIENT_H

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>

#include "../../core/include/Clock.h"
#include "../../core/include/Definitions.h"
#include "../../core/include/Frame.h"
#include "../../core/include/Link.h"
#include "../../core/include/Transport.h"

#include "../include/AppSettings.h"
#include "../include/Messages.h"

namespace sercom::DeviceClient {

/*
 * Ping request that waits for its ping response.
 */
struct PendingPing
{
    bool used = false;                            // slot holds a ping request
    std::chrono::milliseconds requestTimestamp{}; // time the ping request was sent
};

class DeviceClientHandler
{
public:
    /*
     * Receiver of device responses, implemented by the host application.
     */
    DeviceClientHandler() = default;
    virtual ~DeviceClientHandler();

    // delete copy constructor and copy assignment operator
    DeviceClientHandler(const DeviceClientHandler &) = delete;
    DeviceClientHandler &operator=(const DeviceClientHandler &) = delete;

    // delete move constructor and move assignment operator
    DeviceClientHandler(DeviceClientHandler &&) = delete;
    DeviceClientHandler &operator=(DeviceClientHandler &&) = delete;

public:
    /*
     * Listen for a ping response together with the measured round trip time.
     */
    virtual void onReceivedPingResponse(const Messages::PingResponse &pingResponse,
                                        std::chrono::milliseconds roundTripTime)
        = 0;

    /*
     * Listen for a device info response.
     */
    virtual void onReceivedDeviceInfoResponse(const Messages::DeviceInfoResponse &deviceInfoResponse)
        = 0;

    /*
     * Listen for a set LED response.
     */
    virtual void onReceivedSetLedResponse(const Messages::SetLedResponse &setLedResponse) = 0;

    /*
     * Listen for a telemetry report response.
     */
    virtual void onReceivedTelemetryReportResponse(
        const Messages::TelemetryReportResponse &telemetryReportResponse)
        = 0;

    /*
     * Listen for a log report response.
     */
    virtual void onReceivedLogReportResponse(const Messages::LogReportResponse &logReportResponse)
        = 0;

    /*
     * Listen for a request the device acknowledged.
     */
    virtual void onRequestConfirmed(uint16_t messageCounter, Messages::MessageType messageType) = 0;

    /*
     * Listen for a request that timed out or was rejected by the device.
     */
    virtual void onRequestFailed(uint16_t messageCounter,
                                 Messages::MessageType messageType,
                                 Definitions::DeliveryFailureType deliveryFailureType,
                                 Definitions::MessageStatusType remoteMessageStatusType)
        = 0;
};

class DeviceClient final : public Link::LinkHandler
{
public:
    /*
     * Host side of the demo protocol that sends every request acknowledged.
     */
    explicit DeviceClient(
        Transport::Transport &transport,
        const Clock::Clock &clock,
        DeviceClientHandler &handler,
        const Link::LinkConfiguration &linkConfiguration = Link::LinkConfiguration());
    ~DeviceClient() final;

    // delete copy constructor and copy assignment operator
    DeviceClient(const DeviceClient &) = delete;
    DeviceClient &operator=(const DeviceClient &) = delete;

    // delete move constructor and move assignment operator
    DeviceClient(DeviceClient &&) = delete;
    DeviceClient &operator=(DeviceClient &&) = delete;

public:
    /*
     * Send a ping request with the current time and store its message counter.
     */
    [[nodiscard]] Definitions::SendStatusType sendPingRequest(uint16_t &messageCounter);

    /*
     * Send a device info request and store its message counter.
     */
    [[nodiscard]] Definitions::SendStatusType sendDeviceInfoRequest(uint16_t &messageCounter);

    /*
     * Send a set LED request and store its message counter.
     */
    [[nodiscard]] Definitions::SendStatusType sendSetLedRequest(bool ledOn,
                                                                uint16_t &messageCounter);

    /*
     * Send a set telemetry period request and store its message counter.
     * A zero telemetry period stops the telemetry report responses.
     */
    [[nodiscard]] Definitions::SendStatusType sendSetTelemetryPeriodRequest(
        std::chrono::milliseconds telemetryPeriod, uint16_t &messageCounter);

    /*
     * Listen for received data and hand it to the link.
     */
    void onReceivedData(const uint8_t *data, size_t size);

    /*
     * Maintain the link.
     */
    void maintain();

public:
    /*
     * Get whether a request can be sent now.
     */
    [[nodiscard]] bool getSendRequestAllowed() const;

    /*
     * Get the number of requests that wait for an acknowledgement.
     */
    [[nodiscard]] size_t getPendingCount() const;

    /*
     * Get the link statistics.
     */
    [[nodiscard]] Link::LinkStatistics getLinkStatistics() const;

    /*
     * Get the number of ping responses to ping requests that this client did not send.
     */
    [[nodiscard]] uint64_t getIgnoredPingResponseCounter() const;

private:
    /*
     * Listen for a message of the device and get the message status that answers it.
     */
    [[nodiscard]] Definitions::MessageStatusType onReceivedMessage(const Frame::Frame &frame) final;

    /*
     * Listen for a request that reached the device.
     */
    void onDeliveryConfirmed(uint16_t messageCounter, uint16_t messageType) final;

    /*
     * Listen for a request that timed out or was rejected by the device.
     */
    void onDeliveryFailed(uint16_t messageCounter,
                          uint16_t messageType,
                          Definitions::DeliveryFailureType deliveryFailureType,
                          Definitions::MessageStatusType remoteMessageStatusType) final;

private:
    /*
     * Handle a ping response and measure the round trip time of its ping request.
     * A ping response to a ping request of another client is accepted and counted only.
     * NOTE: The request timestamp travels in 32 bits, so only its lower 32 bits are compared.
     */
    [[nodiscard]] Definitions::MessageStatusType handlePingResponse(const Frame::Frame &frame);

    /*
     * Handle a device info response.
     */
    [[nodiscard]] Definitions::MessageStatusType handleDeviceInfoResponse(const Frame::Frame &frame);

    /*
     * Handle a set LED response.
     */
    [[nodiscard]] Definitions::MessageStatusType handleSetLedResponse(const Frame::Frame &frame);

    /*
     * Handle a telemetry report response.
     */
    [[nodiscard]] Definitions::MessageStatusType handleTelemetryReportResponse(
        const Frame::Frame &frame);

    /*
     * Handle a log report response.
     */
    [[nodiscard]] Definitions::MessageStatusType handleLogReportResponse(const Frame::Frame &frame);

    /*
     * Send an acknowledged request and store its message counter.
     */
    [[nodiscard]] Definitions::SendStatusType sendRequest(Messages::MessageType messageType,
                                                          const uint8_t *payload,
                                                          size_t payloadLength,
                                                          uint16_t &messageCounter);

    /*
     * Remember a ping request in place of the oldest pending ping.
     */
    void rememberPendingPing(std::chrono::milliseconds requestTimestamp);

private:
    const Clock::Clock &clock;           // reference for clock
    DeviceClientHandler &handler;        // reference to handler
    Link::Link link;                     // link to the device
    size_t nextPendingPingIndex;         // pending ping to overwrite next
    uint64_t ignoredPingResponseCounter; // ping responses without a pending ping
    std::array<PendingPing, AppSettings::PING_HISTORY_SIZE> pendingPings; // sent ping requests
};

} // namespace sercom::DeviceClient

#endif // SERCOM_DEVICE_CLIENT_H
