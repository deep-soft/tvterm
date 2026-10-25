#ifndef TVTERM_PROCINFO_H
#define TVTERM_PROCINFO_H

// Reading the state of the processes running in a terminal (Linux /proc).

#include <string>

namespace tvterm { struct ClientProcessInfo; }

namespace procinfo
{

// Target of a symbolic link (e.g. /proc/<pid>/cwd), or empty.
std::string readLink(const std::string &path);

// Command line of a process, with its arguments quoted for the shell,
// or empty.
std::string readCommandLine(long pid);

// What is running in the foreground of a terminal.
struct ForegroundInfo
{
    // The command(s) in the foreground, e.g. "tail -f /var/log/mail.log"
    // or "tail -f x | grep y". For an idle shell, the shell's name.
    std::string command;
    // An interactive shell is waiting for input (nothing is running).
    bool idleShell {false};
    // far2l is running (it keeps running in the background when tvterm
    // quits, and can be attached again).
    bool far2l {false};
};

// Describes the foreground of a terminal. Returns false if the processes
// cannot be inspected (no /proc).
bool describeForeground(const tvterm::ClientProcessInfo &info, ForegroundInfo &out);

} // namespace procinfo

#endif // TVTERM_PROCINFO_H
