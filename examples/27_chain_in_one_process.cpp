#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "../app/include/DeviceApplication.h"
#include "../app/include/DeviceClient.h"
#include "../app/include/Messages.h"
#include "../core/include/Clock.h"
#include "../core/include/Definitions.h"
#include "../core/include/Router.h"
#include "../core/include/Settings.h"
#include "../core/include/Transport.h"
#include "../host/include/HostUtils.h"
#include "../host/include/SimulatedPlatform.h"

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

class MockClientHandler final : public sercom::DeviceClient::DeviceClientHandler
{
public:
    /*
     * Mock client handler that prints the ping and device info responses and every failed request.
     */
    MockClientHandler()
        : pingResponseCount(0)
        , routedDeviceInfoCount(0)
        , unreachableCount(0)
        , expiredCount(0)
    {}
    ~MockClientHandler() final {}

    // delete copy constructor and copy assignment operator
    MockClientHandler(const MockClientHandler &) = delete;
    MockClientHandler &operator=(const MockClientHandler &) = delete;

    // delete move constructor and move assignment operator
    MockClientHandler(MockClientHandler &&) = delete;
    MockClientHandler &operator=(MockClientHandler &&) = delete;

public:
    /*
     * Listen for a ping response and print the node that sent it with the round trip time.
     */
    void onReceivedPingResponse(const sercom::Messages::PingResponse &pingResponse,
                                const std::chrono::milliseconds roundTripTime) final
    {
        // keep track of ping responses
        ++this->pingResponseCount;

        // print the ping response
        const std::string line = "  node " + std::to_string(pingResponse.getSourceAddress())
                                 + " answered after " + std::to_string(roundTripTime.count())
                                 + " ms";
        sercom::HostUtils::printLine(line);
    }

    /*
     * Listen for a device info response and print it with its protocol version and its address.
     */
    void onReceivedDeviceInfoResponse(
        const sercom::Messages::DeviceInfoResponse &deviceInfoResponse) final
    {
        using namespace sercom;

        // keep track of device info responses of a node with an address
        if (deviceInfoResponse.getProtocolVersion() == Definitions::ROUTED_PROTOCOL_VERSION) {
            ++this->routedDeviceInfoCount;
        }

        // print the device info response
        const std::string line = "  device " + std::string(deviceInfoResponse.getDeviceName())
                                 + ", protocol version "
                                 + std::to_string(deviceInfoResponse.getProtocolVersion())
                                 + ", node address "
                                 + std::to_string(deviceInfoResponse.getNodeAddress());
        HostUtils::printLine(line);
    }

    /*
     * Listen for a request that timed out or was rejected and print it.
     */
    void onRequestFailed(const uint16_t messageCounter,
                         const sercom::Messages::MessageType messageType,
                         const sercom::Definitions::DeliveryFailureType deliveryFailureType,
                         const sercom::Definitions::MessageStatusType remoteMessageStatusType) final
    {
        using namespace sercom;

        // keep track of the route statuses
        if (remoteMessageStatusType == Definitions::MessageStatusType::UNREACHABLE) {
            ++this->unreachableCount;
        }
        if (remoteMessageStatusType == Definitions::MessageStatusType::EXPIRED) {
            ++this->expiredCount;
        }

        // print the failed request
        const std::string line
            = "  request #" + std::to_string(messageCounter) + " "
              + std::string(Messages::getMessageTypeText(messageType))
              + " failed: " + std::string(Definitions::fromDeliveryFailureType(deliveryFailureType))
              + ", " + std::string(HostUtils::getMessageStatusTypeText(remoteMessageStatusType));
        HostUtils::printLine(line);
    }

public:
    /*
     * Listen for a set LED response that this example does not ask for.
     */
    void onReceivedSetLedResponse(
        [[maybe_unused]] const sercom::Messages::SetLedResponse &setLedResponse) final
    {}

