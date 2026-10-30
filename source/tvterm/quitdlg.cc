#define Uses_TDialog
#define Uses_TListViewer
#define Uses_TScrollBar
#define Uses_TButton
#define Uses_TLabel
#define Uses_TStaticText
#define Uses_TDeskTop
#define Uses_TProgram
#define Uses_TEvent
#define Uses_TText
#define Uses_TKeys
#include <tvision/tv.h>

#include "quitdlg.h"
#include "cmds.h"
#include "procinfo.h"
#include "winlist.h"
#include "wnd.h"
#include <tvterm/termview.h>
#include <tvterm/termctrl.h>

#include <stdio.h>
#include <string.h>

#include <algorithm>
#include <string>
#include <vector>

namespace quitdlg
{

enum : ushort
{
    cmQuitGoTo = 3200,
};

// Cuts 'text' to at most 'width' columns, adding '…' if it was cut.
static std::string fitWidth(const std::string &text, int width)
{
    if (width < 1)
        return std::string();
    if ((int) TText::width(text) <= width)
        return text;
    return text.substr(0, TText::scroll(text, width - 1, False)) + "\u2026";
}

class TermListViewer : public TListViewer
{
public:

    std::vector<std::string> items;

    TermListViewer(const TRect &bounds, TScrollBar *vScrollBar) noexcept :
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
            // The text starts one column in: size.x - 1 columns are visible.
            std::string text = fitWidth(items[item], size.x - 1);
            strncpy(dest, text.c_str(), maxChars);
            dest[maxChars] = '\0';
        }
        else
            *dest = '\0';
    }
};

// Size of the dialog chosen by the user, kept while tvterm runs.
struct RememberedSize
{
    bool sized {false};  // resized by the user (mouse or Ctrl-F5)
    TPoint size {0, 0};
    bool zoomed {false}; // closed while zoomed
};

static RememberedSize remembered;

const TPoint minDialogSize {44, 12};
const int buttonsWidth = 38;

class QuitDialog : public TDialog
{
public:

    TermListViewer *list;
    TButton *buttons[3];
    bool zoomed {false};
    bool autoFit {false}; // the next changeBounds is not the user's choice
    bool inZoom {false};

    QuitDialog(const TRect &bounds, const std::string &summary, bool closing) noexcept :
        TWindowInit(&initFrame),
        TDialog(bounds, closing ? "Close windows" : "Quit")
    {
        options |= ofCentered;
        flags |= wfGrow | wfZoom;
        dragMode = dmLimitAll;
        auto *text = new TStaticText(TRect(2, 2, size.x - 2, 4), summary);
        text->growMode = gfGrowHiX;
        insert(text);
        TRect r(2, 6, size.x - 3, size.y - 4);
        auto *sb = new TScrollBar(TRect(r.b.x, r.a.y, r.b.x + 1, r.b.y));
        sb->growMode = gfGrowLoX | gfGrowHiX | gfGrowHiY;
        insert(sb);
        list = new TermListViewer(r, sb);
        list->growMode = gfGrowHiX | gfGrowHiY;
        insert(list);
        insert(new TLabel(TRect(r.a.x, r.a.y - 1, r.a.x + 30, r.a.y), "~T~erminals", list));
        buttons[0] = new TButton(TRect(0, 0, 10, 2), closing ? "~C~lose" : "~Q~uit", cmOK, bfDefault);
        buttons[1] = new TButton(TRect(0, 0, 12, 2), "~G~o to", cmQuitGoTo, bfNormal);
        buttons[2] = new TButton(TRect(0, 0, 12, 2), "Cancel", cmCancel, bfNormal);
        placeButtons();
        for (auto *b : buttons)
            insert(b);
        list->select();
    }

    // The buttons stay at the bottom, centered.
    void placeButtons()
    {
        int x = (size.x - buttonsWidth) / 2, y = size.y - 3;
        if (x < 1) x = 1;
        int widths[3] = {10, 12, 12};
        for (int i = 0; i < 3; ++i)
        {
            TRect r(x, y, x + widths[i], y + 2);
            buttons[i]->locate(r);
            x += widths[i] + 2;
        }
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
        min = minDialogSize;
        if (min.x > max.x) min.x = max.x;
        if (min.y > max.y) min.y = max.y;
    }

    // The screen changed size: keep the dialog inside it.
    void calcBounds(TRect &bounds, TPoint delta) override
    {
        (void) delta;
        if (zoomed)
            bounds = TRect(0, 0, owner->size.x, owner->size.y);
        else
            bounds = fitInside(getBounds());
        autoFit = true;
    }

    void changeBounds(const TRect &bounds) override
    {
        TPoint old = size;
        TDialog::changeBounds(bounds);
        if (size != old)
        {
            placeButtons();
            if (!autoFit && !inZoom)
            {
                remembered.sized = true;
                remembered.size = size;
                zoomed = false;
            }
        }
        autoFit = false;
    }

