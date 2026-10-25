#define Uses_TStatusLine
#define Uses_TDrawBuffer
#define Uses_TProgram
#include <tvision/tv.h>

#include "statline.h"

#include <chrono>

namespace statline
{

static std::string message;
static std::chrono::steady_clock::time_point until;

} // namespace statline

TVTermStatusLine::TVTermStatusLine(const TRect &bounds, TStatusDef &defs) noexcept :
    TStatusLine(bounds, defs)
{
}

void TVTermStatusLine::draw()
{
    if (statline::message.empty())
    {
        TStatusLine::draw();
        return;
    }
    TDrawBuffer b;
    TAttrPair color = getColor(0x0301);
    b.moveChar(0, ' ', color, size.x);
    b.moveStr(1, statline::message, color, size.x - 1);
    writeLine(0, 0, size.x, 1, b);
}

void TVTermStatusLine::showMessage(const std::string &text, int seconds)
{
    statline::message.clear();
    // One line only.
    for (char c : text)
        statline::message += (c == '\n' || c == '\r' || c == '\t') ? ' ' : c;
    statline::until = std::chrono::steady_clock::now() + std::chrono::seconds(seconds);
    if (TProgram::statusLine)
        TProgram::statusLine->drawView();
}

void TVTermStatusLine::checkExpired()
{
    if (!statline::message.empty() && std::chrono::steady_clock::now() > statline::until)
    {
        statline::message.clear();
        if (TProgram::statusLine)
            TProgram::statusLine->drawView();
    }
}

bool TVTermStatusLine::hasMessage()
{
    return !statline::message.empty();
}
