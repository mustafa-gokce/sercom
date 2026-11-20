#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "../core/include/ByteCodec.h"
#include "../core/include/Clock.h"
#include "../core/include/Definitions.h"
#include "../core/include/Frame.h"
#include "../core/include/Link.h"
#include "../core/include/Transport.h"
#include "../host/include/HostUtils.h"

/*
 * Custom message type.
 */
enum class CustomMessageType : uint16_t {
    UNKNOWN = 0x0000,              // unknown custom message type
    ADD_NUMBERS_REQUEST = 0x0200,  // host asks for the sum of two numbers
    ADD_NUMBERS_RESPONSE = 0x0201, // device answers with the sum
};

/*
 * Custom message type to integer.
 */
uint16_t fromCustomMessageType(CustomMessageType customMessageType);

/*
 * Integer to custom message type.
 */
CustomMessageType toCustomMessageType(uint16_t customMessageType);

class MockTransport final : public sercom::Transport::Transport
{
public:
    /*
     * Mock transport that keeps the sent bytes in memory until the other side receives them.
     */
    MockTransport() = default;
    ~MockTransport() final {}

    // delete copy constructor and copy assignment operator
    MockTransport(const MockTransport &) = delete;
    MockTransport &operator=(const MockTransport &) = delete;

    // delete move constructor and move assignment operator
    MockTransport(MockTransport &&) = delete;
    MockTransport &operator=(MockTransport &&) = delete;

public:
    /*
     * Send the data by keeping it until the other side receives it.
     */
    [[nodiscard]] bool sendData(const uint8_t *data, const size_t size) final
    {
        // keep the data
        this->sentData.insert(this->sentData.end(), data, data + size);

        // data sent successfully
        return true;
    }

    /*
     * Receive the kept data and forget it.
     * Get false when no data waits.
     */
    [[nodiscard]] bool receiveData(std::vector<uint8_t> &data)
    {
        // data must wait
        if (this->sentData.empty()) {
            return false;
        }

        // take the kept data
        data = this->sentData;
        this->sentData.clear();

        // data received successfully
        return true;
    }

private:
    std::vector<uint8_t> sentData; // sent data that waits for the other side
};

class MockDevice final : public sercom::Link::LinkHandler
{
public:
    /*
     * Mock device that answers an add numbers request with the sum of its two numbers.
     */
    explicit MockDevice(sercom::Transport::Transport &transport, const sercom::Clock::Clock &clock)
        : link(transport, *this, clock)
    {}
    ~MockDevice() final {}

    // delete copy constructor and copy assignment operator
    MockDevice(const MockDevice &) = delete;
    MockDevice &operator=(const MockDevice &) = delete;

    // delete move constructor and move assignment operator
    MockDevice(MockDevice &&) = delete;
    MockDevice &operator=(MockDevice &&) = delete;

public:
    /*
     * Listen for received data and hand it to the link.
     */
    void onReceivedData(const uint8_t *data, const size_t size)
    {
        // hand the data to the link
        this->link.onReceivedData(data, size);
    }

    /*
     * Listen for a message of the host and answer an add numbers request with its sum.
     */
    [[nodiscard]] sercom::Definitions::MessageStatusType onReceivedMessage(
        const sercom::Frame::Frame &frame) final
    {
        using namespace sercom;

        // message must be an add numbers request
        if (toCustomMessageType(frame.getMessageType()) != CustomMessageType::ADD_NUMBERS_REQUEST) {
            return Definitions::MessageStatusType::UNSUPPORTED_MESSAGE_TYPE;
        }

        // payload must hold both numbers
        ByteCodec::ByteReader byteReader(frame.getPayload(), frame.getPayloadLength());
        int32_t firstNumber = 0;
        int32_t secondNumber = 0;
        if (!byteReader.readInt32(firstNumber) || !byteReader.readInt32(secondNumber)) {
            return Definitions::MessageStatusType::MALFORMED_PAYLOAD;
        }

        // add the numbers in 64 bits
        const int64_t sum = static_cast<int64_t>(firstNumber) + secondNumber;

        // print the request
        const std::string line = "device adds " + std::to_string(firstNumber) + " and "
                                 + std::to_string(secondNumber);
        HostUtils::printLine(line);

        // add numbers response must be sent
        static constexpr bool acknowledged = true;
        std::vector<uint8_t> payload(sizeof(int32_t));
        ByteCodec::ByteWriter byteWriter(payload.data(), payload.size());
        byteWriter.writeInt32(static_cast<int32_t>(sum));
        uint16_t messageCounter = 0;
        const Definitions::SendStatusType sendStatusType
            = this->link.sendMessage(fromCustomMessageType(CustomMessageType::ADD_NUMBERS_RESPONSE),
                                     payload.data(),
                                     byteWriter.getSize(),
                                     acknowledged,
                                     messageCounter);
        if (sendStatusType != Definitions::SendStatusType::SENT) {
            return Definitions::MessageStatusType::BUSY;
        }

        // add numbers request handled successfully
        return Definitions::MessageStatusType::ACCEPTED;
    }

