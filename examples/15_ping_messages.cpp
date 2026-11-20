#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "../app/include/Messages.h"
#include "../core/include/Frame.h"
#include "../host/include/HostUtils.h"

/*
 * Get a time as a number of milliseconds with its unit.
 */
[[nodiscard]] std::string getMillisecondsText(std::chrono::milliseconds time);

/*
 * Get the four bytes of a time field as hex with the time that they hold.
 */
[[nodiscard]] std::string getTimeFieldText(const uint8_t *data, std::chrono::milliseconds time);

/*
 * Print the text of a field under its name.
 */
void printField(std::string_view name, const std::string &text);

/*
 * Main entry point of the executable.
 */
int main(int argc, char *argv[]);

int main([[maybe_unused]] int argc, [[maybe_unused]] char *argv[])
{
    using namespace sercom;
    using namespace std::chrono_literals;

    // create the ping request of the host at one second of host time
    static constexpr std::chrono::milliseconds requestTimestamp = 1 * 1000ms;
    static constexpr uint16_t messageCounter = 1;
    const Messages::PingRequest pingRequest(requestTimestamp);
    const Frame::Frame requestFrame(0,
                                    messageCounter,
                                    Messages::fromMessageType(Messages::MessageType::PING_REQUEST),
                                    pingRequest.getPayload(),
                                    pingRequest.getPayloadLength());

    // print the ping request
    HostUtils::printLine("host sends a ping request at " + getMillisecondsText(requestTimestamp));
    printField("payload",
               HostUtils::toHexString(pingRequest.getPayload(), pingRequest.getPayloadLength()));
    printField("request timestamp",
               getTimeFieldText(pingRequest.getPayload(), pingRequest.getRequestTimestamp()));
    printField("frame", HostUtils::toHexString(requestFrame.getData(), requestFrame.getSize()));

    // device must receive a valid ping request
    const Messages::MessageType requestType = Messages::toMessageType(requestFrame.getMessageType());
    const Messages::PingRequest receivedPingRequest(requestFrame);
    if (requestType != Messages::MessageType::PING_REQUEST
        || !receivedPingRequest.getLikelyValid()) {
        HostUtils::logWarning("The device did not receive a valid ping request");
        return 1;
    }

    // create the ping response of the device with the request timestamp and its uptime
    static constexpr std::chrono::milliseconds uptime = 5 * 1000ms;
    const Messages::PingResponse pingResponse(receivedPingRequest.getRequestTimestamp(), uptime);
    const Frame::Frame responseFrame(0,
                                     messageCounter,
                                     Messages::fromMessageType(Messages::MessageType::PING_RESPONSE),
                                     pingResponse.getPayload(),
                                     pingResponse.getPayloadLength());

    // print the ping response with the uptime behind the request timestamp
    HostUtils::printLine("device answers with its uptime of " + getMillisecondsText(uptime));
    printField("payload",
               HostUtils::toHexString(pingResponse.getPayload(), pingResponse.getPayloadLength()));
    printField("request timestamp",
               getTimeFieldText(pingResponse.getPayload(), pingResponse.getRequestTimestamp()));
    printField("uptime",
               getTimeFieldText(pingResponse.getPayload() + sizeof(uint32_t),
                                pingResponse.getUptime()));
    printField("frame", HostUtils::toHexString(responseFrame.getData(), responseFrame.getSize()));

    // host must receive a valid ping response
    const Messages::MessageType responseType = Messages::toMessageType(
        responseFrame.getMessageType());
    const Messages::PingResponse receivedPingResponse(responseFrame);
    if (responseType != Messages::MessageType::PING_RESPONSE
        || !receivedPingResponse.getLikelyValid()) {
        HostUtils::logWarning("The host did not receive a valid ping response");
        return 1;
    }

    // calculate the round trip time when the response arrives at the host
    static constexpr std::chrono::milliseconds responseTimestamp = 1012ms;
    const std::chrono::milliseconds roundTripTime = responseTimestamp
                                                    - receivedPingResponse.getRequestTimestamp();

    // print the received fields and the round trip time
    HostUtils::printLine("host receives the ping response at "
                         + getMillisecondsText(responseTimestamp));
    printField("request timestamp", getMillisecondsText(receivedPingResponse.getRequestTimestamp()));
    printField("uptime", getMillisecondsText(receivedPingResponse.getUptime()));
    printField("round trip time",
               getMillisecondsText(responseTimestamp) + " - "
                   + getMillisecondsText(receivedPingResponse.getRequestTimestamp()) + " = "
                   + getMillisecondsText(roundTripTime));

    // example finished successfully
    return 0;
}

std::string getMillisecondsText(const std::chrono::milliseconds time)
{
    // expose the milliseconds with their unit
    return std::to_string(time.count()) + " ms";
}

std::string getTimeFieldText(const uint8_t *data, const std::chrono::milliseconds time)
{
    using namespace sercom;

    // expose the field bytes with the time that they hold
    return HostUtils::toHexString(data, sizeof(uint32_t)) + " = " + getMillisecondsText(time);
}

void printField(const std::string_view name, const std::string &text)
{
    using namespace sercom;

    // print the text under the name
    static constexpr size_t nameWidth = 17;
    const std::string line = "  " + HostUtils::getLeftJustifiedText(name, nameWidth) + " : " + text;
    HostUtils::printLine(line);
}
