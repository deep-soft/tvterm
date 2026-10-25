#ifndef TVTERM_APP_H
#define TVTERM_APP_H

#define Uses_TApplication
#define Uses_TCommandSet
#include <tvision/tv.h>

#include <string>

class TVTermDesk;
struct Profile;

struct TVTermApp : public TApplication
{
    static TCommandSet tileCmds;

    // 'profileName': profile to open at startup (see profile.h), or null.
    TVTermApp(const char *profileName = nullptr);
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
    void loadProfile();
    void saveProfile();
    void windowList();
    void renameTerm();

    bool openProfile(const std::string &nameOrPath);
    void openProfile(const Profile &profile);

private:

    // Message to show once the application is running (e.g. profile errors
    // found at startup).
    std::string pendingMessage;

};

inline TVTermDesk* TVTermApp::getDeskTop()
{
    return (TVTermDesk*) deskTop;
}

#endif // TVTERM_APP_H
