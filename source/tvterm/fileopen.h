#ifndef TVTERM_FILEOPEN_H
#define TVTERM_FILEOPEN_H

// Opening files: in the tvterm editor, in a viewer (a Log View with fixed
// lines, title "View: <file>") or with the programs set in tvterm.ini
// ('external-viewer', 'external-editor'), which run in a new terminal
// window that closes when they end.
//
// A file larger than 'large-file' (tvterm.ini, [general]) asks first: the
// external programs (if set), tvterm (Open) or Cancel.

#include <string>
#include <vector>

class TFileDialog;

namespace fileopen
{

enum class Target { Editor, Viewer };

// Opens 'path' in tvterm (Ctrl-B O, Save > Open after saving).
void open(const std::string &path, Target target);

// The large file check of open() alone (E65, the editor's Open in the same
// window): true when 'path' is to be opened in tvterm; false when it was
// given to an external program, or Cancel.
bool acceptInTvterm(const std::string &path, Target target);

// More... > View file: the external viewer if set (without the size
// check), otherwise the tvterm viewer.
void view(const std::string &path);

// The command for an external program: '%f' replaced by the path quoted
// for the shell (every '%f'); without '%f', the path is added at the end.
std::string externalCommand(const std::string &tmpl, const std::string &path);

// History lists of the Name field of the file dialogs. Open, View file
// and the editor's Save file as get the 'file-filters' patterns; the
// profile dialogs have their own lists.
enum : unsigned char
{
    hiOpenFile = 100,
    hiSaveFileAs = 101,
    hiViewFile = 102,
    hiLoadProfile = 103,
    hiSaveProfile = 104,
};

// Adds to those lists the patterns they do not have yet (at startup and
// when tvterm.ini is reloaded). The first pattern is added last, so it is
// the most recent entry (at the bottom of the list).
void addFileFilters(const std::vector<std::string> &filters);

// The filters used before in the file dialogs' Filter field, newest first
// (saved between runs: histfile.h).
std::vector<std::string> &usedFileFilters();

// The initial filter of Ctrl-B O and View file: the first 'file-filters'.
std::string firstFileFilter();

// The file dialogs of tvterm (Open, View file, Save file as, Load / Save
// Profile): a TVTermFileDialog (directory completion) with the size of
// 'file-dialog-size' and the columns of 'file-list-columns' (tvterm.ini).
// A dialog resized by the user gives its size to the next ones, until
// tvterm exits or 'file-dialog-size' changes.
//
// Above the list of files, one row: [ ] Hidden (names beginning with '.'),
// Filter (patterns separated by spaces; its list: 'file-filters' and the
// filters used before) and Folder (the listed directory; its list: Home,
// Profiles, Settings and 'file-dirs'). Enter in Filter or Folder applies
// it and moves to the list; Tab applies a changed value. Folder is left
// out when the row is too narrow. Hidden and the filters used before are
// kept until tvterm exits. Directories are listed with '/'.
TFileDialog *newFileDialog(const std::string &wildCard, const char *title,
                           unsigned short options, unsigned char histId);

// Lists 'dir' in a dialog made by newFileDialog with fdNoLoadDir.
void setDialogDirectory(TFileDialog *d, const std::string &dir);

// The path quoted for the shell ('...', with ' written as '\'').
std::string shellQuote(const std::string &path);

} // namespace fileopen

#endif // TVTERM_FILEOPEN_H
