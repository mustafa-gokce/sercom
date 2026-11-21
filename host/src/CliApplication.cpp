#include "../include/CliApplication.h"

#include <QCoreApplication>

namespace sercom::CliApplication {

using namespace std::chrono_literals;

std::string_view fromCommandType(const CommandType commandType)
{
    switch (commandType) {
    case CommandType::PING: {
        return {"ping"};
    }
    case CommandType::INFO: {
        return {"info"};
    }
    case CommandType::LED: {
        return {"led"};
    }
    case CommandType::TELEMETRY: {
        return {"telemetry"};
    }
    case CommandType::MONITOR: {
        return {"monitor"};
    }
    case CommandType::UNKNOWN:
    default: {
        return {};
    }
    }
}

CommandType toCommandType(const std::string_view commandType)
{
    if (commandType == std::string_view("ping")) {
        return CommandType::PING;
    }
    if (commandType == std::string_view("info")) {
        return CommandType::INFO;
    }
    if (commandType == std::string_view("led")) {
        return CommandType::LED;
    }
    if (commandType == std::string_view("telemetry")) {
        return CommandType::TELEMETRY;
    }
    if (commandType == std::string_view("monitor")) {
        return CommandType::MONITOR;
    }
    return CommandType::UNKNOWN;
}

CliApplication::CliApplication(const CommandLine &commandLine)
    : commandLine(commandLine)
    , deviceClient(this->serialPort, this->clock, *this)
    , maintainTimer()
    , finished(false)
    , exitCode(0)
    , waitingForResponse(false)
    , monitoring(false)
    , startTimestamp(0)
    , lastRequestTimestamp(0)
    , lastReconnectTimestamp(0)
    , lastPeriodicRequestTimestamp(0)
    , lastDeviceMessageTimestamp(0)
    , outgoingPingRequestCounter(0)
    , incomingPingResponseCounter(0)
    , failedPingRequestCounter(0)
{
    // connect serial port signals and slots
    QObject::connect(&this->serialPort,
                     &SerialPort::SerialPort::receivedData,
                     this,
                     &CliApplication::onReceivedData);
    QObject::connect(&this->serialPort,
                     &SerialPort::SerialPort::errorHappened,
                     this,
                     &CliApplication::onErrorHappened);

    // maintain the command once per maintenance period
    this->maintainTimer.setSingleShot(false);
    this->maintainTimer.setTimerType(Qt::TimerType::PreciseTimer);
    QObject::connect(&this->maintainTimer, &QTimer::timeout, this, &CliApplication::maintain);
}

CliApplication::~CliApplication()
{
    // invoke clean up procedure
    this->cleanUp();
}

int CliApplication::run()
{
    // notify the user
    const std::string startingDebugMessage = "Starting the CliApplication on "
                                             + this->getSerialPortText();
    HostUtils::logDebug(startingDebugMessage);

    // connect to the serial port
    this->attemptReconnect();

    // notify the user
    const std::string startedDebugMessage = "Started the CliApplication on "
                                            + this->getSerialPortText();
    HostUtils::logDebug(startedDebugMessage);

    // start the command
    this->startCommand();

    // run the command on the event loop until it finishes or is stopped
    this->startTimestamp = this->clock.getTime();
    this->maintainTimer.start(HostSettings::MAINTAIN_PERIOD_MSEC);
    [[maybe_unused]] const int eventLoopExitCode = QCoreApplication::exec();
    this->maintainTimer.stop();

    // get the exit code of an interrupted command
    if (!this->finished) {
        this->exitCode = HostSettings::INTERRUPTED_EXIT_CODE;
    }

    // expose the exit code
    return this->exitCode;
}

void CliApplication::onReceivedPingResponse(const Messages::PingResponse &pingResponse,
                                            const std::chrono::milliseconds roundTripTime)
{
    // keep track of messages
    this->lastDeviceMessageTimestamp = this->clock.getTime();

    // ping response must answer a ping request of the ping command
    if (this->commandLine.commandType != CommandType::PING || !this->waitingForResponse) {
        return;
    }

    // ping response must answer the latest ping request
    if (static_cast<uint32_t>(pingResponse.getRequestTimestamp().count())
        < static_cast<uint32_t>(this->lastRequestTimestamp.count())) {
        return;
    }

    // print the round trip time and the device uptime
    const std::string line = "ping response: round trip " + std::to_string(roundTripTime.count())
                             + " ms, device uptime "
                             + std::to_string(pingResponse.getUptime().count()) + " ms";
    HostUtils::printLine(line);

    // keep track of ping responses
    ++this->incomingPingResponseCounter;
    this->waitingForResponse = false;
}

void CliApplication::onReceivedDeviceInfoResponse(
    const Messages::DeviceInfoResponse &deviceInfoResponse)
{
    // keep track of messages
    this->lastDeviceMessageTimestamp = this->clock.getTime();

    // print the device information
    const std::string line = "device: " + std::string(deviceInfoResponse.getDeviceName())
                             + "\nfirmware: "
                             + std::to_string(deviceInfoResponse.getFirmwareVersionMajor()) + "."
                             + std::to_string(deviceInfoResponse.getFirmwareVersionMinor())
                             + "\nuptime: " + std::to_string(deviceInfoResponse.getUptime().count())
                             + " ms\nfree heap: "
                             + std::to_string(deviceInfoResponse.getFreeHeapSize()) + " bytes";
    HostUtils::printLine(line);

    // device info response must answer the info command
    if (this->commandLine.commandType != CommandType::INFO) {
        return;
    }

    // finish the info command
    this->finish(0);
}

void CliApplication::onReceivedSetLedResponse(const Messages::SetLedResponse &setLedResponse)
{
    // keep track of messages
    this->lastDeviceMessageTimestamp = this->clock.getTime();

    // print the LED state
    const std::string line = std::string("led: ") + (setLedResponse.getLedOn() ? "on" : "off");
    HostUtils::printLine(line);

    // set LED response must answer the LED command
    if (this->commandLine.commandType != CommandType::LED) {
        return;
    }

    // finish the LED command with its result
    this->finish(setLedResponse.getLedOn() == this->commandLine.ledOn ? 0 : 1);
}

void CliApplication::onReceivedTelemetryReportResponse(
    const Messages::TelemetryReportResponse &telemetryReportResponse)
{
    // keep track of messages
    this->lastDeviceMessageTimestamp = this->clock.getTime();

    // telemetry must be monitored
    if (!this->monitoring) {
        return;
    }

    // print the telemetry
    const std::string line = "telemetry #" + std::to_string(telemetryReportResponse.getSequence())
                             + ": uptime "
                             + std::to_string(telemetryReportResponse.getUptime().count())
                             + " ms, free heap "
                             + std::to_string(telemetryReportResponse.getFreeHeapSize())
                             + " bytes, temperature "
                             + HostUtils::getTemperatureText(
                                 telemetryReportResponse.getTemperature());
    HostUtils::printLine(line);
}

void CliApplication::onReceivedLogReportResponse(const Messages::LogReportResponse &logReportResponse)
{
    // keep track of messages
    this->lastDeviceMessageTimestamp = this->clock.getTime();

    // device logs must be monitored
    if (!this->monitoring) {
        return;
    }

    // get the log level as text
    const Messages::LogLevelType logLevelType = logReportResponse.getLogLevelType();
    const std::string logLevelTypeAsText(Messages::getLogLevelTypeText(logLevelType));

    // device log must not be a warning
    if (logLevelType == Messages::LogLevelType::WARNING) {
        // warn the user
        const std::string warningMessage = "Device log " + logLevelTypeAsText + ": "
                                           + std::string(logReportResponse.getText());
        HostUtils::logWarning(warningMessage);

        // do not proceed further
        return;
    }

    // notify the user
    const std::string debugMessage = "Device log " + logLevelTypeAsText + ": "
                                     + std::string(logReportResponse.getText());
    HostUtils::logDebug(debugMessage);
}

void CliApplication::onRequestConfirmed([[maybe_unused]] const uint16_t messageCounter,
                                        [[maybe_unused]] const Messages::MessageType messageType)
{
    // keep track of messages
    this->lastDeviceMessageTimestamp = this->clock.getTime();
}

void CliApplication::onRequestFailed([[maybe_unused]] const uint16_t messageCounter,
                                     const Messages::MessageType messageType,
                                     const Definitions::DeliveryFailureType deliveryFailureType,
                                     const Definitions::MessageStatusType remoteMessageStatusType)
{
    // failed ping request must belong to the ping command
    if (messageType == Messages::MessageType::PING_REQUEST
        && (this->commandLine.commandType != CommandType::PING || !this->waitingForResponse)) {
        return;
    }

    // warn the user
    const std::string warningMessage
        = "Request " + std::string(Messages::getMessageTypeText(messageType))
          + " failed: " + std::string(Definitions::fromDeliveryFailureType(deliveryFailureType))
          + " / " + std::string(HostUtils::getMessageStatusTypeText(remoteMessageStatusType));
    HostUtils::logWarning(warningMessage);

    // keep track of failed ping requests of the ping command
    if (this->commandLine.commandType == CommandType::PING) {
        ++this->failedPingRequestCounter;
        this->waitingForResponse = false;
    }
}

void CliApplication::onReceivedData(const QByteArray &data)
{
    // hand the received data to the device client
    const uint8_t *receivedData = reinterpret_cast<const uint8_t *>(data.constData());
    const size_t size = static_cast<size_t>(data.size());
    this->deviceClient.onReceivedData(receivedData, size);

    // command must be running
    if (this->finished) {
        return;
    }

    // advance the command
    this->progressCommand();
}

void CliApplication::onErrorHappened(const SerialPort::SerialPortStatusType serialPortStatusType)
{
    // warn the user
    const std::string warningMessage = "CliApplication disconnected from serial port on "
                                       + this->getSerialPortText() + " ("
                                       + std::string(SerialPort::fromSerialPortStatusType(
                                           serialPortStatusType))
                                       + ")";
    HostUtils::logWarning(warningMessage);

    // close the serial port until the next reconnect attempt
    this->serialPort.close();

    // clear the last device message timestamp
    this->lastDeviceMessageTimestamp = 0ms;
}

void CliApplication::maintain()
{
    // stop the event loop when a stop signal arrived
    if (!HostUtils::getRunning()) {
        QCoreApplication::quit();
        return;
    }

    // maintain the device client
    this->deviceClient.maintain();

    // attempt to reconnect the serial port periodically
    const std::chrono::milliseconds currentTime = this->clock.getTime();
    if (currentTime - this->lastReconnectTimestamp
        >= HostSettings::SERIAL_PORT_RECONNECT_PERIOD_MSEC) {
        this->lastReconnectTimestamp = currentTime;
        this->attemptReconnect();
    }

    // send the periodic request when it is due
    if (currentTime - this->lastPeriodicRequestTimestamp
        >= HostSettings::DEVICE_PERIODIC_REQUEST_PERIOD_MSEC) {
        this->lastPeriodicRequestTimestamp = currentTime;
        this->sendPeriodicRequest();
    }

    // stop the event loop when the command finished
    if (this->finished) {
        QCoreApplication::quit();
        return;
    }

    // advance the command
    this->progressCommand();
}

void CliApplication::cleanUp()
{
    // notify the user
    const std::string closingDebugMessage = "Closing the CliApplication on "
                                            + this->getSerialPortText();
    HostUtils::logDebug(closingDebugMessage);

    // close the serial port
    this->serialPort.close();

    // notify the user
    const std::string closedDebugMessage = "Closed the CliApplication on "
                                           + this->getSerialPortText();
    HostUtils::logDebug(closedDebugMessage);
}

void CliApplication::attemptReconnect()
{
    // serial port must be closed or the device silent
    const bool deviceSilent = this->lastDeviceMessageTimestamp != 0ms
                              && this->clock.getTime() - this->lastDeviceMessageTimestamp
                                     >= HostSettings::DEVICE_DEACTIVATION_PERIOD_MSEC;
    if (this->serialPort.getOpened() && !deviceSilent) {
        return;
    }

    // report a silent device
    if (deviceSilent) {
        // warn the user
        const std::string warningMessage = "CliApplication lost the device on "
                                           + this->getSerialPortText();
        HostUtils::logWarning(warningMessage);
    }

    // close the serial port
    this->serialPort.close();

    // clear the last device message timestamp
    this->lastDeviceMessageTimestamp = 0ms;

    // notify the user
    HostUtils::logDebug("CliApplication trying to connect to the serial port");

    // serial port must open
    const SerialPort::SerialPortStatusType serialPortStatusType
        = this->serialPort.open(this->commandLine.portName, this->commandLine.baudRate);
    if (serialPortStatusType != SerialPort::SerialPortStatusType::OK) {
        // warn the user
        const std::string warningMessage = "CliApplication can not connect to serial port on "
                                           + this->getSerialPortText() + " ("
                                           + std::string(SerialPort::fromSerialPortStatusType(
                                               serialPortStatusType))
                                           + ")";
        HostUtils::logWarning(warningMessage);

        // do not proceed further
        return;
    }

    // notify the user
    const std::string debugMessage = "CliApplication connected to serial port from "
                                     + this->getSerialPortText();
    HostUtils::logDebug(debugMessage);
}

void CliApplication::sendPeriodicRequest()
{
    // command must not send ping requests of its own
    if (this->commandLine.commandType == CommandType::PING) {
        return;
    }

    // link must be idle
    if (this->deviceClient.getPendingCount() > 0) {
        return;
    }

    // send the ping request
    uint16_t messageCounter = 0;
    [[maybe_unused]] const Definitions::SendStatusType sendStatusType
        = this->deviceClient.sendPingRequest(messageCounter);
}

void CliApplication::startCommand()
{
    // start the command
    switch (this->commandLine.commandType) {
    // leave the requests of the ping command to the loop
    case CommandType::PING: {
        return;
    }

    // finish an unknown command
    case CommandType::UNKNOWN:
    default: {
        this->finish(1);
        break;
    }
    }
}

void CliApplication::progressCommand()
{
    // first frame must be sent within the response timeout
    if (this->deviceClient.getLinkStatistics().outgoingFrameCounter == 0
        && this->clock.getTime() - this->startTimestamp > HostSettings::RESPONSE_TIMEOUT_MSEC) {
        // warn the user
        const std::string warningMessage = "CliApplication can not send to the device on "
                                           + this->getSerialPortText();
        HostUtils::logWarning(warningMessage);

        // finish the command with its failure
        this->finish(1);

        // do not proceed further
        return;
    }

    // advance the command
    switch (this->commandLine.commandType) {
    // advance the ping command
    case CommandType::PING: {
        this->progressPingCommand();
        break;
    }

    // do not proceed further with an unknown command
    case CommandType::UNKNOWN:
    default: {
        return;
    }
    }
}

void CliApplication::progressPingCommand()
{
    // ping requests must remain
    if (this->incomingPingResponseCounter + this->failedPingRequestCounter
        >= this->commandLine.requestCount) {
        this->finish(this->failedPingRequestCounter == 0 ? 0 : 1);
        return;
    }

    // ping response must arrive within the response timeout
    const std::chrono::milliseconds currentTime = this->clock.getTime();
    if (this->waitingForResponse
        && currentTime - this->lastRequestTimestamp > HostSettings::RESPONSE_TIMEOUT_MSEC) {
        // warn the user
        HostUtils::logWarning("Ping request timed out");

        // keep track of failed ping requests
        ++this->failedPingRequestCounter;
        this->waitingForResponse = false;

        // do not proceed further
        return;
    }

    // ping response must not be awaited
    if (this->waitingForResponse) {
        return;
    }

    // ping period must have passed since the last ping request
    if (this->outgoingPingRequestCounter > 0
        && currentTime - this->lastRequestTimestamp < HostSettings::PING_PERIOD_MSEC) {
        return;
    }

    // ping request must be sent
    uint16_t messageCounter = 0;
    const Definitions::SendStatusType sendStatusType = this->deviceClient.sendPingRequest(
        messageCounter);
    if (!this->checkSendStatus(Messages::MessageType::PING_REQUEST, sendStatusType)) {
        return;
    }

    // keep track of ping requests
    ++this->outgoingPingRequestCounter;
    this->waitingForResponse = true;
    this->lastRequestTimestamp = currentTime;
}

bool CliApplication::checkSendStatus(const Messages::MessageType messageType,
                                     const Definitions::SendStatusType sendStatusType)
{
    // link must be able to take the request now
    if (sendStatusType == Definitions::SendStatusType::WINDOW_FULL
        || sendStatusType == Definitions::SendStatusType::TRANSPORT_ERROR) {
        return false;
    }

    // request must be sent
    if (sendStatusType != Definitions::SendStatusType::SENT) {
        // warn the user
        const std::string warningMessage = "Request "
                                           + std::string(Messages::getMessageTypeText(messageType))
                                           + " not sent: "
                                           + std::string(
                                               Definitions::fromSendStatusType(sendStatusType));
        HostUtils::logWarning(warningMessage);

        // finish the command with its failure
        this->finish(1);

        // do not proceed further
        return false;
    }

    // request sent successfully
    return true;
}

void CliApplication::finish(const int exitCode)
{
    // finish with the exit code
    this->finished = true;
    this->exitCode = exitCode;
}

std::string CliApplication::getSerialPortText() const
{
    // expose the serial port name and the baud rate
    return HostUtils::getSerialPortText(this->commandLine.portName, this->commandLine.baudRate);
}

} // namespace sercom::CliApplication
