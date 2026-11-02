#define Uses_TStatusLine
#define Uses_TStatusDef
#define Uses_TStatusItem
#define Uses_TDrawBuffer
#define Uses_TProgram
#define Uses_TEvent
#include <tvision/tv.h>

#include "statline.h"

#include <chrono>
#include <vector>

namespace statline
{

static std::string message;
static std::string hint;
static std::chrono::steady_clock::time_point until;

// Commands of the status items that stand for a key (see keyCommand).
enum : ushort { cmFirstKey = 0xE000, maxKeys = 256 };

struct StatusKey
{
    ushort keyCode;
    ushort mods;
    char ch;
};

static std::vector<StatusKey> &keys()
{
    static std::vector<StatusKey> v;
    return v;
}

static std::string oneLine(const std::string &text)
{
    std::string out;
    for (char c : text)
        out += (c == '\n' || c == '\r' || c == '\t') ? ' ' : c;
    return out;
}

} // namespace statline

TVTermStatusLine::TVTermStatusLine(const TRect &bounds, TStatusDef &defs) noexcept :
    TStatusLine(bounds, defs)
{
}

void TVTermStatusLine::setCommandText(ushort command, const std::string &text)
{
    bool changed = false;
    for (TStatusDef *d = defs; d; d = d->next)
        for (TStatusItem *i = d->items; i; i = i->next)
            if (i->command == command && i->text && text != i->text)
            {
                delete[] i->text;
                i->text = newStr(text);
                changed = true;
            }
    if (changed)
        drawView();
}

void TVTermStatusLine::draw()
{
    const std::string &text = !statline::message.empty() ? statline::message : statline::hint;
    if (text.empty())
    {
        TStatusLine::draw();
        return;
    }
    TDrawBuffer b;
    TAttrPair color = getColor(0x0301);
    b.moveChar(0, ' ', color, size.x);
    b.moveStr(1, text, color, size.x - 1);
    writeLine(0, 0, size.x, 1, b);
}

void TVTermStatusLine::showMessage(const std::string &text, int seconds)
{
    statline::message = statline::oneLine(text);
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

void TVTermStatusLine::setHint(const std::string &text)
{
    std::string h = statline::oneLine(text);
    if (h == statline::hint)
        return;
    statline::hint = std::move(h);
    if (TProgram::statusLine)
        TProgram::statusLine->drawView();
}

ushort TVTermStatusLine::keyCommand(ushort keyCode, ushort mods, char ch)
{
    using namespace statline;
    auto &v = keys();
    for (size_t i = 0; i < v.size(); ++i)
        if (v[i].keyCode == keyCode && v[i].mods == mods && v[i].ch == ch)
            return (ushort) (cmFirstKey + i);
    if (v.size() >= maxKeys)
        return 0;
    v.push_back({keyCode, mods, ch});
    return (ushort) (cmFirstKey + v.size() - 1);
}

void TVTermStatusLine::handleEvent(TEvent &event)
{
    // While a message is shown, the items are hidden: a click only removes
    // the message (it must not run the hidden item under it).
    // The same for a hint (when the mouse moves are not reported, it stays
    // after a click on the top bar).
    if (event.what == evMouseDown && (!statline::message.empty() || !statline::hint.empty()))
    {
        statline::message.clear();
        statline::hint.clear();
        drawView();
        clearEvent(event);
        return;
    }
    TStatusLine::handleEvent(event);
}

void TVTermStatusLine::putEvent(TEvent &event)
// A click on an item that stands for a key: the key, instead of the command.
{
    using namespace statline;
    auto &v = keys();
    if ( event.what == evCommand && event.message.command >= cmFirstKey &&
         event.message.command < cmFirstKey + v.size() )
    {
        const StatusKey &k = v[event.message.command - cmFirstKey];
        TEvent key {};
        key.what = evKeyDown;
        key.keyDown.keyCode = k.keyCode;
        key.keyDown.controlKeyState = k.mods;
        if (k.ch)
        {
            key.keyDown.text[0] = k.ch;
            key.keyDown.textLength = 1;
        }
        TStatusLine::putEvent(key);
        return;
    }
    TStatusLine::putEvent(event);
}
