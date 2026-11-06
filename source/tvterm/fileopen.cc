#define Uses_TDialog
#define Uses_TButton
#define Uses_TStaticText
#define Uses_TProgram
#define Uses_TDeskTop
#define Uses_MsgBox
#define Uses_TFileDialog
#define Uses_TFileList
#define Uses_TScrollBar
#define Uses_TCheckBoxes
#define Uses_TSItem
#define Uses_TLabel
#define Uses_TInputLine
#define Uses_THistory
#define Uses_THistoryWindow
#define Uses_THistoryViewer
#define Uses_TFileInfoPane
#define Uses_TDrawBuffer
#define Uses_TText
#include <tvision/tv.h>

#include "fileopen.h"
#include "dircompl.h"
#include "app.h"
#include "editwnd.h"
#include "logview.h"
#include "logsrc.h"
#include "settings.h"
#include "wnd.h"
#include "profile.h"
#include <tvterm/termctrl.h>
#include <tvterm/vtermemu.h>

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <algorithm>
#include <functional>
#include <sstream>
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

// The row of controls above the list of files (all file dialogs):
//
//   [ ] Hidden  Filter: [*.ini     ]↓  Folder: [~/.config/tvterm/profiles ]↓
//
// Hidden shows the names that begin with '.'; Filter holds the patterns of
// the listed files (several, separated by spaces); Folder shows the listed
// directory and changes it. The list of files is read by tvterm (tvision
// hides the names with '.' and takes one pattern only).

// Kept while tvterm runs, shared by all file dialogs.
static bool showHidden = false;
static std::vector<std::string> usedFilters;    // Newest first.

enum : unsigned char { hiFieldList = 105 };     // Never added to.

// File attributes of TSearchRec (FA_* in tvision's compat/borland/dos.h).
enum : unsigned char { attrReadOnly = 0x01, attrDirectory = 0x10, attrArchive = 0x20 };

static std::string homeDir()
{
    const char *home = getenv("HOME");
    std::string h = home ? home : "";
    while (h.size() > 1 && h.back() == '/')
        h.pop_back();
    return h;
}

static std::string expandHome(const std::string &path)
{
    if (path == "~")
        return homeDir();
    if (path.compare(0, 2, "~/") == 0)
        return homeDir() + path.substr(1);
    return path;
}

static std::string tildePath(const std::string &path)
// "/home/x/dir" as "~/dir"; the home directory itself as "~".
{
    std::string home = homeDir();
    if (home.size() > 1 && path.compare(0, home.size(), home) == 0)
    {
        if (path.size() == home.size())
            return "~";
        if (path[home.size()] == '/')
            return "~" + path.substr(home.size());
    }
    return path;
}

static std::string trimSpaces(const std::string &s)
{
    size_t a = s.find_first_not_of(" \t");
    if (a == std::string::npos)
        return "";
    size_t b = s.find_last_not_of(" \t");
    return s.substr(a, b - a + 1);
}

static std::vector<std::string> splitPatterns(const std::string &text)
{
    std::vector<std::string> r;
    std::istringstream words(text);
    for (std::string w; words >> w;)
        r.push_back(w == "*.*" ? "*" : w);
    return r;
}

// Like tvision's findfirst ('*' and '?', case sensitive).
static bool wildcardMatch(const char *wildcard, const char *name)
{
    for (; *wildcard != '\0'; ++wildcard)
        switch (*wildcard)
        {
            case '?':
                if (*name == '\0')
                    return false;
                ++name;
                break;
            case '*':
                if (wildcard[1] == '\0')
                    return true;
                for (size_t i = 0; name[i] != '\0'; ++i)
                    if (wildcardMatch(wildcard + 1, &name[i]))
                        return true;
                return false;
            default:
                if (*name != *wildcard)
                    return false;
                ++name;
        }
    return *name == '\0';
}

