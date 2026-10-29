#define Uses_TEditWindow
#define Uses_TFileEditor
#define Uses_TFileDialog
#define Uses_TProgram
#define Uses_TDeskTop
#define Uses_MsgBox
#include <tvision/tv.h>

#include "editwnd.h"
#include "cmds.h"
#include "dircompl.h"

#include <stdarg.h>

TVTermEditWindow::TVTermEditWindow(const TRect &bounds, TStringView fileName) noexcept :
    TWindowInit(&initFrame),
    TEditWindow(bounds, fileName, wnNoNumber)
{
    // Selects the status line shown while the editor is focused.
    helpCtx = hcEditor;
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
            TFileDialog *d = new TVTermFileDialog("*", "Save file as", "~N~ame",
                                                  fdOKButton, 101);
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
            result = messageBox("Search string not found.",
                                mfError | mfOKButton);
            break;
        default:
            // Find/Replace dialogs are not implemented yet.
            break;
    }
    va_end(args);
    return result;
}

void TVTermEditWindow::initEditorDialogs()
{
    TEditor::editorDialog = &editorDialog;
}
