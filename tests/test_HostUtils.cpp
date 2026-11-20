#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <catch2/catch_all.hpp>

#include "../core/include/Definitions.h"
#include "../core/include/Frame.h"
#include "../core/include/FrameTracer.h"
#include "../host/include/HostUtils.h"

using namespace std::chrono_literals;
using namespace sercom;

class TestHostUtils
{
public:
    /*
     * Host utilities test fixture.
     */
    TestHostUtils() = default;
    ~TestHostUtils() = default;

    /*
     * Get a frame built from its fields.
     */
    [[nodiscard]] static Frame::Frame getFrame(const uint8_t flags,
                                               const uint16_t messageCounter,
                                               const uint16_t messageType,
                                               const std::vector<uint8_t> &payload)
    {
        // expose the frame
        return Frame::Frame(flags, messageCounter, messageType, payload.data(), payload.size());
    }

    // delete copy and move operators
    TestHostUtils(const TestHostUtils &other) = delete;
    TestHostUtils &operator=(const TestHostUtils &other) = delete;
    TestHostUtils(TestHostUtils &&other) noexcept = delete;
    TestHostUtils &operator=(TestHostUtils &&other) noexcept = delete;
};

TEST_CASE_METHOD(TestHostUtils,
                 "Bytes are converted to escaped text in quotes",
                 "[HostUtils][toEscapedString]")
{
    // convert every kind of byte
    const std::vector<uint8_t> data = {'a', '"', '\\', '\r', '\n', '\t', 0x00, 0xA5, '~', ' '};
    const std::string dataAsEscapedString = HostUtils::toEscapedString(data.data(), data.size());

    // check the escapes
    CAPTURE(dataAsEscapedString);
    REQUIRE(dataAsEscapedString == R"("a\"\\\r\n\t\x00\xA5~ ")");
    REQUIRE(HostUtils::toEscapedString(data.data(), 0) == R"("")");
}

TEST_CASE_METHOD(TestHostUtils, "The elapsed time never goes back", "[HostUtils][getElapsedTime]")
{
    // check two elapsed times in a row
    const std::chrono::milliseconds elapsedTime = HostUtils::getElapsedTime();
    const std::chrono::milliseconds laterElapsedTime = HostUtils::getElapsedTime();
    REQUIRE(elapsedTime >= 0ms);
    REQUIRE(laterElapsedTime >= elapsedTime);
}

TEST_CASE_METHOD(TestHostUtils,
                 "A time is shown as seconds with millisecond decimals",
                 "[HostUtils][getTimeText]")
{
    // check times from zero to one day
    static constexpr std::chrono::milliseconds longTime = 24 * 60 * 60 * 1000ms;
    REQUIRE(HostUtils::getTimeText(0ms) == "[     0.000]");
    REQUIRE(HostUtils::getTimeText(1234ms) == "[     1.234]");
    REQUIRE(HostUtils::getTimeText(longTime) == "[ 86400.000]");
}

TEST_CASE_METHOD(TestHostUtils,
                 "The set frame flags are shown by name",
                 "[HostUtils][getFrameFlagsText]")
{
    // check every flag alone and together
    const uint8_t retransmittedRequestFlags = Definitions::fromFrameFlagType(
                                                  Definitions::FrameFlagType::ACK_REQUEST)
                                              | Definitions::fromFrameFlagType(
                                                  Definitions::FrameFlagType::RETRANSMISSION);
    REQUIRE(HostUtils::getFrameFlagsText(0x00) == "NONE");
    REQUIRE(HostUtils::getFrameFlagsText(0x01) == "ACK_REQUEST");
    REQUIRE(HostUtils::getFrameFlagsText(0x02) == "ACK");
    REQUIRE(HostUtils::getFrameFlagsText(0x04) == "NACK");
    REQUIRE(HostUtils::getFrameFlagsText(0x08) == "RETRANSMISSION");
    REQUIRE(HostUtils::getFrameFlagsText(retransmittedRequestFlags) == "ACK_REQUEST|RETRANSMISSION");
    REQUIRE(HostUtils::getFrameFlagsText(0x0F) == "ACK_REQUEST|ACK|NACK|RETRANSMISSION");

    // check that bits without a flag are not shown
    REQUIRE(HostUtils::getFrameFlagsText(0xF0) == "NONE");
}

TEST_CASE_METHOD(TestHostUtils,
                 "A traced frame is shown on one line",
                 "[HostUtils][getTracedFrameLine]")
{
    // get the line of a ping request
    static constexpr std::chrono::milliseconds tracedTime = 512ms;
    const Frame::Frame frame = getFrame(0x01,
                                        7,
                                        0x0001,
                                        std::vector<uint8_t>({0xE8, 0x03, 0x00, 0x00}));
    const std::string line
        = HostUtils::getTracedFrameLine(FrameTracer::TraceDirectionType::HOST_TO_DEVICE,
                                        frame,
                                        tracedTime);

    // check every field
    CAPTURE(line);
    REQUIRE(line
            == "[     0.512] >> #7     PING_REQUEST                 ACK_REQUEST                len "
               "  4  E8 03 00 00  CRC ok");
}

