#define Uses_TDialog
#define Uses_TCheckBoxes
#define Uses_TSItem
#define Uses_TInputLine
#define Uses_TLabel
#define Uses_TStaticText
#define Uses_TButton
#define Uses_TDeskTop
#define Uses_TProgram
#define Uses_TDrawBuffer
#define Uses_TText
#define Uses_TFrame
#define Uses_TWindow
#define Uses_MsgBox
#include <tvision/tv.h>

#include "alerts.h"
#include "app.h"
#include "desktops.h"
#include "globalkeys.h"
#include "wnd.h"
#include "winlist.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>
#include <algorithm>
#include <map>

extern char **environ;

namespace alertdlg
{

static std::string trim(const std::string &s)
{
    size_t a = s.find_first_not_of(" \t");
    if (a == std::string::npos)
        return {};
    size_t b = s.find_last_not_of(" \t");
    return s.substr(a, b - a + 1);
}

// A character of the Keywords text; 'esc': written as '\x' (literal).
struct KwChar
{
    char c;
    bool esc;
};
using KwText = std::vector<KwChar>;

static bool isBlank(const KwChar &k)
{
    return !k.esc && (k.c == ' ' || k.c == '\t');
}

static KwText kwTrim(const KwText &t)
{
    size_t a = 0, b = t.size();
    while (a < b && isBlank(t[a]))
        ++a;
    while (b > a && isBlank(t[b - 1]))
        --b;
    return KwText(t.begin() + a, t.begin() + b);
}

static std::vector<KwText> kwSplit(const KwText &t, char sep)
{
    std::vector<KwText> parts(1);
    for (const auto &k : t)
        if (!k.esc && k.c == sep)
            parts.emplace_back();
        else
            parts.back().push_back(k);
    return parts;
}

static bool kwHas(const KwText &t, char c)
{
    for (const auto &k : t)
        if (!k.esc && k.c == c)
            return true;
    return false;
}

static std::string kwString(const KwText &t)
{
    std::string s;
    for (const auto &k : t)
        s.push_back(k.c);
    return s;
}

// The text as typed, for the dialog and profiles: without the spaces at
// its ends, except an escaped one ('\ ').
static std::string trimKeywordsText(const std::string &text)
{
    size_t a = text.find_first_not_of(" \t");
    if (a == std::string::npos)
        return {};
    size_t b = text.size();
    while (b > a && (text[b - 1] == ' ' || text[b - 1] == '\t'))
    {
        size_t slashes = 0;
        while (b - 1 - slashes > a && text[b - 2 - slashes] == '\\')
            ++slashes;
        if (slashes % 2 == 1)
            break;
        --b;
    }
    return text.substr(a, b - a);
}

// UTF-8: whether 'c' continues a character.
static bool isContinuation(char c)
{
    return ((unsigned char) c & 0xC0) == 0x80;
}

// At most 'maxBytes' bytes of 's', cut at a character boundary, with '…'
// if shortened.
static std::string shorten(const std::string &s, size_t maxBytes)
{
    if (s.size() <= maxBytes)
        return s;
    size_t end = maxBytes;
    while (end > 0 && isContinuation(s[end]))
        --end;
    return s.substr(0, end) + "\u2026";
}

} // namespace alertdlg

std::string lowerAscii(std::string s)
{
    for (char &c : s)
        if (c >= 'A' && c <= 'Z')
            c += 'a' - 'A';
    return s;
}