    /*
     * Listen for an add numbers response that reached the host.
     */
    void onDeliveryConfirmed([[maybe_unused]] const uint16_t messageCounter,
                             [[maybe_unused]] const uint16_t messageType) final
    {}

    /*
     * Listen for an add numbers response that timed out or was rejected by the host.
     */
    void onDeliveryFailed(
        [[maybe_unused]] const uint16_t messageCounter,
        [[maybe_unused]] const uint16_t messageType,
        [[maybe_unused]] const sercom::Definitions::DeliveryFailureType deliveryFailureType,
        [[maybe_unused]] const sercom::Definitions::MessageStatusType remoteMessageStatusType) final
    {}

private:
    sercom::Link::Link link; // link to the host
};

class MockHostHandler final : public sercom::Link::LinkHandler
{
public:
    /*
     * Mock host handler that prints the sum of an add numbers response and keeps it.
     */
    MockHostHandler()
        : sum(0)
        , sumReceived(false)
    {}
    ~MockHostHandler() final {}

    // delete copy constructor and copy assignment operator
    MockHostHandler(const MockHostHandler &) = delete;
    MockHostHandler &operator=(const MockHostHandler &) = delete;

    // delete move constructor and move assignment operator
    MockHostHandler(MockHostHandler &&) = delete;
    MockHostHandler &operator=(MockHostHandler &&) = delete;

public:
    /*
     * Listen for a message of the device and keep the sum of an add numbers response.
     */
    [[nodiscard]] sercom::Definitions::MessageStatusType onReceivedMessage(
        const sercom::Frame::Frame &frame) final
    {
        using namespace sercom;

        // message must be an add numbers response
        if (toCustomMessageType(frame.getMessageType()) != CustomMessageType::ADD_NUMBERS_RESPONSE) {
            return Definitions::MessageStatusType::UNSUPPORTED_MESSAGE_TYPE;
        }

        // payload must hold the sum
        ByteCodec::ByteReader byteReader(frame.getPayload(), frame.getPayloadLength());
        int32_t receivedSum = 0;
        if (!byteReader.readInt32(receivedSum)) {
            return Definitions::MessageStatusType::MALFORMED_PAYLOAD;
        }

        // keep the sum
        this->sum = receivedSum;
        this->sumReceived = true;

        // print the sum
        const std::string line = "host received the sum " + std::to_string(this->sum);
        HostUtils::printLine(line);

        // add numbers response handled successfully
        return Definitions::MessageStatusType::ACCEPTED;
    }

    /*
     * Listen for an add numbers request that reached the device.
     */
    void onDeliveryConfirmed([[maybe_unused]] const uint16_t messageCounter,
                             [[maybe_unused]] const uint16_t messageType) final
    {}

    /*
     * Listen for an add numbers request that timed out or was rejected and print it.
     */
    void onDeliveryFailed([[maybe_unused]] const uint16_t messageCounter,
                          [[maybe_unused]] const uint16_t messageType,
                          const sercom::Definitions::DeliveryFailureType deliveryFailureType,
                          const sercom::Definitions::MessageStatusType remoteMessageStatusType) final
    {
        using namespace sercom;

        // print the failed request
        const std::string line = "host request failed: "
                                 + std::string(
                                     Definitions::fromDeliveryFailureType(deliveryFailureType))
                                 + ", "
                                 + std::string(
                                     HostUtils::getMessageStatusTypeText(remoteMessageStatusType));
        HostUtils::printLine(line);
    }

public:
    /*
     * Get the received sum.
     */
    [[nodiscard]] int32_t getSum() const
    {
        // expose the received sum
        return this->sum;
    }

    /*
     * Get whether a sum was received.
     */
    [[nodiscard]] bool getSumReceived() const
    {
        // expose whether a sum was received
        return this->sumReceived;
    }

private:
    int32_t sum;      // sum of the latest add numbers response
    bool sumReceived; // an add numbers response arrived
};

/*
 * Hand the waiting data between the host and the device until no data waits.
 */
void exchangeData(MockTransport &hostTransport,
                  MockTransport &deviceTransport,
                  sercom::Link::Link &hostLink,
                  MockDevice &mockDevice);

