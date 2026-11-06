#define Uses_TEditor
#include <tvision/tv.h>

#include "editundo.h"

#include <string.h>
#include <stdint.h>
#include <algorithm>

namespace editundo
{

const size_t block = 4096;

// Equal bytes at the start of 'a' and 'b' (at most 'n').
static size_t commonPrefix(const char *a, const char *b, size_t n)
{
    size_t i = 0;
    while (i + block <= n && memcmp(a + i, b + i, block) == 0)
        i += block;
    while (i < n && a[i] == b[i])
        ++i;
    return i;
}

// Equal bytes before 'aEnd' and 'bEnd' (at most 'n').
static size_t commonSuffix(const char *aEnd, const char *bEnd, size_t n)
{
    size_t i = 0;
    while (i + block <= n && memcmp(aEnd - i - block, bEnd - i - block, block) == 0)
        i += block;
    while (i < n && aEnd[-1 - (ptrdiff_t) i] == bEnd[-1 - (ptrdiff_t) i])
        ++i;
    return i;
}

// The text of the editor between 'from' and 'to' (its buffer has a gap
// at the cursor).
static std::string editorRange(const TEditor &e, size_t from, size_t to)
{
    std::string s;
    s.reserve(to - from);
    size_t cur = e.curPtr;
    if (from < cur)
        s.append(e.buffer + from, std::min(to, cur) - from);
    if (to > cur)
    {
        size_t a = std::max(from, cur);
        s.append(e.buffer + a + e.gapLen, to - a);
    }
    return s;
}

static std::string editorText(const TEditor &e)
{
    return editorRange(e, 0, e.bufLen);
}

} // namespace editundo

using namespace editundo;

size_t EditUndo::stepSize(const Step &s)
{
    return sizeof(Step) + s.removed.size() + s.inserted.size();
}

void EditUndo::reset(TEditor &e)
{
    steps.clear();
    pos = 0;
    memory = 0;
    savedPos = e.modified ? -1 : 0;
    shadow = editorText(e);
    shadowCursor = e.curPtr;
    initialized = true;
}

bool EditUndo::findChange( TEditor &e, size_t prefixLimit, size_t &at,
                           std::string &removed, std::string &inserted ) const
// The part of the text that differs from 'shadow': at 'at', 'removed'
// (from 'shadow') became 'inserted'. 'prefixLimit': the change is taken to
// start at or before it (the cursor, for a typed or deleted character
// among equal ones). False: no change.
{
    size_t oldLen = shadow.size(), newLen = e.bufLen;
    const char *segA = e.buffer;
    size_t nA = e.curPtr;
    const char *segB = e.buffer + e.curPtr + e.gapLen;
    size_t nB = e.bufLen - e.curPtr;
    const char *old = shadow.data();

    size_t limit = std::min({oldLen, newLen, prefixLimit});
    size_t n1 = std::min(limit, nA);
    size_t p = commonPrefix(old, segA, n1);
    if (p == n1 && limit > nA)
        p += commonPrefix(old + nA, segB, limit - nA);

    size_t m = std::min(oldLen, newLen) - p;
    size_t m1 = std::min(m, nB);
    size_t s = commonSuffix(old + oldLen, segB + nB, m1);
    if (s == m1 && m > nB)
        s += commonSuffix(old + oldLen - nB, segA + nA, m - nB);

    if (p + s == oldLen && p + s == newLen)
        return false;
    at = p;
    removed.assign(shadow, p, oldLen - s - p);
    inserted = editorRange(e, p, newLen - s);
    return true;
}

void EditUndo::syncSaved(TEditor &e)
{
    // Not modified: the text is the saved one (saved, loaded, or the
    // question on close answered No). Typing after a save is a new step.
    if (!e.modified)
    {
        savedPos = (long) pos;
        if (pos > 0)
            steps[pos - 1].open = false;
    }
}

void EditUndo::addStep(Step &&s)
{
    // A new change: the steps that could be redone are gone.
    while (steps.size() > pos)
    {
        memory -= stepSize(steps.back());
        steps.pop_back();
    }
    if (savedPos > (long) pos)
        savedPos = -1;
    memory += stepSize(s);
    steps.push_back(std::move(s));
    ++pos;
    dropOldest();
}

void EditUndo::dropOldest()
{
    while ( !steps.empty() &&
            ( steps.size() > (size_t) limits.steps ||
              (limits.memory > 0 && memory > (size_t) limits.memory) ) )
    {
        if (pos > 0)
        {
            // The oldest step: the text before it can no longer be reached.
            memory -= stepSize(steps.front());
            steps.pop_front();
            --pos;
            if (savedPos == 0)
                savedPos = -1;
            else if (savedPos > 0)
                --savedPos;
        }
        else
        {
            // Only steps to redo: the newest one goes.
            memory -= stepSize(steps.back());
            steps.pop_back();
            if (savedPos > (long) steps.size())
                savedPos = -1;
        }
    }
}

void EditUndo::setLimits(const Limits &aLimits)
{
    limits = aLimits;
    dropOldest();
}

void EditUndo::before(TEditor &e)
{
    if (!initialized)
    {
        reset(e);
        return;
    }
    size_t at;
    std::string removed, inserted;
    if (findChange(e, SIZE_MAX, at, removed, inserted))
    {
        // Changed from outside the window's events: a step of its own.
        Step s;
        s.at = at;
        s.cursorBefore = std::min(shadowCursor, shadow.size());
        shadow.replace(at, removed.size(), inserted);
        s.removed = std::move(removed);
        s.inserted = std::move(inserted);
        s.cursorAfter = e.curPtr;
        addStep(std::move(s));
    }
    else if (e.curPtr != shadowCursor && pos > 0)
        steps[pos - 1].open = false;
    shadowCursor = e.curPtr;
    syncSaved(e);
}

