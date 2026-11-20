#include "../include/CliApplication.h"

#include <algorithm>
#include <thread>
#include <utility>

#include "../../core/include/ByteCodec.h"

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
    case CommandType::STRESS: {
        return {"stress"};
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
    if (commandType == std::string_view("stress")) {
        return CommandType::STRESS;
    }
    return CommandType::UNKNOWN;
}

CliApplication::CliApplication(const CommandLine &commandLine)
    : commandLine(commandLine)
    , deviceClient(this->serialPort, this->clock, *this)
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
    , nextEchoSequenceNumber(0)
    , verifiedEchoResponseCounter(0)
    , corruptedEchoResponseCounter(0)
    , unexpectedEchoResponseCounter(0)
    , lostEchoRequestCounter(0)
    , busyEchoRequestCounter(0)
    , echoRoundTripTimeSum(0)
    , maximumEchoRoundTripTime(0)
    , readBuffer()
{}

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

    // run the command until it finishes or is stopped
    this->startTimestamp = this->clock.getTime();
    while (!this->finished && HostUtils::getRunning()) {
        // process the received data of the serial port
        this->processSerialPort();

        // maintain the device client and the serial port
        this->maintain();

        // advance the command
        this->progressCommand();
    }

    // get the exit code of an interrupted command
    if (!this->finished) {
        this->exitCode = HostSettings::INTERRUPTED_EXIT_CODE;
    }

    // print the stress report of the stress command
    if (this->commandLine.commandType == CommandType::STRESS) {
        this->printStressReport();
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

    // get the temperature in degrees Celsius with two decimals
    const double temperature = static_cast<double>(telemetryReportResponse.getTemperature())
                               / 100.0;
    const std::string temperatureAsText = HostUtils::getDecimalText(temperature, 2);

    // print the telemetry
    const std::string line = "telemetry #" + std::to_string(telemetryReportResponse.getSequence())
                             + ": uptime "
                             + std::to_string(telemetryReportResponse.getUptime().count())
                             + " ms, free heap "
                             + std::to_string(telemetryReportResponse.getFreeHeapSize())
                             + " bytes, temperature " + temperatureAsText + " C";
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

void CliApplication::onReceivedEchoResponse(const Messages::EchoResponse &echoResponse)
{
    // keep track of messages
    this->lastDeviceMessageTimestamp = this->clock.getTime();

    // echo response must carry a sequence number
    const uint8_t *data = echoResponse.getPayload();
    const size_t size = echoResponse.getPayloadLength();
    ByteCodec::ByteReader byteReader(data, size);
    uint32_t sequenceNumber = 0;
    if (!byteReader.readUint32(sequenceNumber)) {
        // keep track of corrupted echo responses
        ++this->corruptedEchoResponseCounter;

        // do not proceed further
        return;
    }

    // echo request must be pending
    const std::map<uint32_t, PendingEcho>::const_iterator &pendingEchoIterator
        = this->pendingEchoes.find(sequenceNumber);
    if (pendingEchoIterator == this->pendingEchoes.cend()) {
        // keep track of unexpected echo responses
        ++this->unexpectedEchoResponseCounter;

        // do not proceed further
        return;
    }

    // keep track of the echo responses by their payload
    const std::vector<uint8_t> echoPayload(data, data + size);
    if (echoPayload == this->getEchoPayload(sequenceNumber)) {
        ++this->verifiedEchoResponseCounter;
    } else {
        ++this->corruptedEchoResponseCounter;
    }

    // keep track of the round trip time
    const std::chrono::milliseconds roundTripTime = this->clock.getTime()
                                                    - pendingEchoIterator->second.requestTimestamp;
    this->echoRoundTripTimeSum += roundTripTime;
    this->maximumEchoRoundTripTime = std::max(this->maximumEchoRoundTripTime, roundTripTime);

    // forget the answered echo request
    this->pendingEchoes.erase(pendingEchoIterator);
}

void CliApplication::onRequestConfirmed([[maybe_unused]] const uint16_t messageCounter,
                                        const Messages::MessageType messageType)
{
    // keep track of messages
    this->lastDeviceMessageTimestamp = this->clock.getTime();

    // request must set the telemetry period of the telemetry command
    if (messageType != Messages::MessageType::SET_TELEMETRY_PERIOD_REQUEST
        || this->commandLine.commandType != CommandType::TELEMETRY) {
        return;
    }

    // telemetry must not be stopped
    if (this->commandLine.telemetryPeriod == 0ms) {
        // finish the telemetry command
        HostUtils::printLine("telemetry stopped");
        this->finish(0);

        // do not proceed further
        return;
    }

    // print the telemetry until the duration has passed
    this->monitoring = true;
    this->startTimestamp = this->clock.getTime();
}

void CliApplication::onRequestFailed(const uint16_t messageCounter,
                                     const Messages::MessageType messageType,
                                     const Definitions::DeliveryFailureType deliveryFailureType,
                                     const Definitions::MessageStatusType remoteMessageStatusType)
{
    // failed request must not be a stress echo request
    if (messageType == Messages::MessageType::ECHO_REQUEST) {
        this->handleFailedEchoRequest(messageCounter, remoteMessageStatusType);
        return;
    }

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

    // finish every other command
    else {
        this->finish(1);
    }
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

void CliApplication::processSerialPort()
{
    // serial port must be open
    if (!this->serialPort.getOpened()) {
        std::this_thread::sleep_for(HostSettings::READ_TIMEOUT_MSEC);
        return;
    }

    // data must arrive within the read timeout
    size_t size = 0;
    const SerialPort::SerialPortStatusType serialPortStatusType
        = this->serialPort.read(this->readBuffer.data(),
                                this->readBuffer.size(),
                                size,
                                HostSettings::READ_TIMEOUT_MSEC);
    if (serialPortStatusType == SerialPort::SerialPortStatusType::TIMED_OUT) {
        return;
    }

    // serial port must not be disconnected
    if (serialPortStatusType != SerialPort::SerialPortStatusType::OK) {
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

        // do not proceed further
        return;
    }

    // hand the received data to the device client
    this->deviceClient.onReceivedData(this->readBuffer.data(), size);
}

void CliApplication::maintain()
{
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
    // print the device reports right away
    case CommandType::MONITOR: {
        this->monitoring = true;
        break;
    }

    // leave the requests of these commands to the loop
    case CommandType::PING:
    case CommandType::INFO:
    case CommandType::LED:
    case CommandType::TELEMETRY:
    case CommandType::STRESS: {
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

    // advance the commands that wait for one response
    case CommandType::INFO:
    case CommandType::LED: {
        this->progressRequestCommand();
        break;
    }

    // advance the commands that print the device reports
    case CommandType::TELEMETRY:
    case CommandType::MONITOR: {
        this->progressListenCommand();
        break;
    }

    // advance the stress command
    case CommandType::STRESS: {
        this->progressStressCommand();
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

void CliApplication::progressRequestCommand()
{
    // request must be sent
    if (!this->waitingForResponse) {
        this->sendCommandRequest();
        return;
    }

    // response timeout must have passed
    if (this->clock.getTime() - this->lastRequestTimestamp <= HostSettings::RESPONSE_TIMEOUT_MSEC) {
        return;
    }

    // warn the user
    HostUtils::logWarning("No response from the device");

    // finish the command with its failure
    this->finish(1);
}

void CliApplication::progressListenCommand()
{
    // device reports must be printed
    if (!this->monitoring) {
        // advance the request that switches the device reports on
        this->progressRequestCommand();

        // do not proceed further
        return;
    }

    // duration must have passed
    if (this->clock.getTime() - this->startTimestamp < this->commandLine.duration) {
        return;
    }

    // serial port must still be open
    if (!this->serialPort.getOpened()) {
        // warn the user
        const std::string warningMessage = "CliApplication is not connected to the device on "
                                           + this->getSerialPortText();
        HostUtils::logWarning(warningMessage);

        // finish the command with its failure
        this->finish(1);

        // do not proceed further
        return;
    }

    // finish the command
    this->finish(0);
}

void CliApplication::progressStressCommand()
{
    // iterate over the pending echo requests
    const std::chrono::milliseconds currentTime = this->clock.getTime();
    std::vector<uint32_t> lostSequenceNumbers;
    std::for_each(this->pendingEchoes.cbegin(),
                  this->pendingEchoes.cend(),
                  [&lostSequenceNumbers,
                   currentTime](const std::pair<const uint32_t, PendingEcho> &pendingEcho) {
                      // echo response must be overdue
                      if (currentTime - pendingEcho.second.requestTimestamp
                          <= HostSettings::ECHO_LOSS_TIMEOUT_MSEC) {
                          return;
                      }

                      // remember the lost echo request
                      lostSequenceNumbers.push_back(pendingEcho.first);
                  });

    // iterate over the lost echo requests
    std::for_each(lostSequenceNumbers.cbegin(),
                  lostSequenceNumbers.cend(),
                  [this](const uint32_t lostSequenceNumber) {
                      this->forgetLostEchoRequest(lostSequenceNumber);
                  });

    // keep the configured number of echo requests in flight
    bool echoRequestSent = true;
    while (echoRequestSent
           && this->pendingEchoes.size() - this->retrySequenceNumbers.size()
                  < this->commandLine.echoWindowSize
           && this->deviceClient.getSendRequestAllowed()) {
        echoRequestSent = this->sendStressEchoRequest();
    }

    // every echo request must be answered or lost
    if (this->nextEchoSequenceNumber < this->commandLine.requestCount
        || !this->pendingEchoes.empty()) {
        return;
    }

    // finish the stress command with its result
    const bool stressCommandPassed = this->verifiedEchoResponseCounter
                                         == this->commandLine.requestCount
                                     && this->corruptedEchoResponseCounter == 0
                                     && this->unexpectedEchoResponseCounter == 0;
    this->finish(stressCommandPassed ? 0 : 1);
}

void CliApplication::sendCommandRequest()
{
    // send the request of the command
    uint16_t messageCounter = 0;
    switch (this->commandLine.commandType) {
    // send the device info request
    case CommandType::INFO: {
        const Definitions::SendStatusType sendStatusType = this->deviceClient.sendDeviceInfoRequest(
            messageCounter);
        this->waitingForResponse = this->checkSendStatus(Messages::MessageType::DEVICE_INFO_REQUEST,
                                                         sendStatusType);
        break;
    }

    // send the set LED request
    case CommandType::LED: {
        const Definitions::SendStatusType sendStatusType
            = this->deviceClient.sendSetLedRequest(this->commandLine.ledOn, messageCounter);
        this->waitingForResponse = this->checkSendStatus(Messages::MessageType::SET_LED_REQUEST,
                                                         sendStatusType);
        break;
    }

    // send the set telemetry period request
    case CommandType::TELEMETRY: {
        const Definitions::SendStatusType sendStatusType
            = this->deviceClient.sendSetTelemetryPeriodRequest(this->commandLine.telemetryPeriod,
                                                               messageCounter);
        this->waitingForResponse
            = this->checkSendStatus(Messages::MessageType::SET_TELEMETRY_PERIOD_REQUEST,
                                    sendStatusType);
        break;
    }

    // send no request for the other commands
    case CommandType::PING:
    case CommandType::MONITOR:
    case CommandType::STRESS:
    case CommandType::UNKNOWN:
    default: {
        return;
    }
    }

    // request must be sent
    if (!this->waitingForResponse) {
        return;
    }

    // keep track of the request
    this->lastRequestTimestamp = this->clock.getTime();
}

bool CliApplication::sendStressEchoRequest()
{
    // send a busy echo request again first
    if (!this->retrySequenceNumbers.empty()) {
        return this->sendEchoRequestAgain();
    }

    // echo requests must remain
    if (this->nextEchoSequenceNumber >= this->commandLine.requestCount) {
        return false;
    }

    // next echo request must be sent
    if (!this->sendEchoRequest(this->nextEchoSequenceNumber, this->clock.getTime())) {
        return false;
    }

    // continue with the next sequence number
    ++this->nextEchoSequenceNumber;

    // echo request sent successfully
    return true;
}

bool CliApplication::sendEchoRequestAgain()
{
    // get the newest busy echo request
    const uint32_t sequenceNumber = this->retrySequenceNumbers.back();
    this->retrySequenceNumbers.pop_back();

    // echo request must still be pending
    const std::map<uint32_t, PendingEcho>::const_iterator &pendingEchoIterator
        = this->pendingEchoes.find(sequenceNumber);
    if (pendingEchoIterator == this->pendingEchoes.cend()) {
        return true;
    }

    // echo request must be sent again
    if (!this->sendEchoRequest(sequenceNumber, pendingEchoIterator->second.requestTimestamp)) {
        this->retrySequenceNumbers.push_back(sequenceNumber);
        return false;
    }

    // echo request sent again successfully
    return true;
}

bool CliApplication::sendEchoRequest(const uint32_t sequenceNumber,
                                     const std::chrono::milliseconds requestTimestamp)
{
    // echo request must be sent
    const std::vector<uint8_t> echoPayload = this->getEchoPayload(sequenceNumber);
    uint16_t messageCounter = 0;
    const Definitions::SendStatusType sendStatusType
        = this->deviceClient.sendEchoRequest(echoPayload.data(), echoPayload.size(), messageCounter);
    if (!this->checkSendStatus(Messages::MessageType::ECHO_REQUEST, sendStatusType)) {
        return false;
    }

    // remember the pending echo request
    PendingEcho &pendingEcho = this->pendingEchoes[sequenceNumber];
    pendingEcho.requestTimestamp = requestTimestamp;
    pendingEcho.messageCounter = messageCounter;

    // echo request sent successfully
    return true;
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

void CliApplication::handleFailedEchoRequest(
    const uint16_t messageCounter, const Definitions::MessageStatusType remoteMessageStatusType)
{
    // echo request must be pending
    const std::map<uint32_t, PendingEcho>::const_iterator &pendingEchoIterator
        = std::find_if(this->pendingEchoes.cbegin(),
                       this->pendingEchoes.cend(),
                       [messageCounter](const std::pair<const uint32_t, PendingEcho> &pendingEcho) {
                           return pendingEcho.second.messageCounter == messageCounter;
                       });
    if (pendingEchoIterator == this->pendingEchoes.cend()) {
        return;
    }

    // send a busy echo request again
    if (remoteMessageStatusType == Definitions::MessageStatusType::BUSY) {
        // keep track of busy echo requests
        ++this->busyEchoRequestCounter;

        // remember the echo request to send it again
        this->retrySequenceNumbers.push_back(pendingEchoIterator->first);
    }

    // forget a lost echo request
    else {
        // keep track of lost echo requests
        ++this->lostEchoRequestCounter;

        // forget the echo request
        this->pendingEchoes.erase(pendingEchoIterator);
    }
}

void CliApplication::forgetLostEchoRequest(const uint32_t sequenceNumber)
{
    // do not send the lost echo request again
    this->retrySequenceNumbers.erase(std::remove(this->retrySequenceNumbers.begin(),
                                                 this->retrySequenceNumbers.end(),
                                                 sequenceNumber),
                                     this->retrySequenceNumbers.end());

    // keep track of lost echo requests
    ++this->lostEchoRequestCounter;
    this->pendingEchoes.erase(sequenceNumber);
}

void CliApplication::finish(const int exitCode)
{
    // finish with the exit code
    this->finished = true;
    this->exitCode = exitCode;
}

void CliApplication::printStressReport() const
{
    // get the average round trip time
    const uint64_t echoResponseCount = this->verifiedEchoResponseCounter
                                       + this->corruptedEchoResponseCounter;
    const double averageRoundTripTime = echoResponseCount == 0
                                            ? 0.0
                                            : static_cast<double>(this->echoRoundTripTimeSum.count())
                                                  / static_cast<double>(echoResponseCount);

    // get the payload throughput of both directions
    const std::chrono::milliseconds elapsedTime = this->clock.getTime() - this->startTimestamp;
    const double elapsedSeconds = static_cast<double>(elapsedTime.count()) / 1000.0;
    const double payloadThroughput = elapsedSeconds <= 0.0
                                         ? 0.0
                                         : static_cast<double>(this->verifiedEchoResponseCounter)
                                               * static_cast<double>(
                                                   this->commandLine.echoPayloadSize)
                                               * 2.0 / elapsedSeconds;

    // print the stress report
    HostUtils::printLine("stress report");
    HostUtils::printLine(
        HostUtils::getReportLine("requests", std::to_string(this->commandLine.requestCount)));
    HostUtils::printLine(
        HostUtils::getReportLine("verified replies",
                                 std::to_string(this->verifiedEchoResponseCounter)));
    HostUtils::printLine(
        HostUtils::getReportLine("corrupted replies",
                                 std::to_string(this->corruptedEchoResponseCounter)));
    HostUtils::printLine(
        HostUtils::getReportLine("unexpected replies",
                                 std::to_string(this->unexpectedEchoResponseCounter)));
    HostUtils::printLine(
        HostUtils::getReportLine("lost requests", std::to_string(this->lostEchoRequestCounter)));
    HostUtils::printLine(
        HostUtils::getReportLine("busy retries", std::to_string(this->busyEchoRequestCounter)));
    HostUtils::printLine(
        HostUtils::getReportLine("average latency (ms)",
                                 HostUtils::getDecimalText(averageRoundTripTime, 1)));
    HostUtils::printLine(
        HostUtils::getReportLine("maximum latency (ms)",
                                 std::to_string(this->maximumEchoRoundTripTime.count())));
    HostUtils::printLine(
        HostUtils::getReportLine("elapsed (s)", HostUtils::getDecimalText(elapsedSeconds, 1)));
    HostUtils::printLine(HostUtils::getReportLine("payload throughput (B/s)",
                                                  HostUtils::getDecimalText(payloadThroughput, 0)));

    // print the link statistics of the host
    HostUtils::printLine("host link statistics");
    const std::vector<std::string> linkStatisticsLines = HostUtils::getLinkStatisticsLines(
        this->deviceClient.getLinkStatistics());
    std::for_each(linkStatisticsLines.cbegin(),
                  linkStatisticsLines.cend(),
                  [](const std::string &linkStatisticsLine) {
                      HostUtils::printLine(linkStatisticsLine);
                  });
}

std::string CliApplication::getSerialPortText() const
{
    // expose the serial port name and the baud rate
    return HostUtils::getSerialPortText(this->commandLine.portName, this->commandLine.baudRate);
}

std::vector<uint8_t> CliApplication::getEchoPayload(const uint32_t sequenceNumber) const
{
    // echo payload must hold the sequence number
    if (this->commandLine.echoPayloadSize < HostSettings::ECHO_HEADER_SIZE) {
        return {};
    }

    // store the sequence number first
    std::vector<uint8_t> echoPayload(this->commandLine.echoPayloadSize, 0);
    ByteCodec::storeUint32(echoPayload.data(), sequenceNumber);

    // fill the rest with a pattern of the sequence number
    for (size_t index = HostSettings::ECHO_HEADER_SIZE; index < echoPayload.size(); ++index) {
        echoPayload[index] = static_cast<uint8_t>((sequenceNumber + index) & 0xFF);
    }

    // expose the echo payload
    return echoPayload;
}

} // namespace sercom::CliApplication
