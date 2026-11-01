#define Uses_TWindow
#define Uses_TDeskTop
#include <tvision/tv.h>

#include "desktops.h"
#include "evntview.h"

#include <map>
#include <vector>

namespace desktops
{

static int currentDesktop = 1;
// Hidden windows and their desktop.
static std::map<TView *, int> hidden;
// The window focused last on each desktop.
static TView *lastFocus[count + 1] {};

static std::vector<TWindow *> windows(TDeskTop &desk)
// The windows of the desktop, from front to back.
{
    std::vector<TWindow *> out;
    if (TView *v = desk.first())
        do {
            if (auto *w = dynamic_cast<TWindow *>(v))
                out.push_back(w);
            v = v->next;
        } while (v != desk.first());
    return out;
}

static bool isChild(TDeskTop &desk, TView *v)
{
    if (v)
        for (TWindow *w : windows(desk))
            if (w == v)
                return true;
    return false;
}

static void setVisible(TWindow *w, bool visible)
// Without changing the focused window: TView::setState(sfVisible) selects
// the first visible window, which would also clear its alert.
{
    ushort selectable = w->options & ofSelectable;
    w->options &= ~ofSelectable;
    if (visible)
        w->show();
    else
        w->hide();
    w->options |= selectable;
}

static void focusWindow(TDeskTop &desk, TView *w)
// TView::select only moves a window to the front (ofTopSelect) and relies
// on resetCurrent to focus it; the frontmost window of a desktop just shown
// is already in front, so it would stay without the focus (no keys, no
// clicks). It is made the current view here.
{
    w->select();
    if (desk.current != w)
        desk.setCurrent(w, TView::normalSelect);
}

int current()
{
    return currentDesktop;
}

bool isGlobal(TWindow *w)
{
    return dynamic_cast<TEventViewer *>(w) != nullptr;
}

bool isHidden(TWindow *w)
{
    return !(w->state & sfVisible) && hidden.count(w);
}

int of(TWindow *w)
{
    if (isGlobal(w))
        return 0;
    if (!(w->state & sfVisible))
    {
        auto it = hidden.find(w);
        if (it != hidden.end())
            return it->second;
    }
    return currentDesktop;
}

static void focusAfterSwitch(TDeskTop &desk, int n)
{
    TView *f = lastFocus[n];
    if (f && isChild(desk, f) && (f->state & sfVisible))
    {
        focusWindow(desk, f);
        return;
    }
    // The frontmost window of the desktop, else the Event Viewer.
    TWindow *global = nullptr;
    for (TWindow *w : windows(desk))
        if ((w->state & sfVisible) && (w->options & ofSelectable))
        {
            if (!isGlobal(w))
            {
                focusWindow(desk, w);
                return;
            }
            if (!global)
                global = w;
        }
    if (global)
        focusWindow(desk, global);
}

void switchTo(TDeskTop &desk, int n)
{
    if (n < 1 || n > count || n == currentDesktop)
        return;
    prune(desk);
    // The Event Viewer is on every desktop: when it has the focus, the
    // frontmost window gets it on return.
    auto *cur = dynamic_cast<TWindow *>(desk.current);
    lastFocus[currentDesktop] = cur && !isGlobal(cur) ? cur : nullptr;
    desk.lock();
    TWindow *focused = nullptr;
    for (TWindow *w : windows(desk))
        if ((w->state & sfVisible) && !isGlobal(w))
        {
            hidden[w] = currentDesktop;
            if (w == desk.current)
                focused = w; // Hidden last, so that nothing else is selected.
            else
                setVisible(w, false);
        }
    if (focused)
        focused->hide(); // The focus goes to the Event Viewer, if any.
    currentDesktop = n;
    for (TWindow *w : windows(desk))
    {
        auto it = hidden.find(w);
        if (it != hidden.end() && it->second == n)
        {
            hidden.erase(it);
            setVisible(w, true);
        }
    }
    focusAfterSwitch(desk, n);
    desk.unlock();
}

bool moveTo(TDeskTop &desk, TWindow *w, int n)
{
    if (!w || isGlobal(w) || n < 1 || n > count)
        return false;
    int from = of(w);
    if (from == n)
        return true;
    if (n == currentDesktop)
    {
        // Not possible from the menu (only the current window is moved).
        hidden.erase(w);
        setVisible(w, true);
        return true;
    }
    if (lastFocus[from] == w)
        lastFocus[from] = nullptr;
    hidden[w] = n;
    if (w == desk.current)
        w->hide(); // The focus goes to the next window of this desktop.
    else if (w->state & sfVisible)
        setVisible(w, false);
    return true;
}

void selectWindow(TDeskTop &desk, TWindow *w)
{
    if (!w)
        return;
    if (isHidden(w))
    {
        int n = hidden[w];
        lastFocus[n] = w;
        switchTo(desk, n);
    }
    focusWindow(desk, w);
}

std::array<int, count + 1> windowCounts(TDeskTop &desk)
{
    std::array<int, count + 1> counts {};
    for (TWindow *w : windows(desk))
        if (isGlobal(w))
            ++counts[0];
        else if ((w->state & sfVisible) || hidden.count(w))
            ++counts[of(w)];
    return counts;
}

void prune(TDeskTop &desk)
{
    std::vector<TWindow *> ws = windows(desk);
    auto present = [&] (TView *v) {
        for (TWindow *w : ws)
            if (w == v)
                return true;
        return false;
    };
    for (auto it = hidden.begin(); it != hidden.end();)
        if (present(it->first))
            ++it;
        else
            it = hidden.erase(it);
    for (TView *&f : lastFocus)
        if (f && !present(f))
            f = nullptr;
}

} // namespace desktops
