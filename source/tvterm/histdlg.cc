#define Uses_TDialog
#define Uses_TButton
#define Uses_TInputLine
#define Uses_TLabel
#define Uses_TCheckBoxes
#define Uses_TRadioButtons
#define Uses_TSItem
#define Uses_TStaticText
#define Uses_TProgram
#define Uses_TDeskTop
#define Uses_MsgBox
#include <tvision/tv.h>

#include "histdlg.h"
#include "app.h"
#include "wnd.h"
#include "settings.h"
#include "statline.h"
#include "profile.h"
#include "procinfo.h"
#include <tvterm/termview.h>
#include <tvterm/termctrl.h>
#include <tvterm/termopts.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <string>

// Own namespace: this file is compiled together with others (Unity Build).
namespace histdlg
{

static std::u32string toUtf32(const std::string &s)
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

static std::string toUtf8(const std::u32string &s)
{
    std::string r;
    for (char32_t c : s)
    {
        if (c < 0x80)
            r += (char) c;
        else if (c < 0x800)
            r += (char) (0xC0 | (c >> 6)), r += (char) (0x80 | (c & 0x3F));
        else if (c < 0x10000)
            r += (char) (0xE0 | (c >> 12)), r += (char) (0x80 | ((c >> 6) & 0x3F)),
            r += (char) (0x80 | (c & 0x3F));
        else
            r += (char) (0xF0 | (c >> 18)), r += (char) (0x80 | ((c >> 12) & 0x3F)),
            r += (char) (0x80 | ((c >> 6) & 0x3F)), r += (char) (0x80 | (c & 0x3F));
    }
    return r;
}

static ushort execDlg(TDialog *d, void *data)
{
    TView *p = TProgram::application->validView(d);
    if (!p)
        return cmCancel;
    p->setData(data);
    ushort result = TProgram::deskTop->execView(p);
    if (result != cmCancel)
        p->getData(data);
    TObject::destroy(p);
    return result;
}

static std::string expandHome(const std::string &path)
{
    if (path == "~" || path.compare(0, 2, "~/") == 0)
        if (const char *home = getenv("HOME"))
            return home + path.substr(1);
    return path;
}

static std::string sanitizePathInput(const char *text)
// The path typed in the dialog, without surrounding spaces.
{
    std::string s = text;
    size_t a = s.find_first_not_of(" \t");
    size_t b = s.find_last_not_of(" \t");
    return a == std::string::npos ? std::string() : s.substr(a, b - a + 1);
}

static std::string sanitize(TStringView title)
// Title -> part of a file name: letters, digits, '.', '-' and '_' only.
{
    std::string r;
    for (char c : title)
    {
        bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                  (c >= '0' && c <= '9') || c == '.' || c == '-' || c == '_';
        if (ok)
            r += c;
        else if (!r.empty() && r.back() != '_')
            r += '_';
    }
    while (!r.empty() && (r.back() == '_' || r.back() == '.'))
        r.pop_back();
    while (!r.empty() && r[0] == '.')
        r.erase(0, 1);
    if (r.size() > 40)
        r.resize(40);
    return r.empty() ? std::string("term") : r;
}

static std::string terminalDir(tvterm::TerminalView &view)
// Current directory of what runs in the terminal (as in Save Profile).
{
    tvterm::ClientProcessInfo info;
    if (view.termCtrl.getClientProcessInfo(info))
    {
        long pid = info.foregroundPgid ? info.foregroundPgid : info.clientPid;
        std::string cwd = procinfo::readLink("/proc/" + std::to_string(pid) + "/cwd");
        if (!cwd.empty())
            return cwd;
    }
    char buf[4096];
    if (getcwd(buf, sizeof(buf)))
        return buf;
    return ".";
}

static std::string timeStamp()
{
    char stamp[32] = "";
    time_t now = time(nullptr);
    if (struct tm *tm = localtime(&now))
        strftime(stamp, sizeof(stamp), "%Y-%m-%d-%H%M", tm);
    return stamp;
}

static std::string defaultSavePath(tvterm::TerminalView &view, const std::string &saveDir)
{
    TStringView title;
    if (auto *w = dynamic_cast<TerminalWindow *>(view.owner))
    {
        title = w->getCustomTitle();
        if (title.empty())
            title = w->getTerminalTitle();
    }
    char stamp[32] = "";
    time_t now = time(nullptr);
    if (struct tm *tm = localtime(&now))
        strftime(stamp, sizeof(stamp), "%Y-%m-%d-%H%M", tm);
    std::string dir = saveDir.empty() ? terminalDir(view) : saveDir;
    if (!dir.empty() && dir.back() == '/')
        dir.pop_back();
    return dir + "/tvterm-" + sanitize(title) + "-" + stamp + ".txt";
}

// TDialog only closes itself on the standard commands.
enum { cmOverwrite = cmYes, cmAppend = cmNo };

static ushort askOverwrite(const std::string &path)
// Returns cmOverwrite, cmAppend or cmCancel.
{
    TDialog *d = new TDialog(TRect(0, 0, 60, 9), "File exists");
    d->options |= ofCentered;
    std::string text = "\003" + path + "\n\003already exists.";
    d->insert(new TStaticText(TRect(2, 2, 58, 5), text));
    d->insert(new TButton(TRect(5, 6, 20, 8), "~O~verwrite", cmOverwrite, bfDefault));
    d->insert(new TButton(TRect(22, 6, 37, 8), "~A~ppend", cmAppend, bfNormal));
    d->insert(new TButton(TRect(39, 6, 54, 8), "Cancel", cmCancel, bfNormal));
    d->selectNext(False);
    return execDlg(d, nullptr);
}

static int countLines(const std::string &text)
{
    int n = 0;
    for (char c : text)
        n += c == '\n';
    return n;
}

} // namespace histdlg


