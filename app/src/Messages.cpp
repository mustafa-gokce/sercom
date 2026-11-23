#include "../include/Messages.h"

#include <algorithm>

#include "../../core/include/ByteCodec.h"

namespace sercom::Messages {

using namespace std::chrono_literals;

uint16_t fromMessageType(const MessageType messageType)
{
    return static_cast<uint16_t>(messageType);
}

MessageType toMessageType(const uint16_t messageType)
{
    switch (static_cast<MessageType>(messageType)) {
    case MessageType::PING_REQUEST: {
        return MessageType::PING_REQUEST;
    }
    case MessageType::PING_RESPONSE: {
        return MessageType::PING_RESPONSE;
    }
    case MessageType::DEVICE_INFO_REQUEST: {
        return MessageType::DEVICE_INFO_REQUEST;
    }
    case MessageType::DEVICE_INFO_RESPONSE: {
        return MessageType::DEVICE_INFO_RESPONSE;
    }
    case MessageType::LOG_REPORT_RESPONSE: {
        return MessageType::LOG_REPORT_RESPONSE;
    }
    case MessageType::ECHO_REQUEST: {
        return MessageType::ECHO_REQUEST;
    }
    case MessageType::ECHO_RESPONSE: {
        return MessageType::ECHO_RESPONSE;
    }
    case MessageType::ROUTE_FAILURE_REPORT_RESPONSE: {
        return MessageType::ROUTE_FAILURE_REPORT_RESPONSE;
    }
    case MessageType::SET_LED_REQUEST: {
        return MessageType::SET_LED_REQUEST;
    }
    case MessageType::SET_LED_RESPONSE: {
        return MessageType::SET_LED_RESPONSE;
    }
    case MessageType::SET_TELEMETRY_PERIOD_REQUEST: {
        return MessageType::SET_TELEMETRY_PERIOD_REQUEST;
    }
    case MessageType::TELEMETRY_REPORT_RESPONSE: {
        return MessageType::TELEMETRY_REPORT_RESPONSE;
    }
    case MessageType::UNKNOWN:
    default: {
        return MessageType::UNKNOWN;
    }
    }
}

uint8_t fromLogLevelType(const LogLevelType logLevelType)
{
    return static_cast<uint8_t>(logLevelType);
}

LogLevelType toLogLevelType(const uint8_t logLevelType)
{
    switch (static_cast<LogLevelType>(logLevelType)) {
    case LogLevelType::DEBUG: {
        return LogLevelType::DEBUG;
    }
    case LogLevelType::WARNING: {
        return LogLevelType::WARNING;
    }
    case LogLevelType::UNKNOWN:
    default: {
        return LogLevelType::UNKNOWN;
    }
    }
}

std::string_view getMessageTypeText(const MessageType messageType)
{
    switch (messageType) {
    case MessageType::PING_REQUEST: {
        return {"PING_REQUEST"};
    }
    case MessageType::PING_RESPONSE: {
        return {"PING_RESPONSE"};
    }
    case MessageType::DEVICE_INFO_REQUEST: {
        return {"DEVICE_INFO_REQUEST"};
    }
    case MessageType::DEVICE_INFO_RESPONSE: {
        return {"DEVICE_INFO_RESPONSE"};
    }
    case MessageType::LOG_REPORT_RESPONSE: {
        return {"LOG_REPORT_RESPONSE"};
    }
    case MessageType::ECHO_REQUEST: {
        return {"ECHO_REQUEST"};
    }
    case MessageType::ECHO_RESPONSE: {
        return {"ECHO_RESPONSE"};
    }
    case MessageType::ROUTE_FAILURE_REPORT_RESPONSE: {
        return {"ROUTE_FAILURE_REPORT_RESPONSE"};
    }
    case MessageType::SET_LED_REQUEST: {
        return {"SET_LED_REQUEST"};
    }
    case MessageType::SET_LED_RESPONSE: {
        return {"SET_LED_RESPONSE"};
    }
    case MessageType::SET_TELEMETRY_PERIOD_REQUEST: {
        return {"SET_TELEMETRY_PERIOD_REQUEST"};
    }
    case MessageType::TELEMETRY_REPORT_RESPONSE: {
        return {"TELEMETRY_REPORT_RESPONSE"};
    }
    case MessageType::UNKNOWN:
    default: {
        return {"UNKNOWN"};
    }
    }
}

std::string_view getLogLevelTypeText(const LogLevelType logLevelType)
{
    switch (logLevelType) {
    case LogLevelType::DEBUG: {
        return {"DEBUG"};
    }
    case LogLevelType::WARNING: {
        return {"WARNING"};
    }
    case LogLevelType::UNKNOWN:
    default: {
        return {"UNKNOWN"};
    }
    }
}

GenericMessage::GenericMessage()
    : payload()
    , payloadLength(0)
    , sourceAddress(Definitions::UNASSIGNED_ADDRESS)
{}

GenericMessage::GenericMessage(const uint8_t *data, const size_t size)
    : payload()
    , payloadLength(0)
    , sourceAddress(Definitions::UNASSIGNED_ADDRESS)
{
    // data must fit into a payload of this build
    if (size > this->payload.size()) {
        return;
    }

    // data must exist when it is not empty
    if (size > 0 && data == nullptr) {
        return;
    }

    // copy the data into the payload
    std::copy_n(data, size, this->payload.begin());
    this->payloadLength = size;
}

GenericMessage::GenericMessage(const Frame::Frame &frame)
    : GenericMessage(frame.getPayload(), frame.getPayloadLength())
{
    // keep the address of the source of a routed frame
    this->sourceAddress = frame.getSourceAddress();
}

const uint8_t *GenericMessage::getPayload() const
{
    // expose the payload bytes
    return this->payload.data();
}

size_t GenericMessage::getPayloadLength() const
{
    // expose the payload length
    return this->payloadLength;
}

uint8_t GenericMessage::getSourceAddress() const
{
    // expose the address of the source
    return this->sourceAddress;
}

PingRequest::PingRequest(const std::chrono::milliseconds requestTimestamp)
{
    // write the request timestamp
    ByteCodec::ByteWriter byteWriter(this->payload.data(), this->payload.size());
    byteWriter.writeUint32(static_cast<uint32_t>(requestTimestamp.count()));

    // keep the payload length
    this->payloadLength = byteWriter.getSize();
}

PingRequest::PingRequest(const Frame::Frame &frame)
    : GenericMessage(frame)
{}

std::chrono::milliseconds PingRequest::getRequestTimestamp() const
{
    // payload must hold every field
    if (this->payloadLength < PING_REQUEST_SIZE) {
        return {};
    }

    // expose the request timestamp
    return std::chrono::milliseconds(ByteCodec::loadUint32(this->payload.data()));
}

bool PingRequest::getLikelyValid() const
{
    // payload must hold every field
    if (this->payloadLength < PING_REQUEST_SIZE) {
        return false;
    }

    // passed basic checks
    return true;
}

PingResponse::PingResponse(const std::chrono::milliseconds requestTimestamp,
                           const std::chrono::milliseconds uptime)
{
    // write the request timestamp and the uptime
    ByteCodec::ByteWriter byteWriter(this->payload.data(), this->payload.size());
    byteWriter.writeUint32(static_cast<uint32_t>(requestTimestamp.count()));
    byteWriter.writeUint32(static_cast<uint32_t>(uptime.count()));

    // keep the payload length
    this->payloadLength = byteWriter.getSize();
}

PingResponse::PingResponse(const Frame::Frame &frame)
    : GenericMessage(frame)
{}

std::chrono::milliseconds PingResponse::getRequestTimestamp() const
{
    // payload must hold every field
    if (this->payloadLength < PING_RESPONSE_SIZE) {
        return {};
    }

    // expose the request timestamp
    return std::chrono::milliseconds(ByteCodec::loadUint32(this->payload.data()));
}

std::chrono::milliseconds PingResponse::getUptime() const
{
    // payload must hold every field
    if (this->payloadLength < PING_RESPONSE_SIZE) {
        return {};
    }

    // expose the uptime
    return std::chrono::milliseconds(ByteCodec::loadUint32(this->payload.data() + 4));
}

bool PingResponse::getLikelyValid() const
{
    // payload must hold every field
    if (this->payloadLength < PING_RESPONSE_SIZE) {
        return false;
    }

    // passed basic checks
    return true;
}

DeviceInfoResponse::DeviceInfoResponse(const uint8_t firmwareVersionMajor,
                                       const uint8_t firmwareVersionMinor,
                                       const std::chrono::milliseconds uptime,
                                       const uint32_t freeHeapSize,
                                       const std::string_view deviceName)
{
    // device name must not exceed the maximum device name length
    if (deviceName.size() > MAXIMUM_DEVICE_NAME_LENGTH) {
        return;
    }

    // device name must fit into the payload
    if (DEVICE_INFO_RESPONSE_SIZE + deviceName.size() > this->payload.size()) {
        return;
    }

    // write every field and the device name
    ByteCodec::ByteWriter byteWriter(this->payload.data(), this->payload.size());
    byteWriter.writeUint8(firmwareVersionMajor);
    byteWriter.writeUint8(firmwareVersionMinor);
    byteWriter.writeUint32(static_cast<uint32_t>(uptime.count()));
    byteWriter.writeUint32(freeHeapSize);
    byteWriter.writeString(deviceName.data(), deviceName.size());

    // keep the payload length
    this->payloadLength = byteWriter.getSize();
}

DeviceInfoResponse::DeviceInfoResponse(const Frame::Frame &frame)
    : GenericMessage(frame)
{}

uint8_t DeviceInfoResponse::getFirmwareVersionMajor() const
{
    // payload must hold every field
    if (this->payloadLength < DEVICE_INFO_RESPONSE_SIZE) {
        return 0;
    }

    // expose the major firmware version
    return this->payload[0];
}

uint8_t DeviceInfoResponse::getFirmwareVersionMinor() const
{
    // payload must hold every field
    if (this->payloadLength < DEVICE_INFO_RESPONSE_SIZE) {
        return 0;
    }

    // expose the minor firmware version
    return this->payload[1];
}

std::chrono::milliseconds DeviceInfoResponse::getUptime() const
{
    // payload must hold every field
    if (this->payloadLength < DEVICE_INFO_RESPONSE_SIZE) {
        return {};
    }

    // expose the uptime
    return std::chrono::milliseconds(ByteCodec::loadUint32(this->payload.data() + 2));
}

uint32_t DeviceInfoResponse::getFreeHeapSize() const
{
    // payload must hold every field
    if (this->payloadLength < DEVICE_INFO_RESPONSE_SIZE) {
        return 0;
    }

    // expose the free heap size
    return ByteCodec::loadUint32(this->payload.data() + 6);
}

std::string_view DeviceInfoResponse::getDeviceName() const
{
    // payload must hold the whole device name
    if (!this->getLikelyValid()) {
        return {};
    }

    // expose the characters behind the length of the device name
    return {reinterpret_cast<const char *>(this->payload.data() + DEVICE_INFO_RESPONSE_SIZE),
            this->payload[10]};
}

bool DeviceInfoResponse::getLikelyValid() const
{
    // payload must hold every field
    if (this->payloadLength < DEVICE_INFO_RESPONSE_SIZE) {
        return false;
    }

    // payload must hold the whole device name
    if (this->payloadLength < DEVICE_INFO_RESPONSE_SIZE + this->payload[10]) {
        return false;
    }

    // passed basic checks
    return true;
}

LogReportResponse::LogReportResponse(const LogLevelType logLevelType, const std::string_view text)
{
    // text must not exceed the maximum log text length
    if (text.size() > MAXIMUM_LOG_TEXT_LENGTH) {
        return;
    }

    // text must fit into the payload
    if (LOG_REPORT_RESPONSE_SIZE + text.size() > this->payload.size()) {
        return;
    }

    // write the log level type and the text
    ByteCodec::ByteWriter byteWriter(this->payload.data(), this->payload.size());
    byteWriter.writeUint8(fromLogLevelType(logLevelType));
    byteWriter.writeString(text.data(), text.size());

    // keep the payload length
    this->payloadLength = byteWriter.getSize();
}

LogReportResponse::LogReportResponse(const Frame::Frame &frame)
    : GenericMessage(frame)
{}

LogLevelType LogReportResponse::getLogLevelType() const
{
    // payload must hold every field
    if (this->payloadLength < LOG_REPORT_RESPONSE_SIZE) {
        return LogLevelType::UNKNOWN;
    }

    // expose the log level type
    return toLogLevelType(this->payload[0]);
}

std::string_view LogReportResponse::getText() const
{
    // payload must hold the whole text
    if (!this->getLikelyValid()) {
        return {};
    }

    // expose the characters behind the length of the text
    return {reinterpret_cast<const char *>(this->payload.data() + LOG_REPORT_RESPONSE_SIZE),
            this->payload[1]};
}

bool LogReportResponse::getLikelyValid() const
{
    // payload must hold every field
    if (this->payloadLength < LOG_REPORT_RESPONSE_SIZE) {
        return false;
    }

    // payload must hold the whole text
    if (this->payloadLength < LOG_REPORT_RESPONSE_SIZE + this->payload[1]) {
        return false;
    }

    // passed basic checks
    return true;
}

EchoRequest::EchoRequest(const uint8_t *data, const size_t size)
    : GenericMessage(data, size)
{}

EchoRequest::EchoRequest(const Frame::Frame &frame)
    : GenericMessage(frame)
{}

EchoResponse::EchoResponse(const uint8_t *data, const size_t size)
    : GenericMessage(data, size)
{}

EchoResponse::EchoResponse(const Frame::Frame &frame)
    : GenericMessage(frame)
{}

RouteFailureReportResponse::RouteFailureReportResponse(
    const uint8_t destinationAddress,
    const uint16_t messageType,
    const Definitions::DeliveryFailureType deliveryFailureType,
    const Definitions::MessageStatusType messageStatusType,
    const uint8_t *quote,
    const size_t quoteLength)
{
    // quote must not exceed the maximum quote size
    if (quoteLength > Definitions::MAXIMUM_ROUTE_FAILURE_QUOTE_SIZE) {
        return;
    }

    // quote must exist when it is not empty
    if (quoteLength > 0 && quote == nullptr) {
        return;
    }

    // write every field and the quote
    ByteCodec::ByteWriter byteWriter(this->payload.data(), this->payload.size());
    byteWriter.writeUint8(destinationAddress);
    byteWriter.writeUint16(messageType);
    byteWriter.writeUint8(static_cast<uint8_t>(deliveryFailureType));
    byteWriter.writeUint8(Definitions::fromMessageStatusType(messageStatusType));
    byteWriter.writeUint8(static_cast<uint8_t>(quoteLength));
    byteWriter.writeBytes(quote, quoteLength);

    // keep the payload length
    this->payloadLength = byteWriter.getSize();
}

RouteFailureReportResponse::RouteFailureReportResponse(const Frame::Frame &frame)
    : GenericMessage(frame)
{}

uint8_t RouteFailureReportResponse::getDestinationAddress() const
{
    // payload must hold every field
    if (this->payloadLength < Definitions::ROUTE_FAILURE_REPORT_SIZE) {
        return Definitions::UNASSIGNED_ADDRESS;
    }

    // expose the destination address
    return this->payload[0];
}

uint16_t RouteFailureReportResponse::getMessageType() const
{
    // payload must hold every field
    if (this->payloadLength < Definitions::ROUTE_FAILURE_REPORT_SIZE) {
        return 0;
    }

    // expose the message type
    return ByteCodec::loadUint16(this->payload.data() + 1);
}

Definitions::DeliveryFailureType RouteFailureReportResponse::getDeliveryFailureType() const
{
    // payload must hold every field
    if (this->payloadLength < Definitions::ROUTE_FAILURE_REPORT_SIZE) {
        return Definitions::DeliveryFailureType::UNKNOWN;
    }

    // expose a known delivery failure type
    switch (static_cast<Definitions::DeliveryFailureType>(this->payload[3])) {
    case Definitions::DeliveryFailureType::TIMED_OUT: {
        return Definitions::DeliveryFailureType::TIMED_OUT;
    }
    case Definitions::DeliveryFailureType::REJECTED: {
        return Definitions::DeliveryFailureType::REJECTED;
    }
    case Definitions::DeliveryFailureType::UNKNOWN:
    default: {
        return Definitions::DeliveryFailureType::UNKNOWN;
    }
    }
}

Definitions::MessageStatusType RouteFailureReportResponse::getMessageStatusType() const
{
    // payload must hold every field
    if (this->payloadLength < Definitions::ROUTE_FAILURE_REPORT_SIZE) {
        return Definitions::MessageStatusType::UNKNOWN;
    }

    // expose the message status
    return Definitions::toMessageStatusType(this->payload[4]);
}

const uint8_t *RouteFailureReportResponse::getQuote() const
{
    // expose the quote behind the fixed fields
    return this->payload.data() + Definitions::ROUTE_FAILURE_REPORT_SIZE;
}

size_t RouteFailureReportResponse::getQuoteLength() const
{
    // payload must hold the whole quote
    if (!this->getLikelyValid()) {
        return 0;
    }

    // expose the quote length
    return this->payload[5];
}

bool RouteFailureReportResponse::getLikelyValid() const
{
    // payload must hold every field
    if (this->payloadLength < Definitions::ROUTE_FAILURE_REPORT_SIZE) {
        return false;
    }

    // quote must not exceed the maximum quote size
    if (this->payload[5] > Definitions::MAXIMUM_ROUTE_FAILURE_QUOTE_SIZE) {
        return false;
    }

    // payload must hold the whole quote
    if (this->payloadLength < Definitions::ROUTE_FAILURE_REPORT_SIZE + this->payload[5]) {
        return false;
    }

    // passed basic checks
    return true;
}

SetLedRequest::SetLedRequest(const bool ledOn)
{
    // write the LED state as zero or one
    ByteCodec::ByteWriter byteWriter(this->payload.data(), this->payload.size());
    byteWriter.writeUint8(static_cast<uint8_t>(ledOn));

    // keep the payload length
    this->payloadLength = byteWriter.getSize();
}

SetLedRequest::SetLedRequest(const Frame::Frame &frame)
    : GenericMessage(frame)
{}

bool SetLedRequest::getLedOn() const
{
    // payload must hold every field
    if (this->payloadLength < SET_LED_REQUEST_SIZE) {
        return false;
    }

    // expose whether the LED is switched on
    return this->payload[0] == 1;
}

bool SetLedRequest::getLikelyValid() const
{
    // payload must hold every field
    if (this->payloadLength < SET_LED_REQUEST_SIZE) {
        return false;
    }

    // LED state must be zero or one
    if (this->payload[0] > 1) {
        return false;
    }

    // passed basic checks
    return true;
}

SetLedResponse::SetLedResponse(const bool ledOn)
{
    // write the LED state as zero or one
    ByteCodec::ByteWriter byteWriter(this->payload.data(), this->payload.size());
    byteWriter.writeUint8(static_cast<uint8_t>(ledOn));

    // keep the payload length
    this->payloadLength = byteWriter.getSize();
}

SetLedResponse::SetLedResponse(const Frame::Frame &frame)
    : GenericMessage(frame)
{}

bool SetLedResponse::getLedOn() const
{
    // payload must hold every field
    if (this->payloadLength < SET_LED_RESPONSE_SIZE) {
        return false;
    }

    // expose whether the LED is on
    return this->payload[0] == 1;
}

bool SetLedResponse::getLikelyValid() const
{
    // payload must hold every field
    if (this->payloadLength < SET_LED_RESPONSE_SIZE) {
        return false;
    }

    // LED state must be zero or one
    if (this->payload[0] > 1) {
        return false;
    }

    // passed basic checks
    return true;
}

SetTelemetryPeriodRequest::SetTelemetryPeriodRequest(const std::chrono::milliseconds telemetryPeriod)
{
    // telemetry period must fit into its field
    if (telemetryPeriod < 0ms || telemetryPeriod > MAXIMUM_TELEMETRY_PERIOD_MSEC) {
        return;
    }

    // write the telemetry period
    ByteCodec::ByteWriter byteWriter(this->payload.data(), this->payload.size());
    byteWriter.writeUint16(static_cast<uint16_t>(telemetryPeriod.count()));

    // keep the payload length
    this->payloadLength = byteWriter.getSize();
}

SetTelemetryPeriodRequest::SetTelemetryPeriodRequest(const Frame::Frame &frame)
    : GenericMessage(frame)
{}

std::chrono::milliseconds SetTelemetryPeriodRequest::getTelemetryPeriod() const
{
    // payload must hold every field
    if (this->payloadLength < SET_TELEMETRY_PERIOD_REQUEST_SIZE) {
        return {};
    }

    // expose the telemetry period
    return std::chrono::milliseconds(ByteCodec::loadUint16(this->payload.data()));
}

bool SetTelemetryPeriodRequest::getLikelyValid() const
{
    // payload must hold every field
    if (this->payloadLength < SET_TELEMETRY_PERIOD_REQUEST_SIZE) {
        return false;
    }

    // passed basic checks
    return true;
}

TelemetryReportResponse::TelemetryReportResponse(const uint16_t sequence,
                                                 const std::chrono::milliseconds uptime,
                                                 const uint32_t freeHeapSize,
                                                 const int16_t temperature)
{
    // write every field
    ByteCodec::ByteWriter byteWriter(this->payload.data(), this->payload.size());
    byteWriter.writeUint16(sequence);
    byteWriter.writeUint32(static_cast<uint32_t>(uptime.count()));
    byteWriter.writeUint32(freeHeapSize);
    byteWriter.writeInt16(temperature);

    // keep the payload length
    this->payloadLength = byteWriter.getSize();
}

TelemetryReportResponse::TelemetryReportResponse(const Frame::Frame &frame)
    : GenericMessage(frame)
{}

uint16_t TelemetryReportResponse::getSequence() const
{
    // payload must hold every field
    if (this->payloadLength < TELEMETRY_REPORT_RESPONSE_SIZE) {
        return 0;
    }

    // expose the sequence
    return ByteCodec::loadUint16(this->payload.data());
}

std::chrono::milliseconds TelemetryReportResponse::getUptime() const
{
    // payload must hold every field
    if (this->payloadLength < TELEMETRY_REPORT_RESPONSE_SIZE) {
        return {};
    }

    // expose the uptime
    return std::chrono::milliseconds(ByteCodec::loadUint32(this->payload.data() + 2));
}

uint32_t TelemetryReportResponse::getFreeHeapSize() const
{
    // payload must hold every field
    if (this->payloadLength < TELEMETRY_REPORT_RESPONSE_SIZE) {
        return 0;
    }

    // expose the free heap size
    return ByteCodec::loadUint32(this->payload.data() + 6);
}

int16_t TelemetryReportResponse::getTemperature() const
{
    // payload must hold every field
    if (this->payloadLength < TELEMETRY_REPORT_RESPONSE_SIZE) {
        return 0;
    }

    // expose the two's complement bit pattern of the temperature
    return static_cast<int16_t>(ByteCodec::loadUint16(this->payload.data() + 10));
}

bool TelemetryReportResponse::getLikelyValid() const
{
    // payload must hold every field
    if (this->payloadLength < TELEMETRY_REPORT_RESPONSE_SIZE) {
        return false;
    }

    // passed basic checks
    return true;
}

} // namespace sercom::Messages
