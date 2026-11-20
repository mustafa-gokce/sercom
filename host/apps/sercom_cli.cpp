#include <cstddef>
#include <cstdint>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

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

void printUsage(const std::string_view programName)
{
    using namespace sercom;

    // print the usage text
    const std::string usage
        = std::string("Usage: ") + std::string(programName)
          + " <port> [--baud N] <command> [arguments]\n"
            "Options:\n"
            "  --baud N                             baud rate of the serial port\n"
            "Commands:\n"
            "  ping [count]                         measure round trip time";
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
