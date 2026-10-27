#ifndef TVTERM_DESK_H
#define TVTERM_DESK_H

#define Uses_TDeskTop
#include <tvision/tv.h>

#include <vector>

// Tile and Cascade arrange the terminals (and the other tileable windows),
// but the Event Viewer and the editor windows stay where they are: the
// others are arranged in the desktop area those leave free (see freeArea).
class TVTermDesk : public TDeskTop
{

    void tileWithOrientation(bool columnsFirst, const TRect &bounds);
    // Windows that Tile and Cascade leave where they are.
    static bool isFixed(TView *p);
    // Removes ofTileable from the fixed windows that have it (editors),
    // returning them so that it can be restored.
    std::vector<TView *> excludeFixed();
    static void restore(const std::vector<TView *> &views);

public:

    TVTermDesk(const TRect &bounds);

    void tileVertical(const TRect &bounds);
    void tileHorizontal(const TRect &bounds);
    void cascadeWindows(const TRect &bounds);
    // Whether there is a window that Tile or Cascade would arrange.
    bool hasTileable();

    // The part of 'bounds' to arrange the other windows in: 'bounds'
    // without the rectangle that encloses the fixed windows. If that
    // rectangle touches an edge of 'bounds', the area on the opposite side
    // (e.g. the windows are on the right: everything on their left);
    // otherwise, the largest area on any side. 'bounds' itself when there
    // are no fixed windows, or no room is left.
    TRect freeArea(const TRect &bounds);
    static TRect freeArea(const TRect &bounds, const TRect &fixed);

};

#endif // TVTERM_DESK_H
