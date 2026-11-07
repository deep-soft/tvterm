#ifndef TVTERM_WINLIST_H
#define TVTERM_WINLIST_H

#define Uses_TWindow
#define Uses_TPoint
#include <tvision/tv.h>

#include <string>
#include <vector>

// Shows the list of windows of every desktop (columns: Alert, Desk, Name,
// Command, with their names above the list) and lets the user go to one of them, rename a terminal or close
// a window. The dialog can be resized; its size is kept (see
// getWindowListSize).
class TDeskTop;
void execWindowList(TDeskTop &deskTop);

// Asks for a new title for 'window', if it is a terminal or a Log View.
// Returns false otherwise.
bool renameTerminal(TWindow *window);

// Windows on the desktop, from front to back, with those hidden on the
// other desktops (see desktops.h).
std::vector<TWindow *> getDeskTopWindows(TDeskTop &deskTop);

// The same windows, as the lists show them: by desktop (1-9, then the
// Event Viewer), and on each desktop in the order they were opened.
std::vector<TWindow *> getSortedWindows(TDeskTop &deskTop);

// The desktop column of the lists: "*D1" (the current desktop), " D2",
// " 2:mail" (a named desktop), " all" (the Event Viewer).
std::string desktopColumn(TWindow *w);

// The size of the Window List chosen by the user (window-list-size in
// profiles). Returns false if none was chosen (the default is used).
bool getWindowListSize(TPoint &size);
void setWindowListSize(TPoint size);

// The widths of the columns Alert, Desk and Name (window-list-columns in
// profiles): a number of columns (0: hidden) or -1 (automatic); Command
// gets the rest. Returns false if none were set (all automatic).
bool getWindowListColumns(int (&widths)[3]);
void setWindowListColumns(const int (&widths)[3]);

#endif // TVTERM_WINLIST_H
