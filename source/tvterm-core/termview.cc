#include <tvterm/termview.h>
#include <tvterm/termctrl.h>
#include <tvterm/consts.h>
#include "util.h"

#define Uses_TKeys
#define Uses_TEvent
#define Uses_TScrollBar
#define Uses_TClipboard
#define Uses_THardwareInfo
#include <tvision/tv.h>

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <chrono>
#include <string>
#include <vector>

namespace tvterm
{

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
            if (ev.message.command == consts.cmStartSelection)
            {
                startSelection();
                helpCtx = consts.hcSelecting;
                clearEvent(ev);
            }
            else if (ev.message.command == consts.cmCopySelection)
            {
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
        case evMouseMove:
        case evMouseAuto:
        case evMouseUp:
            handleMouse(ev.what, ev.mouse);
            clearEvent(ev);
            break;
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

    // Left click: start selection.
    if ( (inSelectionMode || (!inSelectionMode && !clientIsUsingMouse)) &&
         what == evMouseDown && (mouse.buttons & mbLeftButton) )
        startSelection(absPos);
    // Mouse drag: extend selection.
    else if (inSelectionMode && what == evMouseMove && (mouse.buttons & mbLeftButton))
        extendSelection(absPos);
    // Empty selection: stop selecting.
    else if (inSelectionMode && what == evMouseUp && selection.empty())
        cancelSelection();
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
    if (inSelectionMode && (!selection.empty() || helpCtx == consts.hcSelecting))
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

        // Expand range if selection changed and this row is affected.
        if (selectionInRow != prevSelectionInRow)
        {
            range.start = min(min(range.start, selectionInRow.start), prevSelectionInRow.start);
            range.end = max(max(range.end, selectionInRow.end), prevSelectionInRow.end);
        }

        if (range.size() > 0)
        {
            memcpy(&lineBuffer[range.start], &surface.at(y, range.start), sizeof(TScreenCell) * range.size());
            drawSelection(lineBuffer, range.intersect(selectionInRow));
            writeLine(range.start, y, range.size(), 1, &lineBuffer[range.start]);
        }
    }

    surface.clearDamage();
    delete[] lineBuffer;

    prevSelection = selection;
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
    selection.start = pos;
    selection.end = pos;
    inSelectionMode = true;
}

void TerminalView::extendSelection(SelectionAnchor pos) noexcept
{
    selection.end = pos;
    drawView();
}

void TerminalView::cancelSelection() noexcept
{
    selection = {};
    inSelectionMode = false;
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

} // namespace tvterm
