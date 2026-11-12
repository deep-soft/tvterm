#define Uses_TView
#define Uses_TWindow
#define Uses_TDeskTop
#define Uses_TDrawBuffer
#define Uses_TEvent
#define Uses_TPalette
#define Uses_TProgram
#define Uses_TStatusLine
#include <tvision/tv.h>

#include "topbar.h"
#include "alerts.h"
#include "cmds.h"
#include "desktops.h"
#include "globalkeys.h"
#include "statline.h"
#include "winlist.h"

#include <time.h>

namespace topbar
{

// Colors of the desktop buttons (BIOS attributes: background, foreground).
const uchar currentColor = 0x20; // Black on green.
const uchar otherColor = 0x30;   // Black on cyan.
const uchar alertColor = 0x4F;   // White on red.
const uchar flashColor = 0x74;   // Red on white: the other phase of A1.

static std::string menuText()
{
    return globalkeys::menuKeyName() + " Menu";
}

// "Alerts: 1+7": level 1 and the other levels (E68).
static std::string alertsText(size_t level1, size_t others)
{
    return "Alerts: " + std::to_string(level1) + "+" + std::to_string(others) +
           " (" + globalkeys::menuKeyName() + " G)";
}

// 'text' cut to 'width' columns ('\u2026' at the end if cut).
static std::string fitText(const std::string &text, int width)
{
    if (width <= 0)
        return {};
    if ((int) strwidth(text) <= width)
        return text;
    std::string out = text;
    while (!out.empty() && (int) strwidth(out) > width - 1)
    {
        // One whole UTF-8 character: its continuation bytes, then its
        // first byte.
        while (!out.empty() && ((unsigned char) out.back() & 0xC0) == 0x80)
            out.pop_back();
        if (!out.empty())
            out.pop_back();
    }
    return out + "\u2026";
}

// " D2 ", or " 2:mail " with the names of the desktops.
static std::string desktopText(int n, bool withNames)
{
    return " " + (withNames ? desktops::label(n) : "D" + std::to_string(n)) + " ";
}

static int textWidth(const std::string &text)
{
    return (int) strwidth(text);
}

// The global position of the mouse, last seen.
static TPoint lastMouse {-1, -1};

} // namespace topbar

TopBar *TopBar::instance = nullptr;

TopBar::TopBar(const TRect &bounds) noexcept :
    TView(bounds)
{
    growMode = gfGrowHiX;
    instance = this;
}

TopBar::~TopBar()
{
    if (instance == this)
        instance = nullptr;
}

TPalette &TopBar::getPalette() const
{
    // The colors of the status line (cpStatusLine in tstatusl.cpp).
    static const char colors[] = "\x02\x03\x04\x05\x06\x07";
    static TPalette palette(colors, sizeof(colors) - 1);
    return palette;
}

std::vector<TopBar::Item> TopBar::layout() const
{
    using namespace topbar;
    std::vector<Item> items;
    // The desktop buttons, on the right: those with windows and the
    // current one.
    std::vector<int> shown;
    for (int n = 1; n <= desktops::count; ++n)
        if (state_.counts[n] > 0 || n == state_.current)
            shown.push_back(n);
    // With the names, if they leave room for the menu and WinList items;
    // otherwise every button is short (" D2 ").
    auto widthOf = [&] (bool withNames) {
        int width = 0;
        for (int n : shown)
            width += (width ? 1 : 0) + textWidth(desktopText(n, withNames));
        return width;
    };
    bool withNames = false;
    for (int n : shown)
        withNames |= !desktops::name(n).empty();
    if (withNames && size.x - 1 - widthOf(true) < textWidth(menuText()) + 11)
        withNames = false;
    int buttonsWidth = widthOf(withNames);
    // Not enough room: the first buttons are dropped.
    while (!shown.empty() && buttonsWidth > size.x - 1)
    {
        buttonsWidth -= textWidth(desktopText(shown.front(), withNames)) + (shown.size() > 1 ? 1 : 0);
        shown.erase(shown.begin());
    }
    int right = size.x - 1 - buttonsWidth; // Where the buttons begin.
    // The items on the left, while they fit before the buttons.
    int x = 1;
    auto add = [&] (ItemKind kind, const std::string &text) {
        int width = (int) strwidth(text);
        if (x + width + 1 > right)
            return false;
        items.push_back({kind, 0, x, width, false});
        x += width + 2;
        return true;
    };
    // On a narrow screen they are dropped from the end: Alerts, DeskList,
    // WinList, the menu.
    if ( add(itMenu, menuText()) && add(itWindowList, "WinList") &&
         add(itDesktopList, "DeskList") && state_.alerts > 0 &&
         add(itAlerts, alertsText(state_.level1, state_.others)) && !state_.alert1.empty() )
    {
        // The A1 item: as much of its text as fits (at least 12 columns).
        int room = right - x - 1;
        if (room >= 12)
            add(itAlert1, fitText(state_.alert1, room));
    }
    x = right;
    for (int n : shown)
    {
        int width = textWidth(desktopText(n, withNames));
        items.push_back({itDesktop, n, x, width, withNames});
        x += width + 1;
    }
    return items;
}

