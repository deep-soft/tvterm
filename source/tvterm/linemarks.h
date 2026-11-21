#ifndef TVTERM_LINEMARKS_H
#define TVTERM_LINEMARKS_H

#include <string>
#include <vector>
#include <stddef.h>

// Marks of the lines changed in an editor window since its file was loaded
// (E111, item 52; "Modified lines"), kept per line of the text and moved
// with the lines. No tvision here, so it can be tested alone.
//
// A change is given as tvision's editor text before it ('before'), the
// position 'at', the 'length' bytes removed there and the 'text' put in
// their place (EditUndo's steps, also undo / redo and changes made from
// outside). The lines touched by the change (from the start of the line of
// 'at' to the end of the line of 'at + length') are compared with the lines
// that replace them: lines that are the same (at the start, at the end, or
// in the middle with the longest common subsequence) keep their mark; of
// the other ones, as many as were there before are changed (an inserted line
// stays inserted), the rest are inserted; lines that disappear lose their
// mark (deleted lines leave nothing). So Enter at the end of a line marks
// only the new line, Enter in the middle marks the line changed and the new
// one inserted, joining two lines marks the result changed (inserted when
// the first one was), and a Replace all marks only the lines it changed.
// Line ends: LF, CRLF or a lone CR (as tvision's editor).

namespace linemarks
{

enum Mark : unsigned char
{
    mNone = 0,
    mChanged = 1,
    mInserted = 2,
};

// 'marks': one entry per line of 'before' (shorter: the missing lines have
// no mark). After the call, one entry per line of the new text up to the
// last marked line (it may be shorter than the text; never longer).
void update( std::vector<unsigned char> &marks, const std::string &before,
             size_t at, size_t length, const std::string &text );

// The mark of line 'line' (0-based), mNone outside 'marks'.
inline Mark markOf(const std::vector<unsigned char> &marks, long line)
{
    return line >= 0 && (size_t) line < marks.size() ? (Mark) marks[line] : mNone;
}

} // namespace linemarks

#endif // TVTERM_LINEMARKS_H
