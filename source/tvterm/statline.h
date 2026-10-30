#ifndef TVTERM_STATLINE_H
#define TVTERM_STATLINE_H

#define Uses_TStatusLine
#define Uses_TEvent
#include <tvision/tv.h>

#include <string>

// A status line that can show a short message for a few seconds, in place
// of its items (e.g. "Saved 120 lines to ...", "No older match").

class TVTermStatusLine : public TStatusLine
{
public:

    TVTermStatusLine(const TRect &bounds, TStatusDef &defs) noexcept;

    void draw() override;

    // Shows 'text' in the status line of the application.
    static void showMessage(const std::string &text, int seconds = 5);
    // Removes the message once its time is over. Called periodically.
    static void checkExpired();
    // True while a message is shown.
    static bool hasMessage();

    // A command for a status item that, when clicked, acts like pressing
    // 'keyCode' (with 'mods', and 'ch' for printable keys): the key goes to
    // the focused window, which handles it as usual. Used for the items of
    // windows that handle their own keys (Log View, Event Viewer, History).
    static ushort keyCommand(ushort keyCode, ushort mods = 0, char ch = 0);

    // Replaces the text of the items with 'command' that have a text
    // (e.g. "~Ctrl-B~ Menu" when the menu key changes).
    void setCommandText(ushort command, const std::string &text);

    void handleEvent(TEvent &event) override;
    void putEvent(TEvent &event) override;
};

#endif // TVTERM_STATLINE_H
