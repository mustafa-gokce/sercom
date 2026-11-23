#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#include "../../app/include/DeviceApplication.h"
#include "../../app/include/DeviceClient.h"
#include "../../app/include/Messages.h"
#include "../../core/include/Clock.h"
#include "../../core/include/Definitions.h"
#include "../../core/include/Router.h"
#include "../../core/include/Transport.h"

/*
 * Print a line to the standard output, the consumer has no host library.
 */
void printLine(std::string_view line);

/*
 * Print a warning line to the standard error.
 */
void printWarning(std::string_view warningMessage);

class MockClock final : public sercom::Clock::Clock
{
public:
    /*
     * Mock clock that only moves when the consumer moves it.
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

class MockDevicePlatform final : public sercom::DeviceApplication::DevicePlatform
{
public:
    /*
     * Mock device platform with a fixed device and an LED that only keeps its state.
     */
    MockDevicePlatform()
        : ledOn(false)
    {}
    ~MockDevicePlatform() final {}

    // delete copy constructor and copy assignment operator
    MockDevicePlatform(const MockDevicePlatform &) = delete;
    MockDevicePlatform &operator=(const MockDevicePlatform &) = delete;

    // delete move constructor and move assignment operator
    MockDevicePlatform(MockDevicePlatform &&) = delete;
    MockDevicePlatform &operator=(MockDevicePlatform &&) = delete;

public:
    /*
     * Get whether the LED is on.
     */
    [[nodiscard]] bool getLedOn() const final
    {
        // expose whether the LED is on
        return this->ledOn;
    }

    /*
     * Set whether the LED is on.
     */
    void setLedOn(const bool ledOn) final
    {
        // set whether the LED is on
        this->ledOn = ledOn;
    }

    /*
     * Get the free heap size in bytes.
     */
    [[nodiscard]] uint32_t getFreeHeapSize() const final
    {
        // expose a fixed free heap size
        static constexpr uint32_t freeHeapSize = 176 * 1024;
        return freeHeapSize;
    }

    /*
     * Get the temperature in 0.01 degree Celsius steps.
     */
    [[nodiscard]] int16_t getTemperature() const final
    {
        // expose a fixed temperature
        static constexpr int16_t temperature = 2150;
        return temperature;
    }

    /*
     * Get the device name.
     */
    [[nodiscard]] std::string_view getDeviceName() const final
    {
        // expose a fixed device name
        static constexpr std::string_view deviceName = "consumer-device";
        return deviceName;
    }

private:
    bool ledOn; // LED is on
};

class MockClientHandler final : public sercom::DeviceClient::DeviceClientHandler
{
public:
    /*
     * Mock client handler that prints the ping response and every failed request.
     */
    MockClientHandler()
        : pingResponseReceived(false)
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
     * Listen for a ping response and print it with its node and round trip time.
     */
    void onReceivedPingResponse(const sercom::Messages::PingResponse &pingResponse,
                                const std::chrono::milliseconds roundTripTime) final
    {
        // keep the ping response
        this->pingResponseReceived = true;

        // print the ping response
        const std::string line = "Ping answered by node "
                                 + std::to_string(pingResponse.getSourceAddress()) + " after "
                                 + std::to_string(roundTripTime.count()) + " ms";
        printLine(line);
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

        // warn the user
        const std::string warningMessage
            = "Request #" + std::to_string(messageCounter) + " "
              + std::string(Messages::getMessageTypeText(messageType)) + " failed: "
              + std::string(Definitions::fromDeliveryFailureType(deliveryFailureType)) + ", status "
              + std::to_string(Definitions::fromMessageStatusType(remoteMessageStatusType));
        printWarning(warningMessage);
    }

public:
    /*
     * Get whether a ping response was received.
     */
    [[nodiscard]] bool getPingResponseReceived() const
    {
        // expose whether a ping response was received
        return this->pingResponseReceived;
    }

public:
    /*
     * Listen for a device info response that this consumer does not ask for.
     */
    void onReceivedDeviceInfoResponse(
        [[maybe_unused]] const sercom::Messages::DeviceInfoResponse &deviceInfoResponse) final
    {}

    /*
     * Listen for a set LED response that this consumer does not ask for.
     */
    void onReceivedSetLedResponse(
        [[maybe_unused]] const sercom::Messages::SetLedResponse &setLedResponse) final
    {}

    /*
     * Listen for a telemetry report response that this consumer does not ask for.
     */
    void onReceivedTelemetryReportResponse(
        [[maybe_unused]] const sercom::Messages::TelemetryReportResponse &telemetryReportResponse)
        final
    {}