bool parseKeywords(const std::string &text, KeywordRules &rules, std::string &error)
{
    using namespace alertdlg;
    rules = KeywordRules();
    error.clear();
    rules.text = trimKeywordsText(text);
    KwText all;
    for (size_t i = 0; i < text.size(); ++i)
        if (text[i] == '\\' && i + 1 < text.size())
            all.push_back({text[++i], true});
        else
            all.push_back({text[i], false});
    auto fail = [&] (const std::string &msg) {
        if (error.empty())
            error = msg;
    };
    for (const KwText &rawElem : kwSplit(all, ','))
    {
        KwText elem = kwTrim(rawElem);
        if (elem.empty())
            continue;
        std::string shown = kwString(elem);
        // "N:" at the start: the level of the group.
        int level = alertLevelMax;
        if ( elem.size() >= 2 && !elem[0].esc && elem[0].c >= '0' && elem[0].c <= '9' &&
             !elem[1].esc && elem[1].c == ':' )
        {
            level = elem[0].c - '0';
            if (level < alertLevelMin || level > alertLevelMax)
            {
                fail("'" + shown + "': the level before ':' must be 1-5 (write \\: for a plain ':').");
                continue;
            }
            elem = kwTrim(KwText(elem.begin() + 2, elem.end()));
            if (elem.empty())
            {
                fail("'" + shown + "': a level without keywords.");
                continue;
            }
            if (!elem[0].esc && elem[0].c == '!')
            {
                fail("'" + shown + "': an exclusion (!) has no level.");
                continue;
            }
        }
        if (!elem[0].esc && elem[0].c == '!')
        {
            KwText term = kwTrim(KwText(elem.begin() + 1, elem.end()));
            if (kwHas(term, '+'))
                fail("'" + shown + "': an exclusion (!) cannot be in a group with '+'; "
                     "write it as an element of its own, between commas.");
            else if (term.empty())
                fail("'!' without a text to exclude.");
            else
                rules.excludes.push_back(lowerAscii(kwString(term)));
            continue;
        }
        KeywordGroup group;
        group.level = level;
        bool ok = true;
        for (const KwText &rawTerm : kwSplit(elem, '+'))
        {
            KwText term = kwTrim(rawTerm);
            if (term.empty())
            {
                fail("'" + shown + "': empty term beside '+' (write \\+ for a plain '+').");
                ok = false;
                break;
            }
            if (!term[0].esc && term[0].c == '!')
            {
                fail("'" + shown + "': an exclusion (!) cannot be in a group with '+'; "
                     "write it as an element of its own, between commas.");
                ok = false;
                break;
            }
            std::string t = kwString(term);
            if (!group.terms.empty())
            {
                group.label += " + ";
                group.shortLabel += "+";
            }
            group.label += t;
            group.shortLabel += t;
            group.terms.push_back(lowerAscii(std::move(t)));
        }
        if (ok)
            rules.groups.push_back(std::move(group));
    }
    return error.empty();
}

KeywordRules parseKeywords(const std::string &text)
{
    KeywordRules rules;
    std::string error;
    parseKeywords(text, rules, error);
    return rules;
}

const KeywordGroup *KeywordRules::match(const std::string &lowerLine) const
{
    for (const auto &ex : excludes)
        if (lowerLine.find(ex) != std::string::npos)
            return nullptr;
    const KeywordGroup *found = nullptr;
    for (const auto &group : groups)
    {
        if (found && group.level >= found->level)
            continue;
        bool all = true;
        for (const auto &term : group.terms)
            if (lowerLine.find(term) == std::string::npos)
            {
                all = false;
                break;
            }
        if (all)
        {
            found = &group;
            if (found->level == alertLevelMin)
                break;
        }
    }
    if (found)
        return found;
    if (groups.empty() && !excludes.empty() &&
        lowerLine.find_first_not_of(" \t") != std::string::npos)
    {
        // Only exclusions: any other line.
        static const KeywordGroup anyLine {{}, alertLevelMax, "(any line without the exclusions)", "*"};
        return &anyLine;
    }
    return nullptr;
}

