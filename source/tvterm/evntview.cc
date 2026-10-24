#define Uses_TEvent
#define Uses_TKeys
#define Uses_TFrame
#define Uses_TTerminal
#define Uses_TScrollBar
#include <tvision/tv.h>

#include "cmds.h"
#include "evntview.h"

#include <iomanip>
#include <ostream>
#include <sstream>
#include <string>

const char * const TEventViewer::titles[2] =
{
    "Event Viewer",
    "Event Viewer (Stopped)",
};

TEventViewer::TEventViewer(const TRect &bounds, ushort aBufSize) noexcept :
    TWindowInit(&initFrame),
    TWindow(bounds, nullptr, wnNoNumber)
{
    eventMask |= evBroadcast;
    title = titles[stopped];
    TScrollBar *scrollBar = standardScrollBar(sbVertical | sbHandleKeyboard);
    interior = new TTerminal(getExtent().grow(-1, -1), nullptr, scrollBar, aBufSize);
    insert(interior);
    out = new std::ostream(interior);
}

TEventViewer::~TEventViewer()
{
    title = nullptr; // 'title' points to a static string; don't let TWindow delete it.
}

void TEventViewer::shutDown()
{
    delete out;
    out = nullptr;
    interior = nullptr;
    TWindow::shutDown();
}

void TEventViewer::toggle()
{
    stopped = Boolean(!stopped);
    title = titles[stopped];
    if (frame)
        frame->drawView();
}

void TEventViewer::print(const TEvent &ev)
{
    if (ev.what != evNothing && !stopped && out)
    {
        lock();
        *out << "Received event #" << ++eventCount << '\n';
        printEvent(*out, ev);
        *out << std::flush;
        unlock();
    }
}

void TEventViewer::handleEvent(TEvent &ev)
{
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
