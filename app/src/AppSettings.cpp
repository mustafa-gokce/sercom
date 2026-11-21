#include "../include/AppSettings.h"

namespace sercom::AppSettings {

constexpr uint8_t AppVersionMajor = APP_VERSION_MAJOR;
constexpr uint8_t AppVersionMinor = APP_VERSION_MINOR;
constexpr uint32_t AppVersionBuild = APP_VERSION_BUILD;
constexpr std::string_view AppGitHash = APP_GIT_HASH;
constexpr std::string_view AppVersionString = APP_VERSION_STRING;

} // namespace sercom::AppSettings
