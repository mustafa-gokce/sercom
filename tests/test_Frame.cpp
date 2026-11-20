#include <cstdint>
#include <vector>

#include <catch2/catch_all.hpp>

#include "../core/include/ByteCodec.h"
#include "../core/include/CRC32.h"
#include "../core/include/Definitions.h"
#include "../core/include/Frame.h"
#include "../core/include/Settings.h"

using namespace sercom;

class TestFrame
{
public:
    /*
     * Frame test fixture.
     */
    TestFrame()
    {
        // initialize payload
        this->payload = std::vector<uint8_t>({0x10, 0x20, 0x30});
    }
    ~TestFrame() = default;

    /*
     * Get the bytes of a frame.
     */
    [[nodiscard]] static std::vector<uint8_t> getFrameData(const Frame::Frame &frame)
    {
        // expose a copy of the frame bytes
        return std::vector<uint8_t>(frame.getData(), frame.getData() + frame.getSize());
    }

    /*
     * Get the payload bytes of a frame.
     */
    [[nodiscard]] static std::vector<uint8_t> getPayload(const Frame::Frame &frame)
    {
        // expose a copy of the payload bytes
        return std::vector<uint8_t>(frame.getPayload(),
                                    frame.getPayload() + frame.getPayloadLength());
    }

    // delete copy and move operators
    TestFrame(const TestFrame &other) = delete;
    TestFrame &operator=(const TestFrame &other) = delete;
    TestFrame(TestFrame &&other) noexcept = delete;
    TestFrame &operator=(TestFrame &&other) noexcept = delete;

protected:
    static constexpr size_t FirstSyncByteOffset = 0;  // index of the first sync byte
    static constexpr size_t SecondSyncByteOffset = 1; // index of the second sync byte
    static constexpr size_t VersionOffset = 2;        // index of the version
    static constexpr size_t FlagsOffset = 3;          // index of the flags
    static constexpr size_t MessageCounterOffset = 4; // index of the message counter
    static constexpr size_t MessageTypeOffset = 6;    // index of the message type
    static constexpr size_t PayloadLengthOffset = 8;  // index of the payload length
    static constexpr size_t PayloadOffset = 9;        // index of the first payload byte

protected:
    std::vector<uint8_t> payload; // short payload
};

TEST_CASE_METHOD(TestFrame,
                 "A ping request frame matches the golden bytes of the protocol specification",
                 "[Frame][testGoldenFrame]")
{
    // create a ping request frame (CRC32 checked with Python binascii)
    const std::vector<uint8_t> goldenFrameData = {0xA5,
                                                  0xC3,
                                                  0x01,
                                                  0x01,
                                                  0x07,
                                                  0x00,
                                                  0x01,
                                                  0x00,
                                                  0x04,
                                                  0xE8,
                                                  0x03,
                                                  0x00,
                                                  0x00,
                                                  0x21,
                                                  0x12,
                                                  0xD0,
                                                  0x63};
    const std::vector<uint8_t> pingRequestPayload = {0xE8, 0x03, 0x00, 0x00};
    const Frame::Frame frame(0x01, 7, 0x0001, pingRequestPayload.data(), pingRequestPayload.size());

    // check the golden bytes
    REQUIRE(frame.getSize() == goldenFrameData.size());
    REQUIRE(getFrameData(frame) == goldenFrameData);
}

TEST_CASE_METHOD(TestFrame,
                 "Every header field lands at its documented offset",
                 "[Frame][testFieldOffsets]")
{
    // create a frame
    const Frame::Frame frame(0x09, 0xBEEF, 0x0102, payload.data(), payload.size());
    const std::vector<uint8_t> frameData = getFrameData(frame);

    // check every field at its offset
    CAPTURE(frameData);
    REQUIRE(frameData.size() == Definitions::FRAME_OVERHEAD + payload.size());
    REQUIRE(frameData.at(FirstSyncByteOffset) == 0xA5);
    REQUIRE(frameData.at(SecondSyncByteOffset) == 0xC3);
    REQUIRE(frameData.at(VersionOffset) == Definitions::PROTOCOL_VERSION);
    REQUIRE(frameData.at(FlagsOffset) == 0x09);
    REQUIRE(ByteCodec::loadUint16(frameData.data() + MessageCounterOffset) == 0xBEEF);
    REQUIRE(ByteCodec::loadUint16(frameData.data() + MessageTypeOffset) == 0x0102);
    REQUIRE(frameData.at(PayloadLengthOffset) == 3);
    REQUIRE(frameData.at(PayloadOffset) == 0x10);
    REQUIRE(frameData.at(PayloadOffset + 2) == 0x30);
}

TEST_CASE_METHOD(TestFrame, "Every getter reads its own field", "[Frame][testGetters]")
{
    // create a frame
    const Frame::Frame frame(0x09, 0xBEEF, 0x0102, payload.data(), payload.size());

    // check every getter
    REQUIRE(frame.getVersion() == Definitions::PROTOCOL_VERSION);
    REQUIRE(frame.getFlags() == 0x09);
    REQUIRE(frame.getFlag(Definitions::FrameFlagType::ACK_REQUEST));
    REQUIRE_FALSE(frame.getFlag(Definitions::FrameFlagType::ACK));
    REQUIRE(frame.getFlag(Definitions::FrameFlagType::RETRANSMISSION));
    REQUIRE(frame.getMessageCounter() == 0xBEEF);
    REQUIRE(frame.getMessageType() == 0x0102);
    REQUIRE(frame.getPayloadLength() == 3);
    REQUIRE(getPayload(frame) == payload);
    REQUIRE(frame.getLikelyValid());
    REQUIRE(frame.getCRCValid());
}

