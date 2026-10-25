#ifndef TVTERM_HISTDLG_H
#define TVTERM_HISTDLG_H

// Dialogs of the terminal history mode (Ctrl-B H): searching and saving
// the scrollback.

namespace tvterm { class TerminalView; }

void openHistorySearch(tvterm::TerminalView &view);
void openHistorySave(tvterm::TerminalView &view);

#endif // TVTERM_HISTDLG_H
