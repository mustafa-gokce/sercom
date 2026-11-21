#ifndef SERCOM_BYTE_CODEC_H
#define SERCOM_BYTE_CODEC_H

#include <cstddef>
#include <cstdint>

namespace sercom::ByteCodec {

/*
 * Store a 16 bit value into two bytes with the least significant byte first.
 */
void storeUint16(uint8_t *destination, uint16_t value);

/*
 * Store a 32 bit value into four bytes with the least significant byte first.
 */
void storeUint32(uint8_t *destination, uint32_t value);

/*
 * Load a 16 bit value from two bytes with the least significant byte first.
 */
[[nodiscard]] uint16_t loadUint16(const uint8_t *source);

/*
 * Load a 32 bit value from four bytes with the least significant byte first.
 */
[[nodiscard]] uint32_t loadUint32(const uint8_t *source);

class ByteWriter final
{
public:
    /*
     * Writer of little-endian values into a buffer of the caller.
     * The first write that does not fit makes the writer invalid and every later write is ignored.
     */
    explicit ByteWriter(uint8_t *buffer, size_t capacity);
    ~ByteWriter();

    // delete copy constructor and copy assignment operator
    ByteWriter(const ByteWriter &) = delete;
    ByteWriter &operator=(const ByteWriter &) = delete;

    // delete move constructor and move assignment operator
    ByteWriter(ByteWriter &&) = delete;
    ByteWriter &operator=(ByteWriter &&) = delete;

public:
    /*
     * Write a uint8_t value.
     */
    void writeUint8(uint8_t value);

    /*
     * Write a uint16_t value.
     */
    void writeUint16(uint16_t value);

    /*
     * Write a uint32_t value.
     */
    void writeUint32(uint32_t value);

    /*
     * Write raw bytes.
     */
    void writeBytes(const uint8_t *data, size_t size);

public:
    /*
     * Get the number of written bytes.
     */
    [[nodiscard]] size_t getSize() const;

    /*
     * Get the number of bytes that still fit into the buffer.
     */
    [[nodiscard]] size_t getRemainingSize() const;

    /*
     * Get whether every write so far fitted into the buffer.
     */
    [[nodiscard]] bool getValid() const;

private:
    /*
     * Reserve bytes for a write.
     * Get null and make the writer invalid when the bytes do not fit.
     */
    [[nodiscard]] uint8_t *reserve(size_t size);

private:
    uint8_t *buffer;             // destination buffer
    const size_t bufferCapacity; // destination buffer size in bytes
    size_t position;             // index of the next byte to write
    bool valid;                  // every write so far fitted
};

class ByteReader final
{
public:
    /*
     * Reader of little-endian values from a buffer of the caller.
     * The first read that finds too few bytes makes the reader invalid and every later read fails.
     */
    explicit ByteReader(const uint8_t *buffer, size_t size);
    ~ByteReader();

    // delete copy constructor and copy assignment operator
    ByteReader(const ByteReader &) = delete;
    ByteReader &operator=(const ByteReader &) = delete;

    // delete move constructor and move assignment operator
    ByteReader(ByteReader &&) = delete;
    ByteReader &operator=(ByteReader &&) = delete;

public:
    /*
     * Read a uint8_t value.
     */
    [[nodiscard]] bool readUint8(uint8_t &value);

    /*
     * Read a uint16_t value.
     */
    [[nodiscard]] bool readUint16(uint16_t &value);

    /*
     * Read a uint32_t value.
     */
    [[nodiscard]] bool readUint32(uint32_t &value);

    /*
     * Read raw bytes.
     */
    [[nodiscard]] bool readBytes(uint8_t *data, size_t size);

public:
    /*
     * Get the number of read bytes.
     */
    [[nodiscard]] size_t getPosition() const;

    /*
     * Get the number of bytes that are not read yet.
     */
    [[nodiscard]] size_t getRemainingSize() const;

    /*
     * Get whether every read so far found enough bytes.
     */
    [[nodiscard]] bool getValid() const;

private:
    /*
     * Consume bytes for a read.
     * Get null and make the reader invalid when the bytes are missing.
     */
    [[nodiscard]] const uint8_t *consume(size_t size);

private:
    const uint8_t *buffer;   // source buffer
    const size_t bufferSize; // source buffer size in bytes
    size_t position;         // index of the next byte to read
    bool valid;              // every read so far found enough bytes
};

} // namespace sercom::ByteCodec

#endif // SERCOM_BYTE_CODEC_H
