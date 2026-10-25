#include <tvterm/termview.h>
#include <tvterm/termctrl.h>
#include <tvterm/consts.h>
#include "util.h"

#define Uses_TKeys
#define Uses_TEvent
#define Uses_TScrollBar
#define Uses_TClipboard
#include <tvision/tv.h>

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

namespace tvterm
{

// Debug log for clipboard requests from client applications (OSC 52).
// Enabled by setting TVTERM_CLIPBOARD_LOG to a file path.
static void logClipboardRequest(const char *text, size_t size) noexcept
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
        fprintf(f, "%s OSC 52 from application: %zu bytes: \"", timeBuf, size);
        for (size_t i = 0; i < size && i < 60; ++i)
        {
            unsigned char c = (unsigned char) text[i];
            if (c < 0x20 || c == 0x7F)
                fputc('.', f);
            else
                fputc(c, f);
        }
        fprintf(f, "%s\" -> TClipboard::setText\n", size > 60 ? "..." : "");
        fclose(f);
    }
}

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
            logClipboardRequest(state.clipboardText.data(), state.clipboardText.size());
            TClipboard::setText({state.clipboardText.data(), state.clipboardText.size()});
            state.clipboardText.clear();
        }
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
