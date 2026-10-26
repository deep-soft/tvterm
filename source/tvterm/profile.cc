#include "profile.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <sys/stat.h>
#include <fstream>
#include <sstream>
#include <locale>

std::string profileDir()
{
    const char *xdg = getenv("XDG_CONFIG_HOME");
    if (xdg && *xdg)
        return std::string(xdg) + "/tvterm/profiles";
    const char *home = getenv("HOME");
    return std::string(home ? home : ".") + "/.config/tvterm/profiles";
}

// Own namespace: this file is compiled together with others (Unity Build).
namespace profileini
{

static std::string trim(const std::string &s)
{
    size_t a = 0, b = s.size();
    while (a < b && isspace((unsigned char) s[a]))
        ++a;
    while (b > a && isspace((unsigned char) s[b - 1]))
        --b;
    return s.substr(a, b - a);
}

static std::string lower(std::string s)
{
    for (char &c : s)
        c = (char) tolower((unsigned char) c);
    return s;
}

static std::string expandHome(const std::string &path)
{
    if (path == "~" || path.compare(0, 2, "~/") == 0)
        if (const char *home = getenv("HOME"))
            return home + path.substr(1);
    return path;
}

static bool parseBool(const std::string &value, bool &out)
{
    std::string v = lower(value);
    if (v == "yes" || v == "true" || v == "on" || v == "1")
        return out = true, true;
    if (v == "no" || v == "false" || v == "off" || v == "0")
        return out = false, true;
    return false;
}

// Keys of [eventviewer] that choose what it shows.
static const char * const filterKeys[] =
    { "mouse-move", "mouse-clicks", "keys", "other", "tvterm-messages" };

static bool isFilterKey(const std::string &key)
{
    for (const char *k : filterKeys)
        if (key == k)
            return true;
    return false;
}

static bool &filterField(EventFilter &filter, const std::string &key)
{
    if (key == "mouse-move") return filter.mouseMove;
    if (key == "mouse-clicks") return filter.mouseClicks;
    if (key == "keys") return filter.keys;
    if (key == "other") return filter.other;
    return filter.messages;
}

static std::string unquote(const std::string &value)
// "text" -> text: the quotes keep spaces at the start or the end.
{
    if (value.size() >= 2 && value.front() == '"' && value.back() == '"')
        return value.substr(1, value.size() - 2);
    return value;
}

static std::string quoteIfNeeded(const std::string &value)
{
    bool needed = !value.empty() &&
                  ( isspace((unsigned char) value.front()) || isspace((unsigned char) value.back()) ||
                    value.front() == '"' );
    return needed ? "\"" + value + "\"" : value;
}

static bool parsePos(const std::string &value, WindowProfile &term)
// Format: 'x y width height', in percent of the desktop. Commas, spaces
// and '%' signs are all accepted as separators.
{
    std::string v = value;
    for (char &c : v)
        if (c == ',' || c == '%')
            c = ' ';
    std::istringstream is(v);
    is.imbue(std::locale::classic());
    double n[4];
    for (double &i : n)
        if (!(is >> i))
            return false;
    std::string rest;
    if (is >> rest)
        return false;
    if ( n[0] < 0 || n[1] < 0 || n[2] <= 0 || n[3] <= 0 ||
         n[0] + n[2] > 100.05 || n[1] + n[3] > 100.05 )
        return false;
    term.x = n[0]; term.y = n[1]; term.w = n[2]; term.h = n[3];
    term.hasPos = true;
    return true;
}

} // namespace profileini

