#define Uses_TDeskTop
#define Uses_TWindow
#define Uses_TFrame
#define Uses_TEvent
#define Uses_TKeys
#include <tvision/tv.h>

#include "windrag.h"

#include <algorithm>

namespace windrag
{

// The focused view and its size, as they were when nothing was being
// dragged: a drag that changes the height is a resize, otherwise a move.
static TView *lastView = nullptr;
static TPoint lastSize {0, 0};

void keepAboveStatusLine(TDeskTop &desk)
{
    TView *v = desk.current;
    if (!v || !(v->state & sfDragging))
    {
        lastView = v;
        if (v)
            lastSize = v->size;
        return;
    }
    TRect r = v->getBounds();
    int bottom = desk.size.y;
    if (r.b.y <= bottom)
        return;
    if (v == lastView && v->size.y != lastSize.y)
    {
        // Resized: the bottom edge stops at the status line (not below
        // the minimum height).
        TPoint min, max;
        v->sizeLimits(min, max);
        r.b.y = std::max(bottom, r.a.y + min.y);
    }
    else
    {
        // Moved: up until the bottom edge is on the desktop, but the top
        // row not above the desktop.
        int dy = std::min(r.b.y - bottom, (int) r.a.y);
        if (dy <= 0)
            return;
        r.move(0, -dy);
    }
    if (r != v->getBounds())
        v->locate(r);
}

bool keyboardDrag(TDeskTop &desk)
{
    auto *w = dynamic_cast<TWindow *>(desk.current);
    if (!w || !(w->flags & (wfMove | wfGrow)))
        return false;
    uchar mode = w->dragMode | (w->flags & (wfMove | wfGrow));
    TRect limits = desk.getExtent();
    TPoint min, max;
    w->sizeLimits(min, max);
    TRect saveBounds = w->getBounds();
    w->setState(sfDragging, True);
    TEvent ev;
    do
    {
        TPoint p = w->origin, s = w->size;
        w->keyEvent(ev);
        bool shift = (ev.keyDown.controlKeyState & kbShift) != 0;
        auto change = [&] (int dx, int dy) {
            if ((mode & dmDragMove) && !shift)
                p.x += dx, p.y += dy;
            else if ((mode & dmDragGrow) && shift)
                s.x += dx, s.y += dy;
        };
        switch (ev.keyDown.keyCode & 0xFF00)
        {
            case kbLeft & 0xFF00:      change(-1, 0); break;
            case kbRight & 0xFF00:     change(1, 0); break;
            case kbUp & 0xFF00:        change(0, -1); break;
            case kbDown & 0xFF00:      change(0, 1); break;
            case kbCtrlLeft & 0xFF00:  change(-8, 0); break;
            case kbCtrlRight & 0xFF00: change(8, 0); break;
            case kbCtrlUp & 0xFF00:    change(0, -4); break;
            case kbCtrlDown & 0xFF00:  change(0, 4); break;
            case kbHome & 0xFF00:      p.x = limits.a.x; break;
            case kbEnd & 0xFF00:       p.x = limits.b.x - s.x; break;
            case kbPgUp & 0xFF00:      p.y = limits.a.y; break;
            case kbPgDn & 0xFF00:      p.y = limits.b.y - s.y; break;
        }
        // As TView::moveGrow: the size within the limits, the title row
        // on the desktop, the dmLimit* flags.
        bool resized = s != w->size;
        s.x = std::min(std::max(s.x, min.x), max.x);
        s.y = std::min(std::max(s.y, min.y), max.y);
        if (resized)
            // The bottom edge stops at the status line.
            s.y = std::max(std::min(s.y, limits.b.y - p.y), std::min(min.y, s.y));
        p.x = std::min(std::max(p.x, limits.a.x - s.x + 1), limits.b.x - 1);
        p.y = std::min(std::max(p.y, limits.a.y - s.y + 1), limits.b.y - 1);
        if (mode & dmLimitLoX) p.x = std::max(p.x, limits.a.x);
        if (mode & dmLimitLoY) p.y = std::max(p.y, limits.a.y);
        if (mode & dmLimitHiX) p.x = std::min(p.x, limits.b.x - s.x);
        if (mode & dmLimitHiY) p.y = std::min(p.y, limits.b.y - s.y);
        if (!resized && p.y + s.y > limits.b.y)
            // Moved: the bottom edge stops at the status line.
            p.y = std::max(std::min(p.y, limits.b.y - s.y), std::min(w->origin.y, limits.a.y));
        TRect r(p.x, p.y, p.x + s.x, p.y + s.y);
        if (r != w->getBounds())
            w->locate(r);
    } while (ev.keyDown.keyCode != kbEsc && ev.keyDown.keyCode != kbEnter);
    if (ev.keyDown.keyCode == kbEsc)
        w->locate(saveBounds);
    w->setState(sfDragging, False);
    return true;
}

bool handleTopCorner(TDeskTop &desk, TEvent &event)
{
    if ( event.what != evMouseDown || event.mouse.buttons != mbLeftButton ||
         (event.mouse.eventFlags & meDoubleClick) )
        return false;
    auto *w = dynamic_cast<TWindow *>(desk.current);
    if ( !w || !w->frame || !(w->flags & wfGrow) || !(w->state & sfActive) ||
         !(w->state & sfVisible) )
        return false;
    TPoint m0 = desk.makeLocal(event.mouse.where);
    TPoint local = w->makeLocal(event.mouse.where);
    if (local.y != 0 || local.x < 0 || local.x >= w->size.x)
        return false;
    // The same columns as the bottom corners in tvision (TFrame): the two
    // at each end. The Close and Zoom buttons are further in.
    bool left;
    if (local.x <= 1)
        left = true;
    else if (local.x >= w->size.x - 2)
        left = false;
    else
        return false;

    TRect b0 = w->getBounds();
    TPoint min, max;
    w->sizeLimits(min, max);
    w->setState(sfDragging, True);
    TEvent ev = event;
    do
    {
        TPoint m = desk.makeLocal(ev.mouse.where);
        TRect r = b0;
        int top = b0.a.y + (m.y - m0.y);
        top = std::max(top, 0);                 // Not over the top bar.
        top = std::min(top, r.b.y - min.y);
        top = std::max(top, r.b.y - max.y);
        r.a.y = std::max(top, 0);
        if (left)
        {
            int ax = b0.a.x + (m.x - m0.x);
            ax = std::min(ax, r.b.x - min.x);
            ax = std::max(ax, r.b.x - max.x);
            r.a.x = ax;
        }
        else
        {
            int bx = b0.b.x + (m.x - m0.x);
            bx = std::max(bx, r.a.x + min.x);
            bx = std::min(bx, r.a.x + max.x);
            r.b.x = std::max(bx, 1);            // A column stays visible.
        }
        if (r != w->getBounds())
            w->locate(r);
    } while (w->mouseEvent(ev, evMouseMove));
    w->setState(sfDragging, False);
    return true;
}

} // namespace windrag
