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
    std::string before;     // the text before the run (for the undo)
    uint cursor {0};
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

// The whole text of an editor (its buffer has a gap at the cursor).
static std::string editorText(const TEditor *e)
{
    std::string s;
    s.reserve(e->bufLen);
    s.append(e->buffer, e->curPtr);
    s.append(e->buffer + e->curPtr + e->gapLen, e->bufLen - e->curPtr);
    return s;
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
    if (currentEditor)
    {
        replaceRun.before = editorText(currentEditor);
        replaceRun.cursor = currentEditor->curPtr;
    }
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
// set only for counting is removed again; the window keeps what its undo
// needs. Only at the level that started the run.
static void endReplaceRun(TVTermEditWindow &w)
{
    if (!replaceRun.active || replaceRun.depth != handleDepth)
        return;
    if (replaceRun.replaced > 0 && w.editor)
    {
        w.replaceUndo.valid = true;
        w.replaceUndo.before = std::move(replaceRun.before);
        w.replaceUndo.after = editorText(w.editor);
        w.replaceUndo.cursor = replaceRun.cursor;
        w.replaceUndo.count = replaceRun.replaced;
    }
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

// Ctrl-U or Alt-Backspace (Undo), not inside Ctrl-Q / Ctrl-K.
static bool isUndo(const TEditor *e, const TEvent &ev)
{
    if (ev.what == evCommand)
        return ev.message.command == cmUndo;
    return ev.what == evKeyDown && e && e->keyState == 0 &&
           (ev.keyDown.keyCode == kbCtrlU || ev.keyDown.keyCode == kbAltBack);
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

} // namespace editwnd

TVTermEditWindow::TVTermEditWindow(const TRect &bounds, TStringView fileName) noexcept :
    TWindowInit(&initFrame),
    TEditWindow(bounds, fileName, wnNoNumber)
{
    // Selects the status line shown while the editor is focused.
    helpCtx = hcEditor;
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

bool TVTermEditWindow::undoReplaceAll()
// The text from before the last Replace all, if nothing changed after it.
{
    if (!replaceUndo.valid || !editor)
        return false;
    replaceUndo.valid = false;
    if (editwnd::editorText(editor) != replaceUndo.after)
        return false;
    editor->lock();
    editor->setSelect(0, editor->bufLen, False);
    editor->insertText(replaceUndo.before.data(), (uint) replaceUndo.before.size(), False);
    editor->setCurPtr(std::min<uint>(replaceUndo.cursor, editor->bufLen), 0);
    editor->trackCursor(True);
    editor->unlock();
    editwnd::showStatus( "Undone: Replace all (" + std::to_string(replaceUndo.count) +
                         " replacement(s))." );
    replaceUndo = {};
    return true;
}

void TVTermEditWindow::handleEvent(TEvent &ev)
{
    closeKeyToCommand(*this, ev);
    if (editwnd::isUndo(editor, ev))
    {
        if (undoReplaceAll())
        {
            clearEvent(ev);
            return;
        }
        if (ev.what == evKeyDown && ev.keyDown.keyCode == kbAltBack)
        {
            // As in far2l: Alt-Backspace = Undo (Ctrl-Backspace still
            // deletes the word on the left).
            ev.what = evCommand;
            ev.message.command = cmUndo;
            ev.message.infoPtr = nullptr;
        }
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
    TEditWindow::handleEvent(ev);
    editwnd::endReplaceRun(*this);
    --editwnd::handleDepth;
    editwnd::currentEditor = saved;
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