static int32_t dosTime(time_t t)
// The date and time as tvision keeps them (TFileInfoPane shows them).
{
    struct tm lt;
    if (!localtime_r(&t, &lt))
        return 0;
    unsigned date = (unsigned) lt.tm_mday | ((unsigned) (lt.tm_mon + 1) << 5) |
                    ((unsigned) max(0, lt.tm_year - 80) << 9);
    unsigned tim = (unsigned) (lt.tm_sec / 2) | ((unsigned) lt.tm_min << 5) |
                   ((unsigned) lt.tm_hour << 11);
    return (int32_t) ((date << 16) | (tim & 0xFFFF));
}

// The list of files: directories shown with '/' (tvision: '\').
class FileListView : public TFileList
{
public:
    FileListView(const TRect &bounds, TScrollBar *sb) noexcept :
        TFileList(bounds, sb)
    {
    }

    void getText(char *dest, short item, short maxChars) override
    {
        TSearchRec *f = (TSearchRec *) list()->at(item);
        std::string text = f->name;
        if (f->attr & attrDirectory)
            text += '/';
        if (maxChars < 0)
            maxChars = 0;
        strnzcpy(dest, text.c_str(), (size_t) maxChars + 1);
    }
};

// The fields of the row take no part in the dialog's data: TFileDialog
// gives setData() a buffer of MAXPATH characters (the Name field only),
// which TDialog would otherwise share among all its fields.
class RowInputLine : public TInputLine
{
public:
    RowInputLine(const TRect &bounds, int maxLen) noexcept :
        TInputLine(bounds, maxLen)
    {
    }

    ushort dataSize() override { return 0; }
    void getData(void *) override {}
    void setData(void *) override {}

    void setText(const std::string &text)
    {
        strnzcpy(data, text, (size_t) maxLen + 1);
        selectAll(True);
        drawView();
    }
};

// The Folder field: while it is not focused, a path wider than the field
// shows its end, with '…' in front.
class FolderInputLine : public RowInputLine
{
public:
    FolderInputLine(const TRect &bounds, int maxLen) noexcept :
        RowInputLine(bounds, maxLen)
    {
    }

    void draw() override
    {
        if ((state & sfFocused) || size.x < 4)
        {
            RowInputLine::draw();
            return;
        }
        TDrawBuffer b;
        TColorAttr color = getColor(1);
        b.moveChar(0, ' ', color, size.x);
        TStringView text = data;
        int room = size.x - 2;
        if (strwidth(text) > room)
        {
            // Drop characters from the start until '…' and the rest fit.
            size_t i = 0;
            while (i < text.size() && 1 + strwidth(text.substr(i)) > room)
                i += max<size_t>(1, TText::next(text.substr(i)));
            b.moveStr(1, "\xE2\x80\xA6", color);
            b.moveStr(2, text.substr(i), color, room - 1);
        }
        else
            b.moveStr(1, text, color, room);
        writeLine(0, 0, size.x, size.y, b);
    }
};

struct ListItem
{
    std::string text;   // Shown in the list.
    std::string value;  // Put into the field; empty: a note, not chosen.
};

// The list opened by the arrow next to Filter and Folder.
class ItemListViewer : public THistoryViewer
{
public:
    ItemListViewer( const TRect &bounds, TScrollBar *h, TScrollBar *v,
                    std::vector<ListItem> aItems ) noexcept :
        THistoryViewer(bounds, h, v, hiFieldList),
        items(std::move(aItems))
    {
        setRange((short) items.size());
        focusItem(0);
        int width = 0;
        for (auto &item : items)
            width = max(width, (int) strwidth(item.text));
        if (hScrollBar)
            hScrollBar->setRange(0, width - size.x + 3);
    }

    void getText(char *dest, short item, short maxChars) override
    {
        *dest = '\0';
        if (item >= 0 && (size_t) item < items.size())
            strnzcpy(dest, items[item].text.c_str(), (size_t) maxChars + 1);
    }

    const ListItem *focusedItem() const
    {
        return focused >= 0 && (size_t) focused < items.size() ? &items[focused] : nullptr;
    }

private:
    std::vector<ListItem> items;
};

