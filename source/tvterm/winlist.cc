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
#define Uses_TText
#define Uses_TKeys
#include <tvision/tv.h>

#include "winlist.h"
#include "wnd.h"
#include "evntview.h"
#include "logview.h"
#include "desktops.h"
#include "procinfo.h"
#include <tvterm/termview.h>
#include <tvterm/termctrl.h>

#include <string.h>
#include <algorithm>
#include <string>

namespace winlist
{

enum : ushort
{
    cmWinListRename = 3100,
    cmWinListClose,
};

// One line of the list: three columns.
struct Row
{
    std::string desktop, title, command;
};

// Cuts 'text' to at most 'width' columns ('…' at the end if it was cut)
// and pads it with spaces to 'width' columns.
static std::string fitColumn(const std::string &text, int width)
{
    if (width < 1)
        return std::string();
    std::string s = text;
    if ((int) TText::width(s) > width)
        s = s.substr(0, TText::scroll(s, width - 1, False)) + "\u2026";
    int w = (int) TText::width(s);
    return w < width ? s + std::string(width - w, ' ') : s;
}

class WindowListViewer : public TListViewer
{
public:

    std::vector<Row> rows;
    int desktopWidth {3};

    WindowListViewer(const TRect &bounds, TScrollBar *vScrollBar) noexcept :
        TListViewer(bounds, 1, nullptr, vScrollBar)
    {
    }

    void setRows(std::vector<Row> aRows)
    {
        rows = std::move(aRows);
        desktopWidth = 3;
        for (const Row &r : rows)
            desktopWidth = std::max(desktopWidth, (int) TText::width(r.desktop));
        setRange((short) rows.size());
        drawView();
    }

    // The columns follow the width of the list: the desktop as wide as its
    // longest label, the title about half of the rest, the command the
    // remainder.
    void getText(char *dest, short item, short maxChars) override
    {
        std::string text;
        if (item >= 0 && (size_t) item < rows.size())
        {
            const Row &r = rows[item];
            // The text starts one column in.
            int rest = size.x - 1 - desktopWidth - 4;
            int titleWidth = rest / 2;
            int commandWidth = rest - titleWidth;
            text = fitColumn(r.desktop, desktopWidth) + "  " +
                   fitColumn(r.title, titleWidth) + "  " +
                   fitColumn(r.command, commandWidth);
        }
        strncpy(dest, text.c_str(), maxChars);
        dest[maxChars] = '\0';
    }
};

// Size chosen by the user (mouse or Ctrl-F5), kept while tvterm runs and in
// profiles (window-list-size).
static bool sized = false;
static TPoint rememberedSize {0, 0};

const TPoint defaultSize {90, 20};
const TPoint minSize {50, 10};
const int buttonsWidth = 12; // Buttons on the right, with their margin.

class WindowListDialog : public TDialog
{
public:

    WindowListViewer *list;
    bool autoFit {false}; // the next changeBounds is not the user's choice
    bool inZoom {false};

    WindowListDialog(const TRect &bounds) noexcept :
        TWindowInit(&initFrame),
        TDialog(bounds, "Windows")
    {
        options |= ofCentered;
        flags |= wfGrow | wfZoom;
        dragMode = dmLimitAll;
        TRect r = getExtent().grow(-2, -2);
        r.b.x -= buttonsWidth + 1;
        auto *sb = new TScrollBar(TRect(r.b.x, r.a.y, r.b.x + 1, r.b.y));
        sb->growMode = gfGrowLoX | gfGrowHiX | gfGrowHiY;
        insert(sb);
        list = new WindowListViewer(r, sb);
        list->growMode = gfGrowHiX | gfGrowHiY;
        insert(list);
        insert(new TLabel(TRect(r.a.x, r.a.y - 1, r.a.x + 10, r.a.y), "~W~indows", list));
        int x = size.x - buttonsWidth, y = 2;
        auto addButton = [&] (const char *text, ushort cmd, ushort flags) {
            auto *b = new TButton(TRect(x, y, x + 10, y + 2), text, cmd, flags);
            b->growMode = gfGrowLoX | gfGrowHiX;
            insert(b);
            y += 2;
        };
        addButton("~G~o", cmOK, bfDefault);
        addButton("~R~ename", cmWinListRename, bfNormal);
        addButton("~C~lose", cmWinListClose, bfNormal);
        addButton("Cancel", cmCancel, bfNormal);
        list->select();
    }

    // Moves 'r' inside the owner, then shrinks it if it still does not fit.
    TRect fitInside(TRect r)
    {
        TPoint lim = owner->size;
        TPoint sz = r.b - r.a;
        if (sz.x > lim.x) sz.x = lim.x;
        if (sz.y > lim.y) sz.y = lim.y;
        if (r.a.x + sz.x > lim.x) r.a.x = lim.x - sz.x;
        if (r.a.y + sz.y > lim.y) r.a.y = lim.y - sz.y;
        if (r.a.x < 0) r.a.x = 0;
        if (r.a.y < 0) r.a.y = 0;
        r.b = r.a + sz;
        return r;
    }

    void sizeLimits(TPoint &min, TPoint &max) override
    {
        TDialog::sizeLimits(min, max);
        min = minSize;
        if (min.x > max.x) min.x = max.x;
        if (min.y > max.y) min.y = max.y;
    }

    // The screen changed size: keep the dialog inside it.
    void calcBounds(TRect &bounds, TPoint delta) override
    {
        (void) delta;
        bounds = fitInside(getBounds());
        autoFit = true;
    }

    void changeBounds(const TRect &bounds) override
    {
        TPoint old = size;
        TDialog::changeBounds(bounds);
        if (size != old && !autoFit && !inZoom)
        {
            sized = true;
            rememberedSize = size;
        }
        autoFit = false;
    }

