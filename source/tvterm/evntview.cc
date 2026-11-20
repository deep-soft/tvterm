#define Uses_TEvent
#define Uses_TKeys
#define Uses_TFrame
#define Uses_TTextDevice
#define Uses_TDrawBuffer
#define Uses_TClipboard
#define Uses_TScrollBar
#define Uses_TProgram
#define Uses_TApplication
#define Uses_TMenuItem
#define Uses_TSubMenu
#define Uses_TMenu
#include <tvision/tv.h>

#include "cmds.h"
#include "evntview.h"
#include "apputil.h"
#include "app.h"
#include "settings.h"
#include "statline.h"
#include "logview.h"
#include "alerts.h"
#include "logsrc.h"

#include <algorithm>
#include <deque>
#include <iomanip>
#include <ostream>
#include <sstream>
#include <string>
#include <string.h>
#include <utility>

// The text of the Event Viewer (E81: was a TTerminal): lines kept up to
// 'bufSize' bytes (the oldest dropped), a cursor line, a selection of whole
// lines and copy, with the keys and the behavior of Log View:
// Up/Down PgUp/PgDn Home/End move the cursor line (Shift: select), a
// click / drag selects, Shift+click extends, Ctrl-C / Ctrl-Ins copy the
// selection (or the cursor line), Esc clears the selection. With the
// cursor on the last line and no selection, new lines are followed; a
// selection stops the scrolling until Esc (then the end is followed again).
// Lines are numbered from the first line ever written ('base' is the
// number of lines[0]), so the cursor and the selection stay on their text
// when old lines are dropped. Copy takes the text exactly as shown.
class EventTerminal : public TTextDevice
{
public:

    EventTerminal(const TRect &bounds, TScrollBar *vScrollBar, size_t aBufSize) noexcept :
        TTextDevice(bounds, nullptr, vScrollBar),
        bufSize(aBufSize)
    {
        growMode = gfGrowHiX + gfGrowHiY;
        eventMask |= evMouseWheel;
        lines.push_back(std::string());
    }

    int do_sputn(const char *s, int count) override
    {
        for (int i = 0; i < count; ++i)
        {
            if (s[i] == '\n')
            {
                lines.push_back(std::string());
                ++bytes;
            }
            else if (s[i] != '\r')
            {
                lines.back() += s[i];
                ++bytes;
            }
        }
        while (bytes > bufSize && lines.size() > 1)
        {
            bytes -= lines.front().size() + 1;
            lines.pop_front();
            ++base;
            if (delta.y > 0)
                --delta.y;
        }
        changed();
        return count;
    }

    void clear()
    {
        lines.clear();
        lines.push_back(std::string());
        base += 1000000; // Old numbers never match new lines.
        bytes = 0;
        hasSel = false;
        follow = true;
        changed();
    }

    void draw() override
    {
        TColorAttr normal = getColor(1);
        bool focused = owner && (owner->state & sfFocused);
        uint64_t cur = cursorNo();
        for (int y = 0; y < size.y; ++y)
        {
            TDrawBuffer b;
            b.moveChar(0, ' ', normal, size.x);
            size_t i = (size_t) (delta.y + y);
            if (i < shownCount())
            {
                uint64_t no = base + i;
                TColorAttr attr = focused && no == cur ? TColorAttr(TVTermApp::settings().cursorColor) :
                                  isSelected(no) ? TColorAttr(TVTermApp::settings().selectionColor) : normal;
                if (attr != normal)
                    b.moveChar(0, ' ', attr, size.x);
                b.moveStr(0, lines[i], attr, size.x, delta.x);
            }
            writeLine(0, y, size.x, 1, b);
        }
    }

    // A key of the cursor and the selection (E81); true if it was one.
    bool handleKey(TEvent &ev)
    {
        if (ev.what != evKeyDown)
            return false;
        TKey key(ev.keyDown);
        bool shift = (key.mods & kbShift) != 0;
        TKey plain(key.code, key.mods & ~kbShift);
        long n = (long) shownCount();
        long cur = (long) (cursorNo() - base);
        long page = std::max(1, size.y - 1);
        if (plain == TKey(kbUp)) moveCursor(cur - 1, shift);
        else if (plain == TKey(kbDown)) moveCursor(cur + 1, shift);
        else if (plain == TKey(kbPgUp)) moveCursor(cur - page, shift);
        else if (plain == TKey(kbPgDn)) moveCursor(cur + page, shift);
        else if (plain == TKey(kbHome) || plain == TKey(kbCtrlHome)) moveCursor(0, shift);
        else if (plain == TKey(kbEnd) || plain == TKey(kbCtrlEnd))
        {
            moveCursor(n - 1, shift);
            if (!shift)
                follow = true;
        }
        else if (key == TKey(kbCtrlC) || key == TKey(kbCtrlIns)) copy();
        else if (key == TKey(kbEsc))
        {
            if (!hasSel)
                return false;
            // The scrolling stopped by the selection goes on: to the end.
            hasSel = false;
            follow = true;
            changed();
        }
        else
            return false;
        clearEvent(ev);
        return true;
    }