class ItemListWindow : public THistoryWindow
{
public:
    // The items are taken by the viewer, which is created by the base class.
    ItemListWindow(const TRect &bounds, std::vector<ListItem> aItems) noexcept :
        TWindowInit(&TWindow::initFrame),
        THistInit(&ItemListWindow::initList),
        THistoryWindow((pending = std::move(aItems), bounds), hiFieldList)
    {
    }

    const ListItem *focusedItem() const
    {
        return viewer ? static_cast<ItemListViewer *>(viewer)->focusedItem() : nullptr;
    }

private:
    static std::vector<ListItem> pending;

    static TListViewer *initList(TRect r, TWindow *win, ushort)
    {
        r.grow(-1, -1);
        return new ItemListViewer( r,
            win->standardScrollBar(sbHorizontal | sbHandleKeyboard),
            win->standardScrollBar(sbVertical | sbHandleKeyboard),
            std::move(pending) );
    }
};

std::vector<ListItem> ItemListWindow::pending;

// The arrow next to a field (like THistory): a click, or Down in the
// field, opens the list; the item chosen goes to 'chosen'.
class ItemListButton : public THistory
{
public:
    ItemListButton( const TRect &bounds, TInputLine *aLink,
                    std::function<std::vector<ListItem>()> aItems,
                    std::function<void(const std::string &)> aChosen ) noexcept :
        THistory(bounds, aLink, hiFieldList),
        items(std::move(aItems)),
        chosen(std::move(aChosen))
    {
    }

    void recordHistory(const char *) override {}

    void handleEvent(TEvent &ev) override
    {
        TView::handleEvent(ev);
        if ( ev.what == evMouseDown ||
             ( ev.what == evKeyDown && ctrlToArrow(ev.keyDown.keyCode) == kbDown &&
               (link->state & sfFocused) ) )
        {
            clearEvent(ev);
            if (!link->focus())
                return;
            auto list = items();
            if (list.empty())
                list.push_back({"(empty)", ""});
            // Below the field (on it, like THistory), moved up if it does not fit.
            TRect ext = owner->getExtent();
            TRect r = link->getBounds();
            r.a.x--;
            r.b.x++;
            int width = 0;
            for (auto &item : list)
                width = max(width, (int) strwidth(item.text));
            r.b.x = max(r.b.x, r.a.x + min(width + 4, 60));
            r.a.y--;
            r.b.y = r.a.y + (int) max<size_t>(5, min<size_t>(list.size(), 12)) + 2;
            if (r.b.y > ext.b.y - 1)
                r.move(0, ext.b.y - 1 - r.b.y);
            if (r.b.x > ext.b.x - 1)
                r.move(ext.b.x - 1 - r.b.x, 0);
            r.intersect(ext);
            auto *win = new ItemListWindow(r, std::move(list));
            std::string value;
            if (owner->execView(win) == cmOK)
                if (const ListItem *item = win->focusedItem())
                    value = item->value;
            destroy(win);
            if (!value.empty())
                chosen(value);
        }
    }

private:
    std::function<std::vector<ListItem>()> items;
    std::function<void(const std::string &)> chosen;
};

// [ ] Hidden: tells the dialog when it changes.
class HiddenBox : public TCheckBoxes
{
public:
    HiddenBox(const TRect &bounds, std::function<void()> aChanged) noexcept :
        TCheckBoxes(bounds, new TSItem("~H~idden", nullptr)),
        changed(std::move(aChanged))
    {
    }

    void press(int item) override
    {
        TCheckBoxes::press(item);
        changed();
    }

    // No part in the dialog's data (see RowInputLine).
    ushort dataSize() override { return 0; }
    void getData(void *) override {}
    void setData(void *) override {}

    bool checked() const { return (value & 1) != 0; }
    void setChecked(bool on) { value = on ? 1 : 0; drawView(); }

private:
    std::function<void()> changed;
};

