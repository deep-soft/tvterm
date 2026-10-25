#include "procinfo.h"

#include <tvterm/pty.h>

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <algorithm>
#include <fstream>
#include <iterator>
#include <map>
#include <vector>

namespace procinfo
{

std::string readLink(const std::string &path)
{
    char buf[4096];
    ssize_t len = readlink(path.c_str(), buf, sizeof(buf) - 1);
    if (len <= 0)
        return {};
    buf[len] = '\0';
    return buf;
}

static std::string readFile(const std::string &path)
{
    std::ifstream f(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

static std::vector<std::string> readArgs(long pid)
{
    std::string data = readFile("/proc/" + std::to_string(pid) + "/cmdline");
    std::vector<std::string> args;
    size_t start = 0;
    while (start < data.size())
    {
        size_t end = data.find('\0', start);
        if (end == std::string::npos)
            end = data.size();
        args.push_back(data.substr(start, end - start));
        start = end + 1;
    }
    return args;
}

static std::string shellQuote(const std::string &arg)
{
    if (!arg.empty() && arg.find_first_not_of(
            "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-./=:,+@%") == std::string::npos)
        return arg;
    std::string out = "'";
    for (char c : arg)
        out += c == '\'' ? std::string("'\\''") : std::string(1, c);
    return out + "'";
}

static std::string joinArgs(const std::vector<std::string> &args)
{
    std::string out;
    for (const auto &arg : args)
    {
        if (!out.empty())
            out += ' ';
        out += shellQuote(arg);
    }
    return out;
}

std::string readCommandLine(long pid)
{
    return joinArgs(readArgs(pid));
}

namespace
{

struct Proc
{
    long pid;
    long ppid;
    std::string comm;
};

bool isShellName(const std::string &comm)
{
    static const char *const shells[] =
        {"sh", "ash", "bash", "dash", "zsh", "ksh", "mksh", "pdksh", "yash",
         "fish", "csh", "tcsh", "busybox"};
    for (const char *s : shells)
        if (comm == s)
            return true;
    return false;
}

// Reads the processes of a process group. Returns false if /proc is
// not available.
bool readGroup(long pgid, std::map<long, Proc> &group)
{
    DIR *dir = opendir("/proc");
    if (!dir)
        return false;
    while (struct dirent *e = readdir(dir))
    {
        char *end;
        long pid = strtol(e->d_name, &end, 10);
        if (pid <= 0 || *end)
            continue;
        // Format: "pid (comm) state ppid pgrp ...". 'comm' may contain
        // spaces and parentheses, so look for the last ')'.
        std::string stat = readFile("/proc/" + std::to_string(pid) + "/stat");
        size_t open = stat.find('('), close = stat.rfind(')');
        if (open == std::string::npos || close == std::string::npos || close < open)
            continue;
        char state;
        long ppid, pgrp;
        if (sscanf(stat.c_str() + close + 1, " %c %ld %ld", &state, &ppid, &pgrp) != 3)
            continue;
        if (pgrp == pgid && state != 'Z')
            group[pid] = {pid, ppid, stat.substr(open + 1, close - open - 1)};
    }
    closedir(dir);
    return true;
}

bool hasArg(const std::vector<std::string> &args, const char *arg)
{
    for (size_t i = 1; i < args.size(); ++i)
        if (args[i] == arg)
            return true;
    return false;
}

// A shell that is only waiting for input: started without arguments
// (except options, but not '-c').
bool isInteractiveShell(const Proc &p)
{
    if (!isShellName(p.comm))
        return false;
    std::vector<std::string> args = readArgs(p.pid);
    for (size_t i = 1; i < args.size(); ++i)
        if (args[i].empty() || args[i][0] != '-' || args[i] == "-c")
            return false;
    return true;
}

// A shell that only runs a command line ('sh -c "..."'), like the wrapper
// used for profile commands. A shell running a script is not one.
bool isCommandShell(const Proc &p)
{
    return isShellName(p.comm) && hasArg(readArgs(p.pid), "-c");
}

std::string describeProc(const Proc &p)
{
    std::vector<std::string> args = readArgs(p.pid);
    if (args.empty())
        return p.comm;
    // far2l changes its own command line (argv[0]).
    if (p.comm == "far2l")
        args[0] = "far2l";
    return joinArgs(args);
}

} // namespace

bool describeForeground(const tvterm::ClientProcessInfo &info, ForegroundInfo &out)
{
    out = {};
    long pgid = info.foregroundPgid ? info.foregroundPgid : info.clientPid;
    std::map<long, Proc> group;
    if (pgid <= 0 || !readGroup(pgid, group) || group.empty())
        return false;

    std::map<long, std::vector<long>> children; // Sorted by pid.
    std::vector<long> roots;
    for (const auto &item : group)
    {
        const Proc &p = item.second;
        if (group.count(p.ppid))
            children[p.ppid].push_back(p.pid);
        else
            roots.push_back(p.pid);
    }

    // Shells that only run a command line (e.g. 'sh -c "tail -f x"' or the
    // wrapper used for profile commands) are skipped: what matters is
    // what they run. Several commands side by side form a pipeline.
    std::vector<long> shown;
    std::vector<long> pending(roots.rbegin(), roots.rend());
    while (!pending.empty())
    {
        long pid = pending.back();
        pending.pop_back();
        auto it = children.find(pid);
        if (it != children.end() && isCommandShell(group[pid]))
            pending.insert(pending.end(), it->second.rbegin(), it->second.rend());
        else
            shown.push_back(pid);
    }

    for (long pid : shown)
    {
        const Proc &p = group[pid];
        if (!out.command.empty())
            out.command += " | ";
        out.command += describeProc(p);
        if (p.comm == "far2l")
            out.far2l = true;
    }
    if (shown.size() == 1 && isInteractiveShell(group[shown[0]]))
    {
        out.idleShell = true;
        out.command = group[shown[0]].comm;
    }
    return true;
}

} // namespace procinfo
