#include <algorithm>
#include <cstddef>
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

        // initialize route header from the host to node 3
        static constexpr uint8_t destinationAddress = 3;
        this->routeHeader.sourceAddress = Definitions::HOST_ADDRESS;
        this->routeHeader.destinationAddress = destinationAddress;
        this->routeHeader.timeToLive = Settings::DEFAULT_TIME_TO_LIVE;
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

    static constexpr size_t SourceAddressOffset = 9;       // index of the source address
    static constexpr size_t DestinationAddressOffset = 10; // index of the destination address
    static constexpr size_t TimeToLiveOffset = 11;         // index of the time to live
    static constexpr size_t RoutedPayloadOffset = 12;      // index of the first routed payload byte

protected:
    std::vector<uint8_t> payload; // short payload

protected:
    Frame::RouteHeader routeHeader; // route header from the host to node 3
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
    REQUIRE(frame.getSize() == Settings::MAXIMUM_DIRECT_FRAME_SIZE);
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

TEST_CASE_METHOD(TestFrame,
                 "A routed ping request frame and its forwarded copy match the golden bytes",
                 "[Frame][testGoldenRoutedFrame]")
{
    // create the routed ping request of the host to node 3 (CRC32 checked with Python binascii)
    const std::vector<uint8_t> goldenFrameData = {0xA5, 0xC3, 0x02, 0x11, 0x07, 0x00, 0x01,
                                                  0x00, 0x04, 0x01, 0x03, 0x08, 0xE8, 0x03,
                                                  0x00, 0x00, 0x2B, 0xAB, 0x4A, 0x47};
    const std::vector<uint8_t> pingRequestPayload = {0xE8, 0x03, 0x00, 0x00};
    const Frame::Frame frame(0x11,
                             7,
                             0x0001,
                             routeHeader,
                             pingRequestPayload.data(),
                             pingRequestPayload.size());

    // check the golden bytes
    REQUIRE(frame.getSize() == goldenFrameData.size());
    REQUIRE(getFrameData(frame) == goldenFrameData);
    REQUIRE(frame.getCRC32() == 0x474AAB2BU);

    // create the copy that node 2 forwards with the counter of its next link
    const std::vector<uint8_t> forwardedFrameData = {0xA5, 0xC3, 0x02, 0x11, 0x02, 0x00, 0x01,
                                                     0x00, 0x04, 0x01, 0x03, 0x07, 0xE8, 0x03,
                                                     0x00, 0x00, 0xEA, 0x4B, 0xB9, 0x5D};
    Frame::RouteHeader forwardedRouteHeader = frame.getRouteHeader();
    --forwardedRouteHeader.timeToLive;
    const Frame::Frame forwardedFrame(frame.getFlags(),
                                      2,
                                      frame.getMessageType(),
                                      forwardedRouteHeader,
                                      frame.getPayload(),
                                      frame.getPayloadLength());

    // check the golden bytes of the copy with its new CRC32
    REQUIRE(getFrameData(forwardedFrame) == forwardedFrameData);
    REQUIRE(forwardedFrame.getCRC32() == 0x5DB94BEAU);
}

TEST_CASE_METHOD(TestFrame,
                 "Every field of a routed frame lands at its documented offset",
                 "[Frame][testRoutedFieldOffsets]")
{
    // create a routed frame
    const Frame::Frame frame(0x09, 0xBEEF, 0x0102, routeHeader, payload.data(), payload.size());
    const std::vector<uint8_t> frameData = getFrameData(frame);

    // check every field at its offset
    CAPTURE(frameData);
    REQUIRE(frameData.size() == Definitions::ROUTED_FRAME_OVERHEAD + payload.size());
    REQUIRE(frameData.at(FirstSyncByteOffset) == 0xA5);
    REQUIRE(frameData.at(SecondSyncByteOffset) == 0xC3);
    REQUIRE(frameData.at(VersionOffset) == Definitions::ROUTED_PROTOCOL_VERSION);
    REQUIRE(frameData.at(FlagsOffset) == 0x09);
    REQUIRE(ByteCodec::loadUint16(frameData.data() + MessageCounterOffset) == 0xBEEF);
    REQUIRE(ByteCodec::loadUint16(frameData.data() + MessageTypeOffset) == 0x0102);
    REQUIRE(frameData.at(PayloadLengthOffset) == 3);
    REQUIRE(frameData.at(SourceAddressOffset) == Definitions::HOST_ADDRESS);
    REQUIRE(frameData.at(DestinationAddressOffset) == 3);
    REQUIRE(frameData.at(TimeToLiveOffset) == Settings::DEFAULT_TIME_TO_LIVE);
    REQUIRE(frameData.at(RoutedPayloadOffset) == 0x10);
    REQUIRE(frameData.at(RoutedPayloadOffset + 2) == 0x30);
}

