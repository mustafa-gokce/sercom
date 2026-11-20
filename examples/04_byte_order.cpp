#include <chrono>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <string>
#include <string_view>
#include <vector>

#include "../app/include/Messages.h"
#include "../core/include/ByteCodec.h"
#include "../core/include/Definitions.h"
#include "../host/include/HostUtils.h"

/*
 * Load a value from bytes with the most significant byte first.
 */
[[nodiscard]] uint32_t loadBigEndianValue(const uint8_t *source, size_t size);

/*
 * Get a message type as hex text with its name.
 */
[[nodiscard]] std::string getMessageTypeValueText(uint16_t messageType);

/*
 * Get a timestamp as hex text with its milliseconds.
 */
[[nodiscard]] std::string getTimestampValueText(uint32_t timestamp);

/*
 * Print one result line with its name in an aligned column.
 */
void printResult(std::string_view resultName, const std::string &result);

/*
 * Main entry point of the executable.
 */
int main(int argc, char *argv[]);

int main([[maybe_unused]] int argc, [[maybe_unused]] char *argv[])
{
    using namespace sercom;
    using namespace std::chrono_literals;

    // store the message type of a ping request
    const uint16_t messageType = Messages::fromMessageType(Messages::MessageType::PING_REQUEST);
    std::vector<uint8_t> messageTypeAsBytes(Definitions::MESSAGE_TYPE_SIZE);
    ByteCodec::storeUint16(messageTypeAsBytes.data(), messageType);

    // load the message type in both byte orders
    const uint16_t loadedMessageType = ByteCodec::loadUint16(messageTypeAsBytes.data());
    const uint16_t reversedMessageType = static_cast<uint16_t>(
        loadBigEndianValue(messageTypeAsBytes.data(), messageTypeAsBytes.size()));

    // print the message type at every step
    HostUtils::printLine("message type of a ping request");
    printResult("value", getMessageTypeValueText(messageType));
    printResult("stored as bytes",
                HostUtils::toHexString(messageTypeAsBytes.data(), messageTypeAsBytes.size()));
    printResult("loaded in the same order", getMessageTypeValueText(loadedMessageType));
    printResult("loaded in the wrong order", getMessageTypeValueText(reversedMessageType));

    // store the request timestamp of a ping request
    static constexpr std::chrono::milliseconds requestTimestamp = 1 * 1000ms;
    const uint32_t timestamp = static_cast<uint32_t>(requestTimestamp.count());
    std::vector<uint8_t> timestampAsBytes(Messages::PING_REQUEST_SIZE);
    ByteCodec::storeUint32(timestampAsBytes.data(), timestamp);

    // load the request timestamp in both byte orders
    const uint32_t loadedTimestamp = ByteCodec::loadUint32(timestampAsBytes.data());
    const uint32_t reversedTimestamp = loadBigEndianValue(timestampAsBytes.data(),
                                                          timestampAsBytes.size());

    // print the request timestamp at every step
    HostUtils::printLine("request timestamp of a ping request");
    printResult("value", getTimestampValueText(timestamp));
    printResult("stored as bytes",
                HostUtils::toHexString(timestampAsBytes.data(), timestampAsBytes.size()));
    printResult("loaded in the same order", getTimestampValueText(loadedTimestamp));
    printResult("loaded in the wrong order", getTimestampValueText(reversedTimestamp));

    // values must come back in the same order
    if (loadedMessageType != messageType || loadedTimestamp != timestamp) {
        HostUtils::logWarning("Values changed although they were loaded in the same order");
        return 1;
    }

    // values must change in the wrong order
    if (reversedMessageType == messageType || reversedTimestamp == timestamp) {
        HostUtils::logWarning(
            "Values stayed the same although they were loaded in the wrong order");
        return 1;
    }

    // example finished successfully
    return 0;
}

uint32_t loadBigEndianValue(const uint8_t *source, const size_t size)
{
    // expose the value with the first byte most significant
    return std::accumulate(source,
                           source + size,
                           static_cast<uint32_t>(0),
                           [](const uint32_t value, const uint8_t byte) {
                               return (value << 8) | byte;
                           });
}

std::string getMessageTypeValueText(const uint16_t messageType)
{
    using namespace sercom;

    // expose the hex digits and the name
    static constexpr size_t digitCount = 2 * Definitions::MESSAGE_TYPE_SIZE;
    return "0x" + HostUtils::getHexText(messageType, digitCount) + " "
           + std::string(Messages::getMessageTypeText(Messages::toMessageType(messageType)));
}

std::string getTimestampValueText(const uint32_t timestamp)
{
    using namespace sercom;

    // expose the hex digits and the milliseconds
    static constexpr size_t digitCount = 2 * Messages::PING_REQUEST_SIZE;
    return "0x" + HostUtils::getHexText(timestamp, digitCount) + " = " + std::to_string(timestamp)
           + " ms";
}

void printResult(const std::string_view resultName, const std::string &result)
{
    using namespace sercom;

    // print the result after its name
    static constexpr size_t resultNameWidth = 25;
    const std::string line = "  " + HostUtils::getLeftJustifiedText(resultName, resultNameWidth)
                             + " : " + result;
    HostUtils::printLine(line);
}
