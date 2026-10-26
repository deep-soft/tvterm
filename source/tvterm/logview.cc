#define Uses_TKeys
#define Uses_TEvent
#define Uses_TFrame
#define Uses_TScrollBar
#define Uses_TDrawBuffer
#define Uses_TText
#define Uses_TMenuItem
#define Uses_TSubMenu
#define Uses_TProgram
#define Uses_TApplication
#define Uses_TDeskTop
#define Uses_TDialog
#define Uses_TInputLine
#define Uses_THistory
#define Uses_TLabel
#define Uses_TCheckBoxes
#define Uses_TSItem
#define Uses_TStaticText
#define Uses_TButton
#define Uses_TListViewer
#define Uses_TEditWindow
#define Uses_TFileEditor
#define Uses_TClipboard
#define Uses_MsgBox
#include <tvision/tv.h>

#include "logview.h"
#include "app.h"
#include "cmds.h"
#include "editwnd.h"
#include "histdlg.h"
#include "settings.h"
#include "statline.h"
#include "winlist.h"
#include <tvterm/termopts.h>

#include <regex.h>
#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>

// A compiled regular expression (POSIX extended, like 'grep -E').
struct LogRegex
{
    regex_t re;
    bool compiled {false};

    LogRegex() = default;
    LogRegex(const LogRegex &) = delete;
    LogRegex &operator=(const LogRegex &) = delete;
    ~LogRegex() { if (compiled) regfree(&re); }

    // Returns null, with 'error' set, if 'pattern' is invalid.
    static std::unique_ptr<LogRegex> compile( const std::string &pattern, bool ignoreCase,
                                              bool positions, std::string &error )
    {
        std::unique_ptr<LogRegex> r(new LogRegex);
        int flags = REG_EXTENDED | (ignoreCase ? REG_ICASE : 0) | (positions ? 0 : REG_NOSUB);
        int err = regcomp(&r->re, pattern.c_str(), flags);
        if (err != 0)
        {
            char buf[256];
            regerror(err, &r->re, buf, sizeof(buf));
            error = buf;
            // 'regcomp' frees what it allocated when it fails.
            return nullptr;
        }
        r->compiled = true;
        return r;
    }

    bool matches(const std::string &text) const
    {
        return regexec(&re, text.c_str(), 0, nullptr, 0) == 0;
    }

    // First match at or after byte 'from'.
    bool find(const std::string &text, size_t from, int &begin, int &end) const
    {
        regmatch_t m[1];
        if (from > text.size())
            return false;
        int eflags = from > 0 ? REG_NOTBOL : 0;
        if (regexec(&re, text.c_str() + from, 1, m, eflags) != 0 || m[0].rm_so < 0)
            return false;
        begin = (int) (from + m[0].rm_so);
        end = (int) (from + m[0].rm_eo);
        return true;
    }
};

// Own namespace: this file is compiled together with others (Unity Build).
namespace logview
{

enum : ushort
{
    // Commands of the window menu (F9). They are handled by the window.
    cmLogSettings = 3300,
    cmLogInclude,
    cmLogExclude,
    cmLogFilters,
    cmLogDuplicate,
    cmLogRestart,
    cmLogPause,
    cmLogWrap,
    cmLogClear,
    cmLogSearch,
    cmLogFindOlder,
    cmLogFindNewer,
    cmLogCopy,
    cmLogToNewEditor,
    cmLogToEditor,
    cmLogSave,
    cmLogAlerts,
    cmLogChooseEditor,
};

enum : ushort { hiLogCommand = 76 }; // History of the 'Command' field.

// Colors (background, foreground): like a terminal.
static const TColorAttr colNormal = 0x07;   // Light gray on black.
static const TColorAttr colMarker = 0x0E;   // Yellow on black.
static const TColorAttr colSelected = 0x70; // Black on light gray.
static const TColorAttr colCursor = 0x30;   // Black on cyan (focused window only).
static const TColorAttr colMatch = 0xE0;    // Black on yellow.

static const int tabWidth = 8;

static std::string expandTabs(const std::string &s)
// Tabs -> spaces, up to the next multiple of 8 columns.
{
    if (s.find('\t') == std::string::npos)
        return s;
    std::string out;
    out.reserve(s.size() + 16);
    size_t col = 0;
    size_t i = 0;
    while (i < s.size())
    {
        if (s[i] == '\t')
        {
            size_t n = tabWidth - col % tabWidth;
            out.append(n, ' ');
            col += n;
            ++i;
            continue;
        }
        size_t start = i, width = 0;
        TText::next(s, i, width);
        if (i == start)
            ++i;
        out.append(s, start, i - start);
        col += width;
    }
    return out;
}

static int columnOf(const std::string &raw, size_t bytes)
// Column of byte 'bytes' of 'raw', once tabs are expanded.
{
    return (int) TText::width(expandTabs(raw.substr(0, std::min(bytes, raw.size()))));
}

static void rowStarts(const std::string &text, int width, std::vector<size_t> &starts)
// Wrapped line: byte where each row starts.
{
    starts.clear();
    starts.push_back(0);
    if (width <= 0)
        return;
    size_t pos = 0;
    while (pos < text.size())
    {
        TStringView rest(text.data() + pos, text.size() - pos);
        size_t len = TText::scroll(rest, width, False);
        if (len == 0)
            len = std::max<size_t>(TText::next(rest), 1);
        pos += len;
        if (pos < text.size())
            starts.push_back(pos);
    }
}

static bool isWordChar(unsigned char c)
{
    if (c >= 0x80 || isalnum(c))
        return true;
    const auto &chars = tvterm::terminalOptions.wordChars;
    return chars.find((char32_t) c) != std::u32string::npos;
}

static std::string trim(const std::string &s)
{
    size_t a = s.find_first_not_of(" \t");
    if (a == std::string::npos)
        return {};
    size_t b = s.find_last_not_of(" \t");
    return s.substr(a, b - a + 1);
}

static bool findPlain( const std::string &text, const std::string &needle, bool caseSensitive,
                       size_t from, int &begin, int &end )
{
    if (needle.empty() || from > text.size())
        return false;
    auto eq = [caseSensitive] (char a, char b) {
        return caseSensitive ? a == b
                             : tolower((unsigned char) a) == tolower((unsigned char) b);
    };
    auto it = std::search(text.begin() + from, text.end(), needle.begin(), needle.end(), eq);
    if (it == text.end())
        return false;
    begin = (int) (it - text.begin());
    end = begin + (int) needle.size();
    return true;
}

static int effectiveMaxLines(const LogViewOptions &o)
{
    return o.maxLines > 0 ? o.maxLines : TVTermApp::settings().logViewMaxLines;
}

static void showStatus(const std::string &text)
{
    TVTermStatusLine::showMessage(text);
}

} // namespace logview


// The area of the window where the lines are drawn.
class LogViewInterior : public TView
{
public:

    LogViewWindow &w;

    LogViewInterior(const TRect &bounds, LogViewWindow &aWindow) noexcept :
        TView(bounds),
        w(aWindow)
    {
        growMode = gfGrowHiX | gfGrowHiY;
        options |= ofSelectable;
        eventMask |= evMouseWheel;
        helpCtx = hcLogView;
    }

    void draw() override;
    void handleEvent(TEvent &ev) override;

    // The line (index in 'shown') and its row drawn at row 'y' of the view.
    bool lineAt(int y, size_t &index, int &row) const;

private:

    void copyWordAt(TPoint p);
};

