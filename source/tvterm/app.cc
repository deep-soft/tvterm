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
#include "setedit.h"
#include "evntview.h"
#include "profile.h"
#include "winlist.h"
#include "procinfo.h"
#include "quitdlg.h"
#include "settings.h"
#include "statline.h"
#include "topbar.h"
#include "histdlg.h"
#include "about.h"
#include "alerts.h"
#include "logview.h"
#include "logsrc.h"
#include <tvterm/termopts.h>
#include <tvterm/termview.h>
#include "dlgclip.h"
#include "dircompl.h"
#include "globalkeys.h"
#include "fileopen.h"
#include "desktops.h"
#include "highlight.h"
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
#include <algorithm>

namespace appsettings
{

static Settings currentSettings;
static FileStamp stamp;
static std::chrono::steady_clock::time_point nextCheck;
enum { checkIntervalMs = 2000 };

// Messages for the Event Viewer, kept until one is opened, with their time
// (shown as hh:mm:ss today, with the date on other days).
static std::deque<std::pair<time_t, std::string>> notes;
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
    time_t now = time(nullptr);
    appsettings::notes.emplace_back(now, text);
    while (appsettings::notes.size() > appsettings::maxNotes)
        appsettings::notes.pop_front();
    if (TProgram::deskTop)
        if (auto *viewer = (TEventViewer *) message(TProgram::deskTop, evBroadcast, cmFindEventViewer, nullptr))
            viewer->printNote(formatEventTime(now) + " " + text);
}

static void showNotes(TEventViewer &viewer, bool always)
// The notes received so far (the last 'maxNotes').
{
    for (auto &note : appsettings::notes)
        viewer.printNote(formatEventTime(note.first) + " " + note.second, always);
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
    ts += cmTileNCols;
    ts += cmTileNRows;
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
        "      --print-settings    print a settings file with every key commented\n"
        "                          out (default values) and exit; writes no file\n"
        "      --print-highlight [NAME...]\n"
        "                          print the built-in highlight sets (all, or the\n"
        "                          named ones) in tvterm.ini format and exit\n"
        "  -h, --help              show this help\n"
        "\n"
        "PROFILE is a file path (if it contains '/') or a name, looked up as\n"
        "  %s/PROFILE.ini\n"
        "See examples/profiles/example.ini in the tvterm sources for the format.\n"
        "\n"
        "Settings (default profile, history, keys): %s\n"
        "See examples/tvterm.ini in the tvterm sources, or --print-settings:\n"
        "  first time:  %s --print-settings > %s\n"
        "  beside it:   %s --print-settings > %s.new\n"
        "  new keys:    diff <(%s --print-settings) %s\n",
        argv0, profileDir().c_str(), settingsPath().c_str(),
        argv0, settingsPath().c_str(), argv0, settingsPath().c_str(),
        argv0, settingsPath().c_str());
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
        else if (!strcmp(arg, "--print-settings"))
        {
            // Like the file made by More... > Edit settings; nothing is
            // written (tvterm.ini is only created by Edit settings).
            fputs(settingsTemplate().c_str(), stdout);
            return 0;
        }
        else if (!strcmp(arg, "--print-highlight"))
        {
            // The following arguments are the names of the sets.
            std::vector<std::string> names(argv + i + 1, argv + argc);
            std::string out, error;
            if (!printBuiltinHighlight(names, out, error))
            {
                fprintf(stderr, "%s: %s\n", argv[0], error.c_str());
                return 1;
            }
            fputs(out.c_str(), stdout);
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
    // The top bar (see topbar.h), on the first line.
    insert(new TopBar(TRect(0, 0, size.x, 1)));
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
                                 "': see the Event Viewer (" + globalkeys::menuKeyName() + " V)." );
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
    // [alerts] title-color, seen-color: the frames are drawn again (the windows keep
    // their drawing in a buffer, so redrawing the desktop is not enough).
    if ( alertTitleColor != currentSettings.alertTitleRed ||
         alertSeenColor != currentSettings.alertSeenColor )
    {
        alertTitleColor = currentSettings.alertTitleRed;
        alertSeenColor = currentSettings.alertSeenColor;
        if (TProgram::deskTop)
            TProgram::deskTop->forEach([] (TView *v, void *) {
                auto *w = dynamic_cast<TWindow *>(v);
                if (w && w->frame)
                    w->frame->drawView();
            }, nullptr);
    }
    // [general] window-shadow: the desktop windows already open.
    applyWindowShadowToAll();
    // [general] editor-selection: the editors already open (the color is
    // read when they are drawn, TVTermEditWindow::mapColor); undo-limit-*:
    // their undo steps.
    if (TProgram::deskTop)
        TProgram::deskTop->forEach([] (TView *v, void *) {
            if (auto *w = dynamic_cast<TVTermEditWindow *>(v))
            {
                w->applyUndoLimits();
                // The editor itself: the window keeps a buffer of its last drawing.
                if ((w->state & sfVisible) && w->editor)
                    w->editor->drawView();
            }
        }, nullptr);
    // The 'file-filters' patterns, in the lists of the file dialogs.
    fileopen::addFileFilters(currentSettings.fileFilters);
    // The first menu key, in the status line and in messages.
    globalkeys::setMenuKeyName(currentSettings.globalKeys[gaMenu].front().name());
    if (TopBar::instance)
        TopBar::instance->menuKeyChanged();
    // Log View highlight sets: the built-in ones (unless builtin-highlight =
    // no) and those in the file. The windows take them at their next update.
    rebuildHighlightSets(currentSettings.highlight, path, currentSettings.builtinHighlight, warnings);
    // Log View transformation sets (none built in).
    rebuildTransformSets(currentSettings.transform, path, warnings);

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
                    " problem(s), see the Event Viewer (" + globalkeys::menuKeyName() + " V)." );
    else if (!existed && manual)
        showStatus("No settings file " + path + " (More... > Edit settings creates it).");
    else if (existed && !startup)
    {
        // Not at startup, where there is nothing to report. Only in the
        // status line ('reload-message'), not in the Event Viewer.
        if (manual || currentSettings.reloadMessage)
            showStatus("Settings reloaded.");
    }
    else if (!existed && !startup)
        note("settings file " + path + " removed: using the default settings");
}