class SizedFileDialog : public TVTermFileDialog
{
public:
    SizedFileDialog(const std::string &wildCard, const char *title,
                    unsigned short options, unsigned char histId) noexcept;
    void handleEvent(TEvent &ev) override;
    void shutDown() override;
    Boolean valid(ushort command) override;
    void changeBounds(const TRect &bounds) override;

    // Lists 'dir' (a path with '/'); the profile dialogs.
    void setDirectory(const std::string &dir);

private:
    TPoint initialSize;
    HiddenBox *hiddenBox {nullptr};
    TLabel *filterLabel {nullptr};
    RowInputLine *filterInput {nullptr};
    ItemListButton *filterButton {nullptr};
    TLabel *folderLabel {nullptr};
    FolderInputLine *folderInput {nullptr};
    ItemListButton *folderButton {nullptr};

    void addControlRow();
    void layoutRow();
    void readList(bool keepName = false);
    void showFilter();
    void showFolder();
    bool applyFilter(const std::string &text);
    bool applyFolder(const std::string &text);
    std::string currentDir() const;
    std::vector<ListItem> filterItems() const;
    std::vector<ListItem> folderItems() const;
    void fixNameField(TEvent &ev);

    // The Name field as the list of files last set it; another text was
    // typed (or given by the program) and is kept when Hidden or Filter
    // change.
    std::string autoName;
};

SizedFileDialog::SizedFileDialog(const std::string &wildCard, const char *title,
                                 unsigned short options, unsigned char histId) noexcept :
    TWindowInit(&TFileDialog::initFrame),
    TVTermFileDialog(wildCard, title, "~N~ame", options, histId)
{
    addControlRow();
    // The Name field first, as in tvision (it keeps the filter then).
    fileName->select();
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
    layoutRow();
    if ((options & fdNoLoadDir) == 0)
        readList();
    else
        showFilter();
}

void SizedFileDialog::addControlRow()
// Replaces tvision's list of files with FileListView and adds the row of
// controls between the Name field and the list.
{
    // tvision's list and its "Files" label.
    TView *oldList = fileList;
    TLabel *oldLabel = nullptr;
    TScrollBar *sb = (TScrollBar *) fileList->vScrollBar;
    if (!sb)
        sb = (TScrollBar *) fileList->hScrollBar;
    {
        TView *v = first();
        do
        {
            if (TLabel *l = dynamic_cast<TLabel *>(v))
            {
                TRect lb = l->getBounds();
                if (lb.a.y == oldList->origin.y - 1)
                    oldLabel = l;
            }
            v = v->next;
        } while (v != first());
    }
    auto *list = new FileListView(oldList->getBounds(), sb);
    list->growMode = oldList->growMode;
    insertBefore(list, oldList);
    remove(oldList);
    TObject::destroy(oldList);
    fileList = list;
    if (oldLabel)
    {
        auto *label = new TLabel(oldLabel->getBounds(), "~F~iles", list);
        label->growMode = oldLabel->growMode;
        insertBefore(label, oldLabel);
        remove(oldLabel);
        TObject::destroy(oldLabel);
    }

    int y = fileName->origin.y + 1;
    // Positions are set by layoutRow().
    hiddenBox = new HiddenBox(TRect(0, y, 1, y + 1), [this] {
        showHidden = hiddenBox->checked();
        readList(true);
    });
    filterInput = new RowInputLine(TRect(0, y, 1, y + 1), MAXPATH);
    filterLabel = new TLabel(TRect(0, y, 1, y + 1), "Fil~t~er:", filterInput);
    filterButton = new ItemListButton(TRect(0, y, 1, y + 1), filterInput,
        [this] { return filterItems(); },
        [this](const std::string &v) {
            filterInput->setText(v);
            if (applyFilter(v))
                fileList->select();
        });
    folderInput = new FolderInputLine(TRect(0, y, 1, y + 1), MAXPATH);
    folderLabel = new TLabel(TRect(0, y, 1, y + 1), "Fol~d~er:", folderInput);
    folderButton = new ItemListButton(TRect(0, y, 1, y + 1), folderInput,
        [this] { return folderItems(); },
        [this](const std::string &v) {
            if (applyFolder(v))
                fileList->select();
        });
    // Each one right after the list in the list of views: Tab goes from the
    // Name field to Hidden, Filter, Folder and the list.
    for (TView *v : std::initializer_list<TView *> {hiddenBox, filterLabel, filterInput, filterButton,
                                                    folderLabel, folderInput, folderButton})
    {
        v->growMode = 0;
        insertBefore(v, fileList->next);
    }
    hiddenBox->setChecked(showHidden);
}