bool LogViewInterior::lineAt(int y, size_t &index, int &row) const
{
    using namespace logview;
    size_t i = w.top;
    int r = w.topRow;
    for (int cur = 0; i < w.shown.size(); )
    {
        int rows = w.rowsOf(i);
        int take = rows - r;
        if (y < cur + take)
        {
            index = i;
            row = r + (y - cur);
            return true;
        }
        cur += take;
        ++i;
        r = 0;
    }
    return false;
}

void LogViewInterior::draw()
{
    using namespace logview;
    TDrawBuffer b;
    size_t i = w.top;
    int r = w.topRow;
    std::vector<size_t> starts;
    std::string text;
    int y = 0;
    while (y < size.y)
    {
        b.moveChar(0, ' ', colNormal, size.x);
        if (w.source && i < w.shown.size())
        {
            uint64_t seq = w.shown[i];
            const LogLine &line = w.source->line(seq);
            text = expandTabs(line.text);
            bool isCursor = (w.state & sfFocused) && w.isCursor(seq);
            TColorAttr attr = isCursor ? colCursor :
                              w.isSelected(seq) ? colSelected :
                              line.marker ? colMarker : colNormal;
            if (attr != colNormal && attr != colMarker)
                b.moveChar(0, ' ', attr, size.x);
            int matchB = -1, matchE = -1;
            if (w.hasMatch && w.matchSeq == seq)
            {
                matchB = columnOf(line.text, w.matchBegin);
                matchE = columnOf(line.text, w.matchEnd);
            }
            int startCol;
            if (w.options.wrap)
            {
                rowStarts(text, size.x, starts);
                size_t from = starts[std::min((size_t) r, starts.size() - 1)];
                size_t to = (size_t) r + 1 < starts.size() ? starts[r + 1] : text.size();
                b.moveStr(0, TStringView(text.data() + from, to - from), attr, size.x);
                startCol = (int) TText::width(TStringView(text.data(), from));
            }
            else
            {
                b.moveStr(0, text, attr, size.x, w.leftCol);
                startCol = w.leftCol;
            }
            for (int c = std::max(matchB, startCol); c < matchE; ++c)
                if (c - startCol < size.x)
                    b.putAttribute(c - startCol, colMatch);
            if (++r >= w.rowsOf(i))
            {
                ++i;
                r = 0;
            }
        }
        writeLine(0, y, size.x, 1, b);
        ++y;
    }
}

void LogViewInterior::copyWordAt(TPoint p)
{
    using namespace logview;
    size_t index;
    int row;
    if (!w.source || !lineAt(p.y, index, row))
        return;
    std::string text = expandTabs(w.source->line(w.shown[index]).text);
    int col = p.x;
    if (w.options.wrap)
    {
        std::vector<size_t> starts;
        rowStarts(text, size.x, starts);
        col += (int) TText::width(TStringView(text.data(), starts[std::min((size_t) row, starts.size() - 1)]));
    }
    else
        col += w.leftCol;
    size_t pos = TText::scroll(text, col, False);
    if (pos >= text.size() || !isWordChar((unsigned char) text[pos]))
        return;
    size_t a = pos, b = pos;
    while (a > 0 && isWordChar((unsigned char) text[a - 1]))
        --a;
    while (b < text.size() && isWordChar((unsigned char) text[b]))
        ++b;
    std::string word = text.substr(a, b - a);
    TClipboard::setText(word);
    showStatus("Copied \"" + word + "\" to the clipboard.");
}

void LogViewInterior::handleEvent(TEvent &ev)
{
    using namespace logview;
    TView::handleEvent(ev);
    if (ev.what == evMouseWheel)
    {
        int n = std::max(1, (int) tvterm::terminalOptions.wheelLines);
        switch (ev.mouse.wheel)
        {
            case mwUp: w.scrollRows(-n); w.followFromView(); break;
            case mwDown: w.scrollRows(n); w.followFromView(); break;
            case mwLeft: if (!w.options.wrap) w.leftCol = std::max(0, w.leftCol - 8); break;
            case mwRight: if (!w.options.wrap) w.leftCol += 8; break;
        }
        w.redraw();
        clearEvent(ev);
    }
    else if (ev.what == evMouseDown)
    {
        if (ev.mouse.buttons & mbRightButton)
        {
            clearEvent(ev);
            w.openMenu(ev.mouse.where);
            return;
        }
        if (!(ev.mouse.buttons & mbLeftButton))
            return;
        TPoint p = makeLocal(ev.mouse.where);
        if (ev.mouse.eventFlags & meDoubleClick)
        {
            copyWordAt(p);
            clearEvent(ev);
            return;
        }
        size_t index;
        int row;
        if (!lineAt(p.y, index, row))
        {
            w.hasSel = false;
            w.redraw();
            clearEvent(ev);
            return;
        }
        uint64_t seq = w.shown[index];
        if (ev.mouse.controlKeyState & kbShift)
        {
            // From the selection or, without one, from the cursor line.
            if (!w.hasSel)
                w.selAnchor = w.shown[w.cursorIndex()];
            w.hasSel = true;
            w.selCursor = seq;
        }
        else
        {
            w.hasSel = true;
            w.selAnchor = w.selCursor = seq;
        }
        w.cursorSeq = seq;
        w.redraw();
        while (mouseEvent(ev, evMouseMove | evMouseAuto))
        {
            p = makeLocal(ev.mouse.where);
            if (p.y < 0)
            {
                w.scrollRows(-1);
                p.y = 0;
            }
            else if (p.y >= size.y)
            {
                w.scrollRows(1);
                p.y = size.y - 1;
            }
            if (lineAt(p.y, index, row))
                w.selCursor = w.cursorSeq = w.shown[index];
            w.redraw();
        }
        w.followFromCursor();
        w.redraw();
        clearEvent(ev);
    }
}

// Settings dialog.

namespace logview
{

enum : ushort { cmLogApply = 3390 };

class LogViewDialog : public TDialog
{
public:

    LogViewDialog(LogViewOptions &options, bool isNew, int focusField, LogViewApply apply) noexcept;

    Boolean valid(ushort command) override;
    void handleEvent(TEvent &ev) override;

private:

    LogViewOptions &o;
    LogViewApply apply;
    TInputLine *titleIn, *cmdIn, *incIn, *excIn, *maxIn, *restartIn;
    TCheckBoxes *checkBoxes;

