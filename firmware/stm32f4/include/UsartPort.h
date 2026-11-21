#ifndef SERCOM_USART_PORT_H
#define SERCOM_USART_PORT_H

#include <atomic>
#include <cstddef>
#include <cstdint>

#include <stm32f4xx.h>

#include "../../../core/include/Transport.h"
#include "../../common/include/ConsoleOutput.h"
#include "../../common/include/RingBuffer.h"

namespace sercom::UsartPort {

class UsartPort final : public Transport::Transport, public ConsoleOutput::ConsoleOutput
{
public:
    /*
     * USART on two pins of port A with a receive and a transmit ring buffer filled and emptied by its interrupt.
     * It carries the frames as a transport and the console text as a console output.
     */
    explicit UsartPort(USART_TypeDef *usart,
                       IRQn_Type interruptNumber,
                       uint32_t serialReceivePin,
                       uint32_t serialTransmitPin);
    ~UsartPort() final;

    // delete copy constructor and copy assignment operator
    UsartPort(const UsartPort &) = delete;
    UsartPort &operator=(const UsartPort &) = delete;

    // delete move constructor and move assignment operator
    UsartPort(UsartPort &&) = delete;
    UsartPort &operator=(UsartPort &&) = delete;

public:
    /*
     * Connect the pins and open the USART 8N1 without flow control with its receive interrupt.
     * Bytes written before the start are sent after it.
     */
    void begin(uint32_t baudRate);

    /*
     * Move a received byte into the receive ring buffer and the next byte of the transmit ring buffer out.
     * The interrupt handler of the USART calls it.
     */
    void processInterrupt();

    /*
     * Receive the waiting bytes up to the size and get the number of received bytes.
     */
    [[nodiscard]] size_t receiveData(uint8_t *data, size_t size);

public:
    /*
     * Send every byte of the data and wait while the transmit ring buffer is full.
     */
    [[nodiscard]] bool sendData(const uint8_t *data, size_t size) final;

    /*
     * Get the free space of the transmit ring buffer in characters.
     */
    [[nodiscard]] size_t getAvailableSize() const final;

    /*
     * Write characters into the transmit ring buffer.
     */
    void writeText(const char *text, size_t size) final;

public:
    /*
     * Get the number of received bytes without room in the receive ring buffer.
     */
    [[nodiscard]] uint32_t getDroppedByteCounter() const;

private:
    /*
     * Start the transmit interrupt that empties the transmit ring buffer.
     */
    void startTransmission();

private:
    USART_TypeDef *usart;                     // registers of the USART
    const IRQn_Type interruptNumber;          // interrupt of the USART
    const uint32_t serialReceivePin;          // port A pin of the receive pin
    const uint32_t serialTransmitPin;         // port A pin of the transmit pin
    RingBuffer::RingBuffer receiveBuffer;     // received bytes for the main loop
    RingBuffer::RingBuffer sendBuffer;        // bytes for the transmit interrupt
    std::atomic<uint32_t> droppedByteCounter; // received bytes without room in the ring buffer
};

} // namespace sercom::UsartPort

#endif // SERCOM_USART_PORT_H
