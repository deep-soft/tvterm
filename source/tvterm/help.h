#ifndef TVTERM_HELP_H
#define TVTERM_HELP_H

// Help (E77, item 31): F1 in a window that is not a terminal (Log View,
// Viewer, editor, Event Viewer), More... > Keys and the Keys button of
// About open a list of the keys of every kind of window, in a Log View with
// fixed lines (title "Help: tvterm keys"; F3 / '/' search, Ctrl-C copy...).
// The text is made when the window opens, from the tables of help.cc and
// from the keys set in tvterm.ini ([keys]: menu-key, key-zoom,
// key-window-*, history-*, key-editor-*, ...), so a changed key is shown as
// it is set.
//
// Sections, in this order: Global, Terminal, Log View, Viewer, Editor,
// Event Viewer. The section of the window that was active is the current
// one: it is shown at the top of the window, its title in yellow and the
// cursor line on its first key; with [general] help-current-first = yes it
// also comes first in the text. One Help
// window at a time: opening it again closes the old one.

#define Uses_TWindow
#define Uses_TRect
#include <tvision/tv.h>

#include <string>

namespace help
{

// The sections (also the kind of window the help is opened from).
enum Section { Global, Terminal, LogView, Viewer, Editor, EventViewer, None };

// The section that describes 'w' (None for a dialog, no window...).
Section sectionOf(TWindow *w);

// The text of the help, one line per entry; 'current' tells which section
// title is marked (its line number, 0-based, in 'markLine'; -1: none).
std::string text(Section current, bool currentFirst, int &markLine);

// The bounds of the Help window (E102, item 71): those of the Event Viewer
// (on every Desk), so that it opens over it; when the viewer is maximized,
// its bounds before. Without an Event Viewer: where Ctrl-B V would open one.
TRect bounds();

// Opens the Help window (closing an open one), for the window 'from' (may
// be null), with bounds().
void open(TWindow *from);

// Whether 'w' is the Help window.
bool isHelpWindow(TWindow *w);

} // namespace help

#endif // TVTERM_HELP_H
