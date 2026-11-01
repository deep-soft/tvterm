#ifndef TVTERM_DESKTOPS_H
#define TVTERM_DESKTOPS_H

#define Uses_TWindow
#define Uses_TDeskTop
#include <tvision/tv.h>

#include <array>

// Desktops 1-9: groups of windows. Only the windows of the current desktop
// are shown; the others are hidden (TView::hide), but they keep running:
// their processes, their alerts and their Log View sources go on. The Event
// Viewer is on every desktop.
//
// The windows of the current desktop are the visible ones, so a new window
// is on the current desktop. Only the hidden windows are recorded (with
// their desktop); a recorded window that is closed is forgotten (prune).
namespace desktops
{

enum { count = 9 };

// The current desktop (1-9).
int current();

// Whether 'w' is on every desktop (the Event Viewer).
bool isGlobal(TWindow *w);

// Whether 'w' is hidden because it is on another desktop.
bool isHidden(TWindow *w);

// The desktop of 'w' (1-9); 0 for the Event Viewer.
int of(TWindow *w);

// Shows desktop 'n' (1-9): hides the windows of the current one and shows
// those of 'n', focusing the window that was focused there last.
void switchTo(TDeskTop &desk, int n);

// Moves 'w' to desktop 'n' (hidden if 'n' is not the current desktop).
// Returns false for the Event Viewer.
bool moveTo(TDeskTop &desk, TWindow *w, int n);

// Focuses 'w', switching first to its desktop if it is hidden.
void selectWindow(TDeskTop &desk, TWindow *w);

// Windows on each desktop (index 1-9; index 0: the Event Viewer).
std::array<int, count + 1> windowCounts(TDeskTop &desk);

// Forgets the windows that were closed.
void prune(TDeskTop &desk);

} // namespace desktops

#endif // TVTERM_DESKTOPS_H
