#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "../core/include/ByteCodec.h"
#include "../host/include/HostUtils.h"

/*
 * Print one result line with its name in an aligned column.
 */
void printResult(std::string_view resultName, const std::string &result);

/*
 * Print the result of one read with the state of the reader after it.
 */
void printReadResult(std::string_view readName,
                     bool succeeded,
                     const sercom::ByteCodec::ByteReader &byteReader);

/*
 * Main entry point of the executable.
 */
int main(int argc, char *argv[]);

int main([[maybe_unused]] int argc, [[maybe_unused]] char *argv[])
{
    using namespace sercom;

    // write the fields of a small status record
    static constexpr uint8_t ledState = 1;
    static constexpr uint16_t bootCount = 513;
    static constexpr int32_t temperature = -1234;
    static constexpr float voltage = 3.3F;
    static constexpr size_t bufferSize = 16;
    std::vector<uint8_t> buffer(bufferSize);
    ByteCodec::ByteWriter byteWriter(buffer.data(), buffer.size());
    byteWriter.writeUint8(ledState);
    byteWriter.writeUint16(bootCount);
    byteWriter.writeInt32(temperature);
    byteWriter.writeFloat32(voltage);

    // every write must fit
    if (!byteWriter.getValid()) {
        HostUtils::logWarning("Can not write the status record");
        return 1;
    }

    // print the written bytes
    HostUtils::printLine("write the status record");
    printResult("bytes", HostUtils::toHexString(buffer.data(), byteWriter.getSize()));
    const std::string sizeAsText = std::to_string(byteWriter.getSize()) + " written, "
                                   + std::to_string(byteWriter.getRemainingSize()) + " free";
    printResult("size in bytes", sizeAsText);

    // every field must be read back in the same order
    ByteCodec::ByteReader byteReader(buffer.data(), byteWriter.getSize());
    uint8_t readLedState = 0;
    uint16_t readBootCount = 0;
    int32_t readTemperature = 0;
    float readVoltage = 0.0F;
    const bool fieldsRead = byteReader.readUint8(readLedState)
                            && byteReader.readUint16(readBootCount)
                            && byteReader.readInt32(readTemperature)
                            && byteReader.readFloat32(readVoltage);
    if (!fieldsRead) {
        HostUtils::logWarning("Can not read the status record back");
        return 1;
    }

    // print the read fields
    static constexpr int voltageDecimalCount = 2;
    HostUtils::printLine("read the status record");
    printResult("LED state", std::to_string(readLedState));
    printResult("boot count", std::to_string(readBootCount));
    printResult("temperature", std::to_string(readTemperature));
    printResult("voltage",
                HostUtils::getDecimalText(static_cast<double>(readVoltage), voltageDecimalCount));

    // float read back from the same bytes must be exact
    static constexpr float voltageMargin = 0.0F;
    const bool voltageUnchanged = std::fabs(readVoltage - voltage) <= voltageMargin;

    // fields must come back unchanged
    const bool fieldsUnchanged = readLedState == ledState && readBootCount == bootCount
                                 && readTemperature == temperature && voltageUnchanged;
    if (!fieldsUnchanged) {
        HostUtils::logWarning("Fields of the status record changed on the way back");
        return 1;
    }

    // read the same fields from a record cut after five bytes
    static constexpr size_t cutSize = 5;
    ByteCodec::ByteReader cutReader(buffer.data(), cutSize);
    uint8_t cutLedState = 0;
    uint16_t cutBootCount = 0;
    int32_t cutTemperature = 0;
    uint8_t cutNextByte = 0;
    HostUtils::printLine("read a record cut after five bytes");
    const bool ledStateRead = cutReader.readUint8(cutLedState);
    printReadResult("uint8 LED state", ledStateRead, cutReader);
    const bool bootCountRead = cutReader.readUint16(cutBootCount);
    printReadResult("uint16 boot count", bootCountRead, cutReader);
    const bool temperatureRead = cutReader.readInt32(cutTemperature);
    printReadResult("int32 temperature", temperatureRead, cutReader);
    const bool nextByteRead = cutReader.readUint8(cutNextByte);
    printReadResult("uint8 next byte", nextByteRead, cutReader);

    // reader must stay invalid after the read past the end
    if (temperatureRead || nextByteRead || cutReader.getValid()) {
        HostUtils::logWarning("Reader stayed valid after a read past the end");
        return 1;
    }

    // example finished successfully
    return 0;
}

void printResult(const std::string_view resultName, const std::string &result)
{
    using namespace sercom;

    // print the result after its name
    static constexpr size_t resultNameWidth = 18;
    const std::string line = "  " + HostUtils::getLeftJustifiedText(resultName, resultNameWidth)
                             + " : " + result;
    HostUtils::printLine(line);
}

void printReadResult(const std::string_view readName,
                     const bool succeeded,
                     const sercom::ByteCodec::ByteReader &byteReader)
{
    // get the read result and the reader state as text
    const std::string readAsText = succeeded ? "ok" : "failed";
    const std::string validAsText = byteReader.getValid() ? "valid" : "invalid";
    const std::string stateAsText = readAsText + ", reader " + validAsText + ", position "
                                    + std::to_string(byteReader.getPosition()) + ", remaining "
                                    + std::to_string(byteReader.getRemainingSize());

    // print the read result
    printResult(readName, stateAsText);
}
