#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>

#include "../app/include/Messages.h"
#include "../core/include/Definitions.h"
#include "../core/include/Frame.h"
#include "../core/include/FrameDecoder.h"
#include "../host/include/HostUtils.h"

class MockDecoderHandler final : public sercom::FrameDecoder::FrameDecoderHandler
{
public:
    /*
     * Mock decoder handler that hands every received frame to the function of its message type.
     */
    MockDecoderHandler()
        : handledMessageCounter(0)
        , unsupportedMessageCounter(0)
    {}
    ~MockDecoderHandler() final {}

    // delete copy constructor and copy assignment operator
    MockDecoderHandler(const MockDecoderHandler &) = delete;
    MockDecoderHandler &operator=(const MockDecoderHandler &) = delete;

    // delete move constructor and move assignment operator
    MockDecoderHandler(MockDecoderHandler &&) = delete;
    MockDecoderHandler &operator=(MockDecoderHandler &&) = delete;

public:
    /*
     * Listen for a received frame and hand it to the function of its message type.
     */
    void onReceivedFrame(const sercom::Frame::Frame &frame) final
    {
        using namespace sercom;

        // print the message type
        static constexpr size_t messageTypeDigitCount = 2 * Definitions::MESSAGE_TYPE_SIZE;
        const std::string line = "frame received, message type 0x"
                                 + HostUtils::getHexText(frame.getMessageType(),
                                                         messageTypeDigitCount);
        HostUtils::printLine(line);

        // handle the message by its message type
        switch (Messages::toMessageType(frame.getMessageType())) {
        // handle a ping request
        case Messages::MessageType::PING_REQUEST: {
            this->handlePingRequest(frame);
            break;
        }

        // handle a device info request
        case Messages::MessageType::DEVICE_INFO_REQUEST: {
            this->handleDeviceInfoRequest(frame);
            break;
        }

        // handle a set LED request
        case Messages::MessageType::SET_LED_REQUEST: {
            this->handleSetLedRequest(frame);
            break;
        }

        // handle every other message type as unsupported
        case Messages::MessageType::PING_RESPONSE:
        case Messages::MessageType::DEVICE_INFO_RESPONSE:
        case Messages::MessageType::LOG_REPORT_RESPONSE:
        case Messages::MessageType::ECHO_REQUEST:
        case Messages::MessageType::ECHO_RESPONSE:
        case Messages::MessageType::SET_LED_RESPONSE:
        case Messages::MessageType::SET_TELEMETRY_PERIOD_REQUEST:
        case Messages::MessageType::TELEMETRY_REPORT_RESPONSE:
        case Messages::MessageType::UNKNOWN:
        default: {
            this->handleUnsupportedMessage(frame);
            break;
        }
        }
    }

    /*
     * Listen for a decode error that the frames of this example do not cause.
     */
    void onDecodeError(
        [[maybe_unused]] const sercom::Definitions::DecodeErrorType decodeErrorType) final
    {}

public:
    /*
     * Get the number of handled messages.
     */
    [[nodiscard]] uint64_t getHandledMessageCounter() const
    {
        // expose the handled message counter
        return this->handledMessageCounter;
    }

    /*
     * Get the number of unsupported messages.
     */
    [[nodiscard]] uint64_t getUnsupportedMessageCounter() const
    {
        // expose the unsupported message counter
        return this->unsupportedMessageCounter;
    }

private:
    /*
     * Handle a ping request and print its request timestamp.
     */
    void handlePingRequest(const sercom::Frame::Frame &frame)
    {
        using namespace sercom;

        // print the request timestamp
        const Messages::PingRequest pingRequest(frame);
        const std::string line = "  handlePingRequest: request timestamp "
                                 + std::to_string(pingRequest.getRequestTimestamp().count())
                                 + " ms";
        HostUtils::printLine(line);

        // keep track of handled messages
        ++this->handledMessageCounter;
    }

