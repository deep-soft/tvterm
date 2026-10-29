#ifndef TVTERM_WND_H
#define TVTERM_WND_H

#include <tvterm/termwnd.h>
#include <tvterm/consts.h>

#include "alerts.h"

#include <chrono>
#include <string>
#include <stdint.h>

class TerminalWindow : public tvterm::BasicTerminalWindow, public AlertTarget
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

    // AlertTarget.
    const AlertSettings &getAlertSettings() const noexcept override { return alertSettings; }
    void setAlertSettings(AlertSettings settings) noexcept override;
    bool hasAlert() const noexcept override { return alert != Alert::None; }
    uint64_t getAlertOrder() const noexcept override { return alertOrder; }
    AlertInfo getAlertInfo() noexcept override;
    Alert getAlert() const noexcept { return alert; }

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
    // Alert::Keyword: the group found and the line.
    std::string alertKeyword, alertGroup, alertLine;
    uint64_t alertOrder {0};
    time_t alertTime {0}; // When the alert got its current kind.
    uint64_t lastOutputBytes {0};
    // Output received during this time is ignored, because it is most
    // likely a redraw (startup, resize, focus change).
    Clock::time_point ignoreOutputUntil;
    // Silence: measured from 'silenceSince' (the start of the terminal, a
    // change of the alert settings, the last output or the loss of the
    // focus). Disarmed by a silence alert, armed again by new output.
    bool silenceArmed {true};
    Clock::time_point silenceSince;
    // Keywords are looked for in whole lines. The last line of the output,
    // if not ended yet, waits here for the rest; if nothing more comes
    // for a while (e.g. a prompt), it is checked as it is.
    std::string pendingLine;
    Clock::time_point pendingSince;
    bool pendingChecked {false};
    std::string titleBuf;

    TRect getMaxBounds();
    void checkAlerts() noexcept;
    void checkKeywordLine(const std::string &line) noexcept;
    void raiseAlert(Alert kind, const KeywordGroup *group = nullptr, const std::string &line = {}) noexcept;
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
