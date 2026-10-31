#ifndef TVTERM_HIGHLIGHT_H
#define TVTERM_HIGHLIGHT_H

// Highlighting in Log View: sets of rules that color parts of the lines.
// A set is a [highlight <name>] section of tvterm.ini; the sets built into
// tvterm (see builtinHighlightText) are replaced by a section with the same
// name. Rules (one per line, applied in the order of the file):
//   line   = <color> "<regex>" [icase]   the whole line (the first that matches)
//   match  = <color> "<regex>" [icase]   every match
//   groups = <color> "<regex>" [icase]   the text in parentheses, every match
//   hash   = "<regex>" [icase]           a color derived from the value (the
//                                        parentheses or, without them, the match)
//   include = <set> ...                  the rules of another set, at that place
// Colors: black red green yellow blue magenta cyan white, bright-<color>,
// optionally 'on <color>' for the background. Where rules overlap, the first
// one wins ('line' only gives the base color). Inside the quotes, \" is a
// quote; every other backslash is passed to the regular expression (POSIX
// extended, like grep -E).

#include <memory>
#include <string>
#include <vector>

// A [highlight <name>] section, as read from tvterm.ini.
struct HighlightSection
{
    struct Entry
    {
        int lineNo {0};
        std::string key, value;
    };

    std::string name;
    int lineNo {0};
    std::vector<Entry> entries;
};

struct HighlightRule;
using HighlightRules = std::vector<std::shared_ptr<const HighlightRule>>;

// Rebuilds the sets: the built-in ones, then 'sections' (from 'path').
// Problems (invalid rules, which are ignored) are added to 'warnings', one
// per line.
void rebuildHighlightSets( const std::vector<HighlightSection> &sections, const std::string &path,
                           std::string &warnings );

// Changes at every rebuild (0: never built).
unsigned highlightGeneration();

// The rules of the set 'name', its includes expanded. False if there is no
// such set.
bool findHighlightSet(const std::string &name, HighlightRules &rules);

// Names of the sets, sorted.
std::vector<std::string> highlightSetNames();

// "zimbra-mta kv" -> {"zimbra-mta", "kv"} (spaces or commas).
std::vector<std::string> splitHighlightNames(const std::string &text);

// Colors of the bytes of 'text': -1 (none) or a color attribute (BIOS
// style: background * 16 + foreground).
void highlightLine(const HighlightRules &rules, const std::string &text, std::vector<int> &colors);

// Highlight Words (Log View, F4 > Highlight > Words): the words of 'text',
// separated by commas, spaces at their ends removed, repeated words dropped
// (case ignored). A word may hold spaces. As in the alert keywords, a
// backslash makes the next character plain: \, a comma, \\ a backslash,
// '\ ' a space kept at an end.
std::vector<std::string> splitHighlightWords(const std::string &text);

// One rule per word of 'text' (see splitHighlightWords): every occurrence,
// case ignored for ASCII letters (as in the alert keywords), a color of
// its own (a palette of 8, repeated from the 9th word). Where words
// overlap, the first one in the list wins.
HighlightRules wordHighlightRules(const std::string &text);

// The built-in sets, in the format of tvterm.ini.
const char *builtinHighlightText();

#endif // TVTERM_HIGHLIGHT_H
