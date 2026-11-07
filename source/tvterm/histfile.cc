#define Uses_TKeys
#include <tvision/tv.h>

#include "histfile.h"
#include "fileopen.h"
#include "settings.h"
#include "app.h"
#include "logview.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include <algorithm>
#include <fstream>
#include <map>
#include <set>
#include <string>
#include <vector>

// tvision's history buffer (histlist.cpp): not declared in its headers.
extern ushort historySize;
void clearHistory() noexcept;

namespace histfile
{

// The saved lists: their names in the file and their tvision ids, or, for
// the lists kept by tvterm itself (newest first), a function that returns
// the list.
using Values = std::vector<std::string> &(*)();

struct List
{
    const char *name;
    unsigned char id;
    Values values {nullptr};
};

static std::vector<std::string> &logInclude() { return *logFieldHistory("include"); }
static std::vector<std::string> &logExclude() { return *logFieldHistory("exclude"); }
static std::vector<std::string> &logHighlight() { return *logFieldHistory("highlight"); }
static std::vector<std::string> &logWords() { return *logFieldHistory("words"); }
static std::vector<std::string> &logTransform() { return *logFieldHistory("transform"); }

static const List lists[] =
{
    {"log-command", 76},
    {"log-include", 0, logInclude},
    {"log-exclude", 0, logExclude},
    {"log-highlight", 0, logHighlight},
    {"log-highlight-words", 0, logWords},
    {"log-transform", 0, logTransform},
    {"open-file", fileopen::hiOpenFile},
    {"save-file-as", fileopen::hiSaveFileAs},
    {"view-file", fileopen::hiViewFile},
    {"load-profile", fileopen::hiLoadProfile},
    {"save-profile", fileopen::hiSaveProfile},
    {"editor-find", 105},
    {"editor-replace", 106},
    {"go-to-line", 107},
    {"window-list-filter", hiWindowListFilter},
    {"file-filter", 0, fileopen::usedFileFilters},
};

// Entries are kept oldest first, as in tvision's lists.
using Entries = std::vector<std::string>;

// tvision limits an entry to 255 bytes with its header (HistRec).
static const size_t maxLength = 250;

// Deleted with Ctrl-Del since tvterm started: not taken back from the file.
static std::map<std::string, std::set<std::string>> deleted;
static unsigned long long savedSignature = 0;
static bool haveSignature = false;
static time_t lastCheck = 0;

std::string path()
{
    std::string p = settingsPath();
    size_t slash = p.rfind('/');
    return (slash == std::string::npos ? std::string(".") : p.substr(0, slash)) + "/history";
}

void enlargeBuffer()
{
    historySize = 60000;
}

static Entries current(const List &l)
{
    Entries out;
    if (l.values)
    {
        // Newest first in tvterm's own lists.
        const std::vector<std::string> &v = l.values();
        out.assign(v.rbegin(), v.rend());
    }
    else
    {
        // The 'file-filters' patterns are added from tvterm.ini at every
        // start: not saved (a pattern removed from tvterm.ini goes away).
        bool skipFilters = l.id == fileopen::hiOpenFile || l.id == fileopen::hiSaveFileAs ||
                           l.id == fileopen::hiViewFile;
        const std::vector<std::string> &filters = TVTermApp::settings().fileFilters;
        for (int n = historyCount(l.id), i = 0; i < n; ++i)
            if (const char *s = historyStr(l.id, i))
                if (!skipFilters || std::find(filters.begin(), filters.end(), s) == filters.end())
                    out.push_back(s);
    }
    return out;
}

static unsigned long long signature()
{
    unsigned long long h = 1469598103934665603ULL;
    auto add = [&h] (const char *s, size_t n) {
        for (size_t i = 0; i < n; ++i)
            h = (h ^ (unsigned char) s[i]) * 1099511628211ULL;
    };
    for (const List &l : lists)
    {
        add(l.name, strlen(l.name) + 1);
        for (const std::string &e : current(l))
            add(e.c_str(), e.size() + 1);
    }
    return h;
}

// Escapes: '\\' and line ends, so that an entry is one line of the file.
static std::string escape(const std::string &s)
{
    std::string out;
    for (char c : s)
        if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else out += c;
    return out;
}

static std::string unescape(const std::string &s)
{
    std::string out;
    for (size_t i = 0; i < s.size(); ++i)
    {
        if (s[i] == '\\' && i + 1 < s.size())
        {
            char c = s[++i];
            out += c == 'n' ? '\n' : c == 'r' ? '\r' : c;
        }
        else
            out += s[i];
    }
    return out;
}

// The file as lists by name (entries oldest first). Unknown sections are
// kept, so that a newer tvterm's lists survive an older one.
static std::map<std::string, Entries> readFile(std::vector<std::string> &order)
{
    std::map<std::string, Entries> out;
    std::ifstream f(path());
    std::string line, section;
    while (std::getline(f, line))
    {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        if (line.empty() || line.compare(0, 2, "##") == 0)
            continue;
        if (line.size() > 2 && line[0] == '[' && line.back() == ']')
        {
            section = line.substr(1, line.size() - 2);
            if (!out.count(section))
                order.push_back(section);
            out[section];
            continue;
        }
        if (section.empty() || line.compare(0, 2, "- ") != 0)
            continue;
        std::string e = unescape(line.substr(2));
        if (!e.empty() && e.size() <= maxLength)
            out[section].push_back(e);
    }
    return out;
}

// 'mine' (this instance, oldest first) and 'theirs' (the file): the file's
// entries that this instance has not got (nor deleted) come first, as the
// older ones; then the last maxEntries.
static Entries merge(const Entries &mine, const Entries &theirs, const std::set<std::string> &gone)
{
    Entries out;
    std::set<std::string> have(mine.begin(), mine.end());
    for (const std::string &e : theirs)
        if (!have.count(e) && !gone.count(e) && std::find(out.begin(), out.end(), e) == out.end())
            out.push_back(e);
    for (const std::string &e : mine)
        if (e.size() <= maxLength)
            out.push_back(e);
    if (out.size() > (size_t) maxEntries)
        out.erase(out.begin(), out.end() - maxEntries);
    return out;
}

static void makeDirs(const std::string &file)
{
    for (size_t i = 1; (i = file.find('/', i)) != std::string::npos; ++i)
        mkdir(file.substr(0, i).c_str(), 0700);
}

void save()
{
    std::vector<std::string> order;
    std::map<std::string, Entries> file = readFile(order);
    std::string text = "## tvterm history: written by tvterm (lists of the input fields, oldest first;\n"
                       "## at most " + std::to_string(maxEntries) + " per list). Ctrl-Del in a list deletes an entry.\n";
    std::set<std::string> written;
    for (const List &l : lists)
    {
        Entries e = merge(current(l), file[l.name], deleted[l.name]);
        written.insert(l.name);
        if (e.empty())
            continue;
        text += std::string("\n[") + l.name + "]\n";
        for (const std::string &x : e)
            text += "- " + escape(x) + "\n";
    }
    for (const std::string &name : order)
        if (!written.count(name) && !file[name].empty())
        {
            text += "\n[" + name + "]\n";
            for (const std::string &x : file[name])
                text += "- " + escape(x) + "\n";
        }
    std::string p = path();
    makeDirs(p);
    std::string tmp = p + ".tmp-" + std::to_string((long) getpid());
    int fd = open(tmp.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fd < 0)
        return;
    bool ok = write(fd, text.data(), text.size()) == (ssize_t) text.size();
    ok = close(fd) == 0 && ok;
    if (!ok || rename(tmp.c_str(), p.c_str()) != 0)
        unlink(tmp.c_str());
    savedSignature = signature();
    haveSignature = true;
}

void load()
{
    std::vector<std::string> order;
    std::map<std::string, Entries> file = readFile(order);
    for (const List &l : lists)
    {
        Entries &e = file[l.name];
        if (e.size() > (size_t) maxEntries)
            e.erase(e.begin(), e.end() - maxEntries);
        if (l.values)
            l.values().assign(e.rbegin(), e.rend());
        else
            for (const std::string &x : e)
                historyAdd(l.id, x);
    }
    savedSignature = signature();
    haveSignature = true;
}

void poll()
{
    time_t now = time(nullptr);
    if (now == lastCheck)
        return;
    lastCheck = now;
    unsigned long long s = signature();
    if (!haveSignature || s != savedSignature)
        save();
}

static const List *listOf(unsigned char id)
{
    for (const List &l : lists)
        if (l.id == id && id != 0)
            return &l;
    return nullptr;
}

bool remove(unsigned char id, const std::string &text)
{
    // tvision cannot delete one entry: every list is read, the buffer
    // cleared and filled again without it (each list keeps its order).
    bool found = false;
    std::vector<std::pair<unsigned char, std::string>> all;
    for (int i = 1; i < 256; ++i)
        for (int n = historyCount((unsigned char) i), j = 0; j < n; ++j)
            if (const char *s = historyStr((unsigned char) i, j))
            {
                if (i == id && text == s)
                    found = true;
                else
                    all.emplace_back((unsigned char) i, s);
            }
    if (!found)
        return false;
    clearHistory();
    for (auto &e : all)
        historyAdd(e.first, e.second);
    if (const List *l = listOf(id))
    {
        deleted[l->name].insert(text);
        save();
    }
    return true;
}

void removed(const std::vector<std::string> &list, const std::string &text)
{
    for (const List &l : lists)
        if (l.values && &l.values() == &list)
        {
            deleted[l.name].insert(text);
            save();
            return;
        }
}

} // namespace histfile
