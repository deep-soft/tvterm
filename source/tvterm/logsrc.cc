#define Uses_TPoint
#include <tvision/tv.h>

#include "logsrc.h"
#include "app.h"

#include <tvterm/pty.h>

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <algorithm>
#include <iterator>

// Own namespace: this file is compiled together with others (Unity Build).
namespace logsrc
{

using Clock = LogSource::Clock;

// Every source, so that they can be shared and polled.
static std::vector<std::weak_ptr<LogSource>> sources;

// Processes that were asked to stop (SIGHUP) and have not exited yet.
// They are reaped in pollAll(), and killed if they take too long.
struct Stopping { pid_t pid; Clock::time_point since; bool killed; };
static std::vector<Stopping> stopping;

enum : size_t
{
    // Longer lines are cut (e.g. binary data without line breaks).
    maxLineBytes = 64 * 1024,
    // Bytes read from all the sources in one poll, so that the interface
    // stays responsive while a large history arrives (tail -n 50000).
    readBudget = 4 * 1024 * 1024,
};

// A line without '\n' is shown after this time without new data
// (e.g. a prompt).
static const auto partialTimeout = std::chrono::milliseconds(1000);

static void onPtyError(const char *reason)
{
    TVTermApp::note(std::string("log source: ") + reason);
}

static std::string describeStatus(int status)
{
    if (WIFEXITED(status))
        return "exit " + std::to_string(WEXITSTATUS(status));
    if (WIFSIGNALED(status))
    {
        int sig = WTERMSIG(status);
        const char *name = strsignal(sig);
        return "signal " + std::to_string(sig) + (name ? std::string(", ") + name : std::string());
    }
    return "ended";
}

static std::string shortCommand(const std::string &command)
{
    return command.size() > 60 ? command.substr(0, 57) + "..." : command;
}

static void reapStopping()
{
    auto now = Clock::now();
    for (size_t i = 0; i < stopping.size();)
    {
        auto &s = stopping[i];
        pid_t r = waitpid(s.pid, nullptr, WNOHANG);
        if (r == s.pid || (r < 0 && errno == ECHILD))
        {
            stopping.erase(stopping.begin() + i);
            continue;
        }
        if (!s.killed && now - s.since > std::chrono::seconds(2))
        {
            kill(-s.pid, SIGKILL);
            kill(s.pid, SIGKILL);
            s.killed = true;
        }
        ++i;
    }
}

} // namespace logsrc

std::shared_ptr<LogSource> LogSource::acquire(const std::string &command)
{
    using namespace logsrc;
    for (auto &weak : sources)
        if (auto src = weak.lock())
            if (src->command == command)
                return src;
    std::shared_ptr<LogSource> src(new LogSource(command));
    sources.push_back(src);
    return src;
}

std::shared_ptr<LogSource> LogSource::createFixed(const std::string &command, std::vector<LogLine> lines)
{
    // Not in 'sources': not shared, not polled, not counted as running.
    return std::shared_ptr<LogSource>(new LogSource(FixedTag {}, command, std::move(lines)));
}

bool LogSource::pollAll()
{
    using namespace logsrc;
    bool any = false;
    for (size_t i = 0; i < sources.size();)
    {
        if (auto src = sources[i].lock())
        {
            any |= src->poll();
            ++i;
        }
        else
            sources.erase(sources.begin() + i);
    }
    if (!stopping.empty())
        reapStopping();
    return any;
}

size_t LogSource::countRunning()
{
    size_t n = 0;
    for (auto &weak : logsrc::sources)
        if (auto src = weak.lock())
            n += src->isRunning();
    return n;
}

std::vector<std::string> LogSource::commands()
{
    std::vector<std::string> out;
    for (auto &weak : logsrc::sources)
        if (auto src = weak.lock())
            out.push_back(src->command);
    return out;
}

LogSource::LogSource(const std::string &aCommand) :
    command(aCommand)
{
    start();
}

LogSource::LogSource(FixedTag, const std::string &aCommand, std::vector<LogLine> fixedLines) :
    command(aCommand),
    ended(true),
    fixed(true)
{
    lines.assign(std::make_move_iterator(fixedLines.begin()), std::make_move_iterator(fixedLines.end()));
    startTime = lastData = endTime = Clock::now();
}

LogSource::~LogSource()
{
    if (isRunning())
    {
        stop("");
        TVTermApp::note("log source stopped (no window uses it): " + logsrc::shortCommand(command));
    }
}

void LogSource::start()
{
    using namespace tvterm;
    // TERM=dumb: programs should not send colors or cursor movements.
    static const EnvironmentVar env[] = {
        {"TERM", "dumb"},
    };
    PtyDescriptor d {};
    ClientOptions options;
    options.command = command.c_str();
    options.keepOpen = false;
    // Wide, so that programs do not cut their lines.
    TPoint size {512, 50};
    ended = false;
    partial.clear();
    esc = Esc::None;
    startTime = lastData = Clock::now();
    if (!createPty(d, size, TSpan<const EnvironmentVar>(env), logsrc::onPtyError, options))
    {
        ended = true;
        endTime = Clock::now();
        pushLine("-- cannot start the source --", true);
        return;
    }
    fd = d.masterFd;
    pid = d.clientPid;
    int flags = fcntl(fd, F_GETFL);
    if (flags != -1)
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    fcntl(fd, F_SETFD, FD_CLOEXEC);
    TVTermApp::note( "log source started (pid " + std::to_string(pid) + "): " +
                     logsrc::shortCommand(command) );
}

