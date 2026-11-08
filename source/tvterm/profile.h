#ifndef TVTERM_PROFILE_H
#define TVTERM_PROFILE_H

// Startup profiles: a set of windows (terminals with their commands, working
// directories and titles, editors with their files, the event viewer) and
// their positions, described in an INI file.
// See 'examples/profiles/example.ini' for the format.

#include <map>
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
    std::string logIncludeAnd;  // include-and (E61): words joined by " && ".
    std::string logExclude;
    bool logIgnoreCase {false};
    bool logWrap {false};
    bool logFilters {true};
    int logMaxLines {0};
    int logAutoRestart {0};
    std::string logHighlight;   // Highlight sets (see highlight.h).
    bool logHighlightOn {true};
    bool logHighlightWordsMode {false}; // highlight-mode = words.
    std::string logHighlightWords;      // Comma-separated (see splitHighlightWords).
    std::string logTransform;   // Transformation sets (see transform.h).
    bool logTransformOn {true};
    // Terminal and Log View alerts (see alerts.h).
    bool alertActivity {false};
    std::string alertKeywords; // Comma-separated.
    int alertSilence {0};      // Seconds; 0: disabled.
    // Editor: file name (empty: untitled).
    std::string file;
    bool readOnly {false};      // [editor] read-only (E63).
    // Event Viewer.
    EventFilter filter;
    // All windows: position and size in percent of the desktop, if 'hasPos'.
    bool hasPos {false};
    double x {0}, y {0}, w {100}, h {100};
    // Desktop (1-9; see desktops.h). Not for the Event Viewer, which is on
    // every desktop.
    int desktop {1};
};

struct Profile
{
    // Columns / Rows: 'layout = columns N' / 'rows N' (layoutCount = N).
    enum Layout { Auto, Manual, TileColumns, TileRows, Cascade, Columns, Rows };

    std::string path;
    Layout layout {Auto};
    int layoutCount {0}; // N of Columns / Rows (1-20).
    int focus {0}; // 1-based index of the window to focus; 0: the last one.
    int desktop {1}; // The desktop shown after loading (1-9).
    // desktop-main (E62): written by hand, never set from the program;
    // 1-9: the desktop shown after loading, instead of 'desktop' (the focus:
    // the 'focus' window if it is there, else that desktop's front window).
    // 0: as without it. Kept by Save Profile As.
    bool hasDesktopMain {false};
    int desktopMain {0};
    // desktop-name-N: names of desktops (1-9); only those in the file.
    std::map<int, std::string> desktopNames;
    // window-list-size = WxH (the Window List, see winlist.h).
    bool hasWindowListSize {false};
    int windowListWidth {0}, windowListHeight {0};
    // window-list-columns = A D N [*]: widths of the Window List columns
    // Alert, Desk, Name (a number, 0 = hidden, or 'auto' = -1); Command
    // gets the rest.
    bool hasWindowListColumns {false};
    int windowListColumns[3] {-1, -1, -1};
    std::vector<WindowProfile> windows;
    // Things changed while loading that are not errors (e.g. a position
    // moved inside the desktop), one per line.
    std::string notes;
};

// window-list-columns: parses / writes the value ("auto auto 30 *").
bool parseWindowListColumns(const std::string &value, int (&widths)[3]);
std::string formatWindowListColumns(const int (&widths)[3]);

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