void SizedFileDialog::layoutRow()
// One row from the left edge of the list to its right edge: Hidden and
// Filter always; Folder when at least 8 columns are left for its field.
{
    if (!hiddenBox || !fileList)
        return;
    int y = fileName->origin.y + 1;
    int left = fileList->origin.x - 1;
    int right = fileList->origin.x + fileList->size.x;
    auto place = [y](TView *v, int a, int b) {
        // locate() also redraws what the view leaves uncovered.
        TRect r(a, y, b, y + 1);
        v->locate(r);
    };
    int x = left;
    place(hiddenBox, x, x + 12);            // " [ ] Hidden "
    x += 12;
    place(filterLabel, x, x + 8);           // " Filter:"
    x += 8;
    int rest = right - x;
    // Filter: 10 columns (14 when the row is wide), its arrow 3.
    const int folderMin = 8 + 8 + 3;        // label, field, arrow
    int filterWidth = rest - 3 - folderMin >= 14 + 6 ? 14 : 10;
    bool folder = rest - 3 - filterWidth - 1 >= folderMin;
    if (!folder)
        filterWidth = max(4, rest - 3);
    place(filterInput, x, x + filterWidth);
    x += filterWidth;
    place(filterButton, x, x + 3);
    x += 3;
    if (folder)
    {
        x += 1;
        place(folderLabel, x, x + 8);        // " Folder:"
        x += 8;
        place(folderInput, x, right - 3);
        place(folderButton, right - 3, right);
        folderLabel->show();
        folderInput->show();
        folderButton->show();
    }
    else
    {
        folderLabel->hide();
        folderInput->hide();
        folderButton->hide();
    }
}

void SizedFileDialog::changeBounds(const TRect &bounds)
{
    TVTermFileDialog::changeBounds(bounds);
    layoutRow();
}

std::string SizedFileDialog::currentDir() const
{
    std::string dir = dircompl::unixDir(directory);
    if (dir.empty())
        dir = "/";
    if (dir.back() != '/')
        dir += '/';
    return dir;
}

void SizedFileDialog::readList(bool keepName)
// The files of the directory that match the filter, the directories (both
// without the hidden ones unless Hidden is checked) and '..'. 'keepName':
// a name typed in the Name field stays.
{
    std::string typed;
    bool keep = keepName && fileName && autoName != fileName->data;
    if (keep)
        typed = fileName->data;
    std::string dir = currentDir();
    auto patterns = splitPatterns(wildCard);
    if (patterns.empty())
        patterns.push_back("*");
    auto *coll = new TFileCollection(5, 5);
    if (DIR *d = opendir(dir.c_str()))
    {
        while (struct dirent *e = readdir(d))
        {
            const char *name = e->d_name;
            if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
                continue;
            if (name[0] == '.' && !showHidden)
                continue;
            struct stat st;
            if (stat((dir + name).c_str(), &st) != 0)
                continue;
            bool isDir = S_ISDIR(st.st_mode);
            if (!isDir)
            {
                if (!S_ISREG(st.st_mode))
                    continue;
                bool match = false;
                for (auto &p : patterns)
                    if ((match = wildcardMatch(p.c_str(), name)))
                        break;
                if (!match)
                    continue;
            }
            auto *rec = new TSearchRec;
            memset(rec, 0, sizeof(*rec));
            rec->attr = isDir ? attrDirectory : (st.st_mode & S_IWUSR ? attrArchive : attrReadOnly);
            rec->time = dosTime(st.st_mtime);
            rec->size = (int32_t) st.st_size;
            strnzcpy(rec->name, name, sizeof(rec->name));
            coll->insert(rec);
        }
        closedir(d);
    }
    if (dir != "/")
    {
        auto *rec = new TSearchRec;
        memset(rec, 0, sizeof(*rec));
        strcpy(rec->name, "..");
        rec->attr = attrDirectory;
        struct stat st;
        rec->time = stat((dir + "..").c_str(), &st) == 0 ? dosTime(st.st_mtime) : 0x210000;
        coll->insert(rec);
    }
    fileList->newList(coll);
    static TSearchRec noFile;
    if (fileList->list()->getCount() > 0)
        message(this, evBroadcast, cmFileFocused, fileList->list()->at(0));
    else
        message(this, evBroadcast, cmFileFocused, &noFile);
    if (keep)
    {
        strnzcpy(fileName->data, typed, MAXPATH);
        fileName->selectAll(False);
        fileName->drawView();
    }
    showFilter();
    showFolder();
}