void openHistorySearch(tvterm::TerminalView &view)
{
    using namespace histdlg;
    struct
    {
        char text[256];
        ushort options; // 1: case sensitive, 2: regular expression.
    } data {};

    std::u32string prevText;
    bool regex = false, caseSensitive = tvterm::terminalOptions.searchCaseSensitive;
    if (view.getSearch(prevText, regex, caseSensitive))
        strncpy(data.text, toUtf8(prevText).c_str(), sizeof(data.text) - 1);
    data.options = (caseSensitive ? 1 : 0) | (regex ? 2 : 0);

    while (true)
    {
        TDialog *d = new TDialog(TRect(0, 0, 62, 13), "Search history");
        d->options |= ofCentered;
        TInputLine *input = new TInputLine(TRect(3, 3, 59, 4), sizeof(data.text));
        d->insert(input);
        d->insert(new TLabel(TRect(2, 2, 20, 3), "~T~ext to find", input));
        d->insert(new TCheckBoxes(TRect(3, 5, 59, 7),
            new TSItem("~C~ase sensitive",
            new TSItem("~R~egular expression (like grep -E)", nullptr))));
        d->insert(new TStaticText(TRect(3, 7, 59, 9),
            "From the visible lines towards older ones.\n"
            "Next: F3 or n (older), Shift-F3 or N (newer)."));
        d->insert(new TButton(TRect(16, 10, 28, 12), "~F~ind", cmOK, bfDefault));
        d->insert(new TButton(TRect(32, 10, 44, 12), "Cancel", cmCancel, bfNormal));
        d->selectNext(False);
        if (execDlg(d, &data) == cmCancel)
            return;
        std::string error;
        if (!view.setSearch(toUtf32(data.text), data.options & 2, data.options & 1, error))
        {
            messageBox(mfError | mfOKButton, "Invalid regular expression:\n%s", error.c_str());
            continue;
        }
        break;
    }
    if (data.text[0])
        view.findNext(true, true);
}

void openHistorySave(tvterm::TerminalView &view)
{
    using namespace histdlg;
    bool screenOnly = !view.hasScrollback();
    bool hasSelection = view.hasSelection();
    std::string saveDir = TVTermApp::settings().saveDir;

    struct
    {
        char path[MAXPATH];
        ushort what; // 0: everything, 1: selection only.
    } data {};
    std::string proposed = defaultSavePath(view, saveDir);
    strncpy(data.path, proposed.c_str(), sizeof(data.path) - 1);

    TDialog *d = new TDialog(TRect(0, 0, 70, 13), screenOnly ? "Save screen" : "Save history");
    d->options |= ofCentered;
    TInputLine *input = new TInputLine(TRect(3, 3, 67, 4), sizeof(data.path));
    d->insert(input);
    d->insert(new TLabel(TRect(2, 2, 20, 3), "~F~ile", input));
    TRadioButtons *what = new TRadioButtons(TRect(3, 6, 67, 8),
        new TSItem(screenOnly ? "~V~isible screen (full-screen program: no history)"
                              : "~A~ll history and screen",
        new TSItem("Se~l~ection only", nullptr)));
    if (!hasSelection)
        what->setButtonState(2, False);
    d->insert(what);
    d->insert(new TLabel(TRect(2, 5, 20, 6), "~W~hat to save", what));
    d->insert(new TStaticText(TRect(3, 8, 67, 9), "Plain text (UTF-8), without colors."));
    d->insert(new TButton(TRect(21, 10, 33, 12), "~S~ave", cmOK, bfDefault));
    d->insert(new TButton(TRect(37, 10, 49, 12), "Cancel", cmCancel, bfNormal));
    d->selectNext(False);
    if (execDlg(d, &data) == cmCancel)
        return;

    std::string path = expandHome(histdlg::sanitizePathInput(data.path));
    if (path.empty())
        return;
    if (path[0] != '/')
    {
        // Relative to the proposed directory.
        size_t slash = proposed.rfind('/');
        path = proposed.substr(0, slash + 1) + path;
    }

    bool append = false;
    if (access(path.c_str(), F_OK) == 0)
    {
        ushort choice = askOverwrite(path);
        if (choice == cmCancel)
            return;
        append = choice == cmAppend;
    }

    std::string text;
    bool onlyScreen = false;
    view.getHistoryText(text, data.what == 1 && hasSelection, onlyScreen);

    size_t slash = path.rfind('/');
    if (slash != std::string::npos && slash > 0)
        makeDirs(path.substr(0, slash));
    FILE *f = fopen(path.c_str(), append ? "a" : "w");
    bool ok = f && fwrite(text.data(), 1, text.size(), f) == text.size();
    int err = errno;
    if (f && fclose(f) != 0)
        ok = false, err = errno;
    if (!ok)
    {
        messageBox(mfError | mfOKButton, "Cannot save %s:\n%s", path.c_str(), strerror(err));
        TVTermApp::note("cannot save " + path + ": " + strerror(err));
        return;
    }
    std::string msg = std::string(append ? "Appended " : "Saved ") +
                      std::to_string(countLines(text)) + " line(s)" +
                      (onlyScreen ? " (visible screen)" : "") + " to " + path;
    TVTermStatusLine::showMessage(msg + ".");
    TVTermApp::note(msg);
}

