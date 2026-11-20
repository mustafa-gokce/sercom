#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

#if defined(__linux__)
#include <unistd.h>
#endif

#include "../host/include/HostSettings.h"
#include "../host/include/HostUtils.h"
#include "../host/include/SerialPort.h"

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

    // command line must be valid
    std::string portName;
    if (!parseCommandLine(argc, argv, portName)) {
        printUsage(argv[0]);
        return HostSettings::INVALID_COMMAND_LINE_EXIT_CODE;
    }

    // serial port must open
    SerialPort::SerialPort serialPort;
    if (!serialPort.open(portName, HostSettings::DEFAULT_BAUD_RATE)) {
        // warn the user
        const std::string warningMessage = "Can not open the serial port " + portName;
        HostUtils::logWarning(warningMessage);

        // do not proceed further
        return 1;
    }

    /*
     * Message must be sent as text without frame boundaries and error detection.
     * The frames of the rest of the repository add both.
     */
    static constexpr std::string_view message = "a=1;b=2;c=3;\n";
    const ssize_t writeResult = ::write(serialPort.getFileDescriptor(),
                                        message.data(),
                                        message.size());
    if (writeResult != static_cast<ssize_t>(message.size())) {
        // warn the user
        const std::string warningMessage = "Can not send the message on " + portName;
        HostUtils::logWarning(warningMessage);

        // do not proceed further
        return 1;
    }

    // notify the user
    const std::string debugMessage = "Sent " + std::to_string(writeResult) + " bytes on "
                                     + portName;
    HostUtils::logDebug(debugMessage);

    // read of the answer must succeed
    std::vector<uint8_t> readBuffer(HostSettings::READ_BUFFER_SIZE);
    const ssize_t readResult = ::read(serialPort.getFileDescriptor(),
                                      readBuffer.data(),
                                      readBuffer.size());
    if (readResult < 0) {
        // warn the user
        const std::string warningMessage = "Can not read the serial port " + portName;
        HostUtils::logWarning(warningMessage);

        // do not proceed further
        return 1;
    }

    // iterate over the received bytes
    std::for_each(readBuffer.cbegin(),
                  std::next(readBuffer.cbegin(), readResult),
                  printReceivedByte);

    // close the serial port
    serialPort.close();
    HostUtils::printLine("Serial device closed.");

    // example finished successfully
    return 0;
}

void printUsage(const std::string_view programName)
{
    using namespace sercom;

    // print the usage text
    const std::string usage = std::string("Usage: ") + std::string(programName)
                              + " <serial-port>\nSends a text to the serial port at "
                              + std::to_string(HostSettings::DEFAULT_BAUD_RATE)
                              + " 8N1 and prints the answer.";
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
