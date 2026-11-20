#define Uses_TText
#include <tvision/tv.h>

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

// desktop-pattern (E74): 'space', or one character of one column (printable
// ASCII, or one valid UTF-8 character that the screen shows in one column).
static bool parsePattern(const std::string &value, std::string &out)
{
    if (lower(value) == "space")
        return out = " ", true;
    if (value.empty())
        return false;
    unsigned char c = value[0];
    size_t len = c < 0x80 ? 1 : (c >> 5) == 6 ? 2 : (c >> 4) == 14 ? 3 : (c >> 3) == 30 ? 4 : 0;
    if (len == 0 || value.size() != len)
        return false;
    if (len == 1 && (c <= ' ' || c >= 0x7F))
        return false;
    for (size_t i = 1; i < len; ++i)
        if (((unsigned char) value[i] & 0xC0) != 0x80)
            return false;
    if (TText::width(value) != 1)
        return false;
    out = value;
    return true;
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

    enum { None, General, Scrollback, Alerts, Capture, Editor, Keys, LogView, Highlight, Transform, Unknown } section = None;
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
                      name == "capture" ? Capture :
                      name == "editor" ? Editor :
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
            else if (key == "auto-reload-check")
            {
                // Invalid: the default (not the previous value).
                if (!parseInt(value, 1, 60, settings.autoReloadCheck))
                {
                    settings.autoReloadCheck = Settings().autoReloadCheck;
                    warn( "invalid " + key + " '" + value + "' (1 to 60 seconds); using " +
                          std::to_string(settings.autoReloadCheck) );
                }
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
            else if (key == "help-current-first")
            {
                if (!parseBool(value, settings.helpCurrentFirst))
                    invalid("use yes or no"), settings.helpCurrentFirst = previous.helpCurrentFirst;
            }
            else if (key == "window-shadow")
            {
                if (!parseBool(value, settings.windowShadow))
                    invalid("use yes or no"), settings.windowShadow = previous.windowShadow;
            }
            else if (key == "desktop-pattern" || key == "desktop-pattern-on-capture")
            {
                // Invalid: the default (not the previous value).
                bool capture = key == "desktop-pattern-on-capture";
                std::string &out = capture ? settings.desktopPatternOnCapture : settings.desktopPattern;
                if (capture && lower(value) == "same")
                    out.clear();
                else if (!parsePattern(value, out))
                {
                    out = capture ? Settings().desktopPatternOnCapture : Settings().desktopPattern;
                    warn( "invalid " + key + " '" + value + "' (one character of one column, or space" +
                          (capture ? ", or same" : "") + "); using " +
                          (out == " " ? std::string("space") : out) );
                }
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
            else if (key == "file-backups")
            {
                if (!parseBool(value, settings.fileBackups))
                    invalid("use yes or no"), settings.fileBackups = previous.fileBackups;
            }
            else if (key == "file-backups-number")
            {
                // Invalid: the default (not the previous value).
                if (!parseInt(value, 1, 99, settings.fileBackupsNumber))
                {
                    settings.fileBackupsNumber = Settings().fileBackupsNumber;
                    warn( "invalid " + key + " '" + value + "' (1 to 99); using " +
                          std::to_string(settings.fileBackupsNumber) );
                }
            }
            else if (key == "comment-template")
            {
                // Quotes keep spaces at the start or the end.
                std::string v = value;
                if (v.size() >= 2 && v.front() == '"' && v.back() == '"')
                    v = v.substr(1, v.size() - 2);
                if (v.empty() || v.size() > 200)
                    invalid("1 to 200 characters, strftime codes allowed, e.g. \"# %Y-%m-%d %H:%M:%S \""),
                    settings.commentTemplate = previous.commentTemplate;
                else
                    settings.commentTemplate = v;
            }
            else if (key == "comment-width")
            {
                if (!parseInt(value, 0, 1000, settings.commentWidth))
                    invalid("0 to 1000"), settings.commentWidth = previous.commentWidth;
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
            else if (key == "action")
                settings.alertConfig.action = value;
            else if (key == "log")
                settings.alertConfig.log = value;
            else if (key == "action-interval" || key == "notify-repeat")
            {
                int &out = key == "action-interval" ? settings.alertConfig.actionInterval
                                                    : settings.alertConfig.notifyRepeat;
                char *end;
                long n = strtol(value.c_str(), &end, 10);
                if (value.empty() || *end || n < 0 || n > 86400)
                    invalid("use seconds, 0-86400");
                else
                    out = (int) n;
            }
            else if (key == "mute-above")
            {
                // E72: off (0) or the first muted level, 2-5.
                std::string v = lower(value);
                if (v == "off" || v == "0" || v == "no" || v == "none")
                    settings.alertConfig.muteAbove = 0;
                else if (v.size() == 1 && v[0] >= '2' && v[0] <= '5')
                    settings.alertConfig.muteAbove = v[0] - '0';
                else
                    invalid("use off or 2-5"), settings.alertConfig.muteAbove = previous.alertConfig.muteAbove;
            }
            else if (key == "notify")
            {
                // A list: flash notify bell, or none.
                AlertConfig c = settings.alertConfig;
                c.flash = c.notify = c.bell = false;
                bool ok = true, none = false;
                size_t words = 0;
                std::string v = lower(value);
                for (size_t i = 0; i < v.size();)
                {
                    size_t a = v.find_first_not_of(" \t,", i);
                    if (a == std::string::npos)
                        break;
                    size_t b = v.find_first_of(" \t,", a);
                    if (b == std::string::npos)
                        b = v.size();
                    std::string w = v.substr(a, b - a);
                    ++words;
                    if (w == "flash") c.flash = true;
                    else if (w == "notify") c.notify = true;
                    else if (w == "bell") c.bell = true;
                    else if (w == "none") none = true;
                    else ok = false;
                    i = b;
                }
                if (!ok || words == 0 || (none && words > 1))
                    invalid("use flash, notify, bell (one or more) or none");
                else
                    settings.alertConfig = c;
            }
            else
                warn("unknown key '" + key + "' in [alerts]");
        }
        else if (section == Capture)
        {
            if (key == "folder")
                settings.captureFolder = value.empty() ? Settings().captureFolder : value;
            else
                warn("unknown key '" + key + "' in [capture]");
        }
        else if (section == Editor)
        {
            if (key == "show-whitespace")
            {
                if (!parseBool(value, settings.showWhitespace))
                    invalid("use yes or no"), settings.showWhitespace = previous.showWhitespace;
            }
            else if (key == "whitespace-symbols")
            {
                // Five characters of one column, separated by blanks.
                std::vector<std::string> list;
                std::string one;
                bool ok = true;
                for (size_t i = 0; i < value.size() && ok;)
                {
                    size_t a = value.find_first_not_of(" \t", i);
                    if (a == std::string::npos)
                        break;
                    size_t b = value.find_first_of(" \t", a);
                    if (b == std::string::npos)
                        b = value.size();
                    ok = parsePattern(value.substr(a, b - a), one) && one != " ";
                    list.push_back(one);
                    i = b;
                }
                if (ok && list.size() == 5)
                    settings.whitespaceSymbols = list;
                else
                    invalid("five characters of one column for Space, Tab, LF, CRLF and a lone CR, "
                            "separated by spaces, e.g. \u00B7 \u2192 \u00B6 \u00A4 \u2190"),
                    settings.whitespaceSymbols = previous.whitespaceSymbols;
            }
            else if (key == "whitespace-color-2")
            {
                // E103: optional; empty: none.
                std::string error;
                int attr = 0;
                if (value.empty())
                    settings.whitespaceColor2 = -1;
                else if (parseColorName(value, attr, error))
                {
                    settings.whitespaceColor2 = attr;
                    settings.whitespaceColor2Fore = lower(value).find(" on ") == std::string::npos;
                }
                else
                    invalid(error + "; e.g. white, or yellow on blue"),
                    settings.whitespaceColor2 = previous.whitespaceColor2,
                    settings.whitespaceColor2Fore = previous.whitespaceColor2Fore;
            }
            else if (key == "line-numbers")
            {
                // E108.
                if (!parseBool(value, settings.lineNumbers))
                    invalid("use yes or no"), settings.lineNumbers = previous.lineNumbers;
            }
            else if (key == "line-numbers-width")
            {
                if (!parseInt(value, 1, 9, settings.lineNumbersWidth))
                    invalid("1 to 9"), settings.lineNumbersWidth = previous.lineNumbersWidth;
            }
            else if (key == "line-numbers-color" || key == "line-numbers-current-color")
            {
                bool current = key == "line-numbers-current-color";
                int &color = current ? settings.lineNumbersCurrentColor : settings.lineNumbersColor;
                bool &fore = current ? settings.lineNumbersCurrentColorFore : settings.lineNumbersColorFore;
                std::string error;
                int attr = 0;
                if (parseColorName(value, attr, error))
                {
                    color = attr;
                    fore = lower(value).find(" on ") == std::string::npos;
                }
                else
                    invalid(error + (current ? "; e.g. bright-white, or black on cyan" : "; e.g. cyan, or white on blue")),
                    color = current ? previous.lineNumbersCurrentColor : previous.lineNumbersColor,
                    fore = current ? previous.lineNumbersCurrentColorFore : previous.lineNumbersColorFore;
            }
            else if (key == "whitespace-color")
            {
                std::string error;
                int attr = 0;
                if (parseColorName(value, attr, error))
                {
                    settings.whitespaceColor = attr;
                    settings.whitespaceColorFore = lower(value).find(" on ") == std::string::npos;
                }
                else
                    invalid(error + "; e.g. bright-black, or white on blue"),
                    settings.whitespaceColor = previous.whitespaceColor,
                    settings.whitespaceColorFore = previous.whitespaceColorFore;
            }
            else
                warn("unknown key '" + key + "' in [editor]");
        }
        else if (section == LogView)
        {
            if (key == "max-lines")
            {
                if (!parseInt(value, 100, 1000000, settings.logViewMaxLines))
                    invalid("100 to 1000000"), settings.logViewMaxLines = previous.logViewMaxLines;
            }
            else if (key == "cursor-color" || key == "selection-color")
            {
                bool cursor = key == "cursor-color";
                std::string error;
                int attr = 0;
                if (parseColorName(value, attr, error))
                    (cursor ? settings.cursorColor : settings.selectionColor) = (uint8_t) attr;
                else
                    invalid(error + "; e.g. black on cyan, black on bright-white"),
                    (cursor ? settings.cursorColor : settings.selectionColor) =
                        cursor ? previous.cursorColor : previous.selectionColor;
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
                           key == "history-bottom" ? &settings.keyBottom :
                           key == "key-editor-comment" ? &settings.keyEditorComment :
                           key == "key-logviews-end" ? &settings.keyLogViewsEnd :
                           key == "key-editor-indent" ? &settings.keyEditorIndent :
                           key == "key-editor-unindent" ? &settings.keyEditorUnindent :
                           key == "key-editor-whitespace" ? &settings.keyEditorWhitespace :
                           key == "key-logview-pin" ? &settings.keyLogViewPin :
                           key == "key-eventviewer-mute" ? &settings.keyEventViewerMute :
                           key == "key-editor-pin" ? &settings.keyEditorPin :
                           key == "key-editor-line-numbers" ? &settings.keyEditorLineNumbers :
                           key == "key-editor-hide-comments" ? &settings.keyEditorHideComments : nullptr;
            const TKey *prev = key == "history-page-up" ? &previous.keyPageUp :
                               key == "history-page-down" ? &previous.keyPageDown :
                               key == "history-top" ? &previous.keyTop :
                               key == "history-bottom" ? &previous.keyBottom :
                               key == "key-editor-comment" ? &previous.keyEditorComment :
                               key == "key-logviews-end" ? &previous.keyLogViewsEnd :
                               key == "key-editor-indent" ? &previous.keyEditorIndent :
                               key == "key-editor-unindent" ? &previous.keyEditorUnindent :
                               key == "key-editor-whitespace" ? &previous.keyEditorWhitespace :
                               key == "key-logview-pin" ? &previous.keyLogViewPin :
                               key == "key-eventviewer-mute" ? &previous.keyEventViewerMute :
                               key == "key-editor-pin" ? &previous.keyEditorPin :
                               key == "key-editor-line-numbers" ? &previous.keyEditorLineNumbers :
                               key == "key-editor-hide-comments" ? &previous.keyEditorHideComments : nullptr;
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

namespace settingsini
{

// whitespace-symbols as written in the file.
static std::string joinSymbols(const std::vector<std::string> &list)
{
    std::string out;
    for (const std::string &s : list)
        out += (out.empty() ? "" : " ") + s;
    return out;
}

} // namespace settingsini

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
        "  # Reload this file automatically when it changes. auto-reload-check: the\n"
        "  # seconds between the checks of this file, 1 to 60 (2 while the Settings\n"
        "  # editor is open).\n"
        " #auto-reload = yes\n"
        " #auto-reload-check = " + std::to_string(d.autoReloadCheck) + "\n"
        "  # After an automatic reload: status (\"Settings reloaded.\" in the status line\n"
        "  # for 5 seconds) or none. Problems in this file always go to the Event Viewer.\n"
        " #reload-message = status\n"
        "  # Files larger than this ask before they open in tvterm (Save with 'Open\n"
        "  # after saving', Ctrl-B O, Ctrl-B F View file): K, M or G; 0: never ask.\n"
        " #large-file = " + sizeName(d.largeFile) + "\n"
        "  # Programs for files, run in a new terminal window: %f is the full file path\n"
        "  # (quoted for the shell; without %f, the path is added at the end).\n"
        "  # Ctrl-B F View file uses external-viewer directly; large files offer both.\n"
        "  # --mortal: far2l ends with the window (otherwise it stays in the background).\n"
        " #external-viewer = far2l --mortal -v %f\n"
        " #external-editor = far2l --mortal -e %f\n"
        "  # Filters offered in the Filter list of the file dialogs (and by Down in\n"
        "  # the Name field of Ctrl-B O, Ctrl-B F View file, the editor's Save file as),\n"
        "  # separated by spaces, e.g. *.txt *log* *.ini *. The first one is the\n"
        "  # initial filter of Ctrl-B O and View file. A filter typed in the Filter\n"
        "  # field can hold several patterns separated by spaces: *.log *.txt\n"
        " #file-filters = " + settingsini::joinWords(d.fileFilters) + "\n"
        "  # Directories offered in the Folder list of the file dialogs, after Home,\n"
        "  # Profiles and Settings, separated by spaces ('~' is the home directory).\n"
        " #file-dirs = ~/logs /var/log\n"
        "  # Size of the file dialogs (Ctrl-B O, Ctrl-B F View file, Save file as, Load /\n"
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
        "  # Help (F1 in a window that is not a terminal, More... Keys): the keys of\n"
        "  # every kind of window; the section of the window it was opened from is\n"
        "  # shown at the top, its title in yellow. yes: that section also comes first\n"
        "  # in the text; no: the sections in their fixed order.\n"
        " #help-current-first = " + std::string(d.helpCurrentFirst ? "yes" : "no") + "\n"
        "  # The character of the desktop background (where there is no window): one\n"
        "  # character of one column, written as it is (e.g. \u2592 \u2593 \u00b7 .), or space.\n"
        "  # desktop-pattern-on-capture: the one Capture Desk writes there (also\n"
        "  # under the window shadows): one character, space, or same (the\n"
        "  # desktop-pattern).\n"
        " #desktop-pattern = " + d.desktopPattern + "\n"
        " #desktop-pattern-on-capture = space\n"
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
        "  # Backups of a file saved from an editor: at the first save of each editor\n"
        "  # window (and Save As over an existing file), the file is copied to\n"
        "  # name-YYYY-MM-DD-hhmmss (local time); no: no backups. The newest\n"
        "  # file-backups-number (1 to 99) backups of each file are kept, older ones\n"
        "  # are deleted.\n"
        " #file-backups = " + std::string(d.fileBackups ? "yes" : "no") + "\n"
        " #file-backups-number = " + std::to_string(d.fileBackupsNumber) + "\n"
        "  # The comment line that key-editor-comment ([keys], Alt-C) inserts in an\n"
        "  # editor, at the cursor, followed by a line break: comment-template with the\n"
        "  # date and time (local time; %Y year, %m month, %d day, %H:%M:%S time, %%\n"
        "  # a percent sign, other strftime codes too), filled with '-' up to\n"
        "  # comment-width columns (0 to 1000; 0: not filled). Quotes keep the spaces\n"
        "  # at the start or the end of the template.\n"
        " #comment-template = \"" + d.commentTemplate + "\"\n"
        " #comment-width = " + std::to_string(d.commentWidth) + "\n"
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
        "  # (the title text in the color of its level until the window gets the\n"
        "  # focus: 1 and 5 white on red, 2 magenta, 3 brown, 4 cyan) or none (only\n"
        "  # the mark, e.g. [!], in the title).\n"
        " #title-color = red\n"
        "  # Title of a window that got the focus while it had an alert (Ctrl-B G,\n"
        "  # a click), until it loses the focus: yellow (black on yellow), cyan\n"
        "  # or none.\n"
        " #seen-color = yellow\n"
        "  # Alert levels: 1 (most urgent) to 5 (the default), set in the profile or\n"
        "  # Ctrl-B A (alert-keywords = 1:authentication failed, 3:error; alert-silence\n"
        "  # = 1:120; alert-activity = 3:yes). Only level 1 runs an action and notifies.\n"
        "  # Command run for a level 1 alert of a window without its own alert-action\n"
        "  # (sh -c, detached, no output); it gets TVTERM_ALERT_LEVEL, _TEXT, _WINDOW,\n"
        "  # _DESK, _KEYWORD (the keyword group, activity or silent) and _TIME.\n"
        " #action =\n"
        "  # Seconds before the action runs again for the same level, keyword and\n"
        "  # window (0: every time).\n"
        " #action-interval = 60\n"
        "  # File where every alert is written, one line each (every matching line, even\n"
        "  # of the selected Log View): 'date time L<level> <desk>/<window> <keyword> |\n"
        "  # <line>'. Empty: none.\n"
        " #log =\n"
        "  # How a level 1 alert is shown until its window is selected: flash (the A1\n"
        "  # item of the top bar), notify (desktop notification through far2l; needs\n"
        "  # a far2l terminal and a client that shows them), bell; one or more, or none.\n"
        " #notify = flash\n"
        "  # notify and bell repeated every N seconds while not seen (0: once).\n"
        " #notify-repeat = 60\n"
        "  # Mute at start: the alerts of this level and the less urgent ones (2: 2-5,\n"
        "  # 3: 3-5, ..., 5: only 5) are kept but not counted in the top bar, and Go to\n"
        "  # Next Alert skips them; level 1 never. Changed in Ctrl-B A or More... Mute\n"
        "  # until tvterm restarts or a profile is loaded. off or 2-5.\n"
        " #mute-above = off\n"
        "\n"
        "[capture]\n"
        "  # Folder of Ctrl-B More... Capture Desk (Ctrl-F10) and Capture Window\n"
        "  # (Ctrl-F11): dsk-YYYY-MM-DD-hhmmss.txt / wnd-YYYY-MM-DD-hhmmss.txt, the text\n"
        "  # on the screen with the frames, no colors; created if missing.\n"
        " #folder = " + d.captureFolder + "\n"
        "\n"
        "[editor]\n"
        "  # The whitespace view of the editors (key-editor-whitespace in [keys],\n"
        "  # Ctrl-Space, or F9 > Whitespace): its state in a new editor window, yes\n"
        "  # or no; a profile keeps it per window.\n"
        " #show-whitespace = " + std::string(d.showWhitespace ? "yes" : "no") + "\n"
        "  # Symbols shown by the whitespace view, five one-column characters\n"
        "  # separated by spaces: Space, Tab, LF, CRLF, lone CR. Candidates:\n"
        "  # \u00B7 \u2192 \u21B5 \u00A4 \u2190 \u00B6 \u240A \u240D \u00B0 \u00BB (\u21B5 \u240A \u240D are missing in some fonts;\n"
        "  # check with: printf '\u00B7 \u2192 \u21B5 \u00A4 \u2190 \u00B6 \u240A \u240D \u00B0 \u00BB\\n').\n"
        " #whitespace-symbols = " + settingsini::joinSymbols(d.whitespaceSymbols) + "\n"
        "  # Color of the symbols: <color> (on the editor's background) or <color>\n"
        "  # on <color> (the highlight colors below); selected text keeps the\n"
        "  # selection color.\n"
        " #whitespace-color = bright-black\n"
        "  # A second color for the symbols (optional), e.g. white: the key then\n"
        "  # cycles off -> whitespace-color -> whitespace-color-2 -> off ([WS2] in the\n"
        "  # title; a profile keeps it as whitespace = 2). Empty: on / off only.\n"
        " #whitespace-color-2 =\n"
        "  # Line numbers at the left of the editor windows (key-editor-line-numbers\n"
        "  # in [keys], Alt-L, or F9 > Line numbers): their state in a new editor\n"
        "  # window, yes or no; a profile keeps it per window.\n"
        " #line-numbers = " + std::string(d.lineNumbers ? "yes" : "no") + "\n"
        "  # Digits of the line numbers, 1 to 9 (more when the text has more lines);\n"
        "  # a space always follows them.\n"
        " #line-numbers-width = " + std::to_string(d.lineNumbersWidth) + "\n"
        "  # Color of the line numbers and of the current line's number: <color> (on\n"
        "  # the editor's background) or <color> on <color>.\n"
        " #line-numbers-color = cyan\n"
        " #line-numbers-current-color = bright-white\n"
        "\n"
        "[logview]\n"
        "  # Lines kept by each Log View source (Ctrl-B L), 100 to 1000000; a window\n"
        "  # can ask for another number. About 1 KB per line of a firewall log.\n"
        " #max-lines = " + std::to_string(d.logViewMaxLines) + "\n"
        "  # The highlight sets built into tvterm (listed below): yes, or no to use only\n"
        "  # the [highlight] sections of this file.\n"
        " #builtin-highlight = " + std::string(d.builtinHighlight ? "yes" : "no") + "\n"
        "  # Colors of the cursor line (in the focused window) and of the selected\n"
        "  # lines, in the Log Views and the Event Viewer: <color> on <color> (the\n"
        "  # highlight colors below), e.g. black on bright-white. Highlight colors\n"
        "  # are not shown on these lines.\n"
        " #cursor-color = black on cyan\n"
        " #selection-color = black on white\n"
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
        "  # Key that inserts the comment line in an editor (comment-template in\n"
        "  # [general]), e.g. Alt-C, Ctrl-F5 or none.\n"
        " #key-editor-comment = " + keyName(d.keyEditorComment) + "\n"
        "  # Keys that add / remove one space at the start of the current line or of\n"
        "  # every selected line in an editor (empty lines stay; leading Tabs become\n"
        "  # spaces before one is removed), e.g. Alt-I, Alt-U or none.\n"
        " #key-editor-indent = " + keyName(d.keyEditorIndent) + "\n"
        " #key-editor-unindent = " + keyName(d.keyEditorUnindent) + "\n"
        "  # Key that shows / hides the whitespace view of an editor window (spaces,\n"
        "  # tabs and line ends as the symbols of [editor]), e.g. Ctrl-Space or none.\n"
        " #key-editor-whitespace = " + keyName(d.keyEditorWhitespace) + "\n"
        "  # Key that, in the Event Viewer, moves every Log View (all the desktops) to\n"
        "  # its last line, following again (also More... Scroll Log Views to last).\n"
        " #key-logviews-end = " + keyName(d.keyLogViewsEnd) + "\n"
        "  # Key that pins / unpins a Log View: a pinned one is not closed by Ctrl-W or\n"
        "  # Close (also F9 > Pin, and Windows > Pin for terminals), e.g. F6 or none.\n"
        " #key-logview-pin = " + keyName(d.keyLogViewPin) + "\n"
        "  # Key that pins / unpins an editor window (also F9 > Pin), e.g. F6 or none.\n"
        " #key-editor-pin = " + keyName(d.keyEditorPin) + "\n"
        "  # Key that shows / hides the line numbers of an editor window, e.g. Alt-L\n"
        "  # or none.\n"
        " #key-editor-line-numbers = " + keyName(d.keyEditorLineNumbers) + "\n"
        "  # Key that shows an editor window's text without comments and blank lines\n"
        "  # (read only; again: the whole text), e.g. Alt-H or none.\n"
        " #key-editor-hide-comments = " + keyName(d.keyEditorHideComments) + "\n"
        "  # Key that, in the Event Viewer, opens Mute alerts above level... (also\n"
        "  # F9). Ctrl-M differs from Enter only through far2l's extensions\n"
        "  # (putty4far2l); in other terminals it is Enter. E.g. Ctrl-M or none.\n"
        " #key-eventviewer-mute = " + keyName(d.keyEventViewerMute) + "\n"
        "  # Global keys: they work in every window (not in dialogs, menus or Grab\n"
        "  # Input). A comma separates several keys; modifiers Ctrl-, Alt-, Shift-;\n"
        "  # keys F1-F12, Left Right Up Down Home End PgUp PgDn Ins Del, and, with Ctrl\n"
        "  # or Alt, Tab Enter Backspace Space a-z 0-9 ` (the key left of 1; also\n"
        "  # written backquote). ':not-far2l' after a key: the key goes to far2l when\n"
        "  # the focused terminal runs far2l with its extensions.\n"
        "  # Key that opens the menu (the first one is shown in the status line),\n"
        "  # e.g. Ctrl-B, Ctrl-`\n"
        " #menu-key = Ctrl-B\n"
        "  # Maximize / restore the focused window (also Ctrl-B Z Z), e.g. F12:not-far2l\n"
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
