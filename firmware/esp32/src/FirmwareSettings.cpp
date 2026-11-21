#include "../include/FirmwareSettings.h"

namespace sercom::FirmwareSettings {

#if defined(LET_PRINT_CONSOLE)
constexpr bool ConsolePrinted = true;
constexpr std::string_view SerialPortName = UART2_PORT_NAME;
constexpr int8_t SerialReceivePin = UART2_RECEIVE_PIN;
constexpr int8_t SerialTransmitPin = UART2_TRANSMIT_PIN;
#else
constexpr bool ConsolePrinted = false;
constexpr std::string_view SerialPortName = UART0_PORT_NAME;
constexpr int8_t SerialReceivePin = UART0_RECEIVE_PIN;
constexpr int8_t SerialTransmitPin = UART0_TRANSMIT_PIN;
#endif

} // namespace sercom::FirmwareSettings
