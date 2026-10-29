#include "highlight.h"

#include <regex.h>
#include <ctype.h>
#include <stdint.h>
#include <string.h>
#include <algorithm>
#include <map>
#include <sstream>

struct HighlightRule
{
    enum Kind { Line, Match, Groups, Hash };

    Kind kind {Match};
    int color {-1}; // Not used by 'hash'.
    regex_t re;
    bool compiled {false};
    size_t groups {0};

    HighlightRule() = default;
    HighlightRule(const HighlightRule &) = delete;
    HighlightRule &operator=(const HighlightRule &) = delete;
    ~HighlightRule() { if (compiled) regfree(&re); }
};

// Own namespace: this file is compiled together with others (Unity Build).
namespace highlightini
{

// One item of a set: a rule or an include.
struct Item
{
    std::shared_ptr<const HighlightRule> rule;
    std::string include;
    int lineNo {0};
};

struct RawSet
{
    std::string origin; // "built-in" or the path of tvterm.ini.
    std::vector<Item> items;
};

static std::map<std::string, HighlightRules> sets;
static unsigned generation = 0;

static std::string trim(const std::string &s)
{
    size_t a = s.find_first_not_of(" \t");
    if (a == std::string::npos)
        return {};
    size_t b = s.find_last_not_of(" \t");
    return s.substr(a, b - a + 1);
}

static std::string lower(std::string s)
{
    for (char &c : s)
        c = (char) tolower((unsigned char) c);
    return s;
}

static std::vector<std::string> words(const std::string &s)
{
    std::vector<std::string> out;
    std::istringstream in(s);
    std::string w;
    while (in >> w)
        out.push_back(w);
    return out;
}

static int colorIndex(const std::string &name)
// 'red' -> 4, 'bright-red' -> 12 (BIOS order); -1 if unknown.
{
    static const char *const names[8] =
        {"black", "blue", "green", "cyan", "red", "magenta", "yellow", "white"};
    std::string n = lower(name);
    int bright = 0;
    if (n.compare(0, 7, "bright-") == 0)
    {
        bright = 8;
        n = n.substr(7);
    }
    for (int i = 0; i < 8; ++i)
        if (n == names[i])
            return i + bright;
    return -1;
}

static bool parseColor(const std::vector<std::string> &w, int &attr, std::string &error)
// "red", "bright-yellow on red".
{
    if (w.empty())
        return error = "missing color (e.g. red, bright-yellow, white on blue)", false;
    if (w.size() != 1 && !(w.size() == 3 && lower(w[1]) == "on"))
        return error = "invalid color '" + w[0] + (w.size() > 1 ? " " + w[1] : std::string()) +
                       "' (use e.g. red, bright-yellow, white on blue)", false;
    int fg = colorIndex(w[0]);
    if (fg < 0)
        return error = "unknown color '" + w[0] + "'", false;
    int bg = 0;
    if (w.size() == 3 && (bg = colorIndex(w[2])) < 0)
        return error = "unknown color '" + w[2] + "'", false;
    attr = bg * 16 + fg;
    return true;
}

static bool parseRule( const std::string &key, const std::string &value,
                       std::shared_ptr<const HighlightRule> &out, std::string &error )
// <color> "<regex>" [icase]
{
    HighlightRule::Kind kind = key == "line" ? HighlightRule::Line :
                               key == "match" ? HighlightRule::Match :
                               key == "groups" ? HighlightRule::Groups : HighlightRule::Hash;
    size_t q = value.find('"');
    if (q == std::string::npos)
        return error = "the regular expression must be in quotes", false;
    std::vector<std::string> colorWords = words(value.substr(0, q));
    int color = -1;
    if (kind == HighlightRule::Hash)
    {
        if (!colorWords.empty())
            return error = "'hash' takes no color", false;
    }
    else if (!parseColor(colorWords, color, error))
        return false;
    // \" is a quote; other backslashes are kept for the regular expression.
    std::string pattern;
    size_t i = q + 1;
    bool closed = false;
    while (i < value.size())
    {
        char c = value[i];
        if (c == '\\' && i + 1 < value.size())
        {
            if (value[i + 1] == '"')
                pattern += '"';
            else
                pattern.append(value, i, 2);
            i += 2;
        }
        else if (c == '"')
        {
            closed = true;
            ++i;
            break;
        }
        else
            pattern += c, ++i;
    }
    if (!closed)
        return error = "missing closing quote", false;
    if (pattern.empty())
        return error = "empty regular expression", false;
    bool icase = false;
    for (auto &w : words(value.substr(i)))
        if (lower(w) == "icase")
            icase = true;
        else
            return error = "unknown option '" + w + "' after the expression (only icase)", false;
    std::shared_ptr<HighlightRule> rule(new HighlightRule);
    int err = regcomp(&rule->re, pattern.c_str(), REG_EXTENDED | (icase ? REG_ICASE : 0));
    if (err != 0)
    {
        char buf[256];
        regerror(err, &rule->re, buf, sizeof(buf));
        // 'regcomp' frees what it allocated when it fails ('compiled' stays
        // false: no 'regfree').
        return error = "invalid regular expression \"" + pattern + "\": " + buf, false;
    }
    rule->compiled = true;
    rule->kind = kind;
    rule->color = color;
    rule->groups = rule->re.re_nsub;
    if (kind == HighlightRule::Groups && rule->groups == 0)
        return error = "'groups' needs parentheses in the expression (or use 'match')", false;
    out = std::move(rule);
    return true;
}

static void parseText(const char *text, std::vector<HighlightSection> &sections)
// Sections of an INI text (the built-in sets); other sections are ignored.
{
    std::istringstream in(text);
    std::string line;
    int lineNo = 0;
    HighlightSection *current = nullptr;
    while (std::getline(in, line))
    {
        ++lineNo;
        line = trim(line);
        if (line.empty() || line[0] == '#' || line[0] == ';')
            continue;
        if (line[0] == '[')
        {
            std::string name = lower(trim(line.substr(1, line.find(']') - 1)));
            current = nullptr;
            if (name.compare(0, 10, "highlight ") == 0)
            {
                sections.emplace_back();
                current = &sections.back();
                current->name = trim(name.substr(10));
                current->lineNo = lineNo;
            }
            continue;
        }
        size_t eq = line.find('=');
        if (current && eq != std::string::npos)
            current->entries.push_back({lineNo, lower(trim(line.substr(0, eq))), trim(line.substr(eq + 1))});
    }
}

static void addSections( std::map<std::string, RawSet> &raw, const std::vector<HighlightSection> &sections,
                         const std::string &origin, std::string &warnings )
{
    for (auto &section : sections)
    {
        RawSet set;
        set.origin = origin;
        for (auto &e : section.entries)
        {
            auto warn = [&] (const std::string &msg) {
                warnings += origin + ": line " + std::to_string(e.lineNo) + ": [highlight " +
                            section.name + "] " + msg + "; rule ignored\n";
            };
            if (e.key == "include")
            {
                std::vector<std::string> names = splitHighlightNames(e.value);
                if (names.empty())
                    warn("include without a set name");
                for (auto &n : names)
                {
                    Item item;
                    item.include = n;
                    item.lineNo = e.lineNo;
                    set.items.push_back(std::move(item));
                }
            }
            else if (e.key == "line" || e.key == "match" || e.key == "groups" || e.key == "hash")
            {
                Item item;
                std::string error;
                item.lineNo = e.lineNo;
                if (parseRule(e.key, e.value, item.rule, error))
                    set.items.push_back(std::move(item));
                else
                    warn(e.key + ": " + error);
            }
            else
                warn("unknown key '" + e.key + "' (line, match, groups, hash, include)");
        }
        // A section replaces the set with the same name.
        raw[section.name] = std::move(set);
    }
}

static void expand( const std::map<std::string, RawSet> &raw, const std::string &name,
                    std::vector<std::string> &stack, HighlightRules &out, std::string &warnings )
{
    const RawSet &set = raw.at(name);
    stack.push_back(name);
    for (auto &item : set.items)
    {
        if (item.rule)
            out.push_back(item.rule);
        else
        {
            auto warn = [&] (const std::string &msg) {
                warnings += set.origin + ": line " + std::to_string(item.lineNo) + ": [highlight " +
                            name + "] " + msg + "; include ignored\n";
            };
            if (!raw.count(item.include))
                warn("unknown set '" + item.include + "'");
            else if (std::find(stack.begin(), stack.end(), item.include) != stack.end())
                warn("set '" + item.include + "' includes itself (through " + name + ")");
            else
                expand(raw, item.include, stack, out, warnings);
        }
    }
    stack.pop_back();
}

static int hashColor(const char *s, size_t len)
// A color for the value: always the same for the same text. Without red,
// yellow and green, which the other rules use for meanings.
{
    static const int palette[] = {0x0B, 0x0D, 0x09, 0x03, 0x05, 0x0F, 0x1F, 0x5F};
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < len; ++i)
        h = (h ^ (unsigned char) s[i]) * 16777619u;
    return palette[h % (sizeof(palette) / sizeof(palette[0]))];
}

} // namespace highlightini

