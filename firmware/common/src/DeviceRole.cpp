#include "../include/DeviceRole.h"

namespace sercom::DeviceRole {

DeviceRole::DeviceRole(Transport::Transport &transport,
                       const Clock::Clock &clock,
                       FirmwarePlatform::FirmwarePlatform &firmwarePlatform,
                       Console::Console &console)
    : console(console)
    , frameTracer(console, clock, FrameTracer::TraceDirectionType::DEVICE_TO_HOST)
    , traceTransport(transport, this->frameTracer)
    , deviceApplication(console.getPrinted()
                            ? static_cast<Transport::Transport &>(this->traceTransport)
                            : transport,
                        clock,
                        firmwarePlatform)
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

    // start an activity blink after a valid received frame
    const uint64_t currentIncomingFrameCounter = this->deviceApplication.getLinkStatistics()
                                                     .incomingFrameCounter;
    if (currentIncomingFrameCounter != this->lastIncomingFrameCounter) {
        this->lastIncomingFrameCounter = currentIncomingFrameCounter;
        this->activityLed.startBlink();
    }

    // restore the LED state after an activity blink
    this->activityLed.processActivityLed();
}

Link::LinkStatistics DeviceRole::getLinkStatistics() const
{
    // expose the link statistics of the device application
    return this->deviceApplication.getLinkStatistics();
}

} // namespace sercom::DeviceRole
