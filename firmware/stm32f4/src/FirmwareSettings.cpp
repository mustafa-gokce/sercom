#include "../include/FirmwareSettings.h"

namespace sercom::FirmwareSettings {

#if defined(LET_PRINT_CONSOLE)
constexpr bool ConsolePrinted = true;
constexpr std::string_view SerialPortName = USART1_PORT_NAME;
constexpr uint32_t SerialReceivePin = USART1_RECEIVE_PIN;
constexpr uint32_t SerialTransmitPin = USART1_TRANSMIT_PIN;
#else
constexpr bool ConsolePrinted = false;
constexpr std::string_view SerialPortName = USART2_PORT_NAME;
constexpr uint32_t SerialReceivePin = USART2_RECEIVE_PIN;
constexpr uint32_t SerialTransmitPin = USART2_TRANSMIT_PIN;
#endif

#if defined(LET_RUN_CONTROLLER)
constexpr bool ControllerRun = true;
constexpr std::string_view ApplicationName = "DeviceClient";
#else
constexpr bool ControllerRun = false;
constexpr std::string_view ApplicationName = "DeviceApplication";
#endif

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
