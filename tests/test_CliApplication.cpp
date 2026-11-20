#include <string_view>

#include <catch2/catch_all.hpp>

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
    REQUIRE(CliApplication::toCommandType("LOREM_IPSUM") == CliApplication::CommandType::UNKNOWN);
    REQUIRE(CliApplication::toCommandType("PING") == CliApplication::CommandType::UNKNOWN);
    REQUIRE(CliApplication::toCommandType("ping ") == CliApplication::CommandType::UNKNOWN);
}
