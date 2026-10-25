#ifndef TVTERM_WND_H
#define TVTERM_WND_H

#include <tvterm/termwnd.h>
#include <tvterm/consts.h>

#include "alerts.h"

#include <chrono>
#include <string>
#include <stdint.h>

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

    enum class Alert { None, Activity, Silence, Keyword };

    const AlertSettings &getAlertSettings() const noexcept { return alertSettings; }
    // Also clears the current alert.
    void setAlertSettings(AlertSettings settings) noexcept;
    Alert getAlert() const noexcept { return alert; }
    // Increases with each new alert, so that the oldest one can be found.
    uint64_t getAlertOrder() const noexcept { return alertOrder; }

    void handleEvent(TEvent &ev) override;
    void setState(ushort aState, Boolean enable) override;
    const char *getTitle(short maxSize) override;
    void sizeLimits(TPoint &min, TPoint &max) override;
    void changeBounds(const TRect &bounds) override;
    void zoom() override;

private:

    using Clock = std::chrono::steady_clock;

    AlertSettings alertSettings;
    Alert alert {Alert::None};
    std::string alertKeyword; // The keyword found, for Alert::Keyword.
    uint64_t alertOrder {0};
    uint64_t lastOutputBytes {0};
    // Output received during this time is ignored, because it is most
    // likely a redraw (startup, resize, focus change).
    Clock::time_point ignoreOutputUntil;
    // Silence: measured from 'silenceSince' (the start of the terminal, a
    // change of the alert settings, the last output or the loss of the
    // focus). Disarmed by a silence alert, armed again by new output.
    bool silenceArmed {true};
    Clock::time_point silenceSince;
    // End of the previous output, so that keywords split between two
    // reads are found.
    std::string keywordTail;
    std::string titleBuf;

    TRect getMaxBounds();
    void checkAlerts() noexcept;
    void raiseAlert(Alert kind, const std::string &keyword = {}) noexcept;
    void clearAlert() noexcept;
    void ignoreOutputFor(int ms) noexcept;
};

inline TerminalWindow::TerminalWindow( const TRect &bounds,
                                       tvterm::TerminalController &aTerm ) noexcept :
    TWindowInit(&initFrame),
    Super(bounds, aTerm, appConsts)
{
    // Applications draw their first screen at startup. This output does not
    // raise activity or keyword alerts, but it restarts the silence count.
    ignoreOutputFor(3000);
    silenceSince = Clock::now();
}

#endif // TVTERM_WND_H
