#include "../include/FirmwareSettings.h"

namespace sercom::FirmwareSettings {

constexpr std::string_view ApplicationName = "DeviceApplication";

#if defined(LET_RUN_ON_QEMU)
constexpr uint32_t SystemClockFrequency = QEMU_CLOCK_FREQUENCY;
#else
constexpr uint32_t SystemClockFrequency = HSI_CLOCK_FREQUENCY;
#endif

#if defined(STM32F405xx)
constexpr uint32_t TemperatureChannel = STM32F405_TEMPERATURE_CHANNEL;
#else
constexpr uint32_t TemperatureChannel = STM32F401_TEMPERATURE_CHANNEL;
#endif

} // namespace sercom::FirmwareSettings
