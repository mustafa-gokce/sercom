#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include <QByteArray>
#include <QCoreApplication>
#include <QObject>
#include <QSerialPort>
#include <QString>
#include <QTimer>

#include "../host/include/HostSettings.h"
#include "../host/include/HostUtils.h"

/*
 * Print the usage text.
 */
void printUsage(std::string_view programName);

/*
 * Parse the command line, warn the user and get false on any error.
 */
[[nodiscard]] bool parseCommandLine(int argc, char *argv[], std::string &portName);

/*
 * Print one received byte as hex and as character.
 */
void printReceivedByte(uint8_t byte);

/*
 * Main entry point of the executable.
 */
int main(int argc, char *argv[]);

int main(int argc, char *argv[])
{
    using namespace sercom;
    using namespace std::chrono_literals;

    // initialize the Qt application of the event loop
    const QCoreApplication application(argc, argv);

    // command line must be valid
    std::string portName;
    if (!parseCommandLine(argc, argv, portName)) {
        printUsage(argv[0]);
        return HostSettings::INVALID_COMMAND_LINE_EXIT_CODE;
    }

    // install the stop handlers
    HostUtils::installStopHandlers();

    // serial port must open in raw 8N1 mode without flow control
    QSerialPort serialPort;
    serialPort.setPortName(QString::fromStdString(portName));
    serialPort.setBaudRate(static_cast<qint32>(HostSettings::DEFAULT_BAUD_RATE));
    serialPort.setDataBits(QSerialPort::DataBits::Data8);
    serialPort.setParity(QSerialPort::Parity::NoParity);
    serialPort.setStopBits(QSerialPort::StopBits::OneStop);
    serialPort.setFlowControl(QSerialPort::FlowControl::NoFlowControl);
    if (!serialPort.open(QIODevice::OpenModeFlag::ReadWrite)) {
        // warn the user
        const std::string warningMessage = "Can not open the serial port " + portName + " ("
                                           + serialPort.errorString().toStdString() + ")";
        HostUtils::logWarning(warningMessage);

        // do not proceed further
        return 1;
    }

    // print the listening port and how to stop
    const std::string listeningLine = "Listening on " + portName + "...";
    HostUtils::printLine(listeningLine);
    HostUtils::printLine("Press Ctrl+C to exit.");

    // print every byte that arrives
    QObject::connect(&serialPort, &QSerialPort::readyRead, [&serialPort]() {
        // iterate over the received bytes
        const QByteArray data = serialPort.readAll();
        std::for_each(data.cbegin(), data.cend(), [](const char character) {
            printReceivedByte(static_cast<uint8_t>(character));
        });
    });

    // stop with a failure when the open port fails
    QObject::connect(&serialPort,
                     &QSerialPort::errorOccurred,
                     [&serialPort, &portName](const QSerialPort::SerialPortError serialPortError) {
                         // error must have happened
                         if (serialPortError == QSerialPort::SerialPortError::NoError) {
                             return;
                         }

                         // warn the user
                         const std::string warningMessage = "Can not read the serial port "
                                                            + portName + " ("
                                                            + serialPort.errorString().toStdString()
                                                            + ")";
                         HostUtils::logWarning(warningMessage);

                         // stop the event loop with the failure
                         QCoreApplication::exit(1);
                     });

    // send a text once per second and stop when a stop signal arrives
    static constexpr std::chrono::milliseconds timerPeriod = 100ms;
    static constexpr std::chrono::milliseconds messagePeriod = 1 * 1000ms;
    static constexpr std::string_view message = "a=1;b=2;c=3;\n";
    const HostUtils::SteadyClock clock;
    std::chrono::milliseconds lastMessageTimestamp = clock.getTime();
    QTimer timer;
    QObject::connect(&timer,
                     &QTimer::timeout,
                     [&serialPort, &portName, &clock, &lastMessageTimestamp]() {
                         // stop signal must not have arrived
                         if (!HostUtils::getRunning()) {
                             QCoreApplication::quit();
                             return;
                         }

                         // message period must have passed since the last message
                         const std::chrono::milliseconds currentTime = clock.getTime();
                         if (currentTime - lastMessageTimestamp < messagePeriod) {
                             return;
                         }

                         /*
                          * Message must be sent as text without frame boundaries and error detection.
                          * The frames of the rest of the repository add both.
                          */
                         const qint64 messageSize = static_cast<qint64>(message.size());
                         if (serialPort.write(message.data(), messageSize) != messageSize) {
                             // warn the user
                             const std::string warningMessage = "Can not send the message on "
                                                                + portName;
                             HostUtils::logWarning(warningMessage);

                             // stop the event loop with the failure
                             QCoreApplication::exit(1);
                             return;
                         }

                         // remember the time of the message
                         lastMessageTimestamp = currentTime;
                     });
    timer.start(timerPeriod);

    // run the event loop until a stop signal or a failure
    const int exitCode = QCoreApplication::exec();

    // close the serial port
    serialPort.close();
    HostUtils::printLine("\nSerial device closed.");

    // expose whether the example finished successfully
    return exitCode;
}

void printUsage(const std::string_view programName)
{
    using namespace sercom;

    // print the usage text
    const std::string usage = std::string("Usage: ") + std::string(programName)
                              + " <serial-port>\nPrints every byte of the serial port at "
                              + std::to_string(HostSettings::DEFAULT_BAUD_RATE)
                              + " 8N1 and sends a text once per second.";
    HostUtils::printLine(usage);
}

bool parseCommandLine(const int argc, char *argv[], std::string &portName)
{
    using namespace sercom;

    // serial port argument must be given
    const std::vector<std::string> arguments(argv + 1, argv + argc);
    if (arguments.empty()) {
        HostUtils::logWarning("Missing serial port argument");
        return false;
    }

    // serial port argument must be the only argument
    if (arguments.size() > 1) {
        HostUtils::logWarning("Too many arguments, expected the serial port argument only");
        return false;
    }

    // set the serial port name
    portName = arguments.at(0);

    // command line parsed successfully
    return true;
}

void printReceivedByte(const uint8_t byte)
{
    using namespace sercom;

    // get the character of a printable byte
    const bool printable = byte >= ' ' && byte <= '~';
    const char character = printable ? static_cast<char>(byte) : '.';

    // print the byte as hex and as character
    static constexpr size_t byteDigitCount = 2;
    const std::string line = "Received: 0x" + HostUtils::getHexText(byte, byteDigitCount) + " '"
                             + std::string(1, character) + "'";
    HostUtils::printLine(line);
}