void TopBar::draw()
{
    using namespace topbar;
    TDrawBuffer b;
    TColorAttr normal = mapColor(1);
    TColorAttr shortcut = mapColor(3);
    b.moveChar(0, ' ', normal, size.x);
    for (const Item &item : layout())
        switch (item.kind)
        {
            case itMenu:
                // The key highlighted, like the items of the status line.
                b.moveStr(item.x, globalkeys::menuKeyName(), shortcut);
                b.moveStr(item.x + (int) strwidth(globalkeys::menuKeyName()), " Menu", normal);
                break;
            case itWindowList:
                b.moveStr(item.x, "WinList", normal);
                break;
            case itDesktopList:
                b.moveStr(item.x, "DeskList", normal);
                break;
            case itAlerts:
                b.moveStr(item.x, alertsText(state_.level1, state_.others), TColorAttr(alertColor));
                break;
            case itAlert1:
                b.moveStr(item.x, fitText(state_.alert1, item.width),
                          TColorAttr(state_.flashOn ? flashColor : alertColor));
                break;
            case itDesktop:
            {
                uchar color = item.desktop == state_.current ? currentColor :
                              state_.alerted[item.desktop] ? alertColor : otherColor;
                b.moveStr(item.x, desktopText(item.desktop, item.named), TColorAttr(color));
                break;
            }
        }
    writeLine(0, 0, size.x, 1, b);
}

int TopBar::itemAt(TPoint local) const
{
    if (local.y != 0)
        return -1;
    std::vector<Item> items = layout();
    for (size_t i = 0; i < items.size(); ++i)
        if (local.x >= items[i].x && local.x < items[i].x + items[i].width)
            return (int) i;
    return -1;
}

void TopBar::run(const Item &item)
{
    TEvent event {};
    event.what = evCommand;
    switch (item.kind)
    {
        case itMenu: event.message.command = cmMenu; break;
        case itWindowList: event.message.command = cmWindowList; break;
        case itDesktopList: event.message.command = cmDesktopMenu; break;
        case itAlerts:
        case itAlert1: event.message.command = cmNextAlert; break;
        case itDesktop:
            if (item.desktop == state_.current)
                return;
            event.message.command = cmSwitchDesktop;
            event.message.infoInt = (short) item.desktop;
            break;
    }
    TProgram::application->putEvent(event);
}

void TopBar::handleEvent(TEvent &event)
{
    TView::handleEvent(event);
    if (event.what == evMouseDown)
    {
        // Like the status line: the item runs if the button is released
        // over it.
        int index = itemAt(makeLocal(event.mouse.where));
        std::vector<Item> items = layout();
        while (mouseEvent(event, evMouseMove | evMouseAuto))
            ;
        if (index >= 0 && itemAt(makeLocal(event.mouse.where)) == index)
            run(items[index]);
        clearEvent(event);
    }
}