void TVTermApp::applyWindowShadow(TView *view)
{
    // Only windows; dialogs (TDialog is a TWindow) keep their shadow.
    if (!dynamic_cast<TWindow *>(view) || dynamic_cast<TDialog *>(view))
        return;
    bool on = settings().windowShadow;
    // setState redraws what is under the window (and its shadow) only if
    // the window is already on the desktop.
    if (bool(view->state & sfShadow) != on)
        view->setState(sfShadow, Boolean(on));
}

void TVTermApp::applyWindowShadowToAll()
{
    // Also the hidden windows of the other desktops.
    if (TProgram::deskTop)
        TProgram::deskTop->forEach([] (TView *v, void *) {
            applyWindowShadow(v);
        }, nullptr);
}

TWindow *TVTermApp::insertWindow(TWindow *window)
{
    if (window)
        applyWindowShadow(window);
    return TApplication::insertWindow(window);
}

void TVTermApp::checkSettingsFile()
{
    using namespace appsettings;
    auto now = std::chrono::steady_clock::now();
    if (now < nextCheck)
        return;
    nextCheck = now + std::chrono::milliseconds(checkIntervalMs);
    // The Settings editor: changes on disk and the editing marker (also
    // without 'auto-reload').
    if (SettingsEditWindow::instance)
        SettingsEditWindow::instance->tick();
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
    if (auto *w = SettingsEditWindow::instance)
    {
        // Already open: brought to the front; from another desktop, moved to
        // this one, in the place of a new Settings editor.
        if (desktops::isHidden(w))
        {
            desktops::moveTo(*deskTop, w, desktops::current());
            TRect r = settingsEditorBounds();
            w->locate(r);
        }
        desktops::selectWindow(*deskTop, w);
        return;
    }
    if (auto *w = (SettingsEditWindow *) insertWindow(new SettingsEditWindow(settingsEditorBounds(), path)))
        // At once: another tvterm may be editing the file.
        w->tick();
}

TRect TVTermApp::settingsEditorBounds()
{
    // Below the Event Viewer, as wide as it, down to the bottom; without an
    // Event Viewer (or without room below it), where other editors open.
    TRect extent = deskTop->getExtent();
    if (auto *viewer = (TView *) message(deskTop, evBroadcast, cmFindEventViewer, nullptr))
    {
        TRect ev = viewer->getBounds();
        TRect r(ev.a.x, ev.b.y, ev.b.x, extent.b.y);
        r.intersect(extent);
        if ( (viewer->state & sfVisible) && !(viewer->state & sfDragging) &&
             r.b.y - r.a.y >= minSettingsEditorHeight && r.b.x - r.a.x >= minSettingsEditorWidth )
            return r;
    }
    return editorBounds(extent);
}

