#define Uses_TDialog
#define Uses_TListViewer
#define Uses_TScrollBar
#define Uses_TButton
#define Uses_TLabel
#define Uses_TInputLine
#define Uses_THistory
#define Uses_TDeskTop
#define Uses_TProgram
#define Uses_TEvent
#define Uses_MsgBox
#define Uses_TEditWindow
#define Uses_TText
#define Uses_TKeys
#define Uses_TView
#define Uses_TDrawBuffer
#define Uses_TPalette
#include <tvision/tv.h>

#include "winlist.h"
#include "wnd.h"
#include "evntview.h"
#include "logview.h"
#include "desktops.h"
#include "procinfo.h"
#include "alerts.h"
#include "histfile.h"
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
    cmWinListColumns,  // Broadcast: the rows of the list changed.
};

// One line of the list: four columns.
struct Row
{
    std::string alert, desktop, title, command;
};

// Column widths chosen in the profile (window-list-columns): Alert, Desk,
// Name; -1: automatic; 0: column hidden. Command gets the rest.
static bool columnsSet = false;
static int columnSpec[3] = {-1, -1, -1};

const char *const columnNames[4] = {"Alert", "Desk", "Name", "Command"};
const int columnGap = 2;   // Spaces between two columns.
const int nameShare = 40;  // Automatic Name: at most this % of the line.

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

// The four columns of a row (or of the header), cut and padded to
// 'widths'; hidden columns (width 0) are left out with their gap.
static std::string joinColumns(const std::string (&cells)[4], const int (&widths)[4])
{
    std::string text;
    bool first = true;
    for (int i = 0; i < 4; ++i)
    {
        if (widths[i] <= 0)
            continue;
        if (!first)
            text += std::string(columnGap, ' ');
        text += fitColumn(cells[i], widths[i]);
        first = false;
    }
    return text;
}

class WindowListViewer : public TListViewer
{
public:

    std::vector<Row> rows;
    // The longest text of each column, at least its name (for 'auto').
    int longest[4] {0, 0, 0, 0};

    WindowListViewer(const TRect &bounds, TScrollBar *vScrollBar) noexcept :
        TListViewer(bounds, 1, nullptr, vScrollBar)
    {
    }

    void setRows(std::vector<Row> aRows)
    {
        rows = std::move(aRows);
        for (int i = 0; i < 4; ++i)
            longest[i] = (int) strlen(columnNames[i]);
        for (const Row &r : rows)
        {
            longest[0] = std::max(longest[0], (int) TText::width(r.alert));
            longest[1] = std::max(longest[1], (int) TText::width(r.desktop));
            longest[2] = std::max(longest[2], (int) TText::width(r.title));
            longest[3] = std::max(longest[3], (int) TText::width(r.command));
        }
        setRange((short) rows.size());
        drawView();
        if (owner)
            message(owner, evBroadcast, cmWinListColumns, this);
    }

    // The widths of the columns for the current width of the list (the
    // text starts one column in): Alert, Desk and Name as chosen in the
    // profile or automatic (the longest text, at least the column name;
    // Name at most nameShare % of the line), Command the rest. Columns that
    // do not fit are cut, from the right.
    void columnWidths(int (&widths)[4]) const
    {
        int total = size.x - 1;
        int left = total;
        bool any = false;
        for (int i = 0; i < 3; ++i)
        {
            int w = columnSpec[i];
            if (w < 0)
            {
                w = longest[i];
                if (i == 2)
                    w = std::min(w, std::max((int) strlen(columnNames[2]), total * nameShare / 100));
            }
            int gap = (any && w > 0) ? columnGap : 0;
            w = std::max(0, std::min(w, left - gap));
            widths[i] = w;
            if (w > 0)
            {
                left -= w + gap;
                any = true;
            }
        }
        widths[3] = std::max(0, left - (any ? columnGap : 0));
    }

    void getText(char *dest, short item, short maxChars) override
    {
        std::string text;
        if (item >= 0 && (size_t) item < rows.size())
        {
            const Row &r = rows[item];
            int widths[4];
            columnWidths(widths);
            const std::string cells[4] = {r.alert, r.desktop, r.title, r.command};
            text = joinColumns(cells, widths);
        }
        strncpy(dest, text.c_str(), maxChars);
        dest[maxChars] = '\0';
    }
};

// The names of the columns, above the list and aligned with it (in the
// color of the dialog's static text).
class ColumnHeader : public TView
{
public:

