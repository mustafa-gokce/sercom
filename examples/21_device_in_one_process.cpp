#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "../app/include/DeviceApplication.h"
#include "../app/include/DeviceClient.h"
#include "../app/include/Messages.h"
#include "../core/include/Clock.h"
#include "../core/include/Definitions.h"
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
     * Mock device platform with a fixed device and an LED that prints its switches.
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

        // print the switched LED
        const std::string line = std::string("  device platform switches the LED ")
                                 + (ledOn ? "on" : "off");
        sercom::HostUtils::printLine(line);
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
        static constexpr std::string_view deviceName = "example-device";
        return deviceName;
    }

private:
    bool ledOn; // LED is on
};

class MockClientHandler final : public sercom::DeviceClient::DeviceClientHandler
{
public:
    /*
     * Mock client handler that prints the ping and the set LED response and every failed request.
     */
    MockClientHandler() = default;
    ~MockClientHandler() final {}

    // delete copy constructor and copy assignment operator
    MockClientHandler(const MockClientHandler &) = delete;
    MockClientHandler &operator=(const MockClientHandler &) = delete;

    // delete move constructor and move assignment operator
    MockClientHandler(MockClientHandler &&) = delete;
    MockClientHandler &operator=(MockClientHandler &&) = delete;

public:
    /*
     * Listen for a ping response and print it with its round trip time.
     */
    void onReceivedPingResponse(const sercom::Messages::PingResponse &pingResponse,
                                const std::chrono::milliseconds roundTripTime) final
    {
        // print the ping response
        const std::string line = "host received a ping response: round trip "
                                 + std::to_string(roundTripTime.count()) + " ms, device uptime "
                                 + std::to_string(pingResponse.getUptime().count()) + " ms";
        sercom::HostUtils::printLine(line);
    }

    /*
     * Listen for a set LED response and print it.
     */
    void onReceivedSetLedResponse(const sercom::Messages::SetLedResponse &setLedResponse) final
    {
        // print the set LED response
        const std::string line = std::string("host received a set LED response: LED ")
                                 + (setLedResponse.getLedOn() ? "on" : "off");
        sercom::HostUtils::printLine(line);
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

        // print the failed request
        const std::string line
            = "host request #" + std::to_string(messageCounter) + " "
              + std::string(Messages::getMessageTypeText(messageType))
              + " failed: " + std::string(Definitions::fromDeliveryFailureType(deliveryFailureType))
              + ", " + std::string(HostUtils::getMessageStatusTypeText(remoteMessageStatusType));
        HostUtils::printLine(line);
    }

public:
    /*
     * Listen for a device info response that this example does not ask for.
     */
    void onReceivedDeviceInfoResponse(
        [[maybe_unused]] const sercom::Messages::DeviceInfoResponse &deviceInfoResponse) final
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
     * Listen for a request the device acknowledged, the response shows it.
     */
    void onRequestConfirmed([[maybe_unused]] const uint16_t messageCounter,
                            [[maybe_unused]] const sercom::Messages::MessageType messageType) final
    {}
};

/*
 * Hand the waiting data between the host and the device with one millisecond per hand over.
 */
void exchangeData(MockClock &clock,
                  MockTransport &hostTransport,
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
    using namespace std::chrono_literals;

    // join the device and the host in memory
    MockClock clock;
    MockTransport hostTransport;
    MockTransport deviceTransport;
    MockDevicePlatform mockDevicePlatform;
    MockClientHandler mockClientHandler;
    DeviceApplication::DeviceApplication deviceApplication(deviceTransport,
                                                           clock,
                                                           mockDevicePlatform);
    DeviceClient::DeviceClient deviceClient(hostTransport, clock, mockClientHandler);

    // let the device run for one second
    static constexpr std::chrono::milliseconds deviceRunTime = 1 * 1000ms;
    clock.advanceTime(deviceRunTime);

    // print the ping request
    HostUtils::printLine("host sends a ping request");

    // ping request must be sent
    uint16_t messageCounter = 0;
    if (deviceClient.sendPingRequest(messageCounter) != Definitions::SendStatusType::SENT) {
        HostUtils::logWarning("Can not send the ping request");
        return 1;
    }

    // hand the ping request and its response over
    exchangeData(clock, hostTransport, deviceTransport, deviceClient, deviceApplication);

    // print the set LED request
    HostUtils::printLine("host sends a set LED request");

    // set LED request must be sent
    if (deviceClient.sendSetLedRequest(true, messageCounter) != Definitions::SendStatusType::SENT) {
        HostUtils::logWarning("Can not send the set LED request");
        return 1;
    }

    // hand the set LED request and its response over
    exchangeData(clock, hostTransport, deviceTransport, deviceClient, deviceApplication);

    // host must have received a response to every request
    static constexpr uint64_t requestCount = 2;
    if (deviceClient.getLinkStatistics().deliveredMessageCounter != requestCount) {
        HostUtils::logWarning("The host did not receive a response to every request");
        return 1;
    }

    // LED of the device must be on
    if (!mockDevicePlatform.getLedOn()) {
        HostUtils::logWarning("The LED of the device is not on");
        return 1;
    }

    // example finished successfully
    return 0;
}

void exchangeData(MockClock &clock,
                  MockTransport &hostTransport,
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
        // hand the data of the host to the device
        const bool hostDataWaiting = hostTransport.receiveData(data);
        if (hostDataWaiting) {
            clock.advanceTime(wireDelay);
            deviceApplication.onReceivedData(data.data(), data.size());
        }

        // hand the data of the device to the host
        const bool deviceDataWaiting = deviceTransport.receiveData(data);
        if (deviceDataWaiting) {
            clock.advanceTime(wireDelay);
            deviceClient.onReceivedData(data.data(), data.size());
        }

        // check whether data waited in this round
        dataWaiting = hostDataWaiting || deviceDataWaiting;
    }
}