TStatusLine *TVTermApp::initStatusLine(TRect r)
{
    // The status line, on the last line of the screen: the keys of the
    // focused window, the temporary messages and the hints of the top bar
    // (see topbar.h, which has "Ctrl-B Menu", "WinList", the alerts and the
    // desktops). Items of windows that handle their own keys: a click sends
    // the key (see TVTermStatusLine::keyCommand).
    auto keyCmd = [] (ushort keyCode, ushort mods = 0, char ch = 0) {
        return TVTermStatusLine::keyCommand(keyCode, mods, ch);
    };
    r.a.y = r.b.y - 1;
    TStatusLine *statusLine = new TVTermStatusLine(r,
        *new TStatusDef(hcDragging, hcDragging) +
            *new TStatusItem("~↑↓→←~ Move", kbNoKey, 0) +
            *new TStatusItem("~Shift-↑↓→←~ Resize", kbNoKey, 0) +
            *new TStatusItem("~Ctrl~ Move/Resize Faster", kbNoKey, 0) +
            *new TStatusItem("~Enter~ Done", kbNoKey, 0) +
            *new TStatusItem("~Esc~ Cancel", kbNoKey, 0) +
        *new TStatusDef(hcInputGrabbed, hcInputGrabbed) +
            *new TStatusItem("~Alt-End~ Release Input", kbAltEnd, cmReleaseInput) +
        *new TStatusDef(hcSelecting, hcSelecting) +
            *new TStatusItem("~Click & Drag~ Select Text", kbNoKey, 0) +
            *new TStatusItem("~Ctrl-C~ Copy Selection", kbCtrlC, cmCopySelection) +
            *new TStatusItem("~Esc~ Cancel", kbEsc, cmCancelSelection) +
        // History mode: the keys are handled by the terminal view.
        *new TStatusDef(hcHistory, hcHistory) +
            *new TStatusItem("~↑↓ PgUp PgDn Home End~ Scroll", kbNoKey, 0) +
            *new TStatusItem("~/~ Search", kbNoKey, keyCmd('/', 0, '/')) +
            *new TStatusItem("~F3~ Next", kbNoKey, keyCmd(kbF3)) +
            *new TStatusItem("~Shift-F3~ Prev", kbNoKey, keyCmd(kbShiftF3, kbShift)) +
            *new TStatusItem("~F2~ Save", kbNoKey, keyCmd(kbF2)) +
            *new TStatusItem("~Ctrl-C~ Copy", kbNoKey, keyCmd(kbCtrlC, kbCtrlShift)) +
            *new TStatusItem("~Esc~ Exit", kbNoKey, keyCmd(kbEsc)) +
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
        // Event Viewer: its keys are handled by the window.
        *new TStatusDef(hcEventViewer, hcEventViewer) +
            *new TStatusItem("~M~ Moves", kbNoKey, keyCmd('m', 0, 'm')) +
            *new TStatusItem("~C~ Clicks", kbNoKey, keyCmd('c', 0, 'c')) +
            *new TStatusItem("~K~ Keys", kbNoKey, keyCmd('k', 0, 'k')) +
            *new TStatusItem("~O~ Other", kbNoKey, keyCmd('o', 0, 'o')) +
            *new TStatusItem("~T~ Messages", kbNoKey, keyCmd('t', 0, 't')) +
            *new TStatusItem("~N~ Notes", kbNoKey, keyCmd('n', 0, 'n')) +
            *new TStatusItem("~A~ Alerts", kbNoKey, keyCmd('a', 0, 'a')) +
            *new TStatusItem("~X~ Clear", kbNoKey, keyCmd('x', 0, 'x')) +
        *new TStatusDef(hcEditor, hcEditor) +
            *new TStatusItem("~F2~ Save", kbF2, cmSave) +
            // The keys are handled by the editor window (editwnd.cc).
            *new TStatusItem("~F7~ Find", kbNoKey, cmFind) +
            *new TStatusItem("~Ctrl-F7~ Replace", kbNoKey, cmReplace) +
            *new TStatusItem("~F3~ Next", kbNoKey, cmSearchAgain) +
            *new TStatusItem("~Ctrl-Ins~ Copy", kbNoKey, 0) +
            *new TStatusItem("~Shift-Ins~ Paste", kbNoKey, 0) +
            *new TStatusItem("~Shift-Del~ Cut", kbNoKey, 0) +
        *new TStatusDef(hcMenu, hcMenu) +
            *new TStatusItem("~Esc~ Close Menu", kbNoKey, 0) +
        *new TStatusDef(0, 0xFFFF) +
            // Shift-Ins itself is handled by the terminal view (see
            // TerminalView::pasteOnShiftIns); this item is a hint and can be
            // clicked.
            *new TStatusItem("~Shift-Ins~ Paste", kbNoKey, cmPaste)
            );
    return statusLine;
}

TDeskTop *TVTermApp::initDeskTop(TRect r)
{
    // Between the top bar and the status line.
    r.a.y += 1;
    r.b.y -= 1;
    return new TVTermDesk(r);
}

void TVTermApp::noteNoFreeArea()
{
    if (getDeskTop()->noFreeArea)
        note("no free area beside the fixed windows, using the whole desktop");
}

void TVTermApp::tileInN(bool columns)
{
    const char *what = columns ? "columns" : "rows";
    char buf[8];
    snprintf(buf, sizeof(buf), "%d", tileCount);
    for (;;)
    {
        std::string label = std::string("~N~umber of ") + what + " (1-" + std::to_string((int) maxTileCount) + ")";
        if (inputBox(columns ? "Tile in N Columns" : "Tile in N Rows", label.c_str(), buf, 3) == cmCancel)
            return;
        char *end;
        long n = strtol(buf, &end, 10);
        while (*end == ' ')
            ++end;
        if (buf[0] != '\0' && *end == '\0' && n >= 1 && n <= maxTileCount)
        {
            tileCount = (int) n;
            break;
        }
        messageBox(mfError | mfOKButton, "Enter a number from 1 to %d.", (int) maxTileCount);
    }
    tileGrid(tileCount, columns);
}

