#include "../include/DeviceApplication.h"

#include "../include/AppSettings.h"

namespace sercom::DeviceApplication {

using namespace std::chrono_literals;

DevicePlatform::~DevicePlatform() {}

DeviceApplication::DeviceApplication(Transport::Transport &transport,
                                     const Clock::Clock &clock,
                                     DevicePlatform &devicePlatform,
                                     const Link::LinkConfiguration &linkConfiguration,
                                     const Router::RouterConfiguration &routerConfiguration)
    : DeviceApplication(transport,
                        nullptr,
                        clock,
                        devicePlatform,
                        linkConfiguration,
                        routerConfiguration)
{}

DeviceApplication::DeviceApplication(Transport::Transport &transport,
                                     Transport::Transport *secondaryTransport,
                                     const Clock::Clock &clock,
                                     DevicePlatform &devicePlatform,
                                     const Link::LinkConfiguration &linkConfiguration,
                                     const Router::RouterConfiguration &routerConfiguration)
    : clock(clock)
    , devicePlatform(devicePlatform)
    , router(transport, secondaryTransport, *this, clock, linkConfiguration, routerConfiguration)
    , startTimestamp(clock.getTime())
    , telemetryPeriod(AppSettings::DEFAULT_TELEMETRY_PERIOD_MSEC)
    , lastTelemetryTimestamp(this->startTimestamp)
    , telemetrySequence(0)
    , reportAddress(routerConfiguration.nodeAddress == Definitions::UNASSIGNED_ADDRESS
                        ? Definitions::UNASSIGNED_ADDRESS
                        : Definitions::HOST_ADDRESS)
{}

DeviceApplication::~DeviceApplication() {}

void DeviceApplication::onReceivedData(const uint8_t *data, const size_t size)
{
    // hand the data of the primary port to the router
    this->router.onReceivedData(Definitions::PortType::PRIMARY, data, size);
}

void DeviceApplication::onReceivedSecondaryData(const uint8_t *data, const size_t size)
{
    // hand the data of the secondary port to the router
    this->router.onReceivedData(Definitions::PortType::SECONDARY, data, size);
}

void DeviceApplication::maintain()
{
    // maintain the router
    this->router.maintain();

    // telemetry must be on
    if (this->telemetryPeriod == 0ms) {
        return;
    }

    // telemetry report response must be due
    const std::chrono::milliseconds currentTime = this->clock.getTime();
    if (currentTime - this->lastTelemetryTimestamp < this->telemetryPeriod) {
        return;
    }

    // NOTE: restarting the period from now avoids a burst after a stall
    this->lastTelemetryTimestamp = currentTime;
    this->sendTelemetryReportResponse();
}

bool DeviceApplication::sendLogReportResponse(const Messages::LogLevelType logLevelType,
                                              const std::string_view text)
{
    // log report response must hold the whole text
    const Messages::LogReportResponse logReportResponse(logLevelType, text);
    if (!logReportResponse.getLikelyValid()) {
        return false;
    }

    // log report response must be sent without acknowledgement
    uint16_t messageCounter = 0;
    const Definitions::SendStatusType sendStatusType
        = this->router.sendMessage(this->reportAddress,
                                   Messages::fromMessageType(
                                       Messages::MessageType::LOG_REPORT_RESPONSE),
                                   logReportResponse.getPayload(),
                                   logReportResponse.getPayloadLength(),
                                   false,
                                   messageCounter);
    if (sendStatusType != Definitions::SendStatusType::SENT) {
        return false;
    }

    // log report response sent successfully
    return true;
}

std::chrono::milliseconds DeviceApplication::getTelemetryPeriod() const
{
    // expose the telemetry period
    return this->telemetryPeriod;
}

Link::LinkStatistics DeviceApplication::getLinkStatistics() const
{
    // expose the link statistics of the primary port
    return this->router.getLinkStatistics(Definitions::PortType::PRIMARY);
}

Link::LinkStatistics DeviceApplication::getSecondaryLinkStatistics() const
{
    // expose the link statistics of the secondary port
    return this->router.getLinkStatistics(Definitions::PortType::SECONDARY);
}

Router::RouterStatistics DeviceApplication::getRouterStatistics() const
{
    // expose the router statistics
    return this->router.getStatistics();
}

Definitions::MessageStatusType DeviceApplication::onReceivedMessage(const Frame::Frame &frame)
{
    // send the reports to the source of the latest request
    this->reportAddress = frame.getSourceAddress();

    // handle the message by its message type
    switch (Messages::toMessageType(frame.getMessageType())) {
    // handle a ping request
    case Messages::MessageType::PING_REQUEST: {
        return this->handlePingRequest(frame);
    }

    // handle a device info request
    case Messages::MessageType::DEVICE_INFO_REQUEST: {
        return this->handleDeviceInfoRequest(frame);
    }

    // handle an echo request
    case Messages::MessageType::ECHO_REQUEST: {
        return this->handleEchoRequest(frame);
    }

    // handle a set LED request
    case Messages::MessageType::SET_LED_REQUEST: {
        return this->handleSetLedRequest(frame);
    }

    // handle a set telemetry period request
    case Messages::MessageType::SET_TELEMETRY_PERIOD_REQUEST: {
        return this->handleSetTelemetryPeriodRequest(frame);
    }

    // do not handle responses and unknown messages
    case Messages::MessageType::PING_RESPONSE:
    case Messages::MessageType::DEVICE_INFO_RESPONSE:
    case Messages::MessageType::LOG_REPORT_RESPONSE:
    case Messages::MessageType::ECHO_RESPONSE:
    case Messages::MessageType::ROUTE_FAILURE_REPORT_RESPONSE:
    case Messages::MessageType::SET_LED_RESPONSE:
    case Messages::MessageType::TELEMETRY_REPORT_RESPONSE:
    case Messages::MessageType::UNKNOWN:
    default: {
        return Definitions::MessageStatusType::UNSUPPORTED_MESSAGE_TYPE;
    }
    }
}

void DeviceApplication::onDeliveryConfirmed([[maybe_unused]] const uint16_t messageCounter,
                                            [[maybe_unused]] const uint16_t messageType)
{}

void DeviceApplication::onDeliveryFailed(
    [[maybe_unused]] const uint16_t messageCounter,
    [[maybe_unused]] const uint16_t messageType,
    [[maybe_unused]] const Definitions::DeliveryFailureType deliveryFailureType,
    [[maybe_unused]] const Definitions::MessageStatusType remoteMessageStatusType)
{}

Definitions::MessageStatusType DeviceApplication::handlePingRequest(const Frame::Frame &frame)
{
    // response must be allowed before any work
    if (!this->getSendResponseAllowed(frame)) {
        return Definitions::MessageStatusType::BUSY;
    }

    // get ping request arguments
    const Messages::PingRequest pingRequest(frame);
    if (!pingRequest.getLikelyValid()) {
        return Definitions::MessageStatusType::MALFORMED_PAYLOAD;
    }

    // create ping response
    const Messages::PingResponse pingResponse(pingRequest.getRequestTimestamp(), this->getUptime());

    // send the ping response
    return this->sendResponse(frame,
                              Messages::MessageType::PING_RESPONSE,
                              pingResponse.getPayload(),
                              pingResponse.getPayloadLength());
}

Definitions::MessageStatusType DeviceApplication::handleDeviceInfoRequest(const Frame::Frame &frame)
{
    // response must be allowed before any work
    if (!this->getSendResponseAllowed(frame)) {
        return Definitions::MessageStatusType::BUSY;
    }

    // device info response must hold the whole device name, a node with an address appends its own
    const uint8_t nodeAddress = this->router.getNodeAddress();
    const Messages::DeviceInfoResponse deviceInfoResponse
        = nodeAddress == Definitions::UNASSIGNED_ADDRESS
              ? Messages::DeviceInfoResponse(AppSettings::AppVersionMajor,
                                             AppSettings::AppVersionMinor,
                                             this->getUptime(),
                                             this->devicePlatform.getFreeHeapSize(),
                                             this->devicePlatform.getDeviceName())
              : Messages::DeviceInfoResponse(AppSettings::AppVersionMajor,
                                             AppSettings::AppVersionMinor,
                                             this->getUptime(),
                                             this->devicePlatform.getFreeHeapSize(),
                                             this->devicePlatform.getDeviceName(),
                                             Definitions::ROUTED_PROTOCOL_VERSION,
                                             nodeAddress);
    if (!deviceInfoResponse.getLikelyValid()) {
        return Definitions::MessageStatusType::UNKNOWN;
    }

    // send the device info response
    return this->sendResponse(frame,
                              Messages::MessageType::DEVICE_INFO_RESPONSE,
                              deviceInfoResponse.getPayload(),
                              deviceInfoResponse.getPayloadLength());
}

Definitions::MessageStatusType DeviceApplication::handleEchoRequest(const Frame::Frame &frame)
{
    // response must be allowed before any work
    if (!this->getSendResponseAllowed(frame)) {
        return Definitions::MessageStatusType::BUSY;
    }

    // get echo request arguments
    const Messages::EchoRequest echoRequest(frame);

    // create echo response
    const Messages::EchoResponse echoResponse(echoRequest.getPayload(),
                                              echoRequest.getPayloadLength());

    // send the echo response
    return this->sendResponse(frame,
                              Messages::MessageType::ECHO_RESPONSE,
                              echoResponse.getPayload(),
                              echoResponse.getPayloadLength());
}

Definitions::MessageStatusType DeviceApplication::handleSetLedRequest(const Frame::Frame &frame)
{
    // response must be allowed before switching anything
    if (!this->getSendResponseAllowed(frame)) {
        return Definitions::MessageStatusType::BUSY;
    }

    // get set LED request arguments
    const Messages::SetLedRequest setLedRequest(frame);
    if (!setLedRequest.getLikelyValid()) {
        return Definitions::MessageStatusType::MALFORMED_PAYLOAD;
    }

    // switch the LED
    this->devicePlatform.setLedOn(setLedRequest.getLedOn());

    // create set LED response with the state of the hardware
    const Messages::SetLedResponse setLedResponse(this->devicePlatform.getLedOn());

    // send the set LED response
    return this->sendResponse(frame,
                              Messages::MessageType::SET_LED_RESPONSE,
                              setLedResponse.getPayload(),
                              setLedResponse.getPayloadLength());
}

Definitions::MessageStatusType DeviceApplication::handleSetTelemetryPeriodRequest(
    const Frame::Frame &frame)
{
    // get set telemetry period request arguments
    const Messages::SetTelemetryPeriodRequest setTelemetryPeriodRequest(frame);
    if (!setTelemetryPeriodRequest.getLikelyValid()) {
        return Definitions::MessageStatusType::MALFORMED_PAYLOAD;
    }

    // telemetry period must be zero or at least the minimum telemetry period
    const std::chrono::milliseconds requestedTelemetryPeriod = setTelemetryPeriodRequest
                                                                   .getTelemetryPeriod();
    if (requestedTelemetryPeriod != 0ms
        && requestedTelemetryPeriod < AppSettings::MINIMUM_TELEMETRY_PERIOD_MSEC) {
        return Definitions::MessageStatusType::INVALID_ARGUMENT;
    }

    // set the telemetry period
    this->telemetryPeriod = requestedTelemetryPeriod;
    this->lastTelemetryTimestamp = this->clock.getTime();

    // set telemetry period request handled successfully
    return Definitions::MessageStatusType::ACCEPTED;
}

bool DeviceApplication::getSendResponseAllowed(const Frame::Frame &frame) const
{
    // expose whether the response can be sent now
    return !frame.getFlag(Definitions::FrameFlagType::ACK_REQUEST)
           || this->router.getSendAcknowledgedAllowed(frame.getSourceAddress());
}

Definitions::MessageStatusType DeviceApplication::sendResponse(
    const Frame::Frame &frame,
    const Messages::MessageType messageType,
    const uint8_t *payload,
    const size_t payloadLength)
{
    // response must be sent
    uint16_t messageCounter = 0;
    const Definitions::SendStatusType sendStatusType
        = this->router.sendMessage(frame.getSourceAddress(),
                                   Messages::fromMessageType(messageType),
                                   payload,
                                   payloadLength,
                                   frame.getFlag(Definitions::FrameFlagType::ACK_REQUEST),
                                   messageCounter);
    if (sendStatusType != Definitions::SendStatusType::SENT) {
        return Definitions::MessageStatusType::BUSY;
    }

    // request handled successfully
    return Definitions::MessageStatusType::ACCEPTED;
}

void DeviceApplication::sendTelemetryReportResponse()
{
    // create telemetry report response
    const Messages::TelemetryReportResponse
        telemetryReportResponse(this->telemetrySequence,
                                this->getUptime(),
                                this->devicePlatform.getFreeHeapSize(),
                                this->devicePlatform.getTemperature());

    // advance the sequence that wraps from 65535 to 0
    ++this->telemetrySequence;

    // send the telemetry report response without acknowledgement
    uint16_t messageCounter = 0;
    [[maybe_unused]] const Definitions::SendStatusType sendStatusType
        = this->router.sendMessage(this->reportAddress,
                                   Messages::fromMessageType(
                                       Messages::MessageType::TELEMETRY_REPORT_RESPONSE),
                                   telemetryReportResponse.getPayload(),
                                   telemetryReportResponse.getPayloadLength(),
                                   false,
                                   messageCounter);
}

std::chrono::milliseconds DeviceApplication::getUptime() const
{
    // expose the time since the construction
    return this->clock.getTime() - this->startTimestamp;
}

} // namespace sercom::DeviceApplication
