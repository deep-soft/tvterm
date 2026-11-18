#define Uses_TWindow
#define Uses_TProgram
#define Uses_TDeskTop
#define Uses_TApplication
#include <tvision/tv.h>

#include "help.h"
#include "app.h"
#include "editwnd.h"
#include "evntview.h"
#include "logsrc.h"
#include "logview.h"
#include "settings.h"
#include "winlist.h"
#include "wnd.h"

#include <string>
#include <vector>

// Own namespace for the helpers: this file is compiled together with others
// (Unity Build).
namespace helptext
{

struct Entry
{
    std::string keys;  // As shown; empty: a line of text (no key column).
    std::string action;
};

enum { keyWidth = 30 };

static const char *const sectionTitles[help::None] =
{
    "Global keys (every window)", "Terminal", "Log View", "Viewer", "Editor", "Event Viewer",
};

static std::string keysOf(const std::vector<GlobalKey> &keys)
// "Ctrl-B, Ctrl-`" or "(not set)".
{
    std::string out;
    for (const auto &k : keys)
        out += (out.empty() ? "" : ", ") + k.name();
    return out.empty() ? "(not set)" : out;
}

static std::string keyOf(TKey key)
{
    return key.code == 0 ? "(not set)" : keyName(key);
}

static std::vector<Entry> entries(help::Section s)
// The keys of section 's'; the keys of tvterm.ini as they are set now.
{
    const Settings &st = TVTermApp::settings();
    const auto &g = st.globalKeys;
    // The first menu key, as in the status line ("Ctrl-B").
    const std::string &menu = globalkeys::menuKeyName();
    switch (s)
    {
        case help::Global:
            return {
                {keysOf(g[gaMenu]), "the menu (menu-key); its letters are shown in it"},
                {keysOf(g[gaZoom]), "maximize / restore the window (key-zoom)"},
                {keysOf(g[gaWindowLeft]), "the window on the left (key-window-left)"},
                {keysOf(g[gaWindowRight]), "the window on the right (key-window-right)"},
                {keysOf(g[gaWindowUp]), "the window above (key-window-up)"},
                {keysOf(g[gaWindowDown]), "the window below (key-window-down)"},
                {"F1", "this help (not in a terminal)"},
                {"Ctrl-W", "close the window (not in a terminal)"},
                {"Ctrl-F10 / Ctrl-F11", "Capture Desk / Capture Window (not in a terminal)"},
            };
        case help::Terminal:
            return {
                {"", "Keys go to the program in the terminal, except:"},
                {keyOf(st.keyPageUp), "history: one page up (history-page-up)"},
                {keyOf(st.keyPageDown), "history: one page down (history-page-down)"},
                {keyOf(st.keyTop), "history: the top (history-top)"},
                {keyOf(st.keyBottom), "history: the bottom (history-bottom)"},
                {"Shift-Ins", "paste"},
                {"Shift+click, drag", "select text (shift-click-select); Ctrl-C copies"},
                {menu + " H", "history mode: Up Down PgUp PgDn Home End scroll, / search,"},
                {"", "F3 / Shift-F3 next / previous, F2 save, Ctrl-C copy, Esc exit"},
                {menu + " T T", "select text: drag, Ctrl-C copy, Esc cancel"},
                {menu + " Z P", "pin / unpin: not closed by mistake ([P] in the title)"},
            };
        case help::LogView:
            return {
                {"F9, right click", "the window's menu"},
                {"F4", "settings dialog (I / E: on Include / Exclude)"},
                {"F", "filters on / off"},
                {"H", "colors (highlight) on / off"},
                {"T", "transformation on / off"},
                {"D", "duplicate the window"},
                {"+", "duplicate with the double-clicked word (And include)"},
                {"R", "restart the command"},
                {"Space", "pause / continue"},
                {"W", "long lines: wrapped / scrolled (Left / Right)"},
                {"X", "clear the window (not the source)"},
                {"Enter", "mark checked (#date time ---- at the end)"},
                {"/", "search (New window: the matching lines in a new window)"},
                {"F3 or n / Shift-F3 or N", "next match, older / newer"},
                {"Ctrl-Down / Ctrl-Up", "next / previous highlighted line (Words)"},
                {"Up Down PgUp PgDn Home End", "move the cursor line (End: follow new lines)"},
                {"Shift + those, Shift+click", "select lines (also by dragging)"},
                {"Ctrl-C or Ctrl-Ins", "copy the selection, or else the cursor line"},
                {"Ctrl-E / Ctrl-Shift-E", "to a new editor / to an open editor"},
                {"F2 or s", "save the lines shown, or the selection"},
                {"Esc", "clear the selection, then the match and the word"},
                {"double click", "copy a word (kept for +)"},
                {"Ctrl-A", "go to the next alert"},
                {"F5", "alerts of this window"},
                {keyOf(st.keyLogViewPin), "pin / unpin: Ctrl-W and Close refuse it (key-logview-pin)"},
            };
        case help::Viewer:
            return {
                {"", "View file (" + menu + " F), Search > New window and this help: fixed"},
                {"", "lines, with the Log View keys except Space, R and F5 (no command)."},
            };
        case help::Editor:
            return {
                {"F9", "the window's menu (Read only, Pin, Open, Reload...)"},
                {"F2 / F12 / Ctrl-F12", "save / save as / open another file here"},
                {"F7 or Ctrl-F", "find"},
                {"Ctrl-F7 or Ctrl-H", "replace"},
                {"F3, Shift-F7 or Ctrl-L", "find (replace) again"},
                {"Ctrl-Z, Ctrl-U, Alt-Backspace", "undo"},
                {"Ctrl-Shift-Z", "redo (also Alt-Shift-Backspace)"},
                {"Ctrl-Down / Ctrl-Up", "next / previous line with text in the cursor's column"},
                {"Ctrl-Shift-Down / -Up", "the same, extending the selection"},
                {"Alt-F8", "go to line"},
                {"Alt-D", "duplicate the line (or the selected lines)"},
                {"Alt-W", "select the word"},
                {keyOf(st.keyEditorComment), "comment line with the date and time (key-editor-comment)"},
                {keyOf(st.keyEditorIndent) + " / " + keyOf(st.keyEditorUnindent),
                 "one space more / less at the start of the line(s)"},
                {keyOf(st.keyEditorWhitespace), "show / hide spaces, tabs and line ends ([WS] in the title)"},
                {"Ctrl-Ins / Shift-Del", "copy / cut (the clipboard)"},
                {"Shift-Ins", "paste"},
                {"Shift + arrows, Ctrl-K B / K", "select; Ctrl-K H hides the selection"},
                {"Ctrl-Left / Ctrl-Right", "word left / right"},
                {"Home / End, Ctrl-Home / End", "line start / end, text start / end"},
                {"Ctrl-Y / Ctrl-Q Y", "delete the line / to the end of the line"},
                {"Ctrl-T, Ctrl-Backspace", "delete the word right / left"},
                {"Ins", "insert / overwrite"},
                {"Ctrl-A", "select all"},
            };
        case help::EventViewer:
            return {
                {"M C K O T", "show mouse moves, clicks, keys, other events, messages on / off"},
                {"N", "clear and show the kept messages again"},
                {"Shift-N", "empty the kept messages, not the startup ones (also F9)"},
                {"A", "list the active alerts"},
                {"L", "list the Log Views open now with their commands (also F9)"},
                {"X", "clear the window"},
                {"Space", "pause / resume (like " + menu + " V)"},
                {"Ctrl-A", "go to the next alert"},
                {"F8", "clear the alerts of levels 2-5"},
                {"Ctrl-Right / Ctrl-Left", "show the next / previous Desk with windows"},
                {keyOf(st.keyLogViewsEnd), "every Log View to its last line (key-logviews-end)"},
                {"F9, right click", "the window's menu"},
                {"Up Down PgUp PgDn Home End", "move the cursor line (End: follow new lines)"},
                {"Shift + those, click, drag", "select lines (no scrolling until Esc)"},
                {"Ctrl-C or Ctrl-Ins", "copy the selection, or else the cursor line"},
                {"Esc", "clear the selection, follow new lines again"},
            };
        default:
            return {};
    }
}

static void addSection(std::vector<std::string> &lines, help::Section s, help::Section current, int &markLine)
{
    if (!lines.empty())
        lines.push_back("");
    std::string title = sectionTitles[s];
    if (s == current)
    {
        markLine = (int) lines.size();
        title += "  (this window)";
    }
    lines.push_back(title);
    lines.push_back(std::string(title.size(), '-'));
    for (const auto &e : entries(s))
    {
        std::string line = "  " + e.keys;
        if (!e.keys.empty() || !e.action.empty())
        {
            if (line.size() < keyWidth + 2)
                line.append(keyWidth + 2 - line.size(), ' ');
            line += "  " + e.action;
        }
        lines.push_back(line);
    }
}

static const char *const helpTitle = "tvterm keys";

} // namespace helptext

