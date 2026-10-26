#ifndef TVTERM_LOGSRC_H
#define TVTERM_LOGSRC_H

// The source of a Log View window: a command (e.g. 'tail -F file') whose
// output is kept as a list of lines. Log Views with the same command share
// one source: one process and one buffer.
//
// The command runs with the user's shell ('$SHELL -c command', like the
// commands of terminals) in a pseudo-terminal, so that programs write each
// line as soon as it is ready (in a pipe, many programs, e.g. awk, keep
// their output until a few KB are collected). Escape sequences (colors) and
// carriage returns are removed; tabs are kept.
//
// The sources are read in the application's idle time (LogSource::pollAll),
// without threads.

#include <chrono>
#include <deque>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <stdint.h>
#include <sys/types.h>

struct LogLine
{
    std::string text;
    // A line written by tvterm ('-- source ended (exit 1) --'): always
    // shown, never filtered, no alerts.
    bool marker {false};
};

class LogSource
{
public:

    using Clock = std::chrono::steady_clock;

    // The source running 'command', started if there is none yet.
    static std::shared_ptr<LogSource> acquire(const std::string &command);
    // Reads the output of every source. Called in the application's idle
    // time. Returns true if any source received something.
    static bool pollAll();
    // Number of sources whose process is running.
    static size_t countRunning();
    // Commands of the current sources (for the history of the dialog).
    static std::vector<std::string> commands();

    ~LogSource();

    const std::string &getCommand() const noexcept { return command; }
    bool isRunning() const noexcept { return fd >= 0 || pid > 0; }
    long getPid() const noexcept { return pid; }
    // When the process was started (or restarted).
    Clock::time_point getStartTime() const noexcept { return startTime; }

    // Lines kept: sequence numbers [firstSeq(), endSeq()). A line keeps its
    // number while it is in the buffer; old lines are dropped from the front.
    uint64_t firstSeq() const noexcept { return first; }
    uint64_t endSeq() const noexcept { return first + lines.size(); }
    const LogLine &line(uint64_t seq) const noexcept { return lines[seq - first]; }

    // Options of each window using the source ('user'): the buffer keeps the
    // largest number of lines asked for; the process is restarted after the
    // shortest delay asked for (0: not restarted).
    void setUserOptions(const void *user, int maxLines, int autoRestartSeconds);
    void removeUser(const void *user);

    // Stops the process (if running) and starts it again (R key).
    void restart();

private:

    struct UserOptions { int maxLines; int autoRestart; };

    std::string command;
    int fd {-1};
    pid_t pid {0};
    Clock::time_point startTime;
    // The output ended: waiting for the exit status / for a restart.
    bool ended {false};
    Clock::time_point endTime;
    std::deque<LogLine> lines;
    uint64_t first {0};
    std::map<const void *, UserOptions> users;
    size_t maxLines {50000};
    int autoRestart {0};

    // Line being received (no '\n' yet) and the escape sequence parser.
    std::string partial;
    Clock::time_point lastData;
    enum class Esc : uint8_t { None, Start, Csi, Str, StrEsc, Charset } esc {Esc::None};

    explicit LogSource(const std::string &aCommand);
    void start();
    // 'why': text of the marker line ("" for none).
    void stop(const char *why);
    bool poll();
    void addData(const char *data, size_t len);
    void endLine();
    void pushLine(std::string text, bool marker);
    void trim();
    void finishIfExited();
};

#endif // TVTERM_LOGSRC_H
