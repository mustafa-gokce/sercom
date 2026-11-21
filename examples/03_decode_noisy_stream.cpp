#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <random>
#include <string>
#include <string_view>
#include <vector>

#include "../core/include/Definitions.h"
#include "../core/include/Frame.h"
#include "../core/include/FrameDecoder.h"
#include "../host/include/HostSettings.h"
#include "../host/include/HostUtils.h"

class MockDecoderHandler final : public sercom::FrameDecoder::FrameDecoderHandler
{
public:
    /*
     * Mock decoder handler that leaves the counting of frames and errors to the decoder statistics.
     */
    MockDecoderHandler() = default;
    ~MockDecoderHandler() final {}

    // delete copy constructor and copy assignment operator
    MockDecoderHandler(const MockDecoderHandler &) = delete;
    MockDecoderHandler &operator=(const MockDecoderHandler &) = delete;

    // delete move constructor and move assignment operator
    MockDecoderHandler(MockDecoderHandler &&) = delete;
    MockDecoderHandler &operator=(MockDecoderHandler &&) = delete;

public:
    /*
     * Listen for a received frame that the decoder statistics count.
     */
    void onReceivedFrame([[maybe_unused]] const sercom::Frame::Frame &frame) final {}

    /*
     * Listen for a decode error that the decoder statistics count.
     */
    void onDecodeError(
        [[maybe_unused]] const sercom::Definitions::DecodeErrorType decodeErrorType) final
    {}
};

/*
 * Print the usage text.
 */
void printUsage(std::string_view programName);

/*
 * Parse the command line, warn the user and get false on any error.
 */
[[nodiscard]] bool parseCommandLine(int argc, char *argv[], uint32_t &seed, double &errorRate);

/*
 * Parse the error rate argument, warn the user and get false when it is no probability.
 */
[[nodiscard]] bool parseErrorRate(const std::string &argument, double &errorRate);

/*
 * Add the bytes of the stream to the noisy stream and damage each of them by chance.
 */
void addNoise(const std::vector<uint8_t> &stream,
              double errorRate,
              std::mt19937 &generator,
              std::vector<uint8_t> &noisyStream);

/*
 * Main entry point of the executable.
 */
int main(int argc, char *argv[]);

int main(int argc, char *argv[])
{
    using namespace sercom;
    using namespace std::chrono_literals;

    // command line must be valid
    static constexpr double defaultErrorRate = 0.005;
    uint32_t seed = 1;
    double errorRate = defaultErrorRate;
    if (!parseCommandLine(argc, argv, seed, errorRate)) {
        printUsage(argv[0]);
        return HostSettings::INVALID_COMMAND_LINE_EXIT_CODE;
    }

    // build the clean stream of numbered frames
    static constexpr uint16_t frameCount = 1000;
    static constexpr uint16_t messageType = 0x0006;
    static constexpr size_t payloadLength = 32;
    std::vector<uint8_t> stream;
    for (uint16_t messageCounter = 0; messageCounter < frameCount; ++messageCounter) {
        // fill the payload with a pattern of the message counter
        std::vector<uint8_t> payload(payloadLength);
        for (size_t index = 0; index < payload.size(); ++index) {
            payload[index] = static_cast<uint8_t>((messageCounter + index) & 0xFF);
        }

        // add the frame to the stream
        const Frame::Frame frame(0, messageCounter, messageType, payload.data(), payload.size());
        stream.insert(stream.end(), frame.getData(), frame.getData() + frame.getSize());
    }

    // damage the stream with the seeded random generator
    std::mt19937 generator(seed);
    std::vector<uint8_t> noisyStream;
    addNoise(stream, errorRate, generator, noisyStream);

    // decode the noisy stream without the inter byte timeout
    MockDecoderHandler mockDecoderHandler;
    FrameDecoder::FrameDecoder frameDecoder(mockDecoderHandler, 0ms);
    frameDecoder.onReceivedData(noisyStream.data(), noisyStream.size(), 0ms);

    // print the seed and the error rate
    static constexpr int errorRateDecimalCount = 4;
    const std::string seedLine = "seed " + std::to_string(seed) + ", error rate "
                                 + HostUtils::getDecimalText(errorRate, errorRateDecimalCount);
    HostUtils::printLine(seedLine);

    // print the decoded share of the frames
    static constexpr int percentageDecimalCount = 1;
    const FrameDecoder::DecoderStatistics decoderStatistics = frameDecoder.getStatistics();
    const double decodedPercentage = 100.0
                                     * static_cast<double>(decoderStatistics.incomingFrameCounter)
                                     / static_cast<double>(frameCount);
    const std::string frameLine = "frames sent " + std::to_string(frameCount) + ", frames decoded "
                                  + std::to_string(decoderStatistics.incomingFrameCounter) + " ("
                                  + HostUtils::getDecimalText(decodedPercentage,
                                                              percentageDecimalCount)
                                  + "%)";
    HostUtils::printLine(frameLine);

    // print the byte counts and the decode errors
    const std::string byteLine = "bytes " + std::to_string(stream.size()) + " -> "
                                 + std::to_string(noisyStream.size()) + ", discarded "
                                 + std::to_string(decoderStatistics.discardedByteCounter)
                                 + ", CRC errors "
                                 + std::to_string(decoderStatistics.crcMismatchCounter)
                                 + ", length errors "
                                 + std::to_string(decoderStatistics.payloadTooLongCounter)
                                 + ", version errors "
                                 + std::to_string(decoderStatistics.invalidVersionCounter);
    HostUtils::printLine(byteLine);

    // example finished successfully
    return 0;
}

