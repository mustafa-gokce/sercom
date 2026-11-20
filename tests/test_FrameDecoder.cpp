#include <algorithm>
#include <chrono>
#include <cstdint>
#include <limits>
#include <memory>
#include <random>
#include <set>
#include <vector>

#include <catch2/catch_all.hpp>

#include "../core/include/Definitions.h"
#include "../core/include/Frame.h"
#include "../core/include/FrameDecoder.h"
#include "../core/include/Settings.h"

using namespace std::chrono_literals;
using namespace sercom;

namespace {

class MockDecoderHandler final : public FrameDecoder::FrameDecoderHandler
{
public:
    /*
     * Mock decoder handler that records every received frame and every decode error.
     */
    MockDecoderHandler() = default;
    ~MockDecoderHandler() final {}

    // delete copy constructor and copy assignment operator
    MockDecoderHandler(const MockDecoderHandler &) = delete;
    MockDecoderHandler &operator=(const MockDecoderHandler &) = delete;

    // delete move constructor and move assignment operator
    MockDecoderHandler(MockDecoderHandler &&) = delete;
    MockDecoderHandler &operator=(MockDecoderHandler &&) = delete;

public:
    /*
     * Listen for a received frame and record it.
     */
    void onReceivedFrame(const Frame::Frame &frame) final
    {
        // record the frame
        this->frames.push_back(frame);
    }

    /*
     * Listen for a decode error and record it.
     */
    void onDecodeError(const Definitions::DecodeErrorType decodeErrorType) final
    {
        // record the decode error
        this->decodeErrorTypes.push_back(decodeErrorType);
    }

public:
    /*
     * Get the received frames in arrival order.
     */
    [[nodiscard]] std::vector<Frame::Frame> getFrames() const
    {
        // expose the received frames
        return this->frames;
    }

    /*
     * Get the decode error types in arrival order.
     */
    [[nodiscard]] std::vector<Definitions::DecodeErrorType> getDecodeErrorTypes() const
    {
        // expose the decode error types
        return this->decodeErrorTypes;
    }

private:
    std::vector<Frame::Frame> frames;                           // received frames
    std::vector<Definitions::DecodeErrorType> decodeErrorTypes; // decode errors
};

class ReentrantDecoderHandler final : public FrameDecoder::FrameDecoderHandler
{
public:
    /*
     * Decoder handler that hands data to its frame decoder again from inside the first callback.
     */
    ReentrantDecoderHandler()
        : frameDecoder(nullptr)
        , frameCount(0)
    {}
    ~ReentrantDecoderHandler() final {}

    // delete copy constructor and copy assignment operator
    ReentrantDecoderHandler(const ReentrantDecoderHandler &) = delete;
    ReentrantDecoderHandler &operator=(const ReentrantDecoderHandler &) = delete;

    // delete move constructor and move assignment operator
    ReentrantDecoderHandler(ReentrantDecoderHandler &&) = delete;
    ReentrantDecoderHandler &operator=(ReentrantDecoderHandler &&) = delete;

public:
    /*
     * Listen for a received frame and hand the next data to the frame decoder after the first.
     */
    void onReceivedFrame([[maybe_unused]] const Frame::Frame &frame) final
    {
        // keep track of frames
        ++this->frameCount;

        // hand the next data over inside the first callback
        if (this->frameCount == 1 && this->frameDecoder != nullptr) {
            this->frameDecoder->onReceivedData(this->nextData.data(), this->nextData.size(), 0ms);
        }
    }

    /*
     * Listen for a decode error and ignore it.
     */
    void onDecodeError([[maybe_unused]] const Definitions::DecodeErrorType decodeErrorType) final {}

public:
    /*
     * Set the frame decoder that receives the next data.
     */
    void setFrameDecoder(FrameDecoder::FrameDecoder *frameDecoder)
    {
        // set the frame decoder
        this->frameDecoder = frameDecoder;
    }

