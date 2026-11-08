#define Uses_TEditWindow
#define Uses_TFileEditor
#define Uses_TFileDialog
#define Uses_TProgram
#define Uses_TDeskTop
#define Uses_MsgBox
#define Uses_TDialog
#define Uses_TInputLine
#define Uses_TLabel
#define Uses_THistory
#define Uses_TCheckBoxes
#define Uses_TSItem
#define Uses_TButton
#define Uses_TStaticText
#define Uses_TKeys
#define Uses_TFindDialogRec
#define Uses_TReplaceDialogRec
#define Uses_TMenuItem
#define Uses_TSubMenu
#define Uses_TMenu
#include <tvision/tv.h>

#include "editwnd.h"
#include "cmds.h"
#include "dircompl.h"
#include "apputil.h"
#include "fileopen.h"
#include "statline.h"
#include "app.h"
#include "settings.h"

#include <stdarg.h>
#include <string.h>
#include <string>
#include <algorithm>
#include <limits.h>
#include <vector>

namespace editwnd
{

// History lists of the Find / Replace fields (fileopen.h has 100-104,
// logview.cc 76-77).
enum : uchar { hiEditorFind = 105, hiEditorReplace = 106 };

// The option bits shown in the dialogs (tvision's editorFlags also holds
// efBackupFiles and efDoReplace, which the dialogs must keep).
enum : ushort
{
    findBits = efCaseSensitive | efWholeWordsOnly,
    replaceBits = efCaseSensitive | efWholeWordsOnly | efPromptOnReplace | efReplaceAll,
};

// The editor whose window is handling an event (the dialogs below are
// called by it, without a pointer to it), and how many handleEvent calls of
// editor windows are running (a modal box inside one lets broadcasts reach
// the windows again).
static TEditor *currentEditor = nullptr;
static int handleDepth = 0;

// A Replace all run, counted for the status line. Without "Prompt on
// replace", the prompt bit is set for the run ('forcedPrompt') and every
// prompt is answered Yes without a box, so each occurrence is counted.
static struct
{
    bool active {false};
    int depth {0};          // handleEvent level that started the run
    bool forcedPrompt {false};
    bool stopped {false};
    int found {0};
    int replaced {0};
} replaceRun;

static void showStatus(const std::string &text)
{
    TVTermStatusLine::showMessage(text);
}

static std::string quoted(const char *s)
{
    return "\"" + std::string(s) + "\"";
}

// The selected text of the current editor, when it is on one line and fits
// in the field; otherwise empty.
static std::string selectedText()
{
    TEditor *e = currentEditor;
    if (!e || !e->hasSelection())
        return {};
    uint a = e->selStart, b = e->selEnd;
    if (b - a >= (uint) maxFindStrLen)
        return {};
    std::string s;
    for (uint i = a; i < b; ++i)
    {
        char c = e->bufChar(i);
        if (c == '\n' || c == '\r')
            return {};
        s += c;
    }
    return s;
}

static void copyText(char *dest, size_t size, const std::string &text)
{
    snprintf(dest, size, "%s", text.c_str());
}

static TDialog *newDialog(int width, int height, const char *title)
{
    auto *d = new TDialog(TRect(0, 0, width, height), title);
    d->options |= ofCentered;
    return d;
}

// A field with its label and history list; returns the field.
static TInputLine *insertField(TDialog *d, int y, const char *label, uchar historyId)
{
    auto *input = new TInputLine(TRect(3, y + 1, 52, y + 2), maxFindStrLen);
    d->insert(input);
    d->insert(new TLabel(TRect(2, y, 30, y + 1), label, input));
    d->insert(new THistory(TRect(52, y + 1, 55, y + 2), input, historyId));
    return input;
}

static ushort execDialog(TDialog *d, void *data)
{
    TView *p = TProgram::application->validView(d);
    if (!p)
        return cmCancel;
    p->setData(data);
    ushort result = TProgram::deskTop->execView(p);
    if (result != cmCancel)
        p->getData(data);
    TObject::destroy(p);
    return result;
}

// Find: Text to find, Case sensitive, Whole words only.
static ushort findDialog(TFindDialogRec &rec)
{
    struct
    {
        char find[maxFindStrLen];
        ushort options;
    } data {};
    std::string sel = selectedText();
    copyText(data.find, sizeof(data.find), sel.empty() ? rec.find : sel);
    data.options = rec.options & findBits;

    TDialog *d = newDialog(58, 12, "Find");
    insertField(d, 2, "~T~ext to find", hiEditorFind);
    d->insert(new TCheckBoxes(TRect(3, 5, 30, 7),
        new TSItem("~C~ase sensitive",
        new TSItem("~W~hole words only", nullptr))));
    d->insert(new TStaticText(TRect(3, 7, 56, 8),
        "From the cursor on. Next: F3, Shift-F7 or Ctrl-L."));
    d->insert(new TButton(TRect(15, 9, 27, 11), "~F~ind", cmOK, bfDefault));
    d->insert(new TButton(TRect(31, 9, 43, 11), "Cancel", cmCancel, bfNormal));
    d->selectNext(False);
    if (execDialog(d, &data) == cmCancel || !data.find[0])
        return cmCancel;
    historyAdd(hiEditorFind, data.find);
    copyText(rec.find, sizeof(rec.find), data.find);
    // Replace all is not kept: TEditor::find() would repeat the search to
    // the last occurrence.
    rec.options = (rec.options & ~(findBits | efReplaceAll)) | (data.options & findBits);
    return cmOK;
}

// Replace: Text to find, New text, Case sensitive, Whole words only,
// Prompt on replace, Replace all.
static ushort replaceDialog(TReplaceDialogRec &rec)
{
    struct
    {
        char find[maxFindStrLen];
        char replace[maxReplaceStrLen];
        ushort options;
    } data {};
    std::string sel = selectedText();
    copyText(data.find, sizeof(data.find), sel.empty() ? rec.find : sel);
    copyText(data.replace, sizeof(data.replace), rec.replace);
    data.options = rec.options & replaceBits;

    TDialog *d = newDialog(58, 16, "Replace");
    insertField(d, 2, "~T~ext to find", hiEditorFind);
    insertField(d, 4, "~N~ew text", hiEditorReplace);
    d->insert(new TCheckBoxes(TRect(3, 7, 30, 11),
        new TSItem("~C~ase sensitive",
        new TSItem("~W~hole words only",
        new TSItem("~P~rompt on replace",
        new TSItem("~R~eplace all", nullptr))))));
    d->insert(new TStaticText(TRect(3, 11, 56, 12),
        "From the cursor on. Again: F3, Shift-F7 or Ctrl-L."));
    d->insert(new TButton(TRect(14, 13, 28, 15), "R~e~place", cmOK, bfDefault));
    d->insert(new TButton(TRect(31, 13, 43, 15), "Cancel", cmCancel, bfNormal));
    d->selectNext(False);
    if (execDialog(d, &data) == cmCancel || !data.find[0])
        return cmCancel;
    historyAdd(hiEditorFind, data.find);
    if (data.replace[0])
        historyAdd(hiEditorReplace, data.replace);
    copyText(rec.find, sizeof(rec.find), data.find);
    copyText(rec.replace, sizeof(rec.replace), data.replace);
    rec.options = (rec.options & ~replaceBits) | (data.options & replaceBits);
    return cmOK;
}

// Called before Replace all runs (from the dialog or Search again).
static void startReplaceRun(ushort &flags)
{
    replaceRun = {};
    replaceRun.active = true;
    replaceRun.depth = handleDepth;
    if (!(flags & efPromptOnReplace))
    {
        flags |= efPromptOnReplace;
        replaceRun.forcedPrompt = true;
    }
}

// "Replace this occurrence?", away from the cursor line ('cursor': global
// coordinates of the cursor, on the occurrence).
static ushort replacePrompt(const TPoint &cursor)
{
    if (replaceRun.active)
    {
        ++replaceRun.found;
        if (replaceRun.forcedPrompt)
        {
            ++replaceRun.replaced;
            return cmYes;
        }
    }
    TRect ext = TProgram::deskTop->getExtent();
    TPoint c = TProgram::deskTop->makeLocal(cursor);
    int w = 40, h = 7;
    int x = ext.a.x + (ext.b.x - ext.a.x - w) / 2;
    // Cursor in the upper half: the box at the bottom; otherwise at the top.
    int y = c.y < (ext.a.y + ext.b.y) / 2 ? ext.b.y - h - 1 : ext.a.y + 1;
    if (y < ext.a.y)
        y = ext.a.y;
    ushort result = messageBoxRect( TRect(x, y, x + w, y + h),
                                    "Replace this occurrence?",
                                    mfConfirmation | mfYesNoCancel );
    if (replaceRun.active)
    {
        if (result == cmYes)
            ++replaceRun.replaced;
        else if (result == cmCancel)
            replaceRun.stopped = true;
    }
    return result;
}

// After a Replace all run: the result in the status line; the prompt bit
// set only for counting is removed again; the window gets the count for
// its undo step. Only at the level that started the run.
static void endReplaceRun(TVTermEditWindow &w)
{
    if (!replaceRun.active || replaceRun.depth != handleDepth)
        return;
    if (replaceRun.replaced > 0)
        w.replaceAllCount = replaceRun.replaced;
    if (replaceRun.forcedPrompt)
        TEditor::editorFlags &= ~efPromptOnReplace;
    std::string what = quoted(TEditor::findStr);
    if (replaceRun.found == 0)
        showStatus("Not found: " + what + ".");
    else if (replaceRun.forcedPrompt)
        showStatus( "Replaced " + std::to_string(replaceRun.replaced) +
                    " occurrence(s) of " + what + "." );
    else
        showStatus( "Replaced " + std::to_string(replaceRun.replaced) + " of " +
                    std::to_string(replaceRun.found) + " occurrence(s) of " + what +
                    (replaceRun.stopped ? " (stopped)." : ".") );
    replaceRun = {};
}

enum { noUndoKey, undoKey, redoKey };

// Undo: Ctrl-Z, Ctrl-U, Alt-Backspace (as in far2l; Ctrl-Backspace still
// deletes the word on the left) and the Undo command (the editor's context
// menu). Redo: Ctrl-Shift-Z (only through far2l's extensions: a plain
// terminal sends it as Ctrl-Z) and Alt-Shift-Backspace. Not while the
// editor waits for the second key of Ctrl-Q / Ctrl-K.
static int undoRedoKey(const TEditor *e, const TEvent &ev)
{
    if (ev.what == evCommand)
        return ev.message.command == cmUndo ? undoKey : noUndoKey;
    if (ev.what != evKeyDown || !e || e->keyState != 0)
        return noUndoKey;
    TKey k(ev.keyDown);
    if (k == TKey(kbCtrlZ, kbShift) || k == TKey(kbAltBack, kbShift))
        return redoKey;
    if (k == TKey(kbCtrlZ) || k == TKey(kbCtrlU) || k == TKey(kbAltBack))
        return undoKey;
    return noUndoKey;
}

// What a key does to the text, for joining the undo steps: a typed
// character, a deleted character or word, or anything else (a step of its
// own: Enter, paste, cut, Ctrl-Y...).
static EditUndo::Kind eventKind(const TEditor *e, const TEvent &ev)
{
    if (ev.what != evKeyDown || !e || e->keyState != 0)
        return EditUndo::ukOther;
    if (ev.keyDown.controlKeyState & kbPaste)
        return EditUndo::ukOther;
    switch (ev.keyDown.keyCode)
    {
        case kbBack: case kbDel: case kbCtrlG:
        case kbCtrlBack: case kbCtrlT: case kbCtrlDel:
            return EditUndo::ukDeleting;
    }
    uchar ch = ev.keyDown.charScan.charCode;
    if (ev.keyDown.textLength > 0 || ch == '\t' || ch >= 32)
        return EditUndo::ukTyping;
    return EditUndo::ukOther;
}

// The keys of Find / Replace / Search again (not while the editor waits
// for the second key of Ctrl-Q / Ctrl-K).
static ushort searchKeyCommand(const TEditor *e, ushort keyCode)
{
    if (e && e->keyState != 0)
        return 0;
    switch (keyCode)
    {
        case kbF7: case kbCtrlF:
            return cmFind;
        case kbCtrlF7: case kbCtrlH:
            return cmReplace;
        case kbF3: case kbShiftF7: case kbCtrlL:
            return cmSearchAgain;
    }
    return 0;
}

// History list of the Go to line field.
enum : uchar { hiEditorGoToLine = 107 };

// A word character for Alt-W, as tvision's editor sees words (double
// click, Ctrl-Left / Ctrl-Right): not a space, tab, line break or
// punctuation. Bytes of UTF-8 characters count as word characters.
static bool isWordByte(char c)
{
    return c != '\0' && !strchr("\t \n\r!\"#$%&'()*+,-./:;<=>?@[\\]^`{|}~", c);
}

// Moves the cursor of 'e' to 'p' (extending the selection when 'extend'),
// scrolling only as much as needed, or centering the cursor line when it
// was not visible.
static void moveCursor(TEditor &e, uint p, bool extend)
{
    e.lock();
    e.setCurPtr(p, extend ? smExtend : 0);
    e.trackCursor(Boolean(!e.cursorVisible()));
    e.unlock();
}

// The character of the line starting at 'line' that covers the visual
// column 'col' (tabs expanded, wide characters two columns): its position
// in 'at'. False when the line is shorter, or the column is a space or tab.
static bool nonSpaceAt(TEditor &e, uint line, int col, uint &at)
{
    uint p = line;
    int pos = 0;
    while (p < e.bufLen)
    {
        char c = e.bufChar(p);
        if (c == '\r' || c == '\n')
            return false;
        uint start = p;
        int startPos = pos;
        if (!e.nextCharAndPos(p, pos) || p == start)
            return false;
        if (startPos <= col && col < pos)
        {
            at = start;
            return c != ' ' && c != '\t';
        }
        if (startPos > col)
            return false;
    }
    return false;
}

// Ctrl-Down / Ctrl-Up: the next / previous line with a non-space
// character in the cursor's column. Returns its position, or false.
static bool findColumnLine(TEditor &e, bool down, uint &target)
{
    int col = e.curPos.x;
    uint line = e.lineStart(e.curPtr);
    while (true)
    {
        uint next;
        if (down)
        {
            uint end = e.lineEnd(line);
            if (end >= e.bufLen)
                return false;
            next = e.nextChar(end);
        }
        else
        {
            if (line == 0)
                return false;
            next = e.lineStart(e.prevChar(line));
        }
        line = next;
        if (nonSpaceAt(e, line, col, target))
            return true;
    }
}

// Alt-W: selects the word under the cursor (or the one just left of it),
// the cursor at its end.
static void selectWord(TEditor &e)
{
    uint p = e.curPtr;
    if (p >= e.bufLen || !isWordByte(e.bufChar(p)))
    {
        uint q = p > 0 ? e.prevChar(p) : p;
        if (q < p && isWordByte(e.bufChar(q)))
            p = q;
        else
        {
            showStatus("No word at the cursor.");
            return;
        }
    }
    uint start = p, end = p;
    while (start > 0)
    {
        uint q = e.prevChar(start);
        if (q >= start || !isWordByte(e.bufChar(q)))
            break;
        start = q;
    }
    while (end < e.bufLen && isWordByte(e.bufChar(end)))
        end = e.nextChar(end);
    e.lock();
    e.setCurPtr(start, 0);
    e.setCurPtr(end, smExtend);
    e.trackCursor(False);
    e.unlock();
}

// The line number (0-based) of position 'p' and the position of a line.
static uint lineIndexOf(TEditor &e, uint p)
{
    uint n = 0;
    for (uint q = e.lineStart(p); q > 0; q = e.lineStart(e.prevChar(q)))
        ++n;
    return n;
}

static uint nextLines(TEditor &e, uint line, uint count)
{
    for (uint i = 0; i < count; ++i)
    {
        uint end = e.lineEnd(line);
        if (end >= e.bufLen)
            break;
        line = e.nextChar(end);
    }
    return line;
}

// Alt-D: the current line, or the lines of the selection, copied below
// them. The cursor (and the selection) go to the same place in the copy.
static void duplicateLines(TEditor &e)
{
    uint selA = e.selStart, selB = e.selEnd;
    bool sel = selA < selB;
    uint first = e.lineStart(sel ? selA : e.curPtr);
    uint lastPtr = sel ? selB : e.curPtr;
    // A selection ending at the start of a line does not take that line.
    if (sel && lastPtr > first && e.lineStart(lastPtr) == lastPtr)
        lastPtr = e.prevChar(lastPtr);
    uint last = e.lineStart(lastPtr);
    uint end = e.lineEnd(last);
    // The places to restore in the copy: line within the block, column.
    struct Mark { uint line; int col; };
    auto markOf = [&] (uint p) {
        uint ls = e.lineStart(p);
        uint n = 0;
        for (uint q = ls; q > first; q = e.lineStart(e.prevChar(q)))
            ++n;
        return Mark {n, e.charPos(ls, p)};
    };
    Mark cur = markOf(e.curPtr);
    Mark a = markOf(sel ? selA : e.curPtr), b = markOf(sel ? selB : e.curPtr);
    bool curAtStart = sel && e.curPtr == selA;
    uint blockLines = markOf(last).line + 1;

    std::string text = "\n";
    for (uint i = first; i < end; ++i)
        text += e.bufChar(i);
    e.lock();
    e.setCurPtr(end, 0);
    if (!e.insertText(text.data(), (uint) text.size(), False))
    {
        e.unlock();
        return;
    }
    uint copy = nextLines(e, first, blockLines);
    auto ptrOf = [&] (const Mark &m) {
        return e.charPtr(nextLines(e, copy, m.line), m.col);
    };
    if (sel)
    {
        uint na = ptrOf(a), nb = ptrOf(b);
        e.setCurPtr(curAtStart ? nb : na, 0);
        e.setCurPtr(curAtStart ? na : nb, smExtend);
    }
    else
        e.setCurPtr(ptrOf(cur), 0);
    e.trackCursor(Boolean(!e.cursorVisible()));
    e.unlock();
}

// Parses "line" or "line:column" (1-based, spaces around allowed). Values
// too large become the largest int.
static bool parseLineColumn(const char *s, long &line, long &col)
{
    auto number = [] (const char *&s, long &v) {
        while (*s == ' ')
            ++s;
        if (*s < '0' || *s > '9')
            return false;
        v = 0;
        while (*s >= '0' && *s <= '9')
        {
            if (v < 1000000000L)
                v = v * 10 + (*s - '0');
            ++s;
        }
        while (*s == ' ')
            ++s;
        return true;
    };
    col = 0;
    if (!number(s, line) || line < 1)
        return false;
    if (*s == ':')
    {
        ++s;
        if (!number(s, col) || col < 1)
            return false;
    }
    return *s == '\0';
}

// Alt-F8: Go to line ("120" or "120:15"); past the end: the last line /
// the end of the line. The line is shown in the middle of the window.
static void goToLine(TEditor &e)
{
    uint curLine = lineIndexOf(e, e.curPtr);
    uint lastLine = curLine;
    for (uint end = e.lineEnd(e.curPtr); end < e.bufLen; end = e.lineEnd(e.nextChar(end)))
        ++lastLine;
    char text[32] = {};
    std::string info = "Now " + std::to_string(curLine + 1) + ":" +
                       std::to_string(e.curPos.x + 1) + ", last line " +
                       std::to_string(lastLine + 1) + ".";
    while (true)
    {
        TDialog *d = newDialog(42, 9, "Go to line");
        auto *input = new TInputLine(TRect(3, 3, 36, 4), sizeof(text));
        d->insert(input);
        d->insert(new TLabel(TRect(2, 2, 36, 3), "~L~ine[:column]", input));
        d->insert(new THistory(TRect(36, 3, 39, 4), input, hiEditorGoToLine));
        d->insert(new TStaticText(TRect(3, 4, 40, 5), info));
        d->insert(new TButton(TRect(8, 6, 18, 8), "~G~o", cmOK, bfDefault));
        d->insert(new TButton(TRect(21, 6, 33, 8), "Cancel", cmCancel, bfNormal));
        d->selectNext(False);
        if (execDialog(d, text) == cmCancel)
            return;
        std::string t = text;
        while (!t.empty() && t.back() == ' ')
            t.pop_back();
        if (t.find_first_not_of(' ') == std::string::npos)
            return;
        long line, col;
        if (parseLineColumn(t.c_str(), line, col))
        {
            historyAdd(hiEditorGoToLine, t.c_str());
            uint target = (uint) std::min<long>(line - 1, lastLine);
            uint p = nextLines(e, 0, target);
            if (col > 0)
                p = e.charPtr(p, (int) std::min<long>(col - 1, INT_MAX));
            e.lock();
            e.setCurPtr(p, 0);
            e.scrollTo( std::max(e.curPos.x - e.size.x + 1, std::min(e.delta.x, e.curPos.x)),
                        e.curPos.y - e.size.y / 2 );
            e.unlock();
            if (line - 1 > (long) lastLine)
                showStatus("Past the end: last line " + std::to_string(lastLine + 1) + ".");
            return;
        }
        messageBox(mfError | mfOKButton,
                   "Invalid line \"%s\": use a line number or line:column, e.g. 120 or 120:15.",
                   t.c_str());
    }
}

// The widest line of the text, in screen columns: a tab goes to the next
// multiple of 8; a character of 3 or 4 bytes (UTF-8) counts as 2 columns
// (one too many for most of the 3-byte ones, which only adds a little
// room at the right). The buffer has a gap at the cursor.
static int widestLine(const TEditor &e)
{
    bool singleByte = e.encoding == TEditor::encSingleByte;
    long long widest = 0, col = 0;
    auto scan = [&] (const char *p, size_t n) {
        for (size_t i = 0; i < n; ++i)
        {
            unsigned char c = (unsigned char) p[i];
            if (c == '\n' || c == '\r')
            {
                widest = std::max(widest, col);
                col = 0;
            }
            else if (c == '\t')
                col = (col | 7) + 1;
            else if (singleByte || c < 0x80 || (c >= 0xC0 && c < 0xE0))
                ++col;
            else if (c >= 0xE0)
                col += 2;
            // 0x80-0xBF: the following bytes of a UTF-8 character.
        }
    };
    if (e.buffer)
    {
        scan(e.buffer, e.curPtr);
        scan(e.buffer + e.curPtr + e.gapLen, e.bufLen - e.curPtr);
    }
    widest = std::max(widest, col);
    return (int) std::min<long long>(widest, INT_MAX / 2);
}

// The open editor windows (to find the window of a file name buffer).
static std::vector<TVTermEditWindow *> openWindows;

// Commands of the window's menu (F9).
enum : ushort { cmEditReadOnly = 3460, cmEditSave, cmEditSaveAs, cmEditFind, cmEditReplace, cmEditGoToLine };

// The commands of tvision's editor that change the text.
static bool isEditingCommand(ushort cmd)
{
    switch (cmd)
    {
        case cmCut: case cmPaste: case cmUndo: case cmClear: case cmNewLine:
        case cmBackSpace: case cmDelChar: case cmDelWord: case cmDelWordLeft:
        case cmDelStart: case cmDelEnd: case cmDelLine: case cmReplace:
            return true;
        case cmSearchAgain:
            // Search again repeats a Replace.
            return (TEditor::editorFlags & efDoReplace) != 0;
    }
    return false;
}

enum EditorKey { ekNone, ekColumnDown, ekColumnUp, ekGoToLine, ekDuplicate, ekSelectWord };

// The keys of 45 (not while the editor waits for the second key of
// Ctrl-Q / Ctrl-K). 'extend': with Shift (Ctrl-Shift-Down / Up).
static EditorKey editorKey(const TEditor *e, const TEvent &ev, bool &extend)
{
    extend = false;
    if (ev.what != evKeyDown || !e || e->keyState != 0)
        return ekNone;
    TKey k(ev.keyDown);
    if (k == TKey(kbDown, kbCtrlShift) || k == TKey(kbDown, kbCtrlShift | kbShift))
    {
        extend = (k.mods & kbShift) != 0;
        return ekColumnDown;
    }
    if (k == TKey(kbUp, kbCtrlShift) || k == TKey(kbUp, kbCtrlShift | kbShift))
    {
        extend = (k.mods & kbShift) != 0;
        return ekColumnUp;
    }
    if (k == TKey(kbAltF8))
        return ekGoToLine;
    if (k == TKey(kbAltD))
        return ekDuplicate;
    if (k == TKey(kbAltW))
        return ekSelectWord;
    return ekNone;
}

} // namespace editwnd