void SizedFileDialog::showFilter()
{
    if (filterInput && !(filterInput->state & sfFocused))
        filterInput->setText(wildCard);
}

void SizedFileDialog::showFolder()
{
    if (folderInput && !(folderInput->state & sfFocused))
    {
        std::string dir = currentDir();
        if (dir.size() > 1)
            dir.pop_back();
        folderInput->setText(tildePath(dir));
        folderInput->drawView();
    }
}

bool SizedFileDialog::applyFilter(const std::string &text)
{
    std::string f = trimSpaces(text);
    if (f.empty())
        f = "*";
    if (f.find('/') != std::string::npos || f.find('\\') != std::string::npos)
    {
        messageBox(mfError | mfOKButton, "The filter is a list of patterns without '/'\n(e.g. *.log *.txt).");
        filterInput->select();
        filterInput->selectAll(True);
        filterInput->drawView();
        return false;
    }
    // One space between the patterns.
    std::string joined;
    for (auto &p : splitPatterns(f))
        joined += (joined.empty() ? "" : " ") + p;
    strnzcpy(wildCard, joined, sizeof(wildCard));
    auto it = std::find(usedFilters.begin(), usedFilters.end(), joined);
    if (it != usedFilters.end())
        usedFilters.erase(it);
    usedFilters.insert(usedFilters.begin(), joined);
    if (usedFilters.size() > 20)
        usedFilters.resize(20);
    filterInput->setText(joined);
    readList(true);
    return true;
}

bool SizedFileDialog::applyFolder(const std::string &text)
{
    std::string path = expandHome(trimSpaces(text));
    if (path.empty())
        return false;
    if (path[0] != '/')
        path = currentDir() + path;
    struct stat st;
    if (stat(path.c_str(), &st) != 0 || !S_ISDIR(st.st_mode))
    {
        messageBox(mfError | mfOKButton, "Not a directory: %s", path.c_str());
        // The text stays, selected: typing replaces it, the arrows edit it.
        if (folderInput->state & sfFocused)
        {
            folderInput->selectAll(True);
            folderInput->drawView();
        }
        else
            showFolder();
        return false;
    }
    setDirectory(path);
    return true;
}

void SizedFileDialog::setDirectory(const std::string &dir)
{
    char buf[MAXPATH];
    strnzcpy(buf, dir, sizeof(buf));
    fexpand(buf);   // Removes "..", "." and repeated '/'.
    std::string path = dircompl::unixDir(buf);
    if (path.empty() || path.back() != '/')
        path += '/';
    delete[] (char *) directory;
    directory = newStr(path.c_str());
    if (folderInput && (folderInput->state & sfFocused))
    {
        // Shown as the new directory even while the field is focused.
        std::string shown = path.size() > 1 ? path.substr(0, path.size() - 1) : path;
        folderInput->setText(tildePath(shown));
    }
    readList();
}

