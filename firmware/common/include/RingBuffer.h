#ifndef SERCOM_RING_BUFFER_H
#define SERCOM_RING_BUFFER_H

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

#include "../include/CommonSettings.h"

namespace sercom::RingBuffer {

class RingBuffer final
{
public:
    /*
     * Byte ring between one writer and one reader, for example an interrupt handler and the main loop.
     * The writer moves only the write index and the reader moves only the read index.
     * One byte of the buffer stays free, so that a full ring differs from an empty ring.
     */
    RingBuffer();
    ~RingBuffer();

    // delete copy constructor and copy assignment operator
    RingBuffer(const RingBuffer &) = delete;
    RingBuffer &operator=(const RingBuffer &) = delete;

    // delete move constructor and move assignment operator
    RingBuffer(RingBuffer &&) = delete;
    RingBuffer &operator=(RingBuffer &&) = delete;

public:
    /*
     * Write one byte, get false when the ring is full.
     */
    [[nodiscard]] bool writeByte(uint8_t byte);

    /*
     * Write every byte of the data or none, get false when the ring has no room for all of them.
     */
    [[nodiscard]] bool writeData(const uint8_t *data, size_t size);

    /*
     * Read one byte, get false when the ring is empty.
     */
    [[nodiscard]] bool readByte(uint8_t &byte);

    /*
     * Read the waiting bytes up to the size into the data and get the number of read bytes.
     */
    [[nodiscard]] size_t readData(uint8_t *data, size_t size);

public:
    /*
     * Get the number of bytes that the ring takes now.
     */
    [[nodiscard]] size_t getAvailableSize() const;

    /*
     * Get whether no byte waits in the ring.
     */
    [[nodiscard]] bool getEmpty() const;

private:
    /*
     * Get the index after an index, the last index wraps to zero.
     */
    [[nodiscard]] size_t getNextIndex(size_t index) const;

private:
    std::array<uint8_t, CommonSettings::RING_BUFFER_SIZE> buffer; // bytes of the ring
    std::atomic<size_t> writeIndex; // index of the next written byte, moved by the writer
    std::atomic<size_t> readIndex;  // index of the next read byte, moved by the reader
};

} // namespace sercom::RingBuffer

#endif // SERCOM_RING_BUFFER_H
