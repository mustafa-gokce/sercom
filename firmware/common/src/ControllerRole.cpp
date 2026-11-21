#include "../include/ControllerRole.h"

#include <algorithm>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>

#include "../../../core/include/ByteCodec.h"

namespace sercom::ControllerRole {

using namespace std::chrono_literals;

namespace {

/*
 * Get the echo payload of a sequence number, the sequence number first and a pattern of it after.
 */
[[nodiscard]] std::array<uint8_t, CommonSettings::CONTROLLER_ECHO_PAYLOAD_SIZE> getEchoPayload(
    const uint32_t sequenceNumber)
{
    // store the sequence number first
    std::array<uint8_t, CommonSettings::CONTROLLER_ECHO_PAYLOAD_SIZE> echoPayload
        = std::array<uint8_t, CommonSettings::CONTROLLER_ECHO_PAYLOAD_SIZE>();
    ByteCodec::storeUint32(echoPayload.data(), sequenceNumber);

    // fill the rest with a pattern of the sequence number
    static constexpr uint32_t patternMask = 0xFF;
    for (size_t index = CommonSettings::CONTROLLER_ECHO_HEADER_SIZE; index < echoPayload.size();
         ++index) {
        echoPayload[index] = static_cast<uint8_t>((sequenceNumber + index) & patternMask);
    }

    // expose the echo payload
    return echoPayload;
}

/*
 * Write a temperature in 0.01 degree steps as degrees Celsius with two decimals.
 * The temperature without a reading is written as no reading.
 */
void writeTemperatureText(const int16_t temperature,
                          char *temperatureAsText,
                          const size_t temperatureAsTextSize)
{
    // write a temperature without a reading as no reading
    if (temperature == Messages::UNAVAILABLE_TEMPERATURE) {
        // write no reading
        [[maybe_unused]] const int temperatureAsTextLength = std::snprintf(temperatureAsText,
                                                                           temperatureAsTextSize,
                                                                           "no reading");

        // do not proceed further
        return;
    }

    // write the temperature in degrees Celsius with two decimals
    static constexpr int32_t temperatureStepsPerDegree = 100;
    const int32_t temperatureMagnitude = std::abs(static_cast<int32_t>(temperature));
    [[maybe_unused]] const int temperatureAsTextLength
        = std::snprintf(temperatureAsText,
                        temperatureAsTextSize,
                        "%s%" PRId32 ".%02" PRId32 " C",
                        temperature < 0 ? "-" : "",
                        temperatureMagnitude / temperatureStepsPerDegree,
                        temperatureMagnitude % temperatureStepsPerDegree);
}

} // namespace

std::string_view fromControllerStepType(const ControllerStepType controllerStepType)
{
    switch (controllerStepType) {
    case ControllerStepType::CONNECT: {
        return {"CONNECT"};
    }
    case ControllerStepType::PING: {
        return {"PING"};
    }
    case ControllerStepType::INFO: {
        return {"INFO"};
    }
    case ControllerStepType::TELEMETRY: {
        return {"TELEMETRY"};
    }
    case ControllerStepType::STRESS: {
        return {"STRESS"};
    }
    case ControllerStepType::FINISHED: {
        return {"FINISHED"};
    }
    case ControllerStepType::UNKNOWN:
    default: {
        return {"UNKNOWN"};
    }
    }
}

ControllerStepType toControllerStepType(const std::string_view controllerStepType)
{
    if (controllerStepType == "CONNECT") {
        return ControllerStepType::CONNECT;
    }
    if (controllerStepType == "PING") {
        return ControllerStepType::PING;
    }
    if (controllerStepType == "INFO") {
        return ControllerStepType::INFO;
    }
    if (controllerStepType == "TELEMETRY") {
        return ControllerStepType::TELEMETRY;
    }
    if (controllerStepType == "STRESS") {
        return ControllerStepType::STRESS;
    }
    if (controllerStepType == "FINISHED") {
        return ControllerStepType::FINISHED;
    }
    return ControllerStepType::UNKNOWN;
}

ControllerRole::ControllerRole(Transport::Transport &transport,
                               const Clock::Clock &clock,
                               FirmwarePlatform::FirmwarePlatform &firmwarePlatform,
                               Console::Console &console)
    : console(console)
    , clock(clock)
    , deviceClient(transport, clock, *this)
    , activityLed(firmwarePlatform, clock)
    , controllerStepType(ControllerStepType::CONNECT)
    , controllerReport()
    , stepTimestamp(clock.getTime())
    , sentRequestCount(0)
    , nextEchoSequenceNumber(0)
    , pendingEchoes()
    , lastIncomingFrameCounter(0)
{}

ControllerRole::~ControllerRole() {}

void ControllerRole::onReceivedData(const uint8_t *data, const size_t size)
{
    // hand the data to the device client
    this->deviceClient.onReceivedData(data, size);
}

void ControllerRole::reportLog(const Messages::LogLevelType logLevelType,
                               const std::string_view text)
{
    // print the log line
    this->console.printLog(logLevelType, text);
}

void ControllerRole::maintain()
{
    // maintain the device client
    this->deviceClient.maintain();

    // progress the current step
    this->progressStep();

    // start an activity blink after a valid received frame
    const uint64_t currentIncomingFrameCounter = this->deviceClient.getLinkStatistics()
                                                     .incomingFrameCounter;
    if (currentIncomingFrameCounter != this->lastIncomingFrameCounter) {
        this->lastIncomingFrameCounter = currentIncomingFrameCounter;
        this->activityLed.startBlink();
    }

    // restore the LED state after an activity blink
    this->activityLed.processActivityLed();
}

ControllerStepType ControllerRole::getControllerStepType() const
{
    // expose the current controller step
    return this->controllerStepType;
}

ControllerReport ControllerRole::getControllerReport() const
{
    // expose the results of the controller steps
    return this->controllerReport;
}

Link::LinkStatistics ControllerRole::getLinkStatistics() const
{
    // expose the link statistics of the device client
    return this->deviceClient.getLinkStatistics();
}

void ControllerRole::onReceivedPingResponse(const Messages::PingResponse &pingResponse,
                                            const std::chrono::milliseconds roundTripTime)
{
    // end the connect step with the first ping response
    if (this->controllerStepType == ControllerStepType::CONNECT) {
        // notify the user
        std::array<char, CommonSettings::LOG_LINE_SIZE> debugMessage
            = std::array<char, CommonSettings::LOG_LINE_SIZE>();
        [[maybe_unused]] const int debugMessageLength
            = std::snprintf(debugMessage.data(),
                            debugMessage.size(),
                            "Device answered after %" PRIu32 " ms",
                            static_cast<uint32_t>(
                                (this->clock.getTime() - this->stepTimestamp).count()));
        this->console.printLog(Messages::LogLevelType::DEBUG, debugMessage.data());

        // continue with the ping step
        this->startStep(ControllerStepType::PING);

        // do not proceed further
        return;
    }

    // ping response must belong to a ping request of the ping step
    if (this->controllerStepType != ControllerStepType::PING
        || roundTripTime > this->clock.getTime() - this->stepTimestamp) {
        return;
    }

    // keep track of ping responses
    ++this->controllerReport.pingResponseCounter;

    // notify the user
    std::array<char, CommonSettings::LOG_LINE_SIZE> debugMessage
        = std::array<char, CommonSettings::LOG_LINE_SIZE>();
    [[maybe_unused]] const int debugMessageLength
        = std::snprintf(debugMessage.data(),
                        debugMessage.size(),
                        "Ping response: round trip %" PRIu32 " ms, device uptime %" PRIu32 " ms",
                        static_cast<uint32_t>(roundTripTime.count()),
                        static_cast<uint32_t>(pingResponse.getUptime().count()));
    this->console.printLog(Messages::LogLevelType::DEBUG, debugMessage.data());
}

void ControllerRole::onReceivedDeviceInfoResponse(
    const Messages::DeviceInfoResponse &deviceInfoResponse)
{
    // device info response must belong to the info step
    if (this->controllerStepType != ControllerStepType::INFO) {
        return;
    }

    // keep track of device info responses
    ++this->controllerReport.deviceInfoResponseCounter;

    // notify the user
    const std::string_view deviceName = deviceInfoResponse.getDeviceName();
    std::array<char, CommonSettings::LOG_LINE_SIZE> debugMessage
        = std::array<char, CommonSettings::LOG_LINE_SIZE>();
    [[maybe_unused]] const int debugMessageLength
        = std::snprintf(debugMessage.data(),
                        debugMessage.size(),
                        "Device info: %.*s, firmware %" PRIu32 ".%" PRIu32 ", uptime %" PRIu32
                        " ms, free heap %" PRIu32 " bytes",
                        static_cast<int>(deviceName.size()),
                        deviceName.data(),
                        static_cast<uint32_t>(deviceInfoResponse.getFirmwareVersionMajor()),
                        static_cast<uint32_t>(deviceInfoResponse.getFirmwareVersionMinor()),
                        static_cast<uint32_t>(deviceInfoResponse.getUptime().count()),
                        deviceInfoResponse.getFreeHeapSize());
    this->console.printLog(Messages::LogLevelType::DEBUG, debugMessage.data());
}

void ControllerRole::onReceivedSetLedResponse(
    [[maybe_unused]] const Messages::SetLedResponse &setLedResponse)
{}

void ControllerRole::onReceivedTelemetryReportResponse(
    const Messages::TelemetryReportResponse &telemetryReportResponse)
{
    // telemetry report response must belong to the telemetry step
    if (this->controllerStepType != ControllerStepType::TELEMETRY
        || this->controllerReport.telemetryReportResponseCounter
               >= CommonSettings::CONTROLLER_TELEMETRY_COUNT) {
        return;
    }

    // keep track of telemetry report responses
    ++this->controllerReport.telemetryReportResponseCounter;

    // get the temperature as text
    static constexpr size_t temperatureAsTextSize = 16;
    std::array<char, temperatureAsTextSize> temperatureAsText
        = std::array<char, temperatureAsTextSize>();
    writeTemperatureText(telemetryReportResponse.getTemperature(),
                         temperatureAsText.data(),
                         temperatureAsText.size());

    // notify the user
    std::array<char, CommonSettings::LOG_LINE_SIZE> debugMessage
        = std::array<char, CommonSettings::LOG_LINE_SIZE>();
    [[maybe_unused]] const int debugMessageLength
        = std::snprintf(debugMessage.data(),
                        debugMessage.size(),
                        "Telemetry #%" PRIu32 ": uptime %" PRIu32 " ms, free heap %" PRIu32
                        " bytes, temperature %s",
                        static_cast<uint32_t>(telemetryReportResponse.getSequence()),
                        static_cast<uint32_t>(telemetryReportResponse.getUptime().count()),
                        telemetryReportResponse.getFreeHeapSize(),
                        temperatureAsText.data());
    this->console.printLog(Messages::LogLevelType::DEBUG, debugMessage.data());
}

void ControllerRole::onReceivedLogReportResponse(const Messages::LogReportResponse &logReportResponse)
{
    // print the log line of the device with its level
    const std::string_view text = logReportResponse.getText();
    std::array<char, CommonSettings::CONSOLE_LINE_SIZE> logMessage
        = std::array<char, CommonSettings::CONSOLE_LINE_SIZE>();
    [[maybe_unused]] const int logMessageLength = std::snprintf(logMessage.data(),
                                                                logMessage.size(),
                                                                "Device log: %.*s",
                                                                static_cast<int>(text.size()),
                                                                text.data());
    this->console.printLog(logReportResponse.getLogLevelType(), logMessage.data());
}

void ControllerRole::onReceivedEchoResponse(const Messages::EchoResponse &echoResponse)
{
    // echo response must belong to the stress step
    if (this->controllerStepType != ControllerStepType::STRESS) {
        return;
    }

    // echo response must carry a sequence number
    const uint8_t *payload = echoResponse.getPayload();
    const size_t payloadLength = echoResponse.getPayloadLength();
    if (payloadLength < CommonSettings::CONTROLLER_ECHO_HEADER_SIZE) {
        ++this->controllerReport.corruptedEchoResponseCounter;
        return;
    }

    // pending echo of the echo response must be found
    const uint32_t sequenceNumber = ByteCodec::loadUint32(payload);
    const std::array<PendingEcho, CommonSettings::CONTROLLER_ECHO_WINDOW_SIZE>::iterator &iterator
        = std::find_if(this->pendingEchoes.begin(),
                       this->pendingEchoes.end(),
                       [sequenceNumber](const PendingEcho &pendingEcho) {
                           return pendingEcho.used && pendingEcho.sequenceNumber == sequenceNumber;
                       });
    if (iterator == this->pendingEchoes.end()) {
        ++this->controllerReport.unexpectedEchoResponseCounter;
        return;
    }

    // forget the answered echo request
    iterator->used = false;
    iterator->sent = false;

    // keep track of the echo responses by their payload
    const std::array<uint8_t, CommonSettings::CONTROLLER_ECHO_PAYLOAD_SIZE> echoPayload
        = getEchoPayload(sequenceNumber);
    if (payloadLength == echoPayload.size()
        && std::equal(echoPayload.cbegin(), echoPayload.cend(), payload)) {
        ++this->controllerReport.verifiedEchoResponseCounter;
    } else {
        ++this->controllerReport.corruptedEchoResponseCounter;
    }
}

void ControllerRole::onRequestConfirmed([[maybe_unused]] const uint16_t messageCounter,
                                        [[maybe_unused]] const Messages::MessageType messageType)
{}

void ControllerRole::onRequestFailed(const uint16_t messageCounter,
                                     const Messages::MessageType messageType,
                                     const Definitions::DeliveryFailureType deliveryFailureType,
                                     const Definitions::MessageStatusType remoteMessageStatusType)
{
    // failed request must belong to a running step after the connect step
    if (this->controllerStepType == ControllerStepType::CONNECT
        || this->controllerStepType == ControllerStepType::FINISHED) {
        return;
    }

    // busy request must be sent again
    if (deliveryFailureType == Definitions::DeliveryFailureType::REJECTED
        && remoteMessageStatusType == Definitions::MessageStatusType::BUSY) {
        // keep track of busy retries
        ++this->controllerReport.busyRetryCounter;

        // send the request again
        this->retryRequest(messageCounter, messageType);

        // do not proceed further
        return;
    }

    // keep track of failed requests
    ++this->controllerReport.failedRequestCounter;

    // warn the user
    const std::string_view messageTypeAsText = Messages::getMessageTypeText(messageType);
    const std::string_view deliveryFailureTypeAsText = Definitions::fromDeliveryFailureType(
        deliveryFailureType);
    std::array<char, CommonSettings::LOG_LINE_SIZE> warningMessage
        = std::array<char, CommonSettings::LOG_LINE_SIZE>();
    [[maybe_unused]] const int warningMessageLength
        = std::snprintf(warningMessage.data(),
                        warningMessage.size(),
                        "Request %.*s failed: %.*s, message status %" PRIu32,
                        static_cast<int>(messageTypeAsText.size()),
                        messageTypeAsText.data(),
                        static_cast<int>(deliveryFailureTypeAsText.size()),
                        deliveryFailureTypeAsText.data(),
                        static_cast<uint32_t>(
                            Definitions::fromMessageStatusType(remoteMessageStatusType)));
    this->console.printLog(Messages::LogLevelType::WARNING, warningMessage.data());

    // finish the controller with its failure
    this->finish(false);
}

void ControllerRole::progressStep()
{
    // controller must not be finished
    if (this->controllerStepType == ControllerStepType::FINISHED) {
        return;
    }

    // step must end within the step timeout
    const std::chrono::milliseconds stepDuration = this->clock.getTime() - this->stepTimestamp;
    if (stepDuration >= CommonSettings::CONTROLLER_STEP_TIMEOUT_MSEC) {
        // warn the user
        const std::string_view controllerStepTypeAsText = fromControllerStepType(
            this->controllerStepType);
        std::array<char, CommonSettings::LOG_LINE_SIZE> warningMessage
            = std::array<char, CommonSettings::LOG_LINE_SIZE>();
        [[maybe_unused]] const int warningMessageLength
            = std::snprintf(warningMessage.data(),
                            warningMessage.size(),
                            "Controller step %.*s timed out after %" PRIu32 " ms",
                            static_cast<int>(controllerStepTypeAsText.size()),
                            controllerStepTypeAsText.data(),
                            static_cast<uint32_t>(stepDuration.count()));
        this->console.printLog(Messages::LogLevelType::WARNING, warningMessage.data());

        // finish the controller with its failure
        this->finish(false);

        // do not proceed further
        return;
    }

    // progress the step by its step type
    switch (this->controllerStepType) {
    // wait for the device
    case ControllerStepType::CONNECT: {
        this->progressConnectStep();
        break;
    }

    // measure the round trip times
    case ControllerStepType::PING: {
        this->progressPingStep();
        break;
    }

    // ask for the device information
    case ControllerStepType::INFO: {
        this->progressInfoStep();
        break;
    }

    // collect the telemetry
    case ControllerStepType::TELEMETRY: {
        this->progressTelemetryStep();
        break;
    }

    // run the echo stress test
    case ControllerStepType::STRESS: {
        this->progressStressStep();
        break;
    }

    // do nothing in the other steps
    case ControllerStepType::FINISHED:
    case ControllerStepType::UNKNOWN:
    default: {
        break;
    }
    }
}

void ControllerRole::progressConnectStep()
{
    // previous ping request must be answered or failed
    if (this->deviceClient.getPendingCount() > 0) {
        return;
    }

    // send a ping request that waits for the device
    uint16_t messageCounter = 0;
    [[maybe_unused]] const bool sent = this->checkSendStatus(Messages::MessageType::PING_REQUEST,
                                                             this->deviceClient.sendPingRequest(
                                                                 messageCounter));
}

void ControllerRole::progressPingStep()
{
    // previous request must be answered or failed
    if (this->deviceClient.getPendingCount() > 0) {
        return;
    }

    // every ping request must be sent
    if (this->sentRequestCount < CommonSettings::CONTROLLER_PING_COUNT) {
        uint16_t messageCounter = 0;
        if (this->checkSendStatus(Messages::MessageType::PING_REQUEST,
                                  this->deviceClient.sendPingRequest(messageCounter))) {
            ++this->sentRequestCount;
        }
        return;
    }

    // every ping response must arrive
    if (this->controllerReport.pingResponseCounter < CommonSettings::CONTROLLER_PING_COUNT) {
        return;
    }

    // continue with the info step
    this->startStep(ControllerStepType::INFO);
}

void ControllerRole::progressInfoStep()
{
    // previous request must be answered or failed
    if (this->deviceClient.getPendingCount() > 0) {
        return;
    }

    // device info request must be sent
    if (this->sentRequestCount == 0) {
        uint16_t messageCounter = 0;
        if (this->checkSendStatus(Messages::MessageType::DEVICE_INFO_REQUEST,
                                  this->deviceClient.sendDeviceInfoRequest(messageCounter))) {
            ++this->sentRequestCount;
        }
        return;
    }

    // device info response must arrive
    if (this->controllerReport.deviceInfoResponseCounter == 0) {
        return;
    }

    // continue with the telemetry step
    this->startStep(ControllerStepType::TELEMETRY);
}

void ControllerRole::progressTelemetryStep()
{
    // previous request must be answered or failed
    if (this->deviceClient.getPendingCount() > 0) {
        return;
    }

    // telemetry must be started
    uint16_t messageCounter = 0;
    if (this->sentRequestCount == 0) {
        if (this->checkSendStatus(Messages::MessageType::SET_TELEMETRY_PERIOD_REQUEST,
                                  this->deviceClient.sendSetTelemetryPeriodRequest(
                                      CommonSettings::CONTROLLER_TELEMETRY_PERIOD_MSEC,
                                      messageCounter))) {
            ++this->sentRequestCount;
        }
        return;
    }

    // every telemetry report response must arrive
    if (this->controllerReport.telemetryReportResponseCounter
        < CommonSettings::CONTROLLER_TELEMETRY_COUNT) {
        return;
    }

    // telemetry must be stopped
    if (this->sentRequestCount == 1) {
        if (this->checkSendStatus(Messages::MessageType::SET_TELEMETRY_PERIOD_REQUEST,
                                  this->deviceClient.sendSetTelemetryPeriodRequest(0ms,
                                                                                   messageCounter))) {
            ++this->sentRequestCount;
        }
        return;
    }

    // continue with the stress step
    this->startStep(ControllerStepType::STRESS);
}

void ControllerRole::progressStressStep()
{
    // keep the echo requests of the echo window in flight
    bool echoRequestSent = true;
    while (echoRequestSent && this->deviceClient.getSendRequestAllowed()) {
        echoRequestSent = this->sendStressEchoRequest();
    }

    // every echo request must be sent and acknowledged
    if (this->nextEchoSequenceNumber < CommonSettings::CONTROLLER_ECHO_COUNT
        || this->deviceClient.getPendingCount() > 0) {
        return;
    }

    // every echo response must arrive
    if (std::any_of(this->pendingEchoes.cbegin(),
                    this->pendingEchoes.cend(),
                    [](const PendingEcho &pendingEcho) { return pendingEcho.used; })) {
        return;
    }

    // notify the user
    std::array<char, CommonSettings::LOG_LINE_SIZE> debugMessage
        = std::array<char, CommonSettings::LOG_LINE_SIZE>();
    [[maybe_unused]] const int debugMessageLength
        = std::snprintf(debugMessage.data(),
                        debugMessage.size(),
                        "Stress report: echo responses verified %" PRIu32 ", corrupted %" PRIu32
                        ", unexpected %" PRIu32,
                        static_cast<uint32_t>(this->controllerReport.verifiedEchoResponseCounter),
                        static_cast<uint32_t>(this->controllerReport.corruptedEchoResponseCounter),
                        static_cast<uint32_t>(this->controllerReport.unexpectedEchoResponseCounter));
    this->console.printLog(Messages::LogLevelType::DEBUG, debugMessage.data());

    // finish the controller with its result
    this->finish(this->controllerReport.verifiedEchoResponseCounter
                     == CommonSettings::CONTROLLER_ECHO_COUNT
                 && this->controllerReport.corruptedEchoResponseCounter == 0
                 && this->controllerReport.unexpectedEchoResponseCounter == 0
                 && this->controllerReport.failedRequestCounter == 0);
}

bool ControllerRole::sendStressEchoRequest()
{
    // send a busy echo request again first
    const std::array<PendingEcho, CommonSettings::CONTROLLER_ECHO_WINDOW_SIZE>::iterator &busyIterator
        = std::find_if(this->pendingEchoes.begin(),
                       this->pendingEchoes.end(),
                       [](const PendingEcho &pendingEcho) {
                           return pendingEcho.used && !pendingEcho.sent;
                       });
    if (busyIterator != this->pendingEchoes.end()) {
        return this->sendEchoRequest(*busyIterator, busyIterator->sequenceNumber);
    }

    // echo requests must remain
    if (this->nextEchoSequenceNumber >= CommonSettings::CONTROLLER_ECHO_COUNT) {
        return false;
    }

    // free pending echo must be found
    const std::array<PendingEcho, CommonSettings::CONTROLLER_ECHO_WINDOW_SIZE>::iterator &freeIterator
        = std::find_if(this->pendingEchoes.begin(),
                       this->pendingEchoes.end(),
                       [](const PendingEcho &pendingEcho) { return !pendingEcho.used; });
    if (freeIterator == this->pendingEchoes.end()) {
        return false;
    }

    // next echo request must be sent
    if (!this->sendEchoRequest(*freeIterator, this->nextEchoSequenceNumber)) {
        return false;
    }

    // continue with the next sequence number
    ++this->nextEchoSequenceNumber;

    // echo request sent successfully
    return true;
}

bool ControllerRole::sendEchoRequest(PendingEcho &pendingEcho, const uint32_t sequenceNumber)
{
    // echo request must be sent
    const std::array<uint8_t, CommonSettings::CONTROLLER_ECHO_PAYLOAD_SIZE> echoPayload
        = getEchoPayload(sequenceNumber);
    uint16_t messageCounter = 0;
    if (!this->checkSendStatus(Messages::MessageType::ECHO_REQUEST,
                               this->deviceClient.sendEchoRequest(echoPayload.data(),
                                                                  echoPayload.size(),
                                                                  messageCounter))) {
        return false;
    }

    // remember the pending echo request
    pendingEcho.used = true;
    pendingEcho.sent = true;
    pendingEcho.sequenceNumber = sequenceNumber;
    pendingEcho.messageCounter = messageCounter;

    // echo request sent successfully
    return true;
}

void ControllerRole::retryRequest(const uint16_t messageCounter,
                                  const Messages::MessageType messageType)
{
    // send a busy echo request of the stress step again
    if (messageType == Messages::MessageType::ECHO_REQUEST) {
        std::for_each(this->pendingEchoes.begin(),
                      this->pendingEchoes.end(),
                      [messageCounter](PendingEcho &pendingEcho) {
                          // pending echo must hold the busy echo request
                          if (!pendingEcho.used || pendingEcho.messageCounter != messageCounter) {
                              return;
                          }

                          // send the echo request again
                          pendingEcho.sent = false;
                      });

        // do not proceed further
        return;
    }

    // send the busy request of the other steps again
    if (this->sentRequestCount > 0) {
        --this->sentRequestCount;
    }
}

bool ControllerRole::checkSendStatus(const Messages::MessageType messageType,
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
        const std::string_view messageTypeAsText = Messages::getMessageTypeText(messageType);
        const std::string_view sendStatusTypeAsText = Definitions::fromSendStatusType(
            sendStatusType);
        std::array<char, CommonSettings::LOG_LINE_SIZE> warningMessage
            = std::array<char, CommonSettings::LOG_LINE_SIZE>();
        [[maybe_unused]] const int warningMessageLength
            = std::snprintf(warningMessage.data(),
                            warningMessage.size(),
                            "Request %.*s not sent: %.*s",
                            static_cast<int>(messageTypeAsText.size()),
                            messageTypeAsText.data(),
                            static_cast<int>(sendStatusTypeAsText.size()),
                            sendStatusTypeAsText.data());
        this->console.printLog(Messages::LogLevelType::WARNING, warningMessage.data());

        // finish the controller with its failure
        this->finish(false);

        // do not proceed further
        return false;
    }

