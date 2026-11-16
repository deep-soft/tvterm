#ifndef TVTERM_QUITDLG_H
#define TVTERM_QUITDLG_H

class TDeskTop;
class TWindow;

#include <vector>

// Asks whether to quit, listing what runs in each open terminal.
// The user may also go to one of the terminals instead of quitting.
// Returns true if the application should quit.
// With 'closing', the same for closing all the windows (Load Profile):
// the title, the button and the question say "Close" instead of "Quit".
// 'only' (E84, Load Profile with pinned windows kept): only these windows
// are listed; null: all.
bool confirmQuit(TDeskTop &deskTop, bool closing = false, const std::vector<TWindow *> *only = nullptr);

#endif // TVTERM_QUITDLG_H
