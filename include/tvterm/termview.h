#ifndef TVTERM_TERMVIEW_H
#define TVTERM_TERMVIEW_H

#define Uses_TView
#define Uses_TGroup
#include <tvision/tv.h>

#include <tvterm/termemu.h>
#include <memory>
#include <string>

struct MouseEventType;
class TScrollBar;

namespace tvterm
{

class TerminalController;
struct TVTermConstants;
struct TerminalSearch;

class TerminalView : public TView
{
    const TVTermConstants &consts;
    TScrollBar *scrollBar {nullptr};
    bool ownerBufferChanged {false};

    SelectionRange selection {};
    SelectionRange prevSelection {};
    bool inSelectionMode {false};
    // Selection by words (double click) or lines (triple click): the unit
    // that was clicked first, which stays selected while dragging.
    enum SelectUnit { unitChar, unitWord, unitLine } selectUnit {unitChar};
    SelectionAnchor unitStart {}, unitEnd {};
    bool mouseSelecting {false}; // Mouse button held down while selecting.

    // History mode: keys scroll the scrollback instead of going to the
    // application. 'historyAutoExit': entered with a direct key, so that
    // reaching the bottom leaves the mode.
    bool historyMode {false};
    bool historyAutoExit {false};
    // Last search match (highlighted), in absolute coordinates.
    SelectionRange match {};
    SelectionRange prevMatch {};
    std::unique_ptr<TerminalSearch> search;

    // Copies of the terminal state, for use outside of 'lockState()'.
    int sbOffset {0};
    int sbLimit {0};
    bool sbEnabled {true};
    uint64_t sbDropped {0};

    void handleMouse(ushort what, MouseEventType mouse) noexcept;
    bool handleHistoryHotkey(TEvent &ev) noexcept;
    bool handleHistoryKey(TEvent &ev) noexcept;
    void scrollHistory(int delta) noexcept;
    void shiftAnchors(int delta) noexcept;
    bool getUnitAt(SelectionAnchor pos, SelectUnit unit, SelectionAnchor &start, SelectionAnchor &end) noexcept;
    void copyMatchOrSelection() noexcept;
    void showMessage(const std::string &text) noexcept;
    void updateWindowTitle() noexcept;
    void updateHelpContext() noexcept;
    void updateClipboard() noexcept;
    bool pasteOnShiftIns() noexcept;
    void sendPaste(TEvent &ev) noexcept;
    void updateCursor(TerminalState &state) noexcept;
    void updateScrollBar(TerminalState &state) noexcept;
    void updateDisplay(TerminalState &state) noexcept;
    bool canReuseOwnerBuffer() noexcept;
    TPoint getCursorDisplayedPos(TerminalState &state) noexcept;

    void startSelection(SelectionAnchor pos = {}) noexcept;
    void startUnitSelection(SelectionAnchor pos, SelectUnit unit) noexcept;
    void extendSelection(SelectionAnchor pos) noexcept;
    void cancelSelection() noexcept;
    void copySelection() noexcept;

public:

    TerminalController &termCtrl;

    // Takes ownership over 'termCtrl'.
    // The lifetime of 'consts' must exceed that of 'this'.
    // 'scrollBar' is a non-owning reference.
    TerminalView( const TRect &bounds, TerminalController &termCtrl,
                  const TVTermConstants &consts, TScrollBar *scrollBar ) noexcept;
    ~TerminalView();

    void shutDown() override;
    void changeBounds(const TRect& bounds) override;
    void setState(ushort aState, bool enable) override;
    void handleEvent(TEvent &ev) override;
    void draw() override;

    // History mode (scrolling, searching and saving the scrollback).
    bool inHistoryMode() const noexcept { return historyMode; }
    // Returns false if the application is in the alternate screen
    // (full-screen applications have no scrollback).
    bool enterHistoryMode(bool autoExit = false) noexcept;
    void exitHistoryMode() noexcept;
    bool hasScrollback() const noexcept { return sbEnabled; }
    bool hasSelection() const noexcept { return !selection.empty(); }

    // Sets the text to search for. Returns false, with 'error' set, if
    // 'regex' is true and 'text' is not a valid regular expression.
    bool setSearch( const std::u32string &text, bool regex, bool caseSensitive,
                    std::string &error ) noexcept;
    // Last search, to fill in the search dialog. False if there is none.
    bool getSearch(std::u32string &text, bool &regex, bool &caseSensitive) const noexcept;
    // Finds the next match: 'older' (towards the oldest line) or newer.
    // 'fromView': start from the visible area instead of the last match.
    // Returns whether a match was found.
    bool findNext(bool older, bool fromView = false) noexcept;

    // UTF-8 text of the whole scrollback and screen (or of the selection),
    // as plain text. 'screenOnly' is set if only the screen could be
    // included (full-screen application).
    void getHistoryText(std::string &text, bool selectionOnly, bool &screenOnly) noexcept;
};

} // namespace tvterm

#endif // TVTERM_TERMVIEW_H
