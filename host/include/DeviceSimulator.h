#ifndef SERCOM_DEVICE_SIMULATOR_H
#define SERCOM_DEVICE_SIMULATOR_H

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include <QByteArray>
#include <QObject>
#include <QTimer>

#include "../../app/include/DeviceApplication.h"
#include "../../core/include/Definitions.h"
#include "../../core/include/Frame.h"
#include "../../core/include/FrameTracer.h"
#include "../../core/include/Router.h"
#include "../../core/include/TraceTransport.h"

#include "../include/HostSettings.h"
#include "../include/HostUtils.h"
#include "../include/SerialPort.h"
#include "../include/SimulatedPlatform.h"

namespace sercom::DeviceSimulator {

/*
 * Command line arguments of the device simulator.
 */
struct CommandLine
{
    std::string portName;                                // serial port name
    uint32_t baudRate = HostSettings::DEFAULT_BAUD_RATE; // baud rate in bits per second
    std::string deviceName;                              // device name of the simulated device
    std::chrono::milliseconds logPeriod
        = std::chrono::milliseconds::zero(); // heartbeat log period in milliseconds or zero for none
    bool traced = false;                     // every frame on the serial port is printed
    uint8_t nodeAddress = Definitions::UNASSIGNED_ADDRESS;  // address of the simulated node
    std::string downstreamPortName;                         // serial port to the next node
    uint8_t proxyAddress = Definitions::UNASSIGNED_ADDRESS; // address of a next node without one
};

/*
 * Get the router configuration of the simulated node of a command line.
 */
[[nodiscard]] Router::RouterConfiguration getRouterConfiguration(const CommandLine &commandLine);

class DeviceSimulator final : public QObject, public FrameTracer::FrameTracerHandler
{
    Q_OBJECT

public:
    /*
     * Device simulator that answers the host on a serial port like the firmware does.
     * A simulator with an address and a downstream serial port is a router of a daisy chain.
     */
    explicit DeviceSimulator(const CommandLine &commandLine);
    ~DeviceSimulator() final;

    // delete copy constructor and copy assignment operator
    DeviceSimulator(const DeviceSimulator &) = delete;
    DeviceSimulator &operator=(const DeviceSimulator &) = delete;

    // delete move constructor and move assignment operator
    DeviceSimulator(DeviceSimulator &&) = delete;
    DeviceSimulator &operator=(DeviceSimulator &&) = delete;

public:
    /*
     * Run the device simulator on the event loop until a stop signal arrives.
     * Get the exit code of the process.
     */
    [[nodiscard]] int run();

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

private slots:
    /*
     * Listen for received data of the serial port.
     */
    void onReceivedData(const QByteArray &data);

    /*
     * Listen for an error of the serial port and close it until the next reconnect attempt.
     */
    void onErrorHappened(sercom::SerialPort::SerialPortStatusType serialPortStatusType);

    /*
     * Listen for received data of the downstream serial port.
     */
    void onReceivedDownstreamData(const QByteArray &data);

    /*
     * Listen for an error of the downstream serial port and close it until it is reconnected.
     */
    void onDownstreamErrorHappened(sercom::SerialPort::SerialPortStatusType serialPortStatusType);

    /*
     * Maintain the device application, the serial port and the heartbeat log.
     * Stop the event loop when a stop signal arrived.
     */
    void maintain();

private:
    /*
     * Clean up on close.
     */
    void cleanUp();

    /*
     * Attempt to reconnect the serial port and the downstream serial port when they are closed.
     */
    void attemptReconnect();

    /*
     * Attempt to reconnect one serial port when it is closed and name it in the log lines.
     */
    void reconnectSerialPort(SerialPort::SerialPort &selectedSerialPort,
                             const std::string &portName,
                             std::string_view portText);

    /*
     * Send a heartbeat log report response.
     */
    void sendHeartbeatLogReportResponse();

    /*
     * Get the serial port name and the baud rate as text.
     */
    [[nodiscard]] std::string getSerialPortText() const;

private:
    const CommandLine commandLine;                          // command line arguments
    SerialPort::SerialPort serialPort;                      // serial port to the host
    SerialPort::SerialPort downstreamSerialPort;            // serial port to the next node
    const HostUtils::SteadyClock clock;                     // steady clock of the host
    SimulatedPlatform::SimulatedPlatform simulatedPlatform; // simulated device hardware
    FrameTracer::FrameTracer frameTracer;                   // tracer of the serial port bytes
    TraceTransport::TraceTransport traceTransport;          // serial port tracing the sent bytes
    DeviceApplication::DeviceApplication deviceApplication; // device side of the protocol
    QTimer maintainTimer;                                   // timer of the maintenance period
    std::chrono::milliseconds lastReconnectTimestamp;       // time of the last reconnect attempt
    std::chrono::milliseconds lastHeartbeatTimestamp;       // time of the last heartbeat log
    uint64_t outgoingHeartbeatCounter;                      // heartbeat log report responses
};

} // namespace sercom::DeviceSimulator

#endif // SERCOM_DEVICE_SIMULATOR_H
