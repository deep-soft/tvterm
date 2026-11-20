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
#include "alerts.h"

#include <string>
#include <vector>
#include <time.h>

// The smallest file dialog (TFileDialog::sizeLimits).
const int minFileDialogWidth = 49;
const int minFileDialogHeight = 19;

// Format of tvterm.ini written by this tvterm: the number in the template's
// first line ('## format 7, created by tvterm v0.3.0 (...)'). It goes up by
// one when a version adds, renames or changes the meaning of keys, so that
// an older file can be recognized (a note in the Event Viewer; the file is
// never rewritten). 1: tvterm 0.2.0 (E53); 2: file-backups and
// file-backups-number (E64); 3: comment-template, comment-width and
// key-editor-comment (E66); 4: [alerts] action, action-interval, log,
// notify, notify-repeat (E68); 5: [capture] folder (E71), [alerts]
// mute-above (E72); 6: [general] auto-reload-check, desktop-pattern,
// desktop-pattern-on-capture (E74); 7: [general] help-current-first (E77),
// [keys] key-logviews-end (E78), key-editor-indent, key-editor-unindent (E79),
// [logview] cursor-color, selection-color (E80); 8: [editor]
// show-whitespace, whitespace-symbols, whitespace-color, [keys]
// key-editor-whitespace (E83); 9: [keys] key-logview-pin (E84); 10:
// [editor] whitespace-color-2 (E103), [keys] key-eventviewer-mute (E106),
// key-editor-pin (E107); 11: [editor] line-numbers, line-numbers-width,
// line-numbers-color, line-numbers-current-color, [keys]
// key-editor-line-numbers (E108), key-editor-hide-comments (E109).
const int settingsFormat = 11;

