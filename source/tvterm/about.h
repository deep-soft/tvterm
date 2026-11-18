#ifndef TVTERM_ABOUT_H
#define TVTERM_ABOUT_H

#include <string>

// Build information, generated at build time (see scripts/buildinfo.cmake).
namespace buildinfo
{

extern const char version[];  // e.g. "0.1.0"
extern const char commit[];   // short hash, '-dirty' if modified, or "unknown"
extern const char branch[];
extern const char date[];     // "YYYY-MM-DD hh:mm TZ", local time of the build
extern const char kind[];     // "static" or "dynamic"
extern const char type[];     // CMAKE_BUILD_TYPE: "MinSizeRel", "RelWithDebInfo"...; may be empty
extern const char arch[];     // like 'uname -m': "x86_64", "armv7l", "aarch64"
extern const char tvision[];  // commit of the bundled Turbo Vision
extern const char libvterm[]; // commit of the bundled libvterm

} // namespace buildinfo

// Two short lines, for the Event Viewer:
// "tvterm 0.1.0 (deep-soft), far2l-extended da593f7"
// "built 2026-10-25 16:40 EET, static, x86_64, musl"
std::string versionLine();
std::string buildLine();

// The C library the binary was built with: "glibc 2.28" (version of the
// headers), "musl", "uClibc", "bionic"; empty if not known.
std::string libcName();

// The build type (E93): "release (MinSizeRel)", "debug info (RelWithDebInfo)",
// "debug (Debug)", another CMAKE_BUILD_TYPE as it is; empty if none.
std::string buildTypeText();

// "2026-10-25 16:40 EET, static, release (MinSizeRel), x86_64, musl": date,
// kind, build type (E93), architecture and C library of the build.
std::string buildDescription();

// Several lines, for 'tvterm --version'.
std::string versionText();

// What the About dialog shows besides the build information (known only
// to the running application).
struct AboutInfo
{
    std::string settingsPath;
    bool settingsExists {false};
    std::string profile;          // last profile loaded, empty if none
    long long uptimeSeconds {0};
    size_t terminals {0};         // terminal windows, running or not
};

// Returns true when the dialog was closed with Keys... (E77: the Help).
bool showAbout(const AboutInfo &info);

#endif // TVTERM_ABOUT_H
