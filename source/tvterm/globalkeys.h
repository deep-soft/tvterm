#ifndef TVTERM_GLOBALKEYS_H
#define TVTERM_GLOBALKEYS_H

// Global keys (tvterm.ini, [keys]): the key that opens the menu, Zoom and
// moving to the neighbouring window. They work in every window, before the
// window sees the key, but not while a dialog, a menu or Grab Input is
// active.

#define Uses_TKeys
#define Uses_TEvent
#include <tvision/tv.h>

#include <string>
#include <vector>

struct GlobalKey
{
    TKey key;               // Normalized key (unused for the backquote).
    bool backquote {false}; // The key left of 1 (scan code 41), any layout.
    bool notFar2l {false};  // ':not-far2l': not taken while far2l's
                            // extensions are active in the focused terminal.

    bool sameKey(const GlobalKey &o) const;
    bool matches(const KeyDownEvent &ev) const;
    // E.g. "Ctrl-B", "Ctrl-`", "Alt-Shift-Left", "F12".
    std::string name() const;
};

enum GlobalAction
{
    gaMenu, gaZoom, gaWindowLeft, gaWindowRight, gaWindowUp, gaWindowDown,
    gaCount
};

namespace globalkeys
{

// Setting names, in the order of precedence (see 'removeDuplicates').
extern const char *const settingNames[gaCount];

// Index of the setting called 'name' (lower case), or -1.
int actionOf(const std::string &name);

// Parses a comma-separated list, e.g. "ctrl-b, ctrl-`" or
// "f12:not-far2l". Valid keys are appended to 'keys'; each problem is
// added to 'problems' (without the setting name). "none" or an empty value
// gives no keys.
void parseList(const std::string &value, std::vector<GlobalKey> &keys,
               std::vector<std::string> &problems);

// A key used by more than one setting (or twice in one) is kept only for
// the first one, in the order of 'settingNames'. Problems are described.
void removeDuplicates(std::vector<GlobalKey> (&keys)[gaCount],
                      std::vector<std::string> &problems);

// The default menu key (Ctrl-B).
GlobalKey defaultMenuKey();

// Name of the first menu key, shown in the status line and in messages
// (e.g. "Ctrl-B"). Set by the application when the settings are read.
const std::string &menuKeyName();
void setMenuKeyName(const std::string &name);

} // namespace globalkeys

#endif // TVTERM_GLOBALKEYS_H
