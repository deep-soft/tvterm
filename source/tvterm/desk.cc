#define Uses_TEditWindow
#include <tvision/tv.h>

#include "desk.h"
#include "desktops.h"

#include <algorithm>

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
    std::vector<TRect> fixed;
    for (TView *p : tvtermdesk::children(*this))
        if (isFixed(p))
        {
            TRect r = p->getBounds();
            r.intersect(bounds);
            if (!tvtermdesk::isEmpty(r))
                fixed.push_back(r);
        }
    noFreeArea = false;
    if (fixed.empty())
        return bounds;
    TRect r = freeArea(bounds, fixed);
    if (r.b.x - r.a.x < minFreeWidth || r.b.y - r.a.y < minFreeHeight)
    {
        noFreeArea = true;
        return bounds;
    }
    return r;
}

TRect TVTermDesk::freeArea(const TRect &b, const std::vector<TRect> &fixed)
{
    using namespace tvtermdesk;
    // The largest rectangle inside 'b' that overlaps none of the fixed
    // windows. Its edges are edges of 'b' or of the fixed windows, so it is
    // enough to try those (there are only a few windows).
    std::vector<int> xs {b.a.x, b.b.x}, ys {b.a.y, b.b.y};
    for (const TRect &f : fixed)
    {
        xs.push_back(f.a.x); xs.push_back(f.b.x);
        ys.push_back(f.a.y); ys.push_back(f.b.y);
    }
    std::sort(xs.begin(), xs.end());
    xs.erase(std::unique(xs.begin(), xs.end()), xs.end());
    std::sort(ys.begin(), ys.end());
    ys.erase(std::unique(ys.begin(), ys.end()), ys.end());
    TRect best(b.a.x, b.a.y, b.a.x, b.a.y);
    long bestArea = 0;
    for (size_t x1 = 0; x1 < xs.size(); ++x1)
        for (size_t x2 = x1 + 1; x2 < xs.size(); ++x2)
            for (size_t y1 = 0; y1 < ys.size(); ++y1)
                for (size_t y2 = y1 + 1; y2 < ys.size(); ++y2)
                {
                    TRect r(xs[x1], ys[y1], xs[x2], ys[y2]);
                    long a = area(r);
                    if (a <= bestArea)
                        continue;
                    bool free = true;
                    for (const TRect &f : fixed)
                    {
                        TRect i = r;
                        i.intersect(f);
                        if (!isEmpty(i))
                        {
                            free = false;
                            break;
                        }
                    }
                    if (free)
                        best = r, bestArea = a;
                }
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

TVTermDesk::GridResult TVTermDesk::tileGrid(int n, bool columns, const TRect &bounds)
{
    GridResult res;
    TRect r = freeArea(bounds);
    // The windows opened just now get their order first.
    desktops::prune(*this);
    std::vector<TView *> views;
    for (TView *p : tvtermdesk::children(*this))
        if ((p->options & ofTileable) && (p->state & sfVisible) && !isFixed(p))
            views.push_back(p);
    if (views.empty() || n < 1)
        return res;
    auto order = [] (TView *v) {
        auto *w = dynamic_cast<TWindow *>(v);
        return w ? desktops::openedOrder(w) : (unsigned long) -1;
    };
    std::stable_sort(views.begin(), views.end(), [&] (TView *a, TView *b) {
        return order(a) < order(b);
    });
    for (TView *p : views)
    {
        TPoint mn, mx;
        p->sizeLimits(mn, mx);
        res.minSize.x = std::max(res.minSize.x, mn.x);
        res.minSize.y = std::max(res.minSize.y, mn.y);
    }
    int k = (int) views.size();
    // 'along': the size split into the n columns (rows); 'across': the
    // size of a column (row), split among its windows.
    int along = columns ? r.b.x - r.a.x : r.b.y - r.a.y;
    int across = columns ? r.b.y - r.a.y : r.b.x - r.a.x;
    int minAlong = columns ? res.minSize.x : res.minSize.y;
    int minAcross = columns ? res.minSize.y : res.minSize.x;
    int lines = std::min(n, k);
    int wanted = lines;
    while (lines > 1 && along / lines < minAlong)
        --lines;
    res.reduced = lines < wanted;
    int perLine = k / lines, extra = k % lines;
    res.crowded = across / (perLine + (extra ? 1 : 0)) < minAcross;
    int idx = 0;
    int pos = columns ? r.a.x : r.a.y;
    for (int i = 0; i < lines; ++i)
    {
        int size = along / lines + (i < along % lines ? 1 : 0);
        int count = perLine + (i < extra ? 1 : 0);
        int pos2 = columns ? r.a.y : r.a.x;
        for (int j = 0; j < count; ++j)
        {
            int size2 = across / count + (j < across % count ? 1 : 0);
            TRect cell = columns ? TRect(pos, pos2, pos + size, pos2 + size2)
                                 : TRect(pos2, pos, pos2 + size2, pos + size);
            views[idx++]->locate(cell);
            pos2 += size2;
        }
        pos += size;
    }
    res.used = lines;
    return res;
}

void TVTermDesk::cascadeWindows(const TRect &bounds)
{
    TRect r = freeArea(bounds);
    auto excluded = excludeFixed();
    cascade(r);
    restore(excluded);
}
