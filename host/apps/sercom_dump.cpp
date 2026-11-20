#include <chrono>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

#include "../../core/include/FrameDecoder.h"

#include "../include/DumpHandler.h"
#include "../include/HostSettings.h"
#include "../include/HostUtils.h"

/*
 * Option type.
 */
enum class OptionType : uint8_t {
    UNKNOWN = 0,        // unknown option
    HELP = 1,           // request of the usage text
    STANDARD_INPUT = 2, // capture from standard input
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
[[nodiscard]] bool parseCommandLine(int argc, char *argv[], std::string &inputName);

/*
 * Read the whole capture from a file or from standard input, warn the user and get false on any error.
 */
[[nodiscard]] bool readCapture(const std::string &inputName, std::vector<uint8_t> &capture);

/*
 * Read a stream up to its end into the capture, warn the user and get false on a read error.
 */
[[nodiscard]] bool readStream(std::istream &inputStream,
                              const std::string &inputName,
                              std::vector<uint8_t> &capture);

/*
 * Main entry point of the executable.
 */
int main(int argc, char *argv[]);

int main(int argc, char *argv[])
{
    using namespace sercom;
    using namespace std::chrono_literals;

    // print the usage text on request
    if (argc == 2 && toOptionType(argv[1]) == OptionType::HELP) {
        printUsage(argv[0]);
        return 0;
    }

    // command line must be valid
    std::string inputName;
    if (!parseCommandLine(argc, argv, inputName)) {
        printUsage(argv[0]);
        return HostSettings::INVALID_COMMAND_LINE_EXIT_CODE;
    }

    // capture must be readable
    std::vector<uint8_t> capture;
    if (!readCapture(inputName, capture)) {
        return 2;
    }

    // decode the capture without the inter byte timeout
    DumpHandler::DumpHandler dumpHandler;
    FrameDecoder::FrameDecoder frameDecoder(dumpHandler, 0ms);
    frameDecoder.onReceivedData(capture.data(), capture.size(), 0ms);

    // print the summary
    const FrameDecoder::DecoderStatistics decoderStatistics = frameDecoder.getStatistics();
    const std::string summary = "SUMMARY bytes="
                                + std::to_string(decoderStatistics.incomingByteCounter) + " frames="
                                + std::to_string(decoderStatistics.incomingFrameCounter)
                                + " discarded="
                                + std::to_string(decoderStatistics.discardedByteCounter) + " crc="
                                + std::to_string(decoderStatistics.crcMismatchCounter) + " version="
                                + std::to_string(decoderStatistics.invalidVersionCounter)
                                + " length="
                                + std::to_string(decoderStatistics.payloadTooLongCounter)
                                + " trailing=" + std::to_string(frameDecoder.getBufferedSize());
    HostUtils::printLine(summary);

    // capture decoded successfully
    return 0;
}

std::string_view fromOptionType(const OptionType optionType)
{
    switch (optionType) {
    case OptionType::HELP: {
        return {"--help"};
    }
    case OptionType::STANDARD_INPUT: {
        return {"-"};
    }
    case OptionType::UNKNOWN:
    default: {
        return {};
    }
    }
}

OptionType toOptionType(const std::string_view optionType)
{
    if (optionType == std::string_view("--help")) {
        return OptionType::HELP;
    }
    if (optionType == std::string_view("-")) {
        return OptionType::STANDARD_INPUT;
    }
    return OptionType::UNKNOWN;
}

void printUsage(const std::string_view programName)
{
    using namespace sercom;

    // print the usage text
    const std::string usage = std::string("Usage: ") + std::string(programName)
                              + " <capture-file | ->\n"
                                "Decodes a raw byte capture, for example one recorded with tee.";
    HostUtils::printLine(usage);
}

bool parseCommandLine(const int argc, char *argv[], std::string &inputName)
{
    using namespace sercom;

    // capture argument must be given
    const std::vector<std::string> arguments(argv + 1, argv + argc);
    if (arguments.empty()) {
        HostUtils::logWarning("Missing capture argument");
        return false;
    }

    // capture argument must be the only argument
    if (arguments.size() > 1) {
        HostUtils::logWarning("Too many arguments, expected the capture argument only");
        return false;
    }

    // set the input name
    inputName = arguments.at(0);

    // command line parsed successfully
    return true;
}

bool readCapture(const std::string &inputName, std::vector<uint8_t> &capture)
{
    using namespace sercom;

    // read standard input for a dash
    if (toOptionType(inputName) == OptionType::STANDARD_INPUT) {
        return readStream(std::cin, inputName, capture);
    }

    // capture file must open
    std::ifstream inputFile(inputName, std::ios::binary);
    if (!inputFile.is_open()) {
        // warn the user
        const std::string warningMessage = "Can not open the capture file " + inputName;
        HostUtils::logWarning(warningMessage);

        // do not proceed further
        return false;
    }

    // read the whole capture file
    return readStream(inputFile, inputName, capture);
}

bool readStream(std::istream &inputStream,
                const std::string &inputName,
                std::vector<uint8_t> &capture)
{
    using namespace sercom;

    // read the stream in chunks up to its end
    std::vector<char> chunk(HostSettings::CAPTURE_READ_CHUNK_SIZE);
    while (inputStream.good()) {
        inputStream.read(chunk.data(), static_cast<std::streamsize>(chunk.size()));
        capture.insert(capture.end(),
                       chunk.cbegin(),
                       std::next(chunk.cbegin(), inputStream.gcount()));
    }

    // stream must be read without error
    if (inputStream.bad()) {
        // warn the user
        const std::string warningMessage = "Can not read the capture " + inputName;
        HostUtils::logWarning(warningMessage);

        // do not proceed further
        return false;
    }

    // capture read successfully
    return true;
}