    /*
     * Listen for a telemetry report response that this example does not ask for.
     */
    void onReceivedTelemetryReportResponse(
        [[maybe_unused]] const sercom::Messages::TelemetryReportResponse &telemetryReportResponse)
        final
    {}

    /*
     * Listen for a log report response that this example does not ask for.
     */
    void onReceivedLogReportResponse(
        [[maybe_unused]] const sercom::Messages::LogReportResponse &logReportResponse) final
    {}

    /*
     * Listen for an echo response that this example does not ask for.
     */
    void onReceivedEchoResponse(
        [[maybe_unused]] const sercom::Messages::EchoResponse &echoResponse) final
    {}

    /*
     * Listen for a request that the first node took, its response shows the end to end result.
     */
    void onRequestConfirmed([[maybe_unused]] const uint16_t messageCounter,
                            [[maybe_unused]] const sercom::Messages::MessageType messageType) final
    {}

public:
    /*
     * Get the number of received ping responses.
     */
    [[nodiscard]] uint32_t getPingResponseCount() const
    {
        // expose the number of ping responses
        return this->pingResponseCount;
    }

    /*
     * Get the number of device info responses of a node with an address.
     */
    [[nodiscard]] uint32_t getRoutedDeviceInfoCount() const
    {
        // expose the number of device info responses of a node with an address
        return this->routedDeviceInfoCount;
    }

    /*
     * Get the number of requests that failed as unreachable.
     */
    [[nodiscard]] uint32_t getUnreachableCount() const
    {
        // expose the number of unreachable requests
        return this->unreachableCount;
    }

    /*
     * Get the number of requests that failed as expired.
     */
    [[nodiscard]] uint32_t getExpiredCount() const
    {
        // expose the number of expired requests
        return this->expiredCount;
    }

private:
    uint32_t pingResponseCount;     // received ping responses
    uint32_t routedDeviceInfoCount; // device info responses of a node with an address
    uint32_t unreachableCount;      // requests failed as unreachable
    uint32_t expiredCount;          // requests failed as expired
};

/*
 * Get the router configuration of a node with an address and a time to live.
 */
[[nodiscard]] sercom::Router::RouterConfiguration getRouterConfiguration(uint8_t nodeAddress,
                                                                         uint8_t timeToLive);

/*
 * Hand the waiting data along the chain with one millisecond per hand over.
 */
void exchangeData(MockClock &clock,
                  MockTransport &hostTransport,
                  MockTransport &nodePrimaryTransport,
                  MockTransport &nodeSecondaryTransport,
                  MockTransport &endTransport,
                  sercom::DeviceClient::DeviceClient &deviceClient,
                  sercom::DeviceApplication::DeviceApplication &nodeApplication,
                  sercom::DeviceApplication::DeviceApplication &endApplication);

/*
 * Print the router statistics of a node.
 */
void printRouterStatistics(const sercom::Router::RouterStatistics &routerStatistics);

/*
 * Main entry point of the executable.
 */
int main(int argc, char *argv[]);

