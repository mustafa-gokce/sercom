#ifndef SERCOM_DEVICE_SIMULATOR_H
#define SERCOM_DEVICE_SIMULATOR_H

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>

#include "../../app/include/DeviceApplication.h"
#include "../../core/include/Frame.h"
#include "../../core/include/FrameTracer.h"
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
};

class DeviceSimulator final : public FrameTracer::FrameTracerHandler
{
public:
    /*
     * Device simulator that answers the host on a serial port like the firmware does.
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
     * Run the device simulator until a stop signal arrives.
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
     * Maintain the device application, the serial port and the heartbeat log.
     */
    void maintain();

    /*
     * Attempt to reconnect the serial port when it is closed.
     */
    void attemptReconnect();

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
    const HostUtils::SteadyClock clock;                     // steady clock of the host
    SimulatedPlatform::SimulatedPlatform simulatedPlatform; // simulated device hardware
    FrameTracer::FrameTracer frameTracer;                   // tracer of the serial port bytes
    TraceTransport::TraceTransport traceTransport;          // serial port tracing the sent bytes
    DeviceApplication::DeviceApplication deviceApplication; // device side of the protocol
    std::chrono::milliseconds lastReconnectTimestamp;       // time of the last reconnect attempt
    std::chrono::milliseconds lastHeartbeatTimestamp;       // time of the last heartbeat log
    uint64_t outgoingHeartbeatCounter;                      // heartbeat log report responses
    std::array<uint8_t, HostSettings::READ_BUFFER_SIZE> readBuffer; // received serial port data
};

} // namespace sercom::DeviceSimulator

#endif // SERCOM_DEVICE_SIMULATOR_H
