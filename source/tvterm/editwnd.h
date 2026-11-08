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

    // The file name, or "Untitled"; " [RO]" added while read only.
    const char *getTitle(short maxSize) override;

    // Read only (E63): toggled from the window's menu (F9). The keys and
    // commands that change the text do nothing ("Read only." in the status
    // line); F2 opens Save As, and after Save As the editor is no longer
    // read only. Lines from Log View are not added to it. Profiles keep it
    // ('read-only = yes', editors with a file).
    bool isReadOnly() const noexcept { return readOnly; }
    void setReadOnly(bool on);

    // Backups (E64): before the first save of this window to 'path' (or,
    // with 'always', before Save As over an existing file), the file on disk
    // is copied to name-YYYY-MM-DD-hhmmss (local time; -2, -3, ... in the
    // same second) and the oldest backups over file-backups-number are
    // deleted (the file itself keeps its inode, mode and owner: tvision then
    // writes over it). No backup when the file does not exist or with
    // file-backups = no. If the backup cannot be made, asks whether to save
    // anyway: false = do not save.
    bool backupBeforeSave(const std::string &path, bool always = false);

    // The editor window whose editor has the file name buffer 'fileName'
    // (the dialogs of tvision's editor get only that buffer), or null.
    static TVTermEditWindow *ofFileNameBuffer(const char *fileName);

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

    bool readOnly {false};
    std::string titleBuf;
    // The file whose backups this window made (or that it wrote first by
    // Save As): no more backups for it from this window.
    std::string backedUpFile;

    // F9: the window's menu.
    void openMenu();
    // Whether 'ev' would change the text (for read only).
    bool isEditingEvent(const TEvent &ev);

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
