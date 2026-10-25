#ifndef TVTERM_TERMOPTS_H
#define TVTERM_TERMOPTS_H

#define Uses_TKeys
#include <tvision/tv.h>

#include <atomic>
#include <string>

namespace tvterm
{

class TerminalView;

// Options shared by all terminals. The application sets them (e.g. from
// its settings file); changes apply to the open terminals too.
struct TerminalOptions
{
    // Read by the emulator threads.
    std::atomic<int> scrollbackLines {10000}; // 0: no scrollback.
    std::atomic<int> wheelLines {3};

    // Main thread only.
    // Characters that are part of a word on double click, besides letters
    // and digits (any non-ASCII character counts as a letter).
    std::u32string wordChars {U"-._/@:~+%="};
    // Shift+click selects text even if the application uses the mouse.
    bool shiftClickSelect {true};
    // Keys that enter history mode and scroll at once, outside of
    // full-screen applications. TKey() disables a key.
    TKey keyPageUp {kbPgUp, kbShift | kbAltShift};
    TKey keyPageDown {kbPgDn, kbShift | kbAltShift};
    TKey keyTop {kbHome, kbShift | kbAltShift};
    TKey keyBottom {kbEnd, kbShift | kbAltShift};
    // Default of the 'case sensitive' option when searching.
    bool searchCaseSensitive {false};

    // Callbacks provided by the application (may be null).
    // Shows a short message (e.g. in the status line).
    void (*showMessage)(const std::string &text) {nullptr};
    // Opens the search dialog for 'view' (history mode).
    void (*openSearch)(TerminalView &view) {nullptr};
    // Opens the 'save history' dialog for 'view'.
    void (*openSave)(TerminalView &view) {nullptr};
};

extern TerminalOptions terminalOptions;

} // namespace tvterm

#endif // TVTERM_TERMOPTS_H