    // Reads and checks the fields. On error: a message, the focus on the
    // field, and false.
    bool readFields(LogViewOptions &out);
    void fail(TView *field, const std::string &text);
};

LogViewDialog::LogViewDialog( LogViewOptions &options, bool isNew, int focusField,
                              LogViewApply aApply ) noexcept :
    TWindowInit(&initFrame),
    TDialog(TRect(0, 0, 76, 23), isNew ? "New Log View" : "Log View settings"),
    o(options),
    apply(std::move(aApply))
{
    TView::options |= ofCentered;
    titleIn = new TInputLine(TRect(3, 3, 72, 4), 128);
    insert(titleIn);
    insert(new TLabel(TRect(2, 2, 72, 3), "~T~itle (empty: the command)", titleIn));
    cmdIn = new TInputLine(TRect(3, 6, 69, 7), 512);
    insert(cmdIn);
    insert(new THistory(TRect(69, 6, 72, 7), cmdIn, hiLogCommand));
    insert(new TLabel(TRect(2, 5, 72, 6), "~C~ommand (e.g. tail -F -n 5000 /var/log/maillog)", cmdIn));
    incIn = new TInputLine(TRect(3, 9, 72, 10), 512);
    insert(incIn);
    insert(new TLabel(TRect(2, 8, 72, 9), "~I~nclude: show only lines matching (empty: all)", incIn));
    excIn = new TInputLine(TRect(3, 12, 72, 13), 512);
    insert(excIn);
    insert(new TLabel(TRect(2, 11, 72, 12), "~E~xclude: hide lines matching (empty: none)", excIn));
    checkBoxes = new TCheckBoxes(TRect(3, 14, 30, 17),
        new TSItem("I~g~nore case",
        new TSItem("~W~rap long lines",
        new TSItem("Filters ~o~n", nullptr))));
    insert(checkBoxes);
    maxIn = new TInputLine(TRect(62, 14, 72, 15), 8);
    insert(maxIn);
    insert(new TLabel(TRect(32, 14, 62, 15), "Ma~x~ lines (0: tvterm.ini)", maxIn));
    restartIn = new TInputLine(TRect(62, 15, 72, 16), 6);
    insert(restartIn);
    insert(new TLabel(TRect(32, 15, 62, 16), "~R~estart after N s (0: no)", restartIn));
    insert(new TStaticText(TRect(3, 18, 73, 20),
        "Regular expressions like grep -E; alternatives with |, e.g.\n"
        "dstport=(993|465|587)  Trailing spaces are kept."));
    if (apply)
    {
        // Apply: the changes are applied and the dialog stays open.
        insert(new TButton(TRect(17, 20, 29, 22), "O~K~", cmOK, bfDefault));
        insert(new TButton(TRect(32, 20, 44, 22), "~A~pply", cmLogApply, bfNormal));
        insert(new TButton(TRect(47, 20, 59, 22), "Cancel", cmCancel, bfNormal));
    }
    else
    {
        insert(new TButton(TRect(24, 20, 36, 22), "O~K~", cmOK, bfDefault));
        insert(new TButton(TRect(40, 20, 52, 22), "Cancel", cmCancel, bfNormal));
    }

    // setData copies the whole buffer of the field.
    auto setField = [] (TInputLine *field, const std::string &text) {
        std::vector<char> buf(field->dataSize(), '\0');
        memcpy(buf.data(), text.data(), std::min(text.size(), buf.size() - 1));
        field->setData(buf.data());
    };
    setField(titleIn, o.title);
    setField(cmdIn, o.command);
    setField(incIn, o.include);
    setField(excIn, o.exclude);
    ushort checks = (o.ignoreCase ? 1 : 0) | (o.wrap ? 2 : 0) | (o.filters ? 4 : 0);
    checkBoxes->setData(&checks);
    setField(maxIn, std::to_string(o.maxLines));
    setField(restartIn, std::to_string(o.autoRestart));
    (focusField == 1 ? incIn : focusField == 2 ? excIn : isNew ? cmdIn : titleIn)->select();
}

void LogViewDialog::fail(TView *field, const std::string &text)
{
    messageBox(text.c_str(), mfError | mfOKButton);
    field->select();
}

bool LogViewDialog::readFields(LogViewOptions &out)
{
    char title[129] {}, command[513] {}, include[513] {}, exclude[513] {}, maxLines[9] {}, restart[7] {};
    ushort checks = 0;
    titleIn->getData(title);
    cmdIn->getData(command);
    incIn->getData(include);
    excIn->getData(exclude);
    checkBoxes->getData(&checks);
    maxIn->getData(maxLines);
    restartIn->getData(restart);

    std::string error;
    std::string cmd = trim(command);
    char *end;
    long nLines = strtol(maxLines, &end, 10);
    bool linesOk = !trim(maxLines).empty() && trim(end).empty() &&
                   (nLines == 0 || (nLines >= 100 && nLines <= 1000000));
    long nRestart = strtol(restart, &end, 10);
    bool restartOk = !trim(restart).empty() && trim(end).empty() &&
                     nRestart >= 0 && nRestart <= 86400;
    if (cmd.empty())
    {
        fail(cmdIn, "Enter the command whose output is shown\n(e.g. tail -F /var/log/maillog).");
        return false;
    }
    if (*include && !LogRegex::compile(include, checks & 1, false, error))
    {
        fail(incIn, "Invalid include expression:\n" + error);
        return false;
    }
    if (*exclude && !LogRegex::compile(exclude, checks & 1, false, error))
    {
        fail(excIn, "Invalid exclude expression:\n" + error);
        return false;
    }
    if (!linesOk)
    {
        fail(maxIn, std::string("Max lines: 0 (default) or 100 to 1000000, not '") + maxLines + "'.");
        return false;
    }
    if (!restartOk)
    {
        fail(restartIn, std::string("Restart after: 0 (never) or 1 to 86400 seconds, not '") + restart + "'.");
        return false;
    }
    out.title = trim(title);
    out.command = cmd;
    out.include = include;
    out.exclude = exclude;
    out.ignoreCase = checks & 1;
    out.wrap = checks & 2;
    out.filters = checks & 4;
    out.maxLines = (int) nLines;
    out.autoRestart = (int) nRestart;
    return true;
}

Boolean LogViewDialog::valid(ushort command)
{
    if (command == cmOK)
    {
        LogViewOptions n;
        if (!readFields(n))
            return False;
        o = n;
        historyAdd(hiLogCommand, o.command);
    }
    return TDialog::valid(command);
}

void LogViewDialog::handleEvent(TEvent &ev)
{
    TDialog::handleEvent(ev);
    if (ev.what == evCommand && ev.message.command == cmLogApply)
    {
        clearEvent(ev);
        LogViewOptions n;
        if (!apply || !readFields(n))
            return;
        std::string error;
        if (!apply(n, error))
        {
            messageBox(mfError | mfOKButton, "%s", error.c_str());
            return;
        }
        o = n;
        historyAdd(hiLogCommand, o.command);
        showStatus("Settings applied.");
    }
}

} // namespace logview

bool execLogViewDialog(LogViewOptions &o, bool isNew, int focusField, LogViewApply apply)
{
    using namespace logview;
    for (const auto &cmd : LogSource::commands())
        historyAdd(hiLogCommand, cmd);
    auto *d = new LogViewDialog(o, isNew, focusField, std::move(apply));
    TView *p = TProgram::application->validView(d);
    if (!p)
        return false;
    ushort result = TProgram::deskTop->execView(p);
    TObject::destroy(p);
    // With OK, 'o' has the new options (see LogViewDialog::valid).
    return result == cmOK;
}

// The window.

LogViewWindow::LogViewWindow(const TRect &bounds, const LogViewOptions &aOptions) noexcept :
    TWindowInit(&initFrame),
    TWindow(bounds, nullptr, wnNoNumber)
{
    using namespace logview;
    eventMask |= evBroadcast;
    // Arranged by Tile and Cascade, like terminals.
    TView::options |= ofTileable;
    helpCtx = hcLogView;
    vScrollBar = standardScrollBar(sbVertical);
    hScrollBar = standardScrollBar(sbHorizontal);
    interior = new LogViewInterior(getExtent().grow(-1, -1), *this);
    insert(interior);
    silenceSince = Clock::now();
    std::string error;
    if (!setOptions(aOptions, error))
    {
        // Invalid expressions (e.g. from a profile): without them.
        LogViewOptions o = aOptions;
        o.include.clear();
        o.exclude.clear();
        setOptions(o, error);
    }
}

LogViewWindow::~LogViewWindow()
{
    using namespace logview;
    detachSource();
}

