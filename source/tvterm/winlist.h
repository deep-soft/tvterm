#ifndef TVTERM_WINLIST_H
#define TVTERM_WINLIST_H

#define Uses_TWindow
#include <tvision/tv.h>

#include <vector>

// Shows the list of windows on the desktop and lets the user go to one of
// them, rename a terminal or close a window.
class TDeskTop;
void execWindowList(TDeskTop &deskTop);

// Asks for a new title for 'window', if it is a terminal or a Log View.
// Returns false otherwise.
bool renameTerminal(TWindow *window);

// Windows on the desktop, from front to back.
std::vector<TWindow *> getDeskTopWindows(TDeskTop &deskTop);

#endif // TVTERM_WINLIST_H
