#define Uses_TMenuBar
#define Uses_TSubMenu
#define Uses_TMenu
#define Uses_TMenuItem
#define Uses_TStatusLine
#define Uses_TStatusItem
#define Uses_TStatusDef
#define Uses_TKeys
#define Uses_TEvent
#define Uses_TChDirDialog
#define Uses_TDeskTop
#define Uses_MsgBox
#define Uses_TFileDialog
#define Uses_TFileList
#define Uses_TFileEditor
#include <tvision/tv.h>

#include "app.h"
#include "cmds.h"
#include "desk.h"
#include "wnd.h"
#include "apputil.h"
#include "editwnd.h"
#include "evntview.h"
#include "profile.h"
#include "winlist.h"
#include "procinfo.h"
#include "quitdlg.h"
#include "settings.h"
#include "statline.h"
#include "histdlg.h"
#include "about.h"
#include "alerts.h"
#include "logview.h"
#include "logsrc.h"
#include <tvterm/termopts.h>
#include <tvterm/termview.h>
#include "dlgclip.h"
#include <tvterm/termctrl.h>
#include <tvterm/vtermemu.h>

#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <math.h>
#include <sys/stat.h>
#include <vector>
#include <deque>
#include <chrono>

namespace appsettings
{

static Settings currentSettings;
static FileStamp stamp;
static std::chrono::steady_clock::time_point nextCheck;
enum { checkIntervalMs = 2000 };

// Messages for the Event Viewer, kept until one is opened.
static std::deque<std::string> notes;
enum { maxNotes = 200 };

// When tvterm started (for the uptime in the About dialog).
static const auto startTime = std::chrono::steady_clock::now();

static void showStatus(const std::string &text)
{
    TVTermStatusLine::showMessage(text);
}

} // namespace appsettings

static TRect editorBounds(const TRect &extent);

const Settings &TVTermApp::settings()
{
    return appsettings::currentSettings;
}

void TVTermApp::note(const std::string &text)
{
    // Timestamp, so that notes can be told apart.
    char stamp[16] = "";
    time_t now = time(nullptr);
    if (struct tm *tm = localtime(&now))
        strftime(stamp, sizeof(stamp), "%H:%M:%S ", tm);
    std::string line = stamp + text;
    appsettings::notes.push_back(line);
    while (appsettings::notes.size() > appsettings::maxNotes)
        appsettings::notes.pop_front();
    if (TProgram::deskTop)
        if (auto *viewer = (TEventViewer *) message(TProgram::deskTop, evBroadcast, cmFindEventViewer, nullptr))
            viewer->printNote(line);
}

static void showNotes(TEventViewer &viewer, bool always)
// The notes received so far (the last 'maxNotes').
{
    for (auto &line : appsettings::notes)
        viewer.printNote(line, always);
}

static TEventViewer *newEventViewer(const TRect &bounds, const EventFilter &filter = EventFilter())
{
    // About 64 KB of text: several hundred events.
    auto *viewer = new TEventViewer(bounds, 64000, filter);
    showNotes(*viewer, false);
    return viewer;
}

TCommandSet TVTermApp::tileCmds = []()
{
    TCommandSet ts;
    ts += cmTile;
    ts += cmTileCols;
    ts += cmTileRows;
    ts += cmCascade;
    return ts;
}();

static void printUsage(FILE *f, const char *argv0)
{
    fprintf(f,
        "Usage: %s [-p PROFILE]\n"
        "\n"
        "  -p, --profile PROFILE   open the windows described in PROFILE\n"
        "                          ('none': do not open the default profile)\n"
        "  -V, --version           show the version and build information\n"
        "  -h, --help              show this help\n"
        "\n"
        "PROFILE is a file path (if it contains '/') or a name, looked up as\n"
        "  %s/PROFILE.ini\n"
        "See examples/profiles/example.ini in the tvterm sources for the format.\n"
        "\n"
        "Settings (default profile, history, keys): %s\n"
        "See examples/tvterm.ini in the tvterm sources.\n",
        argv0, profileDir().c_str(), settingsPath().c_str());
}

int main(int argc, char **argv)
{
    const char *profileName = nullptr;
    bool useDefaultProfile = true;
    for (int i = 1; i < argc; ++i)
    {
        const char *arg = argv[i];
        if (!strcmp(arg, "-h") || !strcmp(arg, "--help"))
        {
            printUsage(stdout, argv[0]);
            return 0;
        }
        else if (!strcmp(arg, "-V") || !strcmp(arg, "--version"))
        {
            fputs(versionText().c_str(), stdout);
            return 0;
        }
        else if ((!strcmp(arg, "-p") || !strcmp(arg, "--profile")) && i + 1 < argc)
            profileName = argv[++i];
        else if (!strncmp(arg, "--profile=", 10))
            profileName = arg + 10;
        else
        {
            fprintf(stderr, "%s: invalid argument '%s'\n\n", argv[0], arg);
            printUsage(stderr, argv[0]);
            return 1;
        }
    }
    if (profileName && !strcmp(profileName, "none"))
    {
        profileName = nullptr;
        useDefaultProfile = false;
    }

    TVTermApp app(profileName, useDefaultProfile);
    app.run();
    app.shutDown();
}

