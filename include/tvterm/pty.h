#ifndef TVTERM_PTY_H
#define TVTERM_PTY_H

#include <stddef.h>

#if !defined(_WIN32)
#include <sys/types.h>
#else
// Use Turbo Vision's windows.h, which omits several unneeded or unwanted
// definitions.
#include <tvision/compat/windows/windows.h>
#endif

template <class T>
class TSpan;
class TPoint;

namespace tvterm
{

struct PtyDescriptor
{
#if !defined(_WIN32)
    int masterFd;
    pid_t clientPid;
#else
    HANDLE hMasterRead;
    HANDLE hMasterWrite;
    HPCON hPseudoConsole;
    HANDLE hClientProcess;
#endif
};

struct EnvironmentVar
{
    const char *name;
    const char *value;
};

// Optional settings for the program started in the terminal.
struct ClientOptions
{
    // Command line to run with the user's shell ('$SHELL -c command').
    // If null or empty, an interactive shell is started.
    const char *command {nullptr};
    // Initial working directory. If null or empty, the current one is kept.
    const char *workingDir {nullptr};
    // If true, an interactive shell is started after 'command' finishes
    // (also when it is interrupted with Ctrl+C), instead of leaving the
    // terminal disconnected.
    bool keepOpen {true};
};

bool createPty( PtyDescriptor &ptyDescriptor,
                TPoint size,
                TSpan<const EnvironmentVar> environment,
                void (&onError)(const char *reason),
                const ClientOptions &options = {} ) noexcept;

class PtyMaster
{
    PtyDescriptor d;

public:

    PtyMaster(PtyDescriptor ptyDescriptor) noexcept;

    bool readFromClient(TSpan<char> data, size_t &bytesRead) noexcept;
    bool writeToClient(TSpan<const char> data) noexcept;
    void resizeClient(TPoint size) noexcept;
    void disconnect() noexcept;
};

inline PtyMaster::PtyMaster(PtyDescriptor ptyDescriptor) noexcept :
    d(ptyDescriptor)
{
}

} // namespace tvterm

#endif // TVTERM_PTY_H