void rebuildHighlightSets( const std::vector<HighlightSection> &sections, const std::string &path,
                           std::string &warnings )
{
    using namespace highlightini;
    std::map<std::string, RawSet> raw;
    std::vector<HighlightSection> builtin;
    parseText(builtinHighlightText(), builtin);
    addSections(raw, builtin, "built-in highlight sets", warnings);
    addSections(raw, sections, path, warnings);
    sets.clear();
    // The warnings about an include are given once, not for every set that
    // includes it.
    std::string expandWarnings;
    for (auto &entry : raw)
    {
        std::vector<std::string> stack;
        std::string w;
        expand(raw, entry.first, stack, sets[entry.first], w);
        size_t start = 0, end;
        while ((end = w.find('\n', start)) != std::string::npos)
        {
            std::string one = w.substr(start, end - start + 1);
            if (expandWarnings.find(one) == std::string::npos)
                expandWarnings += one;
            start = end + 1;
        }
    }
    warnings += expandWarnings;
    if (++generation == 0)
        generation = 1;
}

unsigned highlightGeneration()
{
    return highlightini::generation;
}

bool findHighlightSet(const std::string &name, HighlightRules &rules)
{
    using namespace highlightini;
    auto it = sets.find(lower(name));
    if (it == sets.end())
        return false;
    rules = it->second;
    return true;
}

