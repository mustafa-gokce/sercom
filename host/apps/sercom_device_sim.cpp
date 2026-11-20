#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "../../app/include/Messages.h"

#include "../include/DeviceSimulator.h"
#include "../include/HostSettings.h"
#include "../include/HostUtils.h"

/*
 * Option type.
 */
enum class OptionType : uint8_t {
    UNKNOWN = 0,    // unknown option
    BAUD = 1,       // baud rate of the serial port
    NAME = 2,       // device name of the simulated device
    LOG_PERIOD = 3, // heartbeat log period
    HELP = 4,       // request of the usage text
};

/*
 * Option type to string.
 */
std::string_view fromOptionType(OptionType optionType);

/*
 * String to option type.
 */
OptionType toOptionType(std::string_view optionType);

/*
 * Print the usage text.
 */
void printUsage(std::string_view programName);

/*
 * Parse the command line, warn the user and get false on any error.
 */
[[nodiscard]] bool parseCommandLine(int argc,
                                    char *argv[],
                                    sercom::DeviceSimulator::CommandLine &commandLine);

/*
 * Parse one option with its value, warn the user and get false on any error.
 */
[[nodiscard]] bool parseOption(const std::string &option,
                               const std::string &value,
                               sercom::DeviceSimulator::CommandLine &commandLine);

/*
 * Parse the device name argument, warn the user and get false when it is too long.
 */
[[nodiscard]] bool parseDeviceName(const std::string &deviceName,
                                   sercom::DeviceSimulator::CommandLine &commandLine);

/*
 * Main entry point of the executable.
 */
int main(int argc, char *argv[]);

int main(int argc, char *argv[])
{
    using namespace sercom;

    // print the usage text on request
    if (argc == 2 && toOptionType(argv[1]) == OptionType::HELP) {
        printUsage(argv[0]);
        return 0;
    }

    // command line must be valid
    DeviceSimulator::CommandLine commandLine;
    if (!parseCommandLine(argc, argv, commandLine)) {
        printUsage(argv[0]);
        return HostSettings::INVALID_COMMAND_LINE_EXIT_CODE;
    }

    // install the stop handlers
    HostUtils::installStopHandlers();

    // run the device simulator
    DeviceSimulator::DeviceSimulator deviceSimulator(commandLine);
    return deviceSimulator.run();
}

std::string_view fromOptionType(const OptionType optionType)
{
    switch (optionType) {
    case OptionType::BAUD: {
        return {"--baud"};
    }
    case OptionType::NAME: {
        return {"--name"};
    }
    case OptionType::LOG_PERIOD: {
        return {"--log-period"};
    }
    case OptionType::HELP: {
        return {"--help"};
    }
    case OptionType::UNKNOWN:
    default: {
        return {};
    }
    }
}

OptionType toOptionType(const std::string_view optionType)
{
    if (optionType == std::string_view("--baud")) {
        return OptionType::BAUD;
    }
    if (optionType == std::string_view("--name")) {
        return OptionType::NAME;
    }
    if (optionType == std::string_view("--log-period")) {
        return OptionType::LOG_PERIOD;
    }
    if (optionType == std::string_view("--help")) {
        return OptionType::HELP;
    }
    return OptionType::UNKNOWN;
}

void printUsage(const std::string_view programName)
{
    using namespace sercom;

    // print the usage text
    const std::string usage
        = std::string("Usage: ") + std::string(programName)
          + " <port> [--baud N] [--name NAME] [--log-period MILLISECONDS]\n"
            "Simulates the demo device on a serial port or pseudo terminal.\n"
            "Options:\n"
            "  --baud N                             baud rate of the serial port\n"
            "  --name NAME                          device name of the simulated device\n"
            "  --log-period MILLISECONDS            heartbeat log period, none by default";
    HostUtils::printLine(usage);
}

bool parseCommandLine(const int argc,
                      char *argv[],
                      sercom::DeviceSimulator::CommandLine &commandLine)
{
    using namespace sercom;

    // serial port argument must be given
    const std::vector<std::string> arguments(argv + 1, argv + argc);
    if (arguments.empty()) {
        HostUtils::logWarning("Missing serial port argument");
        return false;
    }

    // get the serial port and the default device name
    commandLine.portName = arguments.at(0);
    commandLine.deviceName = std::string(HostSettings::DEFAULT_DEVICE_NAME);

    // iterate over the options
    size_t index = 1;
    while (index < arguments.size()) {
        // option must have a value
        const std::string &option = arguments.at(index);
        if (index + 1 >= arguments.size()) {
            // warn the user
            const std::string warningMessage = "Missing value of the option " + option;
            HostUtils::logWarning(warningMessage);

            // do not proceed further
            return false;
        }

        // option must be valid
        if (!parseOption(option, arguments.at(index + 1), commandLine)) {
            return false;
        }

        // continue with the next option
        index += 2;
    }

    // command line parsed successfully
    return true;
}

bool parseOption(const std::string &option,
                 const std::string &value,
                 sercom::DeviceSimulator::CommandLine &commandLine)
{
    using namespace sercom;

    // parse the option by its type
    switch (toOptionType(option)) {
    // parse the baud rate option
    case OptionType::BAUD: {
        return HostUtils::parseBaudRateArgument(value, commandLine.baudRate);
    }

    // parse the device name option
    case OptionType::NAME: {
        return parseDeviceName(value, commandLine);
    }

    // parse the log period option
    case OptionType::LOG_PERIOD: {
        return HostUtils::parseDurationArgument(value,
                                                "log period",
                                                HostSettings::PERIOD_ARGUMENT_UNIT_MSEC,
                                                HostSettings::MAXIMUM_LOG_PERIOD_MSEC,
                                                commandLine.logPeriod);
    }

    // reject an unknown option
    case OptionType::UNKNOWN:
    default: {
        // warn the user
        const std::string warningMessage = "Unknown option " + option;
        HostUtils::logWarning(warningMessage);

        // do not proceed further
        return false;
    }
    }
}

bool parseDeviceName(const std::string &deviceName,
                     sercom::DeviceSimulator::CommandLine &commandLine)
{
    using namespace sercom;

    // device name must fit the device info response
    if (deviceName.size() > Messages::MAXIMUM_DEVICE_NAME_LENGTH) {
        // warn the user
        const std::string warningMessage = "Invalid device name argument " + deviceName
                                           + ", expected at most "
                                           + std::to_string(Messages::MAXIMUM_DEVICE_NAME_LENGTH)
                                           + " characters";
        HostUtils::logWarning(warningMessage);

        // do not proceed further
        return false;
    }

    // set the device name
    commandLine.deviceName = deviceName;

    // device name parsed successfully
    return true;
}