    void zoom() override
    {
        TPoint min, max;
        sizeLimits(min, max);
        inZoom = true;
        if (size != max)
            zoomed = true;
        else
        {
            zoomRect = fitInside(zoomRect);
            zoomed = false;
        }
        TDialog::zoom();
        inZoom = false;
    }

    ushort execute() override
    {
        if (remembered.zoomed)
            zoom();
        ushort result = TDialog::execute();
        remembered.zoomed = zoomed;
        return result;
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
        if (ev.what == evCommand && ev.message.command == cmQuitGoTo)
        {
            endModal(cmQuitGoTo);
            clearEvent(ev);
        }
        else if (ev.what == evBroadcast && ev.message.command == cmListItemSelected)
        {
            // Space or double click on an item (Enter quits).
            endModal(cmQuitGoTo);
            clearEvent(ev);
        }
    }
};

struct TermItem
{
    TWindow *window;
    std::string title;
    std::string what;
    bool busy;
};

static bool isConnected(TWindow *w)
{
    size_t count = 0;
    message(w, evBroadcast, cmGetOpenTerms, &count);
    return count != 0;
}

static TermItem describe(TerminalWindow &w)
{
    TermItem item {&w, {}, {}, true};
    const char *title = w.getTitle(100);
    item.title = title && *title ? title : "(no title)";

    tvterm::ClientProcessInfo info;
    procinfo::ForegroundInfo fg;
    auto *view = w.getTerminalView();
    if (!view || !view->termCtrl.getClientProcessInfo(info))
        item.what = "(no information)";
    else if (!procinfo::describeForeground(info, fg))
        item.what = info.foregroundPgid && info.foregroundPgid != info.clientPid
                  ? "a program is running"
                  : "(no information)";
    else if (fg.idleShell)
    {
        item.what = fg.command + ", idle";
        item.busy = false;
    }
    else
    {
        item.what = fg.command;
        if (fg.far2l)
            item.what += "  [stays in background]";
    }
    return item;
}

static std::string padRight(const std::string &text, int width)
{
    int w = (int) TText::width(text);
    return w < width ? text + std::string(width - w, ' ') : text;
}

} // namespace quitdlg

bool confirmQuit(TDeskTop &deskTop, bool closing)
{
    using namespace quitdlg;
    std::vector<TermItem> terms;
    for (TWindow *w : getDeskTopWindows(deskTop))
        if (auto *term = dynamic_cast<TerminalWindow *>(w))
            if (isConnected(term))
                terms.push_back(describe(*term));
    if (terms.empty())
        return true;
    // Terminals running a program first; otherwise, desktop order.
    std::stable_sort(terms.begin(), terms.end(),
        [] (const TermItem &a, const TermItem &b) { return a.busy && !b.busy; });

    size_t busy = 0;
    int titleWidth = 0;
    for (auto &t : terms)
    {
        busy += t.busy;
        t.title = fitWidth(t.title, 24);
        titleWidth = std::max(titleWidth, (int) TText::width(t.title));
    }
    std::vector<std::string> items;
    for (const auto &t : terms)
        items.push_back(" " + padRight(t.title, titleWidth) + "  " + t.what);

    char summary[256];
    snprintf(summary, sizeof(summary),
        "%zu terminal%s open, %zu running a program.\n"
        "Programs receive SIGHUP and usually stop. %s anyway?",
        terms.size(), terms.size() == 1 ? " is" : "s are", busy,
        closing ? "Close them" : "Quit");

    // Natural size: every line whole (at least 76 columns), every terminal
    // on its own line; or the size chosen by the user before. Never larger
    // than the desktop.
    TRect r(0, 0, 0, 0);
    if (remembered.sized)
        r.b = remembered.size;
    else
    {
        int width = 76;
        for (const auto &item : items)
            width = std::max(width, (int) TText::width(item) + 7);
        int listHeight = std::max((int) terms.size(), 3);
        r.b = TPoint {width, listHeight + 10};
    }
    r.b.x = std::max(r.b.x, minDialogSize.x);
    r.b.y = std::max(r.b.y, minDialogSize.y);
    if (r.b.x > deskTop.size.x) r.b.x = deskTop.size.x;
    if (r.b.y > deskTop.size.y) r.b.y = deskTop.size.y;

    auto *d = new QuitDialog(r, summary, closing);
    d->list->setItems(std::move(items));
    d->list->focusItem(0);
    TView *p = TProgram::application->validView(d);
    if (!p)
        return false;
    ushort result = deskTop.execView(p);
    short focused = d->list->focused;
    TObject::destroy(p);

    if (result == cmOK)
        return true;
    if (result == cmQuitGoTo && focused >= 0 && (size_t) focused < terms.size())
        terms[focused].window->select();
    return false;
}
