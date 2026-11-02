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
//   Ctrl-B Menu  WinList  Alerts: 2 (Ctrl-B G)              D1  D2  D5
//
// A click on an item runs it (the menu, the Window List, the oldest alert,
// that desktop). The desktop buttons are those with windows, plus the
// current one (the Event Viewer is on every desktop and is not counted):
// the current one is black on green, the others black on cyan, or white on
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
    enum ItemKind { itMenu, itWindowList, itAlerts, itDesktop };
    struct Item
    {
        ItemKind kind;
        int desktop; // itDesktop: 1-9.
        int x, width;
    };

private:

    struct State
    {
        int current {1};
        size_t alerts {0};
        // Windows on each desktop (index 1-9) and whether one of them has
        // an alert.
        std::array<int, 10> counts {};
        std::array<bool, 10> alerted {};
        bool operator==(const State &o) const
        {
            return current == o.current && alerts == o.alerts &&
                   counts == o.counts && alerted == o.alerted;
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
