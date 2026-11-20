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
}

TEST_CASE_METHOD(TestCliApplication,
                 "Command type can be parsed from string",
                 "[CliApplication][toCommandType]")
{
    REQUIRE(CliApplication::toCommandType(std::string_view())
            == CliApplication::CommandType::UNKNOWN);
    REQUIRE(CliApplication::toCommandType("ping") == CliApplication::CommandType::PING);
    REQUIRE(CliApplication::toCommandType("LOREM_IPSUM") == CliApplication::CommandType::UNKNOWN);
    REQUIRE(CliApplication::toCommandType("PING") == CliApplication::CommandType::UNKNOWN);
    REQUIRE(CliApplication::toCommandType("ping ") == CliApplication::CommandType::UNKNOWN);
}
