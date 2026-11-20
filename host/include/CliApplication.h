#ifndef SERCOM_CLI_APPLICATION_H
#define SERCOM_CLI_APPLICATION_H

#include <array>
#include <chrono>
#include <cstdint>
#include <string>
#include <string_view>

#include "../../app/include/DeviceClient.h"
#include "../../app/include/Messages.h"
#include "../../core/include/Definitions.h"

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
    uint32_t requestCount = 1;                           // ping requests to send
    bool ledOn = false;                                  // LED is switched on
    std::chrono::milliseconds telemetryPeriod
        = std::chrono::milliseconds::zero(); // telemetry period in milliseconds
    std::chrono::milliseconds duration
        = HostSettings::DEFAULT_COMMAND_DURATION_MSEC; // command duration in milliseconds
};

class CliApplication final : public DeviceClient::DeviceClientHandler
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
     * Send the request of a command that waits for one response.
     */
    void sendCommandRequest();

    /*
     * Check the send status of a request, warn the user and finish the command on a lasting error.
     * Get false when the request is not sent.
     */
    [[nodiscard]] bool checkSendStatus(Messages::MessageType messageType,
                                       Definitions::SendStatusType sendStatusType);

    /*
     * Finish the command with the exit code.
     */
    void finish(int exitCode);

    /*
     * Get the serial port name and the baud rate as text.
     */
    [[nodiscard]] std::string getSerialPortText() const;

private:
    const CommandLine commandLine;                          // command line arguments
    SerialPort::SerialPort serialPort;                      // serial port to the device
    const HostUtils::SteadyClock clock;                     // steady clock of the host
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
    std::array<uint8_t, HostSettings::READ_BUFFER_SIZE> readBuffer; // received serial port data
};

} // namespace sercom::CliApplication

#endif // SERCOM_CLI_APPLICATION_H
