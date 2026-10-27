#define Uses_TEditWindow
#include <tvision/tv.h>

#include "desk.h"

TVTermDesk::TVTermDesk(const TRect &bounds) :
    TDeskInit(&TDeskTop::initBackground),
    TDeskTop(bounds)
{
}

namespace tvtermdesk
{

static std::vector<TView *> children(TGroup &g)
{
    std::vector<TView *> views;
    if (TView *p = g.last)
        do {
            p = p->next;
            views.push_back(p);
        } while (p != g.last);
    return views;
}

static bool isEmpty(const TRect &r)
{
    return r.a.x >= r.b.x || r.a.y >= r.b.y;
}

static long area(const TRect &r)
{
    return isEmpty(r) ? 0 : (long) (r.b.x - r.a.x) * (r.b.y - r.a.y);
}

} // namespace tvtermdesk

bool TVTermDesk::isFixed(TView *p)
{
    return dynamic_cast<TWindow *>(p) && (p->state & sfVisible) &&
           ( dynamic_cast<TEditWindow *>(p) || !(p->options & ofTileable) );
}

std::vector<TView *> TVTermDesk::excludeFixed()
{
    std::vector<TView *> views;
    for (TView *p : tvtermdesk::children(*this))
        if (isFixed(p) && (p->options & ofTileable))
        {
            p->options &= ~ofTileable;
            views.push_back(p);
        }
    return views;
}

void TVTermDesk::restore(const std::vector<TView *> &views)
{
    for (TView *p : views)
        p->options |= ofTileable;
}

bool TVTermDesk::hasTileable()
{
    for (TView *p : tvtermdesk::children(*this))
        if ((p->options & ofTileable) && (p->state & sfVisible) && !isFixed(p))
            return true;
    return false;
}

TRect TVTermDesk::freeArea(const TRect &bounds)
{
    TRect fixed;
    bool any = false;
    for (TView *p : tvtermdesk::children(*this))
        if (isFixed(p))
        {
            TRect r = p->getBounds();
            r.intersect(bounds);
            if (tvtermdesk::isEmpty(r))
                continue;
            if (any)
                fixed.Union(r);
            else
                fixed = r, any = true;
        }
    return any ? freeArea(bounds, fixed) : bounds;
}

TRect TVTermDesk::freeArea(const TRect &b, const TRect &f)
{
    using namespace tvtermdesk;
    TRect left(b.a.x, b.a.y, f.a.x, b.b.y),
          right(f.b.x, b.a.y, b.b.x, b.b.y),
          top(b.a.x, b.a.y, b.b.x, f.a.y),
          bottom(b.a.x, f.b.y, b.b.x, b.b.y);
    // Candidates: the side opposite to each edge the fixed windows touch;
    // if they touch none, every side.
    std::vector<TRect> sides;
    if (f.a.x <= b.a.x) sides.push_back(right);
    if (f.b.x >= b.b.x) sides.push_back(left);
    if (f.a.y <= b.a.y) sides.push_back(bottom);
    if (f.b.y >= b.b.y) sides.push_back(top);
    if (sides.empty())
        sides = {left, right, top, bottom};
    TRect best = b;
    long bestArea = 0;
    for (const TRect &r : sides)
        if (area(r) > bestArea)
            best = r, bestArea = area(r);
    return best;
}

void TVTermDesk::tileWithOrientation(bool columnsFirst, const TRect &bounds)
{
    TRect r = freeArea(bounds);
    auto excluded = excludeFixed();
    bool bak = tileColumnsFirst;
    tileColumnsFirst = columnsFirst;
    tile(r);
    tileColumnsFirst = bak;
    restore(excluded);
}

void TVTermDesk::tileVertical(const TRect &bounds)
{
    tileWithOrientation(true, bounds);
}

void TVTermDesk::tileHorizontal(const TRect &bounds)
{
    tileWithOrientation(false, bounds);
}

void TVTermDesk::cascadeWindows(const TRect &bounds)
{
    TRect r = freeArea(bounds);
    auto excluded = excludeFixed();
    cascade(r);
    restore(excluded);
}