void printUsage(const std::string_view programName)
{
    using namespace sercom;

    // print the usage text
    const std::string usage = std::string("Usage: ") + std::string(programName)
                              + " [seed] [error-rate]\n"
                                "Damages a stream of frames with a seeded random generator and "
                                "decodes it, the error rate is per byte.";
    HostUtils::printLine(usage);
}

bool parseCommandLine(const int argc, char *argv[], uint32_t &seed, double &errorRate)
{
    using namespace sercom;

    // arguments must not exceed the seed and the error rate
    const std::vector<std::string> arguments(argv + 1, argv + argc);
    if (arguments.size() > 2) {
        HostUtils::logWarning("Too many arguments, expected the seed and the error rate only");
        return false;
    }

    // seed must be valid when given
    if (!arguments.empty()
        && !HostUtils::parseArgument(arguments.at(0),
                                     "seed",
                                     0,
                                     std::numeric_limits<uint32_t>::max(),
                                     seed)) {
        return false;
    }

    // error rate must be valid when given
    if (arguments.size() > 1 && !parseErrorRate(arguments.at(1), errorRate)) {
        return false;
    }

    // command line parsed successfully
    return true;
}

bool parseErrorRate(const std::string &argument, double &errorRate)
{
    using namespace sercom;

    // argument must be one number from zero to one
    char *argumentEnd = nullptr;
    const double argumentAsNumber = std::strtod(argument.c_str(), &argumentEnd);
    const bool probability = argumentAsNumber >= 0.0 && argumentAsNumber <= 1.0;
    if (argument.empty() || argumentEnd != argument.c_str() + argument.size() || !probability) {
        // warn the user
        const std::string warningMessage = "Invalid error rate argument " + argument
                                           + ", expected a number from 0 to 1";
        HostUtils::logWarning(warningMessage);

        // do not proceed further
        return false;
    }

    // set the error rate
    errorRate = argumentAsNumber;

    // error rate parsed successfully
    return true;
}

void addNoise(const std::vector<uint8_t> &stream,
              const double errorRate,
              std::mt19937 &generator,
              std::vector<uint8_t> &noisyStream)
{
    // create the random distributions of the damage
    std::uniform_real_distribution<double> chanceDistribution(0.0, 1.0);
    std::uniform_int_distribution<int> byteDistribution(0, 255);
    std::uniform_int_distribution<int> bitDistribution(0, 7);

    // iterate over the bytes to damage them by chance
    std::for_each(stream.cbegin(),
                  stream.cend(),
                  [errorRate,
                   &generator,
                   &chanceDistribution,
                   &byteDistribution,
                   &bitDistribution,
                   &noisyStream](const uint8_t byte) {
                      // byte must not be dropped
                      if (chanceDistribution(generator) < errorRate) {
                          return;
                      }

                      // flip one bit of the byte by chance
                      uint8_t noisyByte = byte;
                      if (chanceDistribution(generator) < errorRate) {
                          noisyByte = static_cast<uint8_t>(noisyByte
                                                           ^ (1 << bitDistribution(generator)));
                      }

                      // add the byte
                      noisyStream.push_back(noisyByte);

                      // duplicate the byte by chance
                      if (chanceDistribution(generator) < errorRate) {
                          noisyStream.push_back(noisyByte);
                      }

                      // follow the byte by garbage by chance
                      if (chanceDistribution(generator) < errorRate) {
                          noisyStream.push_back(static_cast<uint8_t>(byteDistribution(generator)));
                      }
                  });
}
