#ifndef TVTERM_EDITUNDO_H
#define TVTERM_EDITUNDO_H

#define Uses_TEditor
#include <tvision/tv.h>

#include <string>
#include <deque>
#include <stddef.h>

// Multi-level undo and redo for a tvision TEditor, without changing
// tvision. The editor window calls before() and after() around each event;
// the text is compared with a copy of it (the text after the last event)
// and the changed part becomes a step: its position, the text removed and
// the text inserted. Only the changed part is kept in the steps; the copy
// of the whole text is one per editor.
//
// Steps: typing (letters, overwrite too) and deleting (Backspace, Del,
// word deletes) go on in the same step while the cursor stays where the
// last change left it; anything else (Enter, paste, cut, Ctrl-Y, Replace,
// Replace all...) is a step of its own. Changes made from outside the
// window's events (e.g. lines sent by Log View) become a step at the next
// event. A new change clears the steps that could be redone.

class EditUndo
{
public:

    // What the event that made the change was.
    enum Kind : unsigned char
    {
        ukOther,    // a step of its own
        ukTyping,   // a typed character
        ukDeleting, // Backspace, Del, word deletes (no selection)
    };

    struct Limits
    {
        int steps {500};            // 1-100000
        long long memory {20LL * 1024 * 1024}; // bytes; 0: no limit
    };

    // Forgets every step; the text of the editor is the starting point (the
    // saved state, unless the editor is marked as modified).
    void reset(TEditor &e);

    // Before an event: changes made from outside become a step; the saved
    // state follows the editor's 'modified' mark. True when the text had
    // changed.
    bool before(TEditor &e);

    // After an event of kind 'kind' ('hadSelection': the editor had a
    // selection before it). 'replaceCount' > 0: the event was a Replace all
    // with that many replacements (shown when it is undone). True when the
    // event changed the text.
    bool after(TEditor &e, Kind kind, bool hadSelection, int replaceCount = 0);

    // Undo / redo one step. False when there is nothing to do. 'message':
    // the text for the status line (may be empty).
    bool undo(TEditor &e, std::string &message);
    bool redo(TEditor &e, std::string &message);

    // Drops the oldest steps over the limits.
    void setLimits(const Limits &limits);

    // For tests and messages.
    size_t undoCount() const { return pos; }
    size_t redoCount() const { return steps.size() - pos; }
    size_t memoryUsed() const { return memory; }
    bool atSavedState() const { return savedPos >= 0 && (size_t) savedPos == pos; }

private:

    struct Step
    {
        size_t at {0};          // position of the change
        std::string removed;    // text that was there
        std::string inserted;   // text put there
        size_t cursorBefore {0}, cursorAfter {0};
        Kind kind {ukOther};
        bool open {false};      // later typing / deleting may join it
        int replaceCount {0};
    };

    std::deque<Step> steps;
    size_t pos {0};             // steps[0..pos) are done; the rest can be redone
    long savedPos {0};          // pos of the saved text; -1: not reachable
    size_t memory {0};          // bytes held by the steps
    Limits limits;

    std::string shadow;         // the text after the last event
    size_t shadowCursor {0};    // the cursor after the last event
    bool initialized {false};

    static size_t stepSize(const Step &s);
    bool findChange(TEditor &e, size_t prefixLimit, size_t &at, std::string &removed, std::string &inserted) const;
    void addStep(Step &&s);
    void dropOldest();
    void syncSaved(TEditor &e);
    void apply(TEditor &e, size_t at, size_t length, const std::string &text, size_t cursor);
};

#endif // TVTERM_EDITUNDO_H