    // request sent successfully
    return true;
}

void ControllerRole::startStep(const ControllerStepType controllerStepType)
{
    // start the step with no request sent yet
    this->controllerStepType = controllerStepType;
    this->stepTimestamp = this->clock.getTime();
    this->sentRequestCount = 0;

    // notify the user
    const std::string_view controllerStepTypeAsText = fromControllerStepType(controllerStepType);
    std::array<char, CommonSettings::LOG_LINE_SIZE> debugMessage
        = std::array<char, CommonSettings::LOG_LINE_SIZE>();
    [[maybe_unused]] const int debugMessageLength
        = std::snprintf(debugMessage.data(),
                        debugMessage.size(),
                        "Controller step %.*s started",
                        static_cast<int>(controllerStepTypeAsText.size()),
                        controllerStepTypeAsText.data());
    this->console.printLog(Messages::LogLevelType::DEBUG, debugMessage.data());
}

void ControllerRole::finish(const bool passed)
{
    // notify the user about a passed controller
    if (passed) {
        this->console.printLog(Messages::LogLevelType::DEBUG, "Controller passed every step");
    }

    // warn the user about the step that failed
    else {
        const std::string_view controllerStepTypeAsText = fromControllerStepType(
            this->controllerStepType);
        std::array<char, CommonSettings::LOG_LINE_SIZE> warningMessage
            = std::array<char, CommonSettings::LOG_LINE_SIZE>();
        [[maybe_unused]] const int warningMessageLength
            = std::snprintf(warningMessage.data(),
                            warningMessage.size(),
                            "Controller failed in step %.*s",
                            static_cast<int>(controllerStepTypeAsText.size()),
                            controllerStepTypeAsText.data());
        this->console.printLog(Messages::LogLevelType::WARNING, warningMessage.data());
    }

    // finish every step with its result
    this->controllerStepType = ControllerStepType::FINISHED;
    this->controllerReport.passed = passed;

    // notify the user about the report
    std::array<char, CommonSettings::LOG_LINE_SIZE> reportMessage
        = std::array<char, CommonSettings::LOG_LINE_SIZE>();
    [[maybe_unused]] const int reportMessageLength
        = std::snprintf(reportMessage.data(),
                        reportMessage.size(),
                        "Controller report: pings %" PRIu32 ", info %" PRIu32 ", telemetry %" PRIu32
                        ", echoes %" PRIu32 " of %" PRIu32 ", busy %" PRIu32 ", failed %" PRIu32,
                        static_cast<uint32_t>(this->controllerReport.pingResponseCounter),
                        static_cast<uint32_t>(this->controllerReport.deviceInfoResponseCounter),
                        static_cast<uint32_t>(this->controllerReport.telemetryReportResponseCounter),
                        static_cast<uint32_t>(this->controllerReport.verifiedEchoResponseCounter),
                        static_cast<uint32_t>(CommonSettings::CONTROLLER_ECHO_COUNT),
                        static_cast<uint32_t>(this->controllerReport.busyRetryCounter),
                        static_cast<uint32_t>(this->controllerReport.failedRequestCounter));
    this->console.printLog(passed ? Messages::LogLevelType::DEBUG : Messages::LogLevelType::WARNING,
                           reportMessage.data());
}

} // namespace sercom::ControllerRole
