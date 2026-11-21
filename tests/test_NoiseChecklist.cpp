#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include <catch2/catch_all.hpp>

#include "../core/include/Definitions.h"
#include "../core/include/Frame.h"
#include "../core/include/FrameDecoder.h"

using namespace std::chrono_literals;
using namespace sercom;

namespace {

class MockDecoderHandler final : public FrameDecoder::FrameDecoderHandler
{
public:
    /*
     * Mock decoder handler that records every received frame.
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
     * Listen for a decode error and ignore it.
     */
    void onDecodeError([[maybe_unused]] const Definitions::DecodeErrorType decodeErrorType) final {}

    /*
     * Drop every recorded frame.
     */
    void clear()
    {
        // drop every record
        this->frames.clear();
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

private:
    std::vector<Frame::Frame> frames; // received frames
};

} // namespace

class TestNoiseChecklist
{
public:
    /*
     * Noise checklist test fixture.
     */
    TestNoiseChecklist()
    {
        // initialize frame decoder
        this->testFrameDecoder.reset(new FrameDecoder::FrameDecoder(this->testDecoderHandler, 0ms));

        // initialize the frame to damage and its two clean followers
        this->firstFrameData
            = this->getFrameData(1,
                                 std::vector<uint8_t>(
                                     {0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17}));
        this->secondFrameData = this->getFrameData(2,
                                                   std::vector<uint8_t>({0x20, 0x21, 0x22, 0x23}));
        this->thirdFrameData = this->getFrameData(3, std::vector<uint8_t>({0x30}));
    }
    ~TestNoiseChecklist()
    {
        // close frame decoder
        this->testFrameDecoder.reset(nullptr);
    }

