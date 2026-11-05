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
    // Tile in 'n' columns ('columns') or rows, in the order the windows
    // were opened (as in the Window List): column by column, top to bottom
    // (rows: row by row, left to right); the first columns (rows) get the
    // windows left over and the cells left over. Fewer windows than 'n':
    // one column (row) each. 'n' is lowered while a column (row) would be
    // narrower (lower) than the windows' minimum size.
    struct GridResult
    {
        int used {0};          // Columns (rows) used; 0: no window.
        bool reduced {false};  // 'used' < n because of the minimum size.
        bool crowded {false};  // The windows of a column (row) do not fit
                               // the other way: some overlap.
        TPoint minSize {0, 0}; // The windows' minimum size.
    };
    GridResult tileGrid(int n, bool columns, const TRect &bounds);
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