TVTermApp::TVTermApp(const char *profileName, bool useDefaultProfile) :
    TProgInit( &TVTermApp::initStatusLine,
               nullptr,
               &TVTermApp::initDeskTop
             )
{
    // First lines in the Event Viewer: which build is running.
    note(versionLine());
    note(buildLine());
    disableCommands(tileCmds);
    for (ushort cmd : TerminalWindow::appConsts.focusedCmds())
        disableCommand(cmd);
    disableCommand(cmPaste);
    TVTermEditWindow::initEditorDialogs();

    tvterm::terminalOptions.showMessage = appsettings::showStatus;
    tvterm::terminalOptions.openSearch = openHistorySearch;
    tvterm::terminalOptions.openSave = openHistorySave;
    // Before any terminal is created (scrollback size).
    reloadSettings(false, true);

    std::string profile = profileName ? profileName :
                          useDefaultProfile ? appsettings::currentSettings.profile : "";
    bool opened = !profile.empty() && openProfile(profile);
    if (!profileName && !profile.empty() && !pendingMessage.empty())
    {
        // Problems with the default profile: reported, but not blocking.
        note("default profile '" + profile + "' (from " + settingsPath() + "): " + pendingMessage);
        appsettings::showStatus( "Problems with the default profile '" + profile +
                                 "': see the Event Viewer (Ctrl-B V)." );
        pendingMessage.clear();
    }
    if (!opened)
        newTerm();
}

void TVTermApp::reloadSettings(bool manual, bool startup)
{
    using namespace appsettings;
    std::string path = settingsPath();
    stamp = FileStamp::of(path);
    nextCheck = std::chrono::steady_clock::now() + std::chrono::milliseconds(checkIntervalMs);
    std::string warnings;
    bool existed = loadSettings(path, currentSettings, warnings);
    applySettings(currentSettings);

    int problems = 0;
    for (size_t pos = 0; (pos = warnings.find('\n', pos)) != std::string::npos; ++pos)
        ++problems;
    if (!warnings.empty())
    {
        size_t start = 0, end;
        while ((end = warnings.find('\n', start)) != std::string::npos)
        {
            note("settings: " + warnings.substr(start, end - start));
            start = end + 1;
        }
    }
    if (problems)
        showStatus( "Settings: " + std::to_string(problems) +
                    " problem(s), see the Event Viewer (Ctrl-B V)." );
    else if (!existed && manual)
        showStatus("No settings file " + path + " (More... > Edit settings creates it).");
    else if (existed && !startup)
    {
        // Not at startup, where there is nothing to report.
        note("settings reloaded from " + path);
        showStatus("Settings reloaded.");
    }
    else if (!existed && !startup)
        note("settings file " + path + " removed: using the default settings");
}

void TVTermApp::checkSettingsFile()
{
    using namespace appsettings;
    auto now = std::chrono::steady_clock::now();
    if (now < nextCheck)
        return;
    nextCheck = now + std::chrono::milliseconds(checkIntervalMs);
    if (!currentSettings.autoReload)
        return;
    if (FileStamp::of(settingsPath()) != stamp)
        reloadSettings(false);
}

void TVTermApp::editSettings()
{
    std::string path = settingsPath();
    if (access(path.c_str(), F_OK) != 0)
    {
        // A new file, with every setting commented out.
        size_t slash = path.rfind('/');
        makeDirs(path.substr(0, slash));
        if (FILE *f = fopen(path.c_str(), "w"))
        {
            std::string text = settingsTemplate();
            fwrite(text.data(), 1, text.size(), f);
            fclose(f);
            // Not a change to reload.
            appsettings::stamp = FileStamp::of(path);
        }
        else
        {
            messageBox(mfError | mfOKButton, "Cannot create %s:\n%s", path.c_str(), strerror(errno));
            return;
        }
    }
    insertWindow(new TVTermEditWindow(editorBounds(deskTop->getExtent()), path.c_str()));
}