TEST_CASE_METHOD(TestFrame,
                 "Every getter of a routed frame reads its own field",
                 "[Frame][testRoutedGetters]")
{
    // create a routed frame
    const Frame::Frame frame(0x11, 0xBEEF, 0x0102, routeHeader, payload.data(), payload.size());

    // check every getter
    REQUIRE(frame.getVersion() == Definitions::ROUTED_PROTOCOL_VERSION);
    REQUIRE(frame.getRouted());
    REQUIRE(frame.getFlag(Definitions::FrameFlagType::ROUTE_CAPABLE));
    REQUIRE(frame.getMessageCounter() == 0xBEEF);
    REQUIRE(frame.getMessageType() == 0x0102);
    REQUIRE(frame.getPayloadLength() == 3);
    REQUIRE(frame.getSourceAddress() == Definitions::HOST_ADDRESS);
    REQUIRE(frame.getDestinationAddress() == 3);
    REQUIRE(frame.getTimeToLive() == Settings::DEFAULT_TIME_TO_LIVE);
    REQUIRE(frame.getRouteHeader().sourceAddress == Definitions::HOST_ADDRESS);
    REQUIRE(frame.getRouteHeader().destinationAddress == 3);
    REQUIRE(frame.getRouteHeader().timeToLive == Settings::DEFAULT_TIME_TO_LIVE);
    REQUIRE(frame.getHeaderSize() == Definitions::ROUTED_HEADER_SIZE);
    REQUIRE(getPayload(frame) == payload);
    REQUIRE(frame.getLikelyValid());
    REQUIRE(frame.getCRCValid());
}

TEST_CASE_METHOD(TestFrame,
                 "A direct frame has neither addresses nor a time to live",
                 "[Frame][testDirectFrameRoute]")
{
    // create a direct frame
    const Frame::Frame frame(0x01, 7, 0x0001, payload.data(), payload.size());

    // check the neutral route of the direct frame
    REQUIRE_FALSE(frame.getRouted());
    REQUIRE(frame.getSourceAddress() == Definitions::UNASSIGNED_ADDRESS);
    REQUIRE(frame.getDestinationAddress() == Definitions::UNASSIGNED_ADDRESS);
    REQUIRE(frame.getTimeToLive() == 0);
    REQUIRE(frame.getRouteHeader().sourceAddress == Definitions::UNASSIGNED_ADDRESS);
    REQUIRE(frame.getRouteHeader().destinationAddress == Definitions::UNASSIGNED_ADDRESS);
    REQUIRE(frame.getRouteHeader().timeToLive == 0);
    REQUIRE(frame.getHeaderSize() == Definitions::HEADER_SIZE);
    REQUIRE(getPayload(frame) == payload);
}

TEST_CASE_METHOD(TestFrame,
                 "A damaged time to live or address fails only the CRC32 check",
                 "[Frame][testDamagedRouteHeader]")
{
    // damage each byte of the route header of a routed frame
    const std::vector<uint8_t> frameData = getFrameData(
        Frame::Frame(0x01, 1, 0x0100, routeHeader, payload.data(), payload.size()));
    static const std::vector<size_t> damagedOffsets = {SourceAddressOffset,
                                                       DestinationAddressOffset,
                                                       TimeToLiveOffset};
    std::for_each(damagedOffsets.cbegin(),
                  damagedOffsets.cend(),
                  [&frameData](const size_t damagedOffset) {
                      // flip the lowest bit of the byte
                      CAPTURE(damagedOffset);
                      std::vector<uint8_t> damagedFrameData = frameData;
                      damagedFrameData.at(damagedOffset) = static_cast<uint8_t>(
                          damagedFrameData.at(damagedOffset) ^ 0x01);
                      const Frame::Frame frame(damagedFrameData.data(), damagedFrameData.size());

                      // check that only the CRC32 fails
                      REQUIRE(frame.getLikelyValid());
                      REQUIRE_FALSE(frame.getCRCValid());
                  });
}

TEST_CASE_METHOD(TestFrame,
                 "The largest payload gives the largest routed frame of this build",
                 "[Frame][testLargestRoutedPayload]")
{
    // create a routed frame with the largest payload
    const std::vector<uint8_t> largestPayload(Settings::MaximumPayloadSize, 0x5A);
    const Frame::Frame frame(0, 0, 0, routeHeader, largestPayload.data(), largestPayload.size());

    // check the maximum frame sizes of both versions
    REQUIRE(frame.getSize() == Settings::MAXIMUM_FRAME_SIZE);
    REQUIRE(Settings::MAXIMUM_FRAME_SIZE
            == Settings::MAXIMUM_DIRECT_FRAME_SIZE + Definitions::ROUTE_HEADER_SIZE);
    REQUIRE(frame.getLikelyValid());

    // create a routed frame with a payload above the limit
    const std::vector<uint8_t> longPayload(Settings::MaximumPayloadSize + 1, 0x5A);
    const Frame::Frame longFrame(0, 0, 0, routeHeader, longPayload.data(), longPayload.size());

    // check the empty frame
    REQUIRE(longFrame.getSize() == static_cast<size_t>(0));
    REQUIRE_FALSE(longFrame.getLikelyValid());
}

TEST_CASE_METHOD(TestFrame,
                 "Received bytes give the same routed frame again",
                 "[Frame][testReceivedRoutedFrame]")
{
    // create a frame from the bytes of a sent routed frame
    const Frame::Frame sentFrame(0x01, 42, 0x0100, routeHeader, payload.data(), payload.size());
    const Frame::Frame receivedFrame(sentFrame.getData(), sentFrame.getSize());

    // check the received frame
    REQUIRE(getFrameData(receivedFrame) == getFrameData(sentFrame));
    REQUIRE(receivedFrame.getRouted());
    REQUIRE(receivedFrame.getDestinationAddress() == 3);
    REQUIRE(getPayload(receivedFrame) == payload);
    REQUIRE(receivedFrame.getCRCValid());

    // check that a routed frame one byte short is not likely valid
    const Frame::Frame shortFrame(sentFrame.getData(), sentFrame.getSize() - 1);
    REQUIRE_FALSE(shortFrame.getLikelyValid());
}
