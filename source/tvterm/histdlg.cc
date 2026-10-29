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
// MAXPATH (not brought by the classes above; with Unity Build it used to come
// from another file of the same group).
#include <tvision/compat/borland/dir.h>

#include "histdlg.h"
#include "dircompl.h"
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

static std::string saveDirFor(tvterm::TerminalView &view, const std::string &saveDir)
// Directory proposed for saving the history, without a final '/'.
{
    std::string dir = saveDir.empty() ? terminalDir(view) : saveDir;
    if (!dir.empty() && dir.back() == '/')
        dir.pop_back();
    return dir;
}

static TStringView windowTitle(tvterm::TerminalView &view)
{
    TStringView title;
    if (auto *w = dynamic_cast<TerminalWindow *>(view.owner))
    {
        title = w->getCustomTitle();
        if (title.empty())
            title = w->getTerminalTitle();
    }
    return title;
}

static std::string proposedName(const std::string &name, const std::string &stamp, bool dateFirst)
// 'name' is already sanitized.
{
    return dateFirst ? stamp + "-tvterm-" + name + ".txt"
                     : "tvterm-" + name + "-" + stamp + ".txt";
}

// Choice made in the "File name" group the last time something was saved
// in this session (-1: none yet). A change of 'save-name' in tvterm.ini
// replaces it.
static int sessionDateFirst = -1;
static bool sessionSetting = false;

static bool initialDateFirst()
{
    bool setting = TVTermApp::settings().saveDateFirst;
    if (sessionDateFirst >= 0 && sessionSetting == setting)
        return sessionDateFirst == 1;
    sessionDateFirst = -1;
    return setting;
}

static void rememberDateFirst(bool dateFirst)
{
    sessionDateFirst = dateFirst ? 1 : 0;
    sessionSetting = TVTermApp::settings().saveDateFirst;
}

// Buttons of the "File name" group.
enum { orderDateFirst = 0, orderNameFirst = 1 };

class OrderButtons : public TRadioButtons
{
public:
    using TRadioButtons::TRadioButtons;
    uint32_t current() const { return value; }
};

class SaveDialog : public TDialog
// When "File name" changes, the File field gets the other form of the
// proposed name, but only while it still holds the proposed name.
{
public:
    TInputLine *input {nullptr};
    OrderButtons *order {nullptr};
    std::string dir, name, stamp;
    uint32_t shown {orderNameFirst};

    // Directory completion in the File field (attached once 'input' exists).
    dircompl::Completion completion;

    SaveDialog(const TRect &bounds, TStringView title) noexcept :
        TWindowInit(&TDialog::initFrame),
        TDialog(bounds, title)
    {
    }

    std::string proposal(uint32_t o) const
    {
        return dir + "/" + proposedName(name, stamp, o == orderDateFirst);
    }

    void handleEvent(TEvent &ev) override
    {
        completion.handleEvent(ev, [this](TEvent &e) { handleOwnEvent(e); });
    }

    void handleOwnEvent(TEvent &ev)
    {
        TDialog::handleEvent(ev);
        if (!input || !order || order->current() == shown)
            return;
        std::string before = proposal(shown);
        shown = order->current();
        if (before == input->data)
        {
            std::string after = proposal(shown);
            char buf[MAXPATH] {};
            strncpy(buf, after.c_str(), sizeof(buf) - 1);
            input->setData(buf);
            input->drawView();
        }
    }
};

static bool runSaveDialog( const char *title, const std::string &dir, TStringView name,
                           TSItem *whatItems, bool hasSelection, const char *note,
                           ushort &what, std::string &path )
// The Save dialog shared by the history and the Log View. 'dir' has no
// final '/'. On Save: 'path' = the typed path (absolute, '~' expanded;
// a relative one is taken from 'dir'), 'what' = the "What to save" choice.
{
    struct
    {
        char path[MAXPATH];
        ushort order; // orderDateFirst or orderNameFirst.
        ushort what;  // 0: everything, 1: selection only.
    } data {};

    SaveDialog *d = new SaveDialog(TRect(0, 0, 70, 16), title);
    d->options |= ofCentered;
    d->dir = dir;
    d->name = sanitize(name);
    d->stamp = timeStamp();
    d->shown = initialDateFirst() ? orderDateFirst : orderNameFirst;
    data.order = (ushort) d->shown;
    data.what = what;
    std::string proposed = d->proposal(d->shown);
    strncpy(data.path, proposed.c_str(), sizeof(data.path) - 1);

    d->input = new TInputLine(TRect(3, 3, 67, 4), sizeof(data.path));
    d->insert(d->input);
    d->insert(new TLabel(TRect(2, 2, 20, 3), "~F~ile", d->input));
    d->order = new OrderButtons(TRect(3, 6, 67, 8),
        new TSItem("~D~ate first",
        new TSItem("Na~m~e first", nullptr)));
    d->insert(d->order);
    d->insert(new TLabel(TRect(2, 5, 20, 6), "File ~n~ame", d->order));
    TRadioButtons *whatButtons = new TRadioButtons(TRect(3, 9, 67, 11), whatItems);
    if (!hasSelection)
        whatButtons->setButtonState(2, False);
    d->insert(whatButtons);
    d->insert(new TLabel(TRect(2, 8, 20, 9), "~W~hat to save", whatButtons));
    d->insert(new TStaticText(TRect(3, 11, 67, 12), note));
    d->insert(new TButton(TRect(21, 13, 33, 15), "~S~ave", cmOK, bfDefault));
    d->insert(new TButton(TRect(37, 13, 49, 15), "Cancel", cmCancel, bfNormal));
    // Last, so that the list opens on top of the other fields.
    d->completion.attach(d, d->input, [d] { return d->dir; });
    d->selectNext(False);
    if (execDlg(d, &data) == cmCancel)
        return false;

    rememberDateFirst(data.order == orderDateFirst);
    what = data.what;
    path = expandHome(sanitizePathInput(data.path));
    if (!path.empty() && path[0] != '/')
        path = dir + "/" + path;
    return true;
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

    ushort what = 0;
    std::string path;
    if (!runSaveDialog( screenOnly ? "Save screen" : "Save history",
                        saveDirFor(view, saveDir), windowTitle(view),
                        new TSItem(screenOnly ? "~V~isible screen (full-screen program: no history)"
                                              : "~A~ll history and screen",
                        new TSItem("Se~l~ection only", nullptr)),
                        hasSelection, "Plain text (UTF-8), without colors.", what, path ))
        return;
    if (path.empty())
        return;

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
    view.getHistoryText(text, what == 1 && hasSelection, onlyScreen);

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
    ushort what = hasSelection ? 1 : 0;
    std::string path;
    if (!runSaveDialog( dialogTitle, dir, name,
                        new TSItem("~A~ll the lines shown",
                        new TSItem("Se~l~ection only", nullptr)),
                        hasSelection, "Plain text (UTF-8), one line per line shown.", what, path ))
        return;
    if (path.empty())
        return;

    bool append = false;
    if (access(path.c_str(), F_OK) == 0)
    {
        ushort choice = askOverwrite(path);
        if (choice == cmCancel)
            return;
        append = choice == cmAppend;
    }

    std::string text = getText(what == 1 && hasSelection);
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
