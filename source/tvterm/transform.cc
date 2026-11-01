#include "transform.h"

#include <regex.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>
#include <map>
#include <sstream>

struct TransformRule
{
    // A part of the template: plain text, or a group of the match.
    struct Piece
    {
        std::string text;   // Plain text (group < 0).
        int group {-1};     // 0: the whole match, 1-9: a group.
        int width {0};      // ${n:>W} / ${n:<W}; 0: as it is.
        bool right {false}; // '>': aligned to the right.
        bool cut {false};   // '!': a longer value is cut.
    };

    regex_t re;
    bool compiled {false};
    std::vector<Piece> pieces;

    TransformRule() = default;
    TransformRule(const TransformRule &) = delete;
    TransformRule &operator=(const TransformRule &) = delete;
    ~TransformRule() { if (compiled) regfree(&re); }
};

// Own namespace: this file is compiled together with others (Unity Build).
namespace transformini
{

static std::map<std::string, TransformRules> sets;
static unsigned generation = 0;

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

static bool readQuoted(const std::string &value, size_t &i, std::string &out, std::string &error)
// The text between quotes starting at 'i' (spaces before it skipped); \" is
// a quote, other backslashes are kept. 'i' goes after the closing quote.
{
    while (i < value.size() && (value[i] == ' ' || value[i] == '\t'))
        ++i;
    if (i >= value.size() || value[i] != '"')
        return error = "expected \"<regex>\" \"<template>\"", false;
    ++i;
    out.clear();
    while (i < value.size())
    {
        char c = value[i];
        if (c == '\\' && i + 1 < value.size())
        {
            if (value[i + 1] == '"')
                out += '"';
            else
                out.append(value, i, 2);
            i += 2;
        }
        else if (c == '"')
        {
            ++i;
            return true;
        }
        else
            out += c, ++i;
    }
    return error = "missing closing quote", false;
}

static bool parseTemplate( const std::string &t, std::vector<TransformRule::Piece> &pieces,
                           int &maxGroup, std::string &error )
{
    pieces.clear();
    maxGroup = 0;
    std::string text;
    auto addGroup = [&] (const TransformRule::Piece &p) {
        if (!text.empty())
        {
            pieces.emplace_back();
            pieces.back().text = std::move(text);
            text.clear();
        }
        pieces.push_back(p);
        maxGroup = std::max(maxGroup, p.group);
    };
    size_t i = 0;
    while (i < t.size())
    {
        char c = t[i];
        if (c == '\\' && i + 1 < t.size())
        {
            char n = t[i + 1];
            if (n >= '0' && n <= '9')
            {
                TransformRule::Piece p;
                p.group = n - '0';
                addGroup(p);
            }
            else if (n == 't')
                text += '\t';
            else if (n == '\\' || n == '$')
                text += n;
            else
                text.append(t, i, 2);
            i += 2;
        }
        else if (c == '$' && i + 1 < t.size() && t[i + 1] == '{')
        {
            size_t close = t.find('}', i + 2);
            std::string inner = close == std::string::npos ? std::string() : t.substr(i + 2, close - i - 2);
            // n, n:>W, n:<W, with an optional '!' at the end.
            TransformRule::Piece p;
            bool ok = !inner.empty() && inner[0] >= '0' && inner[0] <= '9';
            if (ok)
            {
                p.group = inner[0] - '0';
                if (inner.size() > 1)
                {
                    std::string spec = inner.substr(1);
                    if (!spec.empty() && spec.back() == '!')
                        p.cut = true, spec.pop_back();
                    ok = spec.size() >= 3 && spec[0] == ':' && (spec[1] == '>' || spec[1] == '<') &&
                         spec.size() <= 6 &&
                         spec.find_first_not_of("0123456789", 2) == std::string::npos;
                    if (ok)
                    {
                        p.right = spec[1] == '>';
                        p.width = atoi(spec.c_str() + 2);
                        ok = p.width > 0;
                    }
                }
            }
            if (!ok)
            {
                std::string shown = close == std::string::npos ? t.substr(i) : t.substr(i, close - i + 1);
                return error = "invalid '" + shown + "' in the template (use ${n}, ${n:>W}, ${n:<W}, "
                               "optionally with ! before }, n from 0 to 9)", false;
            }
            addGroup(p);
            i = close + 1;
        }
        else
            text += c, ++i;
    }
    if (!text.empty())
    {
        pieces.emplace_back();
        pieces.back().text = std::move(text);
    }
    return true;
}

static size_t utf8Next(const std::string &s, size_t i)
// The byte after the character at 'i'.
{
    ++i;
    while (i < s.size() && ((unsigned char) s[i] & 0xC0) == 0x80)
        ++i;
    return i;
}

static size_t charCount(const std::string &s, size_t a, size_t b)
{
    size_t n = 0;
    for (size_t i = a; i < b; i = std::min(utf8Next(s, i), b))
        ++n;
    return n;
}

static void appendPiece( std::string &out, const TransformRule::Piece &p, const std::string &text,
                         size_t base, const regmatch_t *m, size_t nm )
{
    if (p.group < 0)
    {
        out += p.text;
        return;
    }
    size_t a = 0, b = 0;
    if ((size_t) p.group < nm && m[p.group].rm_so >= 0)
        a = base + m[p.group].rm_so, b = base + m[p.group].rm_eo;
    if (p.width <= 0)
    {
        out.append(text, a, b - a);
        return;
    }
    size_t chars = charCount(text, a, b);
    size_t width = (size_t) p.width;
    if (chars > width && p.cut)
    {
        size_t end = a;
        for (size_t k = 0; k < width; ++k)
            end = utf8Next(text, end);
        b = std::min(end, b);
        chars = width;
    }
    size_t pad = chars < width ? width - chars : 0;
    if (p.right)
        out.append(pad, ' ');
    out.append(text, a, b - a);
    if (!p.right)
        out.append(pad, ' ');
}

static std::string applyRule(const TransformRule &rule, const std::string &text)
// Every match, like sed 's/.../.../g' (an empty match right after a match
// is skipped).
{
    enum { nm = 10 };
    regmatch_t m[nm];
    std::string out;
    size_t pos = 0;
    bool any = false;
    bool afterMatch = false; // A non-empty match ended at 'pos'.
    while (pos <= text.size())
    {
        if (regexec(&rule.re, text.c_str() + pos, nm, m, pos > 0 ? REG_NOTBOL : 0) != 0 || m[0].rm_so < 0)
            break;
        size_t so = pos + m[0].rm_so, eo = pos + m[0].rm_eo;
        if (eo == so && so == pos && afterMatch)
        {
            // The next character is kept and the search goes on after it.
            if (pos >= text.size())
                break;
            size_t next = utf8Next(text, pos);
            out.append(text, pos, next - pos);
            pos = next;
            afterMatch = false;
            continue;
        }
        if (!any)
            out.reserve(text.size() + 16), any = true;
        out.append(text, pos, so - pos);
        for (auto &p : rule.pieces)
            appendPiece(out, p, text, pos, m, nm);
        if (eo > so)
        {
            pos = eo;
            afterMatch = true;
        }
        else
        {
            // An empty match: the next character is kept.
            if (so >= text.size())
            {
                pos = text.size();
                break;
            }
            size_t next = utf8Next(text, so);
            out.append(text, so, next - so);
            pos = next;
            afterMatch = false;
        }
    }
    if (!any)
        return text;
    if (pos < text.size())
        out.append(text, pos, std::string::npos);
    return out;
}

} // namespace transformini