TVTermEditWindow::TVTermEditWindow(const TRect &bounds, TStringView fileName) noexcept :
    TWindowInit(&initFrame),
    TEditWindow(bounds, fileName, wnNoNumber)
{
    // Selects the status line shown while the editor is focused.
    helpCtx = hcEditor;
    if (editor)
        undoList.reset(*editor);
    applyUndoLimits();
    updateWidthLimit();
    editwnd::openWindows.push_back(this);
}

TVTermEditWindow::~TVTermEditWindow()
{
    auto &v = editwnd::openWindows;
    v.erase(std::remove(v.begin(), v.end(), this), v.end());
    // Destroyed inside its own handleEvent (e.g. closed): the rest of
    // handleEvent must not use it.
    if (destroyedFlag)
        *destroyedFlag = true;
}

TVTermEditWindow *TVTermEditWindow::ofFileNameBuffer(const char *fileName)
{
    for (TVTermEditWindow *w : editwnd::openWindows)
        if (w->editor && w->editor->fileName == fileName)
            return w;
    return nullptr;
}

const char *TVTermEditWindow::getTitle(short maxSize)
{
    const char *t = TEditWindow::getTitle(maxSize);
    if (!readOnly)
        return t;
    titleBuf = std::string(t ? t : "") + " [RO]";
    return titleBuf.c_str();
}

