#pragma once

#include <string>

// Bump this semantic version for any meaningful shipped code or UI change.
#ifndef BE_RELEASE_VERSION
#define BE_RELEASE_VERSION "9.3.6"
#endif

// Optional build channel (for example: rc1, staging, hotfix). Leave empty for
// normal builds.
#ifndef BE_RELEASE_CHANNEL
#define BE_RELEASE_CHANNEL ""
#endif

// Build identifier must distinguish binaries built from the same source version.
#ifndef BE_RELEASE_BUILD_ID
#define BE_RELEASE_BUILD_ID __DATE__ " " __TIME__
#endif

namespace release_metadata {

inline constexpr const char* kVersion = BE_RELEASE_VERSION;
inline constexpr const char* kChannel = BE_RELEASE_CHANNEL;
inline constexpr const char* kBuildId = BE_RELEASE_BUILD_ID;

inline std::string release_identifier() {
  std::string identifier = kVersion;
  if (kChannel[0] != '\0') {
    identifier += "+";
    identifier += kChannel;
  }
  return identifier;
}

inline std::string release_summary() {
  std::string summary = release_identifier();
  summary += " | built ";
  summary += kBuildId;
  return summary;
}

}  // namespace release_metadata