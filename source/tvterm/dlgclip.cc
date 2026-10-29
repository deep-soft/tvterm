#define Uses_TProgram
#define Uses_TGroup
#define Uses_TKeys
#define Uses_TClipboard
#include <tvision/tv.h>

#include "dlgclip.h"
#include "dircompl.h"

#include <string.h>
#include <stdint.h>

namespace dlgclip
{

TInputLine *focusedInputLine() noexcept
{
    // Follow the chain of current views, from the application down to the
    // view that receives the keyboard (a modal dialog is current in the
    // group it was executed in).
    TView *v = TProgram::application;
    while (v)
    {
        TGroup *g = dynamic_cast<TGroup *>(v);
        if (!g || !g->current)
            break;
        v = g->current;
    }
    return dynamic_cast<TInputLine *>(v);
}

static size_t decodeUtf8(const char *s, size_t avail, uint32_t &cp) noexcept
// Length of the valid UTF-8 character at 's' (with its code point in 'cp'),
// or 0 if the bytes there are not valid UTF-8 (overlong forms and
// surrogates included).
{
    unsigned char c = s[0];
    size_t len;
    uint32_t min;
    if (c < 0x80)
        return cp = c, 1;
    else if (c >= 0xC2 && c <= 0xDF)
        len = 2, min = 0x80, cp = c & 0x1F;
    else if (c >= 0xE0 && c <= 0xEF)
        len = 3, min = 0x800, cp = c & 0x0F;
    else if (c >= 0xF0 && c <= 0xF4)
        len = 4, min = 0x10000, cp = c & 0x07;
    else
        return 0;
    if (len > avail)
        return 0;
    for (size_t i = 1; i < len; ++i)
    {
        unsigned char cc = s[i];
        if ((cc & 0xC0) != 0x80)
            return 0;
        cp = (cp << 6) | (cc & 0x3F);
    }
    if (cp < min || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF))
        return 0;
    return len;
}

static bool isSpace(uint32_t cp) noexcept
{
    return cp == ' ' || cp == '\t' || cp == '\n' || cp == '\r' ||
           cp == 0x2028 || cp == 0x2029; // Line and paragraph separators.
}

static bool isRemoved(uint32_t cp) noexcept
{
    return cp < 0x20 || (cp >= 0x7F && cp <= 0x9F) ||  // C0 (ESC, NUL...), DEL, C1.
           cp == 0x00AD ||                             // Soft hyphen.
           cp == 0x061C || cp == 0x200E || cp == 0x200F || // Bidi marks.
           (cp >= 0x202A && cp <= 0x202E) ||           // Bidi embeddings, overrides.
           (cp >= 0x2066 && cp <= 0x2069) ||           // Bidi isolates.
           (cp >= 0x200B && cp <= 0x200D) ||           // Zero-width space, (non-)joiner.
           (cp >= 0x2060 && cp <= 0x2064) ||           // Word joiner, invisible operators.
           cp == 0x180E || cp == 0xFEFF ||             // Mongolian vowel separator, BOM.
           cp == 0xFFFE || cp == 0xFFFF;               // Noncharacters.
}

std::string sanitize(TStringView text, size_t maxOutputBytes) noexcept
{
    std::string out;
    size_t n = text.size() < maxInputBytes ? text.size() : (size_t) maxInputBytes;
    bool space = false;
    for (size_t i = 0; i < n;)
    {
        uint32_t cp;
        size_t len = decodeUtf8(&text[i], n - i, cp);
        if (len == 0)
        {
            ++i; // Invalid byte: dropped.
            continue;
        }
        const char *chr = &text[i];
        i += len;
        if (isSpace(cp))
            space = true;
        else if (!isRemoved(cp))
        {
            size_t need = len + (space && !out.empty());
            if (out.size() + need > maxOutputBytes)
                break;
            if (space && !out.empty())
                out += ' ';
            space = false;
            out.append(chr, len);
        }
    }
    return out;
}

void insertText(TInputLine &line, const std::string &text) noexcept
{
    for (size_t i = 0; i < text.size();)
    {
        uint32_t cp;
        size_t len = decodeUtf8(&text[i], text.size() - i, cp);
        if (len == 0 || len > sizeof(KeyDownEvent::text))
            break;
        // The same event TInputLine gets for a typed or pasted character.
        TEvent ev {};
        ev.what = evKeyDown;
        ev.keyDown.keyCode = kbNoKey;
        ev.keyDown.controlKeyState = kbPaste;
        ev.keyDown.charScan.charCode = len == 1 ? text[i] : 0;
        memcpy(ev.keyDown.text, &text[i], len);
        ev.keyDown.textLength = (uchar) len;
        int curPos = line.curPos;
        line.handleEvent(ev);
        if (line.curPos == curPos)
            break; // It did not fit (length, width or validator).
        i += len;
    }
}

static std::string readPaste(TApplication &app, TEvent &ev) noexcept
// Collects the text of consecutive paste events, starting with 'ev', and
// gives back the first event that is not part of the paste.
{
    std::string text;
    int saveTimeout = TProgram::eventTimeoutMs;
    TProgram::eventTimeoutMs = 0;
    do {
        if (text.size() < maxInputBytes)
        {
            if (ev.keyDown.textLength)
                text.append(ev.keyDown.text, ev.keyDown.textLength);
            else if (ev.keyDown.keyCode == kbEnter)
                text += '\n';
            else if (ev.keyDown.keyCode == kbTab)
                text += '\t';
        }
        // Not this->getEvent(): the application's own getEvent calls us.
        ev.what = evNothing;
        app.TApplication::getEvent(ev);
    } while (ev.what == evKeyDown && (ev.keyDown.controlKeyState & kbPaste));
    TProgram::eventTimeoutMs = saveTimeout;
    if (ev.what != evNothing)
        app.putEvent(ev);
    return text;
}

bool handleKeyEvent(TApplication &app, TEvent &ev) noexcept
{
    if (ev.what != evKeyDown)
        return false;
    TInputLine *line = focusedInputLine();
    if (!line)
        return false;
    if (ev.keyDown.controlKeyState & kbPaste)
    {
        std::string text = readPaste(app, ev);
        // 'line' is still focused: readPaste does not dispatch events.
        insertText(*line, sanitize(text, line->maxLen > 0 ? line->maxLen : 0));
        ev.what = evNothing;
        // The dialog may react to the new text (directory completion).
        if (line->owner)
            message(line->owner, evBroadcast, dircompl::cmInputLinePasted, line);
        return true;
    }
    switch (ev.keyDown.keyCode)
    {
        case kbShiftIns:
        case kbCtrlV:
            // The text comes back as paste events (see above).
            TClipboard::requestText();
            ev.what = evNothing;
            return true;
        case kbCtrlIns:
        case kbCtrlC:
            if (line->selStart < line->selEnd)
            {
                TClipboard::setText(TStringView(line->data + line->selStart,
                                                line->selEnd - line->selStart));
                ev.what = evNothing;
                return true;
            }
            break;
    }
    return false;
}

} // namespace dlgclip
