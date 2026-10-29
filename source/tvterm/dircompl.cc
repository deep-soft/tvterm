#define Uses_TEvent
#define Uses_TGroup
#define Uses_TInputLine
#define Uses_TListViewer
#define Uses_TScrollBar
#define Uses_TFileDialog
#define Uses_TFileInputLine
#define Uses_TKeys
#include <tvision/tv.h>

#include "dircompl.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <sys/stat.h>

namespace dircompl
{

static bool isSep(char c)
{
    // tvision accepts both in the file dialogs.
    return c == '/' || c == '\\';
}

static size_t lastSep(const std::string &text)
{
    for (size_t i = text.size(); i > 0; --i)
        if (isSep(text[i - 1]))
            return i - 1;
    return std::string::npos;
}

static std::string slashes(std::string s)
{
    std::replace(s.begin(), s.end(), '\\', '/');
    return s;
}

std::string unixDir(const char *directory)
{
    std::string dir = directory ? directory : "";
    // getCurDir() gives "C:\home\x\" also on Unix.
    if (dir.size() >= 2 && dir[1] == ':' && isalpha((unsigned char) dir[0]))
        dir.erase(0, 2);
    return slashes(dir);
}

std::vector<std::string> matches(const std::string &text, const std::string &baseDir,
                                 const std::string &home, size_t maxItems)
{
    std::vector<std::string> result;
    if (text.empty())
        return result;
    size_t sep = lastSep(text);
    std::string dirPart = sep == std::string::npos ? std::string() : text.substr(0, sep + 1);
    std::string prefix = sep == std::string::npos ? text : text.substr(sep + 1);
    if (prefix.find_first_of("*?") != std::string::npos)
        return result;

    std::string dir = slashes(dirPart);
    if (dir.size() >= 2 && dir[0] == '~' && dir[1] == '/')
    {
        if (home.empty())
            return result;
        dir = home + dir.substr(1);
    }
    else if (dir.empty() || dir[0] != '/')
    {
        std::string base = baseDir.empty() ? std::string(".") : baseDir;
        if (base.back() != '/')
            base += '/';
        dir = base + dir;
    }

    DIR *d = opendir(dir.c_str());
    if (!d)
        return result;
    bool hidden = !prefix.empty() && prefix[0] == '.';
    while (dirent *e = readdir(d))
    {
        const char *name = e->d_name;
        if (!strcmp(name, ".") || !strcmp(name, ".."))
            continue;
        if (name[0] == '.' && !hidden)
            continue;
        if (strncmp(name, prefix.c_str(), prefix.size()) != 0)
            continue;
        bool isDir = false;
#ifdef DT_DIR
        if (e->d_type == DT_DIR)
            isDir = true;
        else if (e->d_type == DT_LNK || e->d_type == DT_UNKNOWN)
#endif
        {
            // Links to directories count as directories.
            struct stat st;
            std::string path = dir;
            if (path.back() != '/')
                path += '/';
            path += name;
            isDir = stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
        }
        if (isDir)
            result.emplace_back(name);
    }
    closedir(d);
    std::sort(result.begin(), result.end());
    if (result.size() > maxItems)
        result.resize(maxItems);
    return result;
}

std::string complete(const std::string &text, const std::string &name)
{
    size_t sep = lastSep(text);
    std::string head = sep == std::string::npos ? std::string() : text.substr(0, sep + 1);
    return head + name + "/";
}

// The list below the field. Inserted into the dialog on top of the other
// views and hidden while there is nothing to show. It never takes the focus:
// the field keeps it (so its history only records what it did before), and
// the dialog passes the list keys on while 'active'.
class DirList : public TListViewer
{
public:
    std::vector<std::string> items;
    bool active {false};

    DirList(const TRect &bounds, TScrollBar *vScrollBar) noexcept :
        TListViewer(bounds, 1, nullptr, vScrollBar)
    {
        options &= ~ofSelectable;
    }

    void setItems(std::vector<std::string> list)
    {
        items = std::move(list);
        setRange((short) items.size());
        focusItem(0);
        drawView();
    }

    void getText(char *dest, short item, short maxLen) override
    {
        std::string s;
        if (0 <= item && (size_t) item < items.size())
            s = items[item] + "/";
        strnzcpy(dest, s.c_str(), maxLen);
    }