void EditUndo::after(TEditor &e, Kind kind, bool hadSelection, int replaceCount)
{
    if (!initialized)
    {
        reset(e);
        return;
    }
    size_t cursor = e.curPtr;
    // A typed or deleted character among equal ones: the change is put at
    // the cursor (before or after the key), so that it joins the step.
    size_t prefixLimit = kind == ukOther ? SIZE_MAX : std::min(shadowCursor, cursor);
    size_t at;
    std::string removed, inserted;
    if (!findChange(e, prefixLimit, at, removed, inserted))
    {
        // A cursor move (keys or click) ends the step.
        if (cursor != shadowCursor && pos > 0)
            steps[pos - 1].open = false;
        shadowCursor = cursor;
        syncSaved(e);
        return;
    }
    shadow.replace(at, removed.size(), inserted);

    Step *last = pos > 0 && pos == steps.size() && steps[pos - 1].open ? &steps[pos - 1] : nullptr;
    bool joined = false;
    if (last && !hadSelection && last->kind == kind)
    {
        size_t added = removed.size() + inserted.size();
        if (kind == ukTyping && at == last->at + last->inserted.size())
        {
            last->removed += removed;
            last->inserted += inserted;
            joined = true;
        }
        else if (kind == ukDeleting && inserted.empty() && last->inserted.empty())
        {
            if (at + removed.size() == last->at)
            {
                // Backspace: the text on the left.
                last->removed.insert(0, removed);
                last->at = at;
                joined = true;
            }
            else if (at == last->at)
            {
                // Del: the text on the right.
                last->removed += removed;
                joined = true;
            }
        }
        if (joined)
        {
            last->cursorAfter = cursor;
            memory += added;
            dropOldest();
        }
    }
    if (!joined)
    {
        Step s;
        s.at = at;
        s.removed = std::move(removed);
        s.inserted = std::move(inserted);
        s.cursorBefore = shadowCursor;
        s.cursorAfter = cursor;
        s.kind = kind;
        s.open = kind != ukOther;
        s.replaceCount = replaceCount;
        addStep(std::move(s));
    }
    shadowCursor = cursor;
    syncSaved(e);
}

void EditUndo::apply(TEditor &e, size_t at, size_t length, const std::string &text, size_t cursor)
// Replaces 'length' bytes at 'at' by 'text', exactly as it is: tvision
// converts the line ends of inserted text to the editor's, so the text is
// inserted in parts, each with the line end type of its own line ends.
{
    e.lock();
    auto eol = e.lineEndingType;
    e.setSelect((uint) at, (uint) (at + length), False);
    size_t n = text.size(), start = 0;
    int partType = -1;
    bool first = true;
    auto flush = [&] (size_t end) {
        if (!first && end == start)
            return;
        e.lineEndingType = partType < 0 ? eol : (TEditor::LineEndingType) partType;
        // Offset 0: tvision counts the converted length from the pointer,
        // not from the offset.
        e.insertBuffer(text.data() + start, 0, (uint) (end - start), False, False);
        first = false;
        start = end;
    };
    for (size_t i = 0; i < n;)
    {
        int type;
        size_t len = 1;
        if (text[i] == '\r' && i + 1 < n && text[i + 1] == '\n')
            type = TEditor::eolCrLf, len = 2;
        else if (text[i] == '\r')
            type = TEditor::eolCr;
        else if (text[i] == '\n')
            type = TEditor::eolLf;
        else
        {
            ++i;
            continue;
        }
        if (partType >= 0 && type != partType)
            flush(i);
        partType = type;
        i += len;
    }
    flush(n);
    e.lineEndingType = eol;
    // tvision's own undo data (one level) no longer matches the text.
    e.delCount = 0;
    e.insCount = 0;
    e.setCurPtr((uint) std::min<size_t>(cursor, e.bufLen), 0);
    e.trackCursor(False);
    e.unlock();
    shadow.replace(at, length, text);
    shadowCursor = e.curPtr;
}

static std::string replaceAllText(const char *verb, int count)
{
    return std::string(verb) + ": Replace all (" + std::to_string(count) + " replacement(s)).";
}

bool EditUndo::undo(TEditor &e, std::string &message)
{
    message.clear();
    if (!initialized)
        reset(e);
    if (pos == 0)
    {
        message = "Nothing to undo.";
        return false;
    }
    Step &s = steps[pos - 1];
    s.open = false;
    apply(e, s.at, s.inserted.size(), s.removed, s.cursorBefore);
    --pos;
    e.modified = atSavedState() ? False : True;
    e.update(ufUpdate);
    if (s.replaceCount > 0)
        message = replaceAllText("Undone", s.replaceCount);
    return true;
}

bool EditUndo::redo(TEditor &e, std::string &message)
{
    message.clear();
    if (!initialized)
        reset(e);
    if (pos == steps.size())
    {
        message = "Nothing to redo.";
        return false;
    }
    if (pos > 0)
        steps[pos - 1].open = false;
    Step &s = steps[pos];
    apply(e, s.at, s.removed.size(), s.inserted, s.cursorAfter);
    ++pos;
    e.modified = atSavedState() ? False : True;
    e.update(ufUpdate);
    if (s.replaceCount > 0)
        message = replaceAllText("Redone", s.replaceCount);
    return true;
}