bool loadProfile(const std::string &nameOrPath, Profile &profile, std::string &warnings)
{
    using namespace profileini;
    std::string path = nameOrPath;
    if (path.find('/') == std::string::npos)
    {
        if (path.find('.') == std::string::npos)
            path += ".ini";
        path = profileDir() + "/" + path;
    }
    path = expandHome(path);

    std::ifstream file(path);
    if (!file)
        return false;

    profile = Profile();
    profile.path = path;
    enum { None, InProfile, InWindow } section = None;
    std::string line;
    int lineNo = 0;
    auto warn = [&] (const std::string &msg) {
        warnings += "line " + std::to_string(lineNo) + ": " + msg + "\n";
    };

    while (std::getline(file, line))
    {
        ++lineNo;
        line = trim(line);
        if (line.empty() || line[0] == '#' || line[0] == ';')
            continue;
        if (line[0] == '[')
        {
            std::string name = lower(trim(line.substr(1, line.find(']') - 1)));
            if (name == "profile")
                section = InProfile;
            else if ( name == "terminal" || name == "editor" || name == "eventviewer" ||
                      name == "event-viewer" || name == "logview" || name == "log-view" )
            {
                section = InWindow;
                profile.windows.emplace_back();
                profile.windows.back().kind = name == "terminal" ? WindowProfile::Terminal :
                                              name == "editor"   ? WindowProfile::Editor :
                                              name == "logview" || name == "log-view" ? WindowProfile::LogView :
                                                                   WindowProfile::EventViewer;
            }
            else
            {
                section = None;
                warn("unknown section [" + name + "]");
            }
            continue;
        }
        size_t eq = line.find('=');
        if (eq == std::string::npos)
        {
            warn("expected 'key = value'");
            continue;
        }
        std::string key = lower(trim(line.substr(0, eq)));
        std::string value = trim(line.substr(eq + 1));
        auto parseCount = [&] (int min, int max, int &out, const char *hint) {
            char *end;
            long n = strtol(value.c_str(), &end, 10);
            if (value.empty() || *end || n < min || n > max)
                warn("invalid " + key + " '" + value + "' (" + hint + ")");
            else
                out = (int) n;
        };

        if (section == InProfile)
        {
            if (key == "layout")
            {
                std::string v = lower(value);
                if (v == "auto") profile.layout = Profile::Auto;
                else if (v == "manual") profile.layout = Profile::Manual;
                else if (v == "tile-columns") profile.layout = Profile::TileColumns;
                else if (v == "tile-rows") profile.layout = Profile::TileRows;
                else if (v == "cascade") profile.layout = Profile::Cascade;
                else warn("invalid layout '" + value + "'");
            }
            else if (key == "focus")
            {
                char *end;
                long n = strtol(value.c_str(), &end, 10);
                if (*end || n < 0)
                    warn("invalid focus '" + value + "'");
                else
                    profile.focus = (int) n;
            }
            else
                warn("unknown key '" + key + "' in [profile]");
        }
        else if (section == InWindow)
        {
            WindowProfile &win = profile.windows.back();
            bool isTerm = win.kind == WindowProfile::Terminal;
            bool isEditor = win.kind == WindowProfile::Editor;
            bool isLog = win.kind == WindowProfile::LogView;
            bool hasAlerts = isTerm || isLog;
            const char *sectionName = isTerm ? "[terminal]" : isEditor ? "[editor]" :
                                      isLog ? "[logview]" : "[eventviewer]";
            if (key == "pos")
            {
                if (!parsePos(value, win))
                    warn("invalid pos '" + value + "' (use: x y width height, in percent)");
            }
            else if ((isTerm || isLog) && key == "title")
                win.title = value;
            else if (isLog && key == "command")
                win.command = value;
            else if (isLog && key == "include")
                win.logInclude = unquote(value);
            else if (isLog && key == "exclude")
                win.logExclude = unquote(value);
            else if (isLog && key == "ignore-case")
            {
                if (!parseBool(value, win.logIgnoreCase))
                    warn("invalid ignore-case '" + value + "' (use yes or no)");
            }
            else if (isLog && key == "wrap")
            {
                if (!parseBool(value, win.logWrap))
                    warn("invalid wrap '" + value + "' (use yes or no)");
            }
            else if (isLog && key == "max-lines")
            {
                int n = win.logMaxLines;
                parseCount(0, 1000000, n, "0: default from tvterm.ini, or 100 to 1000000");
                win.logMaxLines = n > 0 && n < 100 ? 100 : n;
            }
            else if (isLog && key == "auto-restart")
                parseCount(0, 86400, win.logAutoRestart, "seconds, 0: off");
            else if (isTerm && key == "dir")
                win.dir = expandHome(value);
            else if (isTerm && key == "command")
                win.command = value;
            else if (isTerm && key == "keep-open")
            {
                if (!parseBool(value, win.keepOpen))
                    warn("invalid keep-open '" + value + "' (use yes or no)");
            }
            else if (hasAlerts && key == "alert-activity")
            {
                if (!parseBool(value, win.alertActivity))
                    warn("invalid alert-activity '" + value + "' (use yes or no)");
            }
            else if (hasAlerts && key == "alert-keywords")
                win.alertKeywords = value;
            else if (hasAlerts && key == "alert-silence")
            {
                char *end;
                long n = strtol(value.c_str(), &end, 10);
                if (value.empty() || *end || n < 0 || n > 999999)
                    warn("invalid alert-silence '" + value + "' (use a number of seconds, 0: off)");
                else
                    win.alertSilence = (int) n;
            }
            else if (isEditor && key == "file")
                win.file = expandHome(value);
            else if (win.kind == WindowProfile::EventViewer && isFilterKey(key))
            {
                if (!parseBool(value, filterField(win.filter, key)))
                    warn("invalid " + key + " '" + value + "' (use yes or no)");
            }
            else
                warn("unknown key '" + key + "' in " + sectionName);
        }
        else
            warn("key '" + key + "' outside of a section");
    }
    return true;
}