    void zoom() override
    {
        inZoom = true;
        zoomRect = fitInside(zoomRect);
        TDialog::zoom();
        inZoom = false;
    }

    void handleEvent(TEvent &ev) override
    {
        if (ev.what == evKeyDown && (ev.keyDown.keyCode == kbCtrlF5 || ev.keyDown.keyCode == kbF5))
        {
            ushort cmd = ev.keyDown.keyCode == kbF5 ? cmZoom : cmResize;
            ev.what = evCommand;
            ev.message.command = cmd;
            ev.message.infoPtr = nullptr;
        }
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

static std::string commandOf(TWindow *w)
{
    if (auto *term = dynamic_cast<TerminalWindow *>(w))
    {
        // The command of the profile; otherwise the program started in the
        // terminal (the shell).
        if (!term->launchCommand.empty())
            return term->launchCommand;
        tvterm::ClientProcessInfo info;
        auto *view = term->getTerminalView();
        if (view && view->termCtrl.getClientProcessInfo(info) && info.clientPid)
            return procinfo::readCommandLine(info.clientPid);
        return std::string();
    }
    if (auto *log = dynamic_cast<LogViewWindow *>(w))
        return "log: " + log->getOptions().command;
    if (dynamic_cast<TEditWindow *>(w))
        return "editor";
    if (dynamic_cast<TEventViewer *>(w))
        return "event viewer";
    return std::string();
}

static Row describe(TWindow *w)
{
    Row row;
    row.desktop = desktopColumn(w);
    const char *title = w->getTitle(100);
    row.title = title && *title ? title : "(no title)";
    row.command = commandOf(w);
    return row;
}

} // namespace winlist

std::vector<TWindow *> getDeskTopWindows(TDeskTop &deskTop)
{
    std::vector<TWindow *> windows;
    // TGroup::first() is the frontmost view.
    if (TView *v = deskTop.first())
        do {
            if (auto *w = dynamic_cast<TWindow *>(v))
                if ((w->state & sfVisible) || desktops::isHidden(w))
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

std::vector<TWindow *> getSortedWindows(TDeskTop &deskTop)
{
    desktops::prune(deskTop); // Numbers the windows opened just now.
    std::vector<TWindow *> windows = getDeskTopWindows(deskTop);
    auto key = [] (TWindow *w) {
        int d = desktops::of(w);
        return d == 0 ? desktops::count + 1 : d;
    };
    std::stable_sort(windows.begin(), windows.end(), [&] (TWindow *a, TWindow *b) {
        int da = key(a), db = key(b);
        if (da != db)
            return da < db;
        return desktops::openedOrder(a) < desktops::openedOrder(b);
    });
    return windows;
}

std::string desktopColumn(TWindow *w)
{
    int d = desktops::of(w);
    if (d == 0)
        return " all";
    return (d == desktops::current() ? "*" : " ") + desktops::label(d);
}

bool getWindowListSize(TPoint &size)
{
    if (winlist::sized)
        size = winlist::rememberedSize;
    return winlist::sized;
}

void setWindowListSize(TPoint size)
{
    winlist::sized = true;
    winlist::rememberedSize.x = std::max(size.x, winlist::minSize.x);
    winlist::rememberedSize.y = std::max(size.y, winlist::minSize.y);
}

void execWindowList(TDeskTop &deskTop)
{
    using namespace winlist;
    TWindow *focusedWindow = dynamic_cast<TWindow *>(deskTop.current);
    while (true)
    {
        std::vector<TWindow *> windows = getSortedWindows(deskTop);
        if (windows.empty())
        {
            messageBox("There are no windows.", mfInformation | mfOKButton);
            return;
        }
        std::vector<Row> rows;
        short focused = 0;
        for (size_t i = 0; i < windows.size(); ++i)
        {
            rows.push_back(describe(windows[i]));
            if (windows[i] == focusedWindow)
                focused = (short) i;
        }

        TRect r(0, 0, 0, 0);
        r.b = sized ? rememberedSize : defaultSize;
        if (r.b.x > deskTop.size.x) r.b.x = deskTop.size.x;
        if (r.b.y > deskTop.size.y) r.b.y = deskTop.size.y;
        auto *d = new WindowListDialog(r);
        d->list->setRows(std::move(rows));
        d->list->focusItem(focused);
        TView *p = TProgram::application->validView(d);
        if (!p)
            return;
        ushort result = deskTop.execView(p);
        short index = d->list->focused;
        TObject::destroy(p);

        if (index < 0 || (size_t) index >= windows.size())
            return;
        TWindow *w = windows[index];
        focusedWindow = w; // The list opens again on the same window.
        switch (result)
        {
            case cmOK:
                desktops::selectWindow(deskTop, w);
                return;
            case cmWinListRename:
                if (!renameTerminal(w))
                    messageBox("Only terminals and Log Views can be renamed.", mfInformation | mfOKButton);
                break; // Show the list again.
            case cmWinListClose:
            {
                // The next window of the list is focused when it opens again.
                focusedWindow = (size_t) index + 1 < windows.size() ? windows[index + 1] :
                                index > 0 ? windows[index - 1] : nullptr;
                TEvent ev;
                ev.what = evCommand;
                ev.message.command = cmClose;
                ev.message.infoPtr = nullptr;
                w->handleEvent(ev); // Asks for confirmation if needed.
                // Not closed (cancelled): it stays focused in the list.
                for (TWindow *x : getDeskTopWindows(deskTop))
                    if (x == w)
                        focusedWindow = w;
                break;
            }
            default:
                return;
        }
    }
}
