#ifndef TVTERM_TERMEMU_H
#define TVTERM_TERMEMU_H

#include <tvterm/pty.h>
#include <tvterm/array.h>
#include <tvterm/mutex.h>
#include <vector>
#include <string>
#include <stdint.h>

#define Uses_TDrawSurface
#define Uses_TEvent
#include <tvision/tv.h>

namespace tvterm
{

struct RowRange
{
    // The range is valid when 'start < end'.
    // Otherwise, we use {INT_MAX, INT_MIN}, which makes it simple to adjust
    // ranges with min()/max().
    int start {INT_MAX};
    int end {INT_MIN};

    RowRange() = default;
    RowRange(int start, int end) noexcept : start(start), end(end) {}

    int size() const noexcept;
    RowRange intersect(RowRange other) const noexcept;

    friend bool operator!=(const RowRange &a, const RowRange &b) noexcept;
};

inline int RowRange::size() const noexcept
{
    if (start < end)
        return end - start;
    return 0;
}

inline RowRange RowRange::intersect(RowRange other) const noexcept
{
    return {
        max(start, other.start),
        min(end, other.end),
    };
}

inline bool operator!=(const RowRange &a, const RowRange &b) noexcept
{
    return a.start != b.start || a.end != b.end;
}

struct SelectionAnchor
{
    // In absolute coordinates: y = 0 is the oldest scrollback line.
    int x, y;

    SelectionAnchor() = default; // Trivial constructor required by TerminalEvent.
    SelectionAnchor(int x, int y) noexcept : x(x), y(y) {}
    SelectionAnchor(TPoint p) noexcept : x(p.x), y(p.y) {}

    operator TPoint() const noexcept { return {x, y}; }

    friend bool operator<(const SelectionAnchor &a, const SelectionAnchor &b) noexcept;
    friend bool operator>(const SelectionAnchor &a, const SelectionAnchor &b) noexcept;
};

inline bool operator<(const SelectionAnchor &a, const SelectionAnchor &b) noexcept
{
    return a.y < b.y || (a.y == b.y && a.x < b.x);
}

inline bool operator>(const SelectionAnchor &a, const SelectionAnchor &b) noexcept
{
    return b < a;
}

struct SelectionRange
{
    // Both 'start < end' and 'start > end' are valid.
    // When 'start == end', there is no selection.
    // Selection goes from {start.x, start.y} to {end.x - 1, end.y}
    SelectionAnchor start;
    SelectionAnchor end;

    bool empty() const noexcept;
    RowRange intersectRow(int y, int cols) const noexcept;
};

inline bool SelectionRange::empty() const noexcept
{
    return start == end;
}

inline RowRange SelectionRange::intersectRow(int y, int cols) const noexcept
{
    SelectionRange normalized = {
        min(start, end),
        max(start, end),
    };

    if (empty() || y < normalized.start.y || y > normalized.end.y)
        return {INT_MAX, INT_MIN};

    return {
        (y == normalized.start.y) ? max(normalized.start.x, 0) : 0,
        (y == normalized.end.y) ? min(normalized.end.x, cols) : cols,
    };
}

// Plain text of the terminal's scrollback and screen, one entry per logical
// line (rows joined when a line was wrapped by the terminal's width).
// Used for searching, saving and selecting words or lines.

struct TextLine
{
    // Codepoints of the line. Empty cells followed by text become spaces;
    // trailing empty cells are left out.
    std::u32string text;
    // Only filled when positions are requested:
    // - 'charCell[i]' is the index in 'cells' of the cell 'text[i]' belongs to
    //   (a cell may hold several codepoints, e.g. combining characters);
    // - 'cells[j]' is the absolute position of a cell (y = 0 is the oldest
    //   scrollback line), and 'cellWidth[j]' its width (1 or 2).
    std::vector<int> charCell;
    std::vector<SelectionAnchor> cells;
    std::vector<uint8_t> cellWidth;
    int firstRow {0}; // Absolute rows covered by the line.
    int lastRow {0};
};

struct TextSnapshot
{
    std::vector<TextLine> lines;
    int scrollbackLines {0}; // Rows below this are the screen.
    int screenRows {0};
    bool altScreen {false};  // When true, only the screen is included.
};

// A TDrawSurface that can keep track of the areas that were modified.
// Otherwise, the view displaying this surface would have to copy all of
// it every time, which doesn't scale well when using big resolutions.

class TerminalSurface : private TDrawSurface
{
public:

    using TDrawSurface::size;

    void resize(TPoint aSize);
    void clearDamage();
    using TDrawSurface::at;
    RowRange &damageAtRow(size_t y);
    const RowRange &damageAtRow(size_t y) const;
    void addDamageAtRow(size_t y, int begin, int end);

private:

    std::vector<RowRange> damageByRow;
};

inline void TerminalSurface::resize(TPoint aSize)
{
    if (aSize != size)
    {
        TDrawSurface::resize(aSize);
        // The surface's contents are not relevant after the resize.
        clearDamage();
    }
}

inline void TerminalSurface::clearDamage()
{
    damageByRow.resize(0);
    damageByRow.resize(max(0, size.y));
}

inline RowRange &TerminalSurface::damageAtRow(size_t y)
{
    return damageByRow[y];
}

inline const RowRange &TerminalSurface::damageAtRow(size_t y) const
{
    return damageByRow[y];
}

inline void TerminalSurface::addDamageAtRow(size_t y, int begin, int end)
{
    auto &damage = damageAtRow(y);
    damage = {
        min(begin, damage.start),
        max(end, damage.end),
    };
}

// Struct that allows the terminal state to be reported from the TerminalEmulator
// to the TerminalController (and from there to the TerminalView, etc.)

// A request from the client application (far2l) to read the clipboard.
struct Far2lClipboardRequest
{
    uint8_t requestId;
    uint32_t format;
};

struct TerminalState
{
    TerminalSurface surface;