std::vector<std::string> highlightSetNames()
{
    using namespace highlightini;
    std::vector<std::string> names;
    for (auto &entry : sets)
        names.push_back(entry.first);
    return names;
}

std::vector<std::string> splitHighlightNames(const std::string &text)
{
    using namespace highlightini;
    std::string t = text;
    std::replace(t.begin(), t.end(), ',', ' ');
    std::vector<std::string> names = words(t);
    for (auto &n : names)
        n = lower(n);
    return names;
}

void highlightLine(const HighlightRules &rules, const std::string &text, std::vector<int> &colors)
{
    using namespace highlightini;
    colors.assign(text.size(), -1);
    if (text.empty() || rules.empty())
        return;
    std::vector<char> taken(text.size(), 0);
    auto paint = [&] (size_t a, size_t b, int color) {
        for (size_t i = a; i < b && i < text.size(); ++i)
            if (!taken[i])
                colors[i] = color, taken[i] = 1;
    };
    // The first 'line' rule that matches: the base color.
    for (auto &rule : rules)
        if (rule->kind == HighlightRule::Line && regexec(&rule->re, text.c_str(), 0, nullptr, 0) == 0)
        {
            std::fill(colors.begin(), colors.end(), rule->color);
            break;
        }
    regmatch_t m[10];
    for (auto &rule : rules)
    {
        if (rule->kind == HighlightRule::Line)
            continue;
        size_t nm = std::min<size_t>(rule->groups + 1, 10);
        size_t pos = 0;
        while (pos <= text.size())
        {
            if (regexec(&rule->re, text.c_str() + pos, nm, m, pos > 0 ? REG_NOTBOL : 0) != 0 || m[0].rm_so < 0)
                break;
            size_t so = pos + m[0].rm_so, eo = pos + m[0].rm_eo;
            switch (rule->kind)
            {
                case HighlightRule::Match:
                    paint(so, eo, rule->color);
                    break;
                case HighlightRule::Groups:
                    for (size_t g = 1; g < nm; ++g)
                        if (m[g].rm_so >= 0)
                            paint(pos + m[g].rm_so, pos + m[g].rm_eo, rule->color);
                    break;
                case HighlightRule::Hash:
                    if (nm > 1)
                    {
                        for (size_t g = 1; g < nm; ++g)
                            if (m[g].rm_so >= 0 && m[g].rm_eo > m[g].rm_so)
                                paint( pos + m[g].rm_so, pos + m[g].rm_eo,
                                       hashColor(text.data() + pos + m[g].rm_so, m[g].rm_eo - m[g].rm_so) );
                    }
                    else
                        paint(so, eo, hashColor(text.data() + so, eo - so));
                    break;
                default:
                    break;
            }
            // An empty match: on to the next character.
            pos = eo > so ? eo : so + 1;
        }
    }
}

