#include "settings.h"
#include "profile.h"

#include <tvterm/termopts.h>

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include <fstream>

// Own namespace: this file is compiled together with others (Unity Build).
namespace settingsini
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

static bool parseBool(const std::string &value, bool &out)
{
    std::string v = lower(value);
    if (v == "yes" || v == "true" || v == "on" || v == "1")
        return out = true, true;
    if (v == "no" || v == "false" || v == "off" || v == "0")
        return out = false, true;
    return false;
}

static bool parseInt(const std::string &value, int min, int max, int &out)
{
    if (value.empty())
        return false;
    char *end;
    long n = strtol(value.c_str(), &end, 10);
    if (*end || n < min || n > max)
        return false;
    out = (int) n;
    return true;
}

static std::u32string utf8ToUtf32(const std::string &s)
{
    std::u32string r;
    for (size_t i = 0; i < s.size();)
    {
        unsigned char c = s[i];
        int len = c < 0x80 ? 1 : (c >> 5) == 6 ? 2 : (c >> 4) == 14 ? 3 : (c >> 3) == 30 ? 4 : 1;
        char32_t cp = len == 1 ? c : len == 2 ? (c & 0x1F) : len == 3 ? (c & 0x0F) : (c & 0x07);
        for (int k = 1; k < len && i + k < s.size(); ++k)
            cp = (cp << 6) | ((unsigned char) s[i + k] & 0x3F);
        r.push_back(cp);
        i += len;
    }
    return r;
}

static std::string expandHome(const std::string &path)
{
    if (path == "~" || path.compare(0, 2, "~/") == 0)
        if (const char *home = getenv("HOME"))
            return home + path.substr(1);
    return path;
}

struct KeyNameEntry { const char *name; ushort code; };

static const KeyNameEntry keyNames[] =
{
    {"PgUp", kbPgUp}, {"PageUp", kbPgUp}, {"PgDn", kbPgDn}, {"PageDown", kbPgDn},
    {"Home", kbHome}, {"End", kbEnd}, {"Up", kbUp}, {"Down", kbDown},
    {"Left", kbLeft}, {"Right", kbRight}, {"Ins", kbIns}, {"Insert", kbIns},
    {"Del", kbDel}, {"Delete", kbDel}, {"Tab", kbTab}, {"Enter", kbEnter},
    {"Esc", kbEsc}, {"Backspace", kbBack}, {"Space", ' '},
    {"F1", kbF1}, {"F2", kbF2}, {"F3", kbF3}, {"F4", kbF4}, {"F5", kbF5},
    {"F6", kbF6}, {"F7", kbF7}, {"F8", kbF8}, {"F9", kbF9}, {"F10", kbF10},
    {"F11", kbF11}, {"F12", kbF12},
};

} // namespace settingsini


std::string settingsPath()
{
    const char *xdg = getenv("XDG_CONFIG_HOME");
    if (xdg && *xdg)
        return std::string(xdg) + "/tvterm/tvterm.ini";
    const char *home = getenv("HOME");
    return std::string(home ? home : ".") + "/.config/tvterm/tvterm.ini";
}

bool parseKey(const std::string &text, TKey &key)
// "Alt-Shift-PgUp", "ctrl+f5", "none"...
{
    using namespace settingsini;
    std::string t = trim(text);
    if (lower(t) == "none" || t.empty())
    {
        key = TKey();
        return !t.empty();
    }
    ushort mods = 0;
    for (;;)
    {
        std::string l = lower(t);
        size_t len = l.compare(0, 5, "ctrl-") == 0 || l.compare(0, 5, "ctrl+") == 0 ? 5 :
                     l.compare(0, 4, "alt-") == 0 || l.compare(0, 4, "alt+") == 0 ? 4 :
                     l.compare(0, 6, "shift-") == 0 || l.compare(0, 6, "shift+") == 0 ? 6 : 0;
        if (len == 0 || t.size() == len)
            break;
        mods |= len == 5 ? kbCtrlShift : len == 4 ? kbAltShift : kbShift;
        t = t.substr(len);
    }
    for (auto &k : keyNames)
        if (lower(t) == lower(k.name))
        {
            key = TKey(k.code, mods);
            return true;
        }
    if (t.size() == 1 && (unsigned char) t[0] > ' ' && (unsigned char) t[0] < 0x7F)
    {
        key = TKey((ushort) toupper((unsigned char) t[0]), mods);
        return true;
    }
    return false;
}