    bool cursorChanged {false};
    TPoint cursorPos {0, 0};
    bool cursorVisible {false};
    bool cursorBlink {false};

    bool titleChanged {false};
    GrowArray title;

    bool scrollbackChanged {false};
    int scrollbackOffset {0}; // 0 (oldest) to 'scrollbackLimit' (no scrollback shown)
    int scrollbackLimit {0};
    bool scrollbackEnabled {true};
    // Total number of scrollback lines dropped so far (oldest lines removed
    // when the scrollback is full). Absolute rows shift up by one for each.
    uint64_t scrollbackDropped {0};

    bool mouseEnabled {false};

    // Text that the client application asked to put in the clipboard
    // (e.g. via OSC 52). Must be handled from the main thread.
    bool clipboardChanged {false};
    GrowArray clipboardText;

    // Clipboard read requests from far2l (see far2lext.h). Must be handled
    // from the main thread, and answered with a Far2lClipboardData event.
    std::vector<Far2lClipboardRequest> far2lClipboardRequests;

    // Whether the client application uses far2l's terminal extensions,
    // and therefore has its own access to the clipboard.
    bool far2lExtensionsEnabled {false};

    // Output of the client application, used for alerts: the total number
    // of bytes received so far, and the text received since the main thread
    // last took it (see OutputTextFilter; at most 'maxNewOutputText' bytes,
    // the most recent ones).
    uint64_t outputBytes {0};
    std::string newOutputText;
    enum { maxNewOutputText = 64 * 1024 };
};

enum class TerminalEventType
{
    KeyDown,
    Mouse,
    ClientDataRead,
    ViewportResize,
    FocusChange,
    ScrollBackOffsetChange,
    ScrollBackScroll,
    CopySelection,
    Far2lClipboardData,
    Paste,
};

struct MouseEvent
{
    ushort what;
    MouseEventType mouse;
};

struct ClientDataReadEvent
{
    const char *data;
    size_t size;
};

struct ViewportResizeEvent
{
    int x, y;
};

struct FocusChangeEvent
{
    bool focusEnabled;
};

struct ScrollBackOffsetChangeEvent
{
    int offset;
};

struct ScrollBackScrollEvent
{
    // < 0: show older lines; > 0: show newer lines.
    // 'scrollToTop' / 'scrollToBottom' go to the oldest line / the screen.
    enum { scrollToTop = -0x40000000, scrollToBottom = 0x40000000 };
    int delta;
};

struct CopySelectionEvent
{
    SelectionRange selection;
};

struct Far2lClipboardDataEvent
{
    uint8_t requestId;
    uint32_t format;
    // UTF-8 text allocated with 'new char[]'. The receiver must delete it.
    char *data;
    size_t size;
};

struct PasteEvent
{
    // UTF-8 text allocated with 'new char[]'. The receiver must delete it.
    char *data;
    size_t size;
};

struct TerminalEvent
{
    TerminalEventType type;
    union
    {
        ::KeyDownEvent keyDown;
        MouseEvent mouse;
        ClientDataReadEvent clientDataRead;
        ViewportResizeEvent viewportResize;
        FocusChangeEvent focusChange;
        ScrollBackOffsetChangeEvent scrollBackOffsetChange;
        ScrollBackScrollEvent scrollBackScroll;
        CopySelectionEvent copySelection;
        Far2lClipboardDataEvent far2lClipboardData;
        PasteEvent paste;
    };
};

class TerminalEmulator
{
public:

    virtual ~TerminalEmulator() = default;

    // Gets called whenever the user interacts with the terminal or the
    // client process sends data.
    virtual void handleEvent(const TerminalEvent &event) noexcept = 0;
    // Gets called shortly after 'handleEvent()', so that any changes that
    // may have occurred can be reported via the TerminalState.
    virtual void updateState(TerminalState &state) noexcept = 0;

    // Text access, used from the main thread through
    // 'TerminalController::withEmulator()'.
    // Fills 'snapshot' with the logical lines that touch the absolute rows
    // [startRow, endRow). Positions are only filled if 'withPositions'.
    virtual void getText( TextSnapshot &snapshot, int startRow, int endRow,
                          bool withPositions ) noexcept
        { (void) snapshot; (void) startRow; (void) endRow; (void) withPositions; }
    // Text of a selection, as it would be copied to the clipboard.
    virtual void getSelectionText(SelectionRange selection, std::string &text) noexcept
        { (void) selection; (void) text; }
};

class Writer
{
public:

    virtual void write(TSpan<const char> data) noexcept = 0;
};

class TerminalEmulatorFactory
{
public:

    // Function returning a new-allocated TerminalEmulator.
    // 'clientDataWriter' is a non-owning reference and exceeds the lifetime
    // of the returned TerminalEmulator.
    virtual TerminalEmulator &create( TPoint size,
                                      Writer &clientDataWriter ) noexcept = 0;

    // Returns environment variables that the TerminalEmulator requires the
    // client process to have.
    // The lifetime of the returned value must not be shorter than that of 'this'.
    virtual TSpan<const EnvironmentVar> getCustomEnvironment() noexcept = 0;
};

} // namespace tvterm

#endif // TVTERM_TERMEMU_H
