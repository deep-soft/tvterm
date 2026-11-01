#define Uses_TDialog
#define Uses_TButton
#define Uses_TStaticText
#define Uses_TProgram
#define Uses_TDeskTop
#define Uses_MsgBox
#define Uses_TFileDialog
#define Uses_TFileList
#define Uses_TScrollBar
#include <tvision/tv.h>

#include "fileopen.h"
#include "dircompl.h"
#include "app.h"
#include "editwnd.h"
#include "logview.h"
#include "logsrc.h"
#include "settings.h"
#include "wnd.h"
#include <tvterm/termctrl.h>
#include <tvterm/vtermemu.h>

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <string>
#include <vector>

// Own namespace for the helpers: this file is compiled together with others
// (Unity Build).
namespace fileopenimpl
{

static std::string baseName(const std::string &path)
{
    size_t slash = path.rfind('/');
    return slash == std::string::npos ? path : path.substr(slash + 1);
}

static std::string sizeText(long long bytes)
{
    char buf[64];
    if (bytes >= 1024LL * 1024 * 1024)
        snprintf(buf, sizeof(buf), "%.1f GB", bytes / (1024.0 * 1024 * 1024));
    else if (bytes >= 1024LL * 1024)
        snprintf(buf, sizeof(buf), "%.1f MB", bytes / (1024.0 * 1024));
    else if (bytes >= 1024)
        snprintf(buf, sizeof(buf), "%.1f KB", bytes / 1024.0);
    else
        snprintf(buf, sizeof(buf), "%lld bytes", bytes);
    return buf;
}

static void onTermError(const char *reason)
{
    messageBox(mfError | mfOKButton, "Cannot create terminal: %s.", reason);
}

static void runExternal(const std::string &tmpl, const std::string &path, const char *what)
// The program in a new terminal window, titled with the file name; the
// window closes when the program ends.
{
    using namespace tvterm;
    std::string command = fileopen::externalCommand(tmpl, path);
    TRect r = TProgram::deskTop->getExtent();
    ClientOptions options;
    options.command = command.c_str();
    options.keepOpen = false;
    VTermEmulatorFactory factory;
    auto *termCtrl = TerminalController::create( TerminalWindow::viewSize(r),
                                                 factory, onTermError, options );
    if (!termCtrl)
        return;
    auto *term = new TerminalWindow(r, *termCtrl);
    term->setCustomTitle(baseName(path));
    term->launchCommand = command;
    term->launchKeepOpen = false;
    term->closeWhenDone = true;
    TProgram::application->insertWindow(term);
    TVTermApp::note(std::string(what) + ": " + command);
}

static void openEditor(const std::string &path)
{
    TProgram::application->insertWindow(new TVTermEditWindow(TVTermApp::editorRect(), path));
}

static void openViewer(const std::string &path)
// The whole file as fixed lines (like Search > New window).
{
    std::string text;
    FILE *f = fopen(path.c_str(), "rb");
    struct stat st;
    if (f && (fstat(fileno(f), &st) != 0 || !S_ISREG(st.st_mode)))
    {
        fclose(f);
        messageBox(mfError | mfOKButton, "Cannot view %s:\nnot a regular file.", path.c_str());
        return;
    }
    bool ok = f != nullptr;
    if (f)
    {
        char buf[65536];
        size_t n;
        while ((n = fread(buf, 1, sizeof(buf), f)) > 0)
            text.append(buf, n);
        ok = !ferror(f);
        fclose(f);
    }
    if (!ok)
    {
        int err = errno;
        messageBox(mfError | mfOKButton, "Cannot view %s:\n%s", path.c_str(), strerror(err));
        return;
    }
    LogViewOptions o;
    o.command = path;
    o.title = path;
    auto source = LogSource::createFixedFromText(path, text);
    auto *w = new LogViewWindow(TProgram::deskTop->getExtent(), o, source);
    w->setFileView(true);
    TProgram::application->insertWindow(w);
}

// Buttons of the large file dialog (TDialog only closes itself on the
// standard commands).
enum { cmExtViewer = cmYes, cmExtEditor = cmNo, cmInTvterm = cmOK };

static ushort askLargeFile(const std::string &path, long long size, fileopen::Target target)
// Returns cmExtViewer, cmExtEditor, cmInTvterm or cmCancel.
{
    const Settings &s = TVTermApp::settings();
    struct Btn { const char *text; ushort cmd; };
    std::vector<Btn> buttons;
    if (!s.externalViewer.empty())
        buttons.push_back({"External ~V~iewer", cmExtViewer});
    if (!s.externalEditor.empty())
        buttons.push_back({"External ~E~ditor", cmExtEditor});
    bool external = !buttons.empty();
    buttons.push_back({external ? "~t~vterm" : "~O~pen", cmInTvterm});
    buttons.push_back({"Cancel", cmCancel});

    const int width = 72;
    TDialog *d = new TDialog(TRect(0, 0, width, 11), "Large file");
    d->options |= ofCentered;
    std::string text = "\003" + baseName(path) + "\n\003" + sizeText(size) +
                       " (large-file = " + sizeName(s.largeFile) + ")\n\003" +
                       (target == fileopen::Target::Editor ? "Open it in the tvterm editor?" : "View it in tvterm?");
    if (!external)
        text += "\n\003(external-viewer / external-editor in tvterm.ini: not set)";
    d->insert(new TStaticText(TRect(2, 2, width - 2, 6), text));
    // Buttons: as wide as their text, centered on one row.
    std::vector<int> widths;
    int total = 0;
    for (auto &b : buttons)
    {
        int w = (int) strlen(b.text) - (strchr(b.text, '~') ? 2 : 0) + 4;
        widths.push_back(w);
        total += w;
    }
    int gap = 2;
    total += gap * ((int) buttons.size() - 1);
    int x = (width - total) / 2;
    for (size_t i = 0; i < buttons.size(); ++i)
    {
        ushort flags = buttons[i].cmd == cmInTvterm && !external ? bfDefault : bfNormal;
        d->insert(new TButton(TRect(x, 7, x + widths[i], 9), buttons[i].text, buttons[i].cmd, flags));
        x += widths[i] + gap;
    }
    d->selectNext(False);
    TView *p = TProgram::application->validView(d);
    if (!p)
        return cmCancel;
    ushort result = TProgram::deskTop->execView(p);
    TObject::destroy(p);
    return result;
}

// The size the user gave to a file dialog, kept until tvterm exits; it is
// dropped when 'file-dialog-size' changes.
struct RememberedSize
{
    bool set {false};
    TPoint size;
    int settingWidth {0}, settingHeight {0};
};

static RememberedSize rememberedSize;

class SizedFileDialog : public TVTermFileDialog
{
public:
    SizedFileDialog(const std::string &wildCard, const char *title,
                    unsigned short options, unsigned char histId) noexcept;
    void handleEvent(TEvent &ev) override;
    void shutDown() override;

private:
    TPoint initialSize;
};

SizedFileDialog::SizedFileDialog(const std::string &wildCard, const char *title,
                                 unsigned short options, unsigned char histId) noexcept :
    TWindowInit(&TFileDialog::initFrame),
    TVTermFileDialog(wildCard, title, "~N~ame", options, histId)
{
    const Settings &s = TVTermApp::settings();
    TPoint want {0, 0};
    if ( rememberedSize.set && rememberedSize.settingWidth == s.fileDialogWidth &&
         rememberedSize.settingHeight == s.fileDialogHeight )
        want = rememberedSize.size;
    else
    {
        rememberedSize.set = false;
        want = {s.fileDialogWidth, s.fileDialogHeight};
    }
    if (want.x > 0 && want.y > 0)
    {
        // Smaller than the desktop; at least tvision's minimum (locate).
        TPoint desk = TProgram::deskTop ? TProgram::deskTop->size : want;
        want.x = min(want.x, (int) desk.x);
        want.y = min(want.y, (int) desk.y);
        // Centered when inserted (ofCentered).
        TRect r(0, 0, want.x, want.y);
        locate(r);
    }
    initialSize = size;
    if (fileList && s.fileListColumns == 1)
    {
        fileList->numCols = 1;
        if (fileList->vScrollBar)
            fileList->vScrollBar->setStep(max(1, fileList->size.y - 1), 1);
        fileList->drawView();
    }
}

void SizedFileDialog::handleEvent(TEvent &ev)
{
    // Ctrl-F5: resize / move with the keyboard (arrows move, Shift+arrows
    // resize, Enter ends), as in the Quit dialog.
    if (ev.what == evKeyDown && ev.keyDown.keyCode == kbCtrlF5)
    {
        ev.what = evCommand;
        ev.message.command = cmResize;
        ev.message.infoPtr = nullptr;
    }
    TVTermFileDialog::handleEvent(ev);
}

void SizedFileDialog::shutDown()
{
    if (size != initialSize)
    {
        const Settings &s = TVTermApp::settings();
        rememberedSize.set = true;
        rememberedSize.size = size;
        rememberedSize.settingWidth = s.fileDialogWidth;
        rememberedSize.settingHeight = s.fileDialogHeight;
    }
    TVTermFileDialog::shutDown();
}

} // namespace fileopenimpl