    // Whether 'ev' would be handled by handleKey (not shown as an event).
    static bool isKey(const TEvent &ev)
    {
        if (ev.what != evKeyDown)
            return false;
        TKey key(ev.keyDown);
        TKey plain(key.code, key.mods & ~kbShift);
        for (ushort code : {kbUp, kbDown, kbPgUp, kbPgDn, kbHome, kbEnd, kbCtrlHome, kbCtrlEnd})
            if (plain == TKey(code))
                return true;
        return key == TKey(kbCtrlC) || key == TKey(kbCtrlIns) || key == TKey(kbEsc);
    }

    void handleEvent(TEvent &ev) override
    {
        TTextDevice::handleEvent(ev);
        if (ev.what == evMouseWheel)
        {
            int step = ev.mouse.wheel == mwUp ? -3 : ev.mouse.wheel == mwDown ? 3 : 0;
            if (step)
            {
                scrollTo(delta.x, std::max(0, std::min(delta.y + step, maxTop())));
                // At the bottom, following starts again (no selection).
                if (!hasSel && delta.y >= maxTop())
                {
                    follow = true;
                    changed();
                }
                else
                    follow = false;
            }
            clearEvent(ev);
        }
        else if (ev.what == evMouseDown && (ev.mouse.buttons & mbLeftButton))
        {
            TPoint p = makeLocal(ev.mouse.where);
            long i = delta.y + p.y;
            if (i < 0 || i >= (long) shownCount())
            {
                clearEvent(ev);
                return;
            }
            uint64_t no = base + i;
            if ((ev.mouse.controlKeyState & kbShift) && !hasSel)
                selAnchor = cursorNo();
            else if (!(ev.mouse.controlKeyState & kbShift))
                selAnchor = no;
            hasSel = true;
            selCursor = cursor = no;
            follow = false;
            drawView();
            while (mouseEvent(ev, evMouseMove | evMouseAuto))
            {
                p = makeLocal(ev.mouse.where);
                if (p.y < 0)
                    scrollTo(delta.x, std::max(0, delta.y - 1)), p.y = 0;
                else if (p.y >= size.y)
                    scrollTo(delta.x, std::min(maxTop(), delta.y + 1)), p.y = size.y - 1;
                long j = std::min((long) shownCount() - 1, (long) delta.y + p.y);
                selCursor = cursor = base + j;
                drawView();
            }
            clearEvent(ev);
        }
    }

private:

    size_t bufSize;
    std::deque<std::string> lines; // The last one: the line being written.
    size_t bytes {0};
    uint64_t base {0};
    uint64_t cursor {0};
    bool follow {true};
    bool hasSel {false};
    uint64_t selAnchor {0}, selCursor {0};

    // The lines shown: the last one only if something is written in it.
    size_t shownCount() const
    {
        return lines.size() - (lines.size() > 1 && lines.back().empty() ? 1 : 0);
    }

    uint64_t cursorNo() const
    {
        uint64_t last = base + shownCount() - 1;
        if (follow && !hasSel)
            return last;
        return std::max(base, std::min(cursor, last));
    }

    int maxTop() const
    {
        return std::max(0, (int) shownCount() - size.y);
    }

    bool isSelected(uint64_t no) const
    {
        if (!hasSel)
            return false;
        uint64_t a = std::min(selAnchor, selCursor), b = std::max(selAnchor, selCursor);
        return no >= a && no <= b;
    }

    void moveCursor(long index, bool extend)
    {
        long n = (long) shownCount();
        index = std::max(0L, std::min(index, n - 1));
        uint64_t from = cursorNo();
        if (extend)
        {
            if (!hasSel)
            {
                hasSel = true;
                selAnchor = from;
            }
            selCursor = base + index;
        }
        else
            hasSel = false;
        cursor = base + index;
        follow = !hasSel && index == n - 1;
        // The cursor line on the screen.
        if (index < delta.y)
            scrollTo(delta.x, (int) index);
        else if (index >= delta.y + size.y)
            scrollTo(delta.x, (int) index - size.y + 1);
        drawView();
    }

