#include "../include/FirmwareSettings.h"

#include "../../../core/include/Definitions.h"

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

#if defined(LET_RUN_ROUTER)
constexpr bool RouterRun = true;
#else
constexpr bool RouterRun = false;
#endif

#if defined(LET_RUN_CONTROLLER)
constexpr bool ControllerRun = true;
constexpr std::string_view ApplicationName = "DeviceClient";
#else
constexpr bool ControllerRun = false;
constexpr std::string_view ApplicationName = RouterRun ? "Router" : "DeviceApplication";
#endif

#if defined(NODE_ADDRESS)
constexpr uint8_t NodeAddress = NODE_ADDRESS;
#else
constexpr uint8_t NodeAddress = Definitions::UNASSIGNED_ADDRESS;
#endif

#if defined(PROXY_ADDRESS)
constexpr uint8_t ProxyAddress = PROXY_ADDRESS;
#else
constexpr uint8_t ProxyAddress = Definitions::UNASSIGNED_ADDRESS;
#endif

constexpr std::string_view SecondaryPortName = UART2_PORT_NAME;
constexpr int8_t SecondaryReceivePin = UART2_RECEIVE_PIN;
constexpr int8_t SecondaryTransmitPin = UART2_TRANSMIT_PIN;

} // namespace sercom::FirmwareSettings

// check settings
static_assert(!sercom::FirmwareSettings::RouterRun || !sercom::FirmwareSettings::ConsolePrinted,
              "Router must print no console, its secondary port is UART2");
static_assert(!sercom::FirmwareSettings::RouterRun || !sercom::FirmwareSettings::ControllerRun,
              "Router must run the device role");
static_assert(!sercom::FirmwareSettings::RouterRun
                  || sercom::FirmwareSettings::NodeAddress
                         != sercom::Definitions::UNASSIGNED_ADDRESS,
              "Router must have a node address");
static_assert(
    sercom::FirmwareSettings::NodeAddress == sercom::Definitions::UNASSIGNED_ADDRESS
        || (sercom::FirmwareSettings::NodeAddress >= sercom::Definitions::MINIMUM_NODE_ADDRESS
            && sercom::FirmwareSettings::NodeAddress <= sercom::Definitions::MAXIMUM_NODE_ADDRESS),
    "Node address must be from 2 to 254");
static_assert(
    sercom::FirmwareSettings::ProxyAddress == sercom::Definitions::UNASSIGNED_ADDRESS
        || (sercom::FirmwareSettings::RouterRun
            && sercom::FirmwareSettings::ProxyAddress >= sercom::Definitions::MINIMUM_NODE_ADDRESS
            && sercom::FirmwareSettings::ProxyAddress <= sercom::Definitions::MAXIMUM_NODE_ADDRESS
            && sercom::FirmwareSettings::ProxyAddress != sercom::FirmwareSettings::NodeAddress),
    "Proxy address must be another node address of a router");
