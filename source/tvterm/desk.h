#ifndef TVTERM_DESK_H
#define TVTERM_DESK_H

#define Uses_TDeskTop
#define Uses_TBackground
#include <tvision/tv.h>

#include <string>
#include <vector>

// The desktop background (E74): 'cell' (one character of one column, UTF-8)
// repeated, so that [general] desktop-pattern can be any such character, not
// only one of tvision's codepage (CP437).
class TVTermBackground : public TBackground
{
public:

    TVTermBackground(const TRect &bounds) noexcept;
    void draw() override;

    std::string cell {"\u2591"}; // '░'
};

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
    static TBackground *initBackground(TRect r);

    // [general] desktop-pattern and desktop-pattern-on-capture (E74): the
    // background shown, and the one Capture Desk writes ('onCapture' is
    // 'shown' for 'same'). Redraws the background when 'shown' changes.
    void setPatterns(const std::string &shown, const std::string &onCapture);
    const std::string &pattern() const { return shownPattern; }
    const std::string &capturePattern() const { return capturedPattern; }
    // Draws the background with 'cell' (only its uncovered parts), without
    // changing the pattern of the settings: Capture Desk draws the capture
    // pattern, reads the screen, then draws pattern() back.
    void drawBackground(const std::string &cell);

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

private:

    std::string shownPattern {"\u2591"};
    std::string capturedPattern {" "};

};

#endif // TVTERM_DESK_H
