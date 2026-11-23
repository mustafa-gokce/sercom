#include "../include/DeviceRole.h"

namespace sercom::DeviceRole {

DeviceRole::DeviceRole(Transport::Transport &transport,
                       const Clock::Clock &clock,
                       FirmwarePlatform::FirmwarePlatform &firmwarePlatform,
                       Console::Console &console,
                       const Router::RouterConfiguration &routerConfiguration)
    : DeviceRole(transport, nullptr, clock, firmwarePlatform, console, routerConfiguration)
{}

DeviceRole::DeviceRole(Transport::Transport &transport,
                       Transport::Transport *secondaryTransport,
                       const Clock::Clock &clock,
                       FirmwarePlatform::FirmwarePlatform &firmwarePlatform,
                       Console::Console &console,
                       const Router::RouterConfiguration &routerConfiguration)
    : console(console)
    , frameTracer(console, clock, FrameTracer::TraceDirectionType::DEVICE_TO_HOST)
    , traceTransport(transport, this->frameTracer)
    , deviceApplication(console.getPrinted()
                            ? static_cast<Transport::Transport &>(this->traceTransport)
                            : transport,
                        secondaryTransport,
                        clock,
                        firmwarePlatform,
                        Link::LinkConfiguration(),
                        routerConfiguration)
    , activityLed(firmwarePlatform, clock)
    , lastIncomingFrameCounter(0)
{}

DeviceRole::~DeviceRole() {}

void DeviceRole::onReceivedData(const uint8_t *data, const size_t size)
{
    // trace the received bytes on a printed console
    if (this->console.getPrinted()) {
        this->frameTracer.onReceivedData(data, size);
    }

    // feed the received bytes to the device application
    this->deviceApplication.onReceivedData(data, size);
}

void DeviceRole::onReceivedSecondaryData(const uint8_t *data, const size_t size)
{
    // feed the received bytes of the secondary port to the device application
    this->deviceApplication.onReceivedSecondaryData(data, size);
}

void DeviceRole::reportLog(const Messages::LogLevelType logLevelType, const std::string_view text)
{
    // print the log line and send it to the host
    this->console.printLog(logLevelType, text);
    [[maybe_unused]] const bool sent = this->deviceApplication.sendLogReportResponse(logLevelType,
                                                                                     text);
}

void DeviceRole::maintain()
{
    // maintain the device application
    this->deviceApplication.maintain();

    // maintain the frame tracer of a printed console
    if (this->console.getPrinted()) {
        this->frameTracer.maintain();
    }

    // start an activity blink after a valid received frame of either port
    const uint64_t currentIncomingFrameCounter
        = this->deviceApplication.getLinkStatistics().incomingFrameCounter
          + this->deviceApplication.getSecondaryLinkStatistics().incomingFrameCounter;
    if (currentIncomingFrameCounter != this->lastIncomingFrameCounter) {
        this->lastIncomingFrameCounter = currentIncomingFrameCounter;
        this->activityLed.startBlink();
    }

    // restore the LED state after an activity blink
    this->activityLed.processActivityLed();
}

Link::LinkStatistics DeviceRole::getLinkStatistics() const
{
    // expose the link statistics of the primary port of the device application
    return this->deviceApplication.getLinkStatistics();
}

Router::RouterStatistics DeviceRole::getRouterStatistics() const
{
    // expose the router statistics of the device application
    return this->deviceApplication.getRouterStatistics();
}

} // namespace sercom::DeviceRole
