#ifndef TVTERM_HISTFILE_H
#define TVTERM_HISTFILE_H

#include <string>
#include <vector>

// The history lists of the input fields (the lists opened by the arrow
// next to a field), kept between runs in a file written by tvterm:
// $XDG_CONFIG_HOME/tvterm/history (or ~/.config/tvterm/history), next to
// tvterm.ini, mode 600.
//
// Saved lists (at most maxEntries each, the newest): Log View Command,
// Include, Exclude, Highlight (sets and words) and Transform, the Name
// field of Open file / Save file as / View file / Load Profile / Save
// Profile As, editor Find / Replace / Go to line, the Window List Filter
// and the Filter of the file dialogs. Not saved: the Folder list and the
// names of the sets and the 'file-filters' patterns offered in the lists
// (they come from tvterm.ini).
//
// Two tvterm instances share the file: each one adds what it has to what
// the file already holds (an entry deleted with Ctrl-Del in this instance
// is not taken back from the file).
namespace histfile
{

const int maxEntries = 30;

// History id of the Window List's Filter field (the other ids are where
// their fields are: logview.cc, fileopen.h, editwnd.cc).
enum : unsigned char { hiWindowListFilter = 108 };

// Size of tvision's history buffer, shared by all the lists (tvision's
// default is 1024 bytes): set before the application is created.
void enlargeBuffer();

// Reads the file into the lists (at startup, before tvterm.ini adds the
// 'file-filters' patterns).
void load();

// Writes the file if a list changed since the last write (checked at most
// once per second; called from idle).
void poll();

// Writes the file now (at exit).
void save();

// Ctrl-Del in an open history list: deletes 'text' from list 'id' and
// writes the file. Returns false if the list does not have it.
bool remove(unsigned char id, const std::string &text);

// Ctrl-Del in a list that tvterm keeps itself (the Filter of the file
// dialogs, the Log View fields): 'text' was deleted from 'list'; writes the
// file.
void removed(const std::vector<std::string> &list, const std::string &text);

// The path of the file.
std::string path();

} // namespace histfile

#endif // TVTERM_HISTFILE_H