    /*
     * Set the data that the first callback hands to the frame decoder.
     */
    void setNextData(const std::vector<uint8_t> &nextData)
    {
        // set the next data
        this->nextData = nextData;
    }

    /*
     * Get the number of received frames.
     */
    [[nodiscard]] size_t getFrameCount() const
    {
        // expose the number of received frames
        return this->frameCount;
    }

private:
    FrameDecoder::FrameDecoder *frameDecoder; // frame decoder of the next data
    std::vector<uint8_t> nextData;            // data handed over from inside the callback
    size_t frameCount;                        // number of received frames
};

} // namespace

class TestFrameDecoder
{
public:
    /*
     * Frame decoder test fixture.
     */
    TestFrameDecoder()
    {
        // initialize frame decoders
        this->testFrameDecoder.reset(new FrameDecoder::FrameDecoder(this->testDecoderHandler, 0ms));
        this->timedFrameDecoder.reset(
            new FrameDecoder::FrameDecoder(this->timedDecoderHandler,
                                           Settings::DEFAULT_INTER_BYTE_TIMEOUT_MSEC));
        this->reentrantFrameDecoder.reset(
            new FrameDecoder::FrameDecoder(this->reentrantDecoderHandler, 0ms));

        // initialize reentrant decoder handler
        this->reentrantDecoderHandler.setFrameDecoder(this->reentrantFrameDecoder.get());
    }
    ~TestFrameDecoder()
    {
        // close reentrant decoder handler
        this->reentrantDecoderHandler.setFrameDecoder(nullptr);

        // close frame decoders
        this->reentrantFrameDecoder.reset(nullptr);
        this->timedFrameDecoder.reset(nullptr);
        this->testFrameDecoder.reset(nullptr);
    }