void LogViewWindow::shutDown()
{
    using namespace logview;
    detachSource();
    interior = nullptr;
    vScrollBar = hScrollBar = nullptr;
    TWindow::shutDown();
}

bool LogViewWindow::setOptions(const LogViewOptions &newOptions, std::string &error)
{
    using namespace logview;
    std::unique_ptr<LogRegex> inc, exc;
    if (!newOptions.include.empty())
        if (!(inc = LogRegex::compile(newOptions.include, newOptions.ignoreCase, false, error)))
        {
            error = "include: " + error;
            return false;
        }
    if (!newOptions.exclude.empty())
        if (!(exc = LogRegex::compile(newOptions.exclude, newOptions.ignoreCase, false, error)))
        {
            error = "exclude: " + error;
            return false;
        }
    bool newSource = !source || newOptions.command != options.command;
    options = newOptions;
    includeRe = std::move(inc);
    excludeRe = std::move(exc);
    if (hScrollBar)
    {
        if (options.wrap)
            hScrollBar->hide();
        else
            hScrollBar->show();
    }
    if (newSource)
    {
        detachSource();
        attachSource();
    }
    else
    {
        source->setUserOptions(this, effectiveMaxLines(options), options.autoRestart);
        refilter();
    }
    return true;
}

void LogViewWindow::attachSource()
{
    using namespace logview;
    source = LogSource::acquire(options.command);
    source->setUserOptions(this, effectiveMaxLines(options), options.autoRestart);
    sourceStart = source->getStartTime();
    clearedSeq = 0;
    hasSel = hasMatch = false;
    follow = true;
    paused = false;
    // The lines the source already has raise no alerts.
    refilter();
}

void LogViewWindow::detachSource()
{
    using namespace logview;
    if (source)
    {
        source->removeUser(this);
        source.reset();
    }
    shown.clear();
    top = 0;
    topRow = 0;
}

void LogViewWindow::setCustomTitle(const std::string &title)
{
    using namespace logview;
    options.title = title;
    updateTitle();
}

bool LogViewWindow::passes(const LogLine &line) const
{
    using namespace logview;
    if (line.marker || !options.filters)
        return true;
    if (includeRe && !includeRe->matches(line.text))
        return false;
    if (excludeRe && excludeRe->matches(line.text))
        return false;
    return true;
}

void LogViewWindow::refilter()
{
    using namespace logview;
    if (!source)
        return;
    uint64_t oldTopSeq = top < shown.size() ? shown[top] : 0;
    shown.clear();
    uint64_t from = std::max(source->firstSeq(), clearedSeq);
    uint64_t end = source->endSeq();
    for (uint64_t seq = from; seq < end; ++seq)
        if (passes(source->line(seq)))
            shown.push_back(seq);
    nextSeq = end;
    if (paused)
        newAfterSeq = std::min(newAfterSeq, end);
    else if (follow)
        newAfterSeq = end > 0 ? end - 1 : 0;
    leftCol = options.wrap ? 0 : leftCol;
    if (follow)
    {
        scrollToEnd();
        if (!shown.empty())
            cursorSeq = shown.back();
    }
    else
    {
        top = std::lower_bound(shown.begin(), shown.end(), oldTopSeq) - shown.begin();
        topRow = 0;
        size_t endIndex;
        int endRow;
        endPosition(endIndex, endRow);
        if (top > endIndex || (top == endIndex && topRow > endRow))
            top = endIndex, topRow = endRow;
        // The cursor keeps its line, if shown (else the one before it).
        if (!shown.empty())
            ensureVisible(cursorIndex());
    }
    redraw();
}

void LogViewWindow::dropOldLines()
{
    using namespace logview;
    uint64_t first = source->firstSeq();
    size_t dropped = 0;
    while (!shown.empty() && shown.front() < first)
    {
        shown.pop_front();
        ++dropped;
    }
    if (dropped)
    {
        if (top >= dropped)
            top -= dropped;
        else
            top = 0, topRow = 0;
    }
    if (hasSel && selAnchor < first && selCursor < first)
        hasSel = false;
    else if (hasSel)
    {
        selAnchor = std::max(selAnchor, first);
        selCursor = std::max(selCursor, first);
    }
    if (hasMatch && matchSeq < first)
        hasMatch = false;
}

void LogViewWindow::update()
{
    using namespace logview;
    if (!source)
        return;
    if (options.maxLines == 0)
        // The default may change (tvterm.ini reloaded).
        source->setUserOptions(this, effectiveMaxLines(options), options.autoRestart);
    if (source->getStartTime() != sourceStart)
        sourceStart = source->getStartTime();
    dropOldLines();
    uint64_t end = source->endSeq();
    nextSeq = std::max({nextSeq, source->firstSeq(), clearedSeq});
    size_t before = shown.size();
    for (uint64_t seq = nextSeq; seq < end; ++seq)
        if (passes(source->line(seq)))
            shown.push_back(seq);
    nextSeq = end;
    checkAlerts(shown, before);
    if (shown.size() != before)
    {
        if (follow)
        {
            scrollToEnd();
            cursorSeq = shown.back();
        }
    }
    // While following, every line is seen: the next new ones (title +N)
    // are those after it.
    if (follow)
        newAfterSeq = nextSeq > 0 ? nextSeq - 1 : 0;
    if (shown.size() != before)
        redraw();
    else
        updateTitle();
}

void LogViewWindow::checkAlerts(const std::deque<uint64_t> &lines, size_t from)
{
    using namespace logview;
    auto now = Clock::now();
    bool newLines = false;
    for (size_t i = from; i < lines.size(); ++i)
        if (!source->line(lines[i]).marker)
        {
            newLines = true;
            break;
        }
    if (newLines)
    {
        silenceArmed = true;
        silenceSince = now;
    }
    // The first lines (e.g. 'tail -n 5000') are not news.
    bool starting = now - sourceStart < std::chrono::seconds(3);
    if (!alertSettings.enabled() || (state & sfSelected))
        return;
    if (newLines && !starting)
    {
        if (!alertSettings.keywords.empty() && alert != Alert::Keyword)
            for (size_t i = from; i < lines.size() && alert != Alert::Keyword; ++i)
            {
                const LogLine &line = source->line(lines[i]);
                if (line.marker)
                    continue;
                for (const auto &keyword : alertSettings.keywords)
                {
                    int b, e;
                    if (findPlain(line.text, keyword, false, 0, b, e))
                    {
                        raiseAlert(Alert::Keyword, keyword);
                        break;
                    }
                }
            }
        if (alertSettings.activity)
            raiseAlert(Alert::Activity);
    }
    if ( alertSettings.silenceSeconds > 0 && silenceArmed && !newLines &&
         now - silenceSince >= std::chrono::seconds(alertSettings.silenceSeconds) )
    {
        silenceArmed = false;
        raiseAlert(Alert::Silence);
    }
}

void LogViewWindow::setAlertSettings(AlertSettings settings) noexcept
{
    using namespace logview;
    alertSettings = std::move(settings);
    silenceArmed = true;
    silenceSince = Clock::now();
    clearAlert();
}

void LogViewWindow::raiseAlert(Alert kind, const std::string &keyword) noexcept
{
    using namespace logview;
    // Keyword > silence > activity.
    if (kind <= alert)
        return;
    if (alert == Alert::None)
        alertOrder = newAlertOrder();
    alert = kind;
    alertKeyword = keyword;
    updateTitle();
}

