#define Uses_TDialog
#define Uses_TListViewer
#define Uses_TScrollBar
#define Uses_TButton
#define Uses_TLabel
#define Uses_TDeskTop
#define Uses_TProgram
#define Uses_TEvent
#define Uses_MsgBox
#define Uses_TEditWindow
#include <tvision/tv.h>

#include "winlist.h"
#include "wnd.h"
#include "evntview.h"
#include "logview.h"

#include <string.h>
#include <string>

namespace
{

enum : ushort
{
    cmWinListRename = 3100,
    cmWinListClose,
};

class WindowListViewer : public TListViewer
{
public:

    std::vector<std::string> items;

    WindowListViewer(const TRect &bounds, TScrollBar *vScrollBar) noexcept :
        TListViewer(bounds, 1, nullptr, vScrollBar)
    {
    }

    void setItems(std::vector<std::string> aItems)
    {
        items = std::move(aItems);
        setRange((short) items.size());
        drawView();
    }

    void getText(char *dest, short item, short maxChars) override
    {
        if (item >= 0 && (size_t) item < items.size())
        {
            strncpy(dest, items[item].c_str(), maxChars);
            dest[maxChars] = '\0';
        }
        else
            *dest = '\0';
    }
};

class WindowListDialog : public TDialog
{
public:

    WindowListViewer *list;

    WindowListDialog(const TRect &bounds) noexcept :
        TWindowInit(&initFrame),
        TDialog(bounds, "Windows")
    {
        options |= ofCentered;
        TRect r = getExtent().grow(-2, -2);
        r.b.x -= 13;
        auto *sb = new TScrollBar(TRect(r.b.x, r.a.y, r.b.x + 1, r.b.y));
        insert(sb);
        list = new WindowListViewer(r, sb);
        insert(list);
        insert(new TLabel(TRect(r.a.x, r.a.y - 1, r.a.x + 10, r.a.y), "~W~indows", list));
        int x = size.x - 12, y = 2;
        insert(new TButton(TRect(x, y, x + 10, y + 2), "~G~o", cmOK, bfDefault)); y += 2;
        insert(new TButton(TRect(x, y, x + 10, y + 2), "~R~ename", cmWinListRename, bfNormal)); y += 2;
        insert(new TButton(TRect(x, y, x + 10, y + 2), "~C~lose", cmWinListClose, bfNormal)); y += 2;
        insert(new TButton(TRect(x, y, x + 10, y + 2), "Cancel", cmCancel, bfNormal));
        list->select();
    }

    void handleEvent(TEvent &ev) override
    {
        TDialog::handleEvent(ev);
        if ( ev.what == evCommand &&
             (ev.message.command == cmWinListRename || ev.message.command == cmWinListClose) )
        {
            endModal(ev.message.command);
            clearEvent(ev);
        }
        else if (ev.what == evBroadcast && ev.message.command == cmListItemSelected)
        {
            // Enter or double click on an item.
            endModal(cmOK);
            clearEvent(ev);
        }
    }
};

std::string describe(TWindow *w)
{
    const char *kind = dynamic_cast<TerminalWindow *>(w) ? "Term  " :
                       dynamic_cast<TEditWindow *>(w) ? "Editor" :
                       dynamic_cast<TEventViewer *>(w) ? "Events" :
                       dynamic_cast<LogViewWindow *>(w) ? "Log   " :
                                                         "Window";
    const char *title = w->getTitle(100);
    std::string s = kind;
    s += "  ";
    s += title && *title ? title : "(no title)";
    return s;
}

} // namespace

std::vector<TWindow *> getDeskTopWindows(TDeskTop &deskTop)
{
    std::vector<TWindow *> windows;
    // TGroup::first() is the frontmost view.
    if (TView *v = deskTop.first())
        do {
            if (auto *w = dynamic_cast<TWindow *>(v))
                if (w->state & sfVisible)
                    windows.push_back(w);
            v = v->next;
        } while (v != deskTop.first());
    return windows;
}

bool renameTerminal(TWindow *window)
{
    if (auto *log = dynamic_cast<LogViewWindow *>(window))
    {
        char buf[256];
        snprintf(buf, sizeof(buf), "%s", log->getOptions().title.c_str());
        if (inputBox("Rename Log View", "~T~itle (empty: the command)", buf, 100) != cmCancel)
            log->setCustomTitle(buf);
        return true;
    }
    auto *term = dynamic_cast<TerminalWindow *>(window);
    if (!term)
        return false;
    char buf[256];
    TStringView current = term->getCustomTitle();
    size_t len = current.size() < sizeof(buf) - 1 ? current.size() : sizeof(buf) - 1;
    memcpy(buf, current.data(), len);
    buf[len] = '\0';
    if (inputBox("Rename terminal", "~T~itle (empty: automatic)", buf, 100) != cmCancel)
        term->setCustomTitle(buf);
    return true;
}

void execWindowList(TDeskTop &deskTop)
{
    short focused = 0;
    while (true)
    {
        std::vector<TWindow *> windows = getDeskTopWindows(deskTop);
        if (windows.empty())
        {
            messageBox("There are no windows.", mfInformation | mfOKButton);
            return;
        }
        std::vector<std::string> items;
        for (TWindow *w : windows)
            items.push_back(describe(w));

        TRect r(0, 0, 66, 18);
        if (r.b.x > deskTop.size.x) r.b.x = deskTop.size.x;
        if (r.b.y > deskTop.size.y) r.b.y = deskTop.size.y;
        auto *d = new WindowListDialog(r);
        d->list->setItems(std::move(items));
        if (focused >= (short) windows.size())
            focused = (short) windows.size() - 1;
        d->list->focusItem(focused);
        TView *p = TProgram::application->validView(d);
        if (!p)
            return;
        ushort result = deskTop.execView(p);
        focused = d->list->focused;
        TObject::destroy(p);

        if (focused < 0 || (size_t) focused >= windows.size())
            return;
        TWindow *w = windows[focused];
        switch (result)
        {
            case cmOK:
                w->select();
                return;
            case cmWinListRename:
                if (!renameTerminal(w))
                    messageBox("Only terminals and Log Views can be renamed.", mfInformation | mfOKButton);
                break; // Show the list again.
            case cmWinListClose:
            {
                TEvent ev;
                ev.what = evCommand;
                ev.message.command = cmClose;
                ev.message.infoPtr = nullptr;
                w->handleEvent(ev); // Asks for confirmation if needed.
                break;
            }
            default:
                return;
        }
    }
}
