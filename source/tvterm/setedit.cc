#define Uses_TFileEditor
#define Uses_TEditWindow
#define Uses_TDialog
#define Uses_TStaticText
#define Uses_TButton
#define Uses_TProgram
#define Uses_TDeskTop
#define Uses_MsgBox
#include <tvision/tv.h>

#include "setedit.h"
#include "statline.h"
#include "apputil.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

SettingsEditWindow *SettingsEditWindow::instance = nullptr;

// Own namespace: the build compiles several .cc files together.
namespace setedit
{

enum { staleSeconds = 10 };

static std::string hostName()
{
    char buf[256] = {};
    if (gethostname(buf, sizeof(buf) - 1) != 0)
        return "";
    return buf;
}

struct Marker
{
    bool exists {false};
    int pid {0};
    std::string host;
    time_t mtime {0};

    static Marker read(const std::string &path)
    {
        Marker m;
        struct stat st;
        if (stat(path.c_str(), &st) != 0)
            return m;
        m.exists = true;
        m.mtime = st.st_mtime;
        std::ifstream f(path);
        std::string key, value;
        while (f >> key)
        {
            std::getline(f, value);
            value.erase(0, value.find_first_not_of(' '));
            if (key == "pid")
                m.pid = atoi(value.c_str());
            else if (key == "host")
                m.host = value;
        }
        return m;
    }

    bool isOwn() const
    {
        return pid == (int) getpid() && host == hostName();
    }

    // Older than 'staleSeconds', or of a process (on this host) that is gone.
    bool isStale() const
    {
        if (time(nullptr) - mtime > staleSeconds)
            return true;
        if (pid <= 0)
            return true;
        if (host == hostName() && kill(pid, 0) != 0 && errno == ESRCH)
            return true;
        return false;
    }
};

static bool writeMarker(const std::string &path)
{
    char when[32];
    time_t now = time(nullptr);
    strftime(when, sizeof(when), "%Y-%m-%d %H:%M:%S", localtime(&now));
    std::string tmp = path + ".tmp" + std::to_string(getpid());
    {
        std::ofstream f(tmp);
        if (!f)
            return false;
        f << "pid " << getpid() << "\n"
          << "host " << hostName() << "\n"
          << "time " << when << "\n";
        if (!f)
            return false;
    }
    if (rename(tmp.c_str(), path.c_str()) != 0)
    {
        unlink(tmp.c_str());
        return false;
    }
    return true;
}

// A question with two buttons; returns true for the first one. Esc chooses
// the second one.
static bool askTwo(const std::string &title, const std::string &text,
                   const char *first, const char *second)
{
    int lines = 1 + (int) std::count(text.begin(), text.end(), '\n');
    int width = 0;
    {
        std::istringstream in(text);
        std::string line;
        while (std::getline(in, line))
            width = std::max(width, (int) line.size());
    }
    width = std::min(std::max(width + 6, 40), 76);
    TDialog *d = new TDialog(TRect(0, 0, width, lines + 6), title.c_str());
    d->options |= ofCentered;
    d->insert(new TStaticText(TRect(3, 2, width - 3, 2 + lines), text.c_str()));
    int bw = 14;
    int x = (width - 2*bw - 2)/2;
    d->insert(new TButton(TRect(x, lines + 3, x + bw, lines + 5), first, cmYes, bfDefault));
    d->insert(new TButton(TRect(x + bw + 2, lines + 3, x + 2*bw + 2, lines + 5), second, cmNo, bfNormal));
    d->selectNext(False);
    return execDialog(d) == cmYes;
}

static std::string fileName(const std::string &path)
{
    size_t slash = path.rfind('/');
    return slash == std::string::npos ? path : path.substr(slash + 1);
}

} // namespace setedit

using namespace setedit;

SettingsEditWindow::SettingsEditWindow(const TRect &bounds, const std::string &aPath) noexcept :
    TWindowInit(&initFrame),
    TVTermEditWindow(bounds, aPath)
{
    // As the editor expanded it, to tell when Save As changes it.
    path = editor->fileName;
    diskStamp = FileStamp::of(path);
    seenStamp = diskStamp;
    savedText = text();
    instance = this;
}

std::string SettingsEditWindow::markerPath(const std::string &path)
{
    return path + ".editing";
}

bool SettingsEditWindow::editsSettings() const
{
    return instance == this && path == editor->fileName;
}

void SettingsEditWindow::detach()
{
    if (instance != this)
        return;
    removeOwnMarker();
    setOtherPid(0);
    instance = nullptr;
}

std::string SettingsEditWindow::text() const
{
    // The editor's buffer has a gap at the cursor.
    std::string s;
    s.reserve(editor->bufLen);
    s.append(editor->buffer, editor->curPtr);
    s.append(editor->buffer + editor->curPtr + editor->gapLen, editor->bufLen - editor->curPtr);
    return s;
}

bool SettingsEditWindow::hasChanges()
{
    if (!editor->modified)
        return false;
    if (text() != savedText)
        return true;
    // Undone back to the text of the file: no longer a change (no question
    // when the window is closed).
    editor->modified = False;
    editor->update(ufUpdate);
    editor->doUpdate();
    return false;
}