void LogViewWindow::clearAlert() noexcept
{
    using namespace logview;
    if (alert != Alert::None)
    {
        alert = Alert::None;
        alertKeyword.clear();
        updateTitle();
    }
}

void LogViewWindow::setState(ushort aState, Boolean enable)
{
    using namespace logview;
    bool wasSelected = state & sfSelected;
    TWindow::setState(aState, enable);
    if (aState & sfSelected)
    {
        if (enable)
            clearAlert();
        else if (wasSelected)
            silenceSince = Clock::now();
    }
    // The cursor line is shown only in the focused window.
    if ((aState & (sfFocused | sfSelected | sfActive)) && interior)
        interior->drawView();
}

void LogViewWindow::sizeLimits(TPoint &min, TPoint &max)
{
    using namespace logview;
    TWindow::sizeLimits(min, max);
}

// Title.

size_t LogViewWindow::newLineCount() const
{
    using namespace logview;
    return shown.end() - std::upper_bound(shown.begin(), shown.end(), newAfterSeq);
}

const char *LogViewWindow::getTitle(short)
{
    using namespace logview;
    std::string text;
    switch (alert)
    {
        case Alert::Activity: text = "[!] "; break;
        case Alert::Silence: text = "[!silent] "; break;
        case Alert::Keyword: text = "[!" + alertKeyword + "] "; break;
        default: break;
    }
    text += "Log: ";
    text += options.title.empty() ? options.command : options.title;
    bool anyFilter = !options.include.empty() || !options.exclude.empty();
    if (anyFilter && !options.filters)
        text += " [F-]";
    else
    {
        if (!options.include.empty())
            text += " [I]";
        if (!options.exclude.empty())
            text += " [E]";
    }
    if (source)
    {
        text += " " + std::to_string(shown.size()) + "/" +
                std::to_string(source->endSeq() - source->firstSeq());
        if (!source->isRunning())
            text += " (ended)";
    }
    size_t n = (paused || !follow) ? newLineCount() : 0;
    if (paused)
        text += " [PAUSE +" + std::to_string(n) + "]";
    else if (n > 0)
        text += " [+" + std::to_string(n) + "]";
    titleBuf = std::move(text);
    return titleBuf.c_str();
}

void LogViewWindow::updateTitle()
{
    using namespace logview;
    std::string current = getTitle(0);
    if (current != lastTitle)
    {
        lastTitle = current;
        if (frame)
            frame->drawView();
    }
}

// Scrolling.

int LogViewWindow::rowsOf(size_t index) const
{
    using namespace logview;
    if (!options.wrap || !interior || !source || index >= shown.size())
        return 1;
    std::vector<size_t> starts;
    rowStarts(expandTabs(source->line(shown[index]).text), interior->size.x, starts);
    return (int) starts.size();
}

void LogViewWindow::endPosition(size_t &index, int &row) const
// The position of the first row on the screen when the last line is at
// the bottom.
{
    using namespace logview;
    index = 0;
    row = 0;
    int height = interior ? interior->size.y : 1;
    if (shown.empty() || height <= 0)
        return;
    int remaining = height;
    size_t i = shown.size() - 1;
    while (true)
    {
        int rows = rowsOf(i);
        if (rows >= remaining)
        {
            index = i;
            row = rows - remaining;
            return;
        }
        remaining -= rows;
        if (i == 0)
            return;
        --i;
    }
}

bool LogViewWindow::atEnd() const
{
    using namespace logview;
    size_t index;
    int row;
    endPosition(index, row);
    return top > index || (top == index && topRow >= row);
}

void LogViewWindow::scrollRows(int delta)
{
    using namespace logview;
    size_t endIndex;
    int endRow;
    endPosition(endIndex, endRow);
    while (delta > 0)
    {
        if (top > endIndex || (top == endIndex && topRow >= endRow))
            break;
        if (topRow + 1 < rowsOf(top))
            ++topRow;
        else
            ++top, topRow = 0;
        --delta;
    }
    while (delta < 0)
    {
        if (topRow > 0)
            --topRow;
        else if (top > 0)
            --top, topRow = rowsOf(top) - 1;
        else
            break;
        ++delta;
    }
    if (top > endIndex || (top == endIndex && topRow > endRow))
        top = endIndex, topRow = endRow;
}

void LogViewWindow::scrollToEnd()
{
    using namespace logview;
    endPosition(top, topRow);
}

void LogViewWindow::followFromView()
{
    using namespace logview;
    follow = !paused && atEnd();
    if (follow && !shown.empty())
        cursorSeq = shown.back();
}

void LogViewWindow::followFromCursor()
{
    using namespace logview;
    follow = !paused && cursorIsLast();
    if (follow)
    {
        if (!shown.empty())
            cursorSeq = shown.back();
        scrollToEnd();
    }
}

size_t LogViewWindow::cursorIndex() const
// The cursor line or, if it is not shown, the nearest one before it.
{
    using namespace logview;
    if (shown.empty())
        return 0;
    size_t i = std::upper_bound(shown.begin(), shown.end(), cursorSeq) - shown.begin();
    return i == 0 ? 0 : i - 1;
}

bool LogViewWindow::cursorIsLast() const
{
    using namespace logview;
    return shown.empty() || cursorIndex() == shown.size() - 1;
}

bool LogViewWindow::isCursor(uint64_t seq) const
{
    using namespace logview;
    return !shown.empty() && shown[cursorIndex()] == seq;
}

void LogViewWindow::ensureVisible(size_t index)
// Scrolls as little as possible so that line 'index' is on the screen.
{
    using namespace logview;
    if (!interior || index >= shown.size())
        return;
    if (index < top || (index == top && topRow > 0))
    {
        top = index;
        topRow = 0;
        return;
    }
    size_t lastIndex;
    int lastRow;
    while ( interior->lineAt(interior->size.y - 1, lastIndex, lastRow) &&
            (index > lastIndex || (index == lastIndex && lastRow + 1 < rowsOf(lastIndex))) )
    {
        size_t oldTop = top;
        int oldRow = topRow;
        scrollRows(1);
        if (top == oldTop && topRow == oldRow)
            break;
    }
}

void LogViewWindow::moveCursorTo(long index, bool extend)
// Up/Down, PgUp/PgDn, Home/End; with Shift ('extend'), the selection
// grows from the cursor line.
{
    using namespace logview;
    if (shown.empty() || !interior)
        return;
    index = std::max(0L, std::min(index, (long) shown.size() - 1));
    if (extend)
    {
        if (!hasSel)
        {
            hasSel = true;
            selAnchor = shown[cursorIndex()];
        }
        selCursor = shown[index];
    }
    else
        hasSel = false;
    cursorSeq = shown[index];
    ensureVisible((size_t) index);
    followFromCursor();
    redraw();
}

void LogViewWindow::updateScrollBars()
{
    using namespace logview;
    if (vScrollBar && interior)
    {
        int count = (int) std::min<size_t>(shown.size(), INT_MAX);
        vScrollBar->setParams( (int) std::min<size_t>(top, INT_MAX), 0,
                               std::max(0, count - 1), std::max(1, interior->size.y - 1), 1 );
    }
    if (hScrollBar && interior && !options.wrap)
    {
        // Widest line on the screen.
        int width = 0;
        for (int y = 0; y < interior->size.y; ++y)
        {
            size_t index = top + y;
            if (index >= shown.size())
                break;
            width = std::max(width, (int) TText::width(expandTabs(source->line(shown[index]).text)));
        }
        int maxCol = std::max(leftCol, width - interior->size.x + 1);
        hScrollBar->setParams(leftCol, 0, std::max(0, maxCol), std::max(1, interior->size.x - 1), 8);
    }
}