std::string keyName(TKey key)
{
    using namespace settingsini;
    if (key.code == 0)
        return "none";
    std::string name;
    if (key.mods & kbCtrlShift) name += "Ctrl-";
    if (key.mods & kbAltShift) name += "Alt-";
    if (key.mods & kbShift) name += "Shift-";
    for (auto &k : keyNames)
        if (TKey(k.code, key.mods) == key)
            return name + k.name;
    if (key.code > ' ' && key.code < 0x7F)
        return name + (char) key.code;
    return name + "?";
}

bool loadSettings(const std::string &path, Settings &settings, std::string &warnings)
{
    using namespace settingsini;
    const Settings previous = settings;
    settings = Settings();
    std::ifstream file(path);
    if (!file)
        return false;

    enum { None, General, Scrollback, Keys, LogView, Unknown } section = None;
    std::string line;
    int lineNo = 0;
    auto warn = [&] (const std::string &msg) {
        warnings += path + ": line " + std::to_string(lineNo) + ": " + msg + "\n";
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
            section = name == "general" ? General :
                      name == "scrollback" ? Scrollback :
                      name == "keys" ? Keys :
                      name == "logview" ? LogView : Unknown;
            if (section == Unknown)
                warn("unknown section [" + name + "]");
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
        auto invalid = [&] (const std::string &hint) {
            warn("invalid " + key + " '" + value + "' (" + hint + "); keeping the previous value");
        };

        if (section == General)
        {
            if (key == "profile")
                settings.profile = value;
            else if (key == "auto-reload")
            {
                if (!parseBool(value, settings.autoReload))
                    invalid("use yes or no"), settings.autoReload = previous.autoReload;
            }
            else
                warn("unknown key '" + key + "' in [general]");
        }
        else if (section == Scrollback)
        {
            if (key == "lines")
            {
                if (!parseInt(value, 0, 100000, settings.lines))
                    invalid("0 to 100000"), settings.lines = previous.lines;
            }
            else if (key == "wheel-lines")
            {
                if (!parseInt(value, 1, 100, settings.wheelLines))
                    invalid("1 to 100"), settings.wheelLines = previous.wheelLines;
            }
            else if (key == "word-chars")
                settings.wordChars = value;
            else if (key == "save-dir")
                settings.saveDir = expandHome(value);
            else if (key == "search-case-sensitive")
            {
                if (!parseBool(value, settings.searchCaseSensitive))
                    invalid("use yes or no"), settings.searchCaseSensitive = previous.searchCaseSensitive;
            }
            else if (key == "shift-click-select")
            {
                if (!parseBool(value, settings.shiftClickSelect))
                    invalid("use yes or no"), settings.shiftClickSelect = previous.shiftClickSelect;
            }
            else
                warn("unknown key '" + key + "' in [scrollback]");
        }
        else if (section == LogView)
        {
            if (key == "max-lines")
            {
                if (!parseInt(value, 100, 1000000, settings.logViewMaxLines))
                    invalid("100 to 1000000"), settings.logViewMaxLines = previous.logViewMaxLines;
            }
            else
                warn("unknown key '" + key + "' in [logview]");
        }
        else if (section == Keys)
        {
            TKey *target = key == "history-page-up" ? &settings.keyPageUp :
                           key == "history-page-down" ? &settings.keyPageDown :
                           key == "history-top" ? &settings.keyTop :
                           key == "history-bottom" ? &settings.keyBottom : nullptr;
            const TKey *prev = key == "history-page-up" ? &previous.keyPageUp :
                               key == "history-page-down" ? &previous.keyPageDown :
                               key == "history-top" ? &previous.keyTop :
                               key == "history-bottom" ? &previous.keyBottom : nullptr;
            if (!target)
                warn("unknown key '" + key + "' in [keys]");
            else if (!parseKey(value, *target))
                invalid("e.g. Alt-Shift-PgUp, Ctrl-F5 or none"), *target = *prev;
        }
        else if (section == None)
            warn("key '" + key + "' outside of a section");
    }
    return true;
}