void TVTermApp::tileGrid(int n, bool columns)
{
    TVTermDesk::GridResult res = getDeskTop()->tileGrid(n, columns, getTileRect());
    noteNoFreeArea();
    if (res.used == 0)
        return;
    auto what = [&] (int count) {
        return std::to_string(count) + (columns ? " column" : " row") + (count == 1 ? "" : "s");
    };
    std::string need = " (windows need " + std::to_string(res.minSize.x) + "x" + std::to_string(res.minSize.y) + ")";
    if (res.reduced)
        note("Tile in " + what(n) + ": only " + std::to_string(res.used) + " fit" + need);
    if (res.crowded)
        note("Tile in " + what(res.used) + ": too many windows, some overlap" + need);
}

void TVTermApp::handleEvent(TEvent &event)
{
    // Instead of TApplication's: the Event Viewer and editors stay in place.
    if (event.what == evCommand && event.message.command == cmCascade)
    {
        getDeskTop()->cascadeWindows(getTileRect());
        noteNoFreeArea();
        clearEvent(event);
    }
    else if (event.what == evCommand && event.message.command == cmTile)
    {
        getDeskTop()->tileHorizontal(getTileRect());
        noteNoFreeArea();
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
                case cmViewFile: viewFile(); break;
                case cmEventViewer: toggleEventViewer(); break;
                case cmNewLogView: newLogView(); break;
                case cmLoadProfile: loadProfile(); break;
                case cmLoadProfileInEditor: loadProfileInEditor(); break;
                case cmEditorSaveAs: editorSaveAs(); break;
                case cmSaveProfile: saveProfile(); break;
                case cmWindowList: windowList(); break;
                case cmRenameTerm: renameTerm(); break;
                case cmEditSettings: editSettings(); break;
                case cmReloadSettings: reloadSettings(true); break;
                case cmAbout: about(); break;
                case cmSwitchDesktop: switchDesktop(event.message.infoInt); break;
                case cmDesktopMenu: desktopMenu(); break;
                case cmMoveWindowMenu: moveWindowMenu(); break;
                case cmShowNotes:
                    if (event.message.infoPtr)
                        showNotes(*(TEventViewer *) event.message.infoPtr, true);
                    break;
                case cmShowAlerts:
                    if (event.message.infoPtr)
                        ((TEventViewer *) event.message.infoPtr)->printText(listAlerts(*deskTop));
                    break;
                case cmAlertSettings: alertSettings(); break;
                case cmNextAlert: nextAlert(); break;
                case cmTileCols: getDeskTop()->tileVertical(getTileRect()); noteNoFreeArea(); break;
                case cmTileRows: getDeskTop()->tileHorizontal(getTileRect()); noteNoFreeArea(); break;
                case cmTileNCols: tileInN(true); break;
                case cmTileNRows: tileInN(false); break;
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
    // Terminals of the external viewer / editor: closed when it ends.
    for (TWindow *w : getDeskTopWindows(*deskTop))
        if (auto *term = dynamic_cast<TerminalWindow *>(w))
            if (term->closeWhenDone && term->programEnded())
                term->close();
    TVTermStatusLine::checkExpired();
    checkSettingsFile();
    // Closed windows of the other desktops.
    desktops::prune(*deskTop);
    // The alerts and the desktops, in the top bar.
    if (TopBar::instance)
        TopBar::instance->update(*deskTop);
}

namespace appmenu
{

// The Ctrl-B menu. F, the former letter of Zoom (Maximize/Restore), still
// works. The digits 1-9 show that desktop, 0 the previous one: the command
// is sent once the menu is closed (TMenuView::execute puts back the
// evCommand that ends it).
class MainMenuPopup : public CenteredMenuPopup
{
public:

    using CenteredMenuPopup::CenteredMenuPopup;

    void getEvent(TEvent &event) override
    {
        CenteredMenuPopup::getEvent(event);
        if ( event.what == evKeyDown && event.keyDown.textLength == 1 &&
             event.keyDown.text[0] >= '0' && event.keyDown.text[0] <= '9' &&
             !(event.keyDown.controlKeyState & (kbCtrlShift | kbAltShift)) )
        {
            int n = event.keyDown.text[0] - '0';
            event.what = evCommand;
            event.message.command = cmSwitchDesktop;
            event.message.infoInt = n;
        }
        else if (event.what == evKeyDown && TKey(event.keyDown) == TKey('F'))
        {
            event.keyDown.keyCode = 'z';
            event.keyDown.controlKeyState = 0;
            event.keyDown.text[0] = 'z';
            event.keyDown.textLength = 1;
        }
    }
};

} // namespace appmenu

using appmenu::MainMenuPopup;

void TVTermApp::openMenu()
{
    TMenuItem &menuItems =
        // Order agreed in chat 43: desktops and windows, alerts, terminals,
        // window size, text and editors, profiles, the rest. Letters of
        // item 30 (chat 43); free: J K X (F is the former Zoom letter).
        *new TMenuItem("Desktop...", cmDesktopMenu, 'D', hcNoContext, "~D~,~0~-~9~") +
        *new TMenuItem("Window List...", cmWindowList, 'W', hcNoContext, "~W~") +
        *new TMenuItem("Move Window to Desktop...", cmMoveWindowMenu, 'Y', hcNoContext, "~Y~") +
        newLine() +
        *new TMenuItem("Log View (filtered output)...", cmNewLogView, 'L', hcNoContext, "~L~") +
        *new TMenuItem("Alerts...", cmAlertSettings, 'A', hcNoContext, "~A~") +
        *new TMenuItem("Go to Next Alert", cmNextAlert, 'G', hcNoContext, "~G~") +
        newLine() +
        *new TMenuItem("New Term", cmNewTerm, 'N', hcNoContext, "~N~") +
        // No key: closed only on purpose (arrows + Enter, or the mouse).
        *new TMenuItem("Close Term", cmClose, kbNoKey) +
        *new TMenuItem("Rename Term...", cmRenameTerm, kbNoKey) +
        *new TMenuItem("Next Term", cmNext, kbTab, hcNoContext, "~Tab~") +
        *new TMenuItem("Previous Term", cmPrev, kbShiftTab, hcNoContext, "~Shift-Tab~") +
        *new TMenuItem("History (Scroll/Search/Save)", cmHistoryMode, 'H', hcNoContext, "~H~") +
        newLine() +
        *new TMenuItem("Resize/Move", cmResize, 'R', hcNoContext, "~R~") +
        *new TMenuItem("Zoom (Maximize/Restore)", cmZoom, 'Z', hcNoContext, "~Z~") +
        newLine() +
        *new TMenuItem("Select Text", cmStartSelection, 'T', hcNoContext, "~T~") +
        *new TMenuItem("Paste", cmPaste, 'P', hcNoContext, "~P~") +
        *new TMenuItem("New Editor", cmNewEditor, 'E', hcNoContext, "~E~") +
        *new TMenuItem("Open File in Editor...", cmOpenEditor, 'O', hcNoContext, "~O~") +
        *new TMenuItem("Editor Save As...", cmEditorSaveAs, 'S', hcNoContext, "~S~") +
        newLine() +
        *new TMenuItem("Load Profile...", cmLoadProfile, 'B', hcNoContext, "~B~") +
        *new TMenuItem("Load Profile in Editor...", cmLoadProfileInEditor, 'I', hcNoContext, "~I~") +
        *new TMenuItem("Save Profile As...", cmSaveProfile, 'C', hcNoContext, "~C~") +
        newLine() +
        *new TMenuItem("Event Viewer", cmEventViewer, 'V', hcNoContext, "~V~") +
        ( *new TSubMenu("~M~ore...", kbNoKey, hcMenu) +
            *new TMenuItem("Change working ~d~ir...", cmChangeDir, kbNoKey) +
            newLine() +
            *new TMenuItem("Tile (~C~olumns First)", cmTileCols, kbNoKey) +
            *new TMenuItem("Tile (~R~ows First)", cmTileRows, kbNoKey) +
            *new TMenuItem("Tile in N C~o~lumns...", cmTileNCols, kbNoKey) +
            *new TMenuItem("Tile in N Ro~w~s...", cmTileNRows, kbNoKey) +
            *new TMenuItem("C~a~scade", cmCascade, kbNoKey) +
            *new TMenuItem("~G~rab Input", cmGrabInput, kbNoKey) +
            newLine() +
            *new TMenuItem("~V~iew file...", cmViewFile, kbNoKey) +
            newLine() +
            *new TMenuItem("~E~dit settings (tvterm.ini)", cmEditSettings, kbNoKey) +
            *new TMenuItem("Reload se~t~tings", cmReloadSettings, kbNoKey) +
            newLine() +
            *new TMenuItem("A~b~out tvterm...", cmAbout, kbNoKey)
        ) +
        *new TMenuItem("Suspend", cmDosShell, 'U', hcNoContext, "~U~") +
        *new TMenuItem("Exit", cmQuit, 'Q', hcNoContext, "~Q~");

    TMenu *menu = new TMenu(menuItems);
    TMenuPopup *menuPopup = new MainMenuPopup(menu);
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
    // The first 'file-filters' pattern (tvterm.ini); Down: the others.
    std::string filter = fileopen::firstFileFilter();
    char fileName[MAXPATH];
    strnzcpy(fileName, filter, sizeof(fileName));
    TFileDialog *d = fileopen::newFileDialog(filter, "Open file", fdOpenButton, fileopen::hiOpenFile);
    TView *p = validView(d);
    if (!p)
        return;
    ushort result = deskTop->execView(p);
    if (result != cmCancel)
        p->getData(fileName);
    TObject::destroy(p);
    // A large file asks first (see fileopen.h).
    if (result != cmCancel)
        fileopen::open(fileName, fileopen::Target::Editor);
}

void TVTermApp::viewFile()
{
    std::string filter = fileopen::firstFileFilter();
    char fileName[MAXPATH];
    strnzcpy(fileName, filter, sizeof(fileName));
    TFileDialog *d = fileopen::newFileDialog(filter, "View file", fdOpenButton, fileopen::hiViewFile);
    TView *p = validView(d);
    if (!p)
        return;
    ushort result = deskTop->execView(p);
    if (result != cmCancel)
        p->getData(fileName);
    TObject::destroy(p);
    if (result != cmCancel)
        fileopen::view(fileName);
}

TRect TVTermApp::editorRect()
{
    return editorBounds(TProgram::deskTop->getExtent());
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
    // The hints of the top bar follow the mouse.
    if ((event.what & evMouse) && TopBar::instance)
        TopBar::instance->hover(event.mouse.where);
    // Show every event in the Event Viewer, if one is open.
    if (event.what != evNothing)
        if (auto *viewer = (TEventViewer *) message(deskTop, evBroadcast, cmFindEventViewer, nullptr))
            viewer->print(event);
    // Global keys (menu-key, key-zoom, key-window-*): not in dialogs, menus
    // or Grab Input (another view is modal then), nor while a window is
    // being moved or resized.
    if ( event.what == evKeyDown && TopView() == this &&
         !(deskTop->current && (deskTop->current->state & sfDragging)) )
        handleGlobalKey(event);
    // Clipboard keys and pasted text in the text fields of dialogs.
    if (event.what == evKeyDown)
        dlgclip::handleKeyEvent(*this, event);
}

namespace appkeys
{

static bool far2lActiveIn(TView *view)
// Whether 'view' is a terminal whose application uses far2l's extensions.
{
    if (auto *win = dynamic_cast<TerminalWindow *>(view))
        if (auto *termView = win->getTerminalView())
            return termView->far2lExtensionsEnabled();
    return false;
}

static void selectNeighbour(TDeskTop &desk, int dx, int dy)
// The window beside the focused one in the direction (dx, dy): among the
// windows whose center lies in that direction, those that share rows
// (left/right) or columns (up/down) with it come first, then the nearest
// edge, then the one on top. Nothing happens at the edge of the screen.
{
    TView *cur = desk.current;
    if (!cur)
        return;
    TRect c = cur->getBounds();
    int cx = c.a.x + c.b.x, cy = c.a.y + c.b.y; // Centers, doubled.
    TView *best = nullptr;
    int bestAside = 0, bestGap = 0;
    int z = 0;
    for (TView *p = desk.first(); p; p = p->nextView(), ++z)
    {
        if ( p == cur || !(p->state & sfVisible) || !(p->options & ofSelectable) ||
             !dynamic_cast<TWindow *>(p) )
            continue;
        TRect r = p->getBounds();
        int px = r.a.x + r.b.x, py = r.a.y + r.b.y;
        if ((dx ? (px - cx) * dx : (py - cy) * dy) <= 0)
            continue;
        int gap = dx > 0 ? r.a.x - c.b.x : dx < 0 ? c.a.x - r.b.x :
                  dy > 0 ? r.a.y - c.b.y : c.a.y - r.b.y;
        if (gap < 0)
            gap = 0;
        bool beside = dx ? (r.a.y < c.b.y && c.a.y < r.b.y)
                         : (r.a.x < c.b.x && c.a.x < r.b.x);
        int aside = beside ? 0 : 1;
        // Windows are visited from the top one down: on a tie, the first wins.
        if (!best || aside < bestAside || (aside == bestAside && gap < bestGap))
        {
            best = p;
            bestAside = aside;
            bestGap = gap;
        }
    }
    if (best)
        best->select();
}

} // namespace appkeys

void TVTermApp::handleGlobalKey(TEvent &event)
{
    const auto &keys = appsettings::currentSettings.globalKeys;
    bool far2l = false, far2lChecked = false;
    for (int action = 0; action < gaCount; ++action)
        for (const GlobalKey &k : keys[action])
        {
            if (!k.matches(event.keyDown))
                continue;
            if (k.notFar2l)
            {
                if (!far2lChecked)
                    far2l = appkeys::far2lActiveIn(deskTop->current), far2lChecked = true;
                if (far2l)
                    continue;
            }
            switch (action)
            {
                case gaMenu:
                case gaZoom:
                    event.what = evCommand;
                    event.message.command = action == gaMenu ? cmMenu : cmZoom;
                    event.message.infoPtr = nullptr;
                    return;
                case gaWindowLeft: appkeys::selectNeighbour(*deskTop, -1, 0); break;
                case gaWindowRight: appkeys::selectNeighbour(*deskTop, 1, 0); break;
                case gaWindowUp: appkeys::selectNeighbour(*deskTop, 0, -1); break;
                case gaWindowDown: appkeys::selectNeighbour(*deskTop, 0, 1); break;
            }
            clearEvent(event);
            return;
        }
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
    std::vector<int> windowDesktops; // Beside 'windows'.
    VTermEmulatorFactory factory;
    // Windows closed just before (Load Profile): forgotten first.
    desktops::prune(*deskTop);
    // Ctrl-B 0 after loading: the desktop shown before (the switches done
    // while loading do not count).
    int shownBefore = desktops::current(), previousBefore = desktops::previous();
    // Desktop names: those in the profile replace the current ones.
    for (const auto &dn : profile.desktopNames)
        if (!desktops::setName(dn.first, dn.second))
            note("profile " + profile.path + ": desktop-name-" + std::to_string(dn.first) +
                 " cut to " + std::to_string((int) desktops::nameMax) + " columns: '" +
                 desktops::name(dn.first) + "'");
    if (profile.hasWindowListSize)
        setWindowListSize(TPoint {profile.windowListWidth, profile.windowListHeight});

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
                o.highlight = win.logHighlight;
                o.highlightOn = win.logHighlightOn;
                o.highlightWordsMode = win.logHighlightWordsMode;
                o.highlightWords = win.logHighlightWords;
                o.transform = win.logTransform;
                o.transformOn = win.logTransformOn;
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
            {
                windows.push_back(inserted);
                windowDesktops.push_back(win.kind == WindowProfile::EventViewer ? 0 : win.desktop);
            }
    }

    // The windows get their order (Window List, Tile in N) as in the profile.
    desktops::prune(*deskTop);
    // Each window on its desktop (the Event Viewer: on all of them). The
    // layout is applied to each desktop that got windows, the one to show
    // last.
    int target = profile.desktop;
    std::vector<int> used;
    for (size_t i = 0; i < windows.size(); ++i)
    {
        int d = windowDesktops[i];
        if (d == 0)
            continue;
        desktops::moveTo(*deskTop, windows[i], d);
        if (d != target && std::find(used.begin(), used.end(), d) == used.end())
            used.push_back(d);
    }
    std::sort(used.begin(), used.end());
    used.push_back(target);
    for (int d : used)
    {
        desktops::switchTo(*deskTop, d);
        size_t count = 0;
        for (size_t i = 0; i < windows.size(); ++i)
            count += windowDesktops[i] == d;
        Profile::Layout layout = profile.layout;
        if (layout == Profile::Auto)
            layout = anyPos || count < 2 ? Profile::Manual : Profile::TileColumns;
        if (count == 0)
            layout = Profile::Manual;
        switch (layout)
        {
            case Profile::TileColumns: getDeskTop()->tileVertical(getTileRect()); noteNoFreeArea(); break;
            case Profile::TileRows: getDeskTop()->tileHorizontal(getTileRect()); noteNoFreeArea(); break;
            case Profile::Cascade: getDeskTop()->cascadeWindows(getTileRect()); noteNoFreeArea(); break;
            case Profile::Columns: tileGrid(profile.layoutCount, true); break;
            case Profile::Rows: tileGrid(profile.layoutCount, false); break;
            default: break;
        }
    }

    if (profile.focus > 0 && (size_t) profile.focus <= windows.size())
        desktops::selectWindow(*deskTop, windows[profile.focus - 1]);
    desktops::setPrevious(desktops::current() != shownBefore ? shownBefore : previousBefore);
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
        return fileopen::newFileDialog("*.ini", title, options, histId);
    TFileDialog *d = fileopen::newFileDialog("*.ini", title, options | fdNoLoadDir, histId);
    std::string path = dir;
    if (path.back() != '/')
        path += '/';
    // Names typed in the dialog are relative to 'directory'.
    fileopen::setDialogDirectory(d, path);
    return d;
}

} // namespace profiledlg