    /*
     * Get the bytes of a frame of the checklist.
     */
    [[nodiscard]] static std::vector<uint8_t> getFrameData(const uint16_t messageCounter,
                                                           const std::vector<uint8_t> &payload)
    {
        // expose the bytes of the frame
        const Frame::Frame frame(0, messageCounter, 0x0100, payload.data(), payload.size());
        return std::vector<uint8_t>(frame.getData(), frame.getData() + frame.getSize());
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
     * Get the message counters of the received frames.
     */
    [[nodiscard]] std::vector<uint16_t> getMessageCounters() const
    {
        // convert every received frame to its message counter
        const std::vector<Frame::Frame> frames = this->testDecoderHandler.getFrames();
        std::vector<uint16_t> messageCounters(frames.size());
        std::transform(frames.cbegin(),
                       frames.cend(),
                       messageCounters.begin(),
                       [](const Frame::Frame &frame) { return frame.getMessageCounter(); });

        // expose the message counters
        return messageCounters;
    }

    /*
     * Hand a damaged first frame and the two clean frames to the frame decoder.
     */
    void receiveDamagedFrameData(const std::vector<uint8_t> &damagedFrameData)
    {
        // hand the stream to the frame decoder
        std::vector<uint8_t> stream = damagedFrameData;
        this->appendData(stream, this->secondFrameData);
        this->appendData(stream, this->thirdFrameData);
        this->testFrameDecoder->onReceivedData(stream.data(), stream.size(), 0ms);
    }

    /*
     * Check that only the two clean frames arrived and no byte is left in the frame decoder.
     */
    void checkRecovery() const
    {
        // check the message counters and the buffered bytes
        REQUIRE(this->getMessageCounters() == std::vector<uint16_t>({2, 3}));
        REQUIRE(this->testFrameDecoder->getBufferedSize() == static_cast<size_t>(0));
    }

    // delete copy and move operators
    TestNoiseChecklist(const TestNoiseChecklist &other) = delete;
    TestNoiseChecklist &operator=(const TestNoiseChecklist &other) = delete;
    TestNoiseChecklist(TestNoiseChecklist &&other) noexcept = delete;
    TestNoiseChecklist &operator=(TestNoiseChecklist &&other) noexcept = delete;

protected:
    static constexpr size_t FirstSyncByteOffset = 0;                // index of the first sync byte
    static constexpr size_t SecondSyncByteOffset = 1;               // index of the second sync byte
    static constexpr size_t VersionOffset = Definitions::SYNC_SIZE; // index of the version
    static constexpr size_t FlagsOffset = VersionOffset
                                          + Definitions::VERSION_SIZE; // index of the flags
    static constexpr size_t MessageCounterOffset
        = FlagsOffset + Definitions::FLAGS_SIZE; // index of the message counter
    static constexpr size_t MessageTypeOffset
        = MessageCounterOffset + Definitions::MESSAGE_COUNTER_SIZE; // index of the message type
    static constexpr size_t PayloadLengthOffset
        = MessageTypeOffset + Definitions::MESSAGE_TYPE_SIZE;         // index of the payload length
    static constexpr size_t PayloadOffset = Definitions::HEADER_SIZE; // index of the payload

protected:
    MockDecoderHandler testDecoderHandler;                        // events of the frame decoder
    std::unique_ptr<FrameDecoder::FrameDecoder> testFrameDecoder; // frame decoder under test
    std::vector<uint8_t> firstFrameData;                          // frame that gets damaged
    std::vector<uint8_t> secondFrameData;                         // first clean follower
    std::vector<uint8_t> thirdFrameData;                          // second clean follower
};

TEST_CASE_METHOD(TestNoiseChecklist,
                 "A corrupted payload costs only the damaged frame (checklist 1)",
                 "[NoiseChecklist][testCorruptedPayload]")
{
    // flip a bit of the payload
    static constexpr size_t damagedOffset = PayloadOffset + 3;
    std::vector<uint8_t> damagedFrameData = firstFrameData;
    REQUIRE(damagedFrameData.size() > damagedOffset);
    damagedFrameData.at(damagedOffset) = static_cast<uint8_t>(damagedFrameData.at(damagedOffset)
                                                              ^ 0x80);

    // check the recovery
    receiveDamagedFrameData(damagedFrameData);
    checkRecovery();
}

TEST_CASE_METHOD(TestNoiseChecklist,
                 "Corrupted header fields cost only the damaged frame (checklist 2)",
                 "[NoiseChecklist][testCorruptedHeader]")
{
    // flip a bit of every header field in turn
    static const std::vector<size_t> damagedOffsets = {FlagsOffset,
                                                       MessageCounterOffset,
                                                       MessageTypeOffset + 1};
    std::for_each(damagedOffsets.cbegin(), damagedOffsets.cend(), [this](const size_t damagedOffset) {
        // check the recovery from the damaged header field
        CAPTURE(damagedOffset);
        testDecoderHandler.clear();
        std::vector<uint8_t> damagedFrameData = firstFrameData;
        damagedFrameData.at(damagedOffset) = static_cast<uint8_t>(damagedFrameData.at(damagedOffset)
                                                                  ^ 0x01);
        receiveDamagedFrameData(damagedFrameData);
        checkRecovery();
    });
}

TEST_CASE_METHOD(TestNoiseChecklist,
                 "A shorter or longer length field costs only the damaged frame (checklist 3)",
                 "[NoiseChecklist][testCorruptedLength]")
{
    // set every corrupted payload length in turn
    static constexpr uint16_t firstTrafficMessageCounter = 10;
    static constexpr uint16_t lastTrafficMessageCounter = 29;
    static const std::vector<uint8_t> payloadLengths = {2, 40, 0xFF};
    std::for_each(payloadLengths.cbegin(),
                  payloadLengths.cend(),
                  [this](const uint8_t payloadLength) {
                      // create the damaged frame with clean frames and later traffic
                      CAPTURE(payloadLength);
                      testDecoderHandler.clear();
                      std::vector<uint8_t> stream = firstFrameData;
                      stream.at(PayloadLengthOffset) = payloadLength;
                      appendData(stream, secondFrameData);
                      appendData(stream, thirdFrameData);
                      std::vector<uint16_t> expectedMessageCounters = {2, 3};
                      for (uint16_t messageCounter = firstTrafficMessageCounter;
                           messageCounter <= lastTrafficMessageCounter;
                           ++messageCounter) {
                          appendData(stream,
                                     getFrameData(messageCounter, std::vector<uint8_t>({0x01})));
                          expectedMessageCounters.push_back(messageCounter);
                      }

                      // check the recovery of every clean frame
                      testFrameDecoder->onReceivedData(stream.data(), stream.size(), 0ms);
                      REQUIRE(getMessageCounters() == expectedMessageCounters);
                      REQUIRE(testFrameDecoder->getBufferedSize() == static_cast<size_t>(0));
                  });
}

TEST_CASE_METHOD(TestNoiseChecklist,
                 "A corrupted CRC32 costs only the damaged frame (checklist 4)",
                 "[NoiseChecklist][testCorruptedCRC]")
{
    // flip a bit of the CRC32
    std::vector<uint8_t> damagedFrameData = firstFrameData;
    const size_t damagedOffset = damagedFrameData.size() - 2;
    damagedFrameData.at(damagedOffset) = static_cast<uint8_t>(damagedFrameData.at(damagedOffset)
                                                              ^ 0x40);

    // check the recovery
    receiveDamagedFrameData(damagedFrameData);
    checkRecovery();
}

TEST_CASE_METHOD(TestNoiseChecklist,
                 "A dropped first or second sync byte costs only the damaged frame (checklist 5)",
                 "[NoiseChecklist][testDroppedSyncByte]")
{
    // drop every sync byte in turn
    static const std::vector<size_t> droppedOffsets = {FirstSyncByteOffset, SecondSyncByteOffset};
    std::for_each(droppedOffsets.cbegin(), droppedOffsets.cend(), [this](const size_t droppedOffset) {
        // check the recovery from the dropped sync byte
        CAPTURE(droppedOffset);
        testDecoderHandler.clear();
        std::vector<uint8_t> damagedFrameData = firstFrameData;
        damagedFrameData.erase(damagedFrameData.begin() + static_cast<ptrdiff_t>(droppedOffset));
        receiveDamagedFrameData(damagedFrameData);
        checkRecovery();
    });
}

TEST_CASE_METHOD(TestNoiseChecklist,
                 "A truncated frame followed by a valid frame costs only itself (checklist 6)",
                 "[NoiseChecklist][testTruncatedFrame]")
{
    // stop the frame early
    static constexpr size_t truncatedSize = 12;
    const std::vector<uint8_t> damagedFrameData(firstFrameData.cbegin(),
                                                firstFrameData.cbegin()
                                                    + static_cast<ptrdiff_t>(truncatedSize));

    // check the recovery
    receiveDamagedFrameData(damagedFrameData);
    checkRecovery();
}

TEST_CASE_METHOD(TestNoiseChecklist,
                 "Garbage before the sync word is skipped (checklist 7)",
                 "[NoiseChecklist][testGarbageBeforeSyncWord]")
{
    // put garbage before the intact first frame
    std::vector<uint8_t> damagedFrameData = {0xA5, 0x00, 0xC3, 0xA5, 0xA5, 0x7E};
    appendData(damagedFrameData, firstFrameData);
    receiveDamagedFrameData(damagedFrameData);

    // check that the intact first frame arrives too
    const std::vector<Frame::Frame> frames = testDecoderHandler.getFrames();
    REQUIRE(frames.size() == static_cast<size_t>(3));
    REQUIRE(frames.at(0).getMessageCounter() == 1);
}

TEST_CASE_METHOD(TestNoiseChecklist,
                 "Garbage inside a frame costs only the damaged frame (checklist 8)",
                 "[NoiseChecklist][testGarbageInsideFrame]")
{
    // insert a sync word inside the frame
    static constexpr size_t insertedOffset = 12;
    const std::vector<uint8_t> syncWord = {Definitions::SYNC_BYTE_1, Definitions::SYNC_BYTE_2};
    std::vector<uint8_t> damagedFrameData = firstFrameData;
    damagedFrameData.insert(damagedFrameData.begin() + static_cast<ptrdiff_t>(insertedOffset),
                            syncWord.cbegin(),
                            syncWord.cend());

    // check the recovery
    receiveDamagedFrameData(damagedFrameData);
    checkRecovery();
}

TEST_CASE_METHOD(TestNoiseChecklist,
                 "A duplicated byte costs only the damaged frame (checklist 9)",
                 "[NoiseChecklist][testDuplicatedByte]")
{
    // duplicate a byte of the frame
    static constexpr size_t duplicatedOffset = 7;
    std::vector<uint8_t> damagedFrameData = firstFrameData;
    const uint8_t duplicatedByte = damagedFrameData.at(duplicatedOffset);
    damagedFrameData.insert(damagedFrameData.begin() + static_cast<ptrdiff_t>(duplicatedOffset),
                            duplicatedByte);

    // check the recovery
    receiveDamagedFrameData(damagedFrameData);
    checkRecovery();
}

TEST_CASE_METHOD(TestNoiseChecklist,
                 "Several consecutive dropped bytes cost only the damaged frame (checklist 10)",
                 "[NoiseChecklist][testDroppedBytes]")
{
    // drop seven consecutive bytes of the frame
    static constexpr size_t firstDroppedOffset = 4;
    static constexpr size_t endDroppedOffset = 11;
    std::vector<uint8_t> damagedFrameData = firstFrameData;
    damagedFrameData.erase(damagedFrameData.begin() + static_cast<ptrdiff_t>(firstDroppedOffset),
                           damagedFrameData.begin() + static_cast<ptrdiff_t>(endDroppedOffset));

    // check the recovery
    receiveDamagedFrameData(damagedFrameData);
    checkRecovery();
}

TEST_CASE_METHOD(TestNoiseChecklist,
                 "One entirely corrupted frame costs only itself (checklist 11)",
                 "[NoiseChecklist][testCorruptedFrame]")
{
    // flip bits in every byte of the frame
    std::vector<uint8_t> damagedFrameData(firstFrameData.size());
    std::transform(firstFrameData.cbegin(),
                   firstFrameData.cend(),
                   damagedFrameData.begin(),
                   [](const uint8_t byte) { return static_cast<uint8_t>(byte ^ 0x5A); });

    // check the recovery
    receiveDamagedFrameData(damagedFrameData);
    checkRecovery();
}

TEST_CASE_METHOD(TestNoiseChecklist,
                 "Several consecutive corrupted frames cost only themselves (checklist 12)",
                 "[NoiseChecklist][testCorruptedFrames]")
{
    // create five frames with a corrupted payload
    static constexpr uint16_t firstMessageCounter = 100;
    static constexpr uint16_t endMessageCounter = 105;
    std::vector<uint8_t> damagedFrameData;
    for (uint16_t messageCounter = firstMessageCounter; messageCounter < endMessageCounter;
         ++messageCounter) {
        std::vector<uint8_t> frameData = getFrameData(messageCounter,
                                                      std::vector<uint8_t>({0x01, 0x02, 0x03}));
        frameData.at(PayloadOffset) = static_cast<uint8_t>(frameData.at(PayloadOffset) ^ 0xFF);
        appendData(damagedFrameData, frameData);
    }

    // check the recovery and the five CRC32 mismatches
    receiveDamagedFrameData(damagedFrameData);
    checkRecovery();
    REQUIRE(testFrameDecoder->getStatistics().crcMismatchCounter
            == static_cast<uint64_t>(endMessageCounter - firstMessageCounter));
}

TEST_CASE_METHOD(
    TestNoiseChecklist,
    "A corrupted frame immediately followed by a valid frame costs only itself (checklist 13)",
    "[NoiseChecklist][testCorruptedFrameBeforeValidFrame]")
{
    // corrupt the version of the frame
    static constexpr uint8_t corruptedVersion = 0x7F;
    std::vector<uint8_t> damagedFrameData = firstFrameData;
    REQUIRE(damagedFrameData.size() > VersionOffset);
    damagedFrameData.at(VersionOffset) = corruptedVersion;

    // check the recovery and the invalid version
    receiveDamagedFrameData(damagedFrameData);
    checkRecovery();
    REQUIRE(testFrameDecoder->getStatistics().invalidVersionCounter == static_cast<uint64_t>(1));
}
