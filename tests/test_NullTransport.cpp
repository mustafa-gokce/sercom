#include <cstdint>
#include <vector>

#include <catch2/catch_all.hpp>

#include "../core/include/NullTransport.h"
#include "../core/include/Transport.h"

using namespace sercom;

class TestNullTransport
{
public:
    /*
     * Null transport test fixture.
     */
    TestNullTransport() = default;
    ~TestNullTransport() = default;

    // delete copy and move operators
    TestNullTransport(const TestNullTransport &other) = delete;
    TestNullTransport &operator=(const TestNullTransport &other) = delete;
    TestNullTransport(TestNullTransport &&other) noexcept = delete;
    TestNullTransport &operator=(TestNullTransport &&other) noexcept = delete;

protected:
    NullTransport::NullTransport nullTransport; // transport under test
};

TEST_CASE_METHOD(TestNullTransport,
                 "A null transport takes no byte of any data",
                 "[NullTransport][testSendData]")
{
    // send data, no data and missing data through the transport of a port without a cable
    const std::vector<uint8_t> data = {0xA5, 0xC3, 0x01};
    Transport::Transport &transport = nullTransport;
    REQUIRE_FALSE(transport.sendData(data.data(), data.size()));
    REQUIRE_FALSE(transport.sendData(data.data(), 0));
    REQUIRE_FALSE(transport.sendData(nullptr, 0));
}
