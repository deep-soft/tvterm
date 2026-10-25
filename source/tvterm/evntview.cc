#define Uses_TEvent
#define Uses_TKeys
#define Uses_TFrame
#define Uses_TTerminal
#define Uses_TScrollBar
#define Uses_TProgram
#define Uses_TApplication
#include <tvision/tv.h>

#include "cmds.h"
#include "evntview.h"

#include <iomanip>
#include <ostream>
#include <sstream>
#include <string>
#include <string.h>
#include <utility>

// A TTerminal whose buffer can be larger than the 32000 bytes Turbo Vision
// allows (its positions are 16-bit, so up to 65535), and can be cleared.
class EventTerminal : public TTerminal
{
public:

    EventTerminal(const TRect &bounds, TScrollBar *vScrollBar, ushort aBufSize) noexcept :
        TTerminal(bounds, nullptr, vScrollBar, 1024)
    {
        if (aBufSize > bufSize)
        {
            delete[] buffer;
            bufSize = aBufSize;
            buffer = new char[bufSize];
        }
    }

    void clear()
    {
        queFront = queBack = 0;
        setLimit(0, 1);
        scrollTo(0, 0);
        drawView();
    }
};

TEventViewer::TEventViewer(const TRect &bounds, ushort aBufSize, const EventFilter &aFilter) noexcept :
    TWindowInit(&initFrame),
    TWindow(bounds, nullptr, wnNoNumber),
    filter(aFilter)
{
    eventMask |= evBroadcast;
    helpCtx = hcEventViewer;
    updateTitle();
    TScrollBar *scrollBar = standardScrollBar(sbVertical | sbHandleKeyboard);
    interior = new EventTerminal(getExtent().grow(-1, -1), scrollBar, aBufSize);
    insert(interior);
    out = new std::ostream(interior);
}

void TEventViewer::shutDown()
{
    delete out;
    out = nullptr;
    interior = nullptr;
    TWindow::shutDown();
}

void TEventViewer::updateTitle()
// "Event Viewer (Stopped) [M- C+ K+ O+ T+]"
{
    std::string text = stopped ? "Event Viewer (Stopped) [" : "Event Viewer [";
    const std::pair<char, bool> flags[] = {
        {'M', filter.mouseMove}, {'C', filter.mouseClicks}, {'K', filter.keys},
        {'O', filter.other}, {'T', filter.messages},
    };
    for (auto &f : flags)
    {
        if (text.back() != '[')
            text += ' ';
        text += f.first;
        text += f.second ? '+' : '-';
    }
    text += ']';
    delete[] (char *) title;
    title = newStr(text.c_str());
    if (frame)
        frame->drawView();
}

void TEventViewer::toggle()
{
    stopped = Boolean(!stopped);
    updateTitle();
}

namespace evntview
{

static bool shown(const EventFilter &filter, const TEvent &ev)
{
    if (ev.what == evMouseMove)
        return filter.mouseMove;
    if (ev.what & evMouse)
        return filter.mouseClicks;
    if (ev.what & evKeyboard)
        return filter.keys;
    return filter.other;
}

static char ownKey(const TEvent &ev)
// The letter of a filter key (lowercase), or 0.
{
    if ( ev.what != evKeyDown ||
         (ev.keyDown.controlKeyState & (kbCtrlShift | kbAltShift)) )
        return 0;
    char c = ev.keyDown.charScan.charCode;
    if (c >= 'A' && c <= 'Z')
        c += 'a' - 'A';
    return strchr("mckotnx", c) && c ? c : 0;
}

} // namespace evntview

bool TEventViewer::isOwnKey(const TEvent &ev)
{
    return (state & sfFocused) && evntview::ownKey(ev);
}

void TEventViewer::print(const TEvent &ev)
{
    if (ev.what == evNothing || stopped || !out)
        return;
    // Counted even when hidden, so that gaps in the numbers show them.
    ++eventCount;
    if (!evntview::shown(filter, ev) || isOwnKey(ev))
        return;
    lock();
    *out << "Received event #" << eventCount << '\n';
    printEvent(*out, ev);
    *out << std::flush;
    unlock();
}

