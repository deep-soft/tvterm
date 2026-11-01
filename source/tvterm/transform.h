#ifndef TVTERM_TRANSFORM_H
#define TVTERM_TRANSFORM_H

// Transformations in Log View: sets of rules that change the text shown
// for each line (the lines kept by the source do not change). A set is a
// [transform <name>] section of tvterm.ini; there are no built-in sets.
// Rules (one per line, applied in the order of the file, each one on the
// result of the one before):
//   replace = "<regex>" "<template>" [icase]
// Every match of the regular expression (POSIX extended, like grep -E) is
// replaced by the template; lines without a match stay as they are. In the
// template:
//   \0 ... \9      the whole match / the text of a group (parentheses)
//   ${n}           the same as \n
//   ${n:>W}        group n aligned to the right in W characters
//   ${n:<W}        group n aligned to the left in W characters
//   ${n:>W!}       the same, a longer value is cut to W characters
//   \t             a tab
//   \\  \$         a backslash, a dollar
// An empty template ("") removes the match. Inside the quotes, \" is a
// quote (in the expression and in the template).
// Order in Log View: the filters (include / exclude) and the alerts use the
// original line; the transformation gives the text shown; the highlight,
// the search, copy and save use the text shown.

#include "highlight.h" // HighlightSection: the same layout for [transform <name>].

#include <memory>
#include <string>
#include <vector>

struct TransformRule;
using TransformRules = std::vector<std::shared_ptr<const TransformRule>>;

// Rebuilds the sets from 'sections' (read from 'path'). Problems (invalid
// rules, which are ignored) are added to 'warnings', one per line.
void rebuildTransformSets( const std::vector<HighlightSection> &sections, const std::string &path,
                           std::string &warnings );

// Changes at every rebuild (0: never built).
unsigned transformGeneration();

// The rules of the set 'name'. False if there is no such set.
bool findTransformSet(const std::string &name, TransformRules &rules);

// Names of the sets, sorted.
std::vector<std::string> transformSetNames();

// One rule: 'key' (only 'replace') and its value. False, with 'error' set,
// if the rule is invalid.
bool parseTransformRule( const std::string &key, const std::string &value,
                         std::shared_ptr<const TransformRule> &rule, std::string &error );

// The text shown for 'text': the rules applied in order.
std::string transformLine(const TransformRules &rules, const std::string &text);

#endif // TVTERM_TRANSFORM_H