void TVTermApp::loadProfile()
{
    char fileName[MAXPATH];
    TFileDialog *d = profiledlg::newDialog(profileDir(), "Load profile", fdOpenButton,
                                              fileopen::hiLoadProfile);
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

void TVTermApp::loadProfileInEditor()
// A profile from the profiles directory, opened in a tvterm editor (to read
// or change it; nothing is loaded).
{
    char fileName[MAXPATH];
    TFileDialog *d = profiledlg::newDialog(profileDir(), "Load profile in editor", fdOpenButton,
                                              fileopen::hiLoadProfile);
    TView *p = validView(d);
    if (!p)
        return;
    ushort result = deskTop->execView(p);
    if (result != cmCancel)
        p->getData(fileName);
    TObject::destroy(p);
    if (result != cmCancel)
        fileopen::open(fileName, fileopen::Target::Editor);
}

void TVTermApp::editorSaveAs()
// Save As of the focused editor (the Settings editor too: after saving to
// another file it is a normal editor).
{
    auto *w = dynamic_cast<TEditWindow *>(deskTop->current);
    if (!w)
    {
        messageBox("The focused window is not an editor.", mfInformation | mfOKButton);
        return;
    }
    message(w, evCommand, cmSaveAs, nullptr);
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
    auto *log = dynamic_cast<LogViewWindow *>(deskTop->current);
    if (log && log->hasFixedLines())
        messageBox( "This Log View has fixed lines (made by Search > New window):\n"
                    "no new lines come, so alerts are not used.", mfInformation | mfOKButton );
    else if (auto *target = dynamic_cast<AlertTarget *>(deskTop->current))
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
    term.alertKeywords = alerts.keywords.text;
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
    // With those of the other desktops (hidden).
    std::vector<TWindow *> windows = getDeskTopWindows(*deskTop);
    Profile profile;
    profile.desktop = desktops::current();
    for (int n = 1; n <= desktops::count; ++n)
        if (!desktops::name(n).empty())
            profile.desktopNames[n] = desktops::name(n);
    {
        TPoint size;
        if (getWindowListSize(size))
        {
            profile.hasWindowListSize = true;
            profile.windowListWidth = size.x;
            profile.windowListHeight = size.y;
        }
    }
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
            // Fixed lines (Search > New window): nothing to start again.
            if (log->hasFixedLines())
                continue;
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
            win.logHighlight = o.highlight;
            win.logHighlightOn = o.highlightOn;
            win.logHighlightWordsMode = o.highlightWordsMode;
            win.logHighlightWords = o.highlightWords;
            win.logTransform = o.transform;
            win.logTransformOn = o.transformOn;
            const AlertSettings &alerts = log->getAlertSettings();
            win.alertActivity = alerts.activity;
            win.alertKeywords = alerts.keywords.text;
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
        // The Event Viewer is on every desktop (no key).
        profile.windows.back().desktop = std::max(1, desktops::of(w));
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
    TFileDialog *d = profiledlg::newDialog(dir, "Save profile as", fdOKButton,
                                              fileopen::hiSaveProfile);
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

// Desktops (see desktops.h).

void TVTermApp::switchDesktop(int n)
// 0: the previous desktop (nothing if there is none).
{
    if (n == 0)
        desktops::switchToPrevious(*deskTop);
    else if (n >= 1 && n <= desktops::count)
        desktops::switchTo(*deskTop, n);
}

namespace desktopmenu
{

// The popup of Desktop... and Ctrl-B Y. Its first item is the header of the
// columns (disabled); the arrow keys skip it.
class DesktopMenuPopup : public CenteredMenuPopup
{
public:

    DesktopMenuPopup(TMenu *aMenu) noexcept :
        CenteredMenuPopup(aMenu)
    {
    }

    void getEvent(TEvent &event) override
    {
        // The previous key moved onto the header: go on to the next item
        // in the same direction (Up from the first row: the last one).
        if (current && current == menu->items && lastWasKey)
        {
            if (lastUp)
            {
                TMenuItem *p = menu->items;
                while (p->next)
                    p = p->next;
                current = p;
            }
            else if (current->next)
                current = current->next;
            drawView();
        }
        CenteredMenuPopup::getEvent(event);
        if (event.what == evKeyDown)
        {
            ushort key = ctrlToArrow(event.keyDown.keyCode);
            lastWasKey = true;
            lastUp = key == kbUp || key == kbEnd;
        }
        else if (event.what & evMouse)
            lastWasKey = false;
    }

private:

    bool lastWasKey {false};
    bool lastUp {false};
};

} // namespace desktopmenu

int TVTermApp::chooseDesktop(bool moving)
// A menu with the desktops 1-9; the digits choose. Columns: S (the digit),
// C (+ the current desktop), Win (its windows), Title (:name or :D2).
{
    auto counts = desktops::windowCounts(*deskTop);
    TMenuItem *first = new TMenuItem(moving ? "S C Win Title (move window to)" : "S C Win Title",
                                     cmDesktopPick, kbNoKey);
    first->disabled = True;
    TMenuItem *last = first;
    for (int n = 1; n <= desktops::count; ++n)
    {
        char win[8];
        snprintf(win, sizeof(win), "%3d", counts[n]);
        const std::string &name = desktops::name(n);
        std::string text = "~" + std::to_string(n) + "~ " +
                           (n == desktops::current() ? "+" : " ") + " " + win + " :" +
                           (name.empty() ? "D" + std::to_string(n) : name);
        auto *item = new TMenuItem(text.c_str(), cmDesktopPick + n, '0' + n, hcNoContext);
        last->next = item;
        last = item;
    }
    TMenu *menu = new TMenu(*first);
    // The current desktop is the default item.
    TMenuItem *item = first->next;
    for (int n = 1; n < desktops::current() && item->next; ++n)
        item = item->next;
    menu->deflt = item;
    TMenuPopup *popup = new desktopmenu::DesktopMenuPopup(menu);
    popup->helpCtx = hcMenu;
    ushort cmd = execView(popup);
    TObject::destroy(popup);
    if (cmd > cmDesktopPick && cmd <= cmDesktopPick + desktops::count)
        return cmd - cmDesktopPick;
    return 0;
}

void TVTermApp::desktopMenu()
{
    if (int n = chooseDesktop(false))
        switchDesktop(n);
}

void TVTermApp::moveWindowMenu()
{
    auto *w = dynamic_cast<TWindow *>(deskTop->current);
    if (!w)
    {
        messageBox("There is no window to move.", mfInformation | mfOKButton);
        return;
    }
    if (desktops::isGlobal(w))
    {
        messageBox("The Event Viewer is on every desktop.", mfInformation | mfOKButton);
        return;
    }
    int n = chooseDesktop(true);
    if (n == 0 || n == desktops::current())
        return;
    desktops::moveTo(*deskTop, w, n);
    std::string where = std::to_string(n);
    if (!desktops::name(n).empty())
        where += " (" + desktops::name(n) + ")";
    appsettings::showStatus("Window moved to desktop " + where + ".");
}