void applySettings(const Settings &s)
{
    using namespace settingsini;
    auto &o = tvterm::terminalOptions;
    o.scrollbackLines = s.lines;
    o.wheelLines = s.wheelLines;
    o.wordChars = utf8ToUtf32(s.wordChars);
    o.searchCaseSensitive = s.searchCaseSensitive;
    o.shiftClickSelect = s.shiftClickSelect;
    o.keyPageUp = s.keyPageUp;
    o.keyPageDown = s.keyPageDown;
    o.keyTop = s.keyTop;
    o.keyBottom = s.keyBottom;
}

std::string settingsTemplate()
{
    Settings d;
    return
        "# tvterm settings. Lines starting with '#' are comments; remove the '#'\n"
        "# to change a value. Changes apply as soon as the file is saved\n"
        "# (or with Ctrl-B More... Reload settings). Problems are reported in the\n"
        "# Event Viewer (Ctrl-B V).\n"
        "\n"
        "[general]\n"
        "# Profile opened when tvterm starts without -p (name or path, see\n"
        "# examples/profiles/example.ini). 'tvterm -p none' starts without it.\n"
        "#profile = server-mail\n"
        "# Reload this file automatically when it changes (checked every 2 seconds).\n"
        "#auto-reload = yes\n"
        "\n"
        "[scrollback]\n"
        "# Lines kept per terminal (0 to 100000). About 4 KB per line of 100 columns.\n"
        "#lines = " + std::to_string(d.lines) + "\n"
        "# Lines scrolled by the mouse wheel.\n"
        "#wheel-lines = " + std::to_string(d.wheelLines) + "\n"
        "# Characters that are part of a word on double click, besides letters and digits.\n"
        "#word-chars = " + d.wordChars + "\n"
        "# Directory proposed when saving the history; empty: the terminal's current directory.\n"
        "#save-dir = ~/logs\n"
        "# Default of the 'Case sensitive' option when searching.\n"
        "#search-case-sensitive = no\n"
        "# Shift+click selects text even in programs that use the mouse (e.g. far2l).\n"
        "#shift-click-select = yes\n"
        "\n"
        "[logview]\n"
        "# Lines kept by each Log View source (Ctrl-B D), 100 to 1000000; a window\n"
        "# can ask for another number. About 1 KB per line of a firewall log.\n"
        "#max-lines = " + std::to_string(d.logViewMaxLines) + "\n"
        "\n"
        "[keys]\n"
        "# Keys that scroll the history directly (outside of full-screen programs).\n"
        "# Examples: Alt-Shift-PgUp, Ctrl-F5, none.\n"
        "#history-page-up = " + keyName(d.keyPageUp) + "\n"
        "#history-page-down = " + keyName(d.keyPageDown) + "\n"
        "#history-top = " + keyName(d.keyTop) + "\n"
        "#history-bottom = " + keyName(d.keyBottom) + "\n";
}

FileStamp FileStamp::of(const std::string &path)
{
    FileStamp f;
    struct stat st;
    if (stat(path.c_str(), &st) == 0)
    {
        f.exists = true;
        f.mtime = st.st_mtime;
        f.mtimeNsec = st.st_mtim.tv_nsec;
        f.size = st.st_size;
    }
    return f;
}

bool FileStamp::operator!=(const FileStamp &o) const
{
    return exists != o.exists || mtime != o.mtime || mtimeNsec != o.mtimeNsec || size != o.size;
}
