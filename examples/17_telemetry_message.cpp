#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "../app/include/Messages.h"
#include "../core/include/Frame.h"
#include "../host/include/HostUtils.h"

/*
 * Print one field of the payload with its offset and its size.
 */
void printField(std::string_view fieldName, const uint8_t *payload, size_t offset, size_t size);

/*
 * Print one received value with its name in an aligned column.
 */
void printValue(std::string_view valueName, const std::string &value);

/*
 * Main entry point of the executable.
 */
int main(int argc, char *argv[]);

int main([[maybe_unused]] int argc, [[maybe_unused]] char *argv[])
{
    using namespace sercom;
    using namespace std::chrono_literals;

    // create the telemetry report response of the device
    static constexpr uint16_t sequence = 42;
    static constexpr std::chrono::milliseconds uptime = 3723500ms;
    static constexpr uint32_t freeHeapSize = 176 * 1024;
    static constexpr int16_t temperature = -1234;
    const Messages::TelemetryReportResponse telemetryReportResponse(sequence,
                                                                    uptime,
                                                                    freeHeapSize,
                                                                    temperature);

    // print the values the device sends
    const std::string sentLine = "sent: sequence " + std::to_string(sequence) + ", uptime "
                                 + std::to_string(uptime.count()) + " ms, free heap "
                                 + std::to_string(freeHeapSize) + " bytes, temperature "
                                 + std::to_string(temperature);
    HostUtils::printLine(sentLine);

    // print the whole payload
    const uint8_t *payload = telemetryReportResponse.getPayload();
    const size_t payloadLength = telemetryReportResponse.getPayloadLength();
    const std::string payloadLine = "payload          : "
                                    + HostUtils::toHexString(payload, payloadLength);
    HostUtils::printLine(payloadLine);

    // print every field after the fields in front of it
    size_t offset = 0;
    printField("SEQUENCE", payload, offset, sizeof(uint16_t));
    offset += sizeof(uint16_t);
    printField("UPTIME", payload, offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    printField("FREE_HEAP_SIZE", payload, offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    printField("TEMPERATURE", payload, offset, sizeof(int16_t));

    // put the response into a frame without flags
    static constexpr uint8_t flags = 0;
    static constexpr uint16_t messageCounter = 0;
    const Frame::Frame frame(flags,
                             messageCounter,
                             Messages::fromMessageType(
                                 Messages::MessageType::TELEMETRY_REPORT_RESPONSE),
                             payload,
                             payloadLength);

    // receive the frame bytes like the host
    const Frame::Frame receivedFrame(frame.getData(), frame.getSize());
    const Messages::TelemetryReportResponse receivedTelemetryReportResponse(receivedFrame);

    // received response must hold every field
    if (!receivedFrame.getCRCValid() || !receivedTelemetryReportResponse.getLikelyValid()) {
        HostUtils::logWarning("Can not read the received telemetry report response");
        return 1;
    }

    // scale the received values back to their units
    static constexpr double bytesPerKibibyte = 1024.0;
    static constexpr double stepsPerDegreeCelsius = 100.0;
    const double uptimeAsSeconds
        = std::chrono::duration<double>(receivedTelemetryReportResponse.getUptime()).count();
    const double freeHeapSizeAsKibibytes = static_cast<double>(
                                               receivedTelemetryReportResponse.getFreeHeapSize())
                                           / bytesPerKibibyte;
    const double temperatureAsDegreesCelsius = static_cast<double>(
                                                   receivedTelemetryReportResponse.getTemperature())
                                               / stepsPerDegreeCelsius;

    // print every received value in its unit
    static constexpr int secondDecimalCount = 3;
    static constexpr int kibibyteDecimalCount = 1;
    static constexpr int degreeDecimalCount = 2;
    printValue("sequence", std::to_string(receivedTelemetryReportResponse.getSequence()));
    printValue("uptime",
               std::to_string(receivedTelemetryReportResponse.getUptime().count()) + " ms = "
                   + HostUtils::getDecimalText(uptimeAsSeconds, secondDecimalCount) + " s");
    printValue("free heap size",
               std::to_string(receivedTelemetryReportResponse.getFreeHeapSize()) + " bytes = "
                   + HostUtils::getDecimalText(freeHeapSizeAsKibibytes, kibibyteDecimalCount)
                   + " KiB");
    printValue("temperature",
               std::to_string(receivedTelemetryReportResponse.getTemperature()) + " steps = "
                   + HostUtils::getDecimalText(temperatureAsDegreesCelsius, degreeDecimalCount)
                   + " degrees Celsius");

    // every received value must equal the sent value
    if (receivedTelemetryReportResponse.getSequence() != sequence
        || receivedTelemetryReportResponse.getUptime() != uptime
        || receivedTelemetryReportResponse.getFreeHeapSize() != freeHeapSize
        || receivedTelemetryReportResponse.getTemperature() != temperature) {
        HostUtils::logWarning("A received value differs from the sent value");
        return 1;
    }

    // print the result
    HostUtils::printLine("every value arrived unchanged");

    // example finished successfully
    return 0;
}

void printField(const std::string_view fieldName,
                const uint8_t *payload,
                const size_t offset,
                const size_t size)
{
    using namespace sercom;

    // get the field columns as text
    static constexpr size_t fieldNameWidth = 16;
    static constexpr size_t numberWidth = 2;
    const std::string fieldAsText = HostUtils::getLeftJustifiedText(fieldName, fieldNameWidth)
                                    + " offset "
                                    + HostUtils::getRightJustifiedText(std::to_string(offset),
                                                                       numberWidth)
                                    + " size "
                                    + HostUtils::getRightJustifiedText(std::to_string(size),
                                                                       numberWidth);

    // print the field with its bytes
    const std::string line = fieldAsText + " : " + HostUtils::toHexString(payload + offset, size);
    HostUtils::printLine(line);
}

void printValue(const std::string_view valueName, const std::string &value)
{
    using namespace sercom;

    // print the value after its name
    static constexpr size_t valueNameWidth = 16;
    const std::string line = "received "
                             + HostUtils::getLeftJustifiedText(valueName, valueNameWidth) + " : "
                             + value;
    HostUtils::printLine(line);
}