bool parseTransformRule( const std::string &key, const std::string &value,
                         std::shared_ptr<const TransformRule> &out, std::string &error )
// replace = "<regex>" "<template>" [icase]
{
    using namespace transformini;
    if (key != "replace")
        return error = "unknown key '" + key + "' (replace)", false;
    size_t i = 0;
    std::string pattern, tmpl;
    if (!readQuoted(value, i, pattern, error) || !readQuoted(value, i, tmpl, error))
        return false;
    if (pattern.empty())
        return error = "empty regular expression", false;
    bool icase = false;
    for (auto &w : words(value.substr(i)))
        if (lower(w) == "icase")
            icase = true;
        else
            return error = "unknown option '" + w + "' after the template (only icase)", false;
    std::shared_ptr<TransformRule> rule(new TransformRule);
    int maxGroup = 0;
    if (!parseTemplate(tmpl, rule->pieces, maxGroup, error))
        return false;
    int err = regcomp(&rule->re, pattern.c_str(), REG_EXTENDED | (icase ? REG_ICASE : 0));
    if (err != 0)
    {
        char buf[256];
        regerror(err, &rule->re, buf, sizeof(buf));
        // 'regcomp' frees what it allocated when it fails.
        return error = "invalid regular expression \"" + pattern + "\": " + buf, false;
    }
    rule->compiled = true;
    if ((size_t) maxGroup > rule->re.re_nsub)
        return error = "the template uses group " + std::to_string(maxGroup) + ", the expression has " +
                       std::to_string(rule->re.re_nsub) + " group(s) in parentheses", false;
    out = std::move(rule);
    return true;
}

void rebuildTransformSets( const std::vector<HighlightSection> &sections, const std::string &path,
                           std::string &warnings )
{
    using namespace transformini;
    sets.clear();
    for (auto &section : sections)
    {
        TransformRules rules;
        for (auto &e : section.entries)
        {
            std::shared_ptr<const TransformRule> rule;
            std::string error;
            if (parseTransformRule(e.key, e.value, rule, error))
                rules.push_back(std::move(rule));
            else
                warnings += path + ": line " + std::to_string(e.lineNo) + ": [transform " + section.name +
                            "] " + error + "; rule ignored\n";
        }
        // A section defined again replaces the earlier one (settings.cc
        // keeps only the last).
        sets[section.name] = std::move(rules);
    }
    if (++generation == 0)
        generation = 1;
}

unsigned transformGeneration()
{
    return transformini::generation;
}

bool findTransformSet(const std::string &name, TransformRules &rules)
{
    using namespace transformini;
    auto it = sets.find(lower(name));
    if (it == sets.end())
        return false;
    rules = it->second;
    return true;
}

std::vector<std::string> transformSetNames()
{
    using namespace transformini;
    std::vector<std::string> names;
    for (auto &entry : sets)
        names.push_back(entry.first);
    return names;
}

std::string transformLine(const TransformRules &rules, const std::string &text)
{
    using namespace transformini;
    if (rules.empty())
        return text;
    std::string out = text;
    for (auto &rule : rules)
        out = applyRule(*rule, out);
    return out;
}
