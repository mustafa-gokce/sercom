#ifndef SERCOM_FIRMWARE_ROLE_H
#define SERCOM_FIRMWARE_ROLE_H

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "../../../app/include/Messages.h"

namespace sercom::FirmwareRole {

class FirmwareRole
{
public:
    /*
     * Role of the firmware on its protocol serial port, the device or the controller of a device.
     */
    FirmwareRole() = default;
    virtual ~FirmwareRole();

    // delete copy constructor and copy assignment operator
    FirmwareRole(const FirmwareRole &) = delete;
    FirmwareRole &operator=(const FirmwareRole &) = delete;

    // delete move constructor and move assignment operator
    FirmwareRole(FirmwareRole &&) = delete;
    FirmwareRole &operator=(FirmwareRole &&) = delete;

public:
    /*
     * Listen for received data of the protocol serial port.
     */
    virtual void onReceivedData(const uint8_t *data, size_t size) = 0;

    /*
     * Report a log line on the console and to the peer that collects the log lines.
     */
    virtual void reportLog(Messages::LogLevelType logLevelType, std::string_view text) = 0;

    /*
     * Maintain the protocol and the activity LED.
     */
    virtual void maintain() = 0;
};

} // namespace sercom::FirmwareRole

#endif // SERCOM_FIRMWARE_ROLE_H
