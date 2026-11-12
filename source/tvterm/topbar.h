#ifndef TVTERM_TOPBAR_H
#define TVTERM_TOPBAR_H

#define Uses_TView
#define Uses_TDeskTop
#define Uses_TEvent
#define Uses_TPalette
#include <tvision/tv.h>

#include <array>
#include <string>
#include <vector>

// The bar on the first line of the screen (the status line is on the last
// one). It is not a menu bar: Alt+letters and F10 stay with the programs in
// the terminals.
//
//   Ctrl-B Menu  WinList  DeskList  Alerts: 2 (Ctrl-B G)     D1  2:mail  D5
//
// A click on an item runs it (the menu, the Window List, the Desktop...
// list, the oldest alert, that desktop). The desktop buttons are those with windows, plus the
// current one (the Event Viewer is on every desktop and is not counted):
// a named desktop shows its name (" 2:mail "), unless the buttons with the
// names would leave no room for the menu and WinList items (then all are
// short); the current one is black on green, the others black on cyan, or white on
// red when one of their windows has an alert. While the mouse is over an
// item, the status line shows what it does (for a desktop: its windows).
class TopBar : public TView
{
public:

    explicit TopBar(const TRect &bounds) noexcept;
    ~TopBar();

    // The bar of the application (null before it is created).
    static TopBar *instance;

    void draw() override;
    void handleEvent(TEvent &event) override;
    TPalette &getPalette() const override;

    // Reads the desktops and the alerts; redraws if something changed.
    // Called periodically.
    void update(TDeskTop &desk);
    // The mouse is at 'where' (global coordinates): the hint in the status
    // line, or none when it is not over an item.
    void hover(TPoint where);
    // The menu key changed (the bar shows its name).
    void menuKeyChanged();

    // The text of the bar and where each item is, for a given width
    // (also used by the tests through the screen).
    // itAlert1 (E68): "A1 <window>: <keyword>" of the oldest level 1 alert.
    enum ItemKind { itMenu, itWindowList, itDesktopList, itAlerts, itAlert1, itDesktop };
    struct Item
    {
        ItemKind kind;
        int desktop; // itDesktop: 1-9.
        int x, width;
        bool named; // itDesktop: shown with its name (" 2:mail ").
    };

private:

    struct State
    {
        int current {1};
        size_t alerts {0};
        size_t level1 {0};     // Alerts of level 1 and of the other levels.
        size_t others {0};
        std::string alert1;    // level1AlertText.
        bool flashOn {false};  // [alerts] notify = flash: the phase.
        // Windows on each desktop (index 1-9) and whether one of them has
        // an alert.
        std::array<int, 10> counts {};
        std::array<bool, 10> alerted {};
        unsigned names {0}; // desktops::namesVersion.
        bool operator==(const State &o) const
        {
            return current == o.current && alerts == o.alerts &&
                   level1 == o.level1 && others == o.others && alert1 == o.alert1 &&
                   flashOn == o.flashOn &&
                   counts == o.counts && alerted == o.alerted && names == o.names;
        }
    };

    State state_;
    TDeskTop *desk {nullptr};
    // The item under the mouse (-1: none).
    int hovered {-1};

    std::vector<Item> layout() const;
    int itemAt(TPoint local) const;
    std::string hintFor(const Item &item) const;
    void showHint(int index);
    void run(const Item &item);
};

#endif // TVTERM_TOPBAR_H