std::vector<ListItem> SizedFileDialog::filterItems() const
// The dialog's first filter, the 'file-filters' patterns, then the
// filters used before.
{
    std::vector<ListItem> items;
    auto add = [&items](const std::string &f) {
        for (auto &i : items)
            if (i.value == f)
                return;
        items.push_back({f, f});
    };
    for (auto &f : TVTermApp::settings().fileFilters)
        add(f);
    for (auto &f : usedFilters)
        add(f);
    return items;
}

std::vector<ListItem> SizedFileDialog::folderItems() const
{
    std::vector<ListItem> items;
    std::string settingsDir = settingsPath();
    size_t slash = settingsDir.rfind('/');
    settingsDir = slash == std::string::npos ? "." : settingsDir.substr(0, slash);
    struct Named { const char *name; std::string path; };
    for (auto &n : { Named {"Home", homeDir()},
                     Named {"Profiles", profileDir()},
                     Named {"Settings", settingsDir} })
    {
        std::string shown = tildePath(n.path);
        std::string text = std::string(n.name) + std::string(10 - strlen(n.name), ' ') + shown;
        items.push_back({text, n.path});
    }
    for (auto &d : TVTermApp::settings().fileDirs)
        items.push_back({std::string(10, ' ') + d, d});
    return items;
}

Boolean SizedFileDialog::valid(ushort command)
{
    // tvision reads the directory itself when a directory or a pattern is
    // given in the Name field (OK, Enter, a double click): the list is read
    // again here.
    TCollection *before = fileList ? fileList->list() : nullptr;
    Boolean r = TVTermFileDialog::valid(command);
    if (fileList && fileList->list() != before)
        readList();
    return r;
}

void SizedFileDialog::fixNameField(TEvent &ev)
// A directory focused in the list: "dir/*.ini" in the Name field
// (tvision writes "dir\*.ini").
{
    if ( ev.what == evBroadcast && ev.message.command == cmFileFocused && fileName &&
         !(fileName->state & sfSelected) && ev.message.infoPtr &&
         (((TSearchRec *) ev.message.infoPtr)->attr & attrDirectory) )
    {
        for (char *p = fileName->data; *p; ++p)
            if (*p == '\\')
                *p = '/';
        fileName->drawView();
    }
    if ( ev.what == evBroadcast && ev.message.command == cmFileFocused && fileName &&
         !(fileName->state & sfSelected) )
        autoName = fileName->data;
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
    // Enter in Filter / Folder applies the field; Tab (Shift-Tab) applies
    // a changed value and moves on.
    if (ev.what == evKeyDown && (current == filterInput || current == folderInput))
    {
        ushort key = ev.keyDown.keyCode;
        bool enter = key == kbEnter;
        bool tab = key == kbTab || key == kbShiftTab;
        if (enter || tab)
        {
            std::string text = current->getState(sfFocused) ? ((TInputLine *) current)->data : "";
            bool ok = true;
            if (current == filterInput)
            {
                if (enter || trimSpaces(text) != wildCard)
                    ok = applyFilter(text);
            }
            else
            {
                std::string shown = currentDir();
                if (shown.size() > 1)
                    shown.pop_back();
                if (enter || trimSpaces(text) != tildePath(shown))
                    ok = applyFolder(text);
            }
            if (enter)
            {
                if (ok)
                    fileList->select();
                clearEvent(ev);
                return;
            }
            if (!ok)
            {
                clearEvent(ev);
                return;
            }
        }
    }
    TVTermFileDialog::handleEvent(ev);
    fixNameField(ev);
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
    hiddenBox = nullptr;
    filterLabel = nullptr;
    filterInput = nullptr;
    filterButton = nullptr;
    folderLabel = nullptr;
    folderInput = nullptr;
    folderButton = nullptr;
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

void setDialogDirectory(TFileDialog *d, const std::string &dir)
{
    if (auto *sd = dynamic_cast<fileopenimpl::SizedFileDialog *>(d))
        sd->setDirectory(dir);
}

} // namespace fileopen