TStatusLine *TVTermApp::initStatusLine(TRect r)
{
    // Items of windows that handle their own keys: a click sends the key
    // (see TVTermStatusLine::keyCommand). "Window List" (no key, like
    // Ctrl-B J) follows "Ctrl-B Open Menu" in every bar except those of
    // moving/resizing a window and of the open menu.
    auto keyCmd = [] (ushort keyCode, ushort mods = 0, char ch = 0) {
        return TVTermStatusLine::keyCommand(keyCode, mods, ch);
    };
    r.b.y = r.a.y + 1;
    TStatusLine *statusLine = new AlertStatusLine(r,
        *new TStatusDef(hcDragging, hcDragging) +
            *new TStatusItem("~↑↓→←~ Move", kbNoKey, 0) +
            *new TStatusItem("~Shift-↑↓→←~ Resize", kbNoKey, 0) +
            *new TStatusItem("~Ctrl~ Move/Resize Faster", kbNoKey, 0) +
            *new TStatusItem("~Enter~ Done", kbNoKey, 0) +
            *new TStatusItem("~Esc~ Cancel", kbNoKey, 0) +
        *new TStatusDef(hcInputGrabbed, hcInputGrabbed) +
            *new TStatusItem("~Alt-End~ Release Input", kbAltEnd, cmReleaseInput) +
            *new TStatusItem("Window List", kbNoKey, cmWindowList) +
        *new TStatusDef(hcSelecting, hcSelecting) +
            *new TStatusItem("~Click & Drag~ Select Text", kbNoKey, 0) +
            *new TStatusItem("~Ctrl-C~ Copy Selection", kbCtrlC, cmCopySelection) +
            *new TStatusItem("~Esc~ Cancel", kbEsc, cmCancelSelection) +
            *new TStatusItem(0 , kbCtrlB, cmMenu) +
            *new TStatusItem("Window List", kbNoKey, cmWindowList) +
        // History mode: the keys are handled by the terminal view.
        *new TStatusDef(hcHistory, hcHistory) +
            *new TStatusItem("~↑↓ PgUp PgDn Home End~ Scroll", kbNoKey, 0) +
            *new TStatusItem("~/~ Search", kbNoKey, keyCmd('/', 0, '/')) +
            *new TStatusItem("~F3~ Next", kbNoKey, keyCmd(kbF3)) +
            *new TStatusItem("~Shift-F3~ Prev", kbNoKey, keyCmd(kbShiftF3, kbShift)) +
            *new TStatusItem("~F2~ Save", kbNoKey, keyCmd(kbF2)) +
            *new TStatusItem("~Ctrl-C~ Copy", kbNoKey, keyCmd(kbCtrlC, kbCtrlShift)) +
            *new TStatusItem("~Esc~ Exit", kbNoKey, keyCmd(kbEsc)) +
            *new TStatusItem(0 , kbCtrlB, cmMenu) +
            *new TStatusItem("Window List", kbNoKey, cmWindowList) +
        // Log View: its keys are handled by the window.
        *new TStatusDef(hcLogView, hcLogView) +
            *new TStatusItem("~F9~ Menu", kbNoKey, keyCmd(kbF9)) +
            *new TStatusItem("~F4~ Settings", kbNoKey, keyCmd(kbF4)) +
            *new TStatusItem("~I~ Include", kbNoKey, keyCmd('i', 0, 'i')) +
            *new TStatusItem("~E~ Exclude", kbNoKey, keyCmd('e', 0, 'e')) +
            *new TStatusItem("~F~ Filters", kbNoKey, keyCmd('f', 0, 'f')) +
            *new TStatusItem("~Space~ Pause", kbNoKey, keyCmd(' ', 0, ' ')) +
            *new TStatusItem("~/~ Search", kbNoKey, keyCmd('/', 0, '/')) +
            *new TStatusItem("~Ctrl-C~ Copy", kbNoKey, keyCmd(kbCtrlC, kbCtrlShift)) +
            *new TStatusItem("~F2~ Save", kbNoKey, keyCmd(kbF2)) +
            *new TStatusItem("~Ctrl-B~ Open Menu", kbCtrlB, cmMenu) +
            *new TStatusItem("Window List", kbNoKey, cmWindowList) +
        // Event Viewer: its keys are handled by the window.
        *new TStatusDef(hcEventViewer, hcEventViewer) +
            *new TStatusItem("~M~ Moves", kbNoKey, keyCmd('m', 0, 'm')) +
            *new TStatusItem("~C~ Clicks", kbNoKey, keyCmd('c', 0, 'c')) +
            *new TStatusItem("~K~ Keys", kbNoKey, keyCmd('k', 0, 'k')) +
            *new TStatusItem("~O~ Other", kbNoKey, keyCmd('o', 0, 'o')) +
            *new TStatusItem("~T~ Messages", kbNoKey, keyCmd('t', 0, 't')) +
            *new TStatusItem("~N~ Notes", kbNoKey, keyCmd('n', 0, 'n')) +
            *new TStatusItem("~X~ Clear", kbNoKey, keyCmd('x', 0, 'x')) +
            *new TStatusItem("~Ctrl-B~ Open Menu", kbCtrlB, cmMenu) +
            *new TStatusItem("Window List", kbNoKey, cmWindowList) +
        *new TStatusDef(hcEditor, hcEditor) +
            *new TStatusItem("~F2~ Save", kbF2, cmSave) +
            *new TStatusItem("~Ctrl-Ins~ Copy", kbNoKey, 0) +
            *new TStatusItem("~Shift-Ins~ Paste", kbNoKey, 0) +
            *new TStatusItem("~Shift-Del~ Cut", kbNoKey, 0) +
            *new TStatusItem("~Ctrl-B~ Open Menu", kbCtrlB, cmMenu) +
            *new TStatusItem("Window List", kbNoKey, cmWindowList) +
        *new TStatusDef(hcMenu, hcMenu) +
            *new TStatusItem("~Esc~ Close Menu", kbNoKey, 0) +
        *new TStatusDef(0, 0xFFFF) +
            *new TStatusItem("~Ctrl-B~ Open Menu" , kbCtrlB, cmMenu) +
            *new TStatusItem("Window List", kbNoKey, cmWindowList) +
            // Shift-Ins itself is handled by the terminal view (see
            // TerminalView::pasteOnShiftIns); this item is a hint and can be
            // clicked.
            *new TStatusItem("~Shift-Ins~ Paste", kbNoKey, cmPaste)
            );
    statusLine->growMode = gfGrowHiX;
    return statusLine;
}

TDeskTop *TVTermApp::initDeskTop(TRect r)
{
    r.a.y += 1;
    return new TVTermDesk(r);
}