namespace fileopen
{

void addFileFilters(const std::vector<std::string> &filters)
{
    for (unsigned char id : {hiOpenFile, hiSaveFileAs, hiViewFile})
        for (size_t i = filters.size(); i-- > 0;)
        {
            const std::string &f = filters[i];
            bool present = false;
            for (int n = historyCount(id), j = 0; j < n && !present; ++j)
            {
                const char *s = historyStr(id, j);
                present = s && f == s;
            }
            if (!present)
                historyAdd(id, f);
        }
}

std::string firstFileFilter()
{
    const auto &f = TVTermApp::settings().fileFilters;
    return f.empty() ? std::string("*.txt") : f.front();
}

std::string shellQuote(const std::string &path)
{
    std::string r = "'";
    for (char c : path)
        if (c == '\'')
            r += "'\\''";
        else
            r += c;
    return r + "'";
}

std::string externalCommand(const std::string &tmpl, const std::string &path)
{
    std::string quoted = shellQuote(path);
    std::string r;
    bool found = false;
    for (size_t i = 0; i < tmpl.size(); ++i)
        if (tmpl[i] == '%' && i + 1 < tmpl.size() && tmpl[i + 1] == 'f')
        {
            r += quoted;
            found = true;
            ++i;
        }
        else
            r += tmpl[i];
    if (!found)
        r += " " + quoted;
    return r;
}

void open(const std::string &path, Target target)
{
    using namespace fileopenimpl;
    const Settings &s = TVTermApp::settings();
    struct stat st;
    if ( s.largeFile > 0 && stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode) &&
         (long long) st.st_size > s.largeFile )
    {
        switch (askLargeFile(path, st.st_size, target))
        {
            case cmExtViewer: runExternal(s.externalViewer, path, "external viewer"); return;
            case cmExtEditor: runExternal(s.externalEditor, path, "external editor"); return;
            case cmInTvterm: break;
            default: return;
        }
    }
    if (target == Target::Editor)
        openEditor(path);
    else
        openViewer(path);
}

void view(const std::string &path)
{
    using namespace fileopenimpl;
    const Settings &s = TVTermApp::settings();
    if (!s.externalViewer.empty())
        runExternal(s.externalViewer, path, "external viewer");
    else
        open(path, Target::Viewer);
}

TFileDialog *newFileDialog(const std::string &wildCard, const char *title,
                           unsigned short options, unsigned char histId)
{
    return new fileopenimpl::SizedFileDialog(wildCard, title, options, histId);
}

} // namespace fileopen
