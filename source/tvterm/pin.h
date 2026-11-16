#ifndef TVTERM_PIN_H
#define TVTERM_PIN_H

// Pin (E84): a pinned Log View or terminal is protected from being closed
// by mistake. Ctrl-W, the close button, Ctrl-B Close Term and Close in the
// Window List refuse it, with a message in the status line and a note in
// the Event Viewer; Load Profile asks whether to keep the pinned windows.
// Everything else is unchanged: moving, resizing, zoom, Tile / Cascade,
// renaming, the filters, moving to another Desk, Quit, and the end of the
// program (a terminal closes as before, a Log View keeps its lines).
// Shown as " [P]" in the title and as P in the Window List and the Log
// Views table; saved in profiles as 'pinned = yes'.
//
// Toggled by: Log View F6 (key-logview-pin) and F9 > Pin / Unpin; Ctrl-B
// Windows > Pin / Unpin for the focused Log View or terminal.

class TView;
class TWindow;
struct TEvent;

#include <string>

namespace pin
{

// A Log View (not the Help window) or a terminal.
bool canPin(TView *view);
bool isPinned(TView *view);
// Pins / unpins 'view' (status message). False if it cannot be pinned.
bool toggle(TView *view);
bool set(TView *view, bool on);
// If 'ev' is a close command for 'window' (cmClose for it or for the
// focused window) and the window is pinned: the message, the note, the
// event cleared, and true.
bool refuseClose(TWindow &window, TEvent &ev);
// The title used in messages ("Log: ..." or the terminal's title).
std::string titleOf(TView *view);

} // namespace pin

#endif // TVTERM_PIN_H
