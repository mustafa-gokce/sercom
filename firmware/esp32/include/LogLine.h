#ifndef SERCOM_LOG_LINE_H
#define SERCOM_LOG_LINE_H

#include <array>
#include <cstddef>

#include "../../../app/include/Messages.h"

#include "../include/FirmwareSettings.h"

namespace sercom::LogLine {

/*
 * Log line handed from any task to the protocol task.
 */
struct LogLine
{
    Messages::LogLevelType logLevelType = Messages::LogLevelType::UNKNOWN; // log level type
    size_t textLength = 0;                                                 // length in characters
    std::array<char, FirmwareSettings::LOG_LINE_SIZE> text{};              // unterminated text
};

} // namespace sercom::LogLine

#endif // SERCOM_LOG_LINE_H