void TVTermEditWindow::setReadOnly(bool on)
{
    if (readOnly == on)
        return;
    readOnly = on;
    if (frame)
        frame->drawView();
}

bool TVTermEditWindow::isEditingEvent(const TEvent &ev)
{
    if (!editor)
        return false;
    if (ev.what == evCommand)
        return editwnd::isEditingCommand(ev.message.command);
    if (ev.what != evKeyDown)
        return false;
    bool extend;
    editwnd::EditorKey ek = editwnd::editorKey(editor, ev, extend);
    if (ek != editwnd::ekNone)
        return ek == editwnd::ekDuplicate;
    if (editwnd::undoRedoKey(editor, ev) != editwnd::noUndoKey)
        return true;
    if (ushort cmd = editwnd::searchKeyCommand(editor, ev.keyDown.keyCode))
        return editwnd::isEditingCommand(cmd);
    // As tvision's editor sees the key: converted (Ctrl-K / Ctrl-Q pairs
    // too) on a copy, its pending prefix kept.
    TEvent copy = ev;
    int keyState = editor->keyState;
    editor->convertEvent(copy);
    editor->keyState = keyState;
    if (copy.what == evCommand)
        return editwnd::isEditingCommand(copy.message.command);
    if (copy.what == evKeyDown)
    {
        uchar ch = copy.keyDown.charScan.charCode;
        return copy.keyDown.textLength > 0 || ch == 9 || (ch >= 32 && ch < 255);
    }
    return false;
}