void SettingsEditWindow::handleEvent(TEvent &ev)
{
    // Read only: F2 is Save As (TVTermEditWindow::handleEvent).
    if ( ev.what == evCommand && ev.message.command == cmSave &&
         editsSettings() && (state & sfFocused) && !isReadOnly() )
    {
        saveChecked();
        clearEvent(ev);
        return;
    }
    TVTermEditWindow::handleEvent(ev);
    // After Save As to another file, a normal editor.
    if (instance == this && path != editor->fileName)
        detach();
}

Boolean SettingsEditWindow::valid(ushort command)
{
    if (command != cmValid && editsSettings() && hasChanges())
    {
        // Like the editor's question, but saving checks the file on disk.
        switch (messageBox(mfInformation | mfYesNoCancel, "%s has been modified. Save?", editor->fileName))
        {
            case cmYes:
                if (!saveChecked())
                    return False;
                break;
            case cmNo:
                editor->modified = False;
                break;
            default:
                return False;
        }
    }
    return TVTermEditWindow::valid(command);
}

void SettingsEditWindow::shutDown()
{
    detach();
    TVTermEditWindow::shutDown();
}

const char *SettingsEditWindow::getTitle(short maxSize)
{
    if (otherPid != 0)
    {
        title = "Settings \u2014 being edited in another tvterm (pid " + std::to_string(otherPid) + ")";
        return title.c_str();
    }
    return TVTermEditWindow::getTitle(maxSize);
}

bool SettingsEditWindow::saveChecked()
{
    FileStamp now = FileStamp::of(path);
    if (now.exists && now != diskStamp)
    {
        if (!askTwo( "Settings",
                     fileName(path) + " was changed on disk after it was opened here\n"
                     "(another tvterm or another program).\n"
                     "Overwrite it with the text of this editor?",
                     "~O~verwrite", "~C~ancel" ))
            return false;
    }
    if (!editor->save())
        return false;
    afterSave();
    return true;
}

void SettingsEditWindow::afterSave()
{
    savedText = text();
    diskStamp = FileStamp::of(path);
    seenStamp = diskStamp;
    removeOwnMarker();
}

void SettingsEditWindow::reloadFromDisk()
{
    uint oldPtr = editor->curPtr;
    if (editor->loadFile())
    {
        // Loading sets the horizontal range back to 256 columns.
        updateWidthLimit();
        editor->setCurPtr(std::min(oldPtr, editor->bufLen), 0);
        editor->trackCursor(True);
    }
    editor->modified = False;
    editor->update(ufView);
    editor->doUpdate();
    savedText = text();
    diskStamp = FileStamp::of(path);
    seenStamp = diskStamp;
    removeOwnMarker();
}

void SettingsEditWindow::removeOwnMarker()
{
    if (!ownMarker)
        return;
    ownMarker = false;
    std::string mpath = markerPath(path);
    Marker m = Marker::read(mpath);
    if (m.exists && m.isOwn())
        unlink(mpath.c_str());
}

void SettingsEditWindow::updateMarker(bool changes)
{
    std::string mpath = markerPath(path);
    Marker m = Marker::read(mpath);
    bool other = m.exists && !m.isOwn() && !m.isStale();
    if (changes && !other)
        ownMarker = writeMarker(mpath);
    else if (!changes)
        removeOwnMarker();
    else
        ownMarker = false;
    setOtherPid(other ? m.pid : 0);
}

void SettingsEditWindow::setOtherPid(int pid)
{
    if (pid == otherPid)
        return;
    otherPid = pid;
    // Another background while another tvterm edits the file.
    palette = pid != 0 ? wpCyanWindow : wpBlueWindow;
    // Every part of the window: drawView() alone would show the window's
    // buffer, where only the frame has the new colors.
    redraw();
}

void SettingsEditWindow::tick()
{
    if (busy)
        return;
    if (!editsSettings())
    {
        detach();
        return;
    }
    // Changes on disk (reported once per change).
    FileStamp now = FileStamp::of(path);
    if (now != diskStamp && now != seenStamp)
    {
        if (!now.exists)
        {
            seenStamp = now;
            TVTermStatusLine::showMessage("Settings editor: " + fileName(path) + " was removed; Save writes it again.");
        }
        else if (!hasChanges())
        {
            seenStamp = now;
            reloadFromDisk();
            TVTermStatusLine::showMessage("Settings editor: " + fileName(path) + " changed on disk, reloaded.");
        }
        else if (TProgram::application->TopView() == TProgram::application)
        {
            // Asked only when no dialog or menu is open.
            seenStamp = now;
            busy = true;
            bool reload = askTwo( "Settings",
                                  fileName(path) + " was changed on disk (another tvterm or another\n"
                                  "program) while it has unsaved changes here.\n"
                                  "Reload: take the file from disk, these changes are lost.\n"
                                  "Keep: keep these changes (Save asks before overwriting).",
                                  "~R~eload", "~K~eep" );
            busy = false;
            if (reload)
            {
                reloadFromDisk();
                TVTermStatusLine::showMessage("Settings editor: " + fileName(path) + " reloaded from disk.");
            }
        }
    }
    updateMarker(hasChanges());
}