    /*
     * Listen for a log report response that this consumer does not ask for.
     */
    void onReceivedLogReportResponse(
        [[maybe_unused]] const sercom::Messages::LogReportResponse &logReportResponse) final
    {}

    /*
     * Listen for an echo response that this consumer does not ask for.
     */
    void onReceivedEchoResponse(
        [[maybe_unused]] const sercom::Messages::EchoResponse &echoResponse) final
    {}

    /*
     * Listen for a request the device acknowledged, the response shows it.
     */
    void onRequestConfirmed([[maybe_unused]] const uint16_t messageCounter,
                            [[maybe_unused]] const sercom::Messages::MessageType messageType) final
    {}

private:
    bool pingResponseReceived; // a ping response arrived
};

/*
 * Get the router configuration of a node with an address.
 */
[[nodiscard]] sercom::Router::RouterConfiguration getRouterConfiguration(uint8_t nodeAddress);

/*
 * Hand the waiting data between the client and the device with one millisecond per hand over.
 */
void exchangeData(MockClock &clock,
                  MockTransport &clientTransport,
                  MockTransport &deviceTransport,
                  sercom::DeviceClient::DeviceClient &deviceClient,
                  sercom::DeviceApplication::DeviceApplication &deviceApplication);

/*
 * Main entry point of the executable.
 */
int main(int argc, char *argv[]);

int main([[maybe_unused]] int argc, [[maybe_unused]] char *argv[])
{
    using namespace sercom;

    // join the device of node 2 and the client of the host in memory
    static constexpr uint8_t deviceAddress = 2;
    MockClock clock;
    MockTransport clientTransport;
    MockTransport deviceTransport;
    MockDevicePlatform mockDevicePlatform;
    MockClientHandler mockClientHandler;
    DeviceApplication::DeviceApplication deviceApplication(deviceTransport,
                                                           clock,
                                                           mockDevicePlatform,
                                                           Link::LinkConfiguration(),
                                                           getRouterConfiguration(deviceAddress));
    DeviceClient::DeviceClient deviceClient(clientTransport,
                                            clock,
                                            mockClientHandler,
                                            Link::LinkConfiguration(),
                                            getRouterConfiguration(Definitions::HOST_ADDRESS));
    deviceClient.setDestinationAddress(deviceAddress);

    // print the ping request
    printLine("Consumer sends a ping request to node 2 in memory");

    // ping request must be sent
    uint16_t messageCounter = 0;
    if (deviceClient.sendPingRequest(messageCounter) != Definitions::SendStatusType::SENT) {
        printWarning("Can not send the ping request");
        return 1;
    }

    // hand the ping request and its response over
    exchangeData(clock, clientTransport, deviceTransport, deviceClient, deviceApplication);

    // device must have answered the ping request
    if (!mockClientHandler.getPingResponseReceived()) {
        printWarning("The device did not answer the ping request");
        return 1;
    }

    // example finished successfully
    return 0;
}

void printLine(const std::string_view line)
{
    // print the line at once
    std::cout << line << std::endl;
}

void printWarning(const std::string_view warningMessage)
{
    // print the warning line at once
    std::cerr << "WARNING " << warningMessage << std::endl;
}

sercom::Router::RouterConfiguration getRouterConfiguration(const uint8_t nodeAddress)
{
    using namespace sercom;

    // create router configuration
    Router::RouterConfiguration routerConfiguration;
    routerConfiguration.nodeAddress = nodeAddress;

    // expose the router configuration
    return routerConfiguration;
}

void exchangeData(MockClock &clock,
                  MockTransport &clientTransport,
                  MockTransport &deviceTransport,
                  sercom::DeviceClient::DeviceClient &deviceClient,
                  sercom::DeviceApplication::DeviceApplication &deviceApplication)
{
    using namespace std::chrono_literals;

    // exchange the data until none waits
    static constexpr std::chrono::milliseconds wireDelay = 1ms;
    std::vector<uint8_t> data;
    bool dataWaiting = true;
    while (dataWaiting) {
        // hand the data of the client to the device
        const bool clientDataWaiting = clientTransport.receiveData(data);
        if (clientDataWaiting) {
            clock.advanceTime(wireDelay);
            deviceApplication.onReceivedData(data.data(), data.size());
        }

        // hand the data of the device to the client
        const bool deviceDataWaiting = deviceTransport.receiveData(data);
        if (deviceDataWaiting) {
            clock.advanceTime(wireDelay);
            deviceClient.onReceivedData(data.data(), data.size());
        }

        // check whether data waited in this round
        dataWaiting = clientDataWaiting || deviceDataWaiting;
    }
}