int main([[maybe_unused]] int argc, [[maybe_unused]] char *argv[])
{
    using namespace sercom;
    using namespace std::chrono_literals;

    // join the host, node 2 with two ports and node 3 at the end in memory
    static constexpr uint8_t nodeAddress = 2;
    static constexpr uint8_t endNodeAddress = 3;
    MockClock clock;
    MockTransport hostTransport;
    MockTransport nodePrimaryTransport;
    MockTransport nodeSecondaryTransport;
    MockTransport endTransport;
    SimulatedPlatform::SimulatedPlatform nodePlatform(clock, "example-node-2");
    SimulatedPlatform::SimulatedPlatform endPlatform(clock, "example-node-3");
    DeviceApplication::DeviceApplication
        nodeApplication(nodePrimaryTransport,
                        &nodeSecondaryTransport,
                        clock,
                        nodePlatform,
                        Link::LinkConfiguration(),
                        getRouterConfiguration(nodeAddress, Settings::DEFAULT_TIME_TO_LIVE));
    DeviceApplication::DeviceApplication
        endApplication(endTransport,
                       clock,
                       endPlatform,
                       Link::LinkConfiguration(),
                       getRouterConfiguration(endNodeAddress, Settings::DEFAULT_TIME_TO_LIVE));

    // create the device client of the host with the host address
    MockClientHandler mockClientHandler;
    DeviceClient::DeviceClient deviceClient(hostTransport,
                                            clock,
                                            mockClientHandler,
                                            Link::LinkConfiguration(),
                                            getRouterConfiguration(Definitions::HOST_ADDRESS,
                                                                   Settings::DEFAULT_TIME_TO_LIVE));

    // let the nodes run for one second
    static constexpr std::chrono::milliseconds nodeRunTime = 1 * 1000ms;
    clock.advanceTime(nodeRunTime);

    // discover the chain with a ping request to every node
    HostUtils::printLine("host pings every node of the chain");
    uint16_t messageCounter = 0;
    deviceClient.setDestinationAddress(Definitions::BROADCAST_ADDRESS);
    if (deviceClient.sendPingRequest(messageCounter) != Definitions::SendStatusType::SENT) {
        HostUtils::logWarning("Can not send the ping request to every node");
        return 1;
    }
    exchangeData(clock,
                 hostTransport,
                 nodePrimaryTransport,
                 nodeSecondaryTransport,
                 endTransport,
                 deviceClient,
                 nodeApplication,
                 endApplication);

    // ping node 3 and ask it for its device info
    HostUtils::printLine("host pings node 3 and asks it for its device info");
    deviceClient.setDestinationAddress(endNodeAddress);
    if (deviceClient.sendPingRequest(messageCounter) != Definitions::SendStatusType::SENT
        || deviceClient.sendDeviceInfoRequest(messageCounter) != Definitions::SendStatusType::SENT) {
        HostUtils::logWarning("Can not send the requests to node 3");
        return 1;
    }
    exchangeData(clock,
                 hostTransport,
                 nodePrimaryTransport,
                 nodeSecondaryTransport,
                 endTransport,
                 deviceClient,
                 nodeApplication,
                 endApplication);

    // ping node 9 that is not in the chain
    static constexpr uint8_t missingNodeAddress = 9;
    HostUtils::printLine("host pings node 9 that is not in the chain");
    deviceClient.setDestinationAddress(missingNodeAddress);
    if (deviceClient.sendPingRequest(messageCounter) != Definitions::SendStatusType::SENT) {
        HostUtils::logWarning("Can not send the ping request to node 9");
        return 1;
    }
    exchangeData(clock,
                 hostTransport,
                 nodePrimaryTransport,
                 nodeSecondaryTransport,
                 endTransport,
                 deviceClient,
                 nodeApplication,
                 endApplication);

    // ping node 3 with one link to live from a second device client
    HostUtils::printLine("host pings node 3 with a time to live of 1");
    DeviceClient::DeviceClient shortDeviceClient(hostTransport,
                                                 clock,
                                                 mockClientHandler,
                                                 Link::LinkConfiguration(),
                                                 getRouterConfiguration(Definitions::HOST_ADDRESS,
                                                                        1));
    shortDeviceClient.setDestinationAddress(endNodeAddress);
    if (shortDeviceClient.sendPingRequest(messageCounter) != Definitions::SendStatusType::SENT) {
        HostUtils::logWarning("Can not send the ping request with a time to live of 1");
        return 1;
    }
    exchangeData(clock,
                 hostTransport,
                 nodePrimaryTransport,
                 nodeSecondaryTransport,
                 endTransport,
                 shortDeviceClient,
                 nodeApplication,
                 endApplication);

    // print the router statistics of node 2
    HostUtils::printLine("router statistics of node 2");
    printRouterStatistics(nodeApplication.getRouterStatistics());

    // host must have the answers of both nodes, the device info and both failures
    static constexpr uint32_t pingResponseCount = 3;
    if (mockClientHandler.getPingResponseCount() != pingResponseCount
        || mockClientHandler.getRoutedDeviceInfoCount() != 1
        || mockClientHandler.getUnreachableCount() != 1
        || mockClientHandler.getExpiredCount() != 1) {
        HostUtils::logWarning("The host did not get every expected answer of the chain");
        return 1;
    }

    // example finished successfully
    return 0;
}

