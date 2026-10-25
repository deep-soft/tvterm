#include <tvterm/termview.h>
#include <tvterm/termctrl.h>
#include <tvterm/consts.h>
#include <tvterm/termopts.h>
#include "util.h"

#define Uses_TKeys
#define Uses_TEvent
#define Uses_TScrollBar
#define Uses_TClipboard
#define Uses_THardwareInfo
#define Uses_TWindow
#define Uses_TFrame
#include <tvision/tv.h>

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <chrono>
#include <string>
#include <vector>
#include <regex>
#include <climits>

namespace tvterm
{

TerminalOptions terminalOptions;

// Searching the scrollback (history mode).
struct TerminalSearch
{
    std::u32string text;
    bool regex {false};
    bool caseSensitive {false};
    std::u32string foldedText; // 'text', lowercase (plain search).
    std::wregex re;
};

namespace termsearch
{

static_assert(sizeof(wchar_t) == sizeof(char32_t), "wchar_t must hold any codepoint");

static char32_t foldCase(char32_t c) noexcept
// Lowercase for the letters commonly found in logs (ASCII, Latin-1, Latin
// Extended-A, Romanian, Greek, Cyrillic). Independent from the locale.
{
    if (c < 0x80)
        return ('A' <= c && c <= 'Z') ? c + 32 : c;
    if (0xC0 <= c && c <= 0xDE && c != 0xD7)
        return c + 32;
    if (0x100 <= c && c <= 0x17F)
    {
        if (c == 0x130) return U'i';
        if (c == 0x178) return 0xFF;
        if (c == 0x131 || c == 0x138 || c == 0x149 || c == 0x17F)
            return c;
        bool oddUpper = (0x139 <= c && c <= 0x148) || (0x179 <= c && c <= 0x17E);
        if (oddUpper)
            return (c & 1) ? c + 1 : c;
        return (c & 1) ? c : c + 1;
    }
    if (0x218 <= c && c <= 0x21B) // Romanian S/T with comma below.
        return (c & 1) ? c : c + 1;
    if (0x391 <= c && c <= 0x3AB && c != 0x3A2)
        return c + 32;
    if (0x410 <= c && c <= 0x42F)
        return c + 32;
    if (0x400 <= c && c <= 0x40F)
        return c + 80;
    return c;
}

static std::u32string fold(const std::u32string &s)
{
    std::u32string r(s);
    for (auto &c : r)
        c = foldCase(c);
    return r;
}

static void findMatches( const TerminalSearch &search, const std::u32string &text,
                         std::vector<std::pair<int, int>> &out )
// Fills 'out' with the [begin, end) indices of the matches in 'text', in order.
{
    out.clear();
    if (search.text.empty())
        return;
    if (search.regex)
    {
        std::wstring w(text.begin(), text.end());
        auto begin = std::wsregex_iterator(w.begin(), w.end(), search.re);
        for (auto it = begin; it != std::wsregex_iterator(); ++it)
            if (it->length(0) > 0)
            {
                int b = (int) it->position(0);
                out.push_back({b, b + (int) it->length(0)});
            }
    }
    else
    {
        const std::u32string &needle = search.caseSensitive ? search.text : search.foldedText;
        std::u32string haystack = search.caseSensitive ? text : fold(text);
        size_t pos = 0;
        while ((pos = haystack.find(needle, pos)) != std::u32string::npos)
        {
            out.push_back({(int) pos, (int) (pos + needle.size())});
            pos += 1;
        }
    }
}

static bool isWordChar(char32_t c) noexcept
{
    if (c >= 0x80)
        return true;
    if (('0' <= c && c <= '9') || ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z'))
        return true;
    return terminalOptions.wordChars.find(c) != std::u32string::npos;
}

static void appendUtf8(std::string &out, const std::u32string &text)
{
    for (char32_t c : text)
    {
        char utf8[4];
        size_t len = utf32To8((uint32_t) c, utf8);
        out.append(utf8, len);
    }
}

} // namespace termsearch

// Debug log for clipboard requests from client applications (OSC 52 and
// far2l extensions). Enabled by setting TVTERM_CLIPBOARD_LOG to a file path.
static void logClipboard(const char *what, const char *text, size_t size) noexcept
{
    static const char *path = getenv("TVTERM_CLIPBOARD_LOG");
    if (!path || !*path)
        return;
    if (FILE *f = fopen(path, "a"))
    {
        char timeBuf[32] = "";
        time_t now = time(nullptr);
        if (struct tm *tm = localtime(&now))
            strftime(timeBuf, sizeof(timeBuf), "%H:%M:%S", tm);
        fprintf(f, "%s %s", timeBuf, what);
        if (text)
        {
            fprintf(f, ": %zu bytes: \"", size);
            for (size_t i = 0; i < size && i < 60; ++i)
            {
                unsigned char c = (unsigned char) text[i];
                fputc(c < 0x20 || c == 0x7F ? '.' : c, f);
            }
            fprintf(f, "%s\"", size > 60 ? "..." : "");
        }
        fputc('\n', f);
        fclose(f);
    }
}

// Reading the system clipboard for far2l (see far2lext.h).
//
// Reading is asynchronous: the request goes to the terminal tvterm runs in
// (e.g. far2l via its extensions), and the text arrives later through
// 'acceptClipboardText'. Since that callback has no context, pending
// requests are kept here. Everything runs in the main thread.
namespace far2lclip
{

struct PendingRead
{
    TerminalView *view;
    uint8_t requestId;
    uint32_t format;
};

static std::vector<PendingRead> pendingReads;
static bool readInProgress = false;
static std::chrono::steady_clock::time_point readDeadline;
// Last text put in the clipboard by tvterm, used when the system clipboard
// cannot be read (no far2l extensions or OSC 52 in the outer terminal).
static std::string lastSetText;

enum { readTimeoutMs = 3000 };

static void deliver(TStringView text) noexcept
{
    logClipboard("far2l clipboard read: answered", text.data(), text.size());
    auto reads = std::move(pendingReads);
    pendingReads.clear();
    readInProgress = false;
    for (auto &read : reads)
    {
        char *data = new char[text.size() ? text.size() : 1];
        memcpy(data, text.data(), text.size());
        TerminalEvent termEvent;
        termEvent.type = TerminalEventType::Far2lClipboardData;
        termEvent.far2lClipboardData = {read.requestId, read.format, data, text.size()};
        read.view->termCtrl.sendEvent(termEvent);
    }
}

static void acceptClipboardText(TStringView text) noexcept
{
    if (readInProgress)
        deliver(text);
}

static void request(TerminalView *view, const Far2lClipboardRequest &req) noexcept
{
    logClipboard("far2l clipboard read: requested", nullptr, 0);
    pendingReads.push_back({view, req.requestId, req.format});
    if (!readInProgress)
    {
        readInProgress = true;
        readDeadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(readTimeoutMs);
        if (!THardwareInfo::requestClipboardText(acceptClipboardText))
            deliver(lastSetText);
    }
}

static void checkTimeout() noexcept
{
    // far2l waits forever for an answer, so never leave it without one.
    if (readInProgress && std::chrono::steady_clock::now() > readDeadline)
    {
        logClipboard("far2l clipboard read: timed out", nullptr, 0);
        deliver({});
    }
}

static void forget(TerminalView *view) noexcept
{
    for (size_t i = 0; i < pendingReads.size();)
        if (pendingReads[i].view == view)
            pendingReads.erase(pendingReads.begin() + i);
        else
            ++i;
}

} // namespace far2lclip

TerminalView::TerminalView( const TRect &bounds, TerminalController &aTermCtrl,
                            const TVTermConstants &aConsts, TScrollBar *aScrollBar ) noexcept :
    TView(bounds),
    consts(aConsts),
    scrollBar(aScrollBar),
    termCtrl(aTermCtrl)
{
    growMode = gfGrowHiX | gfGrowHiY;
    options |= ofSelectable | ofFirstClick;
    eventMask |= evMouseMove | evMouseAuto | evMouseWheel | evBroadcast;
    showCursor();
}

TerminalView::~TerminalView()
{
    far2lclip::forget(this);
    termCtrl.shutDown();
}

void TerminalView::shutDown()
{
    scrollBar = nullptr;
    TView::shutDown();
}

void TerminalView::changeBounds(const TRect& bounds)
{
    setBounds(bounds);
    ownerBufferChanged = true;
    drawView();

    TerminalEvent termEvent;
    termEvent.type = TerminalEventType::ViewportResize;
    termEvent.viewportResize = {size.x, size.y};
    termCtrl.sendEvent(termEvent);
}

void TerminalView::setState(ushort aState, bool enable)
{
    if (aState == sfExposed && enable != getState(sfExposed))
        ownerBufferChanged = true;

    TView::setState(aState, enable);

    if (aState == sfFocused)
    {
        TerminalEvent termEvent;
        termEvent.type = TerminalEventType::FocusChange;
        termEvent.focusChange = {enable};
        termCtrl.sendEvent(termEvent);
    }
}

void TerminalView::handleEvent(TEvent &ev)
{
    TView::handleEvent(ev);

    switch (ev.what)
    {
        case evCommand:
            if (ev.message.command == consts.cmHistoryMode)
            {
                // In full-screen applications there is no scrollback, but
                // the visible screen can still be saved.
                if (!enterHistoryMode() && terminalOptions.openSave)
                    terminalOptions.openSave(*this);
                clearEvent(ev);
            }
            else if (ev.message.command == consts.cmStartSelection)
            {
                startSelection();
                helpCtx = consts.hcSelecting;
                clearEvent(ev);
            }
            else if (ev.message.command == consts.cmCopySelection)
            {
                if (historyMode)
                    copyMatchOrSelection();
                else
                    copySelection();
                clearEvent(ev);
            }
            else if (ev.message.command == consts.cmCancelSelection)
            {
                cancelSelection();
                clearEvent(ev);
            }
            else if (ev.message.command == cmPaste)
            {
                TClipboard::requestText();
                clearEvent(ev);
            }
            break;

        case evBroadcast:
            if (ev.message.command == consts.cmCheckTerminalUpdates)
                far2lclip::checkTimeout();
            if ( ev.message.command == consts.cmCheckTerminalUpdates &&
                 termCtrl.stateHasBeenUpdated() )
            {
                updateClipboard();
                drawView();
                // A full-screen application started: no more scrollback.
                if (historyMode && !sbEnabled)
                    exitHistoryMode();
            }
            else if ( ev.message.command == cmScrollBarChanged &&
                      ev.message.infoPtr == scrollBar && scrollBar )
            {
                // Handle scrollbar changes by sending a scroll event to the terminal
                int newOffset = scrollBar->value;
                TerminalEvent termEvent;
                termEvent.type = TerminalEventType::ScrollBackOffsetChange;
                termEvent.scrollBackOffsetChange = {newOffset};
                termCtrl.sendEvent(termEvent);
                clearEvent(ev);
            }
            break;

        case evKeyDown:
        {
            if (historyMode)
            {
                if (!(ev.keyDown.controlKeyState & kbPaste) && handleHistoryKey(ev))
                {
                    clearEvent(ev);
                    break;
                }
                // Any other key leaves history mode and goes to the
                // application, as usual.
                exitHistoryMode();
            }
            else if (handleHistoryHotkey(ev))
            {
                clearEvent(ev);
                break;
            }
            if (ev.keyDown.controlKeyState & kbPaste)
            {
                // Pasted text (from Ctrl-B P, Shift-Ins or the outer
                // terminal, e.g. PuTTY's right click): send it all at once,
                // so that it can be marked as a paste (bracketed paste).
                sendPaste(ev);
                clearEvent(ev);
                break;
            }
            if (ev.keyDown.keyCode == kbShiftIns && pasteOnShiftIns())
            {
                // Paste from the system clipboard (e.g. the Windows
                // clipboard when running in far2l/putty4far2l over SSH).
                TClipboard::requestText();
                clearEvent(ev);
                break;
            }
            TerminalEvent termEvent;
            termEvent.type = TerminalEventType::KeyDown;
            termEvent.keyDown = ev.keyDown;
            termCtrl.sendEvent(termEvent);
            clearEvent(ev);
            break;
        }

        case evMouseDown:
            // Use a loop so that we can keep handling mouse events even if the
            // mouse is dragged out of the window.
            do {
                handleMouse(ev.what, ev.mouse);
            } while (mouseEvent(ev, evMouse));
            if (ev.what == evMouseUp)
                handleMouse(ev.what, ev.mouse);
            clearEvent(ev);
            break;

        case evMouseWheel:
            if (historyMode)
            {
                int lines = max(1, (int) terminalOptions.wheelLines);
                if (ev.mouse.wheel == mwUp)
                    scrollHistory(-lines);
                else if (ev.mouse.wheel == mwDown)
                    scrollHistory(lines);
                clearEvent(ev);
                break;
            }
            handleMouse(ev.what, ev.mouse);
            clearEvent(ev);
            break;

        case evMouseMove:
        case evMouseAuto:
        case evMouseUp:
            handleMouse(ev.what, ev.mouse);
            clearEvent(ev);
            break;
    }
}

void TerminalView::sendPaste(TEvent &ev) noexcept
// Collects the text of consecutive paste events, starting with 'ev'.
{
    std::string text;
    do {
        if (ev.keyDown.textLength)
            text.append(ev.keyDown.text, ev.keyDown.textLength);
        else if (ev.keyDown.keyCode == kbEnter)
            text += '\n';
        else if (ev.keyDown.keyCode == kbTab)
            text += '\t';
        int timeoutMs = 0;
        getEvent(ev, timeoutMs);
    } while (ev.what == evKeyDown && (ev.keyDown.controlKeyState & kbPaste));
    // Give back the first event that is not part of the paste.
    if (ev.what != evNothing)
        putEvent(ev);

    if (!text.empty())
    {
        char *data = new char[text.size()];
        memcpy(data, text.data(), text.size());
        TerminalEvent termEvent;
        termEvent.type = TerminalEventType::Paste;
        termEvent.paste = {data, text.size()};
        termCtrl.sendEvent(termEvent);
    }
}

bool TerminalView::pasteOnShiftIns() noexcept
// Shift-Ins pastes from the system clipboard, except:
// - in 'Grab Input' mode, where all keys go to the application;
// - when the application uses far2l's terminal extensions, because it can
//   then access the clipboard by itself (and paste better, e.g. without
//   triggering auto-indentation).
{
    if (owner && owner->helpCtx == consts.hcInputGrabbed)
        return false;
    bool far2lEnabled = false;
    termCtrl.lockState([&] (auto &state) {
        far2lEnabled = state.far2lExtensionsEnabled;
    });
    return !far2lEnabled;
}

void TerminalView::handleMouse(ushort what, MouseEventType mouse) noexcept
{
    mouse.where = makeLocal(mouse.where);

    TPoint absPos = mouse.where;
    bool clientIsUsingMouse;
    termCtrl.lockState([&] (auto &state) {
        absPos.y += state.scrollbackOffset;
        clientIsUsingMouse = state.mouseEnabled;
    });
    // Shift+click selects text even if the application uses the mouse
    // (e.g. far2l), if enabled in the options.
    bool forceSelect = clientIsUsingMouse && terminalOptions.shiftClickSelect &&
                       (mouse.controlKeyState & kbShift);

    // Left click: start selection. Double click: select a word; triple
    // click: select a line.
    if ( (inSelectionMode || !clientIsUsingMouse || forceSelect) &&
         what == evMouseDown && (mouse.buttons & mbLeftButton) )
    {
        mouseSelecting = true;
        if (mouse.eventFlags & meTripleClick)
            startUnitSelection(absPos, unitLine);
        else if (mouse.eventFlags & meDoubleClick)
            startUnitSelection(absPos, unitWord);
        else
            startSelection(absPos);
    }
    // Mouse drag: extend selection.
    else if (inSelectionMode && what == evMouseMove && (mouse.buttons & mbLeftButton))
        extendSelection(absPos);
    // Empty selection: stop selecting.
    else if (inSelectionMode && what == evMouseUp && selection.empty())
    {
        mouseSelecting = false;
        cancelSelection();
    }
    // The end of a selection made with the mouse is not for the application.
    else if (mouseSelecting && (what == evMouseUp || what == evMouseMove || what == evMouseAuto))
    {
        if (what == evMouseUp)
            mouseSelecting = false;
    }
    else
    {
        // Pass mouse event to terminal emulator.
        TerminalEvent termEvent;
        termEvent.type = TerminalEventType::Mouse;
        termEvent.mouse = {what, mouse};
        termCtrl.sendEvent(termEvent);
    }
}

void TerminalView::updateClipboard() noexcept
// Forwards the clipboard requests of the client application (OSC 52) to
// the system clipboard. Done here because it must happen in the main thread.
{
    termCtrl.lockState([&] (auto &state) {
        if (state.clipboardChanged)
        {
            state.clipboardChanged = false;
            logClipboard( "Application sets clipboard -> TClipboard::setText",
                          state.clipboardText.data(), state.clipboardText.size() );
            TStringView text {state.clipboardText.data(), state.clipboardText.size()};
            far2lclip::lastSetText.assign(text.data(), text.size());
            TClipboard::setText(text);
            state.clipboardText.clear();
        }
        for (auto &req : state.far2lClipboardRequests)
            far2lclip::request(this, req);
        state.far2lClipboardRequests.clear();
    });
}

void TerminalView::draw()
{
    updateHelpContext();
    termCtrl.lockState([&] (auto &state) {
        // Old scrollback lines were dropped: absolute rows moved up.
        if (state.scrollbackDropped != sbDropped)
        {
            shiftAnchors((int) min<uint64_t>(state.scrollbackDropped - sbDropped, INT_MAX / 2));
            sbDropped = state.scrollbackDropped;
        }
        sbOffset = state.scrollbackOffset;
        sbLimit = state.scrollbackLimit;
        sbEnabled = state.scrollbackEnabled;
        updateCursor(state);
        updateScrollBar(state);
        updateDisplay(state);

        TerminalUpdatedMsg upd {*this, state};
        message(owner, evCommand, consts.cmTerminalUpdated, &upd);
    });
}

void TerminalView::updateHelpContext() noexcept
{
    // Only show the corresponding help context when something is actually
    // selected or if the help context was forcefully set.
    if (historyMode)
        helpCtx = consts.hcHistory;
    else if (inSelectionMode && (!selection.empty() || helpCtx == consts.hcSelecting))
        helpCtx = consts.hcSelecting;
    else
        helpCtx = hcNoContext;
}

void TerminalView::updateCursor(TerminalState &state) noexcept
{
    if (state.cursorChanged)
    {
        state.cursorChanged = false;
        setState(sfCursorVis, state.cursorVisible);
        setState(sfCursorIns, state.cursorBlink);

        TPoint cursorPos = getCursorDisplayedPos(state);
        setCursor(cursorPos.x, cursorPos.y);
    }
}

void TerminalView::updateScrollBar(TerminalState &state) noexcept
{
    if (state.scrollbackChanged && scrollBar)
    {
        state.scrollbackChanged = false;

        if (state.scrollbackEnabled)
        {
            scrollBar->setParams(state.scrollbackOffset, 0, state.scrollbackLimit, size.y, 1);
            scrollBar->show();

            TPoint cursorPos = getCursorDisplayedPos(state);
            setCursor(cursorPos.x, cursorPos.y);
        }
        else
            scrollBar->hide();
    }
}

static RowRange rangeToCopy(int y, int cols, const TerminalSurface &surface, bool reuseBuffer)
{
    auto &damage = surface.damageAtRow(y);
    if (reuseBuffer)
        return {
            max(damage.start, 0),
            min(damage.end, cols),
        };
    else
        return {0, cols};
}

static void drawSelection(TScreenCell *lineBuffer, RowRange range) noexcept
{
    for (int x = range.start; x < range.end; ++x)
        lineBuffer[x].attribute = lineBuffer[x].attribute.reversed();
}

static void drawMatch(TScreenCell *lineBuffer, RowRange range) noexcept
// Search match: black on yellow.
{
    for (int x = range.start; x < range.end; ++x)
    {
        auto &attr = lineBuffer[x].attribute;
        attr = TColorAttr(TColorBIOS(0x0), TColorBIOS(0xE), attr.getStyle() & ~slReverse);
    }
}

void TerminalView::updateDisplay(TerminalState &state) noexcept
{
    auto &surface = state.surface;
    bool reuseBuffer = canReuseOwnerBuffer();

    TPoint drawableAreaSize = {
        min(size.x, surface.size.x),
        min(size.y, surface.size.y),
    };

    TScreenCell *lineBuffer = new TScreenCell[drawableAreaSize.x];
    for (int y = 0; y < drawableAreaSize.y; ++y)
    {
        RowRange range = rangeToCopy(y, drawableAreaSize.x, surface, reuseBuffer);

        // 0 <= absY < (scrollbackOffset + drawableAreaSize.y)
        int absY = y + state.scrollbackOffset;
        RowRange selectionInRow = selection.intersectRow(absY, drawableAreaSize.x);
        RowRange prevSelectionInRow = prevSelection.intersectRow(absY, drawableAreaSize.x);
        RowRange matchInRow = match.intersectRow(absY, drawableAreaSize.x);
        RowRange prevMatchInRow = prevMatch.intersectRow(absY, drawableAreaSize.x);

        // Expand range if selection changed and this row is affected.
        if (selectionInRow != prevSelectionInRow)
        {
            range.start = min(min(range.start, selectionInRow.start), prevSelectionInRow.start);
            range.end = max(max(range.end, selectionInRow.end), prevSelectionInRow.end);
        }
        if (matchInRow != prevMatchInRow)
        {
            range.start = min(min(range.start, matchInRow.start), prevMatchInRow.start);
            range.end = max(max(range.end, matchInRow.end), prevMatchInRow.end);
        }

        if (range.size() > 0)
        {
            memcpy(&lineBuffer[range.start], &surface.at(y, range.start), sizeof(TScreenCell) * range.size());
            drawMatch(lineBuffer, range.intersect(matchInRow));
            drawSelection(lineBuffer, range.intersect(selectionInRow));
            writeLine(range.start, y, range.size(), 1, &lineBuffer[range.start]);
        }
    }

    surface.clearDamage();
    delete[] lineBuffer;

    prevSelection = selection;
    prevMatch = match;
}

bool TerminalView::canReuseOwnerBuffer() noexcept
{
    if (ownerBufferChanged)
    {
        ownerBufferChanged = false;
        return false;
    }
    return owner && owner->buffer;
}

TPoint TerminalView::getCursorDisplayedPos(TerminalState &state) noexcept
{
    TPoint cursorPos = state.cursorPos;
    if (state.scrollbackEnabled)
        cursorPos.y += state.scrollbackLimit - state.scrollbackOffset;
    return cursorPos;
}

void TerminalView::startSelection(SelectionAnchor pos) noexcept
{
    selectUnit = unitChar;
    selection.start = pos;
    selection.end = pos;
    inSelectionMode = true;
}

void TerminalView::startUnitSelection(SelectionAnchor pos, SelectUnit unit) noexcept
{
    SelectionAnchor start, end;
    if (!getUnitAt(pos, unit, start, end))
    {
        startSelection(pos);
        return;
    }
    selectUnit = unit;
    unitStart = start;
    unitEnd = end;
    selection = {start, end};
    inSelectionMode = true;
    drawView();
}

void TerminalView::extendSelection(SelectionAnchor pos) noexcept
{
    if (selectUnit != unitChar)
    {
        // Extend by whole words or lines, keeping the unit clicked first.
        SelectionAnchor start, end;
        if (!getUnitAt(pos, selectUnit, start, end))
            start = end = pos;
        selection.start = min(start, unitStart);
        selection.end = max(end, unitEnd);
    }
    else
        selection.end = pos;
    drawView();
}

void TerminalView::cancelSelection() noexcept
{
    selection = {};
    inSelectionMode = false;
    selectUnit = unitChar;
    drawView();
}

void TerminalView::copySelection() noexcept
{
    if (!selection.empty())
    {
        TerminalEvent termEvent;
        termEvent.type = TerminalEventType::CopySelection;
        termEvent.copySelection = {selection};
        termCtrl.sendEvent(termEvent);
    }
    cancelSelection();
}

// History mode.

void TerminalView::showMessage(const std::string &text) noexcept
{
    if (terminalOptions.showMessage)
        terminalOptions.showMessage(text);
}

void TerminalView::updateWindowTitle() noexcept
// The window title shows whether history mode is active.
{
    if (auto *w = dynamic_cast<TWindow *>(owner))
        if (w->frame)
            w->frame->drawView();
}

bool TerminalView::enterHistoryMode(bool autoExit) noexcept
{
    if (!sbEnabled)
        return false;
    if (!historyMode)
    {
        historyMode = true;
        historyAutoExit = autoExit;
        updateWindowTitle();
        drawView();
    }
    else if (!autoExit)
        historyAutoExit = false;
    return true;
}

void TerminalView::exitHistoryMode() noexcept
{
    if (!historyMode)
        return;
    historyMode = false;
    historyAutoExit = false;
    match = {};
    TerminalEvent termEvent;
    termEvent.type = TerminalEventType::ScrollBackScroll;
    termEvent.scrollBackScroll = {ScrollBackScrollEvent::scrollToBottom};
    termCtrl.sendEvent(termEvent);
    sbOffset = sbLimit;
    updateWindowTitle();
    drawView();
}

void TerminalView::scrollHistory(int delta) noexcept
{
    TerminalEvent termEvent;
    termEvent.type = TerminalEventType::ScrollBackScroll;
    termEvent.scrollBackScroll = {delta};
    termCtrl.sendEvent(termEvent);

    // Predict the new offset, so that several keys pressed quickly add up.
    if (delta <= ScrollBackScrollEvent::scrollToTop)
        sbOffset = 0;
    else if (delta >= ScrollBackScrollEvent::scrollToBottom)
        sbOffset = sbLimit;
    else
        sbOffset = min(max(sbOffset + delta, 0), sbLimit);

    // Entered with a direct key: leave when back at the bottom.
    if (historyAutoExit && delta > 0 && sbOffset >= sbLimit)
        exitHistoryMode();
}

bool TerminalView::handleHistoryHotkey(TEvent &ev) noexcept
// The direct keys (by default Alt-Shift-PgUp/PgDn/Home/End) enter history
// mode and scroll at once. They are left to the application when it has
// no scrollback (full-screen applications) or in 'Grab Input' mode.
{
    if (owner && owner->helpCtx == consts.hcInputGrabbed)
        return false;
    if (ev.keyDown.controlKeyState & kbPaste)
        return false;
    TKey key(ev.keyDown.keyCode, ev.keyDown.controlKeyState);
    auto &o = terminalOptions;
    int page = max(1, size.y - 1);
    int delta;
    if (o.keyPageUp.code && key == o.keyPageUp)
        delta = -page;
    else if (o.keyPageDown.code && key == o.keyPageDown)
        delta = page;
    else if (o.keyTop.code && key == o.keyTop)
        delta = ScrollBackScrollEvent::scrollToTop;
    else if (o.keyBottom.code && key == o.keyBottom)
        delta = ScrollBackScrollEvent::scrollToBottom;
    else
        return false;
    if (!enterHistoryMode(true))
        return false;
    scrollHistory(delta);
    return true;
}

bool TerminalView::handleHistoryKey(TEvent &ev) noexcept
// Keys in history mode. Returns false for keys that leave the mode and
// go to the application.
{
    if (handleHistoryHotkey(ev))
        return true;

    TKey key(ev.keyDown.keyCode, ev.keyDown.controlKeyState);
    char ch = 0;
    if ( ev.keyDown.textLength == 1 &&
         !(ev.keyDown.controlKeyState & (kbCtrlShift | kbAltShift)) )
        ch = ev.keyDown.text[0];
    int page = max(1, size.y - 1);

    if (key == TKey(kbUp))
        scrollHistory(-1);
    else if (key == TKey(kbDown))
        scrollHistory(1);
    else if (key == TKey(kbPgUp))
        scrollHistory(-page);
    else if (key == TKey(kbPgDn))
        scrollHistory(page);
    else if (key == TKey(kbHome) || key == TKey(kbCtrlHome) || key == TKey(kbCtrlPgUp))
        scrollHistory(ScrollBackScrollEvent::scrollToTop);
    else if (key == TKey(kbEnd) || key == TKey(kbCtrlEnd) || key == TKey(kbCtrlPgDn))
        scrollHistory(ScrollBackScrollEvent::scrollToBottom);
    else if (ch == '/' || key == TKey(kbF7))
    {
        if (terminalOptions.openSearch)
            terminalOptions.openSearch(*this);
    }
    else if (ch == 'n' || key == TKey(kbF3))
        findNext(true);
    else if (ch == 'N' || key == TKey(kbF3, kbShift))
        findNext(false);
    else if (ch == 's' || key == TKey(kbF2))
    {
        if (terminalOptions.openSave)
            terminalOptions.openSave(*this);
    }
    else if (key == TKey(kbCtrlC) || key == TKey(kbCtrlIns))
        copyMatchOrSelection();
    else if (key == TKey(kbEsc))
    {
        // First the selection, then the search match, then the mode.
        if (inSelectionMode || !selection.empty())
            cancelSelection();
        else if (!match.empty())
        {
            match = {};
            drawView();
        }
        else
            exitHistoryMode();
    }
    else
        return false;
    return true;
}

void TerminalView::copyMatchOrSelection() noexcept
{
    SelectionRange range = !selection.empty() ? selection : match;
    if (range.empty())
    {
        showMessage("Nothing to copy: select text or search first.");
        return;
    }
    TerminalEvent termEvent;
    termEvent.type = TerminalEventType::CopySelection;
    termEvent.copySelection = {range};
    termCtrl.sendEvent(termEvent);
    if (!selection.empty())
        cancelSelection();
    showMessage("Copied to the clipboard.");
}

void TerminalView::shiftAnchors(int delta) noexcept
// Called when 'delta' old scrollback lines were dropped.
{
    auto shift = [&] (SelectionRange &r, bool clearIfCut) {
        if (r.empty())
            return;
        r.start.y -= delta;
        r.end.y -= delta;
        if (r.start.y < 0 || r.end.y < 0)
        {
            if (clearIfCut || (r.start.y < 0 && r.end.y < 0))
                r = {};
            else
            {
                if (r.start.y < 0) r.start = {0, 0};
                if (r.end.y < 0) r.end = {0, 0};
            }
        }
    };
    shift(selection, false);
    shift(prevSelection, true);
    shift(match, true);
    shift(prevMatch, true);
    unitStart.y -= delta;
    unitEnd.y -= delta;
}

bool TerminalView::getUnitAt( SelectionAnchor pos, SelectUnit unit,
                              SelectionAnchor &start, SelectionAnchor &end ) noexcept
// Bounds of the word or the logical line at 'pos'.
{
    TextSnapshot snap;
    termCtrl.withEmulator([&] (TerminalEmulator &emu) {
        emu.getText(snap, pos.y, pos.y + 1, true);
    });
    for (auto &line : snap.lines)
    {
        if (pos.y < line.firstRow || pos.y > line.lastRow)
            continue;
        if (unit == unitLine)
        {
            // Up to the end of the last row, so that the line break is
            // copied too.
            start = {0, line.firstRow};
            end = {INT_MAX / 2, line.lastRow};
            return true;
        }
        // Word: the cell under the mouse and its neighbours of the same kind.
        int n = (int) line.cells.size();
        std::vector<char32_t> cellChar(n, U' ');
        for (int i = (int) line.text.size() - 1; i >= 0; --i)
            cellChar[line.charCell[i]] = line.text[i];
        int j = -1;
        for (int i = 0; i < n; ++i)
            if ( line.cells[i].y == pos.y && line.cells[i].x <= pos.x &&
                 pos.x < line.cells[i].x + line.cellWidth[i] )
            {
                j = i;
                break;
            }
        if (j < 0)
            return false;
        int a = j, b = j;
        if (termsearch::isWordChar(cellChar[j]) && cellChar[j] != U' ')
        {
            while (a > 0 && termsearch::isWordChar(cellChar[a - 1]) && cellChar[a - 1] != U' ')
                --a;
            while (b + 1 < n && termsearch::isWordChar(cellChar[b + 1]) && cellChar[b + 1] != U' ')
                ++b;
        }
        start = line.cells[a];
        end = {line.cells[b].x + line.cellWidth[b], line.cells[b].y};
        return true;
    }
    return false;
}

bool TerminalView::setSearch( const std::u32string &text, bool regex, bool caseSensitive,
                              std::string &error ) noexcept
{
    std::unique_ptr<TerminalSearch> s(new TerminalSearch);
    s->text = text;
    s->regex = regex;
    s->caseSensitive = caseSensitive;
    s->foldedText = termsearch::fold(text);
    if (regex)
    {
        try
        {
            auto flags = std::regex_constants::ECMAScript;
            if (!caseSensitive)
                flags |= std::regex_constants::icase;
            s->re.assign(std::wstring(text.begin(), text.end()), flags);
        }
        catch (const std::regex_error &e)
        {
            error = e.what();
            return false;
        }
    }
    search = std::move(s);
    match = {};
    return true;
}

bool TerminalView::getSearch(std::u32string &text, bool &regex, bool &caseSensitive) const noexcept
{
    if (!search)
        return false;
    text = search->text;
    regex = search->regex;
    caseSensitive = search->caseSensitive;
    return true;
}

bool TerminalView::findNext(bool older, bool fromView) noexcept
{
    if (!search || search->text.empty())
    {
        if (terminalOptions.openSearch)
            terminalOptions.openSearch(*this);
        return false;
    }
    TextSnapshot snap;
    termCtrl.withEmulator([&] (TerminalEmulator &emu) {
        emu.getText(snap, 0, INT_MAX, true);
    });

    // Where to start: the last match, or the visible area.
    SelectionAnchor ref;
    bool fromMatch = !fromView && !match.empty();
    if (fromMatch)
        ref = min(match.start, match.end);
    else if (older)
        ref = {0, sbOffset + size.y}; // Below the last visible row.
    else
        ref = {-1, sbOffset};        // Before the first visible row.

    std::vector<std::pair<int, int>> found;
    bool ok = false;
    SelectionAnchor mStart, mEnd;
    int count = (int) snap.lines.size();
    for (int k = 0; k < count && !ok; ++k)
    {
        auto &line = snap.lines[older ? count - 1 - k : k];
        if (line.text.empty())
            continue;
        if (older ? line.firstRow > ref.y : line.lastRow < ref.y)
            continue;
        termsearch::findMatches(*search, line.text, found);
        int nf = (int) found.size();
        for (int i = 0; i < nf; ++i)
        {
            auto &f = found[older ? nf - 1 - i : i];
            SelectionAnchor a = line.cells[line.charCell[f.first]];
            if (older ? (a < ref) : (a > ref))
            {
                int lastCell = line.charCell[f.second - 1];
                mStart = a;
                mEnd = {line.cells[lastCell].x + line.cellWidth[lastCell], line.cells[lastCell].y};
                ok = true;
                break;
            }
        }
    }

    if (!ok)
    {
        std::string text;
        termsearch::appendUtf8(text, search->text);
        showMessage( std::string(older ? "No older match for \"" : "No newer match for \"") +
                     text + "\"." );
        return false;
    }

    match = {mStart, mEnd};
    // Scroll so that the match is visible (about a third from the top).
    if (mStart.y < sbOffset || mStart.y >= sbOffset + size.y)
    {
        int newOffset = min(max(mStart.y - size.y/3, 0), snap.scrollbackLines);
        TerminalEvent termEvent;
        termEvent.type = TerminalEventType::ScrollBackOffsetChange;
        termEvent.scrollBackOffsetChange = {newOffset};
        termCtrl.sendEvent(termEvent);
        sbOffset = newOffset;
        // Searching from history mode entered with a direct key: stay in it.
        historyAutoExit = false;
    }
    drawView();
    return true;
}

void TerminalView::getHistoryText(std::string &text, bool selectionOnly, bool &screenOnly) noexcept
{
    text.clear();
    screenOnly = false;
    if (selectionOnly)
    {
        SelectionRange range = !selection.empty() ? selection : match;
        termCtrl.withEmulator([&] (TerminalEmulator &emu) {
            emu.getSelectionText(range, text);
        });
        if (!text.empty() && text.back() != '\n')
            text += '\n';
        return;
    }
    TextSnapshot snap;
    termCtrl.withEmulator([&] (TerminalEmulator &emu) {
        emu.getText(snap, 0, INT_MAX, false);
    });
    screenOnly = snap.altScreen;
    // Leave out the empty lines at the end of the screen (and at the
    // beginning, which happens with full-screen applications).
    size_t count = snap.lines.size();
    while (count > 0 && snap.lines[count - 1].text.empty())
        --count;
    size_t first = 0;
    while (first < count && snap.lines[first].text.empty())
        ++first;
    for (size_t i = first; i < count; ++i)
    {
        auto &line = snap.lines[i].text;
        size_t len = line.size();
        while (len > 0 && (line[len - 1] == U' ' || line[len - 1] == U'\t'))
            --len;
        line.resize(len);
        termsearch::appendUtf8(text, line);
        text += '\n';
    }
}

} // namespace tvterm
