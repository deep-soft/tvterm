#ifndef TVTERM_SETTINGS_H
#define TVTERM_SETTINGS_H

// Global settings: ~/.config/tvterm/tvterm.ini (or $XDG_CONFIG_HOME/...).
// Read at startup and again whenever the file changes (or on demand).
// See 'examples/tvterm.ini' for the format.

#define Uses_TKeys
#include <tvision/tv.h>

#include "highlight.h"
#include "transform.h"
#include "globalkeys.h"

#include <string>
#include <vector>
#include <time.h>

// The smallest file dialog (TFileDialog::sizeLimits).
const int minFileDialogWidth = 49;
const int minFileDialogHeight = 19;

struct Settings
{
    // [general]
    std::string profile;    // Profile opened when tvterm starts without -p.
    bool autoReload {true}; // Reload the file when it changes.
    // reload-message: "Settings reloaded." in the status line (status) or
    // nothing (none) after an automatic reload.
    bool reloadMessage {true};
    // Files larger than this (bytes) ask before they open in tvterm (Save >
    // Open after saving, Ctrl-B O, More... > View file); 0: never ask.
    long long largeFile {5LL * 1024 * 1024};
    // Programs for files ('%f': the path, quoted for the shell); empty: none.
    std::string externalViewer;
    std::string externalEditor;
    // Patterns offered in the Name field of the file dialogs (its history);
    // the first one is the initial filter of Ctrl-B O and View file.
    std::vector<std::string> fileFilters {"*.txt", "*.ini", "*.log", "*"};
    // file-dirs: directories offered in the Folder list of the file
    // dialogs, after Home, Profiles and Settings (as written; '~' allowed).
    std::vector<std::string> fileDirs;
    // Size of the file dialogs when they open (columns x rows); 0: the size
    // chosen by tvision. Made smaller to fit the screen.
    int fileDialogWidth {0};
    int fileDialogHeight {0};
    // Columns of the list of files in the file dialogs (1 or 2).
    int fileListColumns {2};
    // window-shadow: the shadow of the desktop windows (terminals, Log View,
    // editors, Event Viewer); menus and dialogs always keep theirs.
    bool windowShadow {false};
    // editor-selection: color of the selected text in the editors (also the
    // text found by Find), a BIOS attribute; -1: tvision's color.
    int editorSelection {-1};
    // undo-limit-steps / undo-limit-memory: the undo steps kept per editor
    // (the oldest are dropped over a limit); memory in bytes, 0: no limit.
    int undoLimitSteps {500};
    long long undoLimitMemory {20LL * 1024 * 1024};
    // [scrollback]
    int lines {10000};
    int wheelLines {3};
    std::string wordChars {"-._/@:~+%="};
    std::string saveDir;    // Empty: the terminal's current directory.
    bool saveDateFirst {false}; // save-name: date-first (true) or name-first.
    bool searchCaseSensitive {false};
    bool shiftClickSelect {true};
    // [alerts]
    // title-color: the title text of a window with an alert, white on red
    // (red) or in the normal colors (none).
    bool alertTitleRed {true};
    // seen-color: the title text of a window that got the focus while it
    // had an alert, until it loses the focus: yellow, cyan or none (0).
    uint8_t alertSeenColor {0xE0};
    // [logview]
    int logViewMaxLines {50000};
    // builtin-highlight: the highlight sets built into tvterm (no: only the
    // [highlight] sections of tvterm.ini).
    bool builtinHighlight {true};
    // [highlight <name>]: highlight sets for Log View (see highlight.h).
    std::vector<HighlightSection> highlight;
    // [transform <name>]: transformation sets for Log View (see transform.h).
    std::vector<HighlightSection> transform;
    // [keys]
    TKey keyPageUp {kbPgUp, kbShift | kbAltShift};
    TKey keyPageDown {kbPgDn, kbShift | kbAltShift};
    TKey keyTop {kbHome, kbShift | kbAltShift};
    TKey keyBottom {kbEnd, kbShift | kbAltShift};
    // Global keys (menu-key, key-zoom, key-window-*), see globalkeys.h.
    std::vector<GlobalKey> globalKeys[gaCount] {{globalkeys::defaultMenuKey()}};
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
// default value. Layout: the header with '##', sections at the margin,
// explanations indented by 2 spaces ('  # ...'), keys by 1 (' #key = ...'),
// so that a key with its '#' removed is indented by 1 space.
std::string settingsTemplate();

// Sizes as in 'large-file': bytes, or a number with K, M or G (1024).
bool parseSize(const std::string &text, long long &bytes);
std::string sizeName(long long bytes);

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