bool splitAlertLevel(const std::string &text, int &level, std::string &rest)
{
    level = alertLevelMax;
    size_t a = text.find_first_not_of(" \t");
    if (a != std::string::npos && a + 1 < text.size() &&
        text[a] >= '0' && text[a] <= '9')
    {
        size_t colon = text.find_first_not_of(" \t", a + 1);
        if (colon != std::string::npos && text[colon] == ':')
        {
            int n = text[a] - '0';
            if (n < alertLevelMin || n > alertLevelMax)
                return false;
            level = n;
            rest = alertdlg::trim(text.substr(colon + 1));
            return true;
        }
    }
    rest = text;
    return true;
}

std::string alertLevelPrefix(int level)
{
    if (level >= alertLevelMin && level < alertLevelMax)
        return std::to_string(level) + ":";
    return {};
}

std::string alertMark(int kind, const std::string &shortLabel)
{
    using namespace alertdlg;
    switch (kind)
    {
        case 1: return "[!] ";
        case 2: return "[!silent] ";
        case 3:
        {
            std::string label = shorten(shortLabel, 24);
            // Brackets in the keyword would make the mark ambiguous
            // ("[!] Ban]"): quoted.
            if (label.find_first_of("[]") != std::string::npos)
                label = "\"" + label + "\"";
            return "[!" + label + "] ";
        }
        default: return {};
    }
}

std::string formatEventTime(time_t t)
{
    struct tm tmT {}, tmNow {};
    time_t now = time(nullptr);
    if (!localtime_r(&t, &tmT) || !localtime_r(&now, &tmNow))
        return "??:??:??";
    bool today = tmT.tm_year == tmNow.tm_year && tmT.tm_yday == tmNow.tm_yday;
    char buf[32];
    strftime(buf, sizeof(buf), today ? "%H:%M:%S" : "%Y-%m-%d %H:%M:%S", &tmT);
    return buf;
}

std::string alertLineFragment(const std::string &rawLine, const KeywordGroup *group)
{
    using namespace alertdlg;
    enum { maxBytes = 150, before = 40 };
    std::string line = rawLine;
    for (char &c : line)
        if ((unsigned char) c < 0x20 || c == 0x7F)
            c = ' ';
    if (line.size() <= maxBytes)
        return line;
    size_t start = 0;
    if (group && !group->terms.empty())
    {
        size_t pos = lowerAscii(line).find(group->terms[0]);
        if (pos != std::string::npos && pos > before)
            start = pos - before;
    }
    while (start < line.size() && isContinuation(line[start]))
        ++start;
    std::string out = start > 0 ? "\u2026" : "";
    out += shorten(line.substr(start), maxBytes);
    return out;
}

namespace alertdlg
{

struct DialogData
{
    ushort activity;
    char activityLevel[3];
    char keywords[1024];
    char silence[10];
    char action[1024];
};

} // namespace alertdlg

uint64_t newAlertOrder() noexcept
{
    static uint64_t lastAlertOrder = 0;
    return ++lastAlertOrder;
}

bool alertTitleColor = true;
uint8_t alertSeenColor = alertSeenYellow;
AlertConfig alertConfig;

uint8_t alertLevelColor(int level) noexcept
{
    switch (level)
    {
        case 2: return 0x5F; // White on magenta.
        case 3: return 0x6F; // White on brown.
        case 4: return 0x3F; // White on cyan.
        default: return 0x4F; // 1 and 5: white on red.
    }
}

namespace alertrun
{

// UTF-8 cut at a character boundary, control characters as spaces.
static std::string cleanLine(const std::string &line, size_t maxBytes)
{
    std::string out = line;
    for (char &c : out)
        if ((unsigned char) c < 0x20 || c == 0x7F)
            c = ' ';
    while (!out.empty() && out.back() == ' ')
        out.pop_back();
    if (out.size() > maxBytes)
    {
        size_t end = maxBytes;
        while (end > 0 && ((unsigned char) out[end] & 0xC0) == 0x80)
            --end;
        out.resize(end);
    }
    return out;
}

static std::string expandHome(const std::string &path)
{
    if (path.size() >= 2 && path[0] == '~' && path[1] == '/')
        if (const char *home = getenv("HOME"))
            return std::string(home) + path.substr(1);
    return path;
}

// "D1", "2:mail", "*" for a window on every desktop: desktopColumn without
// the marks.
static std::string deskName(TWindow *w)
{
    std::string d = w ? desktopColumn(w) : std::string();
    size_t a = d.find_first_not_of(" *");
    d = a == std::string::npos ? std::string() : d.substr(a);
    while (!d.empty() && d.back() == ' ')
        d.pop_back();
    return d.empty() ? "-" : d;
}

static std::string timeText(time_t t)
{
    struct tm tmT {};
    char buf[32] = "0000-00-00 00:00:00";
    if (localtime_r(&t, &tmT))
        strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tmT);
    return buf;
}

