#include "../include/RingBuffer.h"

#include <algorithm>

namespace sercom::RingBuffer {

RingBuffer::RingBuffer()
    : buffer()
    , writeIndex(0)
    , readIndex(0)
{}

RingBuffer::~RingBuffer() {}

bool RingBuffer::writeByte(const uint8_t byte)
{
    // ring must have room for the byte
    const size_t currentWriteIndex = this->writeIndex.load(std::memory_order_relaxed);
    const size_t nextWriteIndex = this->getNextIndex(currentWriteIndex);
    if (nextWriteIndex == this->readIndex.load(std::memory_order_acquire)) {
        return false;
    }

    // store the byte before the reader may see it
    this->buffer[currentWriteIndex] = byte;
    this->writeIndex.store(nextWriteIndex, std::memory_order_release);

    // byte written successfully
    return true;
}

bool RingBuffer::writeData(const uint8_t *data, const size_t size)
{
    // ring must have room for every byte
    if (size > this->getAvailableSize()) {
        return false;
    }

    // data must exist when it is not empty
    if (size > 0 && data == nullptr) {
        return false;
    }

    // iterate over the bytes
    std::for_each(data, data + size, [this](const uint8_t byte) {
        // NOTE: the room was checked above and only this writer takes it
        [[maybe_unused]] const bool written = this->writeByte(byte);
    });

    // data written successfully
    return true;
}

bool RingBuffer::readByte(uint8_t &byte)
{
    // byte must wait in the ring
    const size_t currentReadIndex = this->readIndex.load(std::memory_order_relaxed);
    if (currentReadIndex == this->writeIndex.load(std::memory_order_acquire)) {
        return false;
    }

    // take the byte before the writer may overwrite it
    byte = this->buffer[currentReadIndex];
    this->readIndex.store(this->getNextIndex(currentReadIndex), std::memory_order_release);

    // byte read successfully
    return true;
}

size_t RingBuffer::readData(uint8_t *data, const size_t size)
{
    // data must exist
    if (data == nullptr) {
        return 0;
    }

    // read the waiting bytes up to the size
    size_t readSize = 0;
    while (readSize < size && this->readByte(data[readSize])) {
        ++readSize;
    }

    // expose the number of read bytes
    return readSize;
}

size_t RingBuffer::getAvailableSize() const
{
    // expose the free bytes without the one that always stays free
    const size_t currentWriteIndex = this->writeIndex.load(std::memory_order_acquire);
    const size_t currentReadIndex = this->readIndex.load(std::memory_order_acquire);
    const size_t usedSize = (currentWriteIndex + this->buffer.size() - currentReadIndex)
                            % this->buffer.size();
    return this->buffer.size() - 1 - usedSize;
}

bool RingBuffer::getEmpty() const
{
    // expose whether the read index caught up with the write index
    return this->readIndex.load(std::memory_order_acquire)
           == this->writeIndex.load(std::memory_order_acquire);
}

size_t RingBuffer::getNextIndex(const size_t index) const
{
    // expose the next index of the ring
    return (index + 1) % this->buffer.size();
}

} // namespace sercom::RingBuffer