    void copy()
    {
        std::string text;
        size_t count = 0;
        uint64_t a = cursorNo(), b = a;
        if (hasSel)
            a = std::min(selAnchor, selCursor), b = std::max(selAnchor, selCursor);
        for (uint64_t no = std::max(a, base); no <= b && no - base < shownCount(); ++no)
        {
            text += lines[no - base] + "\n";
            ++count;
        }
        if (count == 0)
        {
            TVTermStatusLine::showMessage("Nothing to copy: the window is empty.");
            return;
        }
        // Without the last line break (pasting into a shell would run it).
        text.pop_back();
        TClipboard::setText(text);
        TVTermStatusLine::showMessage("Copied " + std::to_string(count) + " line(s) to the clipboard.");
    }

    // After new text, a clear, Esc: the limits, and the end when following
    // (not while a selection exists).
    void changed()
    {
        setLimit(1, (int) shownCount());
        if (follow && !hasSel)
            scrollTo(0, maxTop());
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
    // The keys are the viewer's own (E81): the scroll bar takes none.
    TScrollBar *scrollBar = standardScrollBar(sbVertical);
    interior = new EventTerminal(getExtent().grow(-1, -1), scrollBar, aBufSize);
    insert(interior);
    out = new std::ostream(interior);
}

void TEventViewer::setState(ushort aState, Boolean enable)
{
    TWindow::setState(aState, enable);
    // The cursor line is shown only in the focused window.
    if ((aState & (sfFocused | sfSelected | sfActive)) && interior)
        interior->drawView();
}

void TEventViewer::shutDown()
{
    delete out;
    out = nullptr;
    interior = nullptr;
    TWindow::shutDown();
}

void TEventViewer::updateTitle()
// "Event Viewer (Stopped) [M- C+ K+ O+ T+] A: alerts"
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
    text += "] A: alerts";
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

// Ctrl-A (Go to Next Alert), F8 (Clear non-urgent alerts, E69),
// key-logviews-end (Ctrl-L, E78) and F9 (the window's menu, E78), as
// returned by ownKey.
// key-eventviewer-mute (Ctrl-M, E106): Mute alerts above level...
enum : char { keyNextAlert = 1, keyClearLowAlerts = 2, keyLogViewsEnd = 3, keyMenu = 4, keyClearNotes = 5,
              keyMute = 6 };

static char ownKey(const TEvent &ev)
// The letter of a filter key (lowercase), ' ' (Space: pause / resume),
// keyNextAlert (Ctrl-A), keyClearLowAlerts (F8), or 0.
{
    if (ev.what != evKeyDown)
        return 0;
    if (TKey(ev.keyDown) == TKey(kbCtrlA))
        return keyNextAlert;
    if (TKey(ev.keyDown) == TKey(kbF8))
        return keyClearLowAlerts;
    if (TKey(ev.keyDown) == TKey(kbF9))
        return keyMenu;
    TKey toEnd = TVTermApp::settings().keyLogViewsEnd;
    if (toEnd.code != 0 && TKey(ev.keyDown) == toEnd)
        return keyLogViewsEnd;
    // E106: Ctrl-M is 0x000d (far2l's extensions); Enter (0x1c0d) is
    // another TKey, so Enter never mutes.
    TKey mute = TVTermApp::settings().keyEventViewerMute;
    if (mute.code != 0 && TKey(ev.keyDown) == mute)
        return keyMute;
    if (ev.keyDown.controlKeyState & (kbCtrlShift | kbAltShift))
        return 0;
    char c = ev.keyDown.charScan.charCode;
    // Shift-N (E88): empty the kept messages; with Caps Lock on, a plain N
    // is n.
    if (c == 'N' && !(ev.keyDown.controlKeyState & kbCapsState))
        return keyClearNotes;
    if (c >= 'A' && c <= 'Z')
        c += 'a' - 'A';
    return strchr("mckotnaxls ", c) && c ? c : 0;
}

} // namespace evntview

bool TEventViewer::isOwnKey(const TEvent &ev)
{
    return (state & sfFocused) && (evntview::ownKey(ev) || EventTerminal::isKey(ev));
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
        // 't:' (E98; was 'tvterm:'): short, and the letter of the T filter.
        *out << "t: " << text;
        if (text.empty() || text.back() != '\n')
            *out << '\n';
        *out << std::flush;
        unlock();
    }
}

void TEventViewer::printText(const std::string &text)
{
    if (out)
    {
        lock();
        *out << text;
        if (text.empty() || text.back() != '\n')
            *out << '\n';
        *out << std::flush;
        unlock();
    }
}

