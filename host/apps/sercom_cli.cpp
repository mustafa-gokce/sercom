#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

#include <QCoreApplication>

#include "../../app/include/AppSettings.h"
#include "../../app/include/Messages.h"

#include "../include/CliApplication.h"
#include "../include/HostSettings.h"
#include "../include/HostUtils.h"

/*
 * Option type.
 */
enum class OptionType : uint8_t {
    UNKNOWN = 0, // unknown option
    BAUD = 1,    // baud rate of the serial port
    HELP = 2,    // request of the usage text
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
 * LED state type.
 */
enum class LedStateType : uint8_t {
    UNKNOWN = 0, // unknown LED state
    ON = 1,      // LED is switched on
    OFF = 2,     // LED is switched off
};

/*
 * LED state type to string.
 */
std::string_view fromLedStateType(LedStateType ledStateType);

/*
 * String to LED state type.
 */
LedStateType toLedStateType(std::string_view ledStateType);

/*
 * Print the usage text.
 */
void printUsage(std::string_view programName);

/*
 * Parse the command line, warn the user and get false on any error.
 */
[[nodiscard]] bool parseCommandLine(int argc,
                                    char *argv[],
                                    sercom::CliApplication::CommandLine &commandLine);

/*
 * Check the number of arguments of a command, warn the user and get false when it is out of range.
 */
[[nodiscard]] bool checkParameterCount(const std::vector<std::string> &parameters,
                                       size_t minimumCount,
                                       size_t maximumCount,
                                       sercom::CliApplication::CommandType commandType);

/*
 * Parse the arguments of the ping command.
 */
[[nodiscard]] bool parsePingArguments(const std::vector<std::string> &parameters,
                                      sercom::CliApplication::CommandLine &commandLine);

/*
 * Parse the arguments of the info command.
 */
[[nodiscard]] bool parseInfoArguments(const std::vector<std::string> &parameters);

/*
 * Parse the arguments of the LED command.
 */
[[nodiscard]] bool parseLedArguments(const std::vector<std::string> &parameters,
                                     sercom::CliApplication::CommandLine &commandLine);

/*
 * Parse the arguments of the telemetry command.
 */
[[nodiscard]] bool parseTelemetryArguments(const std::vector<std::string> &parameters,
                                           sercom::CliApplication::CommandLine &commandLine);

/*
 * Parse the arguments of the monitor command.
 */
[[nodiscard]] bool parseMonitorArguments(const std::vector<std::string> &parameters,
                                         sercom::CliApplication::CommandLine &commandLine);

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
    CliApplication::CommandLine commandLine;
    if (!parseCommandLine(argc, argv, commandLine)) {
        printUsage(argv[0]);
        return HostSettings::INVALID_COMMAND_LINE_EXIT_CODE;
    }

    // install the stop handlers
    HostUtils::installStopHandlers();

    // initialize the Qt application of the event loop
    const QCoreApplication application(argc, argv);

    // run the command
    CliApplication::CliApplication cliApplication(commandLine);
    return cliApplication.run();
}

