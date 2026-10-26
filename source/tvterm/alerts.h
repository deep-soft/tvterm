#ifndef TVTERM_ALERTS_H
#define TVTERM_ALERTS_H

// Alerts for terminals and Log Views that are not selected: new text
// (activity), new text containing a keyword, or no output for some time
// (silence).

#define Uses_TStatusLine
#include <tvision/tv.h>

#include "statline.h"

#include <string>
#include <vector>
#include <stdint.h>

struct AlertSettings
{
    bool activity {false};
    // Plain text, case-insensitive (ASCII letters only).
    std::vector<std::string> keywords;
    // 0: disabled.
    int silenceSeconds {0};

    bool enabled() const
        { return activity || !keywords.empty() || silenceSeconds > 0; }
};

// "error, reject ,deferred" -> {"error", "reject", "deferred"}.
std::vector<std::string> parseKeywords(const std::string &text);
// {"error", "reject"} -> "error, reject".
std::string joinKeywords(const std::vector<std::string> &keywords);

// A window that can have alerts (terminal, Log View).
class AlertTarget
{
public:

    virtual const AlertSettings &getAlertSettings() const noexcept = 0;
    // Also clears the current alert.
    virtual void setAlertSettings(AlertSettings settings) noexcept = 0;
    virtual bool hasAlert() const noexcept = 0;
    // Increases with each new alert, so that the oldest one can be found.
    virtual uint64_t getAlertOrder() const noexcept = 0;

protected:

    ~AlertTarget() = default;
};

// Order number for a new alert (shared by all windows).
uint64_t newAlertOrder() noexcept;

class TWindow;
class TDeskTop;

// Shows the alert settings of 'window'.
void execAlertSettings(TWindow &window, AlertTarget &target);

// Selects the window with the oldest alert. Returns false if there is none.
bool goToNextAlert(TDeskTop &deskTop);

// Number of windows with an alert.
size_t countAlerts(TDeskTop &deskTop);

// Status line which also shows the number of alerts on its right side
// (not while a temporary message is shown).
class AlertStatusLine : public TVTermStatusLine
{
public:

    using TVTermStatusLine::TVTermStatusLine;

    // Number of windows with an alert.
    size_t alertCount {0};

    void draw() override;
};

#endif // TVTERM_ALERTS_H
