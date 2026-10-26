#ifndef TVTERM_HISTDLG_H
#define TVTERM_HISTDLG_H

// Dialogs of the terminal history mode (Ctrl-B H): searching and saving
// the scrollback.

#include <functional>
#include <string>

namespace tvterm { class TerminalView; }

void openHistorySearch(tvterm::TerminalView &view);
void openHistorySave(tvterm::TerminalView &view);

// The same Save dialog, for other windows (Log View). 'name' is part of the
// proposed file name ('tvterm-<name>-YYYY-MM-DD-hhmm.txt', in 'save-dir' or
// in the current directory). 'getText(selectionOnly)' returns the text.
void openTextSave( const char *dialogTitle, const std::string &name, bool hasSelection,
                   const std::function<std::string(bool)> &getText );

#endif // TVTERM_HISTDLG_H