void LogSource::stop(const char *why)
{
    if (!partial.empty())
        endLine();
    if (fd >= 0)
    {
        close(fd);
        fd = -1;
    }
    if (pid > 0)
    {
        // The whole process group (e.g. 'tail -F file | awk ...').
        kill(-pid, SIGHUP);
        kill(pid, SIGHUP);
        if (waitpid(pid, nullptr, WNOHANG) != pid)
            logsrc::stopping.push_back({pid, Clock::now(), false});
        pid = 0;
    }
    ended = true;
    endTime = Clock::now();
    if (why && *why)
        pushLine(why, true);
}

void LogSource::restart()
{
    if (fixed)
        return;
    if (isRunning())
        stop("");
    pushLine("-- source restarted --", true);
    TVTermApp::note("log source restarted: " + logsrc::shortCommand(command));
    start();
}

void LogSource::setUserOptions(const void *user, int aMaxLines, int aAutoRestart)
{
    if (fixed)
        // Never trimmed nor restarted.
        aAutoRestart = 0;
    users[user] = {aMaxLines, aAutoRestart};
    size_t newMax = 0;
    int newRestart = 0;
    for (auto &u : users)
    {
        newMax = std::max(newMax, (size_t) std::max(u.second.maxLines, 1));
        if (u.second.autoRestart > 0 && (newRestart == 0 || u.second.autoRestart < newRestart))
            newRestart = u.second.autoRestart;
    }
    maxLines = newMax;
    autoRestart = newRestart;
    trim();
}

void LogSource::removeUser(const void *user)
{
    users.erase(user);
}

void LogSource::pushLine(std::string text, bool marker)
{
    lines.push_back({std::move(text), marker});
    trim();
}

void LogSource::trim()
{
    if (fixed)
        return;
    while (lines.size() > maxLines)
    {
        lines.pop_front();
        ++first;
    }
}

void LogSource::endLine()
{
    pushLine(std::move(partial), false);
    partial.clear();
}

void LogSource::addData(const char *data, size_t len)
// Splits the output into lines, without escape sequences and control
// characters (except tabs).
{
    for (size_t i = 0; i < len; ++i)
    {
        unsigned char c = data[i];
        switch (esc)
        {
            case Esc::None:
                break;
            case Esc::Start:
                // ESC [ (CSI), ESC ] P X ^ _ (strings), ESC ( ) * + (charset),
                // other: a single character.
                esc = c == '[' ? Esc::Csi :
                      (c == ']' || c == 'P' || c == 'X' || c == '^' || c == '_') ? Esc::Str :
                      (c == '(' || c == ')' || c == '*' || c == '+') ? Esc::Charset :
                      Esc::None;
                continue;
            case Esc::Csi:
                if (c >= 0x40 && c <= 0x7E)
                    esc = Esc::None;
                continue;
            case Esc::Str:
                if (c == 0x07)
                    esc = Esc::None;
                else if (c == 0x1B)
                    esc = Esc::StrEsc;
                continue;
            case Esc::StrEsc:
                esc = c == '\\' ? Esc::None : Esc::Str;
                continue;
            case Esc::Charset:
                esc = Esc::None;
                continue;
        }
        if (c == '\n')
            endLine();
        else if (c == 0x1B)
            esc = Esc::Start;
        else if (c == '\t' || c >= 0x20)
        {
            if (c != 0x7F && partial.size() < logsrc::maxLineBytes)
                partial += (char) c;
        }
        // Other control characters ('\r', '\b', bell...) are dropped.
    }
}

void LogSource::finishIfExited()
// After the output ended: the exit status, once the process exits.
{
    if (pid <= 0)
        return;
    int status = 0;
    pid_t r = waitpid(pid, &status, WNOHANG);
    if (r == 0)
    {
        // Still running without output (e.g. it closed its output):
        // give it some time, then stop it.
        if (Clock::now() - endTime > std::chrono::seconds(3))
            stop("-- source ended --");
        return;
    }
    std::string what = r == pid ? logsrc::describeStatus(status) : std::string("ended");
    pid = 0;
    pushLine("-- source ended (" + what + ") --", true);
    TVTermApp::note("log source ended (" + what + "): " + logsrc::shortCommand(command));
}

bool LogSource::poll()
{
    bool changed = false;
    if (fd >= 0)
    {
        size_t budget = logsrc::readBudget;
        static char buf[65536];
        while (budget > 0)
        {
            ssize_t r = read(fd, buf, std::min(sizeof(buf), budget));
            if (r > 0)
            {
                uint64_t before = endSeq();
                addData(buf, (size_t) r);
                changed |= endSeq() != before;
                lastData = Clock::now();
                budget -= (size_t) r;
                continue;
            }
            if (r < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR))
                break;
            // EOF or EIO: every program writing to the terminal has ended.
            if (!partial.empty())
                endLine();
            close(fd);
            fd = -1;
            ended = true;
            endTime = Clock::now();
            changed = true;
            break;
        }
        if (fd >= 0 && !partial.empty() && Clock::now() - lastData > logsrc::partialTimeout)
        {
            endLine();
            changed = true;
        }
    }
    if (fd < 0 && pid > 0)
    {
        uint64_t before = endSeq();
        finishIfExited();
        changed |= endSeq() != before;
    }
    if ( !isRunning() && ended && autoRestart > 0 &&
         Clock::now() - endTime >= std::chrono::seconds(autoRestart) )
    {
        restart();
        changed = true;
    }
    return changed;
}
