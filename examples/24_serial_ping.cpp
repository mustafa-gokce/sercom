#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include <QByteArray>
#include <QCoreApplication>
#include <QEventLoop>
#include <QObject>
#include <QTimer>

#include "../app/include/DeviceClient.h"
#include "../app/include/Messages.h"
#include "../core/include/Clock.h"
#include "../core/include/Definitions.h"
#include "../host/include/HostSettings.h"
#include "../host/include/HostUtils.h"
#include "../host/include/SerialPort.h"

class MockClientHandler final : public sercom::DeviceClient::DeviceClientHandler
{
public:
    /*
     * Mock client handler that keeps the round trip time and the uptime of the latest ping response.
     */
    MockClientHandler()
        : pingResponseReceived(false)
        , roundTripTime(0)
        , uptime(0)
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
     * Listen for a ping response and keep its round trip time and the device uptime.
     */
    void onReceivedPingResponse(const sercom::Messages::PingResponse &pingResponse,
                                const std::chrono::milliseconds roundTripTime) final
    {
        // keep the round trip time and the device uptime
        this->pingResponseReceived = true;
        this->roundTripTime = roundTripTime;
        this->uptime = pingResponse.getUptime();
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

    /*
     * Set whether a ping response was received.
     */
    void setPingResponseReceived(const bool pingResponseReceived)
    {
        // set whether a ping response was received
        this->pingResponseReceived = pingResponseReceived;
    }

    /*
     * Get the round trip time of the latest ping response.
     */
    [[nodiscard]] std::chrono::milliseconds getRoundTripTime() const
    {
        // expose the round trip time
        return this->roundTripTime;
    }

    /*
     * Get the device uptime of the latest ping response.
     */
    [[nodiscard]] std::chrono::milliseconds getUptime() const
    {
        // expose the device uptime
        return this->uptime;
    }

public:
    /*
     * Listen for a device info response that this example does not ask for.
     */
    void onReceivedDeviceInfoResponse(
        [[maybe_unused]] const sercom::Messages::DeviceInfoResponse &deviceInfoResponse) final
    {}

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
     * Listen for a log report response that this example does not print.
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
     * Listen for a ping request the device acknowledged, the ping response follows it.
     */
    void onRequestConfirmed([[maybe_unused]] const uint16_t messageCounter,
                            [[maybe_unused]] const sercom::Messages::MessageType messageType) final
    {}

    /*
     * Listen for a ping request that timed out, the wait sends a new one.
     */
    void onRequestFailed(
        [[maybe_unused]] const uint16_t messageCounter,
        [[maybe_unused]] const sercom::Messages::MessageType messageType,
        [[maybe_unused]] const sercom::Definitions::DeliveryFailureType deliveryFailureType,
        [[maybe_unused]] const sercom::Definitions::MessageStatusType remoteMessageStatusType) final
    {}

private:
    bool pingResponseReceived;               // a ping response arrived
    std::chrono::milliseconds roundTripTime; // round trip time of the latest ping response
    std::chrono::milliseconds uptime;        // device uptime of the latest ping response
};

/*
 * Print the usage text.
 */
void printUsage(std::string_view programName);

/*
 * Parse the command line, warn the user and get false on any error.
 */
[[nodiscard]] bool parseCommandLine(int argc, char *argv[], std::string &portName);

/*
 * Send ping requests on the event loop until a new ping response arrives.
 * Get false when the timeout passes first or the serial port fails.
 */
[[nodiscard]] bool waitForPingResponse(sercom::DeviceClient::DeviceClient &deviceClient,
                                       MockClientHandler &mockClientHandler,
                                       const sercom::Clock::Clock &clock,
                                       std::chrono::milliseconds timeout);

/*
 * Main entry point of the executable.
 */
int main(int argc, char *argv[]);

int main(int argc, char *argv[])
{
    using namespace sercom;

    // initialize the Qt application of the event loop
    const QCoreApplication application(argc, argv);

    // command line must be valid
    std::string portName;
    if (!parseCommandLine(argc, argv, portName)) {
        printUsage(argv[0]);
        return HostSettings::INVALID_COMMAND_LINE_EXIT_CODE;
    }

    // install the stop handlers
    HostUtils::installStopHandlers();

    // serial port must open
    SerialPort::SerialPort serialPort;
    const SerialPort::SerialPortStatusType openStatusType
        = serialPort.open(portName, HostSettings::DEFAULT_BAUD_RATE);
    if (openStatusType != SerialPort::SerialPortStatusType::OK) {
        // warn the user
        const std::string warningMessage = "Can not open the serial port " + portName + " ("
                                           + std::string(
                                               SerialPort::fromSerialPortStatusType(openStatusType))
                                           + ")";
        HostUtils::logWarning(warningMessage);

        // do not proceed further
        return 1;
    }

    // create the device client on the serial port
    const HostUtils::SteadyClock clock;
    MockClientHandler mockClientHandler;
    DeviceClient::DeviceClient deviceClient(serialPort, clock, mockClientHandler);

    // hand the received data to the device client
    QObject::connect(&serialPort,
                     &SerialPort::SerialPort::receivedData,
                     [&deviceClient](const QByteArray &data) {
                         deviceClient.onReceivedData(reinterpret_cast<const uint8_t *>(
                                                         data.constData()),
                                                     static_cast<size_t>(data.size()));
                     });

    // stop the wait with a failure when the serial port fails
    QObject::connect(&serialPort,
                     &SerialPort::SerialPort::errorHappened,
                     [&serialPort](const SerialPort::SerialPortStatusType serialPortStatusType) {
                         // warn the user
                         const std::string warningMessage = "Can not read the serial port "
                                                            + serialPort.getPortName() + " ("
                                                            + std::string(
                                                                SerialPort::fromSerialPortStatusType(
                                                                    serialPortStatusType))
                                                            + ")";
                         HostUtils::logWarning(warningMessage);

                         // stop the event loop with the failure
                         QCoreApplication::exit(1);
                     });

    // print the wait for the device
    const std::string waitingLine = "waiting for the device on "
                                    + HostUtils::getSerialPortText(portName,
                                                                   HostSettings::DEFAULT_BAUD_RATE)
                                    + " for at most "
                                    + std::to_string(HostSettings::DEVICE_BOOT_TIMEOUT_MSEC.count())
                                    + " ms";
    HostUtils::printLine(waitingLine);

    // wait for the device within the boot timeout
    const bool deviceAnswered = waitForPingResponse(deviceClient,
                                                    mockClientHandler,
                                                    clock,
                                                    HostSettings::DEVICE_BOOT_TIMEOUT_MSEC);

    // stop signal must not have arrived
    if (!HostUtils::getRunning()) {
        return 0;
    }

    // device must answer within the boot timeout
    if (!deviceAnswered) {
        HostUtils::logWarning("The device did not answer within the boot timeout");
        return 1;
    }

    // print the answering device
    HostUtils::printLine("device answered, sending one ping request");

    // wait for the response to one more ping request
    const bool pingAnswered = waitForPingResponse(deviceClient,
                                                  mockClientHandler,
                                                  clock,
                                                  HostSettings::RESPONSE_TIMEOUT_MSEC);

    // stop signal must not have arrived
    if (!HostUtils::getRunning()) {
        return 0;
    }

    // device must answer one more ping request
    if (!pingAnswered) {
        HostUtils::logWarning("The device did not answer the ping request");
        return 1;
    }

    // print the round trip time and the device uptime
    const std::string pingLine = "ping response: round trip "
                                 + std::to_string(mockClientHandler.getRoundTripTime().count())
                                 + " ms, device uptime "
                                 + std::to_string(mockClientHandler.getUptime().count()) + " ms";
    HostUtils::printLine(pingLine);

    // close the serial port
    serialPort.close();

    // example finished successfully
    return 0;
}

void printUsage(const std::string_view programName)
{
    using namespace sercom;

    // print the usage text
    const std::string usage = std::string("Usage: ") + std::string(programName)
                              + " <serial-port>\nWaits for the device on the serial port at "
                              + std::to_string(HostSettings::DEFAULT_BAUD_RATE)
                              + " 8N1, sends one ping request and prints its round trip time.";
    HostUtils::printLine(usage);
}

bool parseCommandLine(const int argc, char *argv[], std::string &portName)
{
    using namespace sercom;

    // serial port argument must be given
    const std::vector<std::string> arguments(argv + 1, argv + argc);
    if (arguments.empty()) {
        HostUtils::logWarning("Missing serial port argument");
        return false;
    }

    // serial port argument must be the only argument
    if (arguments.size() > 1) {
        HostUtils::logWarning("Too many arguments, expected the serial port argument only");
        return false;
    }

    // set the serial port name
    portName = arguments.at(0);

    // command line parsed successfully
    return true;
}

bool waitForPingResponse(sercom::DeviceClient::DeviceClient &deviceClient,
                         MockClientHandler &mockClientHandler,
                         const sercom::Clock::Clock &clock,
                         const std::chrono::milliseconds timeout)
{
    using namespace sercom;

    // forget the earlier ping response
    mockClientHandler.setPingResponseReceived(false);

    // send ping requests and maintain the device client once per maintenance period
    const std::chrono::milliseconds waitTimestamp = clock.getTime();
    QEventLoop eventLoop;
    QTimer timer;
    QObject::connect(
        &timer,
        &QTimer::timeout,
        [&deviceClient, &mockClientHandler, &clock, &eventLoop, waitTimestamp, timeout]() {
            // wait must go on until a new ping response, the timeout or a stop signal
            if (mockClientHandler.getPingResponseReceived()
                || clock.getTime() - waitTimestamp >= timeout || !HostUtils::getRunning()) {
                eventLoop.quit();
                return;
            }

            // ping request must be sent when no request waits
            uint16_t messageCounter = 0;
            if (deviceClient.getPendingCount() == 0
                && deviceClient.sendPingRequest(messageCounter)
                       != Definitions::SendStatusType::SENT) {
                HostUtils::logWarning("Can not send the ping request");
                eventLoop.exit(1);
                return;
            }

            // maintain the retransmissions of the device client
            deviceClient.maintain();
        });
    timer.start(HostSettings::MAINTAIN_PERIOD_MSEC);

    // event loop must not stop with a failure
    if (eventLoop.exec() != 0) {
        return false;
    }

    // expose whether a new ping response arrived
    return mockClientHandler.getPingResponseReceived();
}
