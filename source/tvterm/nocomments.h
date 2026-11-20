#ifndef TVTERM_NOCOMMENTS_H
#define TVTERM_NOCOMMENTS_H

#include <string>
#include <vector>

// The text of the editor's "Hide comments" view (E109, item 63), made from
// the whole text (no tvision here, so it can be tested alone).
//
// Removed: blank lines (only spaces and tabs) and comment lines (the first
// character after the spaces and tabs is '#' or ';'). Kept: every other line
// as it is (a line with a comment after its text too), with its own line
// end; the block of '##' lines at the start of the text (each starting with
// '##' in the first column; blank lines inside the block are removed, the
// block ends at the first line that is neither '##' nor blank). A blank
// line is added before each '[section]' line (the first character after the
// spaces is '['), except at the very start. Line ends: LF, CRLF or a lone CR
// (as tvision's editor); an added line takes the line end of the section
// line (or of the first line end of the text, LF without any).

namespace nocomments
{

struct View
{
    std::string text;
    // For each line of 'text': the line of the whole text it comes from
    // (0-based), -1 for an added blank line.
    std::vector<long> lines;
    // The number of lines of the whole text (tvision's count: line ends + 1).
    long originalLines {1};
};

View build(const std::string &full);

} // namespace nocomments

#endif // TVTERM_NOCOMMENTS_H
