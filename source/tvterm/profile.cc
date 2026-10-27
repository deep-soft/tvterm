#include "profile.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <math.h>
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

static std::string formatPercent(double v);

static bool parsePos(const std::string &value, WindowProfile &term, std::string &notes)
// Format: 'x y width height', in percent of the desktop. Commas, spaces
// and '%' signs are all accepted as separators. A position that is (partly)
// outside the desktop is moved inside it, and shrunk if it is larger, with a
// line in 'notes' (older versions saved such positions as they were).
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
    if (!(n[2] > 0) || !(n[3] > 0))
        return false;
    double orig[4] = {n[0], n[1], n[2], n[3]};
    for (int i = 0; i < 2; ++i)
    {
        double &pos = n[i], &size = n[i + 2];
        if (size > 100)
            size = 100;
        if (pos < 0)
            pos = 0;
        if (pos + size > 100)
            pos = 100 - size;
    }
    bool moved = false;
    for (int i = 0; i < 4; ++i)
        moved |= fabs(n[i] - orig[i]) > 0.05; // Rounding: not worth a note.
    if (moved)
        notes += "pos '" + value + "' is outside the desktop, using '" +
                 formatPercent(n[0]) + " " + formatPercent(n[1]) + " " +
                 formatPercent(n[2]) + " " + formatPercent(n[3]) + "'\n";
    term.x = n[0]; term.y = n[1]; term.w = n[2]; term.h = n[3];
    term.hasPos = true;
    return true;
}

bool loadProfile(const std::string &nameOrPath, Profile &profile, std::string &warnings)
{
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
            else if (name == "terminal" || name == "editor" || name == "eventviewer" || name == "event-viewer")
            {
                section = InWindow;
                profile.windows.emplace_back();
                profile.windows.back().kind = name == "terminal" ? WindowProfile::Terminal :
                                              name == "editor"   ? WindowProfile::Editor :
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
            const char *sectionName = isTerm ? "[terminal]" : isEditor ? "[editor]" : "[eventviewer]";
            if (key == "pos")
            {
                if (!parsePos(value, win, profile.notes))
                    warn("invalid pos '" + value + "' (use: x y width height, in percent)");
            }
            else if (isTerm && key == "title")
                win.title = value;
            else if (isTerm && key == "dir")
                win.dir = expandHome(value);
            else if (isTerm && key == "command")
                win.command = value;
            else if (isTerm && key == "keep-open")
            {
                if (!parseBool(value, win.keepOpen))
                    warn("invalid keep-open '" + value + "' (use yes or no)");
            }
            else if (isEditor && key == "file")
                win.file = expandHome(value);
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

bool saveProfile(const std::string &path, const Profile &profile, std::string &error)
{
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
                break;
            case WindowProfile::Editor:
                file << "\n[editor]\n";
                if (!win.file.empty())
                    file << "file = " << win.file << "\n";
                break;
            case WindowProfile::EventViewer:
                file << "\n[eventviewer]\n";
                break;
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
