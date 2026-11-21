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
#include "../core/include/Frame.h"
#include "../core/include/FrameTracer.h"
#include "../core/include/TraceTransport.h"
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

class MockFrameTracerHandler final : public sercom::FrameTracer::FrameTracerHandler
{
public:
    /*
     * Mock frame tracer handler that prints every traced frame and every unframed byte run.
     */
    MockFrameTracerHandler() = default;
    ~MockFrameTracerHandler() final {}

    // delete copy constructor and copy assignment operator
    MockFrameTracerHandler(const MockFrameTracerHandler &) = delete;
    MockFrameTracerHandler &operator=(const MockFrameTracerHandler &) = delete;

    // delete move constructor and move assignment operator
    MockFrameTracerHandler(MockFrameTracerHandler &&) = delete;
    MockFrameTracerHandler &operator=(MockFrameTracerHandler &&) = delete;

public:
    /*
     * Listen for a traced frame and print it as one line.
     */
    void onReceivedTracedFrame(const sercom::FrameTracer::TraceDirectionType traceDirectionType,
                               const sercom::Frame::Frame &frame,
                               const std::chrono::milliseconds currentTime) final
    {
        // print the traced frame
        sercom::HostUtils::printLine(
            sercom::HostUtils::getTracedFrameLine(traceDirectionType, frame, currentTime));
    }

    /*
     * Listen for traced bytes outside a frame and print them as one line.
     */
    void onReceivedUnframedData(const sercom::FrameTracer::TraceDirectionType traceDirectionType,
                                const uint8_t *data,
                                const size_t size,
                                const std::chrono::milliseconds currentTime) final
    {
        // print the unframed data
        sercom::HostUtils::printLine(
            sercom::HostUtils::getUnframedDataLine(traceDirectionType, data, size, currentTime));
    }
};

class MockClientHandler final : public sercom::DeviceClient::DeviceClientHandler
{
public:
    /*
     * Mock client handler that prints the ping and the device info response.
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
     * Listen for a device info response and print it.
     */
    void onReceivedDeviceInfoResponse(
        const sercom::Messages::DeviceInfoResponse &deviceInfoResponse) final
    {
        // print the device info response
        const std::string line = "host received a device info response: "
                                 + std::string(deviceInfoResponse.getDeviceName()) + ", firmware "
                                 + std::to_string(deviceInfoResponse.getFirmwareVersionMajor())
                                 + "."
                                 + std::to_string(deviceInfoResponse.getFirmwareVersionMinor());
        sercom::HostUtils::printLine(line);
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
     * Listen for a request the device acknowledged, the trace shows it.
     */
    void onRequestConfirmed([[maybe_unused]] const uint16_t messageCounter,
                            [[maybe_unused]] const sercom::Messages::MessageType messageType) final
    {}

    /*
     * Listen for a request that timed out or was rejected, the trace shows it.
     */
    void onRequestFailed(
        [[maybe_unused]] const uint16_t messageCounter,
        [[maybe_unused]] const sercom::Messages::MessageType messageType,
        [[maybe_unused]] const sercom::Definitions::DeliveryFailureType deliveryFailureType,
        [[maybe_unused]] const sercom::Definitions::MessageStatusType remoteMessageStatusType) final
    {}
};

/*
 * Hand the waiting data between the host, the tracer and the device with one millisecond per hand over.
 */
void exchangeData(MockClock &clock,
                  MockTransport &hostTransport,
                  MockTransport &deviceTransport,
                  sercom::FrameTracer::FrameTracer &frameTracer,
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

    // create the simulated device
    MockClock clock;
    MockTransport deviceTransport;
    const std::string deviceName = "example-device";
    SimulatedPlatform::SimulatedPlatform simulatedPlatform(clock, deviceName);
    DeviceApplication::DeviceApplication deviceApplication(deviceTransport,
                                                           clock,
                                                           simulatedPlatform);

    // create the tracing host
    MockTransport hostTransport;
    MockFrameTracerHandler mockFrameTracerHandler;
    FrameTracer::FrameTracer frameTracer(mockFrameTracerHandler,
                                         clock,
                                         FrameTracer::TraceDirectionType::HOST_TO_DEVICE);
    TraceTransport::TraceTransport traceTransport(hostTransport, frameTracer);
    MockClientHandler mockClientHandler;
    DeviceClient::DeviceClient deviceClient(traceTransport, clock, mockClientHandler);

    // let the device run for one second
    static constexpr std::chrono::milliseconds deviceRunTime = 1 * 1000ms;
    clock.advanceTime(deviceRunTime);

    // ping request must be sent
    uint16_t messageCounter = 0;
    if (deviceClient.sendPingRequest(messageCounter) != Definitions::SendStatusType::SENT) {
        HostUtils::logWarning("Can not send the ping request");
        return 1;
    }

    // hand the ping request and its response over
    exchangeData(clock, hostTransport, deviceTransport, frameTracer, deviceClient, deviceApplication);

    // device info request must be sent
    if (deviceClient.sendDeviceInfoRequest(messageCounter) != Definitions::SendStatusType::SENT) {
        HostUtils::logWarning("Can not send the device info request");
        return 1;
    }

    // hand the device info request and its response over
    exchangeData(clock, hostTransport, deviceTransport, frameTracer, deviceClient, deviceApplication);

    // host must have received a response to every request
    static constexpr uint64_t requestCount = 2;
    if (deviceClient.getLinkStatistics().deliveredMessageCounter != requestCount) {
        HostUtils::logWarning("The host did not receive a response to every request");
        return 1;
    }

    // example finished successfully
    return 0;
}

void exchangeData(MockClock &clock,
                  MockTransport &hostTransport,
                  MockTransport &deviceTransport,
                  sercom::FrameTracer::FrameTracer &frameTracer,
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

        // hand the device data to the tracer and the host
        const bool deviceDataWaiting = deviceTransport.receiveData(data);
        if (deviceDataWaiting) {
            clock.advanceTime(wireDelay);
            frameTracer.onReceivedData(data.data(), data.size());
            deviceClient.onReceivedData(data.data(), data.size());
        }

        // check whether data waited in this round
        dataWaiting = hostDataWaiting || deviceDataWaiting;
    }
}
