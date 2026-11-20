#ifndef SERCOM_APP_SETTINGS_H
#define SERCOM_APP_SETTINGS_H

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace sercom::AppSettings {

using namespace std::chrono_literals;

extern const uint8_t AppVersionMajor;           // major version of the application
extern const uint8_t AppVersionMinor;           // minor version of the application
extern const uint32_t AppVersionBuild;          // build number of the application
extern const std::string_view AppGitHash;       // git commit hash of the application
extern const std::string_view AppVersionString; // version of the application with its build number

constexpr std::chrono::milliseconds DEFAULT_TELEMETRY_PERIOD_MSEC
    = 0ms; // default telemetry period in milliseconds
constexpr std::chrono::milliseconds MINIMUM_TELEMETRY_PERIOD_MSEC
    = 20ms; // minimum telemetry period in milliseconds

constexpr size_t PING_HISTORY_SIZE = 8; // number of remembered ping requests

} // namespace sercom::AppSettings

#endif // SERCOM_APP_SETTINGS_H
