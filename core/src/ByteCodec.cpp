#include "../include/ByteCodec.h"

#include <cstring>

namespace sercom::ByteCodec {

void storeUint16(uint8_t *destination, const uint16_t value)
{
    // store the bytes from the least significant one
    destination[0] = static_cast<uint8_t>(value);
    destination[1] = static_cast<uint8_t>(value >> 8);
}

void storeUint32(uint8_t *destination, const uint32_t value)
{
    // store the bytes from the least significant one
    destination[0] = static_cast<uint8_t>(value);
    destination[1] = static_cast<uint8_t>(value >> 8);
    destination[2] = static_cast<uint8_t>(value >> 16);
    destination[3] = static_cast<uint8_t>(value >> 24);
}

uint16_t loadUint16(const uint8_t *source)
{
    // join the bytes from the least significant one
    return static_cast<uint16_t>(source[0] | (source[1] << 8));
}

uint32_t loadUint32(const uint8_t *source)
{
    // join the bytes from the least significant one
    return static_cast<uint32_t>(source[0]) | (static_cast<uint32_t>(source[1]) << 8)
           | (static_cast<uint32_t>(source[2]) << 16) | (static_cast<uint32_t>(source[3]) << 24);
}

ByteWriter::ByteWriter(uint8_t *buffer, const size_t capacity)
    : buffer(buffer)
    , bufferCapacity(buffer == nullptr ? static_cast<size_t>(0) : capacity)
    , position(0)
    , valid(true)
{}

ByteWriter::~ByteWriter() {}

void ByteWriter::writeUint8(const uint8_t value)
{
    // value must fit into the buffer
    uint8_t *destination = this->reserve(sizeof(value));
    if (destination == nullptr) {
        return;
    }

    // store the value
    destination[0] = value;
}

void ByteWriter::writeUint16(const uint16_t value)
{
    // value must fit into the buffer
    uint8_t *destination = this->reserve(sizeof(value));
    if (destination == nullptr) {
        return;
    }

    // store the value
    storeUint16(destination, value);
}

void ByteWriter::writeUint32(const uint32_t value)
{
    // value must fit into the buffer
    uint8_t *destination = this->reserve(sizeof(value));
    if (destination == nullptr) {
        return;
    }

    // store the value
    storeUint32(destination, value);
}

void ByteWriter::writeBytes(const uint8_t *data, const size_t size)
{
    // check for an empty write
    if (size == 0) {
        return;
    }

    // data must exist
    if (data == nullptr) {
        this->valid = false;
        return;
    }

    // bytes must fit into the buffer
    uint8_t *destination = this->reserve(size);
    if (destination == nullptr) {
        return;
    }

    // copy the bytes
    std::memcpy(destination, data, size);
}

size_t ByteWriter::getSize() const
{
    // expose the number of written bytes
    return this->position;
}

size_t ByteWriter::getRemainingSize() const
{
    // expose the number of free bytes
    return this->bufferCapacity - this->position;
}

bool ByteWriter::getValid() const
{
    // expose whether every write fitted
    return this->valid;
}

uint8_t *ByteWriter::reserve(const size_t size)
{
    // writer must be valid
    if (!this->valid) {
        return nullptr;
    }

    // bytes must fit into the buffer completely
    if (size > this->bufferCapacity - this->position) {
        this->valid = false;
        return nullptr;
    }

    // reserve the bytes
    uint8_t *destination = this->buffer + this->position;
    this->position += size;

    // expose the reserved bytes
    return destination;
}

} // namespace sercom::ByteCodec
