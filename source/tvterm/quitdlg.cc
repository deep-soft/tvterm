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
#include <tvision/tv.h>

#include "quitdlg.h"
#include "cmds.h"
#include "procinfo.h"
#include "winlist.h"
#include "wnd.h"
#include "logview.h"
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
            strncpy(dest, items[item].c_str(), maxChars);
            dest[maxChars] = '\0';
        }
        else
            *dest = '\0';
    }
};

class QuitDialog : public TDialog
{
public:

    TermListViewer *list;

    QuitDialog(const TRect &bounds, const std::string &summary) noexcept :
        TWindowInit(&initFrame),
        TDialog(bounds, "Quit")
    {
        options |= ofCentered;
        insert(new TStaticText(TRect(2, 2, size.x - 2, 4), summary));
        TRect r(2, 6, size.x - 3, size.y - 4);
        auto *sb = new TScrollBar(TRect(r.b.x, r.a.y, r.b.x + 1, r.b.y));
        insert(sb);
        list = new TermListViewer(r, sb);
        insert(list);
        insert(new TLabel(TRect(r.a.x, r.a.y - 1, r.a.x + 30, r.a.y), "~W~indows", list));
        int x = (size.x - 38) / 2, y = size.y - 3;
        insert(new TButton(TRect(x, y, x + 10, y + 2), "~Q~uit", cmOK, bfDefault)); x += 12;
        insert(new TButton(TRect(x, y, x + 12, y + 2), "~G~o to", cmQuitGoTo, bfNormal)); x += 14;
        insert(new TButton(TRect(x, y, x + 12, y + 2), "Cancel", cmCancel, bfNormal));
        list->select();
    }

    void handleEvent(TEvent &ev) override
    {
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

// Cuts 'text' to at most 'width' columns, adding '…' if it was cut.
static std::string fitWidth(const std::string &text, int width)
{
    if ((int) TText::width(text) <= width)
        return text;
    return text.substr(0, TText::scroll(text, width - 1, False)) + "\u2026";
}

static std::string padRight(const std::string &text, int width)
{
    int w = (int) TText::width(text);
    return w < width ? text + std::string(width - w, ' ') : text;
}

} // namespace quitdlg

bool confirmQuit(TDeskTop &deskTop)
{
    using namespace quitdlg;
    std::vector<TermItem> terms;
    size_t logs = 0;
    for (TWindow *w : getDeskTopWindows(deskTop))
        if (auto *term = dynamic_cast<TerminalWindow *>(w))
        {
            if (isConnected(term))
                terms.push_back(describe(*term));
        }
        else if (auto *log = dynamic_cast<LogViewWindow *>(w))
            if (log->isSourceRunning())
            {
                const char *title = log->getTitle(100);
                terms.push_back({w, title && *title ? title : "Log", "log: " + log->getOptions().command, true});
                ++logs;
            }
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
    size_t termCount = terms.size() - logs;
    if (logs == 0)
        snprintf(summary, sizeof(summary),
            "%zu terminal%s open, %zu running a program.\n"
            "Programs receive SIGHUP and usually stop. Quit anyway?",
            termCount, termCount == 1 ? " is" : "s are", busy);
    else if (termCount == 0)
        snprintf(summary, sizeof(summary),
            "%zu Log View%s reading the output of a command.\n"
            "The commands receive SIGHUP and stop. Quit anyway?",
            logs, logs == 1 ? " is" : "s are");
    else
        snprintf(summary, sizeof(summary),
            "%zu terminal%s (%zu running a program), %zu Log View%s.\n"
            "Programs receive SIGHUP and usually stop. Quit anyway?",
            termCount, termCount == 1 ? "" : "s", busy - logs, logs, logs == 1 ? "" : "s");

    int listHeight = (int) terms.size();
    int maxListHeight = deskTop.size.y - 10;
    if (listHeight > maxListHeight) listHeight = maxListHeight;
    if (listHeight < 3) listHeight = 3;
    TRect r(0, 0, 76, listHeight + 10);
    if (r.b.x > deskTop.size.x) r.b.x = deskTop.size.x;
    if (r.b.y > deskTop.size.y) r.b.y = deskTop.size.y;

    auto *d = new QuitDialog(r, summary);
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