const char *builtinHighlightText()
{
    // The rules of multitail.conf were converted: 'cs_re' -> match,
    // 'cs_re_s' / 'mcsre_s' -> groups; 'x,,bold' -> bright-x; '.*?' (not
    // in POSIX) -> a negated class, e.g. ua=([^;]*); no backslashes inside
    // [ ]. Only POSIX extended expressions: the same on glibc and musl.
    return R"HL(
[highlight levels]
groups = bright-green " (DEBUG)"
groups = bright-green " (INFO)"
groups = bright-yellow " (WARN)"
groups = bright-red " (ERROR)"
groups = bright-red " (NOTICE)"
groups = bright-yellow on red " (FATAL)"

[highlight timestamps]
groups = cyan "^([0-9]{4}-[0-9]{2}-[0-9]{2} [0-9]{2}:[0-9]{2}:[0-9]{2},[0-9]*)"
groups = cyan "^([0-9]{4}-[0-9]{2}-[0-9]{2}\^[0-9]{2}:[0-9]{2}:[0-9]{2})"
groups = cyan "^([0-9]{4}/[0-9]{2}/[0-9]{2} [0-9]{2}:[0-9]{2}:[0-9]{2})"
groups = cyan "^([0-9]{4}-[0-9]{2}-[0-9]{2}[T ][0-9]{2}:[0-9]{2}:[0-9]{2}[.0-9]*([+-][0-9]{2}:?[0-9]{2}|Z)?)"
groups = cyan "^([A-Z][a-z]{2} [ 0-9][0-9] [0-9]{2}:[0-9]{2}:[0-9]{2})"
groups = cyan "\[([0-9]{2}/[A-Z][a-z]{2}/[0-9]{4}:[0-9]{2}:[0-9]{2}:[0-9]{2} [+-][0-9]{4})\]"

[highlight kv]
match = cyan "\[|\]"
match = yellow "[;=]"
groups = bright-blue "([A-Za-z-]*)="
groups = bright-magenta "=([A-Za-z0-9_.-]*@[A-Za-z0-9_.-]*)[;,]"
groups = bright-yellow "=([^=;, ]*@[A-Za-z0-9_.-]*)"
match = bright-green "[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}"
groups = bright-yellow "[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}:([0-9]{1,5})"
groups = green "ua=([^;]*);"
groups = green "mid=([^;]*);"

[highlight syslog]
groups = bright-black "^([0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}[.0-9]*([+-][0-9]{2}:?[0-9]{2}|Z)?) "
groups = bright-black "^([A-Z][a-z]{2} [ 0-9][0-9] [0-9]{2}:[0-9]{2}:[0-9]{2}) "
hash = " ([A-Za-z][A-Za-z0-9_./-]*)\[[0-9]+\]: "

[highlight zimbra-mta]
match = bright-red "status=bounced"
match = bright-red "reject:"
match = bright-red "NOQUEUE"
match = bright-red "SASL [A-Z0-9-]+ authentication failed"
match = bright-yellow "status=deferred"
match = bright-yellow "warning:"
match = bright-green "status=sent"
match = bright-green "Passed CLEAN"
hash = "]: ([0-9A-F]{8,12}): "
hash = "queued as ([0-9A-F]{8,12})"
hash = "Queue-ID: ([0-9A-F]{8,12})"
include = syslog
include = kv

[highlight zimbra-mailbox]
groups = bright-red "(ESMTP)"
groups = bright-green "(IMAPS)"
groups = bright-red " (Ban)"
groups = bright-red " (reject:)"
groups = bright-magenta " (Hop count exceeded)"
groups = bright-green " (Delivery OK)"
groups = bright-cyan "( <[^>]*@[^>]*> -> <[^>]*@[^>]*> )"
include = levels
include = timestamps
include = kv

[highlight nginx]
groups = bright-red "\" (5[0-9]{2}) "
groups = bright-yellow "\" (4[0-9]{2}) "
groups = bright-green "\" (2[0-9]{2}) "
groups = bright-white "\"(GET|POST|PUT|DELETE|HEAD|OPTIONS|PATCH|PROPFIND|REPORT) "
include = timestamps
include = kv

[highlight fail2ban]
groups = bright-red "^.*(fail2ban\.[[:alnum:]]+)\[[[:digit:]]+\]:"
groups = bright-red "^.*fail2ban\.[[:alnum:]]+\[[[:digit:]]+\]: ([[:alnum:]]+) \[[^]]*\] ([[:alnum:]]+) [0-9.:]+"
groups = bright-cyan "^.*fail2ban\.[[:alnum:]]+\[[[:digit:]]+\]: [[:alnum:]]+ (\[[^]]*\]) [[:alnum:]]+ [0-9.:]+"
groups = bright-yellow "^.*fail2ban\.[[:alnum:]]+\[[[:digit:]]+\]: [[:alnum:]]+ \[[^]]*\] [[:alnum:]]+ ([0-9.:]+)"
groups = bright-red " (Ban)"
groups = bright-red " (fail2ban\.actions)"
groups = bright-red " (DROP)"
groups = bright-blue " (RO, Romania)"
match = bright-green "[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}"
include = levels
include = timestamps

[highlight fortigate]
groups = bright-cyan on blue " (srccountry=\"Reserved\")"
groups = bright-cyan on blue " (srccountry=\"Romania\")"
groups = bright-cyan on red " (srccountry=\"[A-Za-z ]*\")"
groups = bright-cyan " (srcip=)"
groups = bright-red "(VIP-TO-MAIL[^\" ]*BLOCK)"
groups = bright-yellow "(FIP-PERMIT-IN-993)"
groups = bright-red "(FIP-PERMIT-IN-465)"
groups = bright-red "(SMTPS)"
groups = bright-green "(IMAPS)"
include = levels
include = timestamps
include = kv

[highlight rsync]
groups = bright-green " (start)"
groups = bright-green " (stop)"
groups = cyan " (sent)"
groups = bright-cyan " (received)"
groups = bright-green " (Total transferred file size)"
groups = bright-red " (failed)"
groups = bright-yellow on red " (FATAL)"
include = timestamps
include = kv
)HL";
}
