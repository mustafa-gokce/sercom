#include "../include/Stm32Platform.h"

#include <algorithm>
#include <chrono>
#include <limits>

#include <stm32f4xx.h>

#include "../../../app/include/Messages.h"

#include "../include/FirmwareSettings.h"

// NOTE: the linker script provides the end of the static data as the symbol _end
extern "C" uint32_t _end[];

namespace sercom::Stm32Platform {

Stm32Platform::Stm32Platform(const Clock::Clock &clock, const std::string_view deviceName)
    : clock(clock)
    , deviceName(deviceName)
    , ledOn(false)
    , paintedWordCount(0)
{}

Stm32Platform::~Stm32Platform() {}

void Stm32Platform::begin()
{
    // enable the clocks of port A, both USARTs and the ADC
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN;
    RCC->APB2ENR |= RCC_APB2ENR_USART1EN | RCC_APB2ENR_ADC1EN;

    // wait until the clocks run before the first register access
    __DSB();

    // configure the LED pin as an output and switch the LED off
    this->showLed(false);
    const uint32_t ledModeShift = FirmwareSettings::LED_PIN * FirmwareSettings::GPIO_MODE_WIDTH;
    GPIOA->MODER = (GPIOA->MODER & ~(FirmwareSettings::GPIO_MODE_MASK << ledModeShift))
                   | (FirmwareSettings::GPIO_MODE_OUTPUT << ledModeShift);

    // connect the temperature sensor to the first conversion of ADC1 with the longest sample time
    const uint32_t sampleTimeShift = (FirmwareSettings::TemperatureChannel
                                      - FirmwareSettings::ADC_FIRST_HIGH_CHANNEL)
                                     * FirmwareSettings::ADC_SAMPLE_TIME_WIDTH;
    ADC->CCR |= ADC_CCR_TSVREFE;
    ADC1->SMPR1 |= FirmwareSettings::ADC_SAMPLE_TIME << sampleTimeShift;
    ADC1->SQR3 = FirmwareSettings::TemperatureChannel;
    ADC1->CR2 |= ADC_CR2_ADON;

    // NOTE: the stack grows down from the end of the RAM towards the static data
    const uintptr_t freeMemoryStart = reinterpret_cast<uintptr_t>(_end);
    const uintptr_t paintEnd = __get_MSP() - FirmwareSettings::STACK_FILL_MARGIN_SIZE;
    if (paintEnd <= freeMemoryStart) {
        return;
    }

    // paint the free memory with the stack fill pattern
    this->paintedWordCount = (paintEnd - freeMemoryStart) / sizeof(uint32_t);
    std::fill_n(_end, this->paintedWordCount, FirmwareSettings::STACK_FILL_PATTERN);
}

bool Stm32Platform::getLedOn() const
{
    // expose whether the LED is on
    return this->ledOn;
}

void Stm32Platform::setLedOn(const bool ledOn)
{
    // switch the LED and keep its state
    this->showLed(ledOn);
    this->ledOn = ledOn;
}

uint32_t Stm32Platform::getFreeHeapSize() const
{
    // find the first word above the static data that the stack overwrote
    const uint32_t *iterator = std::find_if(_end,
                                            _end + this->paintedWordCount,
                                            [](const uint32_t word) {
                                                return word != FirmwareSettings::STACK_FILL_PATTERN;
                                            });

    // expose the untouched memory in bytes
    return static_cast<uint32_t>(static_cast<size_t>(iterator - _end) * sizeof(uint32_t));
}

int16_t Stm32Platform::getTemperature() const
{
    // start a conversion of the temperature sensor
    ADC1->CR2 |= ADC_CR2_SWSTART;

    // wait for the end of the conversion up to the conversion timeout
    const std::chrono::milliseconds conversionTimestamp = this->clock.getTime();
    bool converted = (ADC1->SR & ADC_SR_EOC) != 0;
    while (!converted
           && this->clock.getTime() - conversionTimestamp
                  < FirmwareSettings::TEMPERATURE_CONVERSION_TIMEOUT_MSEC) {
        converted = (ADC1->SR & ADC_SR_EOC) != 0;
    }

    // NOTE: QEMU does not model the conversion, so the temperature stays unavailable there
    if (!converted) {
        return Messages::UNAVAILABLE_TEMPERATURE;
    }

    // factory readings of the sensor must differ
    const int32_t reading = static_cast<int32_t>(ADC1->DR);
    const int32_t lowReading = *reinterpret_cast<const volatile uint16_t *>(
        FirmwareSettings::TEMPERATURE_CALIBRATION_LOW_ADDRESS);
    const int32_t highReading = *reinterpret_cast<const volatile uint16_t *>(
        FirmwareSettings::TEMPERATURE_CALIBRATION_HIGH_ADDRESS);
    if (highReading <= lowReading) {
        return Messages::UNAVAILABLE_TEMPERATURE;
    }

    // get the temperature on the line through both factory readings
    const int32_t temperature = FirmwareSettings::TEMPERATURE_CALIBRATION_LOW
                                + (reading - lowReading)
                                      * (FirmwareSettings::TEMPERATURE_CALIBRATION_HIGH
                                         - FirmwareSettings::TEMPERATURE_CALIBRATION_LOW)
                                      / (highReading - lowReading);

    // expose the temperature in the range of a reading
    static constexpr int32_t minimumTemperature = Messages::UNAVAILABLE_TEMPERATURE + 1;
    static constexpr int32_t maximumTemperature = std::numeric_limits<int16_t>::max();
    return static_cast<int16_t>(std::clamp(temperature, minimumTemperature, maximumTemperature));
}

std::string_view Stm32Platform::getDeviceName() const
{
    // expose the device name
    return this->deviceName;
}

void Stm32Platform::showLed(const bool ledLit)
{
    // set or reset the LED pin without changing the LED state
    const uint32_t ledShift = ledLit
                                  ? FirmwareSettings::LED_PIN
                                  : FirmwareSettings::LED_PIN + FirmwareSettings::GPIO_RESET_SHIFT;
    GPIOA->BSRR = FirmwareSettings::GPIO_PIN_MASK << ledShift;
}

} // namespace sercom::Stm32Platform
