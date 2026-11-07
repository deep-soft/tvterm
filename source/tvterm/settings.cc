#include "settings.h"
#include "profile.h"
#include "highlight.h"
#include "about.h"

#include <tvterm/termopts.h>

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include <fstream>
#include <sstream>

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

// {"*.txt", "*"} -> "*.txt *".
static std::string joinWords(const std::vector<std::string> &words)
{
    std::string out;
    for (auto &w : words)
        out += (out.empty() ? "" : " ") + w;
    return out;
}

} // namespace settingsini


std::string settingsPath()
{
    const char *xdg = getenv("XDG_CONFIG_HOME");
    if (xdg && *xdg)
        return std::string(xdg) + "/tvterm/tvterm.ini";
    const char *home = getenv("HOME");
    return std::string(home ? home : ".") + "/.config/tvterm/tvterm.ini";
}

bool parseSize(const std::string &text, long long &bytes)
// "5M", "500k", "1G", "1048576", "0".
{
    using namespace settingsini;
    std::string t = trim(text);
    if (t.empty() || !isdigit((unsigned char) t[0]))
        return false;
    char *end;
    long long n = strtoll(t.c_str(), &end, 10);
    std::string unit = lower(trim(end));
    long long mult = unit.empty() || unit == "b" ? 1 :
                     unit == "k" || unit == "kb" ? 1024LL :
                     unit == "m" || unit == "mb" ? 1024LL * 1024 :
                     unit == "g" || unit == "gb" ? 1024LL * 1024 * 1024 : 0;
    if (mult == 0 || n < 0 || n > (1LL << 40) / mult)
        return false;
    bytes = n * mult;
    return true;
}