std::string TopBar::hintFor(const Item &item) const
{
    using namespace topbar;
    const std::string &key = globalkeys::menuKeyName();
    switch (item.kind)
    {
        case itMenu:
            return "The menu (" + key + ").";
        case itWindowList:
            return "Window List: the windows of every desktop (" + key + " W).";
        case itDesktopList:
            return "Desktop list: the 9 desktops, a digit or Enter shows one (" + key + " D).";
        case itAlert1:
            return "Level 1 alert (" + key + " G goes to it): " + state_.alert1;
        case itAlerts:
        case itDesktop:
            break;
    }
    // The windows (of a desktop, or with an alert).
    std::vector<std::string> titles;
    size_t withAlert = 0;
    if (desk)
        for (TWindow *w : getDeskTopWindows(*desk))
        {
            if (desktops::isGlobal(w) || !((w->state & sfVisible) || desktops::isHidden(w)))
                continue;
            int n = desktops::of(w);
            auto *target = dynamic_cast<AlertTarget *>(w);
            bool alert = target && target->hasAlert();
            if (item.kind == itAlerts ? !alert : n != item.desktop)
                continue;
            const char *t = w->getTitle(255);
            std::string title = t && *t ? t : "(no title)";
            if (item.kind == itAlerts)
                title += " (" + desktops::label(n) + ")";
            else if (alert)
                ++withAlert; // The title has the mark of the alert ([!]).
            titles.push_back(title);
        }
    std::string text;
    if (item.kind == itAlerts)
        text = "Go to the most urgent alert (" + key + " G). Level 1: " + std::to_string(state_.level1) +
               ", other levels: " + std::to_string(state_.others) + ". Alerts: ";
    else
    {
        text = desktops::label(item.desktop);
        if (item.desktop == state_.current)
            text += " (current)";
        if (titles.empty())
            return text + ": no windows.";
        text += ": " + std::to_string(titles.size()) +
                (titles.size() == 1 ? " window" : " windows");
        if (withAlert)
            text += ", " + std::to_string(withAlert) + " with an alert";
        text += ": ";
    }
    for (size_t i = 0; i < titles.size(); ++i)
        text += (i ? ", " : "") + titles[i];
    return text;
}

void TopBar::showHint(int index)
{
    std::string text;
    if (index >= 0)
    {
        std::vector<Item> items = layout();
        if (index < (int) items.size())
            text = hintFor(items[index]);
    }
    hovered = text.empty() ? -1 : index;
    TVTermStatusLine::setHint(text);
}

void TopBar::hover(TPoint where)
{
    topbar::lastMouse = where;
    showHint((state & sfVisible) ? itemAt(makeLocal(where)) : -1);
}

void TopBar::update(TDeskTop &deskTop)
{
    desk = &deskTop;
    State s;
    s.current = desktops::current();
    s.names = desktops::namesVersion();
    s.alerts = countAlerts(deskTop);
    countAlerts(deskTop, s.level1, s.others);
    if (s.level1)
    {
        s.alert1 = level1AlertText(deskTop);
        // Flashing: the other colors every other second.
        s.flashOn = alertConfig.flash && (time(nullptr) & 1);
    }
    auto counts = desktops::windowCounts(deskTop);
    for (int n = 1; n <= desktops::count; ++n)
        s.counts[n] = counts[n];
    for (TWindow *w : getDeskTopWindows(deskTop))
        if (auto *target = dynamic_cast<AlertTarget *>(w))
            if ( target->hasAlert() && !desktops::isGlobal(w) &&
                 ((w->state & sfVisible) || desktops::isHidden(w)) )
                s.alerted[desktops::of(w)] = true;
    if (!(s == state_))
    {
        state_ = s;
        drawView();
    }
    // The windows may have changed under the mouse (titles, alerts).
    if (hovered >= 0)
        hover(topbar::lastMouse);
}

void TopBar::menuKeyChanged()
{
    drawView();
    if (hovered >= 0)
        hover(topbar::lastMouse);
}
