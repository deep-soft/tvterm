#include "nocomments.h"

namespace nocomments
{

namespace
{

struct Line
{
    size_t start, length;   // without the line end
    std::string end;        // "\n", "\r\n", "\r" or "" (the last line)
};

std::vector<Line> splitLines(const std::string &s)
{
    std::vector<Line> lines;
    size_t start = 0, i = 0;
    while (i < s.size())
    {
        char c = s[i];
        if (c == '\n' || c == '\r')
        {
            size_t endLen = c == '\r' && i + 1 < s.size() && s[i + 1] == '\n' ? 2 : 1;
            lines.push_back({start, i - start, s.substr(i, endLen)});
            i += endLen;
            start = i;
        }
        else
            ++i;
    }
    // The last line (empty after a final line end): tvision counts it too.
    lines.push_back({start, s.size() - start, std::string()});
    return lines;
}

// The first character that is not a space or a tab, 0 when there is none.
char firstChar(const std::string &s, const Line &l)
{
    for (size_t i = l.start; i < l.start + l.length; ++i)
        if (s[i] != ' ' && s[i] != '\t')
            return s[i];
    return 0;
}

} // namespace

View build(const std::string &full)
{
    View v;
    std::vector<Line> lines = splitLines(full);
    v.originalLines = (long) lines.size();
    std::string firstEnd = "\n";
    for (const Line &l : lines)
        if (!l.end.empty())
        {
            firstEnd = l.end;
            break;
        }
    bool header = true;
    // The line end of the last line put in the view: a line kept without
    // one (the last line of the text) gets none, as in the text.
    for (size_t n = 0; n < lines.size(); ++n)
    {
        const Line &l = lines[n];
        char first = firstChar(full, l);
        if (header)
        {
            if (l.length >= 2 && full[l.start] == '#' && full[l.start + 1] == '#')
            {
                v.text.append(full, l.start, l.length);
                v.text += l.end;
                v.lines.push_back((long) n);
                continue;
            }
            if (first == 0)
                continue;
            header = false;
        }
        if (first == 0 || first == '#' || first == ';')
            continue;
        if (first == '[' && !v.lines.empty())
        {
            v.text += l.end.empty() ? firstEnd : l.end;
            v.lines.push_back(-1);
        }
        v.text.append(full, l.start, l.length);
        v.text += l.end;
        v.lines.push_back((long) n);
    }
    // A text that ends with a line end has one more (empty) line in the
    // editor: the view keeps it, without a line of the whole text, so that
    // the editor's line count and 'lines' match.
    if (!v.lines.empty() && !v.text.empty() && (v.text.back() == '\n' || v.text.back() == '\r'))
        v.lines.push_back(-1);
    if (v.lines.empty())
        v.lines.push_back(-1);
    return v;
}

} // namespace nocomments
