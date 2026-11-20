#ifndef TVTERM_EDITWND_H
#define TVTERM_EDITWND_H

#define Uses_TEditWindow
#include <tvision/tv.h>

#include "editundo.h"

#include <string>
#include <vector>

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
    // Alt-D (Duplicate line), Alt-W (Select word) and key-editor-comment
    // (Alt-C, E66: a comment line with the date and time at the cursor).
    // F9 opens the window's menu; F12 is Save As and Ctrl-F12 Open (E65).
    void handleEvent(TEvent &ev) override;

    // Open (E65, F9 > Open..., Ctrl-F12): the Open file dialog (the first
    // 'file-filters' pattern, the same lists as Ctrl-B O), listing the
    // folder of the window's file; the chosen file is loaded into this
    // window (a large file asks first as in Ctrl-B O; a file open in
    // another window opens anyway). A modified text asks "... has been
    // modified. Save?" first: Yes saves as F2 does (Untitled: Save As),
    // No drops the changes, Cancel keeps everything. After it: the new
    // name in the title, undo emptied, read only off, the first save makes
    // a backup.
    void openFile();

    // Reload (E65, F9 > Reload): reads the window's file from disk again
    // ("... has been modified. Discard changes?" when modified; nothing
    // for Untitled, or when the file is gone or cannot be read). The cursor
    // stays on its line and column (the last line when the text is
    // shorter); undo emptied.
    void reloadFromMenu();

    // Reads the file from disk again, keeping the cursor as described
    // above. False on a read error (the editor's box was shown, the
    // editor is empty).
    virtual bool reloadFile();

    // Whitespace view (E83, key-editor-whitespace Ctrl-Space, F9 >
    // Whitespace): spaces, tabs and line ends shown with the symbols of
    // [editor] whitespace-symbols in whitespace-color; only the drawing
    // changes. Starts as [editor] show-whitespace; profiles keep it
    // ('whitespace = yes'). Allowed in read only.
    // E103 (item 44): with [editor] whitespace-color-2 the key cycles off ->
    // whitespace-color -> whitespace-color-2 -> off; the level is 0 (off),
    // 1 or 2 (the second color; [WS2], 'whitespace = 2' in profiles).
    bool showsWhitespace() const noexcept { return whitespace > 0; }
    int whitespaceLevel() const noexcept { return whitespace; }
    void setWhitespace(bool on) { setWhitespaceLevel(on ? 1 : 0); }
    // Level 2 without whitespace-color-2: level 1.
    void setWhitespaceLevel(int level);

    // Line numbers (E108, item 53; key-editor-line-numbers Alt-L, F9 > Line
    // numbers): a column at the left of the text with the number of each
    // line, right-aligned in [editor] line-numbers-width digits (more when
    // the text has more lines) and a space; the current line's number in
    // line-numbers-current-color. A click on a number moves the cursor to
    // the start of that line. Starts as [editor] line-numbers; profiles keep
    // it ('line-numbers = yes'). Allowed in read only.
    bool showsLineNumbers() const noexcept { return gutter != nullptr; }
    void setLineNumbers(bool on);
    // The column's width follows the number of lines (and the settings);
    // drawn again. Called after the text changes and when the settings are
    // reloaded.
    void updateLineNumbers();
    // The number shown for the editor's line 'line' (0-based): line + 1;
    // 0 = no number. E109: in the view without comments, the number of the
    // line in the whole text (0 for an added blank line).
    long numberOfLine(int line) const;

    // Hide comments (E109, item 63; key-editor-hide-comments Alt-H, F9 >
    // Hide comments): the editor shows the text without comment lines ('#'
    // or ';' after spaces) and blank lines, keeping the '##' block at the
    // start and adding a blank line before each [section] (nocomments.h).
    // The view is made from the whole text at each switch; the whole text
    // is kept here meanwhile, the editor's 'modified' mark and undo steps
    // stay as they were. The view is read only: typing, F2, F12, Ctrl-F12,
    // Reload and lines from Log View are refused ("Comments hidden: Alt-H
    // to edit."). The line numbers (E108), the indicator and Alt-F8 use the
    // lines of the whole text; the cursor stays on its line both ways (the
    // next shown line when it was on a hidden one). Not kept in profiles.
    bool hidesComments() const noexcept { return hidden.on; }
    void setCommentsHidden(bool on);
    // The line of the whole text (0-based) of the editor's line 'line': the
    // line itself outside the view; in it, an added blank line gives the
    // next shown line of the text.
    long originalLineOf(int line) const;
    // The whole text (the view's text is not it).
    std::string fullText() const;

    // Closing / quitting with changes asks "Save?" and saves the editor's
    // text: the whole text is put back first (E109).
    Boolean valid(ushort command) override;

    // The file name, or "Untitled"; " [RO]" added while read only, " [WS]"
    // while the whitespace view is on (" [WS2]": second color), " [P]" while
    // pinned.
    const char *getTitle(short maxSize) override;

    // Pin (E86, item 54 b; pin.h): F9 > Pin / Unpin, Ctrl-B Z P. A pinned
    // editor refuses Ctrl-W, the close button and Window List > Close
    // before the "Save?" question. Profiles keep it ('pinned = yes').
    bool isPinned() const noexcept { return pinned; }
    void setPinned(bool on);

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
    bool pinned {false};
    // The whitespace view (kept here too: during the window's shutDown the
    // editor is already gone while broadcasts still reach the window).
    int whitespace {0};
    std::string titleBuf;
    // The line numbers' column (E108), null while they are hidden.
    TView *gutter {nullptr};
    // The view without comments (E109): the whole text and, for each line
    // of the view, its line in the whole text (-1: an added blank line).
    struct HiddenView
    {
        bool on {false};
        std::string full;
        std::vector<long> lines;
        long originalLines {1};
    } hidden;
    // The file whose backups this window made (or that it wrote first by
    // Save As): no more backups for it from this window.
    std::string backedUpFile;

    // F9: the window's menu.
    void openMenu();
    // The whitespace view on / off (or the next level, E103), with its
    // message in the status line; the text of its F9 item ("off,
    // Ctrl-Space").
    void toggleWhitespace();
    std::string whitespaceItemText() const;
    // The line numbers on / off with the message (E108); the text of the F9
    // item; the column's width (digits and the space).
    void toggleLineNumbers();
    std::string lineNumbersItemText() const;
    int lineNumbersWidth() const;
    // E109: the view on / off with the message; the F9 item's text; the
    // message when something would change the view's text.
    void toggleHiddenComments();
    std::string hiddenItemText() const;
    void refuseInHiddenView();
    // The "modified" question before Open: false = do not go on.
    bool saveChangesOrDrop();
    // Loads 'path' into the editor (openFile's last step).
    void loadIntoWindow(const std::string &path);
    // Whether 'ev' would change the text (for read only).
    bool isEditingEvent(const TEvent &ev);

    // handleEvent calls running (a modal box opened inside one lets
    // broadcasts reach the window again).
    int eventDepth {0};
    // Set when the window is destroyed inside its handleEvent.
    bool *destroyedFlag {nullptr};

    bool undoKeys(TEvent &ev);
    // Ctrl-Down / Ctrl-Up (and with Shift), Alt-F8 Go to line, Alt-D
    // Duplicate line, Alt-W Select word, the comment key (editwnd.cc).
    bool editorKeys(TEvent &ev);
};

#endif // TVTERM_EDITWND_H