static int logFd = -1;
static std::string logPath;     // Of 'logFd' (expanded).
static std::string logFailed;   // Path whose error was reported.

static void writeLog(const std::string &text) noexcept
{
    std::string path = expandHome(alertConfig.log);
    if (path != logPath)
    {
        if (logFd >= 0)
            close(logFd);
        logFd = -1;
        logPath = path;
    }
    if (path.empty())
        return;
    if (logFd < 0)
    {
        logFd = open(path.c_str(), O_WRONLY | O_APPEND | O_CREAT | O_CLOEXEC, 0600);
        if (logFd < 0)
        {
            if (logFailed != path)
            {
                logFailed = path;
                TVTermApp::note("[alerts] log " + path + ": " + strerror(errno) +
                                " (alerts are not written)");
            }
            return;
        }
        logFailed.clear();
    }
    if (write(logFd, text.data(), text.size()) != (ssize_t) text.size() && logFailed != path)
    {
        logFailed = path;
        TVTermApp::note("[alerts] log " + path + ": " + strerror(errno));
    }
}

// Last run of an action, by level, keyword and window.
static std::map<std::string, time_t> lastRun;

static void runAction(const std::string &cmd, const std::vector<std::string> &vars) noexcept
{
    // The environment made before fork (nothing allocated in the child).
    std::vector<std::string> env;
    for (char **e = environ; e && *e; ++e)
        if (strncmp(*e, "TVTERM_ALERT_", 13) != 0)
            env.emplace_back(*e);
    for (auto &v : vars)
        env.push_back(v);
    std::vector<char *> envp;
    for (auto &e : env)
        envp.push_back(&e[0]);
    envp.push_back(nullptr);
    const char *argv[] = {"sh", "-c", cmd.c_str(), nullptr};
    pid_t pid = fork();
    if (pid == 0)
    {
        // Detached: a grandchild, in its own session, without the
        // terminal; tvterm does not wait for it.
        setsid();
        int nul = open("/dev/null", O_RDWR);
        if (nul >= 0)
        {
            dup2(nul, 0);
            dup2(nul, 1);
            dup2(nul, 2);
        }
        long maxFd = sysconf(_SC_OPEN_MAX);
        if (maxFd < 0 || maxFd > 4096)
            maxFd = 4096;
        for (int fd = 3; fd < maxFd; ++fd)
            close(fd);
        if (fork() == 0)
        {
            sigset_t all;
            sigemptyset(&all);
            sigprocmask(SIG_SETMASK, &all, nullptr);
            for (int sig : {SIGPIPE, SIGINT, SIGQUIT, SIGTERM, SIGHUP, SIGCHLD, SIGTSTP, SIGTTIN, SIGTTOU})
                signal(sig, SIG_DFL);
            execve("/bin/sh", (char **) argv, envp.data());
            _exit(127);
        }
        _exit(0);
    }
    if (pid > 0)
        waitpid(pid, nullptr, 0);
    else
        TVTermApp::note(std::string("[alerts] action not run: fork: ") + strerror(errno));
}

} // namespace alertrun

