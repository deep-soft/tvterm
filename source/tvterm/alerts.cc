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
#define Uses_MsgBox
#include <tvision/tv.h>

#include "alerts.h"
#include "globalkeys.h"
#include "wnd.h"
#include "winlist.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>

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
    for (const auto &group : groups)
    {
        bool all = true;
        for (const auto &term : group.terms)
            if (lowerLine.find(term) == std::string::npos)
            {
                all = false;
                break;
            }
        if (all)
            return &group;
    }
    if (groups.empty() && !excludes.empty() &&
        lowerLine.find_first_not_of(" \t") != std::string::npos)
    {
        // Only exclusions: any other line.
        static const KeywordGroup anyLine {{}, "(any line without the exclusions)", "*"};
        return &anyLine;
    }
    return nullptr;
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
    char keywords[1024];
    char silence[7];
};

} // namespace alertdlg

uint64_t newAlertOrder() noexcept
{
    static uint64_t lastAlertOrder = 0;
    return ++lastAlertOrder;
}

void execAlertSettings(TWindow &window, AlertTarget &target)
{
    using namespace alertdlg;
    TDialog *d = new TDialog(TRect(0, 0, 66, 17), "Alerts");
    d->options |= ofCentered;

    const char *title = window.getTitle(100);
    std::string heading = dynamic_cast<TerminalWindow *>(&window) ? "Terminal: " : "Window: ";
    heading += title && *title ? title : "(no title)";
    d->insert(new TStaticText(TRect(3, 2, 63, 3), heading));

    auto *activity = new TCheckBoxes(TRect(3, 4, 63, 5),
        new TSItem("~A~ny new text (activity)", nullptr));
    d->insert(activity);

    auto *keywords = new TInputLine(TRect(3, 7, 63, 8), sizeof(DialogData::keywords) - 1);
    d->insert(keywords);
    d->insert(new TLabel(TRect(2, 6, 63, 7), "K~e~ywords (a, b + c, !d; case-insensitive):", keywords));

    auto *silence = new TInputLine(TRect(3, 10, 11, 11), sizeof(DialogData::silence) - 1);
    d->insert(silence);
    d->insert(new TLabel(TRect(2, 9, 63, 10), "~S~ilence: seconds without output (0: off):", silence));

    d->insert(new TStaticText(TRect(3, 12, 63, 13),
        "Alerts appear only for windows that are not selected."));
    d->insert(new TButton(TRect(20, 14, 30, 16), "O~K~", cmOK, bfDefault));
    d->insert(new TButton(TRect(34, 14, 46, 16), "Cancel", cmCancel, bfNormal));
    activity->select();

    const AlertSettings &cur = target.getAlertSettings();
    DialogData data {};
    data.activity = cur.activity ? 1 : 0;
    snprintf(data.keywords, sizeof(data.keywords), "%s", cur.keywords.text.c_str());
    snprintf(data.silence, sizeof(data.silence), "%d", cur.silenceSeconds);
    activity->setData(&data.activity);
    keywords->setData(data.keywords);
    silence->setData(data.silence);

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
        silence->getData(data.silence);
    }
    TObject::destroy(p);
    if (result != cmOK)
        return;

    AlertSettings settings;
    settings.activity = data.activity & 1;
    settings.keywords = std::move(rules);
    char *end;
    long seconds = strtol(data.silence, &end, 10);
    std::string rest = alertdlg::trim(end);
    if (seconds < 0 || !rest.empty() || alertdlg::trim(data.silence).empty())
    {
        if (!alertdlg::trim(data.silence).empty())
            messageBox(mfWarning | mfOKButton,
                "Invalid number of seconds: '%s'. Silence alert disabled.", data.silence);
        seconds = 0;
    }
    settings.silenceSeconds = (int) seconds;
    target.setAlertSettings(std::move(settings));
}

static TWindow *oldestAlert(TDeskTop &deskTop)
{
    TWindow *best = nullptr;
    uint64_t bestOrder = 0;
    for (TWindow *w : getDeskTopWindows(deskTop))
        if (auto *target = dynamic_cast<AlertTarget *>(w))
            if (target->hasAlert() && (!best || target->getAlertOrder() < bestOrder))
            {
                best = w;
                bestOrder = target->getAlertOrder();
            }
    return best;
}

bool goToNextAlert(TDeskTop &deskTop)
{
    if (TWindow *w = oldestAlert(deskTop))
    {
        w->select();
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

std::string listAlerts(TDeskTop &deskTop)
{
    std::vector<std::pair<uint64_t, AlertTarget *>> active;
    for (TWindow *w : getDeskTopWindows(deskTop))
        if (auto *target = dynamic_cast<AlertTarget *>(w))
            if (target->hasAlert())
                active.emplace_back(target->getAlertOrder(), target);
    if (active.empty())
        return "--- No active alerts ---\n";
    std::sort(active.begin(), active.end(),
              [] (const auto &a, const auto &b) { return a.first < b.first; });
    std::string out = "--- Active alerts (" + std::to_string(active.size()) + ") ---\n";
    for (auto &a : active)
    {
        AlertInfo info = a.second->getAlertInfo();
        out += formatEventTime(info.time) + " " + info.mark + " " + info.title + "\n";
        if (!info.group.empty())
        {
            out += "    group: " + info.group + "\n";
            out += "    line:  " + info.line + "\n";
        }
    }
    return out;
}

void AlertStatusLine::draw()
{
    TVTermStatusLine::draw();
    if (alertCount == 0 || TVTermStatusLine::hasMessage())
        return;
    char text[96];
    snprintf( text, sizeof(text), " Alerts: %zu (%s G) ", alertCount,
              globalkeys::menuKeyName().c_str() );
    int width = (int) strwidth(text);
    if (width >= size.x)
        return;
    TDrawBuffer b;
    b.moveStr(0, text, mapColor(4));
    writeLine(size.x - width, 0, width, 1, b);
}