std::string sizeName(long long bytes)
// The shortest exact form: "5M", "500K", "1234".
{
    static const char units[] = {'G', 'M', 'K'};
    for (int i = 0; i < 3; ++i)
    {
        long long mult = 1LL << (10 * (3 - i));
        if (bytes > 0 && bytes % mult == 0)
            return std::to_string(bytes / mult) + units[i];
    }
    return std::to_string(bytes);
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

    enum { None, General, Scrollback, Alerts, Keys, LogView, Highlight, Transform, Unknown } section = None;
    std::string line;
    int lineNo = 0;
    auto warn = [&] (const std::string &msg) {
        warnings += path + ": line " + std::to_string(lineNo) + ": " + msg + "\n";
    };
    while (std::getline(file, line))
    {
        ++lineNo;
        line = trim(line);
        // The format line, in the header (before the first section).
        int format;
        if (section == None && settings.fileFormat < 0 && parseFormatLine(line, format))
            settings.fileFormat = format;
        if (line.empty() || line[0] == '#' || line[0] == ';')
            continue;
        if (line[0] == '[')
        {
            std::string name = lower(trim(line.substr(1, line.find(']') - 1)));
            // [highlight <name>] and [transform <name>]: sets for Log View
            // (rules checked later, see highlight.h and transform.h).
            bool isHighlight = name == "highlight" || name.compare(0, 10, "highlight ") == 0;
            bool isTransform = name == "transform" || name.compare(0, 10, "transform ") == 0;
            if (isHighlight || isTransform)
            {
                const char *kind = isHighlight ? "highlight" : "transform";
                auto &list = isHighlight ? settings.highlight : settings.transform;
                std::string set = name.size() > 10 ? trim(name.substr(10)) : std::string();
                bool validName = !set.empty() &&
                    set.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789_-.") == std::string::npos;
                section = !validName ? Unknown : isHighlight ? Highlight : Transform;
                if (!validName)
                    warn("invalid section [" + name + "] (use [" + kind + " <name>], the name with "
                         "letters, digits, '-', '_' or '.'); section ignored");
                else
                {
                    for (auto it = list.begin(); it != list.end(); ++it)
                        if (it->name == set)
                        {
                            warn("[" + std::string(kind) + " " + set + "] defined again; the last one is used");
                            list.erase(it);
                            break;
                        }
                    list.emplace_back();
                    list.back().name = set;
                    list.back().lineNo = lineNo;
                }
                continue;
            }
            section = name == "general" ? General :
                      name == "scrollback" ? Scrollback :
                      name == "alerts" ? Alerts :
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
        if (section == Highlight || section == Transform)
        {
            (section == Highlight ? settings.highlight : settings.transform).back().entries.push_back({lineNo, key, value});
            continue;
        }
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
            else if (key == "reload-message")
            {
                std::string v = lower(value);
                if (v == "status" || v == "none")
                    settings.reloadMessage = v == "status";
                else
                    invalid("use status or none"), settings.reloadMessage = previous.reloadMessage;
            }
            else if (key == "large-file")
            {
                if (!parseSize(value, settings.largeFile))
                    invalid("e.g. 5M, 500K, 1G or 0"), settings.largeFile = previous.largeFile;
            }
            else if (key == "external-viewer")
                settings.externalViewer = value;
            else if (key == "external-editor")
                settings.externalEditor = value;
            else if (key == "file-filters")
            {
                std::vector<std::string> filters;
                std::istringstream words(value);
                for (std::string w; words >> w;)
                    filters.push_back(w);
                if (filters.empty())
                    invalid("patterns separated by spaces, e.g. *.txt *log* *"),
                    settings.fileFilters = previous.fileFilters;
                else
                    settings.fileFilters = std::move(filters);
            }
            else if (key == "file-dirs")
            {
                std::vector<std::string> dirs;
                std::istringstream words(value);
                for (std::string w; words >> w;)
                    dirs.push_back(w);
                settings.fileDirs = std::move(dirs);
            }
            else if (key == "file-dialog-size")
            {
                // "120x40"; empty or "none": the size chosen by tvision.
                std::string v = lower(value);
                size_t x = v.find('x');
                int w = 0, h = 0;
                if (v.empty() || v == "none")
                    settings.fileDialogWidth = settings.fileDialogHeight = 0;
                else if ( x != std::string::npos &&
                          parseInt(trim(v.substr(0, x)), minFileDialogWidth, 1000, w) &&
                          parseInt(trim(v.substr(x + 1)), minFileDialogHeight, 1000, h) )
                    settings.fileDialogWidth = w, settings.fileDialogHeight = h;
                else
                    invalid("columns x rows, e.g. 120x40; at least " +
                            std::to_string(minFileDialogWidth) + "x" +
                            std::to_string(minFileDialogHeight)),
                    settings.fileDialogWidth = previous.fileDialogWidth,
                    settings.fileDialogHeight = previous.fileDialogHeight;
            }
            else if (key == "file-list-columns")
            {
                if (!parseInt(value, 1, 2, settings.fileListColumns))
                    invalid("1 or 2"), settings.fileListColumns = previous.fileListColumns;
            }
            else if (key == "window-shadow")
            {
                if (!parseBool(value, settings.windowShadow))
                    invalid("use yes or no"), settings.windowShadow = previous.windowShadow;
            }
            else if (key == "editor-selection")
            {
                // "default" (or empty): tvision's color.
                std::string v = lower(trim(value)), error;
                int attr = 0;
                if (v.empty() || v == "default")
                    settings.editorSelection = -1;
                else if (parseColorName(value, attr, error))
                    settings.editorSelection = attr;
                else
                    invalid(error + "; e.g. black on bright-green, or default"),
                    settings.editorSelection = previous.editorSelection;
            }
            else if (key == "undo-limit-steps")
            {
                if (!parseInt(value, 1, 100000, settings.undoLimitSteps))
                    invalid("1 to 100000"), settings.undoLimitSteps = previous.undoLimitSteps;
            }
            else if (key == "undo-limit-memory")
            {
                if (!parseSize(value, settings.undoLimitMemory))
                    invalid("e.g. 20M, 500K, 1G or 0"), settings.undoLimitMemory = previous.undoLimitMemory;
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
            else if (key == "save-name")
            {
                std::string v = lower(value);
                if (v == "date-first")
                    settings.saveDateFirst = true;
                else if (v == "name-first")
                    settings.saveDateFirst = false;
                else
                    invalid("use date-first or name-first"), settings.saveDateFirst = previous.saveDateFirst;
            }
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
        else if (section == Alerts)
        {
            if (key == "title-color")
            {
                std::string v = lower(value);
                if (v == "red" || v == "none")
                    settings.alertTitleRed = v == "red";
                else
                    invalid("use red or none"), settings.alertTitleRed = previous.alertTitleRed;
            }
            else if (key == "seen-color")
            {
                std::string v = lower(value);
                if (v == "yellow" || v == "cyan" || v == "none")
                    settings.alertSeenColor = v == "yellow" ? 0xE0 : v == "cyan" ? 0xB0 : 0;
                else
                    invalid("use yellow, cyan or none"), settings.alertSeenColor = previous.alertSeenColor;
            }
            else
                warn("unknown key '" + key + "' in [alerts]");
        }
        else if (section == LogView)
        {
            if (key == "max-lines")
            {
                if (!parseInt(value, 100, 1000000, settings.logViewMaxLines))
                    invalid("100 to 1000000"), settings.logViewMaxLines = previous.logViewMaxLines;
            }
            else if (key == "builtin-highlight")
            {
                if (!parseBool(value, settings.builtinHighlight))
                    invalid("use yes or no"), settings.builtinHighlight = previous.builtinHighlight;
            }
            else
                warn("unknown key '" + key + "' in [logview]");
        }
        else if (section == Keys && globalkeys::actionOf(key) >= 0)
        {
            int action = globalkeys::actionOf(key);
            std::vector<GlobalKey> keys;
            std::vector<std::string> problems;
            globalkeys::parseList(value, keys, problems);
            for (auto &p : problems)
                warn(key + ": " + p + "; ignored");
            if (action == gaMenu && keys.empty())
            {
                warn("menu-key has no valid key; using Ctrl-B");
                keys.push_back(globalkeys::defaultMenuKey());
            }
            settings.globalKeys[action] = keys;
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
    std::vector<std::string> duplicates;
    globalkeys::removeDuplicates(settings.globalKeys, duplicates);
    for (auto &d : duplicates)
        warnings += path + ": " + d + "\n";
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

bool parseFormatLine(const std::string &line, int &format)
{
    size_t i = line.find_first_not_of(" \t");
    if (i == std::string::npos || line.compare(i, 2, "##") != 0)
        return false;
    i = line.find_first_not_of("# \t", i);
    if (i == std::string::npos || line.compare(i, 7, "format ") != 0)
        return false;
    i += 7;
    size_t end = i;
    while (end < line.size() && end - i < 6 && line[end] >= '0' && line[end] <= '9')
        ++end;
    if (end == i || (end < line.size() && line[end] != ',' && line[end] != ' ' && line[end] != '\t'))
        return false;
    format = std::stoi(line.substr(i, end - i));
    return true;
}

std::string settingsTemplate()
{
    Settings d;
    return
        "## format " + std::to_string(settingsFormat) + ", created by tvterm " + buildinfo::version +
        " (" + buildinfo::commit + ", built " + buildinfo::date + ")\n"
        "## tvterm settings. Lines starting with '#' are comments (spaces before it\n"
        "## are allowed). To change a value, remove the '#' in front of its key:\n"
        "## ' #lines = 10000' becomes ' lines = 10000'. Changes apply as soon as the\n"
        "## file is saved (or with Ctrl-B More... Reload settings). Problems are\n"
        "## reported in the Event Viewer (Ctrl-B V).\n"
        "\n"
        "[general]\n"
        "  # Profile opened when tvterm starts without -p (name or path, see\n"
        "  # examples/profiles/example.ini). 'tvterm -p none' starts without it.\n"
        " #profile = server-mail\n"
        "  # Reload this file automatically when it changes (checked every 2 seconds).\n"
        " #auto-reload = yes\n"
        "  # After an automatic reload: status (\"Settings reloaded.\" in the status line\n"
        "  # for 5 seconds) or none. Problems in this file always go to the Event Viewer.\n"
        " #reload-message = status\n"
        "  # Files larger than this ask before they open in tvterm (Save with 'Open\n"
        "  # after saving', Ctrl-B O, More... View file): K, M or G; 0: never ask.\n"
        " #large-file = " + sizeName(d.largeFile) + "\n"
        "  # Programs for files, run in a new terminal window: %f is the full file path\n"
        "  # (quoted for the shell; without %f, the path is added at the end).\n"
        "  # More... View file uses external-viewer directly; large files offer both.\n"
        "  # --mortal: far2l ends with the window (otherwise it stays in the background).\n"
        " #external-viewer = far2l --mortal -v %f\n"
        " #external-editor = far2l --mortal -e %f\n"
        "  # Filters offered in the Filter list of the file dialogs (and by Down in\n"
        "  # the Name field of Ctrl-B O, More... View file, the editor's Save file as),\n"
        "  # separated by spaces, e.g. *.txt *log* *.ini *. The first one is the\n"
        "  # initial filter of Ctrl-B O and View file. A filter typed in the Filter\n"
        "  # field can hold several patterns separated by spaces: *.log *.txt\n"
        " #file-filters = " + settingsini::joinWords(d.fileFilters) + "\n"
        "  # Directories offered in the Folder list of the file dialogs, after Home,\n"
        "  # Profiles and Settings, separated by spaces ('~' is the home directory).\n"
        " #file-dirs = ~/logs /var/log\n"
        "  # Size of the file dialogs (Ctrl-B O, More... View file, Save file as, Load /\n"
        "  # Save Profile) when they open, columns x rows; smaller if the screen is\n"
        "  # smaller. Without it, tvision's size. A dialog resized with the mouse (or\n"
        "  # Ctrl-F5) keeps its size until tvterm exits.\n"
        " #file-dialog-size = 120x40\n"
        "  # Columns of the list of files in those dialogs: 1 shows long names whole\n"
        "  # (the date and size of the chosen file are on the bottom line), or 2.\n"
        " #file-list-columns = " + std::to_string(d.fileListColumns) + "\n"
        "  # Shadow under the desktop windows (terminals, Log View, editors, Event\n"
        "  # Viewer): yes or no. With Tile, the shadow of a window falls on the title\n"
        "  # of the window below it. Menus and dialogs always have a shadow.\n"
        " #window-shadow = " + std::string(d.windowShadow ? "yes" : "no") + "\n"
        "  # Color of the selected text in the editors, also the text found by Find:\n"
        "  # <color> on <color> (black red green yellow blue magenta cyan white,\n"
        "  # bright-<color>), e.g. black on bright-green; default: tvision's color.\n"
        " #editor-selection = default\n"
        "  # Undo in the editors (Ctrl-Z, Ctrl-U, Alt-Backspace; Redo: Ctrl-Shift-Z,\n"
        "  # Alt-Shift-Backspace): the steps kept per editor (1 to 100000) and the\n"
        "  # memory they may use (K, M or G; 0: no limit). Over a limit, the oldest\n"
        "  # steps are dropped.\n"
        " #undo-limit-steps = " + std::to_string(d.undoLimitSteps) + "\n"
        " #undo-limit-memory = " + sizeName(d.undoLimitMemory) + "\n"
        "\n"
        "[scrollback]\n"
        "  # Lines kept per terminal (0 to 100000). About 4 KB per line of 100 columns.\n"
        " #lines = " + std::to_string(d.lines) + "\n"
        "  # Lines scrolled by the mouse wheel.\n"
        " #wheel-lines = " + std::to_string(d.wheelLines) + "\n"
        "  # Characters that are part of a word on double click, besides letters and digits.\n"
        " #word-chars = " + d.wordChars + "\n"
        "  # Directory proposed when saving the history; empty: the terminal's current directory.\n"
        " #save-dir = ~/logs\n"
        "  # File name proposed when saving (history, screen, Log View):\n"
        "  # name-first: tvterm-<title>-YYYY-MM-DD-hhmm.txt\n"
        "  # date-first: YYYY-MM-DD-hhmm-tvterm-<title>.txt\n"
        "  # The dialog can switch it; the last choice is kept until tvterm exits.\n"
        " #save-name = name-first\n"
        "  # Default of the 'Case sensitive' option when searching.\n"
        " #search-case-sensitive = no\n"
        "  # Shift+click selects text even in programs that use the mouse (e.g. far2l).\n"
        " #shift-click-select = yes\n"
        "\n"
        "[alerts]\n"
        "  # Title of a window with an alert (terminal or Log View, Ctrl-B A): red\n"
        "  # (the title text white on red until the window gets the focus) or none\n"
        "  # (only the mark, e.g. [!], in the title).\n"
        " #title-color = red\n"
        "  # Title of a window that got the focus while it had an alert (Ctrl-B G,\n"
        "  # a click), until it loses the focus: yellow (black on yellow), cyan\n"
        "  # or none.\n"
        " #seen-color = yellow\n"
        "\n"
        "[logview]\n"
        "  # Lines kept by each Log View source (Ctrl-B L), 100 to 1000000; a window\n"
        "  # can ask for another number. About 1 KB per line of a firewall log.\n"
        " #max-lines = " + std::to_string(d.logViewMaxLines) + "\n"
        "  # The highlight sets built into tvterm (listed below): yes, or no to use only\n"
        "  # the [highlight] sections of this file.\n"
        " #builtin-highlight = " + std::string(d.builtinHighlight ? "yes" : "no") + "\n"
        "\n"
        "  # Highlight sets for Log View (F4 > Highlight sets, e.g. 'syslog kv'; H turns\n"
        "  # the colors on/off). Built-in sets: levels timestamps kv syslog nginx\n"
        "  # fail2ban rsync; a section with the same name replaces one. More sets, to\n"
        "  # copy here: examples/highlight-sets.ini (zimbra-mta, zimbra-mailbox,\n"
        "  # fortigate). Rules, in order (where they overlap, the first one wins):\n"
        "  #   line   = <color> \"<regex>\" [icase]   the whole line (first that matches)\n"
        "  #   match  = <color> \"<regex>\" [icase]   every match\n"
        "  #   groups = <color> \"<regex>\" [icase]   only the text in parentheses\n"
        "  #   hash   = \"<regex>\" [icase]           a color derived from the value\n"
        "  #   include = <set>                      the rules of another set (put it last)\n"
        "  # Colors: black red green yellow blue magenta cyan white, bright-<color>,\n"
        "  # optionally 'on <color>' (background). Regular expressions like grep -E;\n"
        "  # inside the quotes, \\\" is a quote.\n"
        "#[highlight my-mail]\n"
        " #line = bright-red \"status=bounced\"\n"
        " #groups = bright-yellow on blue \"from=<([^>]*)>\"\n"
        " #hash = \"client=[^[]*\\[([0-9.]+)\\]\"\n"
        " #include = syslog\n"
        "\n"
        "  # Transformation sets for Log View (F4 > Transform, e.g. 'show-tls'; T turns\n"
        "  # them on/off). They change the text shown (the filters and the alerts\n"
        "  # use the original line). Rules, applied in order, each on the result of\n"
        "  # the one before:\n"
        "  #   replace = \"<regex>\" \"<template>\" [icase]   every match is replaced\n"
        "  # Template: \\0-\\9 the match / a group, ${n:>W} ${n:<W} a group aligned\n"
        "  # right / left in W characters (${n:>W!} cuts a longer value), \\t a tab,\n"
        "  # \\\\ a backslash, \\$ a dollar; \"\" removes the match.\n"
        "  #   fields = k1 k2:>W k3:<W! ...   the values of key=value words, in this\n"
        "  #   order (key:>W / key:<W aligned right / left in W characters, ! cuts)\n"
        "  #   fields-format = pairs | values   key=value (default) or only the values\n"
        "  #   (separated by a tab, by a space when a field has a width)\n"
        "  # A missing field is left out (with a width: blanks); a line without any of\n"
        "  # the fields stays as it is. The fields come before the replace rules.\n"
        "  # Example: the TLS connections of zimbra.log as IP, date, service, name.\n"
        "#[transform show-tls]\n"
        " #replace = \"^([^ ]+) +[^ ]+ +[^/]+/([^/[ ]{1,4})[^ ]* .*TLS connection established from ([^[]+)\\[([^]]+)\\].*\" \"${4:>15}\\t\\1\\t\\2\\t\\3\"\n"
        "  # Example: FortiGate lines without some fields (like multitail -ke), the\n"
        "  # second field (the host) in 20 characters, srcip in 15, the ports in 5.\n"
        "#[transform fortigate-short]\n"
        "  # The alignments first: the last three rules remove the keys they look for.\n"
        " #replace = \"^([^ ]+) +([^ ]+) +\" \"\\1 ${2:<20} \"\n"
        " #replace = \"srcip=([0-9.]+)\" \"srcip=${1:>15}\"\n"
        " #replace = \"dstport=([0-9]+)\" \"dstport=${1:>5}\"\n"
        " #replace = \"dstintf=\\\"([^\\\"]*)\\\"\" \"dstintf=\\\"${1:<6}\\\"\"\n"
        " #replace = \"app=\\\"([a-z]+)/([0-9]+)\\\"\" \"app=\\\"\\1/${2:>5}\\\"\"\n"
        " #replace = \"devname=.* srcip=\" \"\"\n"
        " #replace = \"srcport=.* dstport=\" \"\"\n"
        " #replace = \"policytype=.* policyname=\" \"\"\n"
        "  # Example: FortiGate lines as aligned columns of some fields.\n"
        "#[transform fortigate-fields]\n"
        " #fields = date time srcip:>15 dstip:>15 dstport:<5 action:<7 policyid\n"
        " #fields-format = values\n"
        "\n"
        "[keys]\n"
        "  # Keys that scroll the history directly (outside of full-screen programs).\n"
        "  # Examples: Alt-Shift-PgUp, Ctrl-F5, none.\n"
        " #history-page-up = " + keyName(d.keyPageUp) + "\n"
        " #history-page-down = " + keyName(d.keyPageDown) + "\n"
        " #history-top = " + keyName(d.keyTop) + "\n"
        " #history-bottom = " + keyName(d.keyBottom) + "\n"
        "  # Global keys: they work in every window (not in dialogs, menus or Grab\n"
        "  # Input). A comma separates several keys; modifiers Ctrl-, Alt-, Shift-;\n"
        "  # keys F1-F12, Left Right Up Down Home End PgUp PgDn Ins Del, and, with Ctrl\n"
        "  # or Alt, Tab Enter Backspace Space a-z 0-9 ` (the key left of 1; also\n"
        "  # written backquote). ':not-far2l' after a key: the key goes to far2l when\n"
        "  # the focused terminal runs far2l with its extensions.\n"
        "  # Key that opens the menu (the first one is shown in the status line),\n"
        "  # e.g. Ctrl-B, Ctrl-`\n"
        " #menu-key = Ctrl-B\n"
        "  # Maximize / restore the focused window (also Ctrl-B Z), e.g. F12:not-far2l\n"
        " #key-zoom =\n"
        "  # Go to the neighbouring window on the left / right / above / below,\n"
        "  # e.g. Alt-Shift-Left, Alt-Shift-Right, Alt-Shift-Up, Alt-Shift-Down\n"
        " #key-window-left =\n"
        " #key-window-right =\n"
        " #key-window-up =\n"
        " #key-window-down =\n";
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
