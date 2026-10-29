#include "wnd.h"
#include "cmds.h"

#define Uses_TEvent
#define Uses_TFrame
#include <tvision/tv.h>

#include <tvterm/termview.h>

#include <ctype.h>

const tvterm::TVTermConstants TerminalWindow::appConsts =
{
    cmCheckTerminalUpdates,
    cmTerminalUpdated,
    cmCopySelection,
    cmCancelSelection,
    cmGrabInput,
    cmReleaseInput,
    cmStartSelection,
    cmHistoryMode,
    hcInputGrabbed,
    hcSelecting,
    hcHistory,
};

void TerminalWindow::handleEvent(TEvent &ev)
{
    bool checkUpdates = ev.what == evBroadcast && ev.message.command == cmCheckTerminalUpdates;
    if ( ev.what == evBroadcast &&
         ev.message.command == cmGetOpenTerms && !isDisconnected() )
        *(size_t *) ev.message.infoPtr += 1;
    else if ( ev.what == evCommand && ev.message.command == cmWindowList &&
              (state & sfModal) )
    {
        // Input grabbed (status line click on Window List): release the
        // input, then the application opens the list.
        endModal(cmCancel);
        putEvent(ev);
        clearEvent(ev);
        return;
    }
    else if ( ev.what == evKeyDown && isDisconnected() &&
              !(state & (sfDragging | sfModal)) )
    {
        close();
        return;
    }
    Super::handleEvent(ev);
    // The terminal view has just collected the new output (if any).
    if (checkUpdates)
        checkAlerts();
}

void TerminalWindow::setState(ushort aState, Boolean enable)
{
    bool wasSelected = state & sfSelected;
    Super::setState(aState, enable);
    if (aState & sfSelected)
    {
        if (enable)
            clearAlert();
        else if (wasSelected)
        {
            // Applications may redraw when they lose the focus.
            ignoreOutputFor(1500);
            silenceSince = Clock::now();
        }
    }
}

const char *TerminalWindow::getTitle(short maxSize)
{
    const char *title = Super::getTitle(maxSize);
    if (alert == Alert::None)
        return title;
    titleBuf = alertMark((int) alert == (int) Alert::Keyword ? 3 :
                         (int) alert == (int) Alert::Silence ? 2 : 1, alertKeyword);
    if (title)
        titleBuf += title;
    return titleBuf.c_str();
}

void TerminalWindow::setAlertSettings(AlertSettings settings) noexcept
{
    alertSettings = std::move(settings);
    pendingLine.clear();
    // Silence is counted from now, even if nothing is ever written.
    silenceArmed = true;
    silenceSince = Clock::now();
    clearAlert();
}

void TerminalWindow::ignoreOutputFor(int ms) noexcept
{
    auto until = Clock::now() + std::chrono::milliseconds(ms);
    if (until > ignoreOutputUntil)
        ignoreOutputUntil = until;
    pendingLine.clear();
}

void TerminalWindow::checkAlerts() noexcept
{
    auto *view = getTerminalView();
    if (!view)
        return;
    std::string text = std::move(view->newOutputText);
    view->newOutputText.clear();
    bool newOutput = view->outputBytes != lastOutputBytes;
    lastOutputBytes = view->outputBytes;

    auto now = Clock::now();
    if (newOutput)
    {
        silenceArmed = true;
        silenceSince = now;
    }
    if ( !alertSettings.enabled() || isDisconnected() ||
         (state & sfSelected) || now < ignoreOutputUntil )
    {
        pendingLine.clear();
        return;
    }

    if (!text.empty())
    {
        if (!alertSettings.keywords.empty() && alert != Alert::Keyword)
        {
            pendingLine += text;
            size_t start = 0, end;
            while ( alert != Alert::Keyword &&
                    (end = pendingLine.find('\n', start)) != std::string::npos )
            {
                checkKeywordLine(pendingLine.substr(start, end - start));
                start = end + 1;
            }
            if (alert == Alert::Keyword)
                pendingLine.clear();
            else
            {
                pendingLine.erase(0, start);
                // A very long line without an end (e.g. a progress bar).
                if (pendingLine.size() > 16 * 1024)
                {
                    checkKeywordLine(pendingLine);
                    pendingLine.clear();
                }
            }
            pendingSince = now;
            pendingChecked = false;
        }
        if ( alertSettings.activity &&
             text.find_first_not_of(" \n") != std::string::npos )
            raiseAlert(Alert::Activity);
    }
    else if ( !pendingLine.empty() && !pendingChecked && alert != Alert::Keyword &&
              now - pendingSince >= std::chrono::milliseconds(500) )
    {
        // The line has not ended for a while (a prompt...): checked as it
        // is, once. If it goes on later, the whole line is checked again.
        pendingChecked = true;
        checkKeywordLine(pendingLine);
    }

    if ( alertSettings.silenceSeconds > 0 && silenceArmed && !newOutput &&
         now - silenceSince >= std::chrono::seconds(alertSettings.silenceSeconds) )
    {
        silenceArmed = false;
        raiseAlert(Alert::Silence);
    }
}