void TVTermApp::handleEvent(TEvent &event)
{
    // Instead of TApplication's: the Event Viewer and editors stay in place.
    if (event.what == evCommand && event.message.command == cmCascade)
    {
        getDeskTop()->cascadeWindows(getTileRect());
        clearEvent(event);
    }
    else if (event.what == evCommand && event.message.command == cmTile)
    {
        getDeskTop()->tileHorizontal(getTileRect());
        clearEvent(event);
    }
    TApplication::handleEvent(event);
    bool handled = true;
    switch (event.what)
    {
        case evCommand:
            switch (event.message.command)
            {
                case cmMenu: openMenu(); break;
                case cmNewTerm: newTerm(); break;
                case cmChangeDir: changeDir(); break;
                case cmNewEditor: newEditor(); break;
                case cmOpenEditor: openEditor(); break;
                case cmEventViewer: toggleEventViewer(); break;
                case cmNewLogView: newLogView(); break;
                case cmLoadProfile: loadProfile(); break;
                case cmSaveProfile: saveProfile(); break;
                case cmWindowList: windowList(); break;
                case cmRenameTerm: renameTerm(); break;
                case cmEditSettings: editSettings(); break;
                case cmReloadSettings: reloadSettings(true); break;
                case cmAbout: about(); break;
                case cmShowNotes:
                    if (event.message.infoPtr)
                        showNotes(*(TEventViewer *) event.message.infoPtr, true);
                    break;
                case cmAlertSettings: alertSettings(); break;
                case cmNextAlert: nextAlert(); break;
                case cmTileCols: getDeskTop()->tileVertical(getTileRect()); break;
                case cmTileRows: getDeskTop()->tileHorizontal(getTileRect()); break;
                default:
                    handled = false;
                    break;
            }
            break;
        default:
            handled = false;
            break;
    }
    if (handled)
        clearEvent(event);
}

size_t TVTermApp::getOpenTermCount()
{
    size_t count = 0;
    message(this, evBroadcast, cmGetOpenTerms, &count);
    return count;
}

Boolean TVTermApp::valid(ushort command)
{
    if (command == cmQuit)
    {
        // Lists what runs in each terminal; the user may also go to one
        // of them instead of quitting.
        if ( (getOpenTermCount() || countRunningLogViews(*deskTop)) &&
             !confirmQuit(*deskTop) )
            return False;
        // Let editor windows ask to save modified files.
    }
    return TApplication::valid(command);
}

void TVTermApp::idle()
{
    TApplication::idle();
    if (!pendingMessage.empty())
    {
        // Cleared first, because idle() also runs during the message box.
        std::string msg = std::move(pendingMessage);
        pendingMessage.clear();
        messageBox(msg, mfWarning | mfOKButton);
    }
    {
        // Enable or disable the cmTile and cmCascade commands.
        if (getDeskTop()->hasTileable())
            enableCommands(tileCmds);
        else
            disableCommands(tileCmds);
    }
    // Log Views take the new lines of their sources during the broadcast.
    LogSource::pollAll();
    message(this, evBroadcast, cmCheckTerminalUpdates, nullptr);
    TVTermStatusLine::checkExpired();
    checkSettingsFile();
    if (auto *sl = dynamic_cast<AlertStatusLine *>(statusLine))
    {
        size_t count = countAlerts(*deskTop);
        if (count != sl->alertCount)
        {
            sl->alertCount = count;
            sl->drawView();
        }
    }
}

void TVTermApp::openMenu()
{
    TMenuItem &menuItems =
        *new TMenuItem("New Term", cmNewTerm, 'N', hcNoContext, "~N~") +
        *new TMenuItem("Close Term", cmClose, 'W', hcNoContext, "~W~") +
        newLine() +
        *new TMenuItem("Next Term", cmNext, kbTab, hcNoContext, "~Tab~") +
        *new TMenuItem("Previous Term", cmPrev, kbShiftTab, hcNoContext, "~Shift-Tab~") +
        *new TMenuItem("Window List...", cmWindowList, 'J', hcNoContext, "~J~") +
        *new TMenuItem("Rename Term...", cmRenameTerm, 'T', hcNoContext, "~T~") +
        *new TMenuItem("Alerts...", cmAlertSettings, 'K', hcNoContext, "~K~") +
        *new TMenuItem("Go to Next Alert", cmNextAlert, 'G', hcNoContext, "~G~") +
        *new TMenuItem("Resize/Move", cmResize, 'R', hcNoContext, "~R~") +
        *new TMenuItem("Maximize/Restore", cmZoom, 'F', hcNoContext, "~F~") +
        newLine() +
        *new TMenuItem("Select Text", cmStartSelection, 'S', hcNoContext, "~S~") +
        *new TMenuItem("History (Scroll/Search/Save)", cmHistoryMode, 'H', hcNoContext, "~H~") +
        *new TMenuItem("Paste", cmPaste, 'P', hcNoContext, "~P~") +
        newLine() +
        *new TMenuItem("New Editor", cmNewEditor, 'E', hcNoContext, "~E~") +
        *new TMenuItem("Open File in Editor...", cmOpenEditor, 'O', hcNoContext, "~O~") +
        *new TMenuItem("Event Viewer", cmEventViewer, 'V', hcNoContext, "~V~") +
        *new TMenuItem("Log View (filtered output)...", cmNewLogView, 'D', hcNoContext, "~D~") +
        *new TMenuItem("Load Profile...", cmLoadProfile, 'L', hcNoContext, "~L~") +
        *new TMenuItem("Save Profile As...", cmSaveProfile, 'A', hcNoContext, "~A~") +
        newLine() +
        ( *new TSubMenu("~M~ore...", kbNoKey, hcMenu) +
            *new TMenuItem("Change working ~d~ir...", cmChangeDir, kbNoKey) +
            newLine() +
            *new TMenuItem("Tile (~C~olumns First)", cmTileCols, kbNoKey) +
            *new TMenuItem("Tile (~R~ows First)", cmTileRows, kbNoKey) +
            *new TMenuItem("C~a~scade", cmCascade, kbNoKey) +
            *new TMenuItem("~G~rab Input", cmGrabInput, kbNoKey) +
            newLine() +
            *new TMenuItem("~E~dit settings (tvterm.ini)", cmEditSettings, kbNoKey) +
            *new TMenuItem("Reload se~t~tings", cmReloadSettings, kbNoKey) +
            newLine() +
            *new TMenuItem("A~b~out tvterm...", cmAbout, kbNoKey)
        ) +
        *new TMenuItem("Suspend", cmDosShell, 'U', hcNoContext, "~U~") +
        *new TMenuItem("Exit", cmQuit, 'Q', hcNoContext, "~Q~");

    TMenu *menu = new TMenu(menuItems);
    TMenuPopup *menuPopup = new CenteredMenuPopup(menu);
    menuPopup->helpCtx = hcMenu;

    if (ushort cmd = execView(menuPopup))
    {
        TEvent event = {};
        event.what = evCommand;
        event.message.command = cmd;
        putEvent(event);
    }

    TObject::destroy(menuPopup);
}