bool makeDirs(const std::string &dir)
{
    std::string path;
    size_t pos = 0;
    while (pos != std::string::npos)
    {
        pos = dir.find('/', pos + 1);
        path = dir.substr(0, pos);
        if (!path.empty() && mkdir(path.c_str(), 0755) != 0 && errno != EEXIST)
            return false;
    }
    return true;
}

namespace profileini
{

static std::string formatPercent(double v)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "%.1f", v);
    // Use '.' whatever the locale, and drop a useless ".0".
    for (char *p = buf; *p; ++p)
        if (*p == ',')
            *p = '.';
    size_t len = strlen(buf);
    if (len > 2 && !strcmp(buf + len - 2, ".0"))
        buf[len - 2] = '\0';
    return buf;
}

} // namespace profileini

bool saveProfile(const std::string &path, const Profile &profile, std::string &error)
{
    using namespace profileini;
    std::ofstream file(path);
    if (!file)
    {
        error = strerror(errno);
        return false;
    }
    file << "# tvterm profile, saved by tvterm.\n"
            "# Format: see examples/profiles/example.ini in the tvterm sources.\n"
            "\n"
            "[profile]\n"
            "layout = manual\n";
    if (profile.focus > 0)
        file << "focus = " << profile.focus << "\n";
    for (const auto &win : profile.windows)
    {
        switch (win.kind)
        {
            case WindowProfile::Terminal:
                file << "\n[terminal]\n";
                if (!win.title.empty())
                    file << "title = " << win.title << "\n";
                if (!win.dir.empty())
                    file << "dir = " << win.dir << "\n";
                if (!win.command.empty())
                {
                    file << "command = " << win.command << "\n";
                    if (!win.keepOpen)
                        file << "keep-open = no\n";
                }
                if (win.alertActivity)
                    file << "alert-activity = yes\n";
                if (!win.alertKeywords.empty())
                    file << "alert-keywords = " << win.alertKeywords << "\n";
                if (win.alertSilence > 0)
                    file << "alert-silence = " << win.alertSilence << "\n";
                break;
            case WindowProfile::Editor:
                file << "\n[editor]\n";
                if (!win.file.empty())
                    file << "file = " << win.file << "\n";
                break;
            case WindowProfile::LogView:
                file << "\n[logview]\n";
                if (!win.title.empty())
                    file << "title = " << win.title << "\n";
                file << "command = " << win.command << "\n";
                if (!win.logInclude.empty())
                    file << "include = " << quoteIfNeeded(win.logInclude) << "\n";
                if (!win.logExclude.empty())
                    file << "exclude = " << quoteIfNeeded(win.logExclude) << "\n";
                file << "ignore-case = " << (win.logIgnoreCase ? "yes" : "no") << "\n"
                     << "wrap = " << (win.logWrap ? "yes" : "no") << "\n"
                     << "max-lines = " << win.logMaxLines << "\n";
                if (win.logAutoRestart > 0)
                    file << "auto-restart = " << win.logAutoRestart << "\n";
                if (win.alertActivity)
                    file << "alert-activity = yes\n";
                if (!win.alertKeywords.empty())
                    file << "alert-keywords = " << win.alertKeywords << "\n";
                if (win.alertSilence > 0)
                    file << "alert-silence = " << win.alertSilence << "\n";
                break;
            case WindowProfile::EventViewer:
            {
                const EventFilter &f = win.filter;
                auto yesNo = [] (bool b) { return b ? "yes" : "no"; };
                file << "\n[eventviewer]\n"
                        "# Events shown (yes/no). In the Event Viewer window, the keys\n"
                        "# M C K O T toggle them: M = mouse-move, C = mouse-clicks (and wheel),\n"
                        "# K = keys, O = other, T = tvterm-messages.\n"
                     << "mouse-move = " << yesNo(f.mouseMove) << "\n"
                     << "mouse-clicks = " << yesNo(f.mouseClicks) << "\n"
                     << "keys = " << yesNo(f.keys) << "\n"
                     << "other = " << yesNo(f.other) << "\n"
                     << "tvterm-messages = " << yesNo(f.messages) << "\n";
                break;
            }
        }
        if (win.hasPos)
            file << "pos = " << formatPercent(win.x) << " " << formatPercent(win.y)
                 << " " << formatPercent(win.w) << " " << formatPercent(win.h) << "\n";
    }
    file.close();
    if (!file)
    {
        error = "write error";
        return false;
    }
    return true;
}