sercom::Router::RouterConfiguration getRouterConfiguration(const uint8_t nodeAddress,
                                                           const uint8_t timeToLive)
{
    using namespace sercom;

    // create router configuration
    Router::RouterConfiguration routerConfiguration;
    routerConfiguration.nodeAddress = nodeAddress;
    routerConfiguration.timeToLive = timeToLive;

    // expose the router configuration
    return routerConfiguration;
}

void exchangeData(MockClock &clock,
                  MockTransport &hostTransport,
                  MockTransport &nodePrimaryTransport,
                  MockTransport &nodeSecondaryTransport,
                  MockTransport &endTransport,
                  sercom::DeviceClient::DeviceClient &deviceClient,
                  sercom::DeviceApplication::DeviceApplication &nodeApplication,
                  sercom::DeviceApplication::DeviceApplication &endApplication)
{
    using namespace std::chrono_literals;

    // exchange the data until none waits
    static constexpr std::chrono::milliseconds wireDelay = 1ms;
    std::vector<uint8_t> data;
    bool dataWaiting = true;
    while (dataWaiting) {
        // hand the data of the host to node 2
        const bool hostDataWaiting = hostTransport.receiveData(data);
        if (hostDataWaiting) {
            clock.advanceTime(wireDelay);
            nodeApplication.onReceivedData(data.data(), data.size());
        }

        // hand the data that node 2 sends up to the host
        const bool nodePrimaryDataWaiting = nodePrimaryTransport.receiveData(data);
        if (nodePrimaryDataWaiting) {
            clock.advanceTime(wireDelay);
            deviceClient.onReceivedData(data.data(), data.size());
        }

        // hand the data that node 2 sends down to node 3
        const bool nodeSecondaryDataWaiting = nodeSecondaryTransport.receiveData(data);
        if (nodeSecondaryDataWaiting) {
            clock.advanceTime(wireDelay);
            endApplication.onReceivedData(data.data(), data.size());
        }

        // hand the data of node 3 up to node 2
        const bool endDataWaiting = endTransport.receiveData(data);
        if (endDataWaiting) {
            clock.advanceTime(wireDelay);
            nodeApplication.onReceivedSecondaryData(data.data(), data.size());
        }

        // check whether data waited in this round
        dataWaiting = hostDataWaiting || nodePrimaryDataWaiting || nodeSecondaryDataWaiting
                      || endDataWaiting;
    }
}

void printRouterStatistics(const sercom::Router::RouterStatistics &routerStatistics)
{
    using namespace sercom;

    // print the counters of the router
    HostUtils::printLine(
        HostUtils::getReportLine("local frames",
                                 std::to_string(routerStatistics.localFrameCounter)));
    HostUtils::printLine(
        HostUtils::getReportLine("forwarded frames",
                                 std::to_string(routerStatistics.forwardedFrameCounter)));
    HostUtils::printLine(
        HostUtils::getReportLine("broadcast frames",
                                 std::to_string(routerStatistics.broadcastFrameCounter)));
    HostUtils::printLine(
        HostUtils::getReportLine("expired frames",
                                 std::to_string(routerStatistics.expiredFrameCounter)));
    HostUtils::printLine(
        HostUtils::getReportLine("unreachable frames",
                                 std::to_string(routerStatistics.unreachableFrameCounter)));
    HostUtils::printLine(
        HostUtils::getReportLine("failed forwards",
                                 std::to_string(routerStatistics.failedForwardCounter)));
    HostUtils::printLine(
        HostUtils::getReportLine("route failure reports sent",
                                 std::to_string(routerStatistics.outgoingReportCounter)));
}
