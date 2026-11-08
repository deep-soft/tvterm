#ifndef TVTERM_EDITWND_H
#define TVTERM_EDITWND_H

#define Uses_TEditWindow
#include <tvision/tv.h>

#include "editundo.h"

#include <string>

// A simple text editor window, meant for notes and as an intermediate
// buffer for the clipboard.
//
// The editor has no private clipboard, so Cut/Copy/Paste use the system
// clipboard. When running over SSH from putty4far2l (or inside far2l),
// that is the Windows clipboard.

class TVTermEditWindow : public TEditWindow
{
public:

    TVTermEditWindow(const TRect &bounds, TStringView fileName) noexcept;
    ~TVTermEditWindow();

    // Ctrl-W closes the window (asks to save the changes). Find / Replace
    // keys: F7 and Ctrl-F (Find), Ctrl-F7 and Ctrl-H (Replace), F3,
    // Shift-F7 and Ctrl-L (Search again); the dialogs are in editwnd.cc.
    // Also Ctrl-Down / Ctrl-Up (next / previous line with text in the
    // cursor's column; with Shift the selection grows), Alt-F8 (Go to line),
    // Alt-D (Duplicate line) and Alt-W (Select word).
    void handleEvent(TEvent &ev) override;

    // The selected text of the editor in the color of 'editor-selection'
    // (tvterm.ini), when it is set.
    TColorAttr mapColor(uchar index) override;

    // Must be called once at startup: installs the dialogs used by the
    // editor (Save As, Find, Replace, 'file modified', errors...).
    static void initEditorDialogs();

    // Multi-level undo / redo (editundo.h). Undo: Ctrl-Z, Ctrl-U,
    // Alt-Backspace; Redo: Ctrl-Shift-Z, Alt-Shift-Backspace.
    EditUndo undoList;

    // [general] undo-limit-steps / undo-limit-memory of tvterm.ini (also
    // called when the settings are reloaded).
    void applyUndoLimits();

    // Replacements made by the Replace all of the current event (set by
    // the editor dialogs, editwnd.cc).
    int replaceAllCount {0};

    // The horizontal scroll range: the widest line of the text (at least
    // 256 columns, tvision's maxLineLength). tvision fixes it at 256, so on
    // a screen of 256 columns or more the editor could not scroll right.
    // Called when the window opens and after every change of the text made
    // by an event of the window (typing, paste, Replace all, undo...);
    // code that changes the text from outside (Log View lines) calls it too.
    void updateWidthLimit();

private:

    // handleEvent calls running (a modal box opened inside one lets
    // broadcasts reach the window again).
    int eventDepth {0};
    // Set when the window is destroyed inside its handleEvent.
    bool *destroyedFlag {nullptr};

    bool undoKeys(TEvent &ev);
    // Ctrl-Down / Ctrl-Up (and with Shift), Alt-F8 Go to line, Alt-D
    // Duplicate line, Alt-W Select word (editwnd.cc).
    bool editorKeys(TEvent &ev);
};

#endif // TVTERM_EDITWND_H