void openTextSave( const char *dialogTitle, const std::string &name, bool hasSelection,
                   const std::function<std::string(bool)> &getText )
{
    using namespace histdlg;
    std::string dir = TVTermApp::settings().saveDir;
    if (dir.empty())
    {
        char buf[4096];
        dir = getcwd(buf, sizeof(buf)) ? buf : ".";
    }
    if (!dir.empty() && dir.back() == '/')
        dir.pop_back();
    std::string proposed = dir + "/tvterm-" + sanitize(name) + "-" + timeStamp() + ".txt";

    struct
    {
        char path[MAXPATH];
        ushort what; // 0: everything, 1: selection only.
    } data {};
    strncpy(data.path, proposed.c_str(), sizeof(data.path) - 1);
    data.what = hasSelection ? 1 : 0;

    TDialog *d = new TDialog(TRect(0, 0, 70, 13), dialogTitle);
    d->options |= ofCentered;
    TInputLine *input = new TInputLine(TRect(3, 3, 67, 4), sizeof(data.path));
    d->insert(input);
    d->insert(new TLabel(TRect(2, 2, 20, 3), "~F~ile", input));
    TRadioButtons *what = new TRadioButtons(TRect(3, 6, 67, 8),
        new TSItem("~A~ll the lines shown",
        new TSItem("Se~l~ection only", nullptr)));
    if (!hasSelection)
        what->setButtonState(2, False);
    d->insert(what);
    d->insert(new TLabel(TRect(2, 5, 20, 6), "~W~hat to save", what));
    d->insert(new TStaticText(TRect(3, 8, 67, 9), "Plain text (UTF-8), one line per line shown."));
    d->insert(new TButton(TRect(21, 10, 33, 12), "~S~ave", cmOK, bfDefault));
    d->insert(new TButton(TRect(37, 10, 49, 12), "Cancel", cmCancel, bfNormal));
    d->selectNext(False);
    if (execDlg(d, &data) == cmCancel)
        return;

    std::string path = expandHome(sanitizePathInput(data.path));
    if (path.empty())
        return;
    if (path[0] != '/')
        path = dir + "/" + path;

    bool append = false;
    if (access(path.c_str(), F_OK) == 0)
    {
        ushort choice = askOverwrite(path);
        if (choice == cmCancel)
            return;
        append = choice == cmAppend;
    }

    std::string text = getText(data.what == 1 && hasSelection);
    size_t slash = path.rfind('/');
    if (slash != std::string::npos && slash > 0)
        makeDirs(path.substr(0, slash));
    FILE *f = fopen(path.c_str(), append ? "a" : "w");
    bool ok = f && fwrite(text.data(), 1, text.size(), f) == text.size();
    int err = errno;
    if (f && fclose(f) != 0)
        ok = false, err = errno;
    if (!ok)
    {
        messageBox(mfError | mfOKButton, "Cannot save %s:\n%s", path.c_str(), strerror(err));
        TVTermApp::note("cannot save " + path + ": " + strerror(err));
        return;
    }
    std::string msg = std::string(append ? "Appended " : "Saved ") +
                      std::to_string(countLines(text)) + " line(s) to " + path;
    TVTermStatusLine::showMessage(msg + ".");
    TVTermApp::note(msg);
}
