#ifndef SERCOM_DEVICE_APPLICATION_H
#define SERCOM_DEVICE_APPLICATION_H

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include "../../core/include/Clock.h"
#include "../../core/include/Definitions.h"
#include "../../core/include/Frame.h"
#include "../../core/include/Link.h"
#include "../../core/include/Transport.h"

#include "../include/Messages.h"

namespace sercom::DeviceApplication {

class DevicePlatform
{
public:
    /*
     * Hardware the device application controls, implemented once per target.
     */
    DevicePlatform() = default;
    virtual ~DevicePlatform();

    // delete copy constructor and copy assignment operator
    DevicePlatform(const DevicePlatform &) = delete;
    DevicePlatform &operator=(const DevicePlatform &) = delete;

    // delete move constructor and move assignment operator
    DevicePlatform(DevicePlatform &&) = delete;
    DevicePlatform &operator=(DevicePlatform &&) = delete;

public:
    /*
     * Get whether the LED is on.
     */
    [[nodiscard]] virtual bool getLedOn() const = 0;

    /*
     * Set whether the LED is on.
     */
    virtual void setLedOn(bool ledOn) = 0;

    /*
     * Get the free heap size in bytes.
     */
    [[nodiscard]] virtual uint32_t getFreeHeapSize() const = 0;

    /*
     * Get the temperature in 0.01 degree Celsius steps.
     */
    [[nodiscard]] virtual int16_t getTemperature() const = 0;

    /*
     * Get the device name.
     */
    [[nodiscard]] virtual std::string_view getDeviceName() const = 0;
};

class DeviceApplication final : public Link::LinkHandler
{
public:
    /*
     * Device side of the demo protocol, shared by the firmware and the device simulator.
     */
    explicit DeviceApplication(
        Transport::Transport &transport,
        const Clock::Clock &clock,
        DevicePlatform &devicePlatform,
        const Link::LinkConfiguration &linkConfiguration = Link::LinkConfiguration());
    ~DeviceApplication() final;

    // delete copy constructor and copy assignment operator
    DeviceApplication(const DeviceApplication &) = delete;
    DeviceApplication &operator=(const DeviceApplication &) = delete;

    // delete move constructor and move assignment operator
    DeviceApplication(DeviceApplication &&) = delete;
    DeviceApplication &operator=(DeviceApplication &&) = delete;

public:
    /*
     * Listen for received data and hand it to the link.
     */
    void onReceivedData(const uint8_t *data, size_t size);

    /*
     * Maintain the link and send the telemetry report response when it is due.
     */
    void maintain();

    /*
     * Send a log report response without acknowledgement.
     * A lost log line costs less than a window full of acknowledged messages.
     * A text above the maximum log text length is not sent.
     */
    [[nodiscard]] bool sendLogReportResponse(Messages::LogLevelType logLevelType,
                                             std::string_view text);

public:
    /*
     * Get the telemetry period, zero when the telemetry is off.
     */
    [[nodiscard]] std::chrono::milliseconds getTelemetryPeriod() const;

    /*
     * Get the link statistics.
     */
    [[nodiscard]] Link::LinkStatistics getLinkStatistics() const;

private:
    /*
     * Listen for a message of the host and get the message status that answers it.
     */
    [[nodiscard]] Definitions::MessageStatusType onReceivedMessage(const Frame::Frame &frame) final;

    /*
     * Listen for a response that reached the host.
     * The link statistics count the confirmed responses.
     */
    void onDeliveryConfirmed(uint16_t messageCounter, uint16_t messageType) final;

    /*
     * Listen for a response that timed out or was rejected by the host.
     * The host sends its request again when the response never arrives.
     */
    void onDeliveryFailed(uint16_t messageCounter,
                          uint16_t messageType,
                          Definitions::DeliveryFailureType deliveryFailureType,
                          Definitions::MessageStatusType remoteMessageStatusType) final;

private:
    /*
     * Handle a ping request with a ping response.
     */
    [[nodiscard]] Definitions::MessageStatusType handlePingRequest(const Frame::Frame &frame);

    /*
     * Handle a device info request with a device info response.
     */
    [[nodiscard]] Definitions::MessageStatusType handleDeviceInfoRequest(const Frame::Frame &frame);

    /*
     * Handle an echo request with an echo response.
     */
    [[nodiscard]] Definitions::MessageStatusType handleEchoRequest(const Frame::Frame &frame);

    /*
     * Handle a set LED request with a set LED response.
     */
    [[nodiscard]] Definitions::MessageStatusType handleSetLedRequest(const Frame::Frame &frame);

    /*
     * Handle a set telemetry period request, its acknowledgement is the response.
     */
    [[nodiscard]] Definitions::MessageStatusType handleSetTelemetryPeriodRequest(
        const Frame::Frame &frame);

    /*
     * Get whether the response to a request frame can be sent now.
     */
    [[nodiscard]] bool getSendResponseAllowed(const Frame::Frame &frame) const;

    /*
     * Send a response, acknowledged when its request frame asks for an acknowledgement.
     */
    [[nodiscard]] Definitions::MessageStatusType sendResponse(const Frame::Frame &frame,
                                                              Messages::MessageType messageType,
                                                              const uint8_t *payload,
                                                              size_t payloadLength);

    /*
     * Send a telemetry report response without acknowledgement.
     * The next telemetry report response replaces a lost one.
     */
    void sendTelemetryReportResponse();

    /*
     * Get the time since the construction.
     */
    [[nodiscard]] std::chrono::milliseconds getUptime() const;

private:
    const Clock::Clock &clock;                        // reference for clock
    DevicePlatform &devicePlatform;                   // reference for device platform
    Link::Link link;                                  // link to the host
    const std::chrono::milliseconds startTimestamp;   // time of the construction
    std::chrono::milliseconds telemetryPeriod;        // period of the telemetry report responses
    std::chrono::milliseconds lastTelemetryTimestamp; // time of the last telemetry report response
    uint16_t telemetrySequence; // sequence of the next telemetry report response
};

} // namespace sercom::DeviceApplication

#endif // SERCOM_DEVICE_APPLICATION_H
