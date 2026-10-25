#ifndef TVTERM_STATLINE_H
#define TVTERM_STATLINE_H

#define Uses_TStatusLine
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
};

#endif // TVTERM_STATLINE_H
