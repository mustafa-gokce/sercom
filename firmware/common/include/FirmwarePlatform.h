#ifndef SERCOM_FIRMWARE_PLATFORM_H
#define SERCOM_FIRMWARE_PLATFORM_H

#include "../../../app/include/DeviceApplication.h"

namespace sercom::FirmwarePlatform {

class FirmwarePlatform : public DeviceApplication::DevicePlatform
{
public:
    /*
     * Hardware of a firmware target whose LED can also show an activity blink, implemented once per target.
     */
    FirmwarePlatform() = default;
    ~FirmwarePlatform() override;

    // delete copy constructor and copy assignment operator
    FirmwarePlatform(const FirmwarePlatform &) = delete;
    FirmwarePlatform &operator=(const FirmwarePlatform &) = delete;

    // delete move constructor and move assignment operator
    FirmwarePlatform(FirmwarePlatform &&) = delete;
    FirmwarePlatform &operator=(FirmwarePlatform &&) = delete;

public:
    /*
     * Show the LED lit or dark without changing whether the LED is on.
     */
    virtual void showLed(bool ledLit) = 0;
};

} // namespace sercom::FirmwarePlatform

#endif // SERCOM_FIRMWARE_PLATFORM_H