    WindowListViewer *list {nullptr};

    ColumnHeader(const TRect &bounds) noexcept :
        TView(bounds)
    {
        growMode = gfGrowHiX;
        eventMask |= evBroadcast;
    }

    void draw() override
    {
        TDrawBuffer b;
        TColorAttr color = getColor(1);
        b.moveChar(0, ' ', color, size.x);
        if (list)
        {
            int widths[4];
            list->columnWidths(widths);
            const std::string cells[4] = {columnNames[0], columnNames[1], columnNames[2], columnNames[3]};
            // TListViewer draws its text one column in.
            b.moveStr(1, joinColumns(cells, widths), color);
        }
        writeLine(0, 0, size.x, 1, b);
    }

    TPalette &getPalette() const override
    {
        static TPalette palette("\x06", 1); // Static text of the dialog.
        return palette;
    }

    void handleEvent(TEvent &ev) override
    {
        TView::handleEvent(ev);
        if (ev.what == evBroadcast && ev.message.command == cmWinListColumns)
            drawView();
    }

    // The widths follow the list, which is resized with the dialog.
    void changeBounds(const TRect &bounds) override
    {
        TView::changeBounds(bounds);
        drawView();
    }
};

// Size chosen by the user (mouse or Ctrl-F5), kept while tvterm runs and in
// profiles (window-list-size).
static bool sized = false;
static TPoint rememberedSize {0, 0};

const TPoint defaultSize {90, 20};
const TPoint minSize {50, 10};
const int buttonsWidth = 12; // Buttons on the right, with their margin.

// The arrow of the Filter field: the history is added to only when the
// list closes with Go, Rename or Close (execWindowList), not when the
// field loses the focus or after Esc.
class FilterHistory : public THistory
{
public:

    using THistory::THistory;
    void recordHistory(const char *) override {}
};

class WindowListDialog : public TDialog
{
public:

    WindowListViewer *list;
    ColumnHeader *header {nullptr};
    TInputLine *filter;
    THistory *filterHistory {nullptr};
    bool autoFit {false}; // the next changeBounds is not the user's choice
    bool inZoom {false};
    // All the rows and, after the filter, the indexes of those shown.
    std::vector<Row> allRows;
    std::vector<size_t> visible;
    std::string lastFilter;

