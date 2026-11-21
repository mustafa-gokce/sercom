#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <string>
#include <string_view>

#include "../app/include/Messages.h"
#include "../core/include/CRC32.h"
#include "../core/include/Definitions.h"
#include "../core/include/Frame.h"
#include "../host/include/HostUtils.h"

/*
 * Get a CRC32 or a CRC32 register as hex text with all of its digits.
 */
[[nodiscard]] std::string getCRC32Text(uint32_t crc);

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

    // create the ping request frame of example 02
    static constexpr std::chrono::milliseconds requestTimestamp = 1 * 1000ms;
    static constexpr uint16_t messageCounter = 7;
    const Messages::PingRequest pingRequest(requestTimestamp);
    const Frame::Frame frame(Definitions::fromFrameFlagType(Definitions::FrameFlagType::ACK_REQUEST),
                             messageCounter,
                             Messages::fromMessageType(Messages::MessageType::PING_REQUEST),
                             pingRequest.getPayload(),
                             pingRequest.getPayloadLength());

    // frame must be valid
    if (!frame.getLikelyValid()) {
        HostUtils::logWarning("Can not build the ping request frame");
        return 1;
    }

    // get the bytes from the version to the last payload byte
    const uint8_t *protectedData = frame.getData() + Definitions::SYNC_SIZE;
    const size_t protectedSize = frame.getSize() - Definitions::SYNC_SIZE - Definitions::CRC_SIZE;
    const std::string protectedLine = HostUtils::toHexString(protectedData, protectedSize) + " ("
                                      + std::to_string(protectedSize) + " bytes)";
    printResult("protected bytes", protectedLine);

    // pieces must cover the protected bytes
    static constexpr std::array<size_t, 3> pieceSizes = {3, 5, 3};
    const size_t pieceSizeSum = std::accumulate(pieceSizes.cbegin(),
                                                pieceSizes.cend(),
                                                static_cast<size_t>(0));
    if (pieceSizeSum != protectedSize) {
        HostUtils::logWarning("Pieces do not cover the protected bytes");
        return 1;
    }

    // start the running register
    uint32_t crc = Definitions::CRC32_INITIAL_VALUE;
    printResult("start register", getCRC32Text(crc));

    // fold the pieces into the register as they arrive
    size_t offset = 0;
    for (size_t index = 0; index < pieceSizes.size(); ++index) {
        // fold the piece
        const size_t pieceSize = pieceSizes[index];
        crc = CRC32::updateCRC32(crc, protectedData + offset, pieceSize);

        // print the piece with the register after it
        const std::string pieceName = "piece " + std::to_string(index + 1);
        const std::string pieceLine = HostUtils::toHexString(protectedData + offset, pieceSize)
                                      + " -> register " + getCRC32Text(crc);
        printResult(pieceName, pieceLine);

        // move to the next piece
        offset += pieceSize;
    }

    // turn the register into the CRC32
    const uint32_t piecesCRC = crc ^ Definitions::CRC32_FINAL_XOR_VALUE;
    const std::string finalLine = getCRC32Text(crc) + " ^ "
                                  + getCRC32Text(Definitions::CRC32_FINAL_XOR_VALUE) + " = "
                                  + getCRC32Text(piecesCRC);
    printResult("final XOR", finalLine);

    // calculate the CRC32 over the whole buffer at once
    const uint32_t wholeCRC = CRC32::calculateCRC32(protectedData, protectedSize);
    printResult("whole buffer", getCRC32Text(wholeCRC));
    printResult("CRC32 field", getCRC32Text(frame.getCRC32()));

    // CRC32 of the pieces must match the whole buffer
    if (piecesCRC != wholeCRC) {
        HostUtils::logWarning("CRC32 of the pieces does not match the whole buffer");
        return 1;
    }

    // CRC32 of the pieces must match the CRC32 field of the frame
    if (piecesCRC != frame.getCRC32()) {
        HostUtils::logWarning("CRC32 of the pieces does not match the CRC32 field of the frame");
        return 1;
    }

    // example finished successfully
    return 0;
}

std::string getCRC32Text(const uint32_t crc)
{
    using namespace sercom;

    // expose the hex digits of the CRC32 field
    static constexpr size_t digitCount = 2 * Definitions::CRC_SIZE;
    return "0x" + HostUtils::getHexText(crc, digitCount);
}

void printResult(const std::string_view resultName, const std::string &result)
{
    using namespace sercom;

    // print the result after its name
    static constexpr size_t resultNameWidth = 15;
    const std::string line = HostUtils::getLeftJustifiedText(resultName, resultNameWidth) + " : "
                             + result;
    HostUtils::printLine(line);
}
