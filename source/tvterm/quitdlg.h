#ifndef TVTERM_QUITDLG_H
#define TVTERM_QUITDLG_H

class TDeskTop;

// Asks whether to quit, listing what runs in each open terminal.
// The user may also go to one of the terminals instead of quitting.
// Returns true if the application should quit.
bool confirmQuit(TDeskTop &deskTop);

#endif // TVTERM_QUITDLG_H
