#ifndef TVTERM_ALERTS_H
#define TVTERM_ALERTS_H

// Alerts for terminals and Log Views that are not selected: new text
// (activity), new text containing a keyword, or no output for some time
// (silence).

#define Uses_TStatusLine
#define Uses_TFrame
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
// Alert levels (item 7, E68): 1 (most urgent) to 5. "N:" at the start of a
// group sets its level ("1:authentication failed, 3:error, warning"); a
// group without it has level 5 (as before levels existed). "\:" is a plain
// ':'. A line matching several groups takes the most urgent one.
enum : int { alertLevelMin = 1, alertLevelMax = 5 };

struct KeywordGroup
{
    std::vector<std::string> terms; // Lowercase (ASCII).
    int level {alertLevelMax};
    std::string label;              // "a + b", as shown in the alert list.
    std::string shortLabel;         // "a+b", for the mark in the title.
};

struct KeywordRules
{
    std::string text;               // As typed (spaces at the ends dropped).
    std::vector<KeywordGroup> groups;
    std::vector<std::string> excludes; // Lowercase (ASCII).

    bool empty() const { return groups.empty() && excludes.empty(); }
    // The group found in 'lowerLine' (already lowercase), or null; with
    // several, the most urgent (the first one of that level).
    const KeywordGroup *match(const std::string &lowerLine) const;
};

// Returns false, with 'error' set, if 'text' has an invalid element; the
// valid ones are kept in 'rules'.
bool parseKeywords(const std::string &text, KeywordRules &rules, std::string &error);
// The same, ignoring the invalid elements.
KeywordRules parseKeywords(const std::string &text);

std::string lowerAscii(std::string s);

// "N:" (1-5) at the start of 'text' (spaces around it allowed): 'level' set
// and 'rest' without it; none: level 5 and 'rest' = 'text'. False if the
// text starts with a digit and ':' but the digit is not 1-5.
bool splitAlertLevel(const std::string &text, int &level, std::string &rest);

// "3:" for levels 1-4, "" for 5 (the default), to write before a value.
std::string alertLevelPrefix(int level);

struct AlertSettings
{
    bool activity {false};
    int activityLevel {alertLevelMax};
    KeywordRules keywords;
    // 0: disabled.
    int silenceSeconds {0};
    int silenceLevel {alertLevelMax};
    // Command run for the level 1 alerts of this window (empty: the
    // [alerts] action of tvterm.ini).
    std::string action;

    bool enabled() const
        { return activity || !keywords.empty() || silenceSeconds > 0; }
};

// What the Event Viewer shows about an alert (key A).
struct AlertInfo
{
    time_t time {0};        // When the alert got its current kind.
    int level {0};          // 1-5.
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
    // 1-5 (0: no alert).
    virtual int getAlertLevel() const noexcept = 0;
    virtual AlertInfo getAlertInfo() noexcept = 0;
    // Removes the current alert without selecting the window (More... >
    // Clear alerts of levels 2-5).
    virtual void dismissAlert() noexcept = 0;
    // The window got the focus while it had an alert (the alert is gone);
    // true until it loses the focus ([alerts] seen-color).
    bool isAlertSeen() const noexcept { return alertSeen; }

protected:

    ~AlertTarget() = default;

    bool alertSeen {false};
};

// Order number for a new alert (shared by all windows).
uint64_t newAlertOrder() noexcept;

// [alerts] title-color: the title text of a window with an alert is drawn
// in the color of its level (true, the default: 1 and 5 white on red, 2
// on magenta, 3 on brown, 4 on cyan) or in the normal colors (false).
extern bool alertTitleColor;
uint8_t alertLevelColor(int level) noexcept;

// [alerts] keys of tvterm.ini about level 1 alerts and the alert log.
struct AlertConfig
{
    // Command for level 1 alerts of windows without their own
    // (alert-action in the profile); empty: none.
    std::string action;
    // A command is not run again for the same level, keyword and window
    // within this time (seconds; 0: always).
    int actionInterval {60};
    // File where every alert is written (empty: none; '~' allowed).
    std::string log;
    // How a level 1 alert is shown (notify): the top bar item flashes,
    // a far2l desktop notification, the terminal bell.
    bool flash {true};
    bool notify {false};
    bool bell {false};
    // notify / bell repeated while a level 1 alert is not seen (seconds;
    // 0: once).
    int notifyRepeat {60};
};
extern AlertConfig alertConfig;

class TWindow;

// A line matched a keyword group, or an activity or silence alert came
// (keyword "activity" / "silent"): written to the alert log, and, if
// 'raise' (the window is not selected) and the level is 1, the action is
// run ('windowAction', else the global one). 'title': the window title
// without the mark.
void alertEvent(TWindow *w, const std::string &title, int level, const std::string &keyword,
                const std::string &line, const std::string &windowAction, bool raise) noexcept;

// Called periodically: the far2l notification and the bell of level 1
// alerts (first time and repeats).
class TDeskTop;
void alertTick(TDeskTop &deskTop) noexcept;

// [alerts] seen-color: the title text of a window that got the focus while
// it had an alert, until it loses the focus: 0 (none) or the color
// attribute (black on yellow, the default, or black on cyan).
extern uint8_t alertSeenColor;
enum : uint8_t { alertSeenYellow = 0xE0, alertSeenCyan = 0xB0 };

// Draws the title of 'frame' again, white on red, if its window has an
// alert (and 'alertTitleColor'), or in 'alertSeenColor' if the alert was
// just seen. Called after TFrame::draw, at the same place and with the same
// width; the frame itself keeps its colors.
void drawAlertTitle(TFrame &frame) noexcept;

// A frame (TFrame or a class derived from it) whose title shows the alert.
template <class Frame>
class AlertTitleFrame : public Frame
{
public:

    using Frame::Frame;

    void draw() override
    {
        Frame::draw();
        drawAlertTitle(*this);
    }
};

// Shows the alert settings of 'window'.
void execAlertSettings(TWindow &window, AlertTarget &target);

// Selects the window with the most urgent alert, the oldest of that level.
// Returns false if there is none.
bool goToNextAlert(TDeskTop &deskTop);

// Number of windows with an alert.
size_t countAlerts(TDeskTop &deskTop);
// The same, split: level 1 and the other levels.
void countAlerts(TDeskTop &deskTop, size_t &level1, size_t &others);
// "A1 <title>: <keyword>" of the oldest level 1 alert (empty: none).
std::string level1AlertText(TDeskTop &deskTop);

// Removes the alerts of levels 2-5 (More... > Clear alerts); the number
// removed.
size_t clearLowAlerts(TDeskTop &deskTop);

// The active alerts, the most urgent first, then from the oldest (the
// order of Ctrl-B G), as text for the Event Viewer (key A): time, level,
// desktop (as in the Window List), mark, title.
std::string listAlerts(TDeskTop &deskTop);

#endif // TVTERM_ALERTS_H
