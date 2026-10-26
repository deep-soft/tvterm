#ifndef TVTERM_LOGVIEW_H
#define TVTERM_LOGVIEW_H

// Log View (Ctrl-B D): a window showing the lines written by a command
// (e.g. 'tail -F /var/log/maillog'), filtered by two regular expressions
// (include / exclude), with its own history, search, selection and save.
// Windows with the same command share one source (see logsrc.h).
//
// Keys, when the window is focused (also in its menu: F9 or right click):
//   F4 settings · I / E edit include / exclude · F filters on/off
//   D duplicate · R restart the source · Space pause/continue
//   W long lines: wrapped / scrolled (Left/Right) · X clear the window
//   / search · F3 or n older · Shift-F3 or N newer · F2 or s save
//   Ctrl-C copy · Ctrl-E to a new editor · Ctrl-A to an open editor
//   Up/Down PgUp/PgDn Home/End move the cursor line (Shift: select);
//   with the cursor on the last line, new lines are followed.
// The copy keys use the selection or, without one, the cursor line.

#define Uses_TWindow
#define Uses_TView
#include <tvision/tv.h>

#include "alerts.h"
#include "logsrc.h"

#include <deque>
#include <functional>
#include <memory>
#include <string>
#include <stdint.h>

struct LogViewOptions
{
    std::string title;      // Empty: the command.
    std::string command;
    std::string include;    // Regular expression (POSIX extended, like grep -E); empty: all lines.
    std::string exclude;    // Empty: nothing excluded.
    bool ignoreCase {false};
    bool wrap {false};
    int maxLines {0};       // Lines kept by the source; 0: the default (tvterm.ini).
    int autoRestart {0};    // Restart the source N seconds after it ends; 0: never.
};

// Opens the settings dialog. Returns false if cancelled (after Apply, the
// applied options are kept). 'focusField': 0 command, 1 include, 2 exclude.
// 'apply' (Apply button, for an open window): applies the options; returns
// false, with 'error' set, if they cannot be applied.
using LogViewApply = std::function<bool (const LogViewOptions &, std::string &error)>;
bool execLogViewDialog( LogViewOptions &options, bool isNew, int focusField = 0,
                        LogViewApply apply = nullptr );

class LogViewInterior;
class TScrollBar;
struct LogRegex;

class LogViewWindow : public TWindow, public AlertTarget
{
public:

    LogViewWindow(const TRect &bounds, const LogViewOptions &options) noexcept;
    ~LogViewWindow();

    void shutDown() override;
    void handleEvent(TEvent &ev) override;
    void setState(ushort aState, Boolean enable) override;
    const char *getTitle(short maxSize) override;
    void sizeLimits(TPoint &min, TPoint &max) override;

    const LogViewOptions &getOptions() const noexcept { return options; }
    // Applies new options (from the dialog, a profile...). Returns false,
    // with 'error' set, if a regular expression is invalid.
    bool setOptions(const LogViewOptions &newOptions, std::string &error);
    void setCustomTitle(const std::string &title);
    bool isSourceRunning() const noexcept { return source && source->isRunning(); }

    // AlertTarget.
    const AlertSettings &getAlertSettings() const noexcept override { return alertSettings; }
    void setAlertSettings(AlertSettings settings) noexcept override;
    bool hasAlert() const noexcept override { return alert != Alert::None; }
    uint64_t getAlertOrder() const noexcept override { return alertOrder; }

    // Takes the new lines from the source. Called at every idle time.
    void update();

private:

    friend class LogViewInterior;

    enum class Alert { None, Activity, Silence, Keyword };
    using Clock = LogSource::Clock;

    LogViewOptions options;
    std::shared_ptr<LogSource> source;
    std::unique_ptr<LogRegex> includeRe, excludeRe;
    bool filtersOn {true};

    LogViewInterior *interior {nullptr};
    TScrollBar *vScrollBar {nullptr}, *hScrollBar {nullptr};

    // Lines shown: sequence numbers in the source.
    std::deque<uint64_t> shown;
    // Next line of the source to be filtered.
    uint64_t nextSeq {0};
    // X: lines before this one are hidden.
    uint64_t clearedSeq {0};

    // First line on the screen (index in 'shown') and, for wrapped lines,
    // the first row of that line on the screen.
    size_t top {0};
    int topRow {0};
    int leftCol {0};
    bool follow {true};
    bool paused {false};
    // Lines shown after this sequence number are new (title: +N).
    uint64_t newAfterSeq {0};

    // Cursor line, by sequence number. When it is not shown (filters),
    // the nearest line before it is the cursor. With 'follow', it is the
    // last line.
    uint64_t cursorSeq {0};

    // Selection: whole lines, by sequence number.
    bool hasSel {false};
    uint64_t selAnchor {0}, selCursor {0};

    // Search.
    std::string searchText;
    bool searchRegex {false}, searchCase {false};
    std::unique_ptr<LogRegex> searchRe;
    bool hasMatch {false};
    uint64_t matchSeq {0};
    int matchBegin {0}, matchEnd {0}; // Bytes in the line.

    // Alerts.
    AlertSettings alertSettings;
    Alert alert {Alert::None};
    std::string alertKeyword;
    uint64_t alertOrder {0};
    bool silenceArmed {true};
    Clock::time_point silenceSince;
    Clock::time_point sourceStart;

    std::string titleBuf;
    std::string lastTitle;

    void attachSource();
    void detachSource();
    bool passes(const LogLine &line) const;
    void refilter();
    void dropOldLines();
    void checkAlerts(const std::deque<uint64_t> &newLines, size_t from);
    void raiseAlert(Alert kind, const std::string &keyword = {}) noexcept;
    void clearAlert() noexcept;

    // Scrolling.
    int rowsOf(size_t index) const;
    void endPosition(size_t &index, int &row) const;
    bool atEnd() const;
    void scrollRows(int delta);
    void scrollToEnd();
    // 'follow' after the view moved (wheel, scroll bar): at the end, the
    // cursor goes to the last line.
    void followFromView();
    // 'follow' after the cursor moved: on the last line.
    void followFromCursor();
    size_t cursorIndex() const;
    bool cursorIsLast() const;
    bool isCursor(uint64_t seq) const;
    void ensureVisible(size_t index);
    void moveCursorTo(long index, bool extend);
    void updateScrollBars();
    void redraw();
    void updateTitle();
    size_t newLineCount() const;

    // Commands.
    void openMenu(TPoint where);
    void runCommand(ushort cmd);
    void editOptions(int focusField);
    void duplicate();
    void togglePause();
    void toggleFilters();
    void toggleWrap();
    void clearWindow();
    void openSearch();
    void findNext(bool older);
    void copySelection();
    void sendToEditor(bool newEditor);
    void save();
    std::string selectedText() const;
    // The selection or, without one, the cursor line.
    std::string selectedOrCursorText() const;
    std::string shownText() const;
    void selectionBounds(uint64_t &a, uint64_t &b) const;
    bool isSelected(uint64_t seq) const;
};

// Number of Log View windows whose source is running.
class TDeskTop;
size_t countRunningLogViews(TDeskTop &deskTop);

#endif // TVTERM_LOGVIEW_H