void TVTermEditWindow::openMenu()
{
    using namespace editwnd;
    TMenuItem &items =
        *new TMenuItem("~R~ead only", cmEditReadOnly, kbNoKey, hcNoContext, readOnly ? "on" : "off") +
        newLine() +
        *new TMenuItem("~S~ave", cmEditSave, kbNoKey, hcNoContext, "F2") +
        *new TMenuItem("Save ~a~s...", cmEditSaveAs, kbNoKey, hcNoContext) +
        newLine() +
        *new TMenuItem("~F~ind...", cmEditFind, kbNoKey, hcNoContext, "F7") +
        *new TMenuItem("R~e~place...", cmEditReplace, kbNoKey, hcNoContext, "Ctrl-F7") +
        *new TMenuItem("~G~o to line...", cmEditGoToLine, kbNoKey, hcNoContext, "Alt-F8");
    TPoint where = editor ? editor->makeGlobal(TPoint {0, 0}) : origin;
    ushort cmd = popupMenu(where, items, nullptr);
    switch (cmd)
    {
        case cmEditReadOnly:
            setReadOnly(!readOnly);
            showStatus(readOnly ? "Read only: the text cannot be changed; F2 saves as another file."
                                : "Read only off.");
            break;
        case cmEditSave: message(this, evCommand, cmSave, nullptr); break;
        case cmEditSaveAs: message(this, evCommand, cmSaveAs, nullptr); break;
        case cmEditFind: message(this, evCommand, cmFind, nullptr); break;
        case cmEditReplace: message(this, evCommand, cmReplace, nullptr); break;
        case cmEditGoToLine:
            if (editor)
                goToLine(*editor);
            break;
    }
}

