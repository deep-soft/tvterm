#ifndef TVTERM_DLGCLIP_H
#define TVTERM_DLGCLIP_H

// Clipboard in the text fields (input lines) of dialogs.
//
// TInputLine only uses the clipboard through the cmPaste / cmCopy commands,
// which nothing sends in tvterm's dialogs, so Shift-Ins and Ctrl-V did
// nothing there. The terminals already reach the system clipboard (e.g. the
// Windows clipboard through far2l's extensions or OSC 52) with
// TClipboard::requestText() / setText(); the same is done here for the
// focused input line:
//
//   Shift-Ins, Ctrl-V   paste
//   Ctrl-Ins, Ctrl-C    copy the selected text (only when there is one)
//
// Pasted text (also from the outer terminal, e.g. PuTTY's right click) is
// cleaned up before it is inserted: see 'sanitize'.

#define Uses_TEvent
#define Uses_TApplication
#define Uses_TInputLine
#include <tvision/tv.h>

#include <string>

namespace dlgclip
{

// The input line that has the focus, or null (e.g. a terminal, a button).
TInputLine *focusedInputLine() noexcept;

// Called by the application for every key event. 'app' is used to read the
// rest of a paste. Returns true if the event was handled (and cleared).
bool handleKeyEvent(TApplication &app, TEvent &ev) noexcept;

// Pasted text -> text for a single-line field: valid UTF-8 only; line
// breaks, tabs and runs of spaces become one space; spaces at both ends are
// removed; control, bidirectional and zero-width characters are removed.
// Only the first 'maxInputBytes' bytes are looked at, and the result is at
// most 'maxOutputBytes' long (cut at a character boundary).
enum : size_t { maxInputBytes = 64 * 1024 };
std::string sanitize(TStringView text, size_t maxOutputBytes) noexcept;

// Inserts 'text' (valid UTF-8) at the cursor of 'line', replacing the
// selection, character by character, until a character does not fit.
void insertText(TInputLine &line, const std::string &text) noexcept;

} // namespace dlgclip

#endif // TVTERM_DLGCLIP_H
