#include "../include/DeviceSimulator.h"

#include <algorithm>
#include <vector>

#include <QCoreApplication>

#include "../../app/include/AppSettings.h"
#include "../../app/include/Messages.h"

namespace sercom::DeviceSimulator {

using namespace std::chrono_literals;

Router::RouterConfiguration getRouterConfiguration(const CommandLine &commandLine)
{
    // take the addresses of the command line
    Router::RouterConfiguration routerConfiguration;
    routerConfiguration.nodeAddress = commandLine.nodeAddress;
    routerConfiguration.proxyAddress = commandLine.proxyAddress;

    // expose the router configuration
    return routerConfiguration;
}

DeviceSimulator::DeviceSimulator(const CommandLine &commandLine)
    : commandLine(commandLine)
    , simulatedPlatform(this->clock, this->commandLine.deviceName)
    , frameTracer(*this, this->clock, FrameTracer::TraceDirectionType::DEVICE_TO_HOST)
    , traceTransport(this->serialPort, this->frameTracer)
    , deviceApplication(this->traceTransport,
                        this->commandLine.downstreamPortName.empty() ? nullptr
                                                                     : &this->downstreamSerialPort,
                        this->clock,
                        this->simulatedPlatform,
                        Link::LinkConfiguration(),
                        getRouterConfiguration(this->commandLine))
    , maintainTimer()
    , lastReconnectTimestamp(0)
    , lastHeartbeatTimestamp(0)
    , outgoingHeartbeatCounter(0)
{
    // connect serial port signals and slots
    QObject::connect(&this->serialPort,
                     &SerialPort::SerialPort::receivedData,
                     this,
                     &DeviceSimulator::onReceivedData);
    QObject::connect(&this->serialPort,
                     &SerialPort::SerialPort::errorHappened,
                     this,
                     &DeviceSimulator::onErrorHappened);
    QObject::connect(&this->downstreamSerialPort,
                     &SerialPort::SerialPort::receivedData,
                     this,
                     &DeviceSimulator::onReceivedDownstreamData);
    QObject::connect(&this->downstreamSerialPort,
                     &SerialPort::SerialPort::errorHappened,
                     this,
                     &DeviceSimulator::onDownstreamErrorHappened);

    // maintain the device simulator once per maintenance period
    this->maintainTimer.setSingleShot(false);
    this->maintainTimer.setTimerType(Qt::TimerType::PreciseTimer);
    QObject::connect(&this->maintainTimer, &QTimer::timeout, this, &DeviceSimulator::maintain);
}

DeviceSimulator::~DeviceSimulator()
{
    // invoke clean up procedure
    this->cleanUp();
}

int DeviceSimulator::run()
{
    // notify the user
    const std::string startingDebugMessage = "Starting the DeviceSimulator on "
                                             + this->getSerialPortText();
    HostUtils::logDebug(startingDebugMessage);

    // connect to the serial port
    this->attemptReconnect();

    // name the node and its downstream serial port in the start line of a router
    std::string nodeAsText;
    if (this->commandLine.nodeAddress != Definitions::UNASSIGNED_ADDRESS) {
        nodeAsText = " as node " + std::to_string(this->commandLine.nodeAddress);
    }
    if (!this->commandLine.downstreamPortName.empty()) {
        nodeAsText += " with the downstream serial port on " + this->commandLine.downstreamPortName;
    }
    if (this->commandLine.proxyAddress != Definitions::UNASSIGNED_ADDRESS) {
        nodeAsText += " for node " + std::to_string(this->commandLine.proxyAddress);
    }

    // notify the user
    const std::string startedDebugMessage = "Started the DeviceSimulator on "
                                            + this->getSerialPortText() + " for the device "
                                            + this->commandLine.deviceName + " version "
                                            + std::string(AppSettings::AppVersionString)
                                            + nodeAsText;
    HostUtils::logDebug(startedDebugMessage);

    // answer the host on the event loop until a stop signal arrives
    this->maintainTimer.start(HostSettings::MAINTAIN_PERIOD_MSEC);
    [[maybe_unused]] const int eventLoopExitCode = QCoreApplication::exec();
    this->maintainTimer.stop();

    // device simulator stopped successfully
    return 0;
}

void DeviceSimulator::onReceivedTracedFrame(
    const FrameTracer::TraceDirectionType traceDirectionType,
    const Frame::Frame &frame,
    [[maybe_unused]] const std::chrono::milliseconds currentTime)
{
    // frames must be traced
    if (!this->commandLine.traced) {
        return;
    }

    // print the traced frame at the time of the log lines
    const std::chrono::milliseconds elapsedTime = HostUtils::getElapsedTime();
    HostUtils::printLine(HostUtils::getTracedFrameLine(traceDirectionType, frame, elapsedTime));
}

void DeviceSimulator::onReceivedUnframedData(
    const FrameTracer::TraceDirectionType traceDirectionType,
    const uint8_t *data,
    const size_t size,
    [[maybe_unused]] const std::chrono::milliseconds currentTime)
{
    // frames must be traced
    if (!this->commandLine.traced) {
        return;
    }

    // print the traced bytes outside a frame at the time of the log lines
    const std::chrono::milliseconds elapsedTime = HostUtils::getElapsedTime();
    HostUtils::printLine(
        HostUtils::getUnframedDataLine(traceDirectionType, data, size, elapsedTime));
}

void DeviceSimulator::onReceivedData(const QByteArray &data)
{
    // hand the received data to the frame tracer and the device application
    const uint8_t *receivedData = reinterpret_cast<const uint8_t *>(data.constData());
    const size_t size = static_cast<size_t>(data.size());
    this->frameTracer.onReceivedData(receivedData, size);
    this->deviceApplication.onReceivedData(receivedData, size);
}

void DeviceSimulator::onErrorHappened(const SerialPort::SerialPortStatusType serialPortStatusType)
{
    // warn the user
    const std::string warningMessage = "DeviceSimulator disconnected from serial port on "
                                       + this->getSerialPortText() + " ("
                                       + std::string(SerialPort::fromSerialPortStatusType(
                                           serialPortStatusType))
                                       + ")";
    HostUtils::logWarning(warningMessage);

    // close the serial port until the next reconnect attempt
    this->serialPort.close();
}

void DeviceSimulator::onReceivedDownstreamData(const QByteArray &data)
{
    // hand the received data to the secondary port of the device application
    const uint8_t *receivedData = reinterpret_cast<const uint8_t *>(data.constData());
    const size_t size = static_cast<size_t>(data.size());
    this->deviceApplication.onReceivedSecondaryData(receivedData, size);
}

void DeviceSimulator::onDownstreamErrorHappened(
    const SerialPort::SerialPortStatusType serialPortStatusType)
{
    // warn the user
    const std::string warningMessage
        = "DeviceSimulator disconnected from downstream serial port on "
          + HostUtils::getSerialPortText(this->commandLine.downstreamPortName,
                                         this->commandLine.baudRate)
          + " (" + std::string(SerialPort::fromSerialPortStatusType(serialPortStatusType)) + ")";
    HostUtils::logWarning(warningMessage);

    // close the downstream serial port until the next reconnect attempt
    this->downstreamSerialPort.close();
}

void DeviceSimulator::maintain()
{
    // stop the event loop when a stop signal arrived
    if (!HostUtils::getRunning()) {
        QCoreApplication::quit();
        return;
    }

    // maintain the device application and the frame tracer
    this->deviceApplication.maintain();
    this->frameTracer.maintain();

    // attempt to reconnect the serial port periodically
    const std::chrono::milliseconds currentTime = this->clock.getTime();
    if (currentTime - this->lastReconnectTimestamp
        >= HostSettings::SERIAL_PORT_RECONNECT_PERIOD_MSEC) {
        this->lastReconnectTimestamp = currentTime;
        this->attemptReconnect();
    }

    // send the heartbeat log report response when it is due
    if (this->commandLine.logPeriod != 0ms
        && currentTime - this->lastHeartbeatTimestamp >= this->commandLine.logPeriod) {
        this->lastHeartbeatTimestamp = currentTime;
        this->sendHeartbeatLogReportResponse();
    }
}

void DeviceSimulator::cleanUp()
{
    // notify the user
    const std::string closingDebugMessage = "Closing the DeviceSimulator on "
                                            + this->getSerialPortText();
    HostUtils::logDebug(closingDebugMessage);

    // notify the user
    const std::vector<std::string> linkStatisticsLines = HostUtils::getLinkStatisticsLines(
        this->deviceApplication.getLinkStatistics());
    std::for_each(linkStatisticsLines.cbegin(),
                  linkStatisticsLines.cend(),
                  [](const std::string &linkStatisticsLine) {
                      HostUtils::logDebug(linkStatisticsLine);
                  });

    // notify the user about the downstream link of a router
    if (!this->commandLine.downstreamPortName.empty()) {
        HostUtils::logDebug("Downstream link statistics");
        const std::vector<std::string> downstreamLinkStatisticsLines
            = HostUtils::getLinkStatisticsLines(
                this->deviceApplication.getSecondaryLinkStatistics());
        std::for_each(downstreamLinkStatisticsLines.cbegin(),
                      downstreamLinkStatisticsLines.cend(),
                      [](const std::string &linkStatisticsLine) {
                          HostUtils::logDebug(linkStatisticsLine);
                      });
    }

    // close the serial ports
    this->serialPort.close();
    this->downstreamSerialPort.close();

    // notify the user
    const std::string closedDebugMessage = "Closed the DeviceSimulator on "
                                           + this->getSerialPortText();
    HostUtils::logDebug(closedDebugMessage);
}

void DeviceSimulator::attemptReconnect()
{
    // reconnect the serial port to the host
    this->reconnectSerialPort(this->serialPort, this->commandLine.portName, "serial port");

    // reconnect the serial port to the next node of a router
    if (!this->commandLine.downstreamPortName.empty()) {
        this->reconnectSerialPort(this->downstreamSerialPort,
                                  this->commandLine.downstreamPortName,
                                  "downstream serial port");
    }
}

void DeviceSimulator::reconnectSerialPort(SerialPort::SerialPort &selectedSerialPort,
                                          const std::string &portName,
                                          const std::string_view portText)
{
    // serial port must be closed
    if (selectedSerialPort.getOpened()) {
        return;
    }

    // notify the user
    const std::string tryingDebugMessage = "DeviceSimulator trying to connect to the "
                                           + std::string(portText);
    HostUtils::logDebug(tryingDebugMessage);

    // serial port must open
    const std::string serialPortText = HostUtils::getSerialPortText(portName,
                                                                    this->commandLine.baudRate);
    const SerialPort::SerialPortStatusType serialPortStatusType
        = selectedSerialPort.open(portName, this->commandLine.baudRate);
    if (serialPortStatusType != SerialPort::SerialPortStatusType::OK) {
        // warn the user
        const std::string warningMessage = "DeviceSimulator can not connect to "
                                           + std::string(portText) + " on " + serialPortText + " ("
                                           + std::string(SerialPort::fromSerialPortStatusType(
                                               serialPortStatusType))
                                           + ")";
        HostUtils::logWarning(warningMessage);

        // do not proceed further
        return;
    }

    // notify the user
    const std::string debugMessage = "DeviceSimulator connected to " + std::string(portText)
                                     + " from " + serialPortText;
    HostUtils::logDebug(debugMessage);
}

void DeviceSimulator::sendHeartbeatLogReportResponse()
{
    // keep track of heartbeats
    ++this->outgoingHeartbeatCounter;

    // send the heartbeat log report response
    const std::string text = "heartbeat " + std::to_string(this->outgoingHeartbeatCounter);
    [[maybe_unused]] const bool logReportResponseSent
        = this->deviceApplication.sendLogReportResponse(Messages::LogLevelType::DEBUG, text);
}

std::string DeviceSimulator::getSerialPortText() const
{
    // expose the serial port name and the baud rate
    return HostUtils::getSerialPortText(this->commandLine.portName, this->commandLine.baudRate);
}

} // namespace sercom::DeviceSimulator
