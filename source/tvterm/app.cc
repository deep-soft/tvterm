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
#include <tvision/tv.h>

#include "app.h"
#include "cmds.h"
#include "desk.h"
#include "wnd.h"
#include "apputil.h"
#include "editwnd.h"
#include "evntview.h"
#include "profile.h"
#include <tvterm/termctrl.h>
#include <tvterm/vtermemu.h>

#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <vector>

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
        "  -p, --profile PROFILE   open the terminals described in PROFILE\n"
        "  -h, --help              show this help\n"
        "\n"
        "PROFILE is a file path (if it contains '/') or a name, looked up as\n"
        "  %s/PROFILE.ini\n"
        "See examples/profiles/example.ini in the tvterm sources for the format.\n",
        argv0, profileDir().c_str());
}

int main(int argc, char **argv)
{
    const char *profileName = nullptr;
    for (int i = 1; i < argc; ++i)
    {
        const char *arg = argv[i];
        if (!strcmp(arg, "-h") || !strcmp(arg, "--help"))
        {
            printUsage(stdout, argv[0]);
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

    TVTermApp app(profileName);
    app.run();
    app.shutDown();
}

TVTermApp::TVTermApp(const char *profileName) :
    TProgInit( &TVTermApp::initStatusLine,
               nullptr,
               &TVTermApp::initDeskTop
             )
{
    disableCommands(tileCmds);
    for (ushort cmd : TerminalWindow::appConsts.focusedCmds())
        disableCommand(cmd);
    disableCommand(cmPaste);
    TVTermEditWindow::initEditorDialogs();

    if (!profileName || !openProfile(profileName))
        newTerm();
}

TStatusLine *TVTermApp::initStatusLine(TRect r)
{
    r.b.y = r.a.y + 1;
    TStatusLine *statusLine = new TStatusLine(r,
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
            *new TStatusItem(0 , kbCtrlB, cmMenu) +
        *new TStatusDef(hcEditor, hcEditor) +
            *new TStatusItem("~F2~ Save", kbF2, cmSave) +
            *new TStatusItem("~Ctrl-Ins~ Copy", kbNoKey, 0) +
            *new TStatusItem("~Shift-Ins~ Paste", kbNoKey, 0) +
            *new TStatusItem("~Shift-Del~ Cut", kbNoKey, 0) +
            *new TStatusItem("~Ctrl-B~ Open Menu", kbCtrlB, cmMenu) +
        *new TStatusDef(hcMenu, hcMenu) +
            *new TStatusItem("~Esc~ Close Menu", kbNoKey, 0) +
        *new TStatusDef(0, 0xFFFF) +
            *new TStatusItem("~Ctrl-B~ Open Menu" , kbCtrlB, cmMenu) +
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
                case cmLoadProfile: loadProfile(); break;
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
        if (size_t count = getOpenTermCount())
        {
            auto *format = (count == 1)
                ? "There is %zu open terminal window.\nDo you want to quit anyway?"
                : "There are %zu open terminal windows.\nDo you want to quit anyway?";
            if (messageBox(
                    mfConfirmation | mfYesButton | mfNoButton,
                    format, count
                ) != cmYes)
                return False;
        }
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
        auto isTileable =
            [] (TView *p, void *) -> Boolean { return p->options & ofTileable; };
        if (deskTop->firstThat(isTileable, nullptr))
            enableCommands(tileCmds);
        else
            disableCommands(tileCmds);
    }
    message(this, evBroadcast, cmCheckTerminalUpdates, nullptr);
}

void TVTermApp::openMenu()
{
    TMenuItem &menuItems =
        *new TMenuItem("New Term", cmNewTerm, 'N', hcNoContext, "~N~") +
        *new TMenuItem("Close Term", cmClose, 'W', hcNoContext, "~W~") +
        newLine() +
        *new TMenuItem("Next Term", cmNext, kbTab, hcNoContext, "~Tab~") +
        *new TMenuItem("Previous Term", cmPrev, kbShiftTab, hcNoContext, "~Shift-Tab~") +
        *new TMenuItem("Resize/Move", cmResize, 'R', hcNoContext, "~R~") +
        *new TMenuItem("Maximize/Restore", cmZoom, 'F', hcNoContext, "~F~") +
        newLine() +
        *new TMenuItem("Select Text", cmStartSelection, 'S', hcNoContext, "~S~") +
        *new TMenuItem("Paste", cmPaste, 'P', hcNoContext, "~P~") +
        newLine() +
        *new TMenuItem("New Editor", cmNewEditor, 'E', hcNoContext, "~E~") +
        *new TMenuItem("Open File in Editor...", cmOpenEditor, 'O', hcNoContext, "~O~") +
        *new TMenuItem("Event Viewer", cmEventViewer, 'V', hcNoContext, "~V~") +
        *new TMenuItem("Load Profile...", cmLoadProfile, 'L', hcNoContext, "~L~") +
        newLine() +
        ( *new TSubMenu("~M~ore...", kbNoKey, hcMenu) +
            *new TMenuItem("Change working ~d~ir...", cmChangeDir, kbNoKey) +
            newLine() +
            *new TMenuItem("Tile (~C~olumns First)", cmTileCols, kbNoKey) +
            *new TMenuItem("Tile (~R~ows First)", cmTileRows, kbNoKey) +
            *new TMenuItem("C~a~scade", cmCascade, kbNoKey) +
            *new TMenuItem("~G~rab Input", cmGrabInput, kbNoKey)
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
        TRect r = deskTop->getExtent();
        if (r.b.x - r.a.x > 100)
            r.a.x = r.b.x - 70;
        insertWindow(new TEventViewer(r, 0x0F00));
    }
}

void TVTermApp::getEvent(TEvent &event)
{
    TApplication::getEvent(event);
    // Show every event in the Event Viewer, if one is open.
    if (event.what != evNothing)
        if (auto *viewer = (TEventViewer *) message(deskTop, evBroadcast, cmFindEventViewer, nullptr))
            viewer->print(event);
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
    if (profile.terminals.empty())
    {
        if (warnings.empty())
            pendingMessage = "Profile " + profile.path + " has no [terminal] sections.";
        return false;
    }
    openProfile(profile);
    return true;
}

void TVTermApp::openProfile(const Profile &profile)
{
    using namespace tvterm;
    TRect ext = deskTop->getExtent();
    int width = ext.b.x - ext.a.x, height = ext.b.y - ext.a.y;
    bool anyPos = false;
    std::vector<TWindow *> windows;
    VTermEmulatorFactory factory;

    for (const auto &term : profile.terminals)
    {
        TRect r = ext;
        if (term.hasPos)
        {
            anyPos = true;
            // Computed from the edges, so that adjacent terminals touch.
            r = TRect( ext.a.x + width * term.x / 100,
                       ext.a.y + height * term.y / 100,
                       ext.a.x + width * (term.x + term.w) / 100,
                       ext.a.y + height * (term.y + term.h) / 100 );
        }
        ClientOptions options;
        options.command = term.command.c_str();
        options.workingDir = term.dir.c_str();
        options.keepOpen = term.keepOpen;
        auto *termCtrl = TerminalController::create( TerminalWindow::viewSize(r),
                                                     factory, onTermError, options );
        if (!termCtrl)
            continue;
        auto *window = new TerminalWindow(r, *termCtrl);
        if (!term.title.empty())
            window->setCustomTitle(term.title);
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
        case Profile::Cascade: deskTop->cascade(getTileRect()); break;
        default: break;
    }

    if (profile.focus > 0 && (size_t) profile.focus <= windows.size())
        windows[profile.focus - 1]->select();
}

void TVTermApp::loadProfile()
{
    std::string dir = profileDir();
    bool hasDir = access(dir.c_str(), F_OK) == 0;
    std::string wildCard = hasDir ? dir + "/*.ini" : std::string("*.ini");
    char fileName[MAXPATH];
    strncpy(fileName, wildCard.c_str(), sizeof(fileName) - 1);
    fileName[sizeof(fileName) - 1] = '\0';
    TFileDialog *d = new TFileDialog(fileName, "Load profile", "~N~ame", fdOpenButton, 100);
    if (hasDir)
    {
        // Names typed in the dialog are relative to 'directory', which
        // otherwise is the current directory, not the one being listed.
        delete[] (char *) d->directory;
        d->directory = newStr((dir + "/").c_str());
    }
    TView *p = validView(d);
    if (!p)
        return;
    ushort result = deskTop->execView(p);
    if (result != cmCancel)
        p->getData(fileName);
    TObject::destroy(p);
    if (result != cmCancel && !openProfile(std::string(fileName)) && pendingMessage.empty())
        pendingMessage = "No terminal was opened.";
}
