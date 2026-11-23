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

#include "app/include/DeviceClient.h"
#include "app/include/Messages.h"
#include "core/include/Clock.h"
#include "core/include/Definitions.h"
#include "core/include/Router.h"
#include "host/include/HostSettings.h"
#include "host/include/HostUtils.h"
#include "host/include/SerialPort.h"

class MockClientHandler final : public sercom::DeviceClient::DeviceClientHandler
{
public:
    /*
     * Mock client handler that keeps the source and round trip time of the latest ping response.
     * It keeps the message status of a ping request that a node rejected for good too.
     */
    MockClientHandler()
        : pingResponseReceived(false)
        , sourceAddress(sercom::Definitions::UNASSIGNED_ADDRESS)
        , roundTripTime(0)
        , pingRequestRejected(false)
        , remoteMessageStatusType(sercom::Definitions::MessageStatusType::UNKNOWN)
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
     * Listen for a ping response and keep its source address and round trip time.
     */
    void onReceivedPingResponse(const sercom::Messages::PingResponse &pingResponse,
                                const std::chrono::milliseconds roundTripTime) final
    {
        // keep the source address and the round trip time
        this->pingResponseReceived = true;
        this->sourceAddress = pingResponse.getSourceAddress();
        this->roundTripTime = roundTripTime;
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
     * Get the source address of the latest ping response, unassigned for a direct frame.
     */
    [[nodiscard]] uint8_t getSourceAddress() const
    {
        // expose the source address
        return this->sourceAddress;
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
     * Get whether a node rejected a ping request with a message status other than BUSY.
     */
    [[nodiscard]] bool getPingRequestRejected() const
    {
        // expose whether a ping request was rejected
        return this->pingRequestRejected;
    }

    /*
     * Get the message status of the rejected ping request.
     */
    [[nodiscard]] sercom::Definitions::MessageStatusType getRemoteMessageStatusType() const
    {
        // expose the message status of the rejection
        return this->remoteMessageStatusType;
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
     * Listen for a log report response that this consumer does not print.
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
     * Listen for a ping request the device acknowledged, the ping response follows it.
     */
    void onRequestConfirmed([[maybe_unused]] const uint16_t messageCounter,
                            [[maybe_unused]] const sercom::Messages::MessageType messageType) final
    {}

    /*
     * Listen for a failed ping request and keep a rejection that every ping request would meet.
     * The wait sends a new ping request after a timeout or a BUSY answer.
     */
    void onRequestFailed([[maybe_unused]] const uint16_t messageCounter,
                         const sercom::Messages::MessageType messageType,
                         const sercom::Definitions::DeliveryFailureType deliveryFailureType,
                         const sercom::Definitions::MessageStatusType remoteMessageStatusType) final
    {
        // failed request must be a rejected ping request
        if (messageType != sercom::Messages::MessageType::PING_REQUEST
            || deliveryFailureType != sercom::Definitions::DeliveryFailureType::REJECTED) {
            return;
        }

        // busy node must get the next ping request
        if (remoteMessageStatusType == sercom::Definitions::MessageStatusType::BUSY) {
            return;
        }

        // keep the message status of the rejection
        this->pingRequestRejected = true;
        this->remoteMessageStatusType = remoteMessageStatusType;
    }

private:
    bool pingResponseReceived;               // a ping response arrived
    uint8_t sourceAddress;                   // source address of the latest ping response
    std::chrono::milliseconds roundTripTime; // round trip time of the latest ping response
    bool pingRequestRejected;                // a node rejected a ping request for good
    sercom::Definitions::MessageStatusType
        remoteMessageStatusType; // message status of the rejected ping request
};

/*
 * Print the usage text.
 */
void printUsage(std::string_view programName);

/*
 * Parse the command line, warn the user and get false on any error.
 */
[[nodiscard]] bool parseCommandLine(int argc,
                                    char *argv[],
                                    std::string &portName,
                                    uint8_t &destinationAddress);

/*
 * Get the router configuration of the host, with the host address only for a destination.
 */
[[nodiscard]] sercom::Router::RouterConfiguration getRouterConfiguration(uint8_t destinationAddress);

/*
 * Send a ping request once per periodic request period until a ping response arrives.
 * Get false when the timeout passes first, a node rejects a ping request or the serial port fails.
 */
[[nodiscard]] bool waitForPingResponse(sercom::DeviceClient::DeviceClient &deviceClient,
                                       const MockClientHandler &mockClientHandler,
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
    uint8_t destinationAddress = Definitions::UNASSIGNED_ADDRESS;
    if (!parseCommandLine(argc, argv, portName, destinationAddress)) {
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

    // create the device client on the serial port, a destination makes its requests routed
    const HostUtils::SteadyClock clock;
    MockClientHandler mockClientHandler;
    DeviceClient::DeviceClient deviceClient(serialPort,
                                            clock,
                                            mockClientHandler,
                                            Link::LinkConfiguration(),
                                            getRouterConfiguration(destinationAddress));
    deviceClient.setDestinationAddress(destinationAddress);

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
    const std::string waitingLine = "Waiting for the device on "
                                    + HostUtils::getSerialPortText(portName,
                                                                   HostSettings::DEFAULT_BAUD_RATE)
                                    + " for at most "
                                    + std::to_string(HostSettings::DEVICE_BOOT_TIMEOUT_MSEC.count())
                                    + " ms";
    HostUtils::printLine(waitingLine);

    // wait for the ping response within the boot timeout
    const bool pingAnswered = waitForPingResponse(deviceClient,
                                                  mockClientHandler,
                                                  clock,
                                                  HostSettings::DEVICE_BOOT_TIMEOUT_MSEC);

    // stop signal must not have arrived
    if (!HostUtils::getRunning()) {
        return 0;
    }

    // ping request must not be rejected by the node or by a node on the way to it
    if (mockClientHandler.getPingRequestRejected()) {
        // warn the user
        const std::string warningMessage = "Request "
                                           + std::string(Messages::getMessageTypeText(
                                               Messages::MessageType::PING_REQUEST))
                                           + " failed: "
                                           + std::string(Definitions::fromDeliveryFailureType(
                                               Definitions::DeliveryFailureType::REJECTED))
                                           + " / "
                                           + std::string(HostUtils::getMessageStatusTypeText(
                                               mockClientHandler.getRemoteMessageStatusType()));
        HostUtils::logWarning(warningMessage);

        // do not proceed further
        return 1;
    }

    // device must answer within the boot timeout
    if (!pingAnswered) {
        HostUtils::logWarning("The device did not answer within the boot timeout");
        return 1;
    }

    // print the answered ping with the node that answered it
    const std::string answerText = mockClientHandler.getSourceAddress()
                                           == Definitions::UNASSIGNED_ADDRESS
                                       ? std::string("the device")
                                       : "node "
                                             + std::to_string(mockClientHandler.getSourceAddress());
    const std::string pingLine = "Ping answered by " + answerText + " after "
                                 + std::to_string(mockClientHandler.getRoundTripTime().count())
                                 + " ms";
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
                              + " <serial-port> [--to ADDRESS]\nWaits for the device on the serial"
                                " port at "
                              + std::to_string(HostSettings::DEFAULT_BAUD_RATE)
                              + " 8N1 until it answers a ping request, with --to the node of that"
                                " address from 2 to 254.";
    HostUtils::printLine(usage);
}

bool parseCommandLine(const int argc,
                      char *argv[],
                      std::string &portName,
                      uint8_t &destinationAddress)
{
    using namespace sercom;

    // serial port argument must be given
    const std::vector<std::string> arguments(argv + 1, argv + argc);
    if (arguments.empty()) {
        HostUtils::logWarning("Missing serial port argument");
        return false;
    }

    // serial port argument must come alone or with the destination option
    static constexpr size_t destinationArgumentCount = 3;
    const bool destinationGiven = arguments.size() == destinationArgumentCount
                                  && arguments.at(1) == "--to";
    if (arguments.size() > 1 && !destinationGiven) {
        HostUtils::logWarning(
            "Wrong arguments, expected the serial port argument and an optional --to ADDRESS");
        return false;
    }

    // destination option must name the address of a node
    if (destinationGiven
        && !HostUtils::parseAddressArgument(arguments.at(2), "destination", destinationAddress)) {
        return false;
    }

    // set the serial port name
    portName = arguments.at(0);

    // command line parsed successfully
    return true;
}

sercom::Router::RouterConfiguration getRouterConfiguration(const uint8_t destinationAddress)
{
    using namespace sercom;

    // create router configuration
    Router::RouterConfiguration routerConfiguration;

    // take the host address to reach a destination
    if (destinationAddress != Definitions::UNASSIGNED_ADDRESS) {
        routerConfiguration.nodeAddress = Definitions::HOST_ADDRESS;
    }

    // expose the router configuration
    return routerConfiguration;
}

bool waitForPingResponse(sercom::DeviceClient::DeviceClient &deviceClient,
                         const MockClientHandler &mockClientHandler,
                         const sercom::Clock::Clock &clock,
                         const std::chrono::milliseconds timeout)
{
    using namespace sercom;

    // maintain the device client once per maintenance period, the first ping request is due at once
    const std::chrono::milliseconds deadline = clock.getTime() + timeout;
    std::chrono::milliseconds pingTimestamp = clock.getTime()
                                              - HostSettings::DEVICE_PERIODIC_REQUEST_PERIOD_MSEC;
    QEventLoop eventLoop;
    QTimer timer;
    QObject::connect(
        &timer,
        &QTimer::timeout,
        [&deviceClient, &mockClientHandler, &clock, &eventLoop, &pingTimestamp, deadline]() {
            // wait must go on until a ping response, a rejection, the timeout or a stop signal
            const std::chrono::milliseconds currentTime = clock.getTime();
            if (mockClientHandler.getPingResponseReceived()
                || mockClientHandler.getPingRequestRejected() || currentTime >= deadline
                || !HostUtils::getRunning()) {
                eventLoop.quit();
                return;
            }

            // maintain the retransmissions of the device client
            deviceClient.maintain();

            // ping request must be due once per periodic request period while no request waits
            if (deviceClient.getPendingCount() > 0
                || currentTime - pingTimestamp < HostSettings::DEVICE_PERIODIC_REQUEST_PERIOD_MSEC) {
                return;
            }

            // ping request must be sent
            uint16_t messageCounter = 0;
            if (deviceClient.sendPingRequest(messageCounter) != Definitions::SendStatusType::SENT) {
                HostUtils::logWarning("Can not send the ping request");
                eventLoop.exit(1);
                return;
            }

            // keep track of the ping request
            pingTimestamp = currentTime;
        });
    timer.start(HostSettings::MAINTAIN_PERIOD_MSEC);

    // event loop must not stop with a failure
    if (eventLoop.exec() != 0) {
        return false;
    }

    // expose whether a ping response arrived
    return mockClientHandler.getPingResponseReceived();
}
