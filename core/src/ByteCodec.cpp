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

void ByteWriter::writeInt16(const int16_t value)
{
    // write the two's complement bit pattern
    this->writeUint16(static_cast<uint16_t>(value));
}

void ByteWriter::writeInt32(const int32_t value)
{
    // write the two's complement bit pattern
    this->writeUint32(static_cast<uint32_t>(value));
}

void ByteWriter::writeFloat32(const float value)
{
    // copy the bit pattern instead of converting the value
    uint32_t valueAsUint32 = 0;
    std::memcpy(&valueAsUint32, &value, sizeof(valueAsUint32));

    // write the bit pattern
    this->writeUint32(valueAsUint32);
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

void ByteWriter::writeString(const char *text, const size_t length)
{
    // length must fit into one byte
    if (length > std::numeric_limits<uint8_t>::max()) {
        this->valid = false;
        return;
    }

    // write the length and the characters
    this->writeUint8(static_cast<uint8_t>(length));
    this->writeBytes(reinterpret_cast<const uint8_t *>(text), length);
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

ByteReader::ByteReader(const uint8_t *buffer, const size_t size)
    : buffer(buffer)
    , bufferSize(buffer == nullptr ? static_cast<size_t>(0) : size)
    , position(0)
    , valid(true)
{}

ByteReader::~ByteReader() {}

bool ByteReader::readUint8(uint8_t &value)
{
    // value must be available
    const uint8_t *source = this->consume(sizeof(value));
    if (source == nullptr) {
        return false;
    }

    // load the value
    value = source[0];

    // value read successfully
    return true;
}

bool ByteReader::readUint16(uint16_t &value)
{
    // value must be available
    const uint8_t *source = this->consume(sizeof(value));
    if (source == nullptr) {
        return false;
    }

    // load the value
    value = loadUint16(source);

    // value read successfully
    return true;
}

bool ByteReader::readUint32(uint32_t &value)
{
    // value must be available
    const uint8_t *source = this->consume(sizeof(value));
    if (source == nullptr) {
        return false;
    }

    // load the value
    value = loadUint32(source);

    // value read successfully
    return true;
}

bool ByteReader::readInt16(int16_t &value)
{
    // bit pattern must be available
    uint16_t valueAsUint16 = 0;
    if (!this->readUint16(valueAsUint16)) {
        return false;
    }

    // convert the two's complement bit pattern
    value = static_cast<int16_t>(valueAsUint16);

    // value read successfully
    return true;
}

bool ByteReader::readInt32(int32_t &value)
{
    // bit pattern must be available
    uint32_t valueAsUint32 = 0;
    if (!this->readUint32(valueAsUint32)) {
        return false;
    }

    // convert the two's complement bit pattern
    value = static_cast<int32_t>(valueAsUint32);

    // value read successfully
    return true;
}

bool ByteReader::readFloat32(float &value)
{
    // bit pattern must be available
    uint32_t valueAsUint32 = 0;
    if (!this->readUint32(valueAsUint32)) {
        return false;
    }

    // copy the bit pattern instead of converting the value
    std::memcpy(&value, &valueAsUint32, sizeof(value));

    // value read successfully
    return true;
}

bool ByteReader::readBytes(uint8_t *data, const size_t size)
{
    // check for an empty read
    if (size == 0) {
        return this->valid;
    }

    // data must exist
    if (data == nullptr) {
        this->valid = false;
        return false;
    }

    // bytes must be available
    const uint8_t *source = this->consume(size);
    if (source == nullptr) {
        return false;
    }

    // copy the bytes
    std::memcpy(data, source, size);

    // bytes read successfully
    return true;
}

bool ByteReader::readString(char *text, const size_t capacity)
{
    // text must hold at least the terminating null character
    if (text == nullptr || capacity == 0) {
        this->valid = false;
        return false;
    }

    // length must be available
    uint8_t length = 0;
    if (!this->readUint8(length)) {
        return false;
    }

    // characters and their terminator must fit into the text
    if (static_cast<size_t>(length) >= capacity) {
        this->valid = false;
        return false;
    }

    // characters must be available
    const uint8_t *source = this->consume(length);
    if (source == nullptr) {
        return false;
    }

    // copy and terminate the characters
    std::memcpy(text, source, length);
    text[length] = '\0';

    // string read successfully
    return true;
}

size_t ByteReader::getPosition() const
{
    // expose the number of read bytes
    return this->position;
}

size_t ByteReader::getRemainingSize() const
{
    // expose the number of unread bytes
    return this->bufferSize - this->position;
}

bool ByteReader::getValid() const
{
    // expose whether every read found enough bytes
    return this->valid;
}

const uint8_t *ByteReader::consume(const size_t size)
{
    // reader must be valid
    if (!this->valid) {
        return nullptr;
    }

    // bytes must be available completely
    if (size > this->bufferSize - this->position) {
        this->valid = false;
        return nullptr;
    }

    // consume the bytes
    const uint8_t *source = this->buffer + this->position;
    this->position += size;

    // expose the consumed bytes
    return source;
}

} // namespace sercom::ByteCodec