void TVTermEditWindow::updateWidthLimit()
{
    if (!editor)
        return;
    TEditor &e = *editor;
    // One column more than the widest line: the cursor after its end.
    int limit = std::max<int>(maxLineLength, editwnd::widestLine(e) + 1);
    if (limit == e.limit.x)
        return;
    e.limit.x = limit;
    // A shorter text: the view no further right than the new range allows.
    int maxDelta = std::max(0, limit - e.size.x);
    if (e.delta.x > maxDelta)
        e.scrollTo(maxDelta, e.delta.y);
    // The horizontal scroll bar gets the new range.
    e.update(ufView);
}

void TVTermEditWindow::applyUndoLimits()
{
    const Settings &s = TVTermApp::settings();
    EditUndo::Limits limits;
    limits.steps = s.undoLimitSteps;
    limits.memory = s.undoLimitMemory;
    undoList.setLimits(limits);
}

TColorAttr TVTermEditWindow::mapColor(uchar index)
{
    // 7: the selected text of the editor (cpEditor "\x06\x07" in the window
    // palette); the other parts of the window do not use it.
    int attr = TVTermApp::settings().editorSelection;
    if (index == 7 && attr >= 0)
        return TColorAttr(attr);
    return TEditWindow::mapColor(index);
}

bool TVTermEditWindow::undoKeys(TEvent &ev)
{
    int key = editwnd::undoRedoKey(editor, ev);
    if (key == editwnd::noUndoKey)
        return false;
    std::string message;
    bool changed = key == editwnd::undoKey ? undoList.undo(*editor, message)
                                           : undoList.redo(*editor, message);
    if (changed)
        updateWidthLimit();
    if (!message.empty())
        editwnd::showStatus(message);
    clearEvent(ev);
    return true;
}

