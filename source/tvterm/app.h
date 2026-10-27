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

    size_t getOpenTermCount();

    // Command handlers

    void openMenu();
    void newTerm();
    void changeDir();
    void newEditor();
    void openEditor();
    void toggleEventViewer();
    void newLogView();
    void loadProfile();
    // Asks whether to close the open windows before loading a profile.
    // Returns false if the profile should not be loaded.
    bool closeWindowsBeforeLoad();
    void saveProfile();
    void windowList();
    void renameTerm();
    void editSettings();
    // 'manual': requested by the user (always reports the result).
    // 'startup': first reading, only problems are reported.
    void reloadSettings(bool manual, bool startup = false);
    void about();

    // Current settings (tvterm.ini).
    static const Settings &settings();
    // A message from tvterm, shown in the Event Viewer (kept until one is
    // opened).
    static void note(const std::string &text);
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

    void checkSettingsFile();

};

inline TVTermDesk* TVTermApp::getDeskTop()
{
    return (TVTermDesk*) deskTop;
}

#endif // TVTERM_APP_H
