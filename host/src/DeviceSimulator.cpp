#include "../include/DeviceSimulator.h"

#include <algorithm>
#include <vector>

#include <QCoreApplication>

#include "../../app/include/AppSettings.h"

namespace sercom::DeviceSimulator {

DeviceSimulator::DeviceSimulator(const CommandLine &commandLine)
    : commandLine(commandLine)
    , simulatedPlatform(this->clock, this->commandLine.deviceName)
    , deviceApplication(this->serialPort, this->clock, this->simulatedPlatform)
    , maintainTimer()
    , lastReconnectTimestamp(0)
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

    // notify the user
    const std::string startedDebugMessage = "Started the DeviceSimulator on "
                                            + this->getSerialPortText() + " for the device "
                                            + this->commandLine.deviceName + " version "
                                            + std::string(AppSettings::AppVersionString);
    HostUtils::logDebug(startedDebugMessage);

    // answer the host on the event loop until a stop signal arrives
    this->maintainTimer.start(HostSettings::MAINTAIN_PERIOD_MSEC);
    [[maybe_unused]] const int eventLoopExitCode = QCoreApplication::exec();
    this->maintainTimer.stop();

    // device simulator stopped successfully
    return 0;
}

void DeviceSimulator::onReceivedData(const QByteArray &data)
{
    // hand the received data to the device application
    const uint8_t *receivedData = reinterpret_cast<const uint8_t *>(data.constData());
    const size_t size = static_cast<size_t>(data.size());
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

void DeviceSimulator::maintain()
{
    // stop the event loop when a stop signal arrived
    if (!HostUtils::getRunning()) {
        QCoreApplication::quit();
        return;
    }

    // maintain the device application
    this->deviceApplication.maintain();

    // attempt to reconnect the serial port periodically
    const std::chrono::milliseconds currentTime = this->clock.getTime();
    if (currentTime - this->lastReconnectTimestamp
        >= HostSettings::SERIAL_PORT_RECONNECT_PERIOD_MSEC) {
        this->lastReconnectTimestamp = currentTime;
        this->attemptReconnect();
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

    // close the serial port
    this->serialPort.close();

    // notify the user
    const std::string closedDebugMessage = "Closed the DeviceSimulator on "
                                           + this->getSerialPortText();
    HostUtils::logDebug(closedDebugMessage);
}

void DeviceSimulator::attemptReconnect()
{
    // serial port must be closed
    if (this->serialPort.getOpened()) {
        return;
    }

    // notify the user
    HostUtils::logDebug("DeviceSimulator trying to connect to the serial port");

    // serial port must open
    const SerialPort::SerialPortStatusType serialPortStatusType
        = this->serialPort.open(this->commandLine.portName, this->commandLine.baudRate);
    if (serialPortStatusType != SerialPort::SerialPortStatusType::OK) {
        // warn the user
        const std::string warningMessage = "DeviceSimulator can not connect to serial port on "
                                           + this->getSerialPortText() + " ("
                                           + std::string(SerialPort::fromSerialPortStatusType(
                                               serialPortStatusType))
                                           + ")";
        HostUtils::logWarning(warningMessage);

        // do not proceed further
        return;
    }

    // notify the user
    const std::string debugMessage = "DeviceSimulator connected to serial port from "
                                     + this->getSerialPortText();
    HostUtils::logDebug(debugMessage);
}

std::string DeviceSimulator::getSerialPortText() const
{
    // expose the serial port name and the baud rate
    return HostUtils::getSerialPortText(this->commandLine.portName, this->commandLine.baudRate);
}

} // namespace sercom::DeviceSimulator