TEST_CASE_METHOD(TestHostUtils,
                 "A traced frame of an unknown message type is shown by its number",
                 "[HostUtils][testTracedFrameLineUnknownType]")
{
    // get the line of an empty frame of an unknown message type
    static constexpr std::chrono::milliseconds tracedTime = 1ms;
    const Frame::Frame frame = getFrame(0x06, 0xFFFF, 0xBEEF, std::vector<uint8_t>());
    const std::string line
        = HostUtils::getTracedFrameLine(FrameTracer::TraceDirectionType::DEVICE_TO_HOST,
                                        frame,
                                        tracedTime);

    // check the number of the message type and the missing payload
    CAPTURE(line);
    REQUIRE(line
            == "[     0.001] << #65535 0xBEEF                       ACK|NACK                   len "
               "  0  CRC ok");
}

TEST_CASE_METHOD(TestHostUtils,
                 "A long payload is shortened on a traced frame line",
                 "[HostUtils][testTracedFrameLineLongPayload]")
{
    // get the line of a frame with a payload above the traced payload limit
    static constexpr std::chrono::milliseconds tracedTime = 2000ms;
    static constexpr size_t payloadLength = 20;
    std::vector<uint8_t> payload(payloadLength, 0);
    for (size_t index = 0; index < payload.size(); ++index) {
        payload[index] = static_cast<uint8_t>(index);
    }
    const Frame::Frame frame = getFrame(0x00, 3, 0x0007, payload);
    const std::string line
        = HostUtils::getTracedFrameLine(FrameTracer::TraceDirectionType::DEVICE_TO_HOST,
                                        frame,
                                        tracedTime);

    // check the shortened payload
    CAPTURE(line);
    REQUIRE(line
            == "[     2.000] << #3     ECHO_RESPONSE                NONE                       len "
               " 20  00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F ..  CRC ok");
}

TEST_CASE_METHOD(TestHostUtils,
                 "A traced frame with a wrong CRC32 is shown as such",
                 "[HostUtils][testTracedFrameLineCRCMismatch]")
{
    // get the line of a frame with a corrupted payload
    const Frame::Frame validFrame = getFrame(0x00, 1, 0x0005, std::vector<uint8_t>({0x01, 0x6F}));
    std::vector<uint8_t> data(validFrame.getData(), validFrame.getData() + validFrame.getSize());
    data[Definitions::HEADER_SIZE] ^= 0x01;
    const Frame::Frame corruptedFrame(data.data(), data.size());
    const std::string line
        = HostUtils::getTracedFrameLine(FrameTracer::TraceDirectionType::DEVICE_TO_HOST,
                                        corruptedFrame,
                                        0ms);

    // check the corrupted payload and the result of the CRC32 check
    CAPTURE(line);
    REQUIRE(line
            == "[     0.000] << #1     LOG_REPORT_RESPONSE          NONE                       len "
               "  2  00 6F  CRC bad");
}

TEST_CASE_METHOD(TestHostUtils,
                 "Unframed data is shown as escaped text on one line",
                 "[HostUtils][getUnframedDataLine]")
{
    // get the line of a line of boot text
    static constexpr std::chrono::milliseconds tracedTime = 1500ms;
    const std::vector<uint8_t> data = {'o', 'k', '\r', '\n'};
    const std::string line
        = HostUtils::getUnframedDataLine(FrameTracer::TraceDirectionType::DEVICE_TO_HOST,
                                         data.data(),
                                         data.size(),
                                         tracedTime);

    // check every part of the line
    CAPTURE(line);
    REQUIRE(line == R"([     1.500] << unframed data, 4 bytes: "ok\r\n")");
}

TEST_CASE_METHOD(TestHostUtils,
                 "Unframed bytes that are not text are shown in hex",
                 "[HostUtils][testUnframedDataLineHex]")
{
    // get the line of noise with a line feed and a printable byte
    static constexpr std::chrono::milliseconds tracedTime = 20ms;
    const std::vector<uint8_t> data = {0xA5, 0x0A, 's', 0xFE};
    const std::string line
        = HostUtils::getUnframedDataLine(FrameTracer::TraceDirectionType::HOST_TO_DEVICE,
                                         data.data(),
                                         data.size(),
                                         tracedTime);

    // check the bytes in hex
    CAPTURE(line);
    REQUIRE(line == "[     0.020] >> unframed data, 4 bytes: A5 0A 73 FE");
}

TEST_CASE_METHOD(TestHostUtils,
                 "One unframed byte is named in the singular",
                 "[HostUtils][testUnframedDataLineOneByte]")
{
    // get the line of one byte of noise
    const std::vector<uint8_t> data = {0x00};
    const std::string line
        = HostUtils::getUnframedDataLine(FrameTracer::TraceDirectionType::DEVICE_TO_HOST,
                                         data.data(),
                                         data.size(),
                                         0ms);

    // check the size of one byte
    CAPTURE(line);
    REQUIRE(line == "[     0.000] << unframed data, 1 byte: 00");
}