static void onTermError(const char *reason)
{
    messageBox(mfError | mfOKButton, "Cannot create terminal: %s.", reason);
};

void TVTermApp::newTerm()
{
    using namespace tvterm;
    TRect r = deskTop->getExtent();
    VTermEmulatorFactory factory;
    auto *termCtrl = TerminalController::create( TerminalWindow::viewSize(r),
                                                 factory, onTermError );
    if (termCtrl)
        insertWindow(new TerminalWindow(r, *termCtrl));
}

void TVTermApp::changeDir()
{
    execDialog(new TChDirDialog(cdNormal, 0));
}

static TRect eventViewerBounds(const TRect &extent);

static TRect editorBounds(const TRect &extent)
{
    // Prefer an 80-column editor on the right side, leaving room for
    // the terminals; on narrow screens, just use the whole desktop.
    TRect r = extent;
    if (r.b.x - r.a.x > 100)
        r.a.x = r.b.x - 82;
    return r;
}

void TVTermApp::newEditor()
{
    insertWindow(new TVTermEditWindow(editorBounds(deskTop->getExtent()), ""));
}

void TVTermApp::openEditor()
{
    char fileName[MAXPATH] = "*";
    TFileDialog *d = new TFileDialog("*", "Open file", "~N~ame", fdOpenButton, 100);
    TView *p = validView(d);
    if (!p)
        return;
    ushort result = deskTop->execView(p);
    if (result != cmCancel)
        p->getData(fileName);
    TObject::destroy(p);
    if (result != cmCancel)
        insertWindow(new TVTermEditWindow(editorBounds(deskTop->getExtent()), fileName));
}

void TVTermApp::toggleEventViewer()
{
    // If an Event Viewer is already open, pause/resume it.
    auto *viewer = (TEventViewer *) message(deskTop, evBroadcast, cmFindEventViewer, nullptr);
    if (viewer)
        viewer->toggle();
    else
    {
        insertWindow(newEventViewer(eventViewerBounds(deskTop->getExtent())));
    }
}

void TVTermApp::newLogView()
{
    LogViewOptions options;
    if (execLogViewDialog(options, true))
        insertWindow(new LogViewWindow(deskTop->getExtent(), options));
}

void TVTermApp::getEvent(TEvent &event)
{
    TApplication::getEvent(event);
    // Show every event in the Event Viewer, if one is open.
    if (event.what != evNothing)
        if (auto *viewer = (TEventViewer *) message(deskTop, evBroadcast, cmFindEventViewer, nullptr))
            viewer->print(event);
    // Clipboard keys and pasted text in the text fields of dialogs.
    if (event.what == evKeyDown)
        dlgclip::handleKeyEvent(*this, event);
}

static std::string firstLines(const std::string &text, int maxLines)
{
    size_t pos = 0;
    for (int i = 0; i < maxLines; ++i)
    {
        pos = text.find('\n', pos);
        if (pos == std::string::npos)
            return text;
        ++pos;
    }
    return text.substr(0, pos) + "...\n";
}

bool TVTermApp::openProfile(const std::string &nameOrPath)
// Returns whether at least one terminal was opened.
{
    Profile profile;
    std::string warnings;
    if (!::loadProfile(nameOrPath, profile, warnings))
    {
        pendingMessage = "Cannot read profile '" + nameOrPath + "'.\n"
                         "Profiles are looked up in " + profileDir() + ".";
        return false;
    }
    if (!warnings.empty())
        pendingMessage = "Problems in profile " + profile.path + ":\n" + firstLines(warnings, 8);
    if (profile.windows.empty())
    {
        if (warnings.empty())
            pendingMessage = "Profile " + profile.path + " has no windows "
                             "([terminal], [editor], [eventviewer] or [logview] sections).";
        return false;
    }
    // Not errors, so not in the message box (e.g. positions moved inside
    // the desktop, see parsePos).
    for (size_t start = 0, end; (end = profile.notes.find('\n', start)) != std::string::npos; start = end + 1)
        note("profile " + profile.path + ": " + profile.notes.substr(start, end - start));
    openProfile(profile);
    loadedProfile = profile.path;
    return true;
}