void LogViewWindow::redraw()
{
    using namespace logview;
    updateScrollBars();
    if (interior)
        interior->drawView();
    updateTitle();
}

// Selection.

bool LogViewWindow::isSelected(uint64_t seq) const
{
    using namespace logview;
    if (!hasSel)
        return false;
    uint64_t a, b;
    selectionBounds(a, b);
    return a <= seq && seq <= b;
}

void LogViewWindow::selectionBounds(uint64_t &a, uint64_t &b) const
{
    using namespace logview;
    a = std::min(selAnchor, selCursor);
    b = std::max(selAnchor, selCursor);
}

std::string LogViewWindow::selectedText() const
{
    using namespace logview;
    std::string out;
    if (!hasSel || !source)
        return out;
    uint64_t a, b;
    selectionBounds(a, b);
    auto it = std::lower_bound(shown.begin(), shown.end(), a);
    for (; it != shown.end() && *it <= b; ++it)
    {
        out += source->line(*it).text;
        out += '\n';
    }
    return out;
}

std::string LogViewWindow::selectedOrCursorText() const
{
    using namespace logview;
    if (hasSel)
        return selectedText();
    if (!source || shown.empty())
        return {};
    return source->line(shown[cursorIndex()]).text + '\n';
}

std::string LogViewWindow::shownText() const
{
    using namespace logview;
    std::string out;
    if (!source)
        return out;
    for (uint64_t seq : shown)
    {
        out += source->line(seq).text;
        out += '\n';
    }
    return out;
}

// Commands.

void LogViewWindow::openMenu(TPoint where)
{
    using namespace logview;
    TMenuItem &items =
        *new TMenuItem("~S~ettings...", cmLogSettings, kbNoKey, hcNoContext, "F4") +
        *new TMenuItem("~I~nclude filter...", cmLogInclude, kbNoKey, hcNoContext, "I") +
        *new TMenuItem("~E~xclude filter...", cmLogExclude, kbNoKey, hcNoContext, "E") +
        *new TMenuItem("~F~ilters on/off", cmLogFilters, kbNoKey, hcNoContext, "F") +
        newLine() +
        *new TMenuItem("~D~uplicate window", cmLogDuplicate, kbNoKey, hcNoContext, "D") +
        *new TMenuItem("~R~estart source", cmLogRestart, kbNoKey, hcNoContext, "R") +
        newLine() +
        *new TMenuItem("~P~ause/continue", cmLogPause, kbNoKey, hcNoContext, "Space") +
        *new TMenuItem("~W~rap long lines on/off", cmLogWrap, kbNoKey, hcNoContext, "W") +
        *new TMenuItem("Clear window (~X~)", cmLogClear, kbNoKey, hcNoContext, "X") +
        newLine() +
        *new TMenuItem("Searc~h~...", cmLogSearch, kbNoKey, hcNoContext, "/") +
        *new TMenuItem("Find ~o~lder", cmLogFindOlder, kbNoKey, hcNoContext, "F3 n") +
        *new TMenuItem("Find ~n~ewer", cmLogFindNewer, kbNoKey, hcNoContext, "Shift-F3 N") +
        newLine() +
        *new TMenuItem("~C~opy (selection or line)", cmLogCopy, kbNoKey, hcNoContext, "Ctrl-C") +
        *new TMenuItem("To new edi~t~or", cmLogToNewEditor, kbNoKey, hcNoContext, "Ctrl-E") +
        *new TMenuItem("To open ed~i~tor...", cmLogToEditor, kbNoKey, hcNoContext, "Ctrl-A") +
        *new TMenuItem("Sa~v~e...", cmLogSave, kbNoKey, hcNoContext, "F2 s") +
        newLine() +
        *new TMenuItem("~A~lerts...", cmLogAlerts, kbNoKey, hcNoContext, "Ctrl-B K");
    ushort cmd = popupMenu(where, items, nullptr);
    if (cmd)
        runCommand(cmd);
}

void LogViewWindow::runCommand(ushort cmd)
{
    using namespace logview;
    switch (cmd)
    {
        case cmLogSettings: editOptions(0); break;
        case cmLogInclude: editOptions(1); break;
        case cmLogExclude: editOptions(2); break;
        case cmLogFilters: toggleFilters(); break;
        case cmLogDuplicate: duplicate(); break;
        case cmLogRestart:
            if (source)
            {
                source->restart();
                update();
            }
            break;
        case cmLogPause: togglePause(); break;
        case cmLogWrap: toggleWrap(); break;
        case cmLogClear: clearWindow(); break;
        case cmLogSearch: openSearch(); break;
        case cmLogFindOlder: findNext(true); break;
        case cmLogFindNewer: findNext(false); break;
        case cmLogCopy: copySelection(); break;
        case cmLogToNewEditor: sendToEditor(true); break;
        case cmLogToEditor: sendToEditor(false); break;
        case cmLogSave: save(); break;
        case cmLogAlerts:
            message(TProgram::application, evCommand, cmAlertSettings, nullptr);
            break;
    }
}

void LogViewWindow::editOptions(int focusField)
{
    using namespace logview;
    LogViewOptions o = options;
    auto apply = [this] (const LogViewOptions &n, std::string &error) {
        return setOptions(n, error);
    };
    // Cancel after Apply: what was applied stays.
    if (!execLogViewDialog(o, false, focusField, apply))
        return;
    std::string error;
    if (!setOptions(o, error))
        messageBox(mfError | mfOKButton, "%s", error.c_str());
}

void LogViewWindow::duplicate()
{
    using namespace logview;
    LogViewOptions o = options;
    o.title = (options.title.empty() ? options.command : options.title) + " (2)";
    TRect r = getBounds();
    r.move(2, 1);
    if (owner)
    {
        TRect ext = owner->getExtent();
        if (r.b.x > ext.b.x)
            r.move(ext.b.x - r.b.x, 0);
        if (r.b.y > ext.b.y)
            r.move(0, ext.b.y - r.b.y);
        if (r.a.x < ext.a.x || r.a.y < ext.a.y)
            r = getBounds();
    }
    // The options include the filters on/off state (F).
    auto *copy = new LogViewWindow(r, o);
    TProgram::application->insertWindow(copy);
}

void LogViewWindow::togglePause()
{
    using namespace logview;
    if (paused)
    {
        paused = false;
        follow = true;
        scrollToEnd();
        if (!shown.empty())
            cursorSeq = shown.back();
    }
    else
    {
        paused = true;
        follow = false;
        newAfterSeq = shown.empty() ? 0 : shown.back();
    }
    redraw();
}

void LogViewWindow::toggleFilters()
{
    using namespace logview;
    if (options.include.empty() && options.exclude.empty())
    {
        showStatus("No filters: set them with I (include), E (exclude) or F4.");
        return;
    }
    options.filters = !options.filters;
    refilter();
    showStatus(options.filters ? "Filters on." : "Filters off: all the lines are shown.");
}

void LogViewWindow::toggleWrap()
{
    using namespace logview;
    options.wrap = !options.wrap;
    leftCol = 0;
    topRow = 0;
    if (hScrollBar)
    {
        if (options.wrap)
            hScrollBar->hide();
        else
            hScrollBar->show();
    }
    if (follow)
        scrollToEnd();
    redraw();
}

