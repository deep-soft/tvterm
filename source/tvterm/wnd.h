#ifndef TVTERM_WND_H
#define TVTERM_WND_H

#include <tvterm/termwnd.h>
#include <tvterm/consts.h>

#include <string>

class TerminalWindow : public tvterm::BasicTerminalWindow
{
    using Super = tvterm::BasicTerminalWindow;

public:

    static const tvterm::TVTermConstants appConsts;

    TerminalWindow(const TRect &bounds, tvterm::TerminalController &aTerm) noexcept;

    // How the terminal was started, if from a profile (used when saving
    // the current windows as a profile).
    std::string launchCommand;
    std::string launchDir;
    bool launchKeepOpen {true};

    void handleEvent(TEvent &ev) override;
    void sizeLimits(TPoint &min, TPoint &max) override;
    void changeBounds(const TRect &bounds) override;
    void zoom() override;

private:

    TRect getMaxBounds();
};

inline TerminalWindow::TerminalWindow( const TRect &bounds,
                                       tvterm::TerminalController &aTerm ) noexcept :
    TWindowInit(&initFrame),
    Super(bounds, aTerm, appConsts)
{
}

#endif // TVTERM_WND_H