    void draw() override
    {
        // Drawn as a focused list while active.
        ushort saved = state;
        if (active)
            state |= sfSelected;
        TListViewer::draw();
        state = saved;
    }
};

static std::string homeDir()
{
    const char *home = getenv("HOME");
    std::string h = home ? home : "";
    while (h.size() > 1 && h.back() == '/')
        h.pop_back();
    return h;
}

void Completion::attach(TGroup *aOwner, TInputLine *aInput, std::function<std::string()> aBaseDir)
{
    owner = aOwner;
    input = aInput;
    baseDir = std::move(aBaseDir);
    TRect r = input->getBounds();
    r.a.y = r.b.y;
    r.b.y = r.a.y + 1;
    TRect sr(r.b.x, r.a.y, r.b.x + 1, r.b.y);
    scrollBar = new TScrollBar(sr);
    scrollBar->hide();
    list = new DirList(r, scrollBar);
    list->hide();
    // On top of the list of files and the other views.
    owner->insert(scrollBar);
    owner->insert(list);
}

bool Completion::isOpen() const
{
    return list && (list->state & sfVisible);
}

void Completion::setActive(bool on)
{
    if (list->active == on)
        return;
    list->active = on;
    list->drawView();
}

void Completion::close()
{
    if (!isOpen())
        return;
    list->active = false;
    list->hide();
    scrollBar->hide();
}

void Completion::refresh()
{
    std::vector<std::string> found = matches(input->data, baseDir ? baseDir() : std::string(), homeDir());
    if (found.empty())
    {
        close();
        return;
    }
    // Below the field, as wide as it, within the dialog's frame.
    TRect r = input->getBounds();
    int maxRows = std::min(10, owner->size.y - 1 - r.b.y);
    if (maxRows < 1)
    {
        close();
        return;
    }
    int rows = std::min<int>(maxRows, (int) found.size());
    bool scroll = (int) found.size() > rows;
    r.a.y = r.b.y;
    r.b.y = r.a.y + rows;
    if (scroll)
        r.b.x -= 1;
    // Hidden while it moves, so that what it covered is drawn again.
    close();
    list->changeBounds(r);
    scrollBar->changeBounds(TRect(r.b.x, r.a.y, r.b.x + 1, r.b.y));
    // The scroll bar only for a list longer than the rows shown.
    list->vScrollBar = scroll ? scrollBar : nullptr;
    list->setItems(std::move(found));
    list->show();
    if (scroll)
        scrollBar->show();
}

void Completion::accept()
{
    if (list->focused < 0 || (size_t) list->focused >= list->items.size())
        return;
    std::string text = complete(input->data, list->items[list->focused]);
    if (text.size() > (size_t) input->maxLen)
        return;
    strnzcpy(input->data, text.c_str(), input->maxLen + 1);
    // Cursor at the end, nothing selected, so that typing goes on.
    input->selStart = input->selEnd = input->curPos = (int) strlen(input->data);
    input->firstPos = std::max(0, input->curPos - input->size.x + 2);
    input->drawView();
    refresh();
}

bool Completion::before(TEvent &ev)
{
    if (!list)
        return false;
    if (ev.what == evBroadcast)
    {
        if (ev.message.command == cmInputLinePasted && ev.message.infoPtr == input)
        {
            if (owner->current == input)
                refresh();
            owner->clearEvent(ev);
            return true;
        }
        if (ev.message.command == cmListItemSelected && ev.message.infoPtr == list)
        {
            // Double click on a row.
            accept();
            owner->clearEvent(ev);
            return true;
        }
        return false;
    }
    if (!isOpen() || ev.what != evKeyDown || owner->current != input)
        return false;
    ushort key = ev.keyDown.keyCode;
    if (!list->active)
    {
        switch (key)
        {
            case kbDown:
                list->focusItem(0);
                setActive(true);
                owner->clearEvent(ev);
                return true;
            case kbEsc:
                close();
                owner->clearEvent(ev);
                return true;
            case kbTab:
            case kbShiftTab:
            case kbEnter:
                close();
                return false;
        }
        return false;
    }
    switch (key)
    {
        case kbEnter:
            accept();
            owner->clearEvent(ev);
            return true;
        case kbEsc:
            close();
            owner->clearEvent(ev);
            return true;
        case kbUp:
            if (list->focused <= 0)
                setActive(false);
            else
            {
                list->focusItem(list->focused - 1);
                list->drawView();
            }
            owner->clearEvent(ev);
            return true;
        case kbHome:
        case kbCtrlPgUp:
            list->focusItem(0);
            list->drawView();
            owner->clearEvent(ev);
            return true;
        case kbEnd:
        case kbCtrlPgDn:
            // TListViewer's End stops at the last visible row.
            list->focusItem(list->range - 1);
            list->drawView();
            owner->clearEvent(ev);
            return true;
        case kbDown:
        case kbPgUp:
        case kbPgDn:
            list->handleEvent(ev);
            owner->clearEvent(ev);
            return true;
        case kbTab:
        case kbShiftTab:
            // Moves on from the field, as if the list was not there.
            close();
            return false;
    }
    // Any other key goes back to the field and is handled there.
    setActive(false);
    return false;
}

void Completion::after(const std::string &oldText, bool wasFocused)
{
    if (!list)
        return;
    if (wasFocused && owner->current == input && oldText != input->data)
        refresh();
    else if (isOpen() && owner->current != input)
        // The focus went elsewhere (a click, a label's hot key).
        close();
}

void Completion::handleEvent(TEvent &ev, const std::function<void(TEvent &)> &handle)
{
    if (before(ev))
        return;
    bool wasFocused = list && owner->current == input;
    std::string oldText = input ? input->data : "";
    handle(ev);
    after(oldText, wasFocused);
}

} // namespace dircompl

TVTermFileDialog::TVTermFileDialog(TStringView wildCard, TStringView title, TStringView inputName,
                                   ushort options, uchar histId) noexcept :
    TWindowInit(&TFileDialog::initFrame),
    TFileDialog(wildCard, title, inputName, options, histId)
{
    completion.attach(this, fileName, [this] { return dircompl::unixDir(directory); });
}

void TVTermFileDialog::handleEvent(TEvent &ev)
{
    completion.handleEvent(ev, [this](TEvent &e) { TFileDialog::handleEvent(e); });
}