void LogViewWindow::clearWindow()
{
    using namespace logview;
    if (!source)
        return;
    clearedSeq = source->endSeq();
    cursorSeq = clearedSeq;
    shown.clear();
    top = 0;
    topRow = 0;
    hasSel = hasMatch = false;
    newAfterSeq = clearedSeq;
    redraw();
}

void LogViewWindow::openSearch()
{
    using namespace logview;
    struct
    {
        char text[256];
        ushort options; // 1: case sensitive, 2: regular expression.
    } data {};
    snprintf(data.text, sizeof(data.text), "%s", searchText.c_str());
    data.options = (searchCase ? 1 : 0) | (searchRegex ? 2 : 0);
    while (true)
    {
        TDialog *d = new TDialog(TRect(0, 0, 62, 13), "Search");
        d->options |= ofCentered;
        TInputLine *input = new TInputLine(TRect(3, 3, 59, 4), sizeof(data.text));
        d->insert(input);
        d->insert(new TLabel(TRect(2, 2, 20, 3), "~T~ext to find", input));
        d->insert(new TCheckBoxes(TRect(3, 5, 59, 7),
            new TSItem("~C~ase sensitive",
            new TSItem("~R~egular expression (like grep -E)", nullptr))));
        d->insert(new TStaticText(TRect(3, 7, 59, 9),
            "From the lines on the screen towards older ones.\n"
            "Next: F3 or n (older), Shift-F3 or N (newer)."));
        d->insert(new TButton(TRect(16, 10, 28, 12), "~F~ind", cmOK, bfDefault));
        d->insert(new TButton(TRect(32, 10, 44, 12), "Cancel", cmCancel, bfNormal));
        d->selectNext(False);
        d->setData(&data);
        TView *p = TProgram::application->validView(d);
        if (!p)
            return;
        ushort result = TProgram::deskTop->execView(p);
        if (result != cmCancel)
            p->getData(&data);
        TObject::destroy(p);
        if (result == cmCancel)
            return;
        std::string error;
        std::unique_ptr<LogRegex> re;
        if ((data.options & 2) && data.text[0])
            if (!(re = LogRegex::compile(data.text, !(data.options & 1), true, error)))
            {
                messageBox(mfError | mfOKButton, "Invalid regular expression:\n%s", error.c_str());
                continue;
            }
        searchText = data.text;
        searchCase = data.options & 1;
        searchRegex = data.options & 2;
        searchRe = std::move(re);
        hasMatch = false;
        break;
    }
    if (!searchText.empty())
        findNext(true);
    else
        redraw();
}

void LogViewWindow::findNext(bool older)
{
    using namespace logview;
    if (searchText.empty() || !source)
    {
        openSearch();
        return;
    }
    if (shown.empty())
        return;
    // Starting line: after the current match, or the lines on the screen.
    long index;
    if (hasMatch && std::binary_search(shown.begin(), shown.end(), matchSeq))
        index = (long) (std::lower_bound(shown.begin(), shown.end(), matchSeq) - shown.begin()) + (older ? -1 : 1);
    else if (older)
    {
        size_t i;
        int row;
        int y = interior ? interior->size.y - 1 : 0;
        while (y > 0 && !(interior && interior->lineAt(y, i, row)))
            --y;
        index = interior && interior->lineAt(y, i, row) ? (long) i : (long) shown.size() - 1;
    }
    else
        index = (long) top;
    for (; index >= 0 && index < (long) shown.size(); index += older ? -1 : 1)
    {
        const LogLine &line = source->line(shown[index]);
        int b, e;
        bool found = searchRe ? searchRe->find(line.text, 0, b, e)
                              : findPlain(line.text, searchText, searchCase, 0, b, e);
        if (found)
        {
            hasMatch = true;
            matchSeq = shown[index];
            matchBegin = b;
            matchEnd = std::max(e, b + 1);
            // Show it: a few lines below the top of the screen.
            size_t endIndex;
            int endRow;
            endPosition(endIndex, endRow);
            size_t i;
            int row;
            bool visible = false;
            for (int y = 0; interior && y < interior->size.y; ++y)
                if (interior->lineAt(y, i, row) && i == (size_t) index)
                {
                    visible = true;
                    break;
                }
            if (!visible)
            {
                int above = interior ? interior->size.y / 3 : 0;
                top = (size_t) std::max(0L, index - above);
                topRow = 0;
                if (top > endIndex || (top == endIndex && topRow > endRow))
                    top = endIndex, topRow = endRow;
            }
            cursorSeq = matchSeq;
            follow = !paused && cursorIsLast() && atEnd();
            if (!options.wrap && interior)
            {
                int col = columnOf(line.text, b);
                int colEnd = columnOf(line.text, matchEnd);
                if (col < leftCol || colEnd > leftCol + interior->size.x)
                    leftCol = std::max(0, col - interior->size.x / 3);
            }
            redraw();
            return;
        }
    }
    showStatus( std::string(older ? "No older match for \"" : "No newer match for \"") +
                searchText + "\"." );
}

void LogViewWindow::copySelection()
{
    using namespace logview;
    std::string text = selectedOrCursorText();
    if (text.empty())
    {
        showStatus("Nothing to copy: the window is empty.");
        return;
    }
    size_t lines = std::count(text.begin(), text.end(), '\n');
    // Without the last line break (pasting into a shell would run it).
    text.pop_back();
    TClipboard::setText(text);
    showStatus("Copied " + std::to_string(lines) + " line(s) to the clipboard.");
}

namespace logview
{

class EditorListViewer : public TListViewer
{
public:

    std::vector<std::string> items;

    EditorListViewer(const TRect &bounds, TScrollBar *sb) noexcept :
        TListViewer(bounds, 1, nullptr, sb)
    {
    }

    void getText(char *dest, short item, short maxChars) override
    {
        if (item >= 0 && (size_t) item < items.size())
        {
            strncpy(dest, items[item].c_str(), maxChars);
            dest[maxChars] = '\0';
        }
        else
            *dest = '\0';
    }
};

class EditorChoiceDialog : public TDialog
{
public:

    EditorListViewer *list;

    EditorChoiceDialog(const TRect &bounds) noexcept :
        TWindowInit(&initFrame),
        TDialog(bounds, "Add to editor")
    {
        options |= ofCentered;
        TRect r(2, 2, size.x - 3, size.y - 4);
        auto *sb = new TScrollBar(TRect(r.b.x, r.a.y, r.b.x + 1, r.b.y));
        insert(sb);
        list = new EditorListViewer(r, sb);
        insert(list);
        insert(new TLabel(TRect(r.a.x, r.a.y - 1, r.b.x, r.a.y), "~E~ditors (lines added at the end)", list));
        int x = (size.x - 26) / 2, y = size.y - 3;
        insert(new TButton(TRect(x, y, x + 12, y + 2), "~A~dd", cmOK, bfDefault));
        insert(new TButton(TRect(x + 14, y, x + 26, y + 2), "Cancel", cmCancel, bfNormal));
        list->select();
    }

    void handleEvent(TEvent &ev) override
    {
        TDialog::handleEvent(ev);
        if (ev.what == evBroadcast && ev.message.command == cmListItemSelected)
        {
            endModal(cmOK);
            clearEvent(ev);
        }
    }
};

static void appendToEditor(TEditor &editor, const std::string &text)
{
    const char *eol = editor.lineEndingType == TEditor::eolCrLf ? "\r\n" :
                      editor.lineEndingType == TEditor::eolCr ? "\r" : "\n";
    std::string out;
    if (editor.bufLen > 0 && editor.bufChar(editor.bufLen - 1) != '\n' && editor.bufChar(editor.bufLen - 1) != '\r')
        out += eol;
    for (char c : text)
        if (c == '\n')
            out += eol;
        else
            out += c;
    editor.setCurPtr(editor.bufLen, 0);
    editor.insertText(out.data(), (uint) out.size(), False);
    editor.trackCursor(True);
    editor.drawView();
}

} // namespace logview