void alertEvent(TWindow *w, const std::string &title, int level, const std::string &keyword,
                const std::string &line, const std::string &windowAction, bool raise) noexcept
{
    using namespace alertrun;
    time_t now = time(nullptr);
    std::string desk = deskName(w);
    std::string text = cleanLine(line, 500);
    if (!alertConfig.log.empty())
        writeLog(timeText(now) + " L" + std::to_string(level) + " " + desk + "/" + title +
                 " " + keyword + " | " + text + "\n");
    if (!raise || level != alertLevelMin)
        return;
    const std::string &cmd = !windowAction.empty() ? windowAction : alertConfig.action;
    if (cmd.empty())
        return;
    char ptr[32];
    snprintf(ptr, sizeof(ptr), "%p", (void *) w);
    std::string key = std::to_string(level) + '\x1f' + keyword + '\x1f' + ptr;
    auto it = lastRun.find(key);
    if (it != lastRun.end() && alertConfig.actionInterval > 0 &&
        now - it->second < alertConfig.actionInterval && now >= it->second)
        return;
    lastRun[key] = now;
    runAction(cmd, {
        "TVTERM_ALERT_LEVEL=" + std::to_string(level),
        "TVTERM_ALERT_TEXT=" + text,
        "TVTERM_ALERT_WINDOW=" + title,
        "TVTERM_ALERT_DESK=" + desk,
        "TVTERM_ALERT_KEYWORD=" + keyword,
        "TVTERM_ALERT_TIME=" + timeText(now),
    });
}