void TEventViewer::openMenu(TPoint where)
{
    std::string toEnd = keyName(TVTermApp::settings().keyLogViewsEnd);
    std::string mute = keyName(TVTermApp::settings().keyEventViewerMute);
    TMenuItem &items =
        *new TMenuItem("Scroll Log Views to las~t~", cmLogViewsToEnd, kbNoKey, hcNoContext,
                       TVTermApp::settings().keyLogViewsEnd.code ? toEnd.c_str() : nullptr) +
        // E106 (item 50): as More... > Mute alerts above level...
        *new TMenuItem("~M~ute alerts above level...", cmMuteAlerts, kbNoKey, hcNoContext,
                       TVTermApp::settings().keyEventViewerMute.code ? mute.c_str() : nullptr) +
        newLine() +
        *new TMenuItem("~N~ext Desk", cmNextDesk, kbNoKey, hcNoContext, "Ctrl-Right") +
        *new TMenuItem("~P~revious Desk", cmPrevDesk, kbNoKey, hcNoContext, "Ctrl-Left") +
        newLine() +
        *new TMenuItem("~L~og View commands", cmLogViewCommands, kbNoKey, hcNoContext, "L") +
        *new TMenuItem("Log ~s~ource history", cmLogSourceHistory, kbNoKey, hcNoContext, "S") +
        *new TMenuItem("~E~mpty kept messages", cmClearNotes, kbNoKey, hcNoContext, "Shift-N");
    if (ushort cmd = popupMenu(where, items, nullptr))
    {
        if (cmd == cmLogViewCommands)
            printLogViewCommands();
        else if (cmd == cmLogSourceHistory)
            printLogSourceHistory();
        else
            message(TProgram::application, evCommand, cmd, this);
    }
}

void TEventViewer::printLogViewCommands()
// One block of lines, with the time, shown even while stopped or with T-
// (like the alert list); a new block each time.
{
    if (TProgram::deskTop)
        printText(formatEventTime(time(nullptr)) + " " + logViewCommandsText(*TProgram::deskTop));
}

void TEventViewer::printLogSourceHistory()
// Like L: one block, shown even while stopped or with T-; nothing is
// emptied (Shift-N leaves the history).
{
    printText(formatEventTime(time(nullptr)) + " " + LogSource::historyText());
}

void TEventViewer::clear()
{
    if (interior)
        interior->clear();
}

void TEventViewer::handleEvent(TEvent &ev)
{
    // Ctrl-W: closes the window (see apputil.h).
    closeKeyToCommand(*this, ev);
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
            case 'l': printLogViewCommands(); break;
            case 's': printLogSourceHistory(); break;
            case 'a':
                // The application knows the windows (see listAlerts).
                message(TProgram::application, evCommand, cmShowAlerts, this);
                break;
            case 'n':
                // The application keeps the messages (see TVTermApp::note).
                clear();
                message(TProgram::application, evCommand, cmShowNotes, this);
                break;
            case ' ': toggle(); break; // Like Ctrl-B V.
            case evntview::keyNextAlert:
                // Like Ctrl-B G (see TVTermApp::nextAlert).
                message(TProgram::application, evCommand, cmNextAlert, this);
                break;
            case evntview::keyClearLowAlerts:
                // Like More... > Clear non-urgent alerts (levels 2-5).
                message(TProgram::application, evCommand, cmClearLowAlerts, this);
                break;
            case evntview::keyLogViewsEnd:
                // Like More... > Scroll Log Views to last.
                message(TProgram::application, evCommand, cmLogViewsToEnd, this);
                break;
            case evntview::keyClearNotes:
                message(TProgram::application, evCommand, cmClearNotes, this);
                break;
            case evntview::keyMute:
                // Like More... > Mute alerts above level... (a dialog: the
                // title is updated after it).
                message(TProgram::application, evCommand, cmMuteAlerts, this);
                break;
            case evntview::keyMenu:
                openMenu(interior ? interior->makeGlobal(TPoint {0, 0}) : origin);
                break;
        }
        updateTitle();
        clearEvent(ev);
        return;
    }
    // The cursor line, the selection and copy (E81).
    if ((state & sfFocused) && interior && interior->handleKey(ev))
        return;
    // Ctrl-Right / Ctrl-Left (E77): the next / previous Desk with windows;
    // the viewer keeps the focus. Not own keys: shown with K+.
    if ( ev.what == evKeyDown && (state & sfFocused) &&
         (TKey(ev.keyDown) == TKey(kbCtrlRight) || TKey(ev.keyDown) == TKey(kbCtrlLeft)) )
    {
        message( TProgram::application, evCommand,
                 TKey(ev.keyDown) == TKey(kbCtrlRight) ? cmNextDesk : cmPrevDesk, this );
        clearEvent(ev);
        return;
    }
    // A right click: the window's menu (E78).
    if ( ev.what == evMouseDown && (ev.mouse.buttons & mbRightButton) &&
         interior && interior->mouseInView(ev.mouse.where) )
    {
        clearEvent(ev);
        openMenu(ev.mouse.where);
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