    WindowListDialog(const TRect &bounds) noexcept :
        TWindowInit(&initFrame),
        TDialog(bounds, "Windows")
    {
        options |= ofCentered;
        flags |= wfGrow | wfZoom;
        dragMode = dmLimitAll;
        TRect r = getExtent().grow(-2, -2);
        r.b.x -= buttonsWidth + 1;
        // The filter field on the first row (with its history: the arrow,
        // Alt-Down or Ctrl-Down), the list below it.
        filter = new TInputLine(TRect(r.a.x + 8, r.a.y, r.b.x - 2, r.a.y + 1), 128);
        filter->growMode = gfGrowHiX;
        insert(filter);
        filterHistory = new FilterHistory(TRect(r.b.x - 2, r.a.y, r.b.x + 1, r.a.y + 1), filter,
                                     histfile::hiWindowListFilter);
        filterHistory->growMode = gfGrowLoX | gfGrowHiX;
        insert(filterHistory);
        insert(new TLabel(TRect(r.a.x - 1, r.a.y, r.a.x + 8, r.a.y + 1), "~F~ilter", filter));
        r.a.y += 2;
        auto *sb = new TScrollBar(TRect(r.b.x, r.a.y, r.b.x + 1, r.b.y));
        sb->growMode = gfGrowLoX | gfGrowHiX | gfGrowHiY;
        insert(sb);
        list = new WindowListViewer(r, sb);
        list->growMode = gfGrowHiX | gfGrowHiY;
        // The column names on the row above the list (Alt-W: the list).
        header = new ColumnHeader(TRect(r.a.x, r.a.y - 1, r.b.x, r.a.y));
        header->list = list;
        insert(header);
        insert(list);
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

    // The rows of all the windows; 'focusedRow' (index in 'rows') is focused.
    void setAllRows(std::vector<Row> rows, size_t focusedRow, const std::string &text)
    {
        allRows = std::move(rows);
        filter->setData((void *) text.c_str());
        applyFilter(focusedRow);
    }

    // Shows the rows whose title or command contains the filter text
    // (case-insensitive; empty: all). 'keep' (index in allRows) stays
    // focused if shown, otherwise the first row.
    void applyFilter(size_t keep)
    {
        lastFilter = filter->data;
        std::string needle = lowerAscii(lastFilter);
        visible.clear();
        std::vector<Row> rows;
        short focusedItem = 0;
        for (size_t i = 0; i < allRows.size(); ++i)
        {
            const Row &r = allRows[i];
            if ( needle.empty() ||
                 lowerAscii(r.title).find(needle) != std::string::npos ||
                 lowerAscii(r.command).find(needle) != std::string::npos )
            {
                if (i == keep)
                    focusedItem = (short) visible.size();
                visible.push_back(i);
                rows.push_back(r);
            }
        }
        list->setRows(std::move(rows));
        if (!visible.empty())
            list->focusItem(focusedItem);
        // The title shows how many are left: "Windows 3/12".
        std::string t = "Windows";
        if (!needle.empty())
            t += " " + std::to_string(visible.size()) + "/" + std::to_string(allRows.size());
        delete[] (char *) title;
        title = newStr(t.c_str());
        if (frame)
            frame->drawView();
    }

    // The window of the focused row (index in allRows), or -1.
    long focusedRow() const
    {
        short i = list->focused;
        if (i < 0 || (size_t) i >= visible.size())
            return -1;
        return (long) visible[i];
    }

    static std::string lowerAscii(const std::string &s)
    {
        std::string out = s;
        for (char &c : out)
            if (c >= 'A' && c <= 'Z')
                c = char(c - 'A' + 'a');
        return out;
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
        // The column names follow the new width of the list.
        if (header)
            header->drawView();
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
        // Alt-W: the list (its label was replaced by the column names).
        if (ev.what == evKeyDown && ev.keyDown.keyCode == kbAltW)
        {
            list->select();
            clearEvent(ev);
            return;
        }
        // In the filter field, Alt-Down / Ctrl-Down open its history (Down
        // moves in the list).
        if ( ev.what == evKeyDown && current == filter &&
             (ev.keyDown.keyCode == kbAltDown || ev.keyDown.keyCode == kbCtrlDown) )
        {
            TEvent down = ev;
            down.keyDown.keyCode = kbDown;
            down.keyDown.controlKeyState = 0;
            filterHistory->handleEvent(down);
            clearEvent(ev);
            return;
        }
        // In the filter field, Up / Down / PgUp / PgDn move in the list.
        if ( ev.what == evKeyDown && current == filter &&
             ( ev.keyDown.keyCode == kbUp || ev.keyDown.keyCode == kbDown ||
               ev.keyDown.keyCode == kbPgUp || ev.keyDown.keyCode == kbPgDn ) )
        {
            list->handleEvent(ev);
            clearEvent(ev);
            return;
        }
        // Nothing shown (filter): Go, Rename and Close do nothing.
        if ( ev.what == evCommand && visible.empty() &&
             ( ev.message.command == cmOK || ev.message.command == cmWinListRename ||
               ev.message.command == cmWinListClose ) )
        {
            clearEvent(ev);
            return;
        }
        TDialog::handleEvent(ev);
        if (filter && lastFilter != filter->data)
        {
            long cur = focusedRow();
            applyFilter(cur < 0 ? allRows.size() : (size_t) cur);
        }
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
    // Alert levels come later (item 7): an alert is level 5 for now.
    if (auto *target = dynamic_cast<AlertTarget *>(w))
        if (target->hasAlert())
            row.alert = "A5";
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

bool getWindowListColumns(int (&widths)[3])
{
    for (int i = 0; i < 3; ++i)
        widths[i] = winlist::columnSpec[i];
    return winlist::columnsSet;
}

void setWindowListColumns(const int (&widths)[3])
{
    winlist::columnsSet = true;
    for (int i = 0; i < 3; ++i)
        winlist::columnSpec[i] = widths[i];
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
    // The filter text stays when the list opens again (after Rename, Close).
    std::string filterText;
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
        d->setAllRows(std::move(rows), (size_t) focused, filterText);
        // The list keeps the focus (Enter: Go); Alt-F or Tab: the filter.
        d->list->select();
        TView *p = TProgram::application->validView(d);
        if (!p)
            return;
        ushort result = deskTop.execView(p);
        long index = d->focusedRow();
        filterText = d->lastFilter;
        // The filter goes to its history (not after Cancel / Esc).
        if (result != cmCancel)
            historyAdd(histfile::hiWindowListFilter, filterText);
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
