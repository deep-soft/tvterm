#ifndef TVTERM_ALERTS_H
#define TVTERM_ALERTS_H

// Alerts for terminals that are not selected: new text (activity), new
// text containing a keyword, or no output for some time (silence).

#define Uses_TStatusLine
#include <tvision/tv.h>

#include "statline.h"

#include <string>
#include <vector>

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

class TerminalWindow;
class TDeskTop;

// Shows the alert settings of 'term'.
void execAlertSettings(TerminalWindow &term);

// Selects the terminal with the oldest alert. Returns false if there is none.
bool goToNextAlert(TDeskTop &deskTop);

// Number of terminals with an alert.
size_t countAlerts(TDeskTop &deskTop);

// Status line which also shows the number of alerts on its right side
// (not while a temporary message is shown).
class AlertStatusLine : public TVTermStatusLine
{
public:

    using TVTermStatusLine::TVTermStatusLine;

    // Number of terminals with an alert.
    size_t alertCount {0};

    void draw() override;
};

#endif // TVTERM_ALERTS_H