static TRect profileBounds(const WindowProfile &win, const TRect &ext)
// Position in percent of the desktop -> window bounds. Computed from the
// edges, so that adjacent windows touch.
{
    int width = ext.b.x - ext.a.x, height = ext.b.y - ext.a.y;
    return TRect( ext.a.x + (int) lround(width * win.x / 100),
                  ext.a.y + (int) lround(height * win.y / 100),
                  ext.a.x + (int) lround(width * (win.x + win.w) / 100),
                  ext.a.y + (int) lround(height * (win.y + win.h) / 100) );
}

static TRect eventViewerBounds(const TRect &extent)
{
    TRect r = extent;
    if (r.b.x - r.a.x > 100)
        r.a.x = r.b.x - 70;
    return r;
}

static TRect editorBounds(const TRect &extent);

void TVTermApp::openProfile(const Profile &profile)
{
    using namespace tvterm;
    TRect ext = deskTop->getExtent();
    bool anyPos = false;
    std::vector<TWindow *> windows;
    VTermEmulatorFactory factory;

    for (const auto &win : profile.windows)
    {
        anyPos |= win.hasPos;
        TWindow *window = nullptr;
        switch (win.kind)
        {
            case WindowProfile::Terminal:
            {
                TRect r = win.hasPos ? profileBounds(win, ext) : ext;
                ClientOptions options;
                options.command = win.command.c_str();
                options.workingDir = win.dir.c_str();
                options.keepOpen = win.keepOpen;
                auto *termCtrl = TerminalController::create( TerminalWindow::viewSize(r),
                                                             factory, onTermError, options );
                if (!termCtrl)
                    break;
                auto *term = new TerminalWindow(r, *termCtrl);
                if (!win.title.empty())
                    term->setCustomTitle(win.title);
                term->launchCommand = win.command;
                term->launchDir = win.dir;
                term->launchKeepOpen = win.keepOpen;
                AlertSettings alerts;
                alerts.activity = win.alertActivity;
                alerts.keywords = parseKeywords(win.alertKeywords);
                alerts.silenceSeconds = win.alertSilence;
                term->setAlertSettings(std::move(alerts));
                window = term;
                break;
            }
            case WindowProfile::Editor:
            {
                TRect r = win.hasPos ? profileBounds(win, ext) : editorBounds(ext);
                window = new TVTermEditWindow(r, win.file);
                break;
            }
            case WindowProfile::EventViewer:
            {
                // Only one Event Viewer.
                if (message(deskTop, evBroadcast, cmFindEventViewer, nullptr))
                    break;
                TRect r = win.hasPos ? profileBounds(win, ext) : eventViewerBounds(ext);
                window = newEventViewer(r, win.filter);
                break;
            }
            case WindowProfile::LogView:
            {
                if (win.command.empty())
                {
                    note("profile " + profile.path + ": [logview] without a command, skipped");
                    break;
                }
                TRect r = win.hasPos ? profileBounds(win, ext) : ext;
                LogViewOptions o;
                o.title = win.title;
                o.command = win.command;
                o.include = win.logInclude;
                o.exclude = win.logExclude;
                o.ignoreCase = win.logIgnoreCase;
                o.wrap = win.logWrap;
                o.filters = win.logFilters;
                o.maxLines = win.logMaxLines;
                o.autoRestart = win.logAutoRestart;
                auto *log = new LogViewWindow(r, o);
                std::string error;
                if (!log->setOptions(o, error))
                {
                    note("profile " + profile.path + ": [logview] '" + win.command + "': " + error);
                    if (pendingMessage.empty())
                        pendingMessage = "Problems in profile " + profile.path + ":\n[logview] '" +
                                         win.command + "': " + error + "\n(the filter is not used)";
                }
                AlertSettings alerts;
                alerts.activity = win.alertActivity;
                alerts.keywords = parseKeywords(win.alertKeywords);
                alerts.silenceSeconds = win.alertSilence;
                log->setAlertSettings(std::move(alerts));
                window = log;
                break;
            }
        }
        if (window)
            if (TWindow *inserted = insertWindow(window))
                windows.push_back(inserted);
    }

    Profile::Layout layout = profile.layout;
    if (layout == Profile::Auto)
        layout = anyPos || windows.size() < 2 ? Profile::Manual : Profile::TileColumns;
    switch (layout)
    {
        case Profile::TileColumns: getDeskTop()->tileVertical(getTileRect()); break;
        case Profile::TileRows: getDeskTop()->tileHorizontal(getTileRect()); break;
        case Profile::Cascade: getDeskTop()->cascadeWindows(getTileRect()); break;
        default: break;
    }

    if (profile.focus > 0 && (size_t) profile.focus <= windows.size())
        windows[profile.focus - 1]->select();
}

