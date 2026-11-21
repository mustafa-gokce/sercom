#ifndef SERCOM_FIRMWARE_SETTINGS_H
#define SERCOM_FIRMWARE_SETTINGS_H

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include "../../../app/include/Messages.h"

namespace sercom::FirmwareSettings {

using namespace std::chrono_literals;

extern const bool ConsolePrinted;              // console on USART2 and protocol on USART1
extern const bool ControllerRun;               // controller role instead of the device role
extern const std::string_view ApplicationName; // name of the application on the protocol serial port
extern const std::string_view SerialPortName;  // name of the protocol serial port
extern const uint32_t SerialReceivePin;        // port A pin of the protocol receive pin
extern const uint32_t SerialTransmitPin;       // port A pin of the protocol transmit pin
extern const uint32_t SystemClockFrequency;    // frequency of the system clock in hertz
extern const uint32_t TemperatureChannel;      // ADC1 channel of the internal temperature sensor

constexpr std::string_view USART1_PORT_NAME = "USART1"; // name of the serial port on PA9 and PA10
constexpr uint32_t USART1_RECEIVE_PIN = 10;             // port A pin of the USART1 receive pin
constexpr uint32_t USART1_TRANSMIT_PIN = 9;             // port A pin of the USART1 transmit pin
constexpr std::string_view USART2_PORT_NAME = "USART2"; // name of the serial port of the ST-LINK
constexpr uint32_t USART2_RECEIVE_PIN = 3;              // port A pin of the USART2 receive pin
constexpr uint32_t USART2_TRANSMIT_PIN = 2;             // port A pin of the USART2 transmit pin
constexpr uint32_t USART_ALTERNATE_FUNCTION = 7; // alternate function of both USARTs on port A

constexpr uint32_t LED_PIN = 5;                            // port A pin of the user LED LD2
constexpr std::string_view DEVICE_NAME = "sercom-stm32f4"; // reported device name

constexpr uint32_t HSI_CLOCK_FREQUENCY = 16000000;   // internal oscillator after reset in hertz
constexpr uint32_t QEMU_CLOCK_FREQUENCY = 168000000; // fixed clock of the QEMU machine in hertz
constexpr uint32_t SYSTICK_FREQUENCY = 1000;         // SysTick interrupts per second

constexpr uint32_t GPIO_PIN_MASK = 0x1;    // bit of one pin in the set and reset register
constexpr uint32_t GPIO_RESET_SHIFT = 16;  // offset of the reset bits in the set and reset register
constexpr uint32_t GPIO_MODE_MASK = 0x3;   // mode bits of one pin
constexpr uint32_t GPIO_MODE_WIDTH = 2;    // mode bits of one pin in the mode register
constexpr uint32_t GPIO_MODE_OUTPUT = 0x1; // mode of an output pin
constexpr uint32_t GPIO_MODE_ALTERNATE = 0x2;    // mode of an alternate function pin
constexpr uint32_t GPIO_ALTERNATE_MASK = 0xF;    // alternate function bits of one pin
constexpr uint32_t GPIO_ALTERNATE_WIDTH = 4;     // alternate function bits of one pin in a register
constexpr uint32_t GPIO_ALTERNATE_PIN_COUNT = 8; // pins of one alternate function register

constexpr uint32_t STM32F405_TEMPERATURE_CHANNEL = 16; // ADC1 channel of the STM32F405 sensor
constexpr uint32_t STM32F401_TEMPERATURE_CHANNEL = 18; // ADC1 channel of the STM32F401 and F411 sensor
constexpr uint32_t ADC_SAMPLE_TIME = 7;         // sample time bits of the longest sample time
constexpr uint32_t ADC_SAMPLE_TIME_WIDTH = 3;   // sample time bits of one channel in a register
constexpr uint32_t ADC_FIRST_HIGH_CHANNEL = 10; // first channel of the sample time register 1
constexpr uintptr_t TEMPERATURE_CALIBRATION_LOW_ADDRESS
    = 0x1FFF7A2C; // address of the factory reading at the low calibration temperature
constexpr uintptr_t TEMPERATURE_CALIBRATION_HIGH_ADDRESS
    = 0x1FFF7A2E; // address of the factory reading at the high calibration temperature
constexpr int32_t TEMPERATURE_CALIBRATION_LOW
    = 3000; // low calibration temperature in 0.01 degree Celsius steps
constexpr int32_t TEMPERATURE_CALIBRATION_HIGH
    = 11000; // high calibration temperature in 0.01 degree Celsius steps
constexpr std::chrono::milliseconds TEMPERATURE_CONVERSION_TIMEOUT_MSEC
    = 2ms; // longest wait for a temperature conversion in milliseconds

constexpr uint32_t STACK_FILL_PATTERN = 0xA5A5A5A5; // pattern of the memory the stack never reached
constexpr size_t STACK_FILL_MARGIN_SIZE = 256; // unpainted bytes below the painting stack frame

} // namespace sercom::FirmwareSettings

// check settings
static_assert(sercom::FirmwareSettings::DEVICE_NAME.size()
                  <= sercom::Messages::MAXIMUM_DEVICE_NAME_LENGTH,
              "Device name must be at most 31 characters");

#endif // SERCOM_FIRMWARE_SETTINGS_H
