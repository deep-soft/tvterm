#include "globalkeys.h"

#include <ctype.h>

// Own namespace for the helpers: this file is compiled together with others
// (Unity Build).
namespace globalkeys
{

const char *const settingNames[gaCount] =
{
    "menu-key", "key-zoom", "key-window-left", "key-window-right",
    "key-window-up", "key-window-down",
};

static std::string current = "Ctrl-B";

// Scan code of the key left of 1 (` on US keyboards, „ on Romanian ones).
enum { scanBackquote = 0x29 };

struct NamedKey
{
    const char *name;    // As typed in tvterm.ini (lower case).
    const char *display; // As shown.
    ushort code;
    bool needsCtrlAlt;   // Would block typing if used without Ctrl or Alt.
};

static const NamedKey namedKeys[] =
{
    {"f1", "F1", kbF1, false}, {"f2", "F2", kbF2, false}, {"f3", "F3", kbF3, false},
    {"f4", "F4", kbF4, false}, {"f5", "F5", kbF5, false}, {"f6", "F6", kbF6, false},
    {"f7", "F7", kbF7, false}, {"f8", "F8", kbF8, false}, {"f9", "F9", kbF9, false},
    {"f10", "F10", kbF10, false}, {"f11", "F11", kbF11, false}, {"f12", "F12", kbF12, false},
    {"left", "Left", kbLeft, false}, {"right", "Right", kbRight, false},
    {"up", "Up", kbUp, false}, {"down", "Down", kbDown, false},
    {"home", "Home", kbHome, false}, {"end", "End", kbEnd, false},
    {"pgup", "PgUp", kbPgUp, false}, {"pageup", "PgUp", kbPgUp, false},
    {"pgdn", "PgDn", kbPgDn, false}, {"pagedown", "PgDn", kbPgDn, false},
    {"ins", "Ins", kbIns, false}, {"insert", "Ins", kbIns, false},
    {"del", "Del", kbDel, false}, {"delete", "Del", kbDel, false},
    {"tab", "Tab", kbTab, true}, {"enter", "Enter", kbEnter, true},
    {"backspace", "Backspace", kbBack, true}, {"space", "Space", ' ', true},
};

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
        if ((unsigned char) c < 0x80)
            c = (char) tolower((unsigned char) c);
    return s;
}

static bool hasCtrlOrAlt(ushort mods)
{
    return (mods & (kbCtrlShift | kbAltShift)) != 0;
}

// One key, e.g. "Alt-Shift-Left" or "ctrl-`:not-far2l".
static bool parseOne(const std::string &text, GlobalKey &out, std::string &problem)
{
    std::string t = trim(text);
    GlobalKey k;
    size_t colon = t.find(':');
    if (colon != std::string::npos && colon > 0)
    {
        std::string suffix = lower(trim(t.substr(colon + 1)));
        if (suffix != "not-far2l")
        {
            problem = "unknown suffix ':" + suffix + "' in '" + t + "' (only :not-far2l)";
            return false;
        }
        k.notFar2l = true;
        t = trim(t.substr(0, colon));
    }
    ushort mods = 0;
    std::string rest = t;
    for (;;)
    {
        std::string l = lower(rest);
        size_t len = l.compare(0, 5, "ctrl-") == 0 || l.compare(0, 5, "ctrl+") == 0 ? 5 :
                     l.compare(0, 4, "alt-") == 0 || l.compare(0, 4, "alt+") == 0 ? 4 :
                     l.compare(0, 6, "shift-") == 0 || l.compare(0, 6, "shift+") == 0 ? 6 : 0;
        if (len == 0 || rest.size() == len)
            break;
        mods |= len == 5 ? kbCtrlShift : len == 4 ? kbAltShift : kbShift;
        rest = rest.substr(len);
    }
    std::string name = lower(rest);
    bool needsCtrlAlt = true;
    if (name == "`" || name == "backquote" || name == "\xE2\x80\x9E" /* „ */)
    {
        k.backquote = true;
        k.key = TKey('`', mods);
    }
    else if (name == "esc" || name == "escape")
    {
        problem = "'" + t + "': Esc cannot be used (it starts the escape sequences of terminals)";
        return false;
    }
    else if (name.size() == 1 && ((name[0] >= 'a' && name[0] <= 'z') || (name[0] >= '0' && name[0] <= '9')))
        k.key = TKey((ushort) toupper((unsigned char) name[0]), mods);
    else
    {
        const NamedKey *found = nullptr;
        for (auto &n : namedKeys)
            if (name == n.name)
                found = &n;
        if (!found)
        {
            problem = "unknown key '" + t + "'";
            return false;
        }
        k.key = TKey(found->code, mods);
        needsCtrlAlt = found->needsCtrlAlt;
    }
    if (needsCtrlAlt && !hasCtrlOrAlt(mods))
    {
        problem = "'" + t + "': letters, digits, ` and Tab, Enter, Backspace, Space need Ctrl or Alt";
        return false;
    }
    out = k;
    return true;
}