namespace profiledlg
{

// A file dialog listing the *.ini files of 'dir' (the current directory if
// 'dir' does not exist). TFileDialog keeps its wildcard exactly as given and
// joins it to the listed directory: in the path shown at the bottom, and in
// the Name field when a directory such as ..\ is focused. So the wildcard
// must be a bare pattern; a full path there was doubled ("Invalid drive or
// directory" after ..\). The directory is set separately instead.
static TFileDialog *newDialog(const std::string &dir, const char *title,
                              ushort options, uchar histId)
{
    if (dir.empty() || access(dir.c_str(), F_OK) != 0)
        return new TFileDialog("*.ini", title, "~N~ame", options, histId);
    TFileDialog *d = new TFileDialog("*.ini", title, "~N~ame",
                                     options | fdNoLoadDir, histId);
    std::string path = dir;
    if (path.back() != '/')
        path += '/';
    // Names typed in the dialog are relative to 'directory'.
    delete[] (char *) d->directory;
    d->directory = newStr(path.c_str());
    d->fileList->readDirectory(d->directory, d->wildCard);
    return d;
}

} // namespace profiledlg

void TVTermApp::loadProfile()
{
    char fileName[MAXPATH];
    TFileDialog *d = profiledlg::newDialog(profileDir(), "Load profile", fdOpenButton, 100);
    TView *p = validView(d);
    if (!p)
        return;
    ushort result = deskTop->execView(p);
    if (result != cmCancel)
        p->getData(fileName);
    TObject::destroy(p);
    if (result == cmCancel || !closeWindowsBeforeLoad())
        return;
    if (!openProfile(std::string(fileName)) && pendingMessage.empty())
        pendingMessage = "No terminal was opened.";
}

bool TVTermApp::closeWindowsBeforeLoad()
{
    auto windows = getDeskTopWindows(*deskTop);
    if (windows.empty())
        return true;
    ushort answer = messageBox(
        "Close the current windows first?\n"
        "(No: the profile's windows are added to them.)",
        mfConfirmation | mfYesNoCancel);
    if (answer == cmNo)
        return true;
    if (answer != cmYes)
        return false;
    // Lists what runs in the terminals and Log Views, as when quitting.
    if ( (getOpenTermCount() || countRunningLogViews(*deskTop)) &&
         !confirmQuit(*deskTop, true) )
        return false;
    // Editors with unsaved changes ask first, before anything is closed:
    // Cancel there keeps all the windows and loads nothing.
    for (TWindow *w : windows)
        if (!w->valid(cmClose))
            return false;
    for (TWindow *w : windows)
        w->close();
    return true;
}

void TVTermApp::windowList()
{
    execWindowList(*deskTop);
}

void TVTermApp::alertSettings()
{
    auto *w = dynamic_cast<TWindow *>(deskTop->current);
    if (auto *target = dynamic_cast<AlertTarget *>(deskTop->current))
        execAlertSettings(*w, *target);
    else
        messageBox("The current window is not a terminal or a Log View.", mfInformation | mfOKButton);
}

void TVTermApp::nextAlert()
{
    if (!goToNextAlert(*deskTop))
        messageBox("There are no alerts.", mfInformation | mfOKButton);
}

void TVTermApp::renameTerm()
{
    auto *w = dynamic_cast<TWindow *>(deskTop->current);
    if (!w || !renameTerminal(w))
        messageBox("The current window is not a terminal or a Log View.", mfInformation | mfOKButton);
}

// Describing the current windows as a profile.

static std::string homeRelative(const std::string &path)
{
    const char *home = getenv("HOME");
    if (home && *home)
    {
        size_t len = strlen(home);
        if (path.compare(0, len, home) == 0 && (path.size() == len || path[len] == '/'))
            return "~" + path.substr(len);
    }
    return path;
}

static void setProfilePos(WindowProfile &win, TWindow &w, const TRect &ext);

static WindowProfile describeTerminal(TerminalWindow &w, const TRect &ext)
{
    WindowProfile term;
    TStringView title = w.getCustomTitle();
    term.title.assign(title.data(), title.size());

    // Command: the one from the profile, if any; otherwise a command
    // running in the foreground of an interactive shell (e.g. 'tail -f').
    term.command = w.launchCommand;
    term.keepOpen = w.launchKeepOpen;
    term.dir = w.launchDir;
    tvterm::ClientProcessInfo info;
    auto *view = w.getTerminalView();
    if (view && view->termCtrl.getClientProcessInfo(info))
    {
        long pid = info.foregroundPgid ? info.foregroundPgid : info.clientPid;
        if (term.command.empty() && info.foregroundPgid && info.foregroundPgid != info.clientPid)
        {
            term.command = procinfo::readCommandLine(info.foregroundPgid);
            term.keepOpen = true;
        }
        std::string cwd = procinfo::readLink("/proc/" + std::to_string(pid) + "/cwd");
        if (!cwd.empty())
            term.dir = homeRelative(cwd);
    }

    const AlertSettings &alerts = w.getAlertSettings();
    term.alertActivity = alerts.activity;
    term.alertKeywords = joinKeywords(alerts.keywords);
    term.alertSilence = alerts.silenceSeconds;

    setProfilePos(term, w, ext);
    return term;
}

static void profilePercent(int pos, int size, int extPos, int extSize, double &outPos, double &outSize)
// One axis of setProfilePos: the window is kept inside the desktop (moved,
// and shrunk if it is larger), and the values are rounded to one decimal,
// as they are written, so that position + size never exceeds 100.
{
    if (size > extSize)
        size = extSize;
    if (pos < extPos)
        pos = extPos;
    if (pos + size > extPos + extSize)
        pos = extPos + extSize - size;
    outPos = round(1000.0 * (pos - extPos) / extSize) / 10;
    outSize = round(1000.0 * size / extSize) / 10;
    if (outPos + outSize > 100)
        outSize = (round(1000 - 10 * outPos)) / 10;
}