bool TVTermEditWindow::editorKeys(TEvent &ev)
{
    bool extend;
    editwnd::EditorKey key = editwnd::editorKey(editor, ev, extend);
    if (key == editwnd::ekNone)
        return false;
    clearEvent(ev);
    TEditor &e = *editor;
    switch (key)
    {
        case editwnd::ekColumnDown:
        case editwnd::ekColumnUp:
        {
            uint target;
            if (editwnd::findColumnLine(e, key == editwnd::ekColumnDown, target))
                editwnd::moveCursor(e, target, extend || e.selecting);
            else
                editwnd::showStatus("No other line.");
            break;
        }
        case editwnd::ekGoToLine:
            editwnd::goToLine(e);
            break;
        case editwnd::ekDuplicate:
            editwnd::duplicateLines(e);
            break;
        case editwnd::ekSelectWord:
            editwnd::selectWord(e);
            break;
        default:
            break;
    }
    return true;
}

void TVTermEditWindow::handleEvent(TEvent &ev)
{
    closeKeyToCommand(*this, ev);
    if ( ev.what == evKeyDown && editor && editor->keyState == 0 &&
         TKey(ev.keyDown) == TKey(kbF9) && (state & sfFocused) )
    {
        clearEvent(ev);
        openMenu();
        return;
    }
    if (readOnly && (state & sfFocused))
    {
        // F2: Save As (another file; then the editor is no longer read only).
        if (ev.what == evCommand && ev.message.command == cmSave)
            ev.message.command = cmSaveAs;
        else if (isEditingEvent(ev))
        {
            clearEvent(ev);
            editwnd::showStatus("Read only.");
            return;
        }
    }
    // The undo steps: only for the events that can change the text, not
    // inside a modal box opened by this window (eventDepth > 0), not for
    // Close (the window is gone after it).
    bool track = eventDepth == 0 && editor &&
                 (ev.what & (evKeyDown | evCommand | evMouseDown)) &&
                 !(ev.what == evCommand && ev.message.command == cmClose);
    EditUndo::Kind kind = EditUndo::ukOther;
    bool hadSelection = false;
    if (track)
    {
        // The text changed (from outside before the event, or by it): the
        // horizontal scroll range is taken again.
        if (undoList.before(*editor))
            updateWidthLimit();
        if (undoKeys(ev))
            return;
        kind = editwnd::eventKind(editor, ev);
        hadSelection = editor->hasSelection();
        replaceAllCount = 0;
    }
    if (ev.what == evKeyDown)
        if (ushort cmd = editwnd::searchKeyCommand(editor, ev.keyDown.keyCode))
        {
            ev.what = evCommand;
            ev.message.command = cmd;
            ev.message.infoPtr = nullptr;
        }
    if (ev.what == evCommand && ev.message.command == cmSearchAgain)
    {
        if (!TEditor::findStr[0])
            // Nothing searched yet: Find.
            ev.message.command = cmFind;
        else if ( (TEditor::editorFlags & (efReplaceAll | efDoReplace)) ==
                  (efReplaceAll | efDoReplace) )
        {
            TEditor *saved = editwnd::currentEditor;
            editwnd::currentEditor = editor;
            editwnd::startReplaceRun(TEditor::editorFlags);
            editwnd::currentEditor = saved;
            // Run by the handleEvent call below (one level deeper).
            editwnd::replaceRun.depth = editwnd::handleDepth + 1;
        }
    }
    TEditor *saved = editwnd::currentEditor;
    editwnd::currentEditor = editor;
    ++editwnd::handleDepth;
    bool destroyed = false;
    bool *outerFlag = destroyedFlag;
    destroyedFlag = &destroyed;
    ++eventDepth;
    if (!editorKeys(ev))
        TEditWindow::handleEvent(ev);
    if (!destroyed)
    {
        --eventDepth;
        destroyedFlag = outerFlag;
        editwnd::endReplaceRun(*this);
    }
    else if (outerFlag)
        *outerFlag = true;
    --editwnd::handleDepth;
    editwnd::currentEditor = saved;
    if (!destroyed && track && editor)
        if (undoList.after(*editor, kind, hadSelection, replaceAllCount))
            updateWidthLimit();
}