    /*
     * Get the bytes of a frame.
     */
    [[nodiscard]] static std::vector<uint8_t> getFrameData(const uint8_t flags,
                                                           const uint16_t messageCounter,
                                                           const uint16_t messageType,
                                                           const std::vector<uint8_t> &payload)
    {
        // expose the bytes of the frame
        const Frame::Frame frame(flags, messageCounter, messageType, payload.data(), payload.size());
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

    /*
     * Append data to a stream.
     */
    static void appendData(std::vector<uint8_t> &stream, const std::vector<uint8_t> &data)
    {
        // append the data
        stream.insert(stream.end(), data.cbegin(), data.cend());
    }

    /*
     * Hand data to the test frame decoder at once.
     */
    void receiveData(const std::vector<uint8_t> &data)
    {
        // hand the data at time zero
        this->testFrameDecoder->onReceivedData(data.data(), data.size(), 0ms);
    }

    // delete copy and move operators
    TestFrameDecoder(const TestFrameDecoder &other) = delete;
    TestFrameDecoder &operator=(const TestFrameDecoder &other) = delete;
    TestFrameDecoder(TestFrameDecoder &&other) noexcept = delete;
    TestFrameDecoder &operator=(TestFrameDecoder &&other) noexcept = delete;

protected:
    MockDecoderHandler testDecoderHandler;                             // test decoder events
    MockDecoderHandler timedDecoderHandler;                            // timed decoder events
    ReentrantDecoderHandler reentrantDecoderHandler;                   // reentrant decoder feeder
    std::unique_ptr<FrameDecoder::FrameDecoder> testFrameDecoder;      // decoder without timeout
    std::unique_ptr<FrameDecoder::FrameDecoder> timedFrameDecoder;     // decoder with timeout
    std::unique_ptr<FrameDecoder::FrameDecoder> reentrantFrameDecoder; // decoder fed by its handler
};

TEST_CASE_METHOD(TestFrameDecoder,
                 "A single frame is decoded with every field intact",
                 "[FrameDecoder][testSingleFrame]")
{
    // receive one frame
    receiveData(getFrameData(0x01, 0x1234, 0x0100, std::vector<uint8_t>({0xDE, 0xAD, 0xBE, 0xEF})));

    // check every field of the received frame
    const std::vector<Frame::Frame> frames = testDecoderHandler.getFrames();
    REQUIRE(frames.size() == static_cast<size_t>(1));
    REQUIRE(testDecoderHandler.getDecodeErrorTypes().empty());
    REQUIRE(frames.at(0).getVersion() == Definitions::PROTOCOL_VERSION);
    REQUIRE(frames.at(0).getFlags() == 0x01);
    REQUIRE(frames.at(0).getMessageCounter() == 0x1234);
    REQUIRE(frames.at(0).getMessageType() == 0x0100);
    REQUIRE(frames.at(0).getPayloadLength() == 4);
    REQUIRE(getPayload(frames.at(0)) == std::vector<uint8_t>({0xDE, 0xAD, 0xBE, 0xEF}));
    REQUIRE(testFrameDecoder->getBufferedSize() == static_cast<size_t>(0));
}

TEST_CASE_METHOD(TestFrameDecoder,
                 "A frame fed one byte at a time is reported exactly once at its last byte",
                 "[FrameDecoder][testByteByByte]")
{
    // receive a frame one byte at a time
    const std::vector<uint8_t> frameData = getFrameData(0, 1, 2, std::vector<uint8_t>({1, 2, 3}));
    std::for_each(frameData.cbegin(), frameData.cend(), [this](const uint8_t byte) {
        REQUIRE(testDecoderHandler.getFrames().empty());
        testFrameDecoder->onReceivedByte(byte, 0ms);
    });

    // check the frame after its last byte
    REQUIRE(testDecoderHandler.getFrames().size() == static_cast<size_t>(1));
}

TEST_CASE_METHOD(TestFrameDecoder,
                 "Frames split at random chunk boundaries all arrive",
                 "[FrameDecoder][testRandomChunks]")
{
    // create a stream of frames with growing payloads
    static constexpr uint16_t frameCount = 200;
    static constexpr size_t payloadLengthCycle = 40;
    std::vector<uint8_t> stream;
    for (uint16_t messageCounter = 0; messageCounter < frameCount; ++messageCounter) {
        appendData(stream,
                   getFrameData(0,
                                messageCounter,
                                0x0100,
                                std::vector<uint8_t>(messageCounter % payloadLengthCycle,
                                                     static_cast<uint8_t>(messageCounter))));
    }

    // receive the stream in chunks of a random size
    static constexpr uint32_t randomSeed = 99;
    static constexpr size_t maximumChunkSize = 64;
    std::mt19937 generator(randomSeed);
    std::uniform_int_distribution<size_t> chunkSizeDistribution(1, maximumChunkSize);
    size_t position = 0;
    while (position < stream.size()) {
        // receive the next chunk
        const size_t chunkSize = std::min(chunkSizeDistribution(generator),
                                          stream.size() - position);
        testFrameDecoder->onReceivedData(stream.data() + position, chunkSize, 0ms);
        position += chunkSize;
    }

    // check every frame in order
    const std::vector<Frame::Frame> frames = testDecoderHandler.getFrames();
    REQUIRE(frames.size() == static_cast<size_t>(frameCount));
    REQUIRE(testDecoderHandler.getDecodeErrorTypes().empty());
    for (uint16_t messageCounter = 0; messageCounter < frameCount; ++messageCounter) {
        CAPTURE(messageCounter);
        REQUIRE(frames.at(messageCounter).getMessageCounter() == messageCounter);
    }
}

TEST_CASE_METHOD(TestFrameDecoder,
                 "Back to back frames in one chunk are all reported",
                 "[FrameDecoder][testBackToBackFrames]")
{
    // receive three frames in one chunk
    std::vector<uint8_t> stream;
    appendData(stream, getFrameData(0, 1, 1, std::vector<uint8_t>()));
    appendData(stream, getFrameData(0, 2, 1, std::vector<uint8_t>({0xAA})));
    appendData(stream, getFrameData(0, 3, 1, std::vector<uint8_t>()));
    receiveData(stream);

    // check the three frames
    REQUIRE(testDecoderHandler.getFrames().size() == static_cast<size_t>(3));
}

TEST_CASE_METHOD(TestFrameDecoder,
                 "A frame with an empty payload is valid",
                 "[FrameDecoder][testEmptyPayload]")
{
    // receive a frame without payload
    receiveData(getFrameData(0, 9, 0x0003, std::vector<uint8_t>()));

    // check the empty payload
    const std::vector<Frame::Frame> frames = testDecoderHandler.getFrames();
    REQUIRE(frames.size() == static_cast<size_t>(1));
    REQUIRE(getPayload(frames.at(0)).empty());
}

TEST_CASE_METHOD(TestFrameDecoder,
                 "Garbage before a frame is skipped and counted",
                 "[FrameDecoder][testLeadingGarbage]")
{
    // receive garbage before a frame
    const std::vector<uint8_t> garbage = {0x00, 0x11, 0xFF, 0xC3, 0x42};
    std::vector<uint8_t> stream = garbage;
    appendData(stream, getFrameData(0, 1, 1, std::vector<uint8_t>({0x01})));
    receiveData(stream);

    // check the frame and the discarded bytes
    REQUIRE(testDecoderHandler.getFrames().size() == static_cast<size_t>(1));
    REQUIRE(testFrameDecoder->getStatistics().discardedByteCounter == garbage.size());
}

TEST_CASE_METHOD(TestFrameDecoder,
                 "A repeated first sync byte does not hide the real frame",
                 "[FrameDecoder][testRepeatedSyncByte]")
{
    // receive a first sync byte before a frame
    std::vector<uint8_t> stream = {0xA5};
    appendData(stream, getFrameData(0, 1, 1, std::vector<uint8_t>({0x01})));
    receiveData(stream);

    // check the frame and the discarded byte
    REQUIRE(testDecoderHandler.getFrames().size() == static_cast<size_t>(1));
    REQUIRE(testFrameDecoder->getStatistics().discardedByteCounter == static_cast<uint64_t>(1));
}

TEST_CASE_METHOD(TestFrameDecoder,
                 "Sync bytes inside the payload are just data",
                 "[FrameDecoder][testSyncBytesInPayload]")
{
    // receive a frame with sync bytes in its payload
    const std::vector<uint8_t> payload = {0xA5, 0xC3, 0x01, 0x00, 0xA5, 0xC3, 0xA5};
    receiveData(getFrameData(0, 1, 1, payload));

    // check the payload
    const std::vector<Frame::Frame> frames = testDecoderHandler.getFrames();
    REQUIRE(frames.size() == static_cast<size_t>(1));
    REQUIRE(getPayload(frames.at(0)) == payload);
}

TEST_CASE_METHOD(TestFrameDecoder,
                 "A frame embedded in the payload of a corrupted frame is recovered",
                 "[FrameDecoder][testEmbeddedFrame]")
{
    // receive a corrupted frame that carries a complete inner frame
    const std::vector<uint8_t> innerFrameData = getFrameData(0,
                                                             77,
                                                             0x0100,
                                                             std::vector<uint8_t>({0x01, 0x02}));
    std::vector<uint8_t> outerFrameData = getFrameData(0, 1, 0x0100, innerFrameData);
    outerFrameData.back() = static_cast<uint8_t>(outerFrameData.back() ^ 0xFF);
    receiveData(outerFrameData);

    // check the inner frame (a rejected frame loses one byte)
    const std::vector<Frame::Frame> frames = testDecoderHandler.getFrames();
    REQUIRE(
        testDecoderHandler.getDecodeErrorTypes()
        == std::vector<Definitions::DecodeErrorType>({Definitions::DecodeErrorType::CRC_MISMATCH}));
    REQUIRE(frames.size() == static_cast<size_t>(1));
    REQUIRE(frames.at(0).getMessageCounter() == 77);
}

TEST_CASE_METHOD(TestFrameDecoder,
                 "A CRC32 mismatch is reported and the next frame still decodes",
                 "[FrameDecoder][testCRCMismatch]")
{
    // receive a damaged frame and a clean frame
    static constexpr size_t damagedOffset = Definitions::HEADER_SIZE + 1;
    std::vector<uint8_t> stream = getFrameData(0, 1, 1, std::vector<uint8_t>({0x10, 0x20, 0x30}));
    stream.at(damagedOffset) = static_cast<uint8_t>(stream.at(damagedOffset) ^ 0x04);
    appendData(stream, getFrameData(0, 2, 1, std::vector<uint8_t>({0x40})));
    receiveData(stream);

    // check the decode error and the clean frame
    const std::vector<Frame::Frame> frames = testDecoderHandler.getFrames();
    REQUIRE(
        testDecoderHandler.getDecodeErrorTypes()
        == std::vector<Definitions::DecodeErrorType>({Definitions::DecodeErrorType::CRC_MISMATCH}));
    REQUIRE(frames.size() == static_cast<size_t>(1));
    REQUIRE(frames.at(0).getMessageCounter() == 2);
    REQUIRE(testFrameDecoder->getStatistics().crcMismatchCounter == static_cast<uint64_t>(1));
}

TEST_CASE_METHOD(TestFrameDecoder,
                 "An unsupported version is rejected before the payload arrives",
                 "[FrameDecoder][testInvalidVersion]")
{
    // receive the header of a frame with another version
    static constexpr size_t versionOffset = Definitions::SYNC_SIZE;
    std::vector<uint8_t> frameData = getFrameData(0, 1, 1, std::vector<uint8_t>({0x01, 0x02}));
    frameData.at(versionOffset) = Definitions::PROTOCOL_VERSION + 1;
    testFrameDecoder->onReceivedData(frameData.data(), Definitions::HEADER_SIZE, 0ms);

    // check the decode error
    REQUIRE(testDecoderHandler.getDecodeErrorTypes()
            == std::vector<Definitions::DecodeErrorType>(
                {Definitions::DecodeErrorType::INVALID_VERSION}));
    REQUIRE(testFrameDecoder->getStatistics().invalidVersionCounter == static_cast<uint64_t>(1));
}

TEST_CASE_METHOD(TestFrameDecoder,
                 "A payload length above the limit is rejected right after the header",
                 "[FrameDecoder][testPayloadTooLong]")
{
    // payload limit must be below the largest field value
    if constexpr (Settings::MaximumPayloadSize >= std::numeric_limits<uint8_t>::max()) {
        SKIP("This build accepts every length the one byte field can carry");
    }

    // receive a header with a payload length above the limit
    static constexpr size_t payloadLengthOffset = Definitions::HEADER_SIZE - 1;
    std::vector<uint8_t> frameData = getFrameData(0, 1, 1, std::vector<uint8_t>({0x01}));
    frameData.at(payloadLengthOffset) = static_cast<uint8_t>(Settings::MaximumPayloadSize + 1);
    testFrameDecoder->onReceivedData(frameData.data(), Definitions::HEADER_SIZE, 0ms);

    // check the decode error
    REQUIRE(testDecoderHandler.getDecodeErrorTypes()
            == std::vector<Definitions::DecodeErrorType>(
                {Definitions::DecodeErrorType::PAYLOAD_TOO_LONG}));
    REQUIRE(testFrameDecoder->getBufferedSize() == static_cast<size_t>(0));
}

TEST_CASE_METHOD(TestFrameDecoder,
                 "A corrupted length that is still in range costs only that frame",
                 "[FrameDecoder][testCorruptedLength]")
{
    // receive a lengthened frame and ten clean frames
    static constexpr size_t payloadLengthOffset = Definitions::HEADER_SIZE - 1;
    static constexpr uint8_t corruptedPayloadLength = 60;
    static constexpr uint16_t firstMessageCounter = 2;
    static constexpr uint16_t lastMessageCounter = 11;
    std::vector<uint8_t> stream = getFrameData(0,
                                               1,
                                               1,
                                               std::vector<uint8_t>({0x01, 0x02, 0x03, 0x04}));
    stream.at(payloadLengthOffset) = corruptedPayloadLength;
    for (uint16_t messageCounter = firstMessageCounter; messageCounter <= lastMessageCounter;
         ++messageCounter) {
        appendData(stream, getFrameData(0, messageCounter, 1, std::vector<uint8_t>({0x55, 0x66})));
    }
    receiveData(stream);

    // check the clean frames found by the rescan
    const std::vector<Frame::Frame> frames = testDecoderHandler.getFrames();
    REQUIRE(
        testDecoderHandler.getDecodeErrorTypes()
        == std::vector<Definitions::DecodeErrorType>({Definitions::DecodeErrorType::CRC_MISMATCH}));
    REQUIRE(frames.size() == static_cast<size_t>(lastMessageCounter - firstMessageCounter + 1));
    REQUIRE(frames.front().getMessageCounter() == firstMessageCounter);
    REQUIRE(frames.back().getMessageCounter() == lastMessageCounter);
}

TEST_CASE_METHOD(TestFrameDecoder,
                 "Unknown flag bits are passed through untouched",
                 "[FrameDecoder][testUnknownFlags]")
{
    // receive a frame with unknown flag bits
    receiveData(getFrameData(0xF0, 1, 1, std::vector<uint8_t>()));

    // check the flags
    const std::vector<Frame::Frame> frames = testDecoderHandler.getFrames();
    REQUIRE(frames.size() == static_cast<size_t>(1));
    REQUIRE(frames.at(0).getFlags() == 0xF0);
}

TEST_CASE_METHOD(TestFrameDecoder,
                 "A partial frame is dropped after the inter byte timeout",
                 "[FrameDecoder][testInterByteTimeout]")
{
    // receive half a frame
    static constexpr size_t partialFrameSize = 8;
    static constexpr std::chrono::milliseconds startTimestamp = 1000ms;
    const std::vector<uint8_t> frameData = getFrameData(0,
                                                        1,
                                                        1,
                                                        std::vector<uint8_t>({0x01, 0x02, 0x03}));
    timedFrameDecoder->onReceivedData(frameData.data(), partialFrameSize, startTimestamp);

    // check the timeout at its last millisecond
    timedFrameDecoder->checkTimeout(startTimestamp + Settings::DEFAULT_INTER_BYTE_TIMEOUT_MSEC);
    REQUIRE(timedDecoderHandler.getDecodeErrorTypes().empty());

    // check the timeout after its last millisecond
    timedFrameDecoder->checkTimeout(startTimestamp + Settings::DEFAULT_INTER_BYTE_TIMEOUT_MSEC
                                    + 1ms);
    REQUIRE(timedDecoderHandler.getDecodeErrorTypes()
            == std::vector<Definitions::DecodeErrorType>(
                {Definitions::DecodeErrorType::INTER_BYTE_TIMEOUT}));
    REQUIRE(timedFrameDecoder->getBufferedSize() == static_cast<size_t>(0));

    // receive the stale rest as garbage and a complete frame
    const std::chrono::milliseconds currentTime = startTimestamp
                                                  + 2 * Settings::DEFAULT_INTER_BYTE_TIMEOUT_MSEC;
    timedFrameDecoder->onReceivedData(frameData.data() + partialFrameSize,
                                      frameData.size() - partialFrameSize,
                                      currentTime);
    timedFrameDecoder->onReceivedData(frameData.data(), frameData.size(), currentTime);
    REQUIRE(timedDecoderHandler.getFrames().size() == static_cast<size_t>(1));
}

TEST_CASE_METHOD(TestFrameDecoder,
                 "The timeout is also checked when the next byte arrives late",
                 "[FrameDecoder][testLateByte]")
{
    // receive a partial frame and a complete frame much later
    static constexpr size_t partialFrameSize = 5;
    const std::vector<uint8_t> frameData = getFrameData(0, 1, 1, std::vector<uint8_t>({0x01}));
    timedFrameDecoder->onReceivedData(frameData.data(), partialFrameSize, 0ms);
    timedFrameDecoder->onReceivedData(frameData.data(),
                                      frameData.size(),
                                      10 * Settings::DEFAULT_INTER_BYTE_TIMEOUT_MSEC);

    // check the timeout and the complete frame
    REQUIRE(timedDecoderHandler.getDecodeErrorTypes()
            == std::vector<Definitions::DecodeErrorType>(
                {Definitions::DecodeErrorType::INTER_BYTE_TIMEOUT}));
    REQUIRE(timedDecoderHandler.getFrames().size() == static_cast<size_t>(1));
}

TEST_CASE_METHOD(TestFrameDecoder,
                 "The timeout keeps working beyond 32 bit milliseconds",
                 "[FrameDecoder][testTimeoutBeyond32Bit]")
{
    // receive a frame from before to after 2^32 milliseconds
    static constexpr size_t partialFrameSize = 5;
    static constexpr std::chrono::milliseconds wrapTimestamp
        = std::chrono::milliseconds(std::numeric_limits<uint32_t>::max()) + 1ms;
    const std::vector<uint8_t> frameData = getFrameData(0, 1, 1, std::vector<uint8_t>({0x01}));
    timedFrameDecoder->onReceivedData(frameData.data(),
                                      partialFrameSize,
                                      wrapTimestamp - Settings::DEFAULT_INTER_BYTE_TIMEOUT_MSEC / 4);
    timedFrameDecoder->onReceivedData(frameData.data() + partialFrameSize,
                                      frameData.size() - partialFrameSize,
                                      wrapTimestamp + Settings::DEFAULT_INTER_BYTE_TIMEOUT_MSEC / 4);

    // check the frame without a timeout
    REQUIRE(timedDecoderHandler.getDecodeErrorTypes().empty());
    REQUIRE(timedDecoderHandler.getFrames().size() == static_cast<size_t>(1));
}

TEST_CASE_METHOD(TestFrameDecoder,
                 "Statistics count bytes, frames and errors",
                 "[FrameDecoder][testStatistics]")
{
    // receive a clean and a damaged frame after garbage
    const std::vector<uint8_t> garbage = {0x00, 0x00};
    std::vector<uint8_t> stream = garbage;
    appendData(stream, getFrameData(0, 1, 1, std::vector<uint8_t>({0x01})));
    std::vector<uint8_t> damagedFrameData = getFrameData(0, 2, 1, std::vector<uint8_t>({0x02}));
    damagedFrameData.back() = static_cast<uint8_t>(damagedFrameData.back() ^ 0x01);
    appendData(stream, damagedFrameData);
    receiveData(stream);

    // check the statistics
    const FrameDecoder::DecoderStatistics decoderStatistics = testFrameDecoder->getStatistics();
    REQUIRE(decoderStatistics.incomingByteCounter == stream.size());
    REQUIRE(decoderStatistics.incomingFrameCounter == static_cast<uint64_t>(1));
    REQUIRE(decoderStatistics.crcMismatchCounter == static_cast<uint64_t>(1));
    REQUIRE(decoderStatistics.discardedByteCounter == garbage.size() + damagedFrameData.size());

    // check the cleared statistics after a reset
    testFrameDecoder->reset();
    REQUIRE(testFrameDecoder->getStatistics().incomingByteCounter == static_cast<uint64_t>(0));
}

TEST_CASE_METHOD(TestFrameDecoder,
                 "Random noise never produces a frame that was not sent",
                 "[FrameDecoder][testRandomNoise]")
{
    // receive random bytes of a fixed seed
    static constexpr uint32_t randomSeed = 2024;
    static constexpr size_t noiseSize = 200000;
    std::mt19937 generator(randomSeed);
    std::uniform_int_distribution<int> byteDistribution(0, std::numeric_limits<uint8_t>::max());
    std::vector<uint8_t> noise(noiseSize);
    std::generate(noise.begin(), noise.end(), [&generator, &byteDistribution]() {
        return static_cast<uint8_t>(byteDistribution(generator));
    });
    receiveData(noise);

    // check that no frame was found
    REQUIRE(testDecoderHandler.getFrames().empty());
}

TEST_CASE_METHOD(TestFrameDecoder,
                 "A damaged stream only ever yields frames that were sent intact",
                 "[FrameDecoder][testDamagedStream]")
{
    // create a clean stream and remember every sent message counter
    static constexpr uint16_t frameCount = 2000;
    static constexpr size_t payloadLength = 20;
    std::set<uint16_t> sentMessageCounters;
    std::vector<uint8_t> stream;
    for (uint16_t messageCounter = 0; messageCounter < frameCount; ++messageCounter) {
        appendData(stream,
                   getFrameData(0,
                                messageCounter,
                                0x0100,
                                std::vector<uint8_t>(payloadLength,
                                                     static_cast<uint8_t>(messageCounter))));
        sentMessageCounters.insert(messageCounter);
    }

    // damage the stream with a fixed seed
    static constexpr uint32_t randomSeed = 7;
    static constexpr double damageProbability = 0.002;
    std::mt19937 generator(randomSeed);
    std::uniform_real_distribution<double> chanceDistribution(0.0, 1.0);
    std::uniform_int_distribution<int> byteDistribution(0, std::numeric_limits<uint8_t>::max());
    std::vector<uint8_t> noisyStream;
    std::for_each(stream.cbegin(),
                  stream.cend(),
                  [&generator, &chanceDistribution, &byteDistribution, &noisyStream](
                      const uint8_t byte) {
                      // drop the byte
                      if (chanceDistribution(generator) < damageProbability) {
                          return;
                      }

                      // flip a bit of the byte
                      const bool flipped = chanceDistribution(generator) < damageProbability;
                      noisyStream.push_back(flipped ? static_cast<uint8_t>(byte ^ 0x10) : byte);

                      // insert a random byte after the byte
                      if (chanceDistribution(generator) < damageProbability) {
                          noisyStream.push_back(static_cast<uint8_t>(byteDistribution(generator)));
                      }
                  });

    // receive the damaged stream
    receiveData(noisyStream);

    // check that most frames survive (at least three quarters)
    const std::vector<Frame::Frame> frames = testDecoderHandler.getFrames();
    WARN("The damaged stream delivered " << frames.size() << " of " << frameCount
                                         << " frames, expected more than " << frameCount * 3 / 4
                                         << " frames");
    REQUIRE(frames.size() > static_cast<size_t>(frameCount) * 3 / 4);

    // check that every received frame was sent with its payload
    std::for_each(frames.cbegin(), frames.cend(), [&sentMessageCounters](const Frame::Frame &frame) {
        // check the message counter and the payload of the frame
        CAPTURE(frame.getMessageCounter());
        REQUIRE(sentMessageCounters.count(frame.getMessageCounter()) == static_cast<size_t>(1));
        REQUIRE(getPayload(frame)
                == std::vector<uint8_t>(payloadLength,
                                        static_cast<uint8_t>(frame.getMessageCounter())));
    });
}

TEST_CASE_METHOD(TestFrameDecoder,
                 "Feeding the decoder from inside its own callback is safe",
                 "[FrameDecoder][testReentrantFeed]")
{
    // receive a frame whose callback hands over another frame
    const std::vector<uint8_t> frameData = getFrameData(0, 1, 1, std::vector<uint8_t>({0x01}));
    reentrantDecoderHandler.setNextData(getFrameData(0, 2, 1, std::vector<uint8_t>({0x02})));
    reentrantFrameDecoder->onReceivedData(frameData.data(), frameData.size(), 0ms);

    // check both frames
    REQUIRE(reentrantDecoderHandler.getFrameCount() == static_cast<size_t>(2));
}
