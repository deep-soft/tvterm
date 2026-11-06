#ifndef TVTERM_EDITWND_H
#define TVTERM_EDITWND_H

#define Uses_TEditWindow
#include <tvision/tv.h>

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

    // Ctrl-W closes the window (asks to save the changes). Find / Replace
    // keys: F7 and Ctrl-F (Find), Ctrl-F7 and Ctrl-H (Replace), F3,
    // Shift-F7 and Ctrl-L (Search again); the dialogs are in editwnd.cc.
    void handleEvent(TEvent &ev) override;

    // The selected text of the editor in the color of 'editor-selection'
    // (tvterm.ini), when it is set.
    TColorAttr mapColor(uchar index) override;

    // Must be called once at startup: installs the dialogs used by the
    // editor (Save As, Find, Replace, 'file modified', errors...).
    static void initEditorDialogs();

    // Undo of the last Replace all (Ctrl-U / Alt-Backspace right after it):
    // the text before it, the text after it and the cursor.
    struct ReplaceUndo
    {
        bool valid {false};
        std::string before, after;
        uint cursor {0};
        int count {0};
    };
    ReplaceUndo replaceUndo;

private:

    bool undoReplaceAll();
};

#endif // TVTERM_EDITWND_H