void TerminalWindow::checkKeywordLine(const std::string &line) noexcept
{
    if (const KeywordGroup *group = alertSettings.keywords.match(lowerAscii(line)))
        raiseAlert(Alert::Keyword, group, line);
}

void TerminalWindow::raiseAlert(Alert kind, const KeywordGroup *group, const std::string &line) noexcept
{
    // A keyword is more important than silence, which is more important
    // than activity.
    if (kind <= alert)
        return;
    if (alert == Alert::None)
        alertOrder = newAlertOrder();
    alert = kind;
    alertTime = time(nullptr);
    alertKeyword = group ? group->shortLabel : std::string();
    alertGroup = group ? group->label : std::string();
    alertLine = group ? alertLineFragment(line, group) : std::string();
    if (frame)
        frame->drawView();
}

void TerminalWindow::clearAlert() noexcept
{
    if (alert != Alert::None)
    {
        alert = Alert::None;
        alertKeyword.clear();
        alertGroup.clear();
        alertLine.clear();
        if (frame)
            frame->drawView();
    }
}

AlertInfo TerminalWindow::getAlertInfo() noexcept
{
    AlertInfo info;
    info.time = alertTime;
    info.mark = alertMark((int) alert == (int) Alert::Keyword ? 3 :
                          (int) alert == (int) Alert::Silence ? 2 : 1, alertKeyword);
    while (!info.mark.empty() && info.mark.back() == ' ')
        info.mark.pop_back();
    const char *title = Super::getTitle(255);
    info.title = title && *title ? title : "(no title)";
    info.group = alertGroup;
    info.line = alertLine;
    return info;
}

void TerminalWindow::sizeLimits(TPoint &min, TPoint &max)
{
    Super::sizeLimits(min, max);
    if (owner)
    {
        max = owner->size;
        max.x += 2;
        max.y += 1;
    }
}

void TerminalWindow::changeBounds(const TRect &bounds)
{
    TRect maxBounds = getMaxBounds();
    if (bounds == maxBounds)
    {
        // A maximized terminal shows just the title bar and cannot be dragged.
        growMode = gfGrowHiX | gfGrowHiY;
        flags &= ~(wfMove | wfGrow);
    }
    else
    {
        growMode = gfGrowAll | gfGrowRel;
        flags |= wfMove | wfGrow;
    }
    // Applications redraw when the terminal is resized.
    TPoint oldSize = size;
    Super::changeBounds(bounds);
    if (size != oldSize)
        ignoreOutputFor(1500);
}

void TerminalWindow::zoom()
{
    TRect bounds = getBounds();
    TRect maxBounds = getMaxBounds();
    if (bounds != maxBounds)
    {
        zoomRect = bounds;
        locate(maxBounds);
    }
    else
        locate(zoomRect);
}

TRect TerminalWindow::getMaxBounds()
{
    TPoint minSize, maxSize;
    sizeLimits(minSize, maxSize);
    TRect r(0, 0, maxSize.x, maxSize.y);
    r.move(-1, 0);
    return r;
}
