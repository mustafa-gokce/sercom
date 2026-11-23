#include <cstdint>
#include <string_view>

#include <catch2/catch_all.hpp>

#include "../core/include/Definitions.h"
#include "../core/include/Router.h"
#include "../core/include/Settings.h"
#include "../host/include/CliApplication.h"

using namespace sercom;

class TestCliApplication
{
public:
    /*
     * Command line application test fixture.
     */
    TestCliApplication() = default;
    ~TestCliApplication() = default;

    // delete copy and move operators
    TestCliApplication(const TestCliApplication &other) = delete;
    TestCliApplication &operator=(const TestCliApplication &other) = delete;
    TestCliApplication(TestCliApplication &&other) noexcept = delete;
    TestCliApplication &operator=(TestCliApplication &&other) noexcept = delete;
};

TEST_CASE_METHOD(TestCliApplication,
                 "Command type can be converted to string",
                 "[CliApplication][fromCommandType]")
{
    REQUIRE(CliApplication::fromCommandType(CliApplication::CommandType::UNKNOWN).empty());
    REQUIRE(CliApplication::fromCommandType(CliApplication::CommandType::PING) == "ping");
    REQUIRE(CliApplication::fromCommandType(CliApplication::CommandType::INFO) == "info");
    REQUIRE(CliApplication::fromCommandType(CliApplication::CommandType::LED) == "led");
    REQUIRE(CliApplication::fromCommandType(CliApplication::CommandType::TELEMETRY) == "telemetry");
    REQUIRE(CliApplication::fromCommandType(CliApplication::CommandType::MONITOR) == "monitor");
    REQUIRE(CliApplication::fromCommandType(CliApplication::CommandType::STRESS) == "stress");
    REQUIRE(CliApplication::fromCommandType(CliApplication::CommandType::DISCOVER) == "discover");
}

TEST_CASE_METHOD(TestCliApplication,
                 "Command type can be parsed from string",
                 "[CliApplication][toCommandType]")
{
    REQUIRE(CliApplication::toCommandType(std::string_view())
            == CliApplication::CommandType::UNKNOWN);
    REQUIRE(CliApplication::toCommandType("ping") == CliApplication::CommandType::PING);
    REQUIRE(CliApplication::toCommandType("info") == CliApplication::CommandType::INFO);
    REQUIRE(CliApplication::toCommandType("led") == CliApplication::CommandType::LED);
    REQUIRE(CliApplication::toCommandType("telemetry") == CliApplication::CommandType::TELEMETRY);
    REQUIRE(CliApplication::toCommandType("monitor") == CliApplication::CommandType::MONITOR);
    REQUIRE(CliApplication::toCommandType("stress") == CliApplication::CommandType::STRESS);
    REQUIRE(CliApplication::toCommandType("discover") == CliApplication::CommandType::DISCOVER);
    REQUIRE(CliApplication::toCommandType("LOREM_IPSUM") == CliApplication::CommandType::UNKNOWN);
    REQUIRE(CliApplication::toCommandType("PING") == CliApplication::CommandType::UNKNOWN);
    REQUIRE(CliApplication::toCommandType("ping ") == CliApplication::CommandType::UNKNOWN);
}

TEST_CASE_METHOD(TestCliApplication,
                 "A command line without a destination sends direct frames without an address",
                 "[CliApplication][testDirectCommandLine]")
{
    // check the defaults of a command line
    CliApplication::CommandLine commandLine;
    commandLine.commandType = CliApplication::CommandType::PING;
    REQUIRE(commandLine.destinationAddress == Definitions::UNASSIGNED_ADDRESS);
    REQUIRE(commandLine.timeToLive == Settings::DEFAULT_TIME_TO_LIVE);

    // check the router configuration of the device client without an address
    const Router::RouterConfiguration routerConfiguration = CliApplication::getRouterConfiguration(
        commandLine);
    REQUIRE(routerConfiguration.nodeAddress == Definitions::UNASSIGNED_ADDRESS);
    REQUIRE(routerConfiguration.proxyAddress == Definitions::UNASSIGNED_ADDRESS);
}

TEST_CASE_METHOD(
    TestCliApplication,
    "A command line with a destination and a time to live gives the client the host address",
    "[CliApplication][testDestinationCommandLine]")
{
    // set a destination and a time to live
    static constexpr uint8_t destinationAddress = 4;
    static constexpr uint8_t timeToLive = 2;
    CliApplication::CommandLine commandLine;
    commandLine.commandType = CliApplication::CommandType::INFO;
    commandLine.destinationAddress = destinationAddress;
    commandLine.timeToLive = timeToLive;

    // check the router configuration of the device client
    const Router::RouterConfiguration routerConfiguration = CliApplication::getRouterConfiguration(
        commandLine);
    REQUIRE(routerConfiguration.nodeAddress == Definitions::HOST_ADDRESS);
    REQUIRE(routerConfiguration.timeToLive == timeToLive);
}

TEST_CASE_METHOD(TestCliApplication,
                 "The discover command gives the client the host address without a destination",
                 "[CliApplication][testDiscoverCommandLine]")
{
    // check the router configuration of the discover command
    CliApplication::CommandLine commandLine;
    commandLine.commandType = CliApplication::CommandType::DISCOVER;
    const Router::RouterConfiguration routerConfiguration = CliApplication::getRouterConfiguration(
        commandLine);
    REQUIRE(routerConfiguration.nodeAddress == Definitions::HOST_ADDRESS);
    REQUIRE(routerConfiguration.timeToLive == Settings::DEFAULT_TIME_TO_LIVE);
}