void TEventViewer::printNote(const std::string &text, bool always)
{
    if (out && (always || filter.messages))
    {
        lock();
        *out << "tvterm: " << text;
        if (text.empty() || text.back() != '\n')
            *out << '\n';
        *out << std::flush;
        unlock();
    }
}

void TEventViewer::clear()
{
    if (interior)
        interior->clear();
}

void TEventViewer::handleEvent(TEvent &ev)
{
    if (char c = (state & sfFocused) ? evntview::ownKey(ev) : 0)
    {
        switch (c)
        {
            case 'm': filter.mouseMove = !filter.mouseMove; break;
            case 'c': filter.mouseClicks = !filter.mouseClicks; break;
            case 'k': filter.keys = !filter.keys; break;
            case 'o': filter.other = !filter.other; break;
            case 't': filter.messages = !filter.messages; break;
            case 'x': clear(); break;
            case 'n':
                // The application keeps the messages (see TVTermApp::note).
                clear();
                message(TProgram::application, evCommand, cmShowNotes, this);
                break;
        }
        updateTitle();
        clearEvent(ev);
        return;
    }
    TWindow::handleEvent(ev);
    // Answer to "is there an Event Viewer?" broadcasts (see TVTermApp).
    if (ev.what == evBroadcast && ev.message.command == cmFindEventViewer)
        clearEvent(ev);
}

static void printConstants( std::ostream &out, ushort value,
                            void (&doPrint)(std::ostream &, ushort) )
{
    out << std::hex << std::setfill('0') << "0x" << std::setw(4) << value;
    std::ostringstream os;
    doPrint(os, value);
    std::string s = os.str();
    if (!s.empty() && s[0] != '0')
        out << " (" << s << ")";
    out << std::dec;
}

void TEventViewer::printEvent(std::ostream &out, const TEvent &ev)
{
    out << "TEvent {\n"
        << "  .what = ";
    printConstants(out, ev.what, printEventCode);
    out << ",\n";
    if (ev.what & evMouse)
    {
        out << "  .mouse = MouseEventType {\n"
            << "    .where = TPoint {\n"
            << "      .x = " << ev.mouse.where.x << "\n"
            << "      .y = " << ev.mouse.where.y << "\n"
            << "    },\n"
            << "    .eventFlags = ";
        printConstants(out, ev.mouse.eventFlags, printMouseEventFlags);
        out << ",\n"
            << "    .controlKeyState = ";
        printConstants(out, ev.mouse.controlKeyState, printControlKeyState);
        out << ",\n"
            << "    .buttons = ";
        printConstants(out, ev.mouse.buttons, printMouseButtonState);
        out << ",\n"
            << "    .wheel = ";
        printConstants(out, ev.mouse.wheel, printMouseWheelState);
        out << "\n"
            << "  }\n";
    }
    if (ev.what & evKeyboard)
    {
        char charCode = ev.keyDown.charScan.charCode;
        out << "  .keyDown = KeyDownEvent {\n"
            << "    .keyCode = ";
        printConstants(out, ev.keyDown.keyCode, printKeyCode);
        out << ",\n"
            << "    .charScan = CharScanType {\n"
            << "      .charCode = " << (int) (uchar) charCode;
        if (charCode)
            out << " ('" << charCode << "')";
        out << ",\n"
            << "      .scanCode = " << (int) (uchar) ev.keyDown.charScan.scanCode << "\n"
            << "    },\n"
            << "    .controlKeyState = ";
        printConstants(out, ev.keyDown.controlKeyState, printControlKeyState);
        out << ",\n"
            << std::hex
            << "    .text = {";
        for (int i = 0; i < ev.keyDown.textLength; ++i)
        {
            if (i > 0)
                out << ", ";
            out << "0x" << (int) (uchar) ev.keyDown.text[i];
        }
        out << "},\n"
            << std::dec
            << "    .textLength = " << (int) ev.keyDown.textLength << "\n"
            << "  }\n";
    }
    if (ev.what & evCommand)
        out << "  .message = MessageEvent {\n"
            << "    .command = " << ev.message.command << ",\n"
            << "    .infoPtr = " << ev.message.infoPtr << "\n"
            << "  }\n";
    out << "}\n";
}