    /*
     * Handle a device info request without fields.
     */
    void handleDeviceInfoRequest([[maybe_unused]] const sercom::Frame::Frame &frame)
    {
        // print that there is no field to read
        sercom::HostUtils::printLine("  handleDeviceInfoRequest: no fields to read");

        // keep track of handled messages
        ++this->handledMessageCounter;
    }

    /*
     * Handle a set LED request and print the LED state that it asks for.
     */
    void handleSetLedRequest(const sercom::Frame::Frame &frame)
    {
        using namespace sercom;

        // print the LED state
        const Messages::SetLedRequest setLedRequest(frame);
        const std::string line = std::string("  handleSetLedRequest: LED ")
                                 + (setLedRequest.getLedOn() ? "on" : "off");
        HostUtils::printLine(line);

        // keep track of handled messages
        ++this->handledMessageCounter;
    }

    /*
     * Handle a message whose message type has no function of its own.
     */
    void handleUnsupportedMessage([[maybe_unused]] const sercom::Frame::Frame &frame)
    {
        // print that the message type is not supported
        sercom::HostUtils::printLine("  handleUnsupportedMessage: message type not supported");

        // keep track of unsupported messages
        ++this->unsupportedMessageCounter;
    }

private:
    uint64_t handledMessageCounter;     // messages handled by their own function
    uint64_t unsupportedMessageCounter; // messages of an unsupported message type
};

/*
 * Create the frame of a message and hand it to the decoder.
 */
void receiveMessage(sercom::FrameDecoder::FrameDecoder &frameDecoder,
                    uint16_t messageType,
                    const sercom::Messages::GenericMessage &message);

/*
 * Main entry point of the executable.
 */
int main(int argc, char *argv[]);

int main([[maybe_unused]] int argc, [[maybe_unused]] char *argv[])
{
    using namespace sercom;
    using namespace std::chrono_literals;

    // create the decoder without the inter byte timeout
    MockDecoderHandler mockDecoderHandler;
    FrameDecoder::FrameDecoder frameDecoder(mockDecoderHandler, 0ms);

    // receive a ping request sent at one second
    static constexpr std::chrono::milliseconds requestTimestamp = 1 * 1000ms;
    receiveMessage(frameDecoder,
                   Messages::fromMessageType(Messages::MessageType::PING_REQUEST),
                   Messages::PingRequest(requestTimestamp));

    // receive a device info request
    receiveMessage(frameDecoder,
                   Messages::fromMessageType(Messages::MessageType::DEVICE_INFO_REQUEST),
                   Messages::DeviceInfoRequest());

    // receive a set LED request that switches the LED on
    static constexpr bool ledOn = true;
    receiveMessage(frameDecoder,
                   Messages::fromMessageType(Messages::MessageType::SET_LED_REQUEST),
                   Messages::SetLedRequest(ledOn));

    // receive a message without payload of a message type that the device does not know
    static constexpr uint16_t unknownMessageType = 0x0200;
    receiveMessage(frameDecoder, unknownMessageType, Messages::GenericMessage());

    // handler must handle the three requests and find the unknown message type unsupported
    static constexpr uint64_t handledMessageCount = 3;
    if (mockDecoderHandler.getHandledMessageCounter() != handledMessageCount
        || mockDecoderHandler.getUnsupportedMessageCounter() != 1) {
        HostUtils::logWarning("The handler did not hand every message to its function");
        return 1;
    }

    // example finished successfully
    return 0;
}

void receiveMessage(sercom::FrameDecoder::FrameDecoder &frameDecoder,
                    const uint16_t messageType,
                    const sercom::Messages::GenericMessage &message)
{
    using namespace sercom;
    using namespace std::chrono_literals;

    // create the frame of the message with a fixed message counter
    static constexpr uint16_t messageCounter = 1;
    const Frame::Frame frame(0,
                             messageCounter,
                             messageType,
                             message.getPayload(),
                             message.getPayloadLength());

    // hand the frame to the decoder
    frameDecoder.onReceivedData(frame.getData(), frame.getSize(), 0ms);
}
