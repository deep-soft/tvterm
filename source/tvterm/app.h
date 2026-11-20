#ifndef TVTERM_APP_H
#define TVTERM_APP_H

#define Uses_TApplication
#define Uses_TCommandSet
#include <tvision/tv.h>

#include <string>

class TVTermDesk;
struct Profile;
struct Settings;

struct TVTermApp : public TApplication
{
    static TCommandSet tileCmds;

    // 'profileName': profile to open at startup (see profile.h), or null.
    // 'useDefaultProfile': if 'profileName' is null, open the profile set
    // in the settings file (tvterm.ini), if any.
    TVTermApp(const char *profileName = nullptr, bool useDefaultProfile = true);
    static TStatusLine* initStatusLine(TRect r);
    static TDeskTop* initDeskTop(TRect r);

    TVTermDesk* getDeskTop();

    void handleEvent(TEvent &event) override;
    Boolean valid(ushort command) override;
    void idle() override;
    void getEvent(TEvent &event) override;
    // Every desktop window goes through here: its shadow follows
    // 'window-shadow' (tvterm.ini). Dialogs keep theirs.
    TWindow *insertWindow(TWindow *window) override;
    // 'window-shadow' for one window (not a dialog) / for all the desktop
    // windows (after a reload).
    static void applyWindowShadow(TView *view);
    static void applyWindowShadowToAll();
    // Global keys (tvterm.ini, [keys]): turns 'event' into a command or
    // handles it.
    void handleGlobalKey(TEvent &event);
    // Ctrl-Del in an open history list (see histfile.h).
    bool deleteHistoryEntry();

    size_t getOpenTermCount();

    // Command handlers

    void openMenu();
    void newTerm();
    void changeDir();
    void newEditor();
    void openEditor();
    // Ctrl-B F (View file).
    void viewFile();
    // Where a new editor goes (80 columns on the right of a wide desktop).
    static TRect editorRect();
    // Where a new Event Viewer goes (Ctrl-B V): 70 columns on the right of a
    // wide desktop, its whole height. Also the Help window when there is no
    // Event Viewer (E102).
    static TRect eventViewerRect();
    void toggleEventViewer();
    void newLogView();
    void loadProfile();
    void loadProfileInEditor();
    void editorSaveAs();
    // Asks whether to close the open windows before loading a profile.
    // Returns false if the profile should not be loaded. Pinned windows
    // (E84) that stay open go to 'keptPinned'.
    bool closeWindowsBeforeLoad();
    // Load Profile (E84): the pinned windows kept open; a profile window
    // with the same title and command as one of them is skipped.
    std::vector<TWindow *> keptPinned;
    // Windows > Pin / Unpin (E84).
    void pinWindow();
    // Windows > Pin all / Unpin all (E87): every terminal, Log View (not the
    // Help window) and editor, on every Desk; the count in the status line.
    void pinAll(bool on);
    // After Tile / Cascade: a note if the fixed windows left no room.
    void noteNoFreeArea();
    // Windows > Tile in N Columns / Rows: asks for N (1-20; the last value
    // is proposed), then tileGrid.
    void tileInN(bool columns);
    // Tile in 'n' columns / rows (see TVTermDesk::tileGrid), with notes in
    // the Event Viewer when 'n' was lowered or windows overlap.
    void tileGrid(int n, bool columns);
    enum { maxTileCount = 20 };
    void saveProfile();
    void windowList();
    void renameTerm();
    void editSettings();
    // Where Edit settings puts the Settings editor (see setedit.h).
    TRect settingsEditorBounds();
    enum { minSettingsEditorWidth = 24, minSettingsEditorHeight = 6 };
    // 'manual': requested by the user (always reports the result).
    // 'startup': first reading, only problems are reported.
    void reloadSettings(bool manual, bool startup = false);
    void about();
    // Desktops (see desktops.h).
    void switchDesktop(int n);
    // Event Viewer Ctrl-Right / Ctrl-Left (E77): the next / previous desktop
    // (1-9) with windows ('direction' +1 / -1), the Event Viewer keeping the
    // focus; none: a status message.
    void showNextDesk(int direction);
    // More... > Scroll Log Views to last, Event Viewer Ctrl-L (E78): every
    // Log View (all the desktops) to its last line; a status message.
    void logViewsToEnd();
    void desktopMenu();
    void moveWindowMenu();
    // A menu of the desktops 1-9; returns the chosen one, or 0. F2 on a
    // row (E104, not 'moving'): Rename Desk, then the menu again.
    int chooseDesktop(bool moving);
    int chooseDesktopOnce(bool moving, int &rename);
    void renameDesktop(int n);

    // Current settings (tvterm.ini).
    static const Settings &settings();
    // A message from tvterm, shown in the Event Viewer (kept until one is
    // opened).
    static void note(const std::string &text);
    // A message shown once (E88): printed in an open Event Viewer, not kept
    // (N does not show it again).
    static void noteOnce(const std::string &text);
    // A note always kept (E93): not dropped as the oldest, not emptied by
    // Shift-N. The startup notes; 'loadedProfile': the profile of the
    // last Load Profile (replaces the previous one).
    static void noteLasting(const std::string &text, bool loadedProfile = false);
    // Event Viewer Shift-N / F9 (E88): empties the kept messages (except
    // the lasting ones, E93); their number.
    static size_t clearNotes();
    void alertSettings();
    void nextAlert();

    bool openProfile(const std::string &nameOrPath);
    void openProfile(const Profile &profile);

private:

    // Message to show once the application is running (e.g. profile errors
    // found at startup).
    std::string pendingMessage;
    // Last profile loaded (path), shown in the About dialog.
    std::string loadedProfile;
    // Last N of Tile in N Columns / Rows (kept while tvterm runs).
    int tileCount {2};
    // [profile] desktop-main of the profiles loaded (E62; 0: none), written
    // again by Save Profile As.
    int keptDesktopMain {0};
    // The same as a desktop name (E77: 'desktop-main = mail'), as written.
    std::string keptDesktopMainName;

    void checkSettingsFile();

};

inline TVTermDesk* TVTermApp::getDeskTop()
{
    return (TVTermDesk*) deskTop;
}

#endif // TVTERM_APP_H
