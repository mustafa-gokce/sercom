#ifndef SERCOM_CONTROLLER_ROLE_H
#define SERCOM_CONTROLLER_ROLE_H

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include "../../../app/include/DeviceClient.h"
#include "../../../app/include/Messages.h"
#include "../../../core/include/Clock.h"
#include "../../../core/include/Definitions.h"
#include "../../../core/include/Link.h"
#include "../../../core/include/Transport.h"

#include "../include/ActivityLed.h"
#include "../include/CommonSettings.h"
#include "../include/Console.h"
#include "../include/FirmwarePlatform.h"
#include "../include/FirmwareRole.h"

namespace sercom::ControllerRole {

/*
 * Controller step type.
 */
enum class ControllerStepType : uint8_t {
    UNKNOWN = 0,   // unknown controller step
    CONNECT = 1,   // send ping requests until the device answers
    PING = 2,      // measure the round trip time of ping requests
    INFO = 3,      // ask for the device information
    TELEMETRY = 4, // collect telemetry report responses
    STRESS = 5,    // send echo requests and verify the echo responses
    FINISHED = 6,  // every step is over
};

/*
 * Controller step type to string.
 */
std::string_view fromControllerStepType(ControllerStepType controllerStepType);

/*
 * String to controller step type.
 */
ControllerStepType toControllerStepType(std::string_view controllerStepType);

/*
 * Results of the controller steps.
 */
struct ControllerReport
{
    uint64_t pingResponseCounter = 0;            // ping responses of the ping step
    uint64_t deviceInfoResponseCounter = 0;      // device info responses of the info step
    uint64_t telemetryReportResponseCounter = 0; // telemetry report responses of the telemetry step
    uint64_t verifiedEchoResponseCounter = 0;    // echo responses equal to their echo request
    uint64_t corruptedEchoResponseCounter = 0;   // echo responses that differ from their request
    uint64_t unexpectedEchoResponseCounter = 0;  // echo responses without a pending echo request
    uint64_t busyRetryCounter = 0;               // requests sent again after a busy rejection
    uint64_t failedRequestCounter = 0;           // requests that timed out or were rejected
    bool passed = false;                         // every step passed
};

/*
 * Echo request of the stress step that waits for its echo response.
 */
struct PendingEcho
{
    bool used = false;           // slot holds an echo request
    bool sent = false;           // echo request waits for its echo response
    uint32_t sequenceNumber = 0; // sequence number at the start of the echo payload
    uint16_t messageCounter = 0; // message counter of the latest echo request
};

class ControllerRole final : public FirmwareRole::FirmwareRole,
                             public DeviceClient::DeviceClientHandler
{
public:
    /*
     * Controller role that queries a device with the device client one step after the other.
     * It waits for the device, measures pings, asks for the device information, collects telemetry and runs an echo stress test.
     * Every result is a log line on the console and the last lines say whether every step passed.
     */
    explicit ControllerRole(Transport::Transport &transport,
                            const Clock::Clock &clock,
                            FirmwarePlatform::FirmwarePlatform &firmwarePlatform,
                            Console::Console &console);
    ~ControllerRole() final;

    // delete copy constructor and copy assignment operator
    ControllerRole(const ControllerRole &) = delete;
    ControllerRole &operator=(const ControllerRole &) = delete;

    // delete move constructor and move assignment operator
    ControllerRole(ControllerRole &&) = delete;
    ControllerRole &operator=(ControllerRole &&) = delete;

public:
    /*
     * Listen for received data and hand it to the device client.
     */
    void onReceivedData(const uint8_t *data, size_t size) final;

    /*
     * Print a log line on the console, the controller has no peer that collects log lines.
     */
    void reportLog(Messages::LogLevelType logLevelType, std::string_view text) final;

    /*
     * Maintain the device client, progress the current step and maintain the activity LED.
     */
    void maintain() final;

public:
    /*
     * Get the current controller step.
     */
    [[nodiscard]] ControllerStepType getControllerStepType() const;

    /*
     * Get the results of the controller steps so far.
     */
    [[nodiscard]] ControllerReport getControllerReport() const;

    /*
     * Get the link statistics of the device client.
     */
    [[nodiscard]] Link::LinkStatistics getLinkStatistics() const;

private:
    /*
     * Listen for a ping response, the first one ends the connect step.
     * The ping step counts only the ping responses to its own ping requests.
     */
    void onReceivedPingResponse(const Messages::PingResponse &pingResponse,
                                std::chrono::milliseconds roundTripTime) final;

    /*
     * Listen for a device info response.
     */
    void onReceivedDeviceInfoResponse(const Messages::DeviceInfoResponse &deviceInfoResponse) final;

    /*
     * Listen for a set LED response, the controller sends no set LED request.
     */
    void onReceivedSetLedResponse(const Messages::SetLedResponse &setLedResponse) final;

    /*
     * Listen for a telemetry report response.
     */
    void onReceivedTelemetryReportResponse(
        const Messages::TelemetryReportResponse &telemetryReportResponse) final;

    /*
     * Listen for a log report response and print it on the console.
     */
    void onReceivedLogReportResponse(const Messages::LogReportResponse &logReportResponse) final;

    /*
     * Listen for an echo response and verify it against its echo request.
     */
    void onReceivedEchoResponse(const Messages::EchoResponse &echoResponse) final;

    /*
     * Listen for a request the device acknowledged.
     */
    void onRequestConfirmed(uint16_t messageCounter, Messages::MessageType messageType) final;

    /*
     * Listen for a request that timed out or was rejected.
     * A busy request is sent again and any other failure fails every step but the connect step.
     */
    void onRequestFailed(uint16_t messageCounter,
                         Messages::MessageType messageType,
                         Definitions::DeliveryFailureType deliveryFailureType,
                         Definitions::MessageStatusType remoteMessageStatusType) final;

private:
    /*
     * Progress the current step and fail it when it does not end within the step timeout.
     */
    void progressStep();

    /*
     * Send a ping request whenever no request is pending until the device answers.
     */
    void progressConnectStep();

    /*
     * Send the ping requests one after the other and wait for their ping responses.
     */
    void progressPingStep();

    /*
     * Send the device info request and wait for the device info response.
     */
    void progressInfoStep();

    /*
     * Start the telemetry, wait for the telemetry report responses and stop the telemetry.
     */
    void progressTelemetryStep();

    /*
     * Keep the echo requests of the echo window in flight until every one is answered.
     */
    void progressStressStep();

    /*
     * Send a busy echo request again or the next echo request of the stress step.
     * Get false when no echo request is due, no pending echo is free or the request was not sent.
     */
    [[nodiscard]] bool sendStressEchoRequest();

    /*
     * Send the echo request of a sequence number and remember it in a pending echo.
     */
    [[nodiscard]] bool sendEchoRequest(PendingEcho &pendingEcho, uint32_t sequenceNumber);

    /*
     * Send a request of the current step again after a busy rejection.
     */
    void retryRequest(uint16_t messageCounter, Messages::MessageType messageType);

    /*
     * Get whether a request was sent.
     * A request the link cannot take now is sent again later, any other send status fails the controller.
     */
    [[nodiscard]] bool checkSendStatus(Messages::MessageType messageType,
                                       Definitions::SendStatusType sendStatusType);

    /*
     * Start a step with no request sent yet.
     */
    void startStep(ControllerStepType controllerStepType);

    /*
     * Finish every step with its result and print the controller report.
     */
    void finish(bool passed);

private:
    Console::Console &console;               // reference for console
    const Clock::Clock &clock;               // reference for clock
    DeviceClient::DeviceClient deviceClient; // host side of the protocol towards the device
    ActivityLed::ActivityLed activityLed;    // activity blink of the LED
    ControllerStepType controllerStepType;   // current controller step
    ControllerReport controllerReport;       // results of the steps so far
    std::chrono::milliseconds stepTimestamp; // start time of the current step
    size_t sentRequestCount;                 // requests of the current step sent so far
    uint32_t nextEchoSequenceNumber;         // sequence number of the next echo request
    std::array<PendingEcho, CommonSettings::CONTROLLER_ECHO_WINDOW_SIZE>
        pendingEchoes;                 // echo requests in flight
    uint64_t lastIncomingFrameCounter; // valid frames at the latest blink check
};

} // namespace sercom::ControllerRole

#endif // SERCOM_CONTROLLER_ROLE_H
