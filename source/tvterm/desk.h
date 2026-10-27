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

    // The part of 'bounds' to arrange the other windows in: the largest
    // rectangle that overlaps none of the fixed windows (each one counted
    // separately). 'bounds' itself when there are no fixed windows, or when
    // that rectangle is smaller than minFreeWidth x minFreeHeight (then
    // noFreeArea is set, so that the caller can say so).
    TRect freeArea(const TRect &bounds);
    static TRect freeArea(const TRect &bounds, const std::vector<TRect> &fixed);
    static constexpr short minFreeWidth = 20, minFreeHeight = 6;
    // Set by the last Tile or Cascade: the fixed windows left no room, so the
    // whole desktop was used.
    bool noFreeArea {false};

};

#endif // TVTERM_DESK_H