/*
 * Main entry point of the executable.
 */
int main(int argc, char *argv[]);

int main([[maybe_unused]] int argc, [[maybe_unused]] char *argv[])
{
    using namespace sercom;

    // print the custom message types
    const uint16_t requestMessageType = fromCustomMessageType(
        CustomMessageType::ADD_NUMBERS_REQUEST);
    const uint16_t responseMessageType = fromCustomMessageType(
        CustomMessageType::ADD_NUMBERS_RESPONSE);
    static constexpr size_t messageTypeDigitCount = 2 * Definitions::MESSAGE_TYPE_SIZE;
    const std::string messageTypeLine = "custom message types: ADD_NUMBERS_REQUEST 0x"
                                        + HostUtils::getHexText(requestMessageType,
                                                                messageTypeDigitCount)
                                        + ", ADD_NUMBERS_RESPONSE 0x"
                                        + HostUtils::getHexText(responseMessageType,
                                                                messageTypeDigitCount);
    HostUtils::printLine(messageTypeLine);

    // join the host and the device in memory
    const HostUtils::SteadyClock clock;
    MockTransport hostTransport;
    MockTransport deviceTransport;
    MockHostHandler mockHostHandler;
    Link::Link hostLink(hostTransport, mockHostHandler, clock);
    MockDevice mockDevice(deviceTransport, clock);

    // write both numbers into the payload of the request
    static constexpr int32_t firstNumber = 40;
    static constexpr int32_t secondNumber = 2;
    std::vector<uint8_t> payload(2 * sizeof(int32_t));
    ByteCodec::ByteWriter byteWriter(payload.data(), payload.size());
    byteWriter.writeInt32(firstNumber);
    byteWriter.writeInt32(secondNumber);

    // print the request payload
    const std::string payloadLine = "host sends ADD_NUMBERS_REQUEST with payload "
                                    + HostUtils::toHexString(payload.data(), byteWriter.getSize());
    HostUtils::printLine(payloadLine);

    // add numbers request must be sent
    static constexpr bool acknowledged = true;
    uint16_t messageCounter = 0;
    const Definitions::SendStatusType sendStatusType = hostLink.sendMessage(requestMessageType,
                                                                            payload.data(),
                                                                            byteWriter.getSize(),
                                                                            acknowledged,
                                                                            messageCounter);
    if (sendStatusType != Definitions::SendStatusType::SENT) {
        HostUtils::logWarning("Can not send the add numbers request");
        return 1;
    }

    // hand the request and its response over
    exchangeData(hostTransport, deviceTransport, hostLink, mockDevice);

    // host must have received the right sum
    const int64_t expectedSum = static_cast<int64_t>(firstNumber) + secondNumber;
    if (!mockHostHandler.getSumReceived() || mockHostHandler.getSum() != expectedSum) {
        HostUtils::logWarning("The host did not receive the sum of both numbers");
        return 1;
    }

    // example finished successfully
    return 0;
}

uint16_t fromCustomMessageType(const CustomMessageType customMessageType)
{
    return static_cast<uint16_t>(customMessageType);
}

CustomMessageType toCustomMessageType(const uint16_t customMessageType)
{
    switch (static_cast<CustomMessageType>(customMessageType)) {
    case CustomMessageType::ADD_NUMBERS_REQUEST: {
        return CustomMessageType::ADD_NUMBERS_REQUEST;
    }
    case CustomMessageType::ADD_NUMBERS_RESPONSE: {
        return CustomMessageType::ADD_NUMBERS_RESPONSE;
    }
    case CustomMessageType::UNKNOWN:
    default: {
        return CustomMessageType::UNKNOWN;
    }
    }
}

void exchangeData(MockTransport &hostTransport,
                  MockTransport &deviceTransport,
                  sercom::Link::Link &hostLink,
                  MockDevice &mockDevice)
{
    // exchange the data until none waits
    std::vector<uint8_t> data;
    bool dataWaiting = true;
    while (dataWaiting) {
        // hand the data of the host to the device
        const bool hostDataWaiting = hostTransport.receiveData(data);
        if (hostDataWaiting) {
            mockDevice.onReceivedData(data.data(), data.size());
        }

        // hand the data of the device to the host
        const bool deviceDataWaiting = deviceTransport.receiveData(data);
        if (deviceDataWaiting) {
            hostLink.onReceivedData(data.data(), data.size());
        }

        // check whether data waited in this round
        dataWaiting = hostDataWaiting || deviceDataWaiting;
    }
}