struct Settings
{
    // The format line of the file read ('## format N, ...' before the first
    // section); -1: the file has none (written before tvterm 0.2.0).
    int fileFormat {-1};
    // [general]
    std::string profile;    // Profile opened when tvterm starts without -p.
    bool autoReload {true}; // Reload the file when it changes.
    // auto-reload-check: seconds between the checks of the file (auto-reload
    // and the Settings editor), 1-60; 2 while the Settings editor is open.
    int autoReloadCheck {2};
    // reload-message: "Settings reloaded." in the status line (status) or
    // nothing (none) after an automatic reload.
    bool reloadMessage {true};
    // Files larger than this (bytes) ask before they open in tvterm (Save >
    // Open after saving, Ctrl-B O, Ctrl-B F View file); 0: never ask.
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
    // help-current-first (E77): the Help window shows the section of the
    // window it was opened from first (yes) or the sections in their fixed
    // order, that one at the top of the window (no).
    bool helpCurrentFirst {false};
    // desktop-pattern: the character of the desktop background (one
    // character of one column, UTF-8; 'space' in the file: a space).
    // desktop-pattern-on-capture: the one Capture Desk writes there (E74);
    // empty: 'same', the desktop-pattern.
    std::string desktopPattern {"\u2591"};
    std::string desktopPatternOnCapture {" "};
    // editor-selection: color of the selected text in the editors (also the
    // text found by Find), a BIOS attribute; -1: tvision's color.
    int editorSelection {-1};
    // undo-limit-steps / undo-limit-memory: the undo steps kept per editor
    // (the oldest are dropped over a limit); memory in bytes, 0: no limit.
    int undoLimitSteps {500};
    long long undoLimitMemory {20LL * 1024 * 1024};
    // file-backups: a copy name-YYYY-MM-DD-hhmmss at the first save of an
    // editor window (no: none); file-backups-number: the backups kept per
    // file (1 to 99), the oldest deleted over it.
    bool fileBackups {true};
    int fileBackupsNumber {5};
    // comment-template / comment-width: the line inserted by
    // key-editor-comment in an editor (E66): the template through strftime
    // (local time), filled with '-' up to comment-width columns (0: not
    // filled). The default makes "# 2026-11-10 10:06:43 ---...", 79 columns.
    std::string commentTemplate {"# %Y-%m-%d %H:%M:%S "};
    int commentWidth {79};
    // [editor] (E83): the whitespace view of the editors.
    // show-whitespace: its state in a new editor window.
    bool showWhitespace {false};
    // whitespace-symbols: Space, Tab, LF, CRLF, lone CR; each one character
    // of one column (UTF-8).
    std::vector<std::string> whitespaceSymbols {"\u00B7", "\u2192", "\u00B6", "\u00A4", "\u2190"};
    // whitespace-color: a BIOS attribute; with whitespaceColorFore only its
    // foreground is used (the editor's background stays).
    int whitespaceColor {0x08};
    bool whitespaceColorFore {true};
    // whitespace-color-2 (E103): the second color of the symbols (as
    // whitespace-color); -1: not set (the key turns the view on / off).
    int whitespaceColor2 {-1};
    bool whitespaceColor2Fore {true};
    // [editor] line numbers (E108, item 53). line-numbers: the state in a
    // new editor window; line-numbers-width: the digits of the column (1-9,
    // more when the text has more lines; a space always follows);
    // line-numbers-color / line-numbers-current-color: as whitespace-color
    // (Fore: only the foreground, on the editor's background).
    bool lineNumbers {false};
    int lineNumbersWidth {4};
    int lineNumbersColor {0x03};
    bool lineNumbersColorFore {true};
    int lineNumbersCurrentColor {0x0F};
    bool lineNumbersCurrentColorFore {true};
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
    // action, action-interval, log, notify, notify-repeat (E68).
    AlertConfig alertConfig;
    // [capture] folder: where Capture Desk / Window write their files (E71;
    // '~' allowed; empty: the default).
    std::string captureFolder {"~/logs/tvterm/capture"};
    // [logview]
    int logViewMaxLines {50000};
    // builtin-highlight: the highlight sets built into tvterm (no: only the
    // [highlight] sections of tvterm.ini).
    bool builtinHighlight {true};
    // cursor-color / selection-color (E80): the cursor line (focused
    // window) and the selected lines of the Log Views (and of the Event
    // Viewer, E81), BIOS attributes (black on cyan, black on white).
    uint8_t cursorColor {0x30};
    uint8_t selectionColor {0x70};
    // [highlight <name>]: highlight sets for Log View (see highlight.h).
    std::vector<HighlightSection> highlight;
    // [transform <name>]: transformation sets for Log View (see transform.h).
    std::vector<HighlightSection> transform;
    // [keys]
    TKey keyPageUp {kbPgUp, kbShift | kbAltShift};
    TKey keyPageDown {kbPgDn, kbShift | kbAltShift};
    TKey keyTop {kbHome, kbShift | kbAltShift};
    TKey keyBottom {kbEnd, kbShift | kbAltShift};
    // key-editor-comment: inserts the comment line in an editor (E66).
    TKey keyEditorComment {'C', kbAltShift};
    // key-logviews-end (E78): in the Event Viewer, every Log View to its
    // last line (More... > Scroll Log Views to last).
    TKey keyLogViewsEnd {kbCtrlL};
    // key-editor-indent / key-editor-unindent (E79): one space added at /
    // removed from the start of the current or selected lines of an editor.
    TKey keyEditorIndent {'I', kbAltShift};
    TKey keyEditorUnindent {'U', kbAltShift};
    // key-editor-whitespace (E83): the whitespace view of an editor on/off.
    TKey keyEditorWhitespace {' ', kbCtrlShift};
    // key-logview-pin (E84): Pin / Unpin of a Log View (see pin.h).
    TKey keyLogViewPin {kbF6};
    // key-eventviewer-mute (E106): in the Event Viewer, More... > Mute alerts
    // above level... (Ctrl-M: through far2l's extensions it differs from
    // Enter; in a plain terminal it is Enter, so only the F9 item works).
    TKey keyEventViewerMute {'M', kbCtrlShift};
    // key-editor-pin (E107): Pin / Unpin of an editor window (as F9 > Pin).
    TKey keyEditorPin {kbF6};
    // key-editor-line-numbers (E108): the line numbers of an editor on/off.
    TKey keyEditorLineNumbers {'L', kbAltShift};
    // key-editor-hide-comments (E109): the view without comments on/off.
    TKey keyEditorHideComments {'H', kbAltShift};
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
// default value. The first line is the format line (settingsFormat, the
// version, commit and build date of this tvterm). Layout: the header with
// '##', sections at the margin,
// explanations indented by 2 spaces ('  # ...'), keys by 1 (' #key = ...'),
// so that a key with its '#' removed is indented by 1 space.
std::string settingsTemplate();

// Sizes as in 'large-file': bytes, or a number with K, M or G (1024).
bool parseSize(const std::string &text, long long &bytes);

// The number of a format line ('## format 2, created by ...', spaces allowed
// before and after the '#'s) in 'format'; false if 'line' is not one.
bool parseFormatLine(const std::string &line, int &format);
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
