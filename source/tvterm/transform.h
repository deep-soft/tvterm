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
//   fields = k1 k2:>W k3:<W! ...
//   fields-format = pairs | values
// The values of the 'key=value' words of the line, in the order given (a
// value in quotes may hold spaces); 'pairs' (default) shows key=value,
// 'values' only the values, separated by a tab (by a space when a field has
// a width). ':>W' / ':<W' align the value right / left in W characters, '!'
// cuts a longer value. A missing field is left out (with a width: blanks);
// a line without any of the fields stays as it is. One 'fields' per set,
// applied before the replace rules.
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

// One rule: 'key' ('replace' or 'fields', the latter as pairs) and its
// value. False, with 'error' set, if the rule is invalid.
bool parseTransformRule( const std::string &key, const std::string &value,
                         std::shared_ptr<const TransformRule> &rule, std::string &error );

// The text shown for 'text': the rules applied in order.
std::string transformLine(const TransformRules &rules, const std::string &text);

#endif // TVTERM_TRANSFORM_H
