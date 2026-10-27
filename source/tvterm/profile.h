#ifndef TVTERM_PROFILE_H
#define TVTERM_PROFILE_H

// Startup profiles: a set of windows (terminals with their commands, working
// directories and titles, editors with their files, the event viewer) and
// their positions, described in an INI file.
// See 'examples/profiles/example.ini' for the format.

#include <string>
#include <vector>

// What the Event Viewer shows (see evntview.h).
struct EventFilter
{
    bool mouseMove {false};   // M: mouse movements (many, hidden by default)
    bool mouseClicks {true};  // C: mouse buttons, wheel, auto-repeat
    bool keys {true};         // K: key presses
    bool other {true};        // O: other events (commands, ...)
    bool messages {true};     // T: tvterm's own messages
};

struct WindowProfile
{
    enum Kind { Terminal, Editor, EventViewer, LogView };
    Kind kind {Terminal};

    // Terminal (title and command: also Log View).
    std::string title;
    std::string dir;
    std::string command;
    bool keepOpen {true};
    // Log View (see logview.h).
    std::string logInclude;
    std::string logExclude;
    bool logIgnoreCase {false};
    bool logWrap {false};
    bool logFilters {true};
    int logMaxLines {0};
    int logAutoRestart {0};
    // Terminal and Log View alerts (see alerts.h).
    bool alertActivity {false};
    std::string alertKeywords; // Comma-separated.
    int alertSilence {0};      // Seconds; 0: disabled.
    // Editor: file name (empty: untitled).
    std::string file;
    // Event Viewer.
    EventFilter filter;
    // All windows: position and size in percent of the desktop, if 'hasPos'.
    bool hasPos {false};
    double x {0}, y {0}, w {100}, h {100};
};

struct Profile
{
    enum Layout { Auto, Manual, TileColumns, TileRows, Cascade };

    std::string path;
    Layout layout {Auto};
    int focus {0}; // 1-based index of the window to focus; 0: the last one.
    std::vector<WindowProfile> windows;
    // Things changed while loading that are not errors (e.g. a position
    // moved inside the desktop), one per line.
    std::string notes;
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

// Writes 'profile' to 'path'. Returns false on error, with 'error' set.
bool saveProfile(const std::string &path, const Profile &profile, std::string &error);

// Creates 'dir' and its parents if they don't exist.
bool makeDirs(const std::string &dir);

#endif // TVTERM_PROFILE_H