TEST_CASE_METHOD(TestFrame,
                 "The CRC32 covers the version through the payload but not the sync word",
                 "[Frame][testCRCCoverage]")
{
    // create a frame
    const std::vector<uint8_t> longerPayload = {0x01, 0x02, 0x03, 0x04, 0x05};
    const Frame::Frame frame(0, 1, 2, longerPayload.data(), longerPayload.size());
    const size_t crcOffset = frame.getSize() - Definitions::CRC_SIZE;

    // check the CRC32 of the bytes after the sync word
    const uint32_t crc = CRC32::calculateCRC32(frame.getData() + Definitions::SYNC_SIZE,
                                               crcOffset - Definitions::SYNC_SIZE);
    REQUIRE(ByteCodec::loadUint32(frame.getData() + crcOffset) == crc);
    REQUIRE(frame.getCRC32() == crc);
}

TEST_CASE_METHOD(TestFrame, "An empty payload gives a 13 byte frame", "[Frame][testEmptyPayload]")
{
    // create a frame without payload
    const Frame::Frame frame(0, 0, 0x0003, nullptr, 0);

    // check the frame overhead
    REQUIRE(frame.getSize() == Definitions::FRAME_OVERHEAD);
    REQUIRE(frame.getPayloadLength() == 0);
    REQUIRE(frame.getCRCValid());
}

TEST_CASE_METHOD(TestFrame,
                 "The largest payload of this build is accepted",
                 "[Frame][testLargestPayload]")
{
    // create a frame with the largest payload
    const std::vector<uint8_t> largestPayload(Settings::MaximumPayloadSize, 0x5A);
    const Frame::Frame frame(0, 0, 0, largestPayload.data(), largestPayload.size());

    // check the maximum frame size
    REQUIRE(frame.getSize() == Settings::MAXIMUM_FRAME_SIZE);
    REQUIRE(frame.getLikelyValid());
}

TEST_CASE_METHOD(TestFrame,
                 "A payload above the limit gives an empty frame",
                 "[Frame][testPayloadTooLong]")
{
    // create a frame with a payload above the limit
    const std::vector<uint8_t> longPayload(Settings::MaximumPayloadSize + 1, 0x5A);
    const Frame::Frame frame(0, 0, 0, longPayload.data(), longPayload.size());

    // check the empty frame
    REQUIRE(frame.getSize() == static_cast<size_t>(0));
    REQUIRE_FALSE(frame.getLikelyValid());
}

TEST_CASE_METHOD(TestFrame,
                 "A missing payload with a non zero payload length gives an empty frame",
                 "[Frame][testMissingPayload]")
{
    // create a frame without payload but with a payload length
    static constexpr size_t payloadLength = 4;
    const Frame::Frame frame(0, 0, 0, nullptr, payloadLength);

    // check the empty frame
    REQUIRE(frame.getSize() == static_cast<size_t>(0));
    REQUIRE_FALSE(frame.getLikelyValid());
}

TEST_CASE_METHOD(TestFrame, "Received bytes give the same frame again", "[Frame][testReceivedFrame]")
{
    // create a frame from the bytes of a sent frame
    const Frame::Frame sentFrame(0x01, 42, 0x0100, payload.data(), payload.size());
    const Frame::Frame receivedFrame(sentFrame.getData(), sentFrame.getSize());

    // check the received frame
    REQUIRE(getFrameData(receivedFrame) == getFrameData(sentFrame));
    REQUIRE(receivedFrame.getMessageCounter() == 42);
    REQUIRE(receivedFrame.getCRCValid());
}

TEST_CASE_METHOD(TestFrame, "A damaged byte fails only the CRC32 check", "[Frame][testDamagedByte]")
{
    // damage the first payload byte of a frame
    std::vector<uint8_t> frameData = getFrameData(
        Frame::Frame(0, 1, 0x0100, payload.data(), payload.size()));
    frameData.at(PayloadOffset) = static_cast<uint8_t>(frameData.at(PayloadOffset) ^ 0x01);
    const Frame::Frame frame(frameData.data(), frameData.size());

    // check that only the CRC32 fails
    REQUIRE(frame.getLikelyValid());
    REQUIRE_FALSE(frame.getCRCValid());
}

TEST_CASE_METHOD(TestFrame,
                 "More received bytes than a frame holds give an empty frame",
                 "[Frame][testTooManyBytes]")
{
    // create a frame from more bytes than a frame holds
    const std::vector<uint8_t> frameData(Settings::MAXIMUM_FRAME_SIZE + 1, 0xA5);
    const Frame::Frame frame(frameData.data(), frameData.size());

    // check the empty frame
    REQUIRE(frame.getSize() == static_cast<size_t>(0));
    REQUIRE(frame.getMessageCounter() == 0);
    REQUIRE_FALSE(frame.getLikelyValid());
}

TEST_CASE_METHOD(TestFrame,
                 "Different message counters give different CRC32 for the same payload",
                 "[Frame][testMessageCounterCRC]")
{
    // create two frames that differ in their message counters only
    const std::vector<uint8_t> samePayload = {0x42};
    const Frame::Frame testFrame(0, 1, 0x0100, samePayload.data(), samePayload.size());
    const Frame::Frame anotherFrame(0, 2, 0x0100, samePayload.data(), samePayload.size());

    // check the CRC32 of both frames
    REQUIRE(testFrame.getSize() == anotherFrame.getSize());
    REQUIRE_FALSE(
        ByteCodec::loadUint32(testFrame.getData() + testFrame.getSize() - Definitions::CRC_SIZE)
        == ByteCodec::loadUint32(anotherFrame.getData() + anotherFrame.getSize()
                                 - Definitions::CRC_SIZE));
}
