#include "../include/UsartPort.h"

#include "../../common/include/CommonSettings.h"

#include "../include/FirmwareSettings.h"

namespace sercom::UsartPort {

namespace {

/*
 * Connect a pin of port A to the alternate function of its USART.
 */
void connectPin(const uint32_t pin, const uint32_t alternateFunction)
{
    // switch the pin to its alternate function
    const uint32_t modeShift = pin * FirmwareSettings::GPIO_MODE_WIDTH;
    GPIOA->MODER = (GPIOA->MODER & ~(FirmwareSettings::GPIO_MODE_MASK << modeShift))
                   | (FirmwareSettings::GPIO_MODE_ALTERNATE << modeShift);

    // select the alternate function of its USART
    const uint32_t alternateIndex = pin / FirmwareSettings::GPIO_ALTERNATE_PIN_COUNT;
    const uint32_t alternateShift = (pin % FirmwareSettings::GPIO_ALTERNATE_PIN_COUNT)
                                    * FirmwareSettings::GPIO_ALTERNATE_WIDTH;
    GPIOA->AFR[alternateIndex] = (GPIOA->AFR[alternateIndex]
                                  & ~(FirmwareSettings::GPIO_ALTERNATE_MASK << alternateShift))
                                 | (alternateFunction << alternateShift);
}

} // namespace

UsartPort::UsartPort(USART_TypeDef *usart,
                     const IRQn_Type interruptNumber,
                     const uint32_t serialReceivePin,
                     const uint32_t serialTransmitPin,
                     const uint32_t alternateFunction)
    : usart(usart)
    , interruptNumber(interruptNumber)
    , serialReceivePin(serialReceivePin)
    , serialTransmitPin(serialTransmitPin)
    , alternateFunction(alternateFunction)
    , receiveBuffer()
    , sendBuffer()
    , droppedByteCounter(0)
{}

UsartPort::~UsartPort() {}

void UsartPort::begin(const uint32_t baudRate)
{
    // connect the receive and transmit pins
    connectPin(this->serialReceivePin, this->alternateFunction);
    connectPin(this->serialTransmitPin, this->alternateFunction);

    // NOTE: the bus clocks of the USARTs run at the system clock after reset
    static constexpr uint32_t roundingDivisor = 2;
    this->usart->BRR = (FirmwareSettings::SystemClockFrequency + baudRate / roundingDivisor)
                       / baudRate;

    // open the USART 8N1 without flow control with its receive interrupt
    this->usart->CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE | USART_CR1_RXNEIE;
    NVIC_EnableIRQ(this->interruptNumber);

    // send the bytes written before the start
    this->startTransmission();
}

void UsartPort::processInterrupt()
{
    // move a received byte into the receive ring buffer, reading it also clears an overrun
    const uint32_t status = this->usart->SR;
    if ((status & (USART_SR_RXNE | USART_SR_ORE)) != 0) {
        const uint8_t receivedByte = static_cast<uint8_t>(this->usart->DR);
        if (!this->receiveBuffer.writeByte(receivedByte)) {
            ++this->droppedByteCounter;
        }
    }

    // transmit interrupt must be on and the transmit data register empty
    if ((this->usart->CR1 & USART_CR1_TXEIE) == 0 || (status & USART_SR_TXE) == 0) {
        return;
    }

    // stop the transmit interrupt when the transmit ring buffer is empty
    uint8_t sentByte = 0;
    if (!this->sendBuffer.readByte(sentByte)) {
        this->usart->CR1 &= ~USART_CR1_TXEIE;
        return;
    }

    // send the next byte
    this->usart->DR = sentByte;
}

size_t UsartPort::receiveData(uint8_t *data, const size_t size)
{
    // take the waiting bytes of the receive ring buffer
    return this->receiveBuffer.readData(data, size);
}

bool UsartPort::sendData(const uint8_t *data, const size_t size)
{
    // data must be smaller than the transmit ring buffer, which keeps one byte free
    if (size >= CommonSettings::RING_BUFFER_SIZE) {
        return false;
    }

    // wait for the transmit interrupt to make room
    while (this->sendBuffer.getAvailableSize() < size) {
        __WFI();
    }

    // every byte must be written into the transmit ring buffer
    if (!this->sendBuffer.writeData(data, size)) {
        return false;
    }

    // send the written bytes
    this->startTransmission();

    // data sent successfully
    return true;
}

size_t UsartPort::getAvailableSize() const
{
    // expose the free space of the transmit ring buffer
    return this->sendBuffer.getAvailableSize();
}

void UsartPort::writeText(const char *text, const size_t size)
{
    // text must fit into the transmit ring buffer
    if (!this->sendBuffer.writeData(reinterpret_cast<const uint8_t *>(text), size)) {
        return;
    }

    // send the written characters
    this->startTransmission();
}

uint32_t UsartPort::getDroppedByteCounter() const
{
    // expose the number of dropped received bytes
    return this->droppedByteCounter.load();
}

void UsartPort::startTransmission()
{
    // switch the transmit interrupt on
    this->usart->CR1 |= USART_CR1_TXEIE;
}

} // namespace sercom::UsartPort
