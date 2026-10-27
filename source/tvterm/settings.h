#ifndef TVTERM_SETTINGS_H
#define TVTERM_SETTINGS_H

// Global settings: ~/.config/tvterm/tvterm.ini (or $XDG_CONFIG_HOME/...).
// Read at startup and again whenever the file changes (or on demand).
// See 'examples/tvterm.ini' for the format.

#define Uses_TKeys
#include <tvision/tv.h>

#include <string>
#include <time.h>

struct Settings
{
    // [general]
    std::string profile;    // Profile opened when tvterm starts without -p.
    bool autoReload {true}; // Reload the file when it changes.
    // [scrollback]
    int lines {10000};
    int wheelLines {3};
    std::string wordChars {"-._/@:~+%="};
    std::string saveDir;    // Empty: the terminal's current directory.
    bool saveDateFirst {false}; // save-name: date-first (true) or name-first.
    bool searchCaseSensitive {false};
    bool shiftClickSelect {true};
    // [logview]
    int logViewMaxLines {50000};
    // [keys]
    TKey keyPageUp {kbPgUp, kbShift | kbAltShift};
    TKey keyPageDown {kbPgDn, kbShift | kbAltShift};
    TKey keyTop {kbHome, kbShift | kbAltShift};
    TKey keyBottom {kbEnd, kbShift | kbAltShift};
};

// Path of the settings file.
std::string settingsPath();

// Reads 'path' into 'settings'. Keys missing from the file get their
// default values; invalid values keep the value they had in 'settings'.
// Returns false if the file does not exist or cannot be read ('settings'
// then gets the default values). Problems are described in 'warnings',
// one per line.
bool loadSettings(const std::string &path, Settings &settings, std::string &warnings);

// Makes the settings effective (terminal options).
void applySettings(const Settings &settings);

// Contents of a new settings file: every key, commented out, with its
// default value.
std::string settingsTemplate();

// Key names, e.g. "Alt-Shift-PgUp". 'parseKey' accepts "none" (TKey()).
bool parseKey(const std::string &text, TKey &key);
std::string keyName(TKey key);

// Detects changes to the settings file (modification time and size).
struct FileStamp
{
    bool exists {false};
    time_t mtime {0};
    long mtimeNsec {0};
    long long size {0};

    static FileStamp of(const std::string &path);
    bool operator!=(const FileStamp &o) const;
};

#endif // TVTERM_SETTINGS_H
