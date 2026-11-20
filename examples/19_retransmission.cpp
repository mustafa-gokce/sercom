#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "../app/include/Messages.h"
#include "../core/include/Clock.h"
#include "../core/include/Definitions.h"
#include "../core/include/Frame.h"
#include "../core/include/Link.h"
#include "../core/include/Settings.h"
#include "../core/include/Transport.h"
#include "../host/include/HostUtils.h"

class MockClock final : public sercom::Clock::Clock
{
public:
    /*
     * Mock clock that only moves when the example moves it.
     */
    MockClock()
        : currentTime(0)
    {}
    ~MockClock() final {}

    // delete copy constructor and copy assignment operator
    MockClock(const MockClock &) = delete;
    MockClock &operator=(const MockClock &) = delete;

    // delete move constructor and move assignment operator
    MockClock(MockClock &&) = delete;
    MockClock &operator=(MockClock &&) = delete;

public:
    /*
     * Get the current time in milliseconds.
     */
    [[nodiscard]] std::chrono::milliseconds getTime() const final
    {
        // expose the current time
        return this->currentTime;
    }

    /*
     * Move the current time forward.
     */
    void advanceTime(const std::chrono::milliseconds duration)
    {
        // move the current time forward
        this->currentTime += duration;
    }

private:
    std::chrono::milliseconds currentTime; // current time in milliseconds
};

class MockTransport final : public sercom::Transport::Transport
{
public:
    /*
     * Mock transport that keeps the sent bytes in memory and prints every frame it carries.
     * It loses the next frame on the wire when the example asks for it.
     */
    explicit MockTransport(const std::string_view name)
        : name(name)
        , nextFrameLost(false)
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
     * Send the data by keeping it until the other side receives it, or lose it on the wire.
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
                                 + "  " + HostUtils::getFrameFlagsText(frame.getFlags())
                                 + (this->nextFrameLost ? "  (lost on the wire)" : "");
        HostUtils::printLine(line);

        // lose the frame once when the example asks for it
        if (this->nextFrameLost) {
            this->nextFrameLost = false;
            return true;
        }

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

public:
    /*
     * Set whether the next frame is lost on the wire.
     */
    void setNextFrameLost(const bool nextFrameLost)
    {
        // set whether the next frame is lost
        this->nextFrameLost = nextFrameLost;
    }

private:
    const std::string name;        // direction of the transport
    std::vector<uint8_t> sentData; // sent data that waits for the other side
    bool nextFrameLost;            // next frame is lost on the wire
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
 * Print a step of the example after the time of the clock.
 */
void printStep(const sercom::Clock::Clock &clock, const std::string &step);

/*
 * Main entry point of the executable.
 */
int main(int argc, char *argv[]);

int main([[maybe_unused]] int argc, [[maybe_unused]] char *argv[])
{
    using namespace sercom;

    // join both links in memory
    MockClock clock;
    MockTransport hostTransport("host -> device");
    MockTransport deviceTransport("device -> host");
    MockLinkHandler hostLinkHandler("host");
    MockLinkHandler deviceLinkHandler("device");
    Link::Link hostLink(hostTransport, hostLinkHandler, clock);
    Link::Link deviceLink(deviceTransport, deviceLinkHandler, clock);

    // lose the first frame of the host
    hostTransport.setNextFrameLost(true);

    // print the first step
    printStep(clock, "host sends a set LED request and requests an acknowledgement");

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

    // hand the host frames to the device
    hostTransport.deliverData(deviceLink);
    printStep(clock,
              "device frames received: "
                  + std::to_string(deviceLink.getStatistics().incomingFrameCounter)
                  + ", host pending messages: " + std::to_string(hostLink.getPendingCount()));

    // let the retransmit timeout pass
    clock.advanceTime(Settings::DEFAULT_RETRANSMIT_TIMEOUT_MSEC);
    printStep(clock, "retransmit timeout passed, host maintains its link");

    // maintain the host link
    hostLink.maintain();

    // hand the retransmission of the host to the device
    hostTransport.deliverData(deviceLink);

    // hand the acknowledgement of the device to the host
    deviceTransport.deliverData(hostLink);

    // print the statistics of the host link
    HostUtils::printLine("host link statistics:");
    const Link::LinkStatistics hostLinkStatistics = hostLink.getStatistics();
    const std::vector<std::string> statisticsLines = HostUtils::getLinkStatisticsLines(
        hostLinkStatistics);
    std::for_each(statisticsLines.cbegin(), statisticsLines.cend(), HostUtils::printLine);

    // host must have sent the message once more
    if (hostLinkStatistics.retransmissionCounter != 1) {
        HostUtils::logWarning("The host did not retransmit the set LED request once");
        return 1;
    }

    // delivery must be confirmed
    if (hostLink.getPendingCount() > 0 || hostLinkStatistics.incomingAcknowledgementCounter != 1) {
        HostUtils::logWarning("The set LED request was not acknowledged");
        return 1;
    }

    // example finished successfully
    return 0;
}

void printStep(const sercom::Clock::Clock &clock, const std::string &step)
{
    using namespace sercom;

    // print the step after the time
    static constexpr size_t timeWidth = 3;
    const std::string timeAsText = std::to_string(clock.getTime().count());
    const std::string line = "at " + HostUtils::getRightJustifiedText(timeAsText, timeWidth)
                             + " ms: " + step;
    HostUtils::printLine(line);
}
