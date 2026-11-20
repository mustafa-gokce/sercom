#include "../include/main.h"

#include <chrono>
#include <cstdint>

#include <Arduino.h>

#include "../../../app/include/Messages.h"
#include "../../../core/include/Frame.h"

#include "../include/FirmwareSettings.h"

void setup()
{
    using namespace sercom;

    // open the serial port 8N1 without flow control
    Serial.begin(FirmwareSettings::SERIAL_BAUD_RATE, SERIAL_8N1);
}

void loop()
{
    using namespace sercom;

    // create a ping request with the time since the start
    const std::chrono::milliseconds requestTimestamp(millis());
    const Messages::PingRequest pingRequest(requestTimestamp);

    // send the ping request frame without acknowledgement
    static constexpr uint16_t messageCounter = 0;
    const Frame::Frame frame(0,
                             messageCounter,
                             Messages::fromMessageType(Messages::MessageType::PING_REQUEST),
                             pingRequest.getPayload(),
                             pingRequest.getPayloadLength());
    Serial.write(frame.getData(), frame.getSize());

    // wait for the next frame
    delay(static_cast<uint32_t>(FirmwareSettings::PING_REQUEST_PERIOD_MSEC.count()));
}