std::string_view fromOptionType(const OptionType optionType)
{
    switch (optionType) {
    case OptionType::BAUD: {
        return {"--baud"};
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
    if (optionType == std::string_view("--help")) {
        return OptionType::HELP;
    }
    return OptionType::UNKNOWN;
}

std::string_view fromLedStateType(const LedStateType ledStateType)
{
    switch (ledStateType) {
    case LedStateType::ON: {
        return {"on"};
    }
    case LedStateType::OFF: {
        return {"off"};
    }
    case LedStateType::UNKNOWN:
    default: {
        return {};
    }
    }
}

LedStateType toLedStateType(const std::string_view ledStateType)
{
    if (ledStateType == std::string_view("on")) {
        return LedStateType::ON;
    }
    if (ledStateType == std::string_view("off")) {
        return LedStateType::OFF;
    }
    return LedStateType::UNKNOWN;
}

void printUsage(const std::string_view programName)
{
    using namespace sercom;

    // print the usage text
    const std::string usage
        = std::string("Usage: ") + std::string(programName)
          + " <port> [--baud N] <command> [arguments]\n"
            "Port:\n"
            "  <port>                               device path such as /dev/ttyUSB0 on Linux,\n"
            "                                       COM port such as COM3 on Windows\n"
            "Options:\n"
            "  --baud N                             baud rate of the serial port\n"
            "Commands:\n"
            "  ping [count]                         measure round trip time\n"
            "  info                                 print device information\n"
            "  led on|off                           switch the device LED\n"
            "  telemetry <milliseconds> [seconds]   set the period and print telemetry\n"
            "  monitor [seconds]                    print telemetry and device logs";
    HostUtils::printLine(usage);
}

bool parseCommandLine(const int argc, char *argv[], sercom::CliApplication::CommandLine &commandLine)
{
    using namespace sercom;

    // serial port argument must be given
    const std::vector<std::string> arguments(argv + 1, argv + argc);
    if (arguments.empty()) {
        HostUtils::logWarning("Missing serial port argument");
        return false;
    }

    // set the serial port name
    commandLine.portName = arguments.at(0);

    // baud rate must be valid when given
    static constexpr size_t baudRateIndex = 1;
    const bool baudRateGiven = baudRateIndex + 1 < arguments.size()
                               && toOptionType(arguments.at(baudRateIndex)) == OptionType::BAUD;
    if (baudRateGiven
        && !HostUtils::parseBaudRateArgument(arguments.at(baudRateIndex + 1),
                                             commandLine.baudRate)) {
        return false;
    }

    // command argument must be given
    const size_t commandIndex = baudRateGiven ? baudRateIndex + 2 : baudRateIndex;
    if (commandIndex >= arguments.size()) {
        HostUtils::logWarning("Missing command argument");
        return false;
    }

    // get the command and its arguments
    commandLine.commandType = CliApplication::toCommandType(arguments.at(commandIndex));
    const std::vector<std::string> parameters(std::next(arguments.cbegin(),
                                                        static_cast<ptrdiff_t>(commandIndex) + 1),
                                              arguments.cend());

    // parse the arguments of the command
    switch (commandLine.commandType) {
    // parse the arguments of the ping command
    case CliApplication::CommandType::PING: {
        return parsePingArguments(parameters, commandLine);
    }

    // parse the arguments of the info command
    case CliApplication::CommandType::INFO: {
        return parseInfoArguments(parameters);
    }

    // parse the arguments of the LED command
    case CliApplication::CommandType::LED: {
        return parseLedArguments(parameters, commandLine);
    }

    // parse the arguments of the telemetry command
    case CliApplication::CommandType::TELEMETRY: {
        return parseTelemetryArguments(parameters, commandLine);
    }

    // parse the arguments of the monitor command
    case CliApplication::CommandType::MONITOR: {
        return parseMonitorArguments(parameters, commandLine);
    }

    // reject an unknown command
    case CliApplication::CommandType::UNKNOWN:
    default: {
        // warn the user
        const std::string warningMessage = "Unknown command " + arguments.at(commandIndex);
        HostUtils::logWarning(warningMessage);

        // do not proceed further
        return false;
    }
    }
}

bool checkParameterCount(const std::vector<std::string> &parameters,
                         const size_t minimumCount,
                         const size_t maximumCount,
                         const sercom::CliApplication::CommandType commandType)
{
    using namespace sercom;

    // number of arguments must be within the limits
    if (parameters.size() < minimumCount || parameters.size() > maximumCount) {
        // warn the user
        const std::string warningMessage = "Invalid number of arguments for the "
                                           + std::string(
                                               CliApplication::fromCommandType(commandType))
                                           + " command, expected " + std::to_string(minimumCount)
                                           + " to " + std::to_string(maximumCount);
        HostUtils::logWarning(warningMessage);

        // do not proceed further
        return false;
    }

    // passed basic checks
    return true;
}

bool parsePingArguments(const std::vector<std::string> &parameters,
                        sercom::CliApplication::CommandLine &commandLine)
{
    using namespace sercom;

    // ping command must have at most one argument
    if (!checkParameterCount(parameters, 0, 1, CliApplication::CommandType::PING)) {
        return false;
    }

    // count must be valid when given
    if (!parameters.empty()
        && !HostUtils::parseArgument(parameters.at(0),
                                     "ping count",
                                     1,
                                     HostSettings::MAXIMUM_PING_COUNT,
                                     commandLine.requestCount)) {
        return false;
    }

    // arguments parsed successfully
    return true;
}

bool parseInfoArguments(const std::vector<std::string> &parameters)
{
    using namespace sercom;

    // info command must have no argument
    return checkParameterCount(parameters, 0, 0, CliApplication::CommandType::INFO);
}

bool parseLedArguments(const std::vector<std::string> &parameters,
                       sercom::CliApplication::CommandLine &commandLine)
{
    using namespace sercom;

    // LED command must have one argument
    if (!checkParameterCount(parameters, 1, 1, CliApplication::CommandType::LED)) {
        return false;
    }

    // LED state must be on or off
    const LedStateType ledStateType = toLedStateType(parameters.at(0));
    if (ledStateType == LedStateType::UNKNOWN) {
        // warn the user
        const std::string warningMessage = "Invalid LED state argument " + parameters.at(0)
                                           + ", expected "
                                           + std::string(fromLedStateType(LedStateType::ON))
                                           + " or "
                                           + std::string(fromLedStateType(LedStateType::OFF));
        HostUtils::logWarning(warningMessage);

        // do not proceed further
        return false;
    }

    // set the LED state
    commandLine.ledOn = ledStateType == LedStateType::ON;

    // arguments parsed successfully
    return true;
}

bool parseTelemetryArguments(const std::vector<std::string> &parameters,
                             sercom::CliApplication::CommandLine &commandLine)
{
    using namespace sercom;
    using namespace std::chrono_literals;

    // telemetry command must have one or two arguments
    if (!checkParameterCount(parameters, 1, 2, CliApplication::CommandType::TELEMETRY)) {
        return false;
    }

    // telemetry period must be valid
    if (!HostUtils::parseDurationArgument(parameters.at(0),
                                          "telemetry period",
                                          HostSettings::PERIOD_ARGUMENT_UNIT_MSEC,
                                          Messages::MAXIMUM_TELEMETRY_PERIOD_MSEC,
                                          commandLine.telemetryPeriod)) {
        return false;
    }

    // telemetry period must be zero or at least the minimum
    if (commandLine.telemetryPeriod != 0ms
        && commandLine.telemetryPeriod < AppSettings::MINIMUM_TELEMETRY_PERIOD_MSEC) {
        // warn the user
        const std::string warningMessage = "Invalid telemetry period argument " + parameters.at(0)
                                           + ", expected 0 or a number from "
                                           + std::to_string(
                                               AppSettings::MINIMUM_TELEMETRY_PERIOD_MSEC
                                               / HostSettings::PERIOD_ARGUMENT_UNIT_MSEC)
                                           + " to "
                                           + std::to_string(
                                               Messages::MAXIMUM_TELEMETRY_PERIOD_MSEC
                                               / HostSettings::PERIOD_ARGUMENT_UNIT_MSEC);
        HostUtils::logWarning(warningMessage);

        // do not proceed further
        return false;
    }

    // duration must be valid when given
    if (parameters.size() > 1
        && !HostUtils::parseDurationArgument(parameters.at(1),
                                             "duration",
                                             HostSettings::DURATION_ARGUMENT_UNIT_MSEC,
                                             HostSettings::MAXIMUM_COMMAND_DURATION_MSEC,
                                             commandLine.duration)) {
        return false;
    }

    // arguments parsed successfully
    return true;
}

bool parseMonitorArguments(const std::vector<std::string> &parameters,
                           sercom::CliApplication::CommandLine &commandLine)
{
    using namespace sercom;

    // monitor command must have at most one argument
    if (!checkParameterCount(parameters, 0, 1, CliApplication::CommandType::MONITOR)) {
        return false;
    }

    // duration must be valid when given
    if (!parameters.empty()
        && !HostUtils::parseDurationArgument(parameters.at(0),
                                             "duration",
                                             HostSettings::DURATION_ARGUMENT_UNIT_MSEC,
                                             HostSettings::MAXIMUM_COMMAND_DURATION_MSEC,
                                             commandLine.duration)) {
        return false;
    }

    // arguments parsed successfully
    return true;
}
