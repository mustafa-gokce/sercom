#ifndef SERCOM_CLI_APPLICATION_H
#define SERCOM_CLI_APPLICATION_H

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "../../app/include/DeviceClient.h"
#include "../../app/include/Messages.h"
#include "../../core/include/Definitions.h"
#include "../../core/include/Frame.h"
#include "../../core/include/FrameTracer.h"
#include "../../core/include/TraceTransport.h"

#include "../include/HostSettings.h"
#include "../include/HostUtils.h"
#include "../include/SerialPort.h"

namespace sercom::CliApplication {

/*
 * Command type.
 */
enum class CommandType : uint8_t {
    UNKNOWN = 0,   // unknown command
    PING = 1,      // measure the round trip time
    INFO = 2,      // print the device information
    LED = 3,       // switch the device LED
    TELEMETRY = 4, // set the telemetry period and print telemetry
    MONITOR = 5,   // print telemetry and device logs
    STRESS = 6,    // run the echo stress test
};

/*
 * Command type to string.
 */
std::string_view fromCommandType(CommandType commandType);

/*
 * String to command type.
 */
CommandType toCommandType(std::string_view commandType);

/*
 * Command line arguments of the command line tool.
 */
struct CommandLine
{
    std::string portName;                                // serial port name
    uint32_t baudRate = HostSettings::DEFAULT_BAUD_RATE; // baud rate in bits per second
    CommandType commandType = CommandType::UNKNOWN;      // command to run
    uint32_t requestCount = 1;                           // ping or echo requests to send
    bool ledOn = false;                                  // LED is switched on
    std::chrono::milliseconds telemetryPeriod
        = std::chrono::milliseconds::zero(); // telemetry period in milliseconds
    uint32_t echoWindowSize = HostSettings::DEFAULT_ECHO_WINDOW_SIZE;   // echo requests in flight
    uint32_t echoPayloadSize = HostSettings::DEFAULT_ECHO_PAYLOAD_SIZE; // echo payload size in bytes
    std::chrono::milliseconds duration
        = HostSettings::DEFAULT_COMMAND_DURATION_MSEC; // command duration in milliseconds
    bool traced = false;                               // every frame on the serial port is printed
};

/*
 * Echo request that waits for its echo response.
 */
struct PendingEcho
{
    std::chrono::milliseconds requestTimestamp
        = std::chrono::milliseconds::zero(); // time the echo request was sent first
    uint16_t messageCounter = 0;             // message counter of the latest echo request
};

class CliApplication final : public DeviceClient::DeviceClientHandler,
                             public FrameTracer::FrameTracerHandler
{
public:
    /*
     * Command line tool that runs one command against the device on a serial port.
     */
    explicit CliApplication(const CommandLine &commandLine);
    ~CliApplication() final;

    // delete copy constructor and copy assignment operator
    CliApplication(const CliApplication &) = delete;
    CliApplication &operator=(const CliApplication &) = delete;

    // delete move constructor and move assignment operator
    CliApplication(CliApplication &&) = delete;
    CliApplication &operator=(CliApplication &&) = delete;

public:
    /*
     * Run the command until it finishes or a stop signal arrives.
     * Get the exit code of the process.
     */
    [[nodiscard]] int run();

private:
    /*
     * Listen for a ping response together with the measured round trip time.
     */
    void onReceivedPingResponse(const Messages::PingResponse &pingResponse,
                                std::chrono::milliseconds roundTripTime) final;

    /*
     * Listen for a device info response.
     */
    void onReceivedDeviceInfoResponse(const Messages::DeviceInfoResponse &deviceInfoResponse) final;

    /*
     * Listen for a set LED response.
     */
    void onReceivedSetLedResponse(const Messages::SetLedResponse &setLedResponse) final;

    /*
     * Listen for a telemetry report response.
     */
    void onReceivedTelemetryReportResponse(
        const Messages::TelemetryReportResponse &telemetryReportResponse) final;

    /*
     * Listen for a log report response.
     */
    void onReceivedLogReportResponse(const Messages::LogReportResponse &logReportResponse) final;

    /*
     * Listen for an echo response.
     */
    void onReceivedEchoResponse(const Messages::EchoResponse &echoResponse) final;

    /*
     * Listen for a request the device acknowledged.
     */
    void onRequestConfirmed(uint16_t messageCounter, Messages::MessageType messageType) final;

    /*
     * Listen for a request that timed out or was rejected by the device.
     */
    void onRequestFailed(uint16_t messageCounter,
                         Messages::MessageType messageType,
                         Definitions::DeliveryFailureType deliveryFailureType,
                         Definitions::MessageStatusType remoteMessageStatusType) final;

private:
    /*
     * Listen for a traced frame and print it when the frames are traced.
     */
    void onReceivedTracedFrame(FrameTracer::TraceDirectionType traceDirectionType,
                               const Frame::Frame &frame,
                               std::chrono::milliseconds currentTime) final;

    /*
     * Listen for traced bytes outside a frame and print them when the frames are traced.
     */
    void onReceivedUnframedData(FrameTracer::TraceDirectionType traceDirectionType,
                                const uint8_t *data,
                                size_t size,
                                std::chrono::milliseconds currentTime) final;

private:
    /*
     * Clean up on close.
     */
    void cleanUp();

    /*
     * Process the received data of the serial port.
     */
    void processSerialPort();

    /*
     * Maintain the device client, the serial port and the periodic requests.
     */
    void maintain();

    /*
     * Attempt to reconnect the serial port when it is closed or the device went silent.
     */
    void attemptReconnect();

    /*
     * Wait up to the boot time for the first frame of the device after the serial port opened.
     * A ping request asks a device that is already running for an answer.
     * The wait also lasts until that ping request is acknowledged or has failed.
     */
    void waitForDevice();

    /*
     * Send a ping request as periodic request while the link is idle.
     */
    void sendPeriodicRequest();

    /*
     * Start the command.
     */
    void startCommand();

    /*
     * Advance the command once per loop.
     */
    void progressCommand();

    /*
     * Advance the ping command.
     */
    void progressPingCommand();

    /*
     * Advance a command that waits for one response.
     */
    void progressRequestCommand();

    /*
     * Advance a command that prints the device reports until its duration has passed.
     */
    void progressListenCommand();

    /*
     * Advance the stress command.
     */
    void progressStressCommand();

    /*
     * Send the request of a command that waits for one response.
     */
    void sendCommandRequest();

    /*
     * Send the next echo request of the stress command.
     * A busy echo request is sent again before the next one.
     * Get false when no echo request could be sent.
     */
    [[nodiscard]] bool sendStressEchoRequest();

    /*
     * Send the newest busy echo request again.
     * Get false when the echo request could not be sent.
     */
    [[nodiscard]] bool sendEchoRequestAgain();

    /*
     * Send an echo request with the sequence number and remember it as pending.
     * Get false when the echo request could not be sent.
     */
    [[nodiscard]] bool sendEchoRequest(uint32_t sequenceNumber,
                                       std::chrono::milliseconds requestTimestamp);

    /*
     * Check the send status of a request, warn the user and finish the command on a lasting error.
     * Get false when the request is not sent.
     */
    [[nodiscard]] bool checkSendStatus(Messages::MessageType messageType,
                                       Definitions::SendStatusType sendStatusType);

    /*
     * Handle a failed echo request of the stress command.
     */
    void handleFailedEchoRequest(uint16_t messageCounter,
                                 Definitions::MessageStatusType remoteMessageStatusType);

    /*
     * Forget an echo request whose echo response never came.
     */
    void forgetLostEchoRequest(uint32_t sequenceNumber);

    /*
     * Finish the command with the exit code.
     */
    void finish(int exitCode);

    /*
     * Print the stress report.
     */
    void printStressReport() const;

    /*
     * Get the serial port name and the baud rate as text.
     */
    [[nodiscard]] std::string getSerialPortText() const;

    /*
     * Get the echo payload of a sequence number.
     */
    [[nodiscard]] std::vector<uint8_t> getEchoPayload(uint32_t sequenceNumber) const;

private:
    const CommandLine commandLine;                          // command line arguments
    SerialPort::SerialPort serialPort;                      // serial port to the device
    const HostUtils::SteadyClock clock;                     // steady clock of the host
    FrameTracer::FrameTracer frameTracer;                   // tracer of the serial port bytes
    TraceTransport::TraceTransport traceTransport;          // serial port tracing the sent bytes
    DeviceClient::DeviceClient deviceClient;                // protocol client of the device
    bool finished;                                          // command is finished
    int exitCode;                                           // exit code of the process
    bool waitingForResponse;                                // response of a request is awaited
    bool monitoring;                                        // device reports are printed
    std::chrono::milliseconds startTimestamp;               // time the command started
    std::chrono::milliseconds lastRequestTimestamp;         // time of the last request
    std::chrono::milliseconds lastReconnectTimestamp;       // time of the last reconnect attempt
    std::chrono::milliseconds lastPeriodicRequestTimestamp; // time of the last periodic request
    std::chrono::milliseconds lastDeviceMessageTimestamp;   // time of the last device message
    uint64_t outgoingPingRequestCounter;                    // ping requests of the ping command
    uint64_t incomingPingResponseCounter;                   // ping responses of the ping command
    uint64_t failedPingRequestCounter;                      // ping requests without ping response
    uint32_t nextEchoSequenceNumber;                        // sequence number of the next echo
    uint64_t verifiedEchoResponseCounter;                   // echo responses with the echo payload
    uint64_t corruptedEchoResponseCounter;                  // echo responses with another payload
    uint64_t unexpectedEchoResponseCounter;                 // echo responses without echo request
    uint64_t lostEchoRequestCounter;                        // echo requests without echo response
    uint64_t busyEchoRequestCounter;                        // echo requests sent again when busy
    std::chrono::milliseconds echoRoundTripTimeSum;         // sum of the echo round trip times
    std::chrono::milliseconds maximumEchoRoundTripTime;     // longest echo round trip time
    std::map<uint32_t, PendingEcho> pendingEchoes;          // pending echoes by sequence number
    std::vector<uint32_t> retrySequenceNumbers;             // busy echo requests to send again
    std::array<uint8_t, HostSettings::READ_BUFFER_SIZE> readBuffer; // received serial port data
};

} // namespace sercom::CliApplication

#endif // SERCOM_CLI_APPLICATION_H
