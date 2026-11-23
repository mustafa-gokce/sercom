#ifndef SERCOM_DEVICE_ROLE_H
#define SERCOM_DEVICE_ROLE_H

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "../../../app/include/DeviceApplication.h"
#include "../../../app/include/Messages.h"
#include "../../../core/include/Clock.h"
#include "../../../core/include/FrameTracer.h"
#include "../../../core/include/Link.h"
#include "../../../core/include/Router.h"
#include "../../../core/include/TraceTransport.h"
#include "../../../core/include/Transport.h"

#include "../include/ActivityLed.h"
#include "../include/Console.h"
#include "../include/FirmwarePlatform.h"
#include "../include/FirmwareRole.h"

namespace sercom::DeviceRole {

class DeviceRole final : public FirmwareRole::FirmwareRole
{
public:
    /*
     * Device role that answers a host or a controller with the device application.
     * The constructor with a secondary transport makes the device a router between two ports, a null one an end node.
     * A printed console shows every frame of the primary port and every log line.
     * The LED blinks after a valid frame of either port.
     */
    explicit DeviceRole(
        Transport::Transport &transport,
        const Clock::Clock &clock,
        FirmwarePlatform::FirmwarePlatform &firmwarePlatform,
        Console::Console &console,
        const Router::RouterConfiguration &routerConfiguration = Router::RouterConfiguration());
    explicit DeviceRole(Transport::Transport &transport,
                        Transport::Transport *secondaryTransport,
                        const Clock::Clock &clock,
                        FirmwarePlatform::FirmwarePlatform &firmwarePlatform,
                        Console::Console &console,
                        const Router::RouterConfiguration &routerConfiguration);
    ~DeviceRole() final;

    // delete copy constructor and copy assignment operator
    DeviceRole(const DeviceRole &) = delete;
    DeviceRole &operator=(const DeviceRole &) = delete;

    // delete move constructor and move assignment operator
    DeviceRole(DeviceRole &&) = delete;
    DeviceRole &operator=(DeviceRole &&) = delete;

public:
    /*
     * Listen for received data of the primary port, trace it on a printed console and hand it to the device application.
     */
    void onReceivedData(const uint8_t *data, size_t size) final;

    /*
     * Listen for received data of the secondary port and hand it to the device application.
     */
    void onReceivedSecondaryData(const uint8_t *data, size_t size);

    /*
     * Print a log line on the console and send it to the host.
     */
    void reportLog(Messages::LogLevelType logLevelType, std::string_view text) final;

    /*
     * Maintain the device application, the frame tracer of a printed console and the activity LED.
     */
    void maintain() final;

public:
    /*
     * Get the link statistics of the primary port of the device application.
     */
    [[nodiscard]] Link::LinkStatistics getLinkStatistics() const;

    /*
     * Get the router statistics of the device application.
     */
    [[nodiscard]] Router::RouterStatistics getRouterStatistics() const;

private:
    Console::Console &console;                              // reference for console
    FrameTracer::FrameTracer frameTracer;                   // tracer of the protocol bytes
    TraceTransport::TraceTransport traceTransport;          // transport tracing the sent bytes
    DeviceApplication::DeviceApplication deviceApplication; // device side of the protocol
    ActivityLed::ActivityLed activityLed;                   // activity blink of the LED
    uint64_t lastIncomingFrameCounter; // valid frames of both ports at the latest blink check
};

} // namespace sercom::DeviceRole

#endif // SERCOM_DEVICE_ROLE_H
