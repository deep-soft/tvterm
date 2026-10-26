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
    switch (alert)
    {
        case Alert::Activity: titleBuf = "[!] "; break;
        case Alert::Silence: titleBuf = "[!silent] "; break;
        case Alert::Keyword: titleBuf = "[!" + alertKeyword + "] "; break;
        default: titleBuf.clear(); break;
    }
    if (title)
        titleBuf += title;
    return titleBuf.c_str();
}

void TerminalWindow::setAlertSettings(AlertSettings settings) noexcept
{
    alertSettings = std::move(settings);
    keywordTail.clear();
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
    keywordTail.clear();
}

static std::string toLowerAscii(std::string s)
{
    for (char &c : s)
        c = (char) tolower((unsigned char) c);
    return s;
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
        keywordTail.clear();
        return;
    }

    if (!text.empty())
    {
        if (!alertSettings.keywords.empty())
        {
            std::string haystack = keywordTail + toLowerAscii(std::move(text));
            size_t maxLen = 0;
            for (const auto &keyword : alertSettings.keywords)
            {
                maxLen = std::max(maxLen, keyword.size());
                if (alert != Alert::Keyword && haystack.find(toLowerAscii(keyword)) != std::string::npos)
                    raiseAlert(Alert::Keyword, keyword);
            }
            size_t keep = std::min(haystack.size(), maxLen > 0 ? maxLen - 1 : 0);
            keywordTail = haystack.substr(haystack.size() - keep);
            text = std::move(haystack);
        }
        if ( alertSettings.activity &&
             text.find_first_not_of(" \n") != std::string::npos )
            raiseAlert(Alert::Activity);
    }

    if ( alertSettings.silenceSeconds > 0 && silenceArmed && !newOutput &&
         now - silenceSince >= std::chrono::seconds(alertSettings.silenceSeconds) )
    {
        silenceArmed = false;
        raiseAlert(Alert::Silence);
    }
}

void TerminalWindow::raiseAlert(Alert kind, const std::string &keyword) noexcept
{
    // A keyword is more important than silence, which is more important
    // than activity.
    if (kind <= alert)
        return;
    if (alert == Alert::None)
        alertOrder = newAlertOrder();
    alert = kind;
    alertKeyword = keyword;
    if (frame)
        frame->drawView();
}

void TerminalWindow::clearAlert() noexcept
{
    if (alert != Alert::None)
    {
        alert = Alert::None;
        alertKeyword.clear();
        if (frame)
            frame->drawView();
    }
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
