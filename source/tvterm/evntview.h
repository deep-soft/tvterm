#ifndef TVTERM_EVNTVIEW_H
#define TVTERM_EVNTVIEW_H

#define Uses_TWindow
#define Uses_TRect
#include <tvision/tv.h>

#include "profile.h"

#include <iosfwd>
#include <string>

// TEventViewer: a window displaying the attributes of the TEvents received
// by the application. Useful to find out what Turbo Vision receives when a
// key or key combination is pressed.
//
// Adapted from the Turbo Vision 'tvdemo' example, which was inspired by
// TTYWindow from Daniel Ambrose.
//
// Keys, when the window is focused (they are not shown as events):
//   M C K O T   show/hide mouse movements, clicks (and wheel), keys,
//               other events, tvterm's messages (see EventFilter);
//               only new events are affected
//   N           clear, then show again the messages kept by tvterm
//   A           list the active alerts (oldest first)
//   X           clear
//   Ctrl-Right / Ctrl-Left  show the next / previous Desk with windows
//               (E77; the viewer keeps the focus; shown with K+)
//   Ctrl-L      (key-logviews-end, E78) every Log View to its last line
//   F9          the window's menu (also a right click; E78)

class TScrollBar;
class EventTerminal;

class TEventViewer : public TWindow
{
    Boolean stopped {False};
    size_t eventCount {0};
    EventFilter filter;
    EventTerminal *interior {nullptr};
    std::ostream *out {nullptr};

    static void printEvent(std::ostream &out, const TEvent &ev);
    void updateTitle();
    // Whether 'ev' is one of the keys above and the window would handle it.
    bool isOwnKey(const TEvent &ev);

public:

    // 'aBufSize': size of the text kept, in bytes (up to 65000).
    TEventViewer(const TRect &bounds, ushort aBufSize, const EventFilter &aFilter = EventFilter()) noexcept;

    void handleEvent(TEvent &ev) override;
    void shutDown() override;

    void toggle();
    void print(const TEvent &ev);
    // Messages from tvterm itself (settings, saved files, errors). Shown
    // even while the viewer is stopped, unless hidden by the filter;
    // 'always': shown anyway (N key).
    void printNote(const std::string &text, bool always = false);
    // Text shown as it is, even while stopped or with T- (e.g. the alert
    // list, key A).
    void printText(const std::string &text);
    void clear();
    // F9 or a right click (E78): Scroll Log Views to last, Next / Previous
    // Desk.
    void openMenu(TPoint where);

    const EventFilter &getFilter() const { return filter; }
};

#endif // TVTERM_EVNTVIEW_H