namespace alertrun
{

static std::string base64(const std::string &in)
{
    static const char tbl[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    size_t i = 0;
    for (; i + 2 < in.size(); i += 3)
    {
        unsigned v = ((unsigned char) in[i] << 16) | ((unsigned char) in[i + 1] << 8) | (unsigned char) in[i + 2];
        out += tbl[v >> 18]; out += tbl[(v >> 12) & 63]; out += tbl[(v >> 6) & 63]; out += tbl[v & 63];
    }
    if (i + 1 == in.size())
    {
        unsigned v = (unsigned char) in[i] << 16;
        out += tbl[v >> 18]; out += tbl[(v >> 12) & 63]; out += "==";
    }
    else if (i + 2 == in.size())
    {
        unsigned v = ((unsigned char) in[i] << 16) | ((unsigned char) in[i + 1] << 8);
        out += tbl[v >> 18]; out += tbl[(v >> 12) & 63]; out += tbl[(v >> 6) & 63]; out += '=';
    }
    return out;
}

static void pushStr(std::string &stack, const std::string &s)
{
    uint32_t n = (uint32_t) s.size();
    stack += s;
    for (int i = 0; i < 4; ++i)
        stack += (char) ((n >> (8 * i)) & 0xFF);
}

static void writeTerminal(const std::string &data) noexcept
{
    if (!isatty(STDOUT_FILENO))
        return;
    size_t done = 0;
    while (done < data.size())
    {
        ssize_t n = write(STDOUT_FILENO, data.data() + done, data.size() - done);
        if (n <= 0)
        {
            if (n < 0 && errno == EINTR)
                continue;
            break;
        }
        done += n;
    }
}

// far2l desktop notification (FARTTY_INTERACT_DESKTOP_NOTIFICATION, no
// answer): the far2l terminal tvterm runs in passes it to its client. Other
// terminals ignore the sequence (APC).
static void far2lNotify(const std::string &title, const std::string &text) noexcept
{
    std::string stack;
    pushStr(stack, text);
    pushStr(stack, title);
    stack += 'n';
    stack += '\0'; // Request id 0: no answer.
    writeTerminal("\x1B_far2l:" + base64(stack) + "\x1B\\");
}

static uint64_t notifiedOrder = 0; // The newest level 1 alert notified.
static time_t lastNotify = 0;

} // namespace alertrun

void drawAlertTitle(TFrame &frame) noexcept
{
    auto *window = dynamic_cast<TWindow *>(frame.owner);
    auto *target = dynamic_cast<AlertTarget *>(frame.owner);
    if (!window || !target)
        return;
    uint8_t color = 0;
    if (target->hasAlert())
        color = alertTitleColor ? alertLevelColor(target->getAlertLevel()) : 0;
    else if (target->isAlertSeen())
        color = alertSeenColor;
    if (!color)
        return;
    // The same position and width as in TFrame::draw.
    short width = frame.size.x;
    short l = width - 10;
    if (window->flags & (wfClose | wfZoom))
        l -= 6;
    if (window->number != wnNoNumber && window->number < 10)
        l -= 4;
    const char *title = window->getTitle(l);
    if (!title)
        return;
    l = min<short>(strwidth(title), width - 10);
    if (l <= 0)
        return;
    short i = (width - l) >> 1;
    TDrawBuffer b;
    // Only the text; the spaces around it and the frame keep their colors.
    b.moveStr(0, title, TColorAttr(color), l);
    frame.writeLine(i, 0, l, 1, b);
}

void execAlertSettings(TWindow &window, AlertTarget &target)
{
    using namespace alertdlg;
    TDialog *d = new TDialog(TRect(0, 0, 70, 21), "Alerts");
    d->options |= ofCentered;

    const char *title = window.getTitle(100);
    std::string heading = dynamic_cast<TerminalWindow *>(&window) ? "Terminal: " : "Window: ";
    heading += title && *title ? title : "(no title)";
    d->insert(new TStaticText(TRect(3, 2, 67, 3), heading));

    auto *activity = new TCheckBoxes(TRect(3, 4, 40, 5),
        new TSItem("~A~ny new text (activity)", nullptr));
    d->insert(activity);
    auto *activityLevel = new TInputLine(TRect(55, 4, 60, 5), sizeof(DialogData::activityLevel) - 1);
    d->insert(activityLevel);
    d->insert(new TLabel(TRect(42, 4, 55, 5), "~L~evel 1-5:", activityLevel));

    auto *keywords = new TInputLine(TRect(3, 7, 67, 8), sizeof(DialogData::keywords) - 1);
    d->insert(keywords);
    d->insert(new TLabel(TRect(2, 6, 67, 7), "K~e~ywords (a, b + c, !d, 1:e = level 1; case ignored):", keywords));

    auto *silence = new TInputLine(TRect(3, 10, 14, 11), sizeof(DialogData::silence) - 1);
    d->insert(silence);
    d->insert(new TLabel(TRect(2, 9, 67, 10), "~S~ilence: seconds without output (0: off; 1:120 = level 1):", silence));

    auto *action = new TInputLine(TRect(3, 13, 67, 14), sizeof(DialogData::action) - 1);
    d->insert(action);
    d->insert(new TLabel(TRect(2, 12, 67, 13), "Ac~t~ion for level 1 (empty: [alerts] action of tvterm.ini):", action));

    d->insert(new TStaticText(TRect(3, 15, 67, 16),
        "Alerts appear only for windows that are not selected."));
    d->insert(new TButton(TRect(22, 17, 32, 19), "O~K~", cmOK, bfDefault));
    d->insert(new TButton(TRect(36, 17, 48, 19), "Cancel", cmCancel, bfNormal));
    activity->select();

    const AlertSettings &cur = target.getAlertSettings();
    DialogData data {};
    data.activity = cur.activity ? 1 : 0;
    snprintf(data.activityLevel, sizeof(data.activityLevel), "%d", cur.activityLevel);
    snprintf(data.keywords, sizeof(data.keywords), "%s", cur.keywords.text.c_str());
    snprintf(data.silence, sizeof(data.silence), "%s%d",
             cur.silenceSeconds > 0 ? alertLevelPrefix(cur.silenceLevel).c_str() : "", cur.silenceSeconds);
    snprintf(data.action, sizeof(data.action), "%s", cur.action.c_str());
    activity->setData(&data.activity);
    activityLevel->setData(data.activityLevel);
    keywords->setData(data.keywords);
    silence->setData(data.silence);
    action->setData(data.action);

    TView *p = TProgram::application->validView(d);
    if (!p)
        return;
    // Invalid keywords: the dialog stays open, with a message.
    KeywordRules rules;
    ushort result;
    for (;;)
    {
        result = TProgram::deskTop->execView(p);
        if (result != cmOK)
            break;
        keywords->getData(data.keywords);
        std::string error;
        if (parseKeywords(data.keywords, rules, error))
            break;
        messageBox(mfError | mfOKButton, "Invalid keywords: %s", error.c_str());
        keywords->select();
    }
    if (result == cmOK)
    {
        activity->getData(&data.activity);
        activityLevel->getData(data.activityLevel);
        silence->getData(data.silence);
        action->getData(data.action);
    }
    TObject::destroy(p);
    if (result != cmOK)
        return;

    AlertSettings settings;
    settings.activity = data.activity & 1;
    {
        std::string lv = alertdlg::trim(data.activityLevel);
        char *end;
        long n = strtol(lv.c_str(), &end, 10);
        if (lv.empty())
            n = alertLevelMax;
        else if (*end || n < alertLevelMin || n > alertLevelMax)
        {
            messageBox(mfWarning | mfOKButton,
                "Invalid level: '%s' (use 1-5). Level 5 used for activity.", lv.c_str());
            n = alertLevelMax;
        }
        settings.activityLevel = (int) n;
    }
    settings.keywords = std::move(rules);
    int silenceLevel;
    std::string silenceText;
    bool levelOk = splitAlertLevel(data.silence, silenceLevel, silenceText);
    char *end;
    long seconds = strtol(silenceText.c_str(), &end, 10);
    std::string rest = alertdlg::trim(end);
    if (!levelOk || seconds < 0 || seconds > 999999 || !rest.empty() || alertdlg::trim(silenceText).empty())
    {
        if (!alertdlg::trim(data.silence).empty())
            messageBox(mfWarning | mfOKButton,
                "Invalid silence: '%s' (seconds, or N:seconds with level N 1-5). Silence alert disabled.",
                data.silence);
        seconds = 0;
        silenceLevel = alertLevelMax;
    }
    settings.silenceSeconds = (int) seconds;
    settings.silenceLevel = silenceLevel;
    settings.action = alertdlg::trim(data.action);
    target.setAlertSettings(std::move(settings));
}

// The most urgent alert, the oldest of that level ('level': only that
// level; 0: any).
static TWindow *nextAlert(TDeskTop &deskTop, int level = 0)
{
    TWindow *best = nullptr;
    int bestLevel = 0;
    uint64_t bestOrder = 0;
    for (TWindow *w : getDeskTopWindows(deskTop))
        if (auto *target = dynamic_cast<AlertTarget *>(w))
            if (target->hasAlert())
            {
                int l = target->getAlertLevel();
                uint64_t o = target->getAlertOrder();
                if (level && l != level)
                    continue;
                if (!best || l < bestLevel || (l == bestLevel && o < bestOrder))
                {
                    best = w;
                    bestLevel = l;
                    bestOrder = o;
                }
            }
    return best;
}

bool goToNextAlert(TDeskTop &deskTop)
{
    if (TWindow *w = nextAlert(deskTop))
    {
        // On another desktop: that desktop is shown.
        desktops::selectWindow(deskTop, w);
        return true;
    }
    return false;
}

size_t countAlerts(TDeskTop &deskTop)
{
    size_t count = 0;
    for (TWindow *w : getDeskTopWindows(deskTop))
        if (auto *target = dynamic_cast<AlertTarget *>(w))
            count += target->hasAlert();
    return count;
}

void countAlerts(TDeskTop &deskTop, size_t &level1, size_t &others)
{
    level1 = others = 0;
    for (TWindow *w : getDeskTopWindows(deskTop))
        if (auto *target = dynamic_cast<AlertTarget *>(w))
            if (target->hasAlert())
                ++(target->getAlertLevel() == alertLevelMin ? level1 : others);
}

std::string level1AlertText(TDeskTop &deskTop)
{
    TWindow *w = nextAlert(deskTop, alertLevelMin);
    auto *target = dynamic_cast<AlertTarget *>(w);
    if (!target)
        return {};
    AlertInfo info = target->getAlertInfo();
    // The keyword (the mark without "[!" and "]"); silence and activity.
    std::string what = info.mark;
    if (what.size() >= 3 && what.compare(0, 2, "[!") == 0 && what.back() == ']')
        what = what.substr(2, what.size() - 3);
    if (what.empty())
        what = "activity";
    return "A1 " + info.title + ": " + what;
}

size_t clearLowAlerts(TDeskTop &deskTop)
{
    size_t n = 0;
    for (TWindow *w : getDeskTopWindows(deskTop))
        if (auto *target = dynamic_cast<AlertTarget *>(w))
            if (target->hasAlert() && target->getAlertLevel() > alertLevelMin)
            {
                target->dismissAlert();
                ++n;
            }
    return n;
}

void alertTick(TDeskTop &deskTop) noexcept
{
    using namespace alertrun;
    uint64_t newest = 0;
    for (TWindow *w : getDeskTopWindows(deskTop))
        if (auto *target = dynamic_cast<AlertTarget *>(w))
            if (target->hasAlert() && target->getAlertLevel() == alertLevelMin)
                newest = max(newest, target->getAlertOrder());
    if (!newest)
        return;
    time_t now = time(nullptr);
    bool fresh = newest > notifiedOrder;
    bool repeat = alertConfig.notifyRepeat > 0 && !fresh &&
                  (now - lastNotify >= alertConfig.notifyRepeat || now < lastNotify);
    if (!fresh && !repeat)
        return;
    notifiedOrder = max(notifiedOrder, newest);
    lastNotify = now;
    if (alertConfig.bell)
        writeTerminal("\a");
    if (alertConfig.notify)
    {
        std::string text = level1AlertText(deskTop);
        if (text.size() > 3)
            text = text.substr(3); // Without "A1 ".
        far2lNotify("tvterm: level 1 alert", text);
    }
}

std::string listAlerts(TDeskTop &deskTop)
{
    struct Active
    {
        int level;
        uint64_t order;
        AlertTarget *target;
        std::string desktop; // As in the Window List: "*D1", " 2:mail".
    };
    std::vector<Active> active;
    size_t deskWidth = 0;
    for (TWindow *w : getDeskTopWindows(deskTop))
        if (auto *target = dynamic_cast<AlertTarget *>(w))
            if (target->hasAlert())
            {
                active.push_back({target->getAlertLevel(), target->getAlertOrder(), target, desktopColumn(w)});
                deskWidth = max<size_t>(deskWidth, strwidth(active.back().desktop));
            }
    if (active.empty())
        return "--- No active alerts ---\n";
    std::sort(active.begin(), active.end(),
              [] (const auto &a, const auto &b) {
                  return a.level != b.level ? a.level < b.level : a.order < b.order;
              });
    std::string out = "--- Active alerts (" + std::to_string(active.size()) + ") ---\n";
    for (auto &a : active)
    {
        AlertInfo info = a.target->getAlertInfo();
        // The desktop column padded, so that the marks start in one column.
        std::string desk = a.desktop;
        desk.append(deskWidth - min<size_t>(deskWidth, strwidth(desk)), ' ');
        out += formatEventTime(info.time) + " A" + std::to_string(a.level) + " " + desk + " " +
               info.mark + " " + info.title + "\n";
        if (!info.group.empty())
        {
            out += "    group: " + info.group + "\n";
            out += "    line:  " + info.line + "\n";
        }
    }
    return out;
}
