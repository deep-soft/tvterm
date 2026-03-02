#ifndef TVTERM_APP_H
#define TVTERM_APP_H

#define Uses_TApplication
#define Uses_TCommandSet
#include <tvision/tv.h>

class TVTermDesk;

struct TVTermApp : public TApplication
{
    static TCommandSet tileCmds;

    TVTermApp();
    static TStatusLine* initStatusLine(TRect r);
    static TDeskTop* initDeskTop(TRect r);

    TVTermDesk* getDeskTop();

    void handleEvent(TEvent &event) override;
    Boolean valid(ushort command) override;
    void idle() override;
    virtual void getEvent(TEvent& event); 

    size_t getOpenTermCount();

    // Command handlers

    void openMenu();
    void newTerm();
    void changeDir();
    void eventViewer();
    void printEvent(const TEvent &);

};

inline TVTermDesk* TVTermApp::getDeskTop()
{
    return (TVTermDesk*) deskTop;
}

#endif // TVTERM_APP_H
