#ifndef TVTERM_DIRCOMPL_H
#define TVTERM_DIRCOMPL_H

// Directory completion for the Name field of file dialogs.
//
// While a path is typed, a list of the matching subdirectories opens below
// the field by itself: the directories of the part before the last '/'
// whose names begin with the text after it (e.g. "/var/l" -> lib, local,
// lock, log). Only directories are listed; hidden ones only after a '.' is
// typed. A path without '/' is taken from the dialog's directory, "~/" from
// the home directory. Names are compared case sensitively.
//
//   Down          (in the field, list open) moves into the list
//   Enter         (in the list) adds the directory and '/' to the field and
//                 lists its subdirectories (also: double click on a row)
//   Up            (on the first row of the list) back to the field
//   other keys    (in the list) back to the field, where they are handled
//   Esc           closes the list (not the dialog)
//   Tab           closes the list and moves to the next field
//   Enter         (in the field) closes the list and works as before
//
// With the list closed, the keys work as before (Down opens the history).
// The list of files in the dialog does not change while typing. The cursor
// stays in the field also while moving in the list, so the field's history
// records only what it recorded before.

#define Uses_TEvent
#define Uses_TGroup
#define Uses_TInputLine
#define Uses_TListViewer
#define Uses_TScrollBar
#define Uses_TFileDialog
#define Uses_TFileInputLine
#include <tvision/tv.h>

#include <functional>
#include <string>
#include <vector>

namespace dircompl
{

// Sent by dlgclip to the owner of an input line after text was pasted into it.
enum : ushort { cmInputLinePasted = 3500 };

// The names (without '/') of the subdirectories that complete 'text';
// 'baseDir' is the directory of a relative path. Sorted, at most
// 'maxItems'. Empty when the text is empty, has '*' or '?' after the last
// '/', or the directory cannot be read.
std::vector<std::string> matches(const std::string &text, const std::string &baseDir,
                                 const std::string &home, size_t maxItems = 2000);

// 'text' with its last component replaced by 'name' + '/'.
std::string complete(const std::string &text, const std::string &name);

// A TFileDialog directory ("C:\home\x\" or "/home/x/") as a path with '/'.
std::string unixDir(const char *directory);

class DirList;

// Adds the completion to a dialog. The dialog calls handleEvent() around its
// own event handling (see TVTermFileDialog).
class Completion
{
public:
    // 'owner' is the dialog, 'input' its field; 'baseDir' gives the
    // directory of relative paths at the time of typing.
    void attach(TGroup *owner, TInputLine *input, std::function<std::string()> baseDir);

    // Handles 'ev' with 'handle' (the dialog's own handleEvent) in between.
    void handleEvent(TEvent &ev, const std::function<void(TEvent &)> &handle);

private:
    TGroup *owner {nullptr};
    TInputLine *input {nullptr};
    DirList *list {nullptr};
    TScrollBar *scrollBar {nullptr};
    std::function<std::string()> baseDir;

    bool before(TEvent &ev);
    void after(const std::string &oldText, bool wasFocused);
    bool isOpen() const;
    void refresh();
    void close();
    void setActive(bool on);
    void accept();
};

} // namespace dircompl

// TFileDialog with directory completion in the Name field.
class TVTermFileDialog : public TFileDialog
{
public:
    TVTermFileDialog(TStringView wildCard, TStringView title, TStringView inputName,
                     ushort options, uchar histId) noexcept;
    void handleEvent(TEvent &ev) override;

private:
    dircompl::Completion completion;
};

#endif // TVTERM_DIRCOMPL_H
