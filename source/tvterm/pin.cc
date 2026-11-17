#define Uses_TWindow
#define Uses_TEvent
#include <tvision/tv.h>

#include "pin.h"
#include "app.h"
#include "globalkeys.h"
#include "logview.h"
#include "editwnd.h"
#include "settings.h"
#include "statline.h"
#include "wnd.h"

namespace pin
{

static LogViewWindow *asLogView(TView *view)
{
    auto *log = dynamic_cast<LogViewWindow *>(view);
    return log && !log->isHelpView() ? log : nullptr;
}

bool canPin(TView *view)
{
    return asLogView(view) || dynamic_cast<TerminalWindow *>(view) || dynamic_cast<TVTermEditWindow *>(view);
}

bool isPinned(TView *view)
{
    if (auto *log = asLogView(view))
        return log->isPinned();
    if (auto *term = dynamic_cast<TerminalWindow *>(view))
        return term->isPinned();
    if (auto *edit = dynamic_cast<TVTermEditWindow *>(view))
        return edit->isPinned();
    return false;
}

std::string titleOf(TView *view)
{
    auto *w = dynamic_cast<TWindow *>(view);
    if (auto *log = asLogView(view))
        return log->messageTitle();
    if (auto *term = dynamic_cast<TerminalWindow *>(view))
        return term->messageTitle();
    if (auto *edit = dynamic_cast<TVTermEditWindow *>(view))
        return std::string("Editor: ") + (edit->editor && *edit->editor->fileName ? edit->editor->fileName : "Untitled");
    const char *t = w ? w->getTitle(100) : nullptr;
    return t && *t ? t : "(no title)";
}

bool set(TView *view, bool on)
{
    if (auto *log = asLogView(view))
        log->setPinned(on);
    else if (auto *term = dynamic_cast<TerminalWindow *>(view))
        term->setPinned(on);
    else if (auto *edit = dynamic_cast<TVTermEditWindow *>(view))
        edit->setPinned(on);
    else
        return false;
    return true;
}

bool toggle(TView *view)
{
    if (!canPin(view))
    {
        TVTermStatusLine::showMessage("Only Log Views, terminals and editors can be pinned.");
        return false;
    }
    bool on = !isPinned(view);
    set(view, on);
    TVTermStatusLine::showMessage( on ? "Pinned: " + titleOf(view) + " (not closed by Ctrl-W or Close)." :
                                        "Unpinned: " + titleOf(view) + "." );
    return true;
}

bool refuseClose(TWindow &window, TEvent &ev)
{
    if ( ev.what != evCommand || ev.message.command != cmClose ||
         (ev.message.infoPtr && ev.message.infoPtr != &window) ||
         !isPinned(&window) )
        return false;
    std::string text;
    if (asLogView(&window))
        text = "Log View is pinned (F9 > Unpin)";
    else if (dynamic_cast<TVTermEditWindow *>(&window))
        text = "Editor is pinned (F9 > Unpin)";
    else
        text = "Term is pinned (" + globalkeys::menuKeyName() + " Z P to unpin)";
    TVTermStatusLine::showMessage(text + ".");
    TVTermApp::note("not closed: " + titleOf(&window) + ": " + text);
    window.clearEvent(ev);
    return true;
}

} // namespace pin