static void setProfilePos(WindowProfile &win, TWindow &w, const TRect &ext)
{
    int width = ext.b.x - ext.a.x, height = ext.b.y - ext.a.y;
    if (width > 0 && height > 0)
    {
        TRect r = w.getBounds();
        win.hasPos = true;
        profilePercent(r.a.x, r.b.x - r.a.x, ext.a.x, width, win.x, win.w);
        profilePercent(r.a.y, r.b.y - r.a.y, ext.a.y, height, win.y, win.h);
    }
}

void TVTermApp::saveProfile()
{
    // Windows, from back to front, so that the stacking order is the
    // same when the profile is loaded again.
    std::vector<TWindow *> windows = getDeskTopWindows(*deskTop);
    Profile profile;
    TRect ext = deskTop->getExtent();
    int untitledWithText = 0, modifiedFiles = 0;
    for (auto it = windows.rbegin(); it != windows.rend(); ++it)
    {
        TWindow *w = *it;
        if (auto *term = dynamic_cast<TerminalWindow *>(w))
            profile.windows.push_back(describeTerminal(*term, ext));
        else if (auto *edit = dynamic_cast<TEditWindow *>(w))
        {
            WindowProfile win;
            win.kind = WindowProfile::Editor;
            if (edit->editor)
            {
                win.file = edit->editor->fileName;
                if (win.file.empty() && edit->editor->bufLen > 0)
                    ++untitledWithText;
                else if (!win.file.empty() && edit->editor->modified)
                    ++modifiedFiles;
            }
            setProfilePos(win, *w, ext);
            profile.windows.push_back(std::move(win));
        }
        else if (auto *log = dynamic_cast<LogViewWindow *>(w))
        {
            const LogViewOptions &o = log->getOptions();
            WindowProfile win;
            win.kind = WindowProfile::LogView;
            win.title = o.title;
            win.command = o.command;
            win.logInclude = o.include;
            win.logExclude = o.exclude;
            win.logIgnoreCase = o.ignoreCase;
            win.logWrap = o.wrap;
            win.logFilters = o.filters;
            win.logMaxLines = o.maxLines;
            win.logAutoRestart = o.autoRestart;
            const AlertSettings &alerts = log->getAlertSettings();
            win.alertActivity = alerts.activity;
            win.alertKeywords = joinKeywords(alerts.keywords);
            win.alertSilence = alerts.silenceSeconds;
            setProfilePos(win, *w, ext);
            profile.windows.push_back(std::move(win));
        }
        else if (auto *viewer = dynamic_cast<TEventViewer *>(w))
        {
            WindowProfile win;
            win.kind = WindowProfile::EventViewer;
            win.filter = viewer->getFilter();
            setProfilePos(win, *w, ext);
            profile.windows.push_back(std::move(win));
        }
        else
            continue;
        if (w == deskTop->current)
            profile.focus = (int) profile.windows.size();
    }
    if (profile.windows.empty())
    {
        messageBox("There are no windows to save.", mfInformation | mfOKButton);
        return;
    }

    std::string dir = profileDir();
    makeDirs(dir);
    char fileName[MAXPATH];
    TFileDialog *d = profiledlg::newDialog(dir, "Save profile as", fdOKButton, 101);
    TView *p = validView(d);
    if (!p)
        return;
    ushort result = deskTop->execView(p);
    if (result != cmCancel)
        p->getData(fileName);
    TObject::destroy(p);
    if (result == cmCancel)
        return;

    std::string path = fileName;
    if (path.empty() || path.back() == '/' || path.find('*') != std::string::npos)
        return;
    size_t slash = path.rfind('/');
    if (path.find('.', slash == std::string::npos ? 0 : slash) == std::string::npos)
        path += ".ini";
    if ( access(path.c_str(), F_OK) == 0 &&
         messageBox(mfConfirmation | mfYesButton | mfNoButton,
                    "%s already exists. Overwrite?", path.c_str()) != cmYes )
        return;

    std::string error;
    if (!::saveProfile(path, profile, error))
    {
        messageBox(mfError | mfOKButton, "Cannot save %s: %s", path.c_str(), error.c_str());
        return;
    }
    std::string notes;
    if (untitledWithText)
        notes += "\nThe text of untitled editors is not saved\n(save it to a file with F2 first).";
    if (modifiedFiles)
        notes += "\nSome editors have unsaved changes:\nthe profile only keeps their file names.";
    messageBox( mfInformation | mfOKButton, "Saved %zu window(s) to\n%s%s",
                profile.windows.size(), path.c_str(), notes.c_str() );
}

void TVTermApp::about()
{
    AboutInfo info;
    std::string path = settingsPath();
    info.settingsPath = homeRelative(path);
    info.settingsExists = access(path.c_str(), F_OK) == 0;
    info.profile = homeRelative(loadedProfile);
    info.uptimeSeconds = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now() - appsettings::startTime ).count();
    for (TWindow *w : getDeskTopWindows(*deskTop))
        if (dynamic_cast<TerminalWindow *>(w))
            ++info.terminals;
    showAbout(info);
}