void LogViewWindow::sendToEditor(bool newEditor)
{
    using namespace logview;
    std::string text = selectedOrCursorText();
    if (text.empty())
    {
        showStatus("Nothing to send: the window is empty.");
        return;
    }
    size_t lines = std::count(text.begin(), text.end(), '\n');
    TEditWindow *target = nullptr;
    if (newEditor)
    {
        TRect r = TProgram::deskTop->getExtent();
        if (r.b.x - r.a.x > 100)
            r.a.x = r.b.x - 82;
        auto *w = new TVTermEditWindow(r, "");
        if (!TProgram::application->insertWindow(w))
            return;
        target = w;
    }
    else
    {
        std::vector<TEditWindow *> editors;
        std::vector<std::string> names;
        for (TWindow *w : getDeskTopWindows(*TProgram::deskTop))
            if (auto *e = dynamic_cast<TEditWindow *>(w))
            {
                editors.push_back(e);
                const char *t = e->getTitle(100);
                names.push_back(t && *t ? t : "Untitled");
            }
        if (editors.empty())
        {
            showStatus("No editor is open: Ctrl-E sends the lines to a new one.");
            return;
        }
        TRect r(0, 0, 60, std::min<int>(10 + (int) editors.size(), TProgram::deskTop->size.y));
        auto *d = new EditorChoiceDialog(r);
        d->list->items = names;
        d->list->setRange((short) names.size());
        d->list->focusItem(0);
        TView *p = TProgram::application->validView(d);
        if (!p)
            return;
        ushort result = TProgram::deskTop->execView(p);
        short focused = d->list->focused;
        TObject::destroy(p);
        if (result != cmOK || focused < 0 || (size_t) focused >= editors.size())
            return;
        target = editors[focused];
    }
    if (target && target->editor)
    {
        appendToEditor(*target->editor, text);
        const char *t = target->getTitle(100);
        showStatus( "Added " + std::to_string(lines) + " line(s) to " +
                    (t && *t ? std::string(t) : std::string("the editor")) + "." );
    }
}

void LogViewWindow::save()
{
    using namespace logview;
    std::string name = options.title.empty() ? "log" : options.title;
    openTextSave( "Save Log View", name, hasSel,
                  [this] (bool selection) { return selection ? selectedText() : shownText(); } );
}

void LogViewWindow::handleEvent(TEvent &ev)
{
    using namespace logview;
    if (ev.what == evBroadcast)
    {
        if (ev.message.command == cmCheckTerminalUpdates)
            update();
        else if (ev.message.command == cmScrollBarChanged)
        {
            if (ev.message.infoPtr == vScrollBar && vScrollBar)
            {
                size_t value = (size_t) std::max(0, vScrollBar->value);
                if (value != top)
                {
                    top = std::min(value, shown.empty() ? 0 : shown.size() - 1);
                    topRow = 0;
                    size_t endIndex;
                    int endRow;
                    endPosition(endIndex, endRow);
                    if (top > endIndex || (top == endIndex && topRow > endRow))
                        top = endIndex, topRow = endRow;
                    followFromView();
                    redraw();
                }
                clearEvent(ev);
                return;
            }
            if (ev.message.infoPtr == hScrollBar && hScrollBar)
            {
                if (hScrollBar->value != leftCol)
                {
                    leftCol = std::max(0, hScrollBar->value);
                    redraw();
                }
                clearEvent(ev);
                return;
            }
        }
    }
    if (ev.what == evKeyDown && (state & sfFocused))
    {
        TKey key(ev.keyDown);
        ushort mods = ev.keyDown.controlKeyState;
        char ch = (mods & (kbCtrlShift | kbAltShift)) ? 0 : ev.keyDown.charScan.charCode;
        int page = interior ? std::max(1, interior->size.y - 1) : 1;
        bool handled = true;
        if (key == TKey(kbF4)) editOptions(0);
        else if (key == TKey(kbF9)) openMenu(interior ? interior->makeGlobal(TPoint {0, 0}) : origin);
        else if (key == TKey(kbF2)) save();
        else if (key == TKey(kbF3)) findNext(true);
        else if (key == TKey(kbShiftF3)) findNext(false);
        else if (key == TKey(kbCtrlC) || key == TKey(kbCtrlIns)) copySelection();
        else if (key == TKey(kbCtrlE)) sendToEditor(true);
        else if (key == TKey(kbCtrlA)) sendToEditor(false);
        else if (key == TKey(kbUp) || key == TKey(kbUp, kbShift))
            moveCursorTo((long) cursorIndex() - 1, mods & kbShift);
        else if (key == TKey(kbDown) || key == TKey(kbDown, kbShift))
            moveCursorTo((long) cursorIndex() + 1, mods & kbShift);
        else if (key == TKey(kbPgUp) || key == TKey(kbPgUp, kbShift))
            moveCursorTo((long) cursorIndex() - page, mods & kbShift);
        else if (key == TKey(kbPgDn) || key == TKey(kbPgDn, kbShift))
            moveCursorTo((long) cursorIndex() + page, mods & kbShift);
        else if ( key == TKey(kbHome) || key == TKey(kbCtrlHome) ||
                  key == TKey(kbHome, kbShift) || key == TKey(kbCtrlHome, kbShift) )
            moveCursorTo(0, mods & kbShift);
        else if ( key == TKey(kbEnd) || key == TKey(kbCtrlEnd) ||
                  key == TKey(kbEnd, kbShift) || key == TKey(kbCtrlEnd, kbShift) )
        {
            paused = false;
            moveCursorTo((long) shown.size() - 1, mods & kbShift);
            follow = true;
            scrollToEnd();
            redraw();
        }
        else if (key == TKey(kbLeft) && !options.wrap) { leftCol = std::max(0, leftCol - 8); redraw(); }
        else if (key == TKey(kbRight) && !options.wrap) { leftCol += 8; redraw(); }
        else if (key == TKey(kbEsc))
        {
            if (hasSel)
                hasSel = false;
            else
                hasMatch = false;
            redraw();
        }
        else if (ch == ' ') togglePause();
        else if (ch == '/') openSearch();
        else if (ch == 'n') findNext(true);
        else if (ch == 'N') findNext(false);
        else
        {
            switch (ch >= 'A' && ch <= 'Z' ? ch - 'A' + 'a' : ch)
            {
                case 'd': duplicate(); break;
                case 'r': runCommand(cmLogRestart); break;
                case 'i': editOptions(1); break;
                case 'e': editOptions(2); break;
                case 'f': toggleFilters(); break;
                case 'w': toggleWrap(); break;
                case 'x': clearWindow(); break;
                case 's': save(); break;
                default: handled = false; break;
            }
        }
        if (handled)
        {
            clearEvent(ev);
            return;
        }
    }
    TWindow::handleEvent(ev);
}

size_t countRunningLogViews(TDeskTop &deskTop)
{
    using namespace logview;
    size_t n = 0;
    for (TWindow *w : getDeskTopWindows(deskTop))
        if (auto *log = dynamic_cast<LogViewWindow *>(w))
            n += log->isSourceRunning();
    return n;
}
