#ifndef TVTERM_EVNTVIEW_H
#define TVTERM_EVNTVIEW_H

#define Uses_TWindow
#define Uses_TRect
#include <tvision/tv.h>

#include <iosfwd>
#include <string>

// TEventViewer: a window displaying the attributes of the TEvents received
// by the application. Useful to find out what Turbo Vision receives when a
// key or key combination is pressed.
//
// Adapted from the Turbo Vision 'tvdemo' example, which was inspired by
// TTYWindow from Daniel Ambrose.

class TTerminal;
class TScrollBar;

class TEventViewer : public TWindow
{
    Boolean stopped {False};
    size_t eventCount {0};
    TTerminal *interior {nullptr};
    std::ostream *out {nullptr};

    static const char * const titles[2];

    static void printEvent(std::ostream &out, const TEvent &ev);

public:

    TEventViewer(const TRect &bounds, ushort aBufSize) noexcept;
    ~TEventViewer();

    void handleEvent(TEvent &ev) override;
    void shutDown() override;

    void toggle();
    void print(const TEvent &ev);
    // Messages from tvterm itself (settings, saved files, errors). Shown
    // even while the viewer is stopped.
    void printNote(const std::string &text);
};

#endif // TVTERM_EVNTVIEW_H
