#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "../app/include/Messages.h"
#include "../core/include/Frame.h"
#include "../host/include/HostSettings.h"
#include "../host/include/HostUtils.h"

/*
 * Print one field of the payload with its offset, its bytes and its value.
 */
void printField(std::string_view fieldName,
                const uint8_t *payload,
                size_t offset,
                size_t size,
                const std::string &value);

/*
 * Main entry point of the executable.
 */
int main(int argc, char *argv[]);

int main([[maybe_unused]] int argc, [[maybe_unused]] char *argv[])
{
    using namespace sercom;
    using namespace std::chrono_literals;

    // create the device info response of a device that runs for one and a half minutes
    static constexpr uint8_t firmwareVersionMajor = 1;
    static constexpr uint8_t firmwareVersionMinor = 2;
    static constexpr std::chrono::milliseconds uptime = 90 * 1000ms;
    static constexpr uint32_t freeHeapSize = 180 * 1024;
    static constexpr std::string_view deviceName = HostSettings::DEFAULT_DEVICE_NAME;
    const Messages::DeviceInfoResponse deviceInfoResponse(firmwareVersionMajor,
                                                          firmwareVersionMinor,
                                                          uptime,
                                                          freeHeapSize,
                                                          deviceName);

    // response must hold every field and the device name
    if (!deviceInfoResponse.getLikelyValid()) {
        HostUtils::logWarning("Can not build the device info response");
        return 1;
    }

    // print the whole payload
    const uint8_t *payload = deviceInfoResponse.getPayload();
    const std::string payloadLine = "payload of "
                                    + std::to_string(deviceInfoResponse.getPayloadLength())
                                    + " bytes: "
                                    + HostUtils::toHexString(payload,
                                                             deviceInfoResponse.getPayloadLength());
    HostUtils::printLine(payloadLine);

    // print every field after the fields in front of it
    const size_t deviceNameLength = deviceInfoResponse.getDeviceName().size();
    size_t offset = 0;
    printField("FIRMWARE_VERSION_MAJOR",
               payload,
               offset,
               sizeof(uint8_t),
               std::to_string(deviceInfoResponse.getFirmwareVersionMajor()));
    offset += sizeof(uint8_t);
    printField("FIRMWARE_VERSION_MINOR",
               payload,
               offset,
               sizeof(uint8_t),
               std::to_string(deviceInfoResponse.getFirmwareVersionMinor()));
    offset += sizeof(uint8_t);
    printField("UPTIME",
               payload,
               offset,
               sizeof(uint32_t),
               std::to_string(deviceInfoResponse.getUptime().count()) + " ms");
    offset += sizeof(uint32_t);
    printField("FREE_HEAP_SIZE",
               payload,
               offset,
               sizeof(uint32_t),
               std::to_string(deviceInfoResponse.getFreeHeapSize()) + " bytes");
    offset += sizeof(uint32_t);
    printField("DEVICE_NAME_LENGTH",
               payload,
               offset,
               sizeof(uint8_t),
               std::to_string(deviceNameLength));
    offset += sizeof(uint8_t);
    printField("DEVICE_NAME",
               payload,
               offset,
               deviceNameLength,
               "\"" + std::string(deviceInfoResponse.getDeviceName()) + "\"");

    // put the response into a frame and read it back like the host
    static constexpr uint16_t messageCounter = 1;
    const Frame::Frame frame(0,
                             messageCounter,
                             Messages::fromMessageType(Messages::MessageType::DEVICE_INFO_RESPONSE),
                             deviceInfoResponse.getPayload(),
                             deviceInfoResponse.getPayloadLength());
    const Messages::DeviceInfoResponse receivedDeviceInfoResponse(frame);

    // print the fields that the host reads back
    const std::string versionLine
        = "host reads firmware version "
          + std::to_string(receivedDeviceInfoResponse.getFirmwareVersionMajor()) + "."
          + std::to_string(receivedDeviceInfoResponse.getFirmwareVersionMinor()) + ", device name "
          + std::string(receivedDeviceInfoResponse.getDeviceName());
    HostUtils::printLine(versionLine);
    const std::string stateLine = "host reads uptime "
                                  + std::to_string(receivedDeviceInfoResponse.getUptime().count())
                                  + " ms, free heap size "
                                  + std::to_string(receivedDeviceInfoResponse.getFreeHeapSize())
                                  + " bytes";
    HostUtils::printLine(stateLine);

    // host must read back every field unchanged
    if (receivedDeviceInfoResponse.getFirmwareVersionMajor() != firmwareVersionMajor
        || receivedDeviceInfoResponse.getFirmwareVersionMinor() != firmwareVersionMinor
        || receivedDeviceInfoResponse.getUptime() != uptime
        || receivedDeviceInfoResponse.getFreeHeapSize() != freeHeapSize
        || receivedDeviceInfoResponse.getDeviceName() != deviceName) {
        HostUtils::logWarning("The host did not read back every field unchanged");
        return 1;
    }

    // example finished successfully
    return 0;
}

void printField(const std::string_view fieldName,
                const uint8_t *payload,
                const size_t offset,
                const size_t size,
                const std::string &value)
{
    using namespace sercom;

    // get the field columns as text
    static constexpr size_t fieldNameWidth = 22;
    static constexpr size_t numberWidth = 2;
    const std::string fieldAsText = HostUtils::getLeftJustifiedText(fieldName, fieldNameWidth)
                                    + " offset "
                                    + HostUtils::getRightJustifiedText(std::to_string(offset),
                                                                       numberWidth)
                                    + " size "
                                    + HostUtils::getRightJustifiedText(std::to_string(size),
                                                                       numberWidth);

    // print the field with its bytes and its value
    const std::string line = fieldAsText + " : " + HostUtils::toHexString(payload + offset, size)
                             + " = " + value;
    HostUtils::printLine(line);
}
