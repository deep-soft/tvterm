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

namespace fileopen
{

enum class Target { Editor, Viewer };

// Opens 'path' in tvterm (Ctrl-B O, Save > Open after saving).
void open(const std::string &path, Target target);

// More... > View file: the external viewer if set (without the size
// check), otherwise the tvterm viewer.
void view(const std::string &path);

// The command for an external program: '%f' replaced by the path quoted
// for the shell (every '%f'); without '%f', the path is added at the end.
std::string externalCommand(const std::string &tmpl, const std::string &path);

// The path quoted for the shell ('...', with ' written as '\'').
std::string shellQuote(const std::string &path);

} // namespace fileopen

#endif // TVTERM_FILEOPEN_H