namespace help
{

Section sectionOf(TWindow *w)
{
    if (!w)
        return None;
    if (auto *lw = dynamic_cast<LogViewWindow *>(w))
        return lw->hasFixedLines() ? Viewer : LogView;
    if (dynamic_cast<TVTermEditWindow *>(w))
        return Editor;
    if (dynamic_cast<TEventViewer *>(w))
        return EventViewer;
    if (dynamic_cast<TerminalWindow *>(w))
        return Terminal;
    return None;
}

std::string text(Section current, bool currentFirst, int &markLine)
{
    using namespace helptext;
    markLine = -1;
    std::vector<std::string> lines;
    lines.push_back("tvterm keys (F1). Search: / or F3; Ctrl-C copies; Ctrl-W closes.");
    if (currentFirst && current != None)
        addSection(lines, current, current, markLine);
    for (int s = Global; s < None; ++s)
        if (!(currentFirst && s == current))
            addSection(lines, (Section) s, current, markLine);
    std::string out;
    for (auto &l : lines)
        out += l + "\n";
    return out;
}

bool isHelpWindow(TWindow *w)
{
    auto *lw = dynamic_cast<LogViewWindow *>(w);
    return lw && lw->isHelpView();
}

void open(TWindow *from)
{
    using namespace helptext;
    Section current = sectionOf(from);
    // The Help window itself: its own section is Viewer, the others stay as
    // they were shown.
    for (TWindow *w : getDeskTopWindows(*TProgram::deskTop))
        if (isHelpWindow(w))
        {
            if (w == from)
                current = Viewer;
            w->close();
        }
    int markLine;
    std::string t = text(current, TVTermApp::settings().helpCurrentFirst, markLine);
    std::vector<LogLine> lines;
    size_t start = 0;
    while (start < t.size())
    {
        size_t end = t.find('\n', start);
        LogLine line;
        line.text = t.substr(start, end - start);
        // The current section's title: yellow (a line of tvterm's own).
        line.marker = (int) lines.size() == markLine;
        lines.push_back(std::move(line));
        start = end + 1;
    }
    LogViewOptions o;
    o.command = helpTitle;
    o.title = helpTitle;
    auto *w = new LogViewWindow(TProgram::deskTop->getExtent(), o,
                                LogSource::createFixed(helpTitle, std::move(lines)));
    w->setHelpView(true);
    TProgram::application->insertWindow(w);
    // The current section at the top, its title in yellow, the cursor line
    // on its first key (else the cursor on the first line).
    // (help-current-first = yes: from the first line, the section is there.)
    if (markLine >= 0)
        w->showAt(TVTermApp::settings().helpCurrentFirst ? 0 : (size_t) markLine, (size_t) markLine + 2);
    else
        w->showAt(0, 0);
}

} // namespace help
