#include "../include/Messages.h"

namespace sercom::Messages {

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

} // namespace sercom::Messages
