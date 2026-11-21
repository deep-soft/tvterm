#include "linemarks.h"

#include <algorithm>
#include <stdint.h>

namespace linemarks
{

namespace
{

// The start of the line holding position 'at' of 's'.
size_t lineStart(const std::string &s, size_t at)
{
    size_t i = at;
    while (i > 0)
    {
        char c = s[i - 1];
        if (c == '\n')
            break;
        if (c == '\r' && (i >= s.size() || s[i] != '\n'))
            break;
        --i;
    }
    return i;
}

// The end of the line holding position 'p' of 's' (its line end, or the
// end of the text).
size_t lineEnd(const std::string &s, size_t p)
{
    while (p < s.size() && s[p] != '\n' && s[p] != '\r')
        ++p;
    return p;
}

// The line ends in s[0, end) (a CR followed by LF counts once).
size_t countBreaks(const std::string &s, size_t end)
{
    size_t n = 0;
    for (size_t i = 0; i < end; ++i)
        if (s[i] == '\n' || (s[i] == '\r' && (i + 1 >= s.size() || s[i + 1] != '\n')))
            ++n;
    return n;
}

// The lines of 's' without their line ends (always at least one).
std::vector<std::string> split(const std::string &s)
{
    std::vector<std::string> lines;
    size_t start = 0;
    for (size_t i = 0; i < s.size(); ++i)
        if (s[i] == '\n' || s[i] == '\r')
        {
            lines.emplace_back(s, start, i - start);
            if (s[i] == '\r' && i + 1 < s.size() && s[i + 1] == '\n')
                ++i;
            start = i + 1;
        }
    lines.emplace_back(s, start, s.size() - start);
    return lines;
}

// A line that took the place of a line with mark 'old'.
unsigned char changedFrom(unsigned char old)
{
    return old == mInserted ? mInserted : mChanged;
}

// Lines old[oi, oi + a) became neu[ni, ni + b), nothing in common.
void gap( const std::vector<unsigned char> &oldMarks, size_t oi, size_t a,
          std::vector<unsigned char> &res, size_t ni, size_t b )
{
    for (size_t k = 0; k < b; ++k)
        res[ni + k] = k < a ? changedFrom(oldMarks[oi + k]) : (unsigned char) mInserted;
}

// Above this (lines before x lines after) the middle of a change is not
// compared line by line (the table of the common subsequence).
const size_t lcsLimit = 1000000;

} // namespace

void update( std::vector<unsigned char> &marks, const std::string &before,
             size_t at, size_t length, const std::string &text )
{
    at = std::min(at, before.size());
    length = std::min(length, before.size() - at);
    size_t ls = lineStart(before, at);
    size_t le = lineEnd(before, at + length);
    // One more line on each side: the line ends next to the change (a CR
    // that meets an LF, or no longer does) are then inside the block, so the
    // lines are counted as in the whole text.
    if (ls > 0)
        ls = lineStart(before, ls - 1);
    if (le < before.size())
    {
        size_t next = le + (before[le] == '\r' && le + 1 < before.size() && before[le + 1] == '\n' ? 2 : 1);
        le = lineEnd(before, next);
    }
    size_t first = countBreaks(before, ls);

    std::string newBlock;
    newBlock.reserve(le - ls - length + text.size());
    newBlock.append(before, ls, at - ls);
    newBlock += text;
    newBlock.append(before, at + length, le - at - length);

    std::vector<std::string> oldLines = split(before.substr(ls, le - ls));
    std::vector<std::string> newLines = split(newBlock);
    size_t m = oldLines.size(), n = newLines.size();

    std::vector<unsigned char> oldMarks(m);
    for (size_t k = 0; k < m; ++k)
        oldMarks[k] = markOf(marks, (long) (first + k));

    std::vector<unsigned char> res(n, mNone);
    size_t top = 0;
    while (top < m && top < n && oldLines[top] == newLines[top])
    {
        res[top] = oldMarks[top];
        ++top;
    }
    size_t bottom = 0;
    while ( bottom < m - top && bottom < n - top &&
            oldLines[m - 1 - bottom] == newLines[n - 1 - bottom] )
    {
        res[n - 1 - bottom] = oldMarks[m - 1 - bottom];
        ++bottom;
    }
    size_t om = m - top - bottom, nm = n - top - bottom;
    if (om == nm)
    {
        // Same number of lines (typing, Replace all...): line by line.
        for (size_t k = 0; k < nm; ++k)
            res[top + k] = oldLines[top + k] == newLines[top + k] ? oldMarks[top + k]
                                                                  : changedFrom(oldMarks[top + k]);
    }
    else if (om > 0 && nm > 0 && om * nm <= lcsLimit)
    {
        // The longest common subsequence of the middle lines.
        std::vector<uint32_t> len((om + 1) * (nm + 1), 0);
        auto L = [&] (size_t i, size_t j) -> uint32_t & { return len[i * (nm + 1) + j]; };
        for (size_t i = om; i-- > 0;)
            for (size_t j = nm; j-- > 0;)
                L(i, j) = oldLines[top + i] == newLines[top + j] ? L(i + 1, j + 1) + 1
                                                                 : std::max(L(i + 1, j), L(i, j + 1));
        size_t i = 0, j = 0, gi = 0, gj = 0;
        while (i < om && j < nm)
        {
            if (oldLines[top + i] == newLines[top + j] && L(i, j) == L(i + 1, j + 1) + 1)
            {
                gap(oldMarks, top + gi, i - gi, res, top + gj, j - gj);
                res[top + j] = oldMarks[top + i];
                gi = ++i;
                gj = ++j;
            }
            else if (L(i + 1, j) >= L(i, j + 1))
                ++i;
            else
                ++j;
        }
        gap(oldMarks, top + gi, om - gi, res, top + gj, nm - gj);
    }
    else
        gap(oldMarks, top, om, res, top, nm);

    // Put the new marks in place of the old ones; nothing to keep for lines
    // that have no mark after the last marked one.
    bool any = std::any_of(res.begin(), res.end(), [] (unsigned char c) { return c != mNone; });
    if (marks.size() <= first && !any)
        return;
    if (marks.size() < first + m)
        marks.resize(first + m, mNone);
    marks.erase(marks.begin() + first, marks.begin() + first + m);
    marks.insert(marks.begin() + first, res.begin(), res.end());
    while (!marks.empty() && marks.back() == mNone)
        marks.pop_back();
}

} // namespace linemarks