static ushort editorDialog(int dialog, ...)
{
    va_list args;
    va_start(args, dialog);
    ushort result = cmCancel;
    switch (dialog)
    {
        case edOutOfMemory:
            result = messageBox("Not enough memory for this operation.",
                                mfError | mfOKButton);
            break;
        case edReadError:
            result = messageBox(mfError | mfOKButton,
                                "Error reading file %s.", va_arg(args, char *));
            break;
        case edWriteError:
            result = messageBox(mfError | mfOKButton,
                                "Error writing file %s.", va_arg(args, char *));
            break;
        case edCreateError:
            result = messageBox(mfError | mfOKButton,
                                "Error creating file %s.", va_arg(args, char *));
            break;
        case edSaveModify:
            result = messageBox(mfInformation | mfYesNoCancel,
                                "%s has been modified. Save?", va_arg(args, char *));
            break;
        case edSaveUntitled:
            result = messageBox("Save untitled file?",
                                mfInformation | mfYesNoCancel);
            break;
        case edSaveAs:
        {
            // 'fileName' is a buffer of MAXPATH characters owned by the editor.
            char *fileName = va_arg(args, char *);
            TFileDialog *d = fileopen::newFileDialog("*", "Save file as",
                                                     fdOKButton, fileopen::hiSaveFileAs);
            TView *p = TProgram::application->validView(d);
            if (p)
            {
                p->setData(fileName);
                result = TProgram::deskTop->execView(p);
                if (result != cmCancel)
                    p->getData(fileName);
                TObject::destroy(p);
            }
            if (result != cmCancel)
                if (TVTermEditWindow *w = TVTermEditWindow::ofFileNameBuffer(fileName))
                    w->setReadOnly(false);
            break;
        }
        case edSearchFailed:
            // In the status line, not in a box.
            editwnd::showStatus("Not found: " + editwnd::quoted(TEditor::findStr) + ".");
            result = cmOK;
            break;
        case edFind:
            result = editwnd::findDialog(*va_arg(args, TFindDialogRec *));
            break;
        case edReplace:
        {
            auto &rec = *va_arg(args, TReplaceDialogRec *);
            result = editwnd::replaceDialog(rec);
            if (result != cmCancel && (rec.options & efReplaceAll))
                editwnd::startReplaceRun(rec.options);
            break;
        }
        case edReplacePrompt:
            result = editwnd::replacePrompt(*va_arg(args, TPoint *));
            break;
        default:
            break;
    }
    va_end(args);
    return result;
}

void TVTermEditWindow::initEditorDialogs()
{
    TEditor::editorDialog = &editorDialog;
}
