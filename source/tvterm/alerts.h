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
#include <time.h>

// Alert keywords (the Keywords field, the profile key alert-keywords):
//   "a, b"      groups: a line with any of them raises the alert;
//   "a + b"     every term of the group must be in the line;
//   "!c"        exclusion: a line containing c raises no keyword alert
//               (only as an element of its own, between commas);
//   only exclusions: any other new line raises the alert.
// '\' makes the next character literal ('\!', '\+', '\,', '\\', '\ ').
// Spaces at the ends of the terms are dropped. Case is ignored (ASCII
// letters only).
struct KeywordGroup
{
    std::vector<std::string> terms; // Lowercase (ASCII).
    std::string label;              // "a + b", as shown in the alert list.
    std::string shortLabel;         // "a+b", for the mark in the title.
};

struct KeywordRules
{
    std::string text;               // As typed (spaces at the ends dropped).
    std::vector<KeywordGroup> groups;
    std::vector<std::string> excludes; // Lowercase (ASCII).

    bool empty() const { return groups.empty() && excludes.empty(); }
    // The group found in 'lowerLine' (already lowercase), or null.
    const KeywordGroup *match(const std::string &lowerLine) const;
};

// Returns false, with 'error' set, if 'text' has an invalid element; the
// valid ones are kept in 'rules'.
bool parseKeywords(const std::string &text, KeywordRules &rules, std::string &error);
// The same, ignoring the invalid elements.
KeywordRules parseKeywords(const std::string &text);

std::string lowerAscii(std::string s);

struct AlertSettings
{
    bool activity {false};
    KeywordRules keywords;
    // 0: disabled.
    int silenceSeconds {0};

    bool enabled() const
        { return activity || !keywords.empty() || silenceSeconds > 0; }
};

// What the Event Viewer shows about an alert (key A).
struct AlertInfo
{
    time_t time {0};        // When the alert got its current kind.
    std::string mark;       // "[!]", "[!error]", "[!silent]".
    std::string title;      // The window title, without the mark.
    std::string group;      // Keyword alerts: the group found...
    std::string line;       // ...and the line.
};

// The mark of an alert, for the title: "[!] ", "[!silent] ", "[!a+b] ".
// 'kind': 1 activity, 2 silence, 3 keyword.
std::string alertMark(int kind, const std::string &shortLabel);

// "hh:mm:ss" if 't' is today, else "YYYY-MM-DD hh:mm:ss" (local time).
std::string formatEventTime(time_t t);

// A shortened line for the alert list: around the first term of 'group'.
std::string alertLineFragment(const std::string &line, const KeywordGroup *group);

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
    virtual AlertInfo getAlertInfo() noexcept = 0;

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

// The active alerts, from the oldest (the order of Ctrl-B G), as text for
// the Event Viewer (key A).
std::string listAlerts(TDeskTop &deskTop);

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
