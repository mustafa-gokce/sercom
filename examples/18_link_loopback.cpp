#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "../app/include/Messages.h"
#include "../core/include/Definitions.h"
#include "../core/include/Frame.h"
#include "../core/include/Link.h"
#include "../core/include/Transport.h"
#include "../host/include/HostUtils.h"

class MockTransport final : public sercom::Transport::Transport
{
public:
    /*
     * Mock transport that keeps the sent bytes in memory and prints every frame it carries.
     */
    explicit MockTransport(const std::string_view name)
        : name(name)
    {}
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
        using namespace sercom;

        // print the frame on the wire
        const Frame::Frame frame(data, size);
        const std::string line = "  " + this->name + "  #"
                                 + std::to_string(frame.getMessageCounter()) + "  "
                                 + std::string(Messages::getMessageTypeText(
                                     Messages::toMessageType(frame.getMessageType())))
                                 + "  " + HostUtils::getFrameFlagsText(frame.getFlags());
        HostUtils::printLine(line);

        // keep the data
        this->sentData.insert(this->sentData.end(), data, data + size);

        // data sent successfully
        return true;
    }

    /*
     * Hand the kept data to the link of the other side and forget it.
     */
    void deliverData(sercom::Link::Link &link)
    {
        // hand the kept data to the link
        link.onReceivedData(this->sentData.data(), this->sentData.size());

        // forget the delivered data
        this->sentData.clear();
    }

private:
    const std::string name;        // direction of the transport
    std::vector<uint8_t> sentData; // sent data that waits for the other side
};

class MockLinkHandler final : public sercom::Link::LinkHandler
{
public:
    /*
     * Mock link handler that prints every link event and accepts every message.
     */
    explicit MockLinkHandler(const std::string_view name)
        : name(name)
    {}
    ~MockLinkHandler() final {}

    // delete copy constructor and copy assignment operator
    MockLinkHandler(const MockLinkHandler &) = delete;
    MockLinkHandler &operator=(const MockLinkHandler &) = delete;

    // delete move constructor and move assignment operator
    MockLinkHandler(MockLinkHandler &&) = delete;
    MockLinkHandler &operator=(MockLinkHandler &&) = delete;

public:
    /*
     * Listen for a received message, print it and accept it.
     */
    [[nodiscard]] sercom::Definitions::MessageStatusType onReceivedMessage(
        const sercom::Frame::Frame &frame) final
    {
        using namespace sercom;

        // print the received message and the answer
        static constexpr Definitions::MessageStatusType messageStatusType
            = Definitions::MessageStatusType::ACCEPTED;
        const std::string line = this->name + " received message #"
                                 + std::to_string(frame.getMessageCounter()) + " and answers "
                                 + std::string(
                                     HostUtils::getMessageStatusTypeText(messageStatusType));
        HostUtils::printLine(line);

        // expose the answer
        return messageStatusType;
    }

    /*
     * Listen for a confirmed delivery and print it.
     */
    void onDeliveryConfirmed(const uint16_t messageCounter,
                             [[maybe_unused]] const uint16_t messageType) final
    {
        // print the confirmed delivery
        const std::string line = this->name + " got the acknowledgement of message #"
                                 + std::to_string(messageCounter);
        sercom::HostUtils::printLine(line);
    }

    /*
     * Listen for a failed delivery and print it.
     */
    void onDeliveryFailed(
        const uint16_t messageCounter,
        [[maybe_unused]] const uint16_t messageType,
        const sercom::Definitions::DeliveryFailureType deliveryFailureType,
        [[maybe_unused]] const sercom::Definitions::MessageStatusType remoteMessageStatusType) final
    {
        // print the failed delivery
        const std::string line = this->name + " gave up message #" + std::to_string(messageCounter)
                                 + " ("
                                 + std::string(sercom::Definitions::fromDeliveryFailureType(
                                     deliveryFailureType))
                                 + ")";
        sercom::HostUtils::printLine(line);
    }

private:
    const std::string name; // name of the side
};

/*
 * Print the number of host messages that wait for their acknowledgement.
 */
void printPendingCount(const sercom::Link::Link &hostLink);

/*
 * Main entry point of the executable.
 */
int main(int argc, char *argv[]);

int main([[maybe_unused]] int argc, [[maybe_unused]] char *argv[])
{
    using namespace sercom;

    // join both links in memory
    const HostUtils::SteadyClock clock;
    MockTransport hostTransport("host -> device");
    MockTransport deviceTransport("device -> host");
    MockLinkHandler hostLinkHandler("host");
    MockLinkHandler deviceLinkHandler("device");
    Link::Link hostLink(hostTransport, hostLinkHandler, clock);
    Link::Link deviceLink(deviceTransport, deviceLinkHandler, clock);

    // print the first step
    HostUtils::printLine("host sends a set LED request and requests an acknowledgement");

    // set LED request must be sent
    static constexpr bool acknowledged = true;
    const Messages::SetLedRequest setLedRequest(true);
    uint16_t messageCounter = 0;
    const Definitions::SendStatusType sendStatusType
        = hostLink.sendMessage(Messages::fromMessageType(Messages::MessageType::SET_LED_REQUEST),
                               setLedRequest.getPayload(),
                               setLedRequest.getPayloadLength(),
                               acknowledged,
                               messageCounter);
    if (sendStatusType != Definitions::SendStatusType::SENT) {
        HostUtils::logWarning("Can not send the set LED request");
        return 1;
    }

    // print the message that waits for its acknowledgement
    printPendingCount(hostLink);

    // hand the frame of the host to the device
    hostTransport.deliverData(deviceLink);

    // hand the acknowledgement of the device to the host
    deviceTransport.deliverData(hostLink);

    // print that no message waits any more
    printPendingCount(hostLink);

    // device must deliver the message once
    const Link::LinkStatistics deviceLinkStatistics = deviceLink.getStatistics();
    if (deviceLinkStatistics.deliveredMessageCounter != 1) {
        HostUtils::logWarning("The device did not receive the set LED request");
        return 1;
    }

    // delivery must be confirmed
    const Link::LinkStatistics hostLinkStatistics = hostLink.getStatistics();
    if (hostLink.getPendingCount() > 0 || hostLinkStatistics.incomingAcknowledgementCounter != 1) {
        HostUtils::logWarning("The set LED request was not acknowledged");
        return 1;
    }

    // example finished successfully
    return 0;
}

void printPendingCount(const sercom::Link::Link &hostLink)
{
    using namespace sercom;

    // print the number of pending host messages
    const std::string line = "host pending messages: " + std::to_string(hostLink.getPendingCount());
    HostUtils::printLine(line);
}
