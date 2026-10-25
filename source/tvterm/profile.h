#ifndef TVTERM_PROFILE_H
#define TVTERM_PROFILE_H

// Startup profiles: a set of terminals with their commands, working
// directories, titles and positions, described in an INI file.
// See 'examples/profiles/example.ini' for the format.

#include <string>
#include <vector>

struct TerminalProfile
{
    std::string title;
    std::string dir;
    std::string command;
    bool keepOpen {true};
    // Position and size in percent of the desktop, if 'hasPos'.
    bool hasPos {false};
    int x {0}, y {0}, w {100}, h {100};
};

struct Profile
{
    enum Layout { Auto, Manual, TileColumns, TileRows, Cascade };

    std::string path;
    Layout layout {Auto};
    int focus {0}; // 1-based index of the terminal to focus; 0: the last one.
    std::vector<TerminalProfile> terminals;
};

// Directory where profiles are looked up by name:
// $XDG_CONFIG_HOME/tvterm/profiles, or ~/.config/tvterm/profiles.
std::string profileDir();

// 'nameOrPath' is either a path (if it contains '/') or a profile name,
// looked up as '<profileDir>/<name>.ini'.
// Returns false if the file cannot be read. Problems found in the file
// (unknown keys, invalid values) are described in 'warnings', but do not
// prevent the profile from loading.
bool loadProfile(const std::string &nameOrPath, Profile &profile, std::string &warnings);

#endif // TVTERM_PROFILE_H