int actionOf(const std::string &name)
{
    for (int i = 0; i < gaCount; ++i)
        if (name == settingNames[i])
            return i;
    return -1;
}

void parseList(const std::string &value, std::vector<GlobalKey> &keys,
               std::vector<std::string> &problems)
{
    std::string v = trim(value);
    if (v.empty() || lower(v) == "none")
        return;
    size_t start = 0;
    for (;;)
    {
        size_t comma = v.find(',', start);
        std::string item = trim(v.substr(start, comma == std::string::npos ? std::string::npos : comma - start));
        if (!item.empty())
        {
            GlobalKey k;
            std::string problem;
            if (parseOne(item, k, problem))
                keys.push_back(k);
            else
                problems.push_back(problem);
        }
        if (comma == std::string::npos)
            break;
        start = comma + 1;
    }
}

void removeDuplicates(std::vector<GlobalKey> (&keys)[gaCount],
                      std::vector<std::string> &problems)
{
    for (int a = 0; a < gaCount; ++a)
        for (size_t i = 0; i < keys[a].size();)
        {
            int owner = -1;
            for (int b = 0; b <= a && owner < 0; ++b)
                for (size_t j = 0; j < (b == a ? i : keys[b].size()); ++j)
                    if (keys[b][j].sameKey(keys[a][i]))
                    {
                        owner = b;
                        break;
                    }
            if (owner >= 0)
            {
                problems.push_back( std::string(settingNames[a]) + ": " + keys[a][i].name() +
                                    " is already used by " + settingNames[owner] + "; ignored" );
                keys[a].erase(keys[a].begin() + i);
            }
            else
                ++i;
        }
}

GlobalKey defaultMenuKey()
{
    GlobalKey k;
    k.key = TKey('B', kbCtrlShift);
    return k;
}

const std::string &menuKeyName()
{
    return current;
}

void setMenuKeyName(const std::string &name)
{
    current = name;
}

} // namespace globalkeys

bool GlobalKey::sameKey(const GlobalKey &o) const
{
    if (backquote != o.backquote)
        return false;
    if (backquote)
        // Shift is ignored for this key (layouts differ).
        return (key.mods & (kbCtrlShift | kbAltShift)) == (o.key.mods & (kbCtrlShift | kbAltShift));
    return key == o.key;
}

bool GlobalKey::matches(const KeyDownEvent &ev) const
{
    if (backquote)
    {
        if (ev.charScan.scanCode != globalkeys::scanBackquote && ev.charScan.charCode != '`')
            return false;
        bool ctrl = (ev.controlKeyState & kbCtrlShift) != 0;
        bool alt = (ev.controlKeyState & kbAltShift) != 0;
        return ctrl == ((key.mods & kbCtrlShift) != 0) && alt == ((key.mods & kbAltShift) != 0);
    }
    return TKey(ev) == key;
}

std::string GlobalKey::name() const
{
    using namespace globalkeys;
    std::string s;
    if (key.mods & kbCtrlShift) s += "Ctrl-";
    if (key.mods & kbAltShift) s += "Alt-";
    if (key.mods & kbShift) s += "Shift-";
    if (backquote)
        return s + "`";
    for (auto &n : namedKeys)
        if (TKey(n.code, key.mods) == key)
            return s + n.display;
    if (key.code < 0x80 && isalnum(key.code))
        return s + (char) key.code;
    return s + "?";
}
