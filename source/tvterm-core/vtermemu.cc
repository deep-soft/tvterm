#define Uses_TText
#define Uses_TKeys
#define Uses_TEvent
#define Uses_TClipboard
#include <tvision/tv.h>

#include "util.h"
#include <tvterm/vtermemu.h>
#include <tvterm/termopts.h>
#include "far2lext.h"
#include <tvterm/debug.h>
#include <unordered_map>
#include <string>

namespace tvterm
{

const VTermScreenCallbacks VTermEmulator::callbacks =
{
    _static_wrap(&VTermEmulator::damage),
    _static_wrap(&VTermEmulator::moverect),
    _static_wrap(&VTermEmulator::movecursor),
    _static_wrap(&VTermEmulator::settermprop),
    _static_wrap(&VTermEmulator::bell),
    _static_wrap(&VTermEmulator::resize),
    /* sb_pushline */ nullptr,
    /* sb_popline */ nullptr,
    /* sb_clear */ nullptr,
    _static_wrap(&VTermEmulator::sb_pushline4),
};

const VTermSelectionCallbacks VTermEmulator::selectionCallbacks =
{
    _static_wrap(&VTermEmulator::selection_set),
    _static_wrap(&VTermEmulator::selection_query),
};

const VTermStateFallbacks VTermEmulator::stateFallbacks =
{
    /* control */ nullptr,
    /* csi */ nullptr,
    /* osc */ nullptr,
    /* dcs */ nullptr,
    _static_wrap(&VTermEmulator::apc),
    /* pm */ nullptr,
    /* sos */ nullptr,
};

namespace vtermemu
{

    // Input conversion.

    static const std::unordered_map<ushort, ushort> keys =
    {
        { kbEnter,          VTERM_KEY_ENTER             },
        { kbTab,            VTERM_KEY_TAB               },
        { kbBack,           VTERM_KEY_BACKSPACE         },
        { kbEsc,            VTERM_KEY_ESCAPE            },
        { kbUp,             VTERM_KEY_UP                },
        { kbDown,           VTERM_KEY_DOWN              },
        { kbLeft,           VTERM_KEY_LEFT              },
        { kbRight,          VTERM_KEY_RIGHT             },
        { kbIns,            VTERM_KEY_INS               },
        { kbDel,            VTERM_KEY_DEL               },
        { kbHome,           VTERM_KEY_HOME              },
        { kbEnd,            VTERM_KEY_END               },
        { kbPgUp,           VTERM_KEY_PAGEUP            },
        { kbPgDn,           VTERM_KEY_PAGEDOWN          },
        { kbF1,             VTERM_KEY_FUNCTION(1)       },
        { kbF2,             VTERM_KEY_FUNCTION(2)       },
        { kbF3,             VTERM_KEY_FUNCTION(3)       },
        { kbF4,             VTERM_KEY_FUNCTION(4)       },
        { kbF5,             VTERM_KEY_FUNCTION(5)       },
        { kbF6,             VTERM_KEY_FUNCTION(6)       },
        { kbF7,             VTERM_KEY_FUNCTION(7)       },
        { kbF8,             VTERM_KEY_FUNCTION(8)       },
        { kbF9,             VTERM_KEY_FUNCTION(9)       },
        { kbF10,            VTERM_KEY_FUNCTION(10)      },
        { kbF11,            VTERM_KEY_FUNCTION(11)      },
        { kbF12,            VTERM_KEY_FUNCTION(12)      },
    };

    static constexpr struct { ushort tv; VTermModifier vt; } modifiers[] =
    {
        { kbShift,      VTERM_MOD_SHIFT },
        { kbLeftAlt,    VTERM_MOD_ALT   },
        { kbCtrlShift,  VTERM_MOD_CTRL  },
    };

    static VTermKey convKey(ushort keyCode)
    {
        auto it = keys.find(keyCode);
        if (it != keys.end())
            return VTermKey(it->second);
        return VTERM_KEY_NONE;
    }

    static VTermModifier convMod(ushort controlKeyState)
    {
        VTermModifier mod = VTERM_MOD_NONE;
        for (const auto &m : modifiers)
            if (controlKeyState & m.tv)
                mod = VTermModifier(mod | m.vt);
        return mod;
    }

    static void convMouse( const MouseEventType &mouse,
                           VTermModifier &mod, int &button )
    {
        mod = convMod(mouse.controlKeyState);
        button =    (mouse.buttons & mbLeftButton)   ? 1 :
                    (mouse.buttons & mbMiddleButton) ? 2 :
                    (mouse.buttons & mbRightButton)  ? 3 :
                    (mouse.wheel & mwUp)             ? 4 :
                    (mouse.wheel & mwDown)           ? 5 :
                                                       0 ;
    }

#ifdef HAVE_VTERM_WIN32INPUT
    // win32-input-mode (DEC private mode 9001, used by far2l).
    // Reverse Mapping: Turbo Vision -> Win32
    static uint tvToWin32ControlState(ushort controlKeyState)
    {
        uint cs = 0;
        if (controlKeyState & kbShift)      cs |= 0x0010; // SHIFT_PRESSED
        if (controlKeyState & kbLeftCtrl)   cs |= 0x0008; // LEFT_CTRL_PRESSED
        if (controlKeyState & kbRightCtrl)  cs |= 0x0004; // RIGHT_CTRL_PRESSED
        if (controlKeyState & kbLeftAlt)    cs |= 0x0002; // LEFT_ALT_PRESSED
        if (controlKeyState & kbRightAlt)   cs |= 0x0001; // RIGHT_ALT_PRESSED
        if (controlKeyState & kbScrollState) cs |= 0x0040; // SCROLLLOCK_ON
        if (controlKeyState & kbNumState)    cs |= 0x0020; // NUMLOCK_ON
        if (controlKeyState & kbCapsState)   cs |= 0x0080; // CAPSLOCK_ON
        if (controlKeyState & kbEnhanced)    cs |= 0x0100; // ENHANCED_KEY
        return cs;
    }

    static ushort tvToWin32KeyCode(ushort keyCode)
    {
        // Handle Alphanumeric
        if (keyCode >= 'a' && keyCode <= 'z') return keyCode - 'a' + 'A';
        if (keyCode >= 'A' && keyCode <= 'Z') return keyCode;
        if (keyCode >= '0' && keyCode <= '9') return keyCode;

        // Handle Special Keys
        switch (keyCode)
        {
            case kbUp:    return 0x26; // VK_UP
            case kbDown:  return 0x28; // VK_DOWN
            case kbLeft:  return 0x25; // VK_LEFT
            case kbRight: return 0x27; // VK_RIGHT
            case kbHome:  return 0x24; // VK_HOME
            case kbEnd:   return 0x23; // VK_END
            case kbPgUp:  return 0x21; // VK_PRIOR
            case kbPgDn:  return 0x22; // VK_NEXT
            case kbIns:   return 0x2D; // VK_INSERT
            case kbDel:   return 0x2E; // VK_DELETE
            case kbF1:    return 0x70; // VK_F1
            case kbF2:    return 0x71;
            case kbF3:    return 0x72;
            case kbF4:    return 0x73;
            case kbF5:    return 0x74;
            case kbF6:    return 0x75;
            case kbF7:    return 0x76;
            case kbF8:    return 0x77;
            case kbF9:    return 0x78;
            case kbF10:   return 0x79;
            case kbF11:   return 0x7A;
            case kbF12:   return 0x7B;
            case kbBack:  return 0x08; // VK_BACK
            case kbTab:   return 0x09; // VK_TAB
            case kbEnter: return 0x0D; // VK_RETURN
            case kbEsc:   return 0x1B; // VK_ESCAPE
            case ' ':     return 0x20; // VK_SPACE
            case '-': case kbAltMinus: return 0xBD; // VK_OEM_MINUS
            case '=': case kbAltEqual: return 0xBB; // VK_OEM_PLUS
            case '[': return 0xDB; // VK_OEM_4
            case ']': return 0xDD; // VK_OEM_6
            case '\\': return 0xDC; // VK_OEM_5
            case ';': return 0xBA; // VK_OEM_1
            case '\'': return 0xDE; // VK_OEM_7
            case ',': return 0xBC; // VK_OEM_COMMA
            case '.': return 0xBE; // VK_OEM_PERIOD
            case '/': return 0xBF; // VK_OEM_2
            case '`': return 0xC0; // VK_OEM_3
        }
        return 0;
    }
#endif // HAVE_VTERM_WIN32INPUT

    // Keys that Turbo Vision does not turn into a usable key code: Ctrl
    // with \ [ ] - (they arrive as control characters, e.g. 0x1C for
    // Ctrl+\, possibly with the scan code: 0x2B1C) and the keys of the
    // numeric keypad (told apart from the main keyboard by the scan code).
    struct SpecialKey
    {
        uchar scanCode;
        char ctrlChar;   // Control character with Ctrl (0: none).
        char ch;         // Character on the key.
        ushort vk;       // Windows virtual key code.
        bool enhanced;   // Scan code shared with another key, which this
                         // one tells apart with kbEnhanced (keypad '/').
    };

    static constexpr SpecialKey specialKeys[] =
    {
        { 0x2B, '\x1C', '\\', 0xDC, false }, // VK_OEM_5
        { 0x1A, '\x1B', '[',  0xDB, false }, // VK_OEM_4
        { 0x1B, '\x1D', ']',  0xDD, false }, // VK_OEM_6
        { 0x0C, '\x1F', '-',  0xBD, false }, // VK_OEM_MINUS (Ctrl+_ = Ctrl+Shift+-)
        { 0x37, 0,      '*',  0x6A, false }, // VK_MULTIPLY (keypad)
        { 0x4E, 0,      '+',  0x6B, false }, // VK_ADD (keypad)
        { 0x4A, 0,      '-',  0x6D, false }, // VK_SUBTRACT (keypad)
        { 0x35, 0,      '/',  0x6F, true  }, // VK_DIVIDE (keypad)
    };

    static const SpecialKey *findSpecialKey(const KeyDownEvent &keyDown)
    {
        uchar scan = keyDown.charScan.scanCode;
        uchar ch = keyDown.charScan.charCode;
        bool ctrl = (keyDown.controlKeyState & kbCtrlShift) != 0;
        bool enhanced = (keyDown.controlKeyState & kbEnhanced) != 0;
        for (const auto &k : specialKeys)
        {
            if (k.ctrlChar)
            {
                // With the scan code, the character may also be the one on
                // the key (or '_'); without it, only the control character
                // identifies the key. Escape has its own scan code (0x01).
                if ( ctrl &&
                     ( (scan == k.scanCode && (ch == (uchar) k.ctrlChar || ch == (uchar) k.ch ||
                                               (k.ch == '-' && ch == '_'))) ||
                       (scan == 0 && ch == (uchar) k.ctrlChar) ) )
                    return &k;
            }
            else if (scan == k.scanCode && ch == (uchar) k.ch && enhanced == k.enhanced)
                return &k;
        }
        return nullptr;
    }

    static void processKey(VTerm *vt, KeyDownEvent keyDown, bool win32InputEnabled)
    {
        TKey tvKey(keyDown);
        const SpecialKey *special = findSpecialKey(keyDown);
#ifdef HAVE_VTERM_WIN32INPUT
        if (win32InputEnabled)
        {
            uint vk = special ? special->vk : tvToWin32KeyCode(tvKey.code);
            uint cs = tvToWin32ControlState(keyDown.controlKeyState);
            uint uc = (keyDown.textLength > 0) ? utf8To32(keyDown.getText()) : 0;
            // Like Windows: Ctrl+\ gives the character 0x1C, etc.
            if ( special && special->ctrlChar &&
                 !(keyDown.controlKeyState & kbAltShift) )
                uc = (uchar) special->ctrlChar;
            vterm_keyboard_win32(vt, vk, 0, uc, 1, cs, 1);
            vterm_keyboard_win32(vt, vk, 0, uc, 0, cs, 1);
            return;
        }
#else
        (void) win32InputEnabled;
#endif
        VTermModifier vtMod = convMod(tvKey.mods);
        // Ctrl+\ [ ] _ that arrived as control characters: pass them
        // directly, as a terminal does.
        if ( special && special->ctrlChar
             && (uchar) keyDown.charScan.charCode == (uchar) special->ctrlChar )
        {
            keyDown.text[0] = special->ctrlChar;
            keyDown.textLength = 1;
            vtMod = VTERM_MOD_NONE;
        }
        // Pass control characters directly, with no modifiers.
        else if ( tvKey.mods == kbCtrlShift
             && 'A' <= tvKey.code && tvKey.code <= 'Z' )
        {
            keyDown.text[0] = keyDown.charScan.charCode;
            keyDown.textLength = 1;
            vtMod = VTERM_MOD_NONE;
        }
        // Pass other legacy 'letter+mod' combinations as text.
        else if ( keyDown.textLength == 0
                  && ' ' <= tvKey.code && tvKey.code < '\x7F' )
        {
            keyDown.text[0] = (char) tvKey.code;
            keyDown.textLength = 1;
            // On Windows, ConPTY unfortunately adds the Shift modifier on an
            // uppercase Alt+Key, so make it lowercase.
            if ( (keyDown.controlKeyState & (kbShift | kbCtrlShift | kbAltShift)) == kbLeftAlt
                 && ('A' <= tvKey.code && tvKey.code <= 'Z') )
                keyDown.text[0] += 'a' - 'A';
        }

        if (keyDown.textLength != 0)
            vterm_keyboard_unichar(vt, utf8To32(keyDown.getText()), vtMod);
        else if (VTermKey vtKey = convKey(tvKey.code))
            vterm_keyboard_key(vt, vtKey, vtMod);
    }

    static void processMouse(VTerm *vt, ushort what, const MouseEventType &mouse)
    {
        VTermModifier mod; int button;
        convMouse(mouse, mod, button);
        vterm_mouse_move(vt, mouse.where.y, mouse.where.x, mod);
        if (what & (evMouseDown | evMouseUp | evMouseWheel))
            vterm_mouse_button(vt, button, what != evMouseUp, mod);
    }

    static void wheelToArrow(VTerm *vt, uchar wheel)
    {
        VTermKey key = VTERM_KEY_NONE;
        switch (wheel)
        {
            case mwUp: key = VTERM_KEY_UP; break;
            case mwDown: key = VTERM_KEY_DOWN; break;
            case mwLeft: key = VTERM_KEY_LEFT; break;
            case mwRight: key = VTERM_KEY_RIGHT; break;
        }
        int lines = terminalOptions.wheelLines;
        for (int i = 0; i < lines; ++i)
            vterm_keyboard_key(vt, key, VTERM_MOD_NONE);
    }

    // Output conversion.

    static TColorRGB VTermRGBtoRGB(VTermColor c)
    {
        return {c.rgb.red, c.rgb.green, c.rgb.blue};
    }

    static TColorAttr convAttr(const VTermScreenCell &cell)
    {
        auto &vt_fg = cell.fg,
             &vt_bg = cell.bg;
        auto &vt_attr = cell.attrs;
        TColor fg, bg;

        if (!VTERM_COLOR_IS_DEFAULT_FG(&vt_fg))
        {
            if (VTERM_COLOR_IS_INDEXED(&vt_fg))
                fg = TColorXTerm(vt_fg.indexed.idx);
            else if (VTERM_COLOR_IS_RGB(&vt_fg))
                fg = VTermRGBtoRGB(vt_fg);
        }

        if (!VTERM_COLOR_IS_DEFAULT_BG(&vt_bg))
        {
            if (VTERM_COLOR_IS_INDEXED(&vt_bg))
                bg = TColorXTerm(vt_bg.indexed.idx);
            else if (VTERM_COLOR_IS_RGB(&vt_bg))
                bg = VTermRGBtoRGB(vt_bg);
        }

        ushort style =
              (slBold & -!!vt_attr.bold)
            | (slItalic & -!!vt_attr.italic)
            | (slUnderline & -!!vt_attr.underline)
            | (slBlink & -!!vt_attr.blink)
            | (slReverse & -!!vt_attr.reverse)
            | (slStrike & -!!vt_attr.strike)
            ;
        return {fg, bg, style};
    }

    static void convCell( TSpan<TScreenCell> cells, int x,
                          const VTermScreenCell &vtCell )
    {
        if (vtCell.chars[0] == (uint32_t) -1) // Wide char trail.
        {
            // Turbo Vision and libvterm may disagree on what characters
            // are double-width. If libvterm considers a character isn't
            // double-width but Turbo Vision does, it will manage to display it
            // properly anyway. But, in the opposite case, we need to place a
            // space after the double-width character.
            if (x > 0 && !cells[x - 1].character.isWide())
            {
                cells[x].character = ' ';
                cells[x].attribute = cells[x - 1].attribute;
            }
        }
        else
        {
            size_t length = 0;
            while (vtCell.chars[length])
                ++length;
            TSpan<const uint32_t> text {vtCell.chars, max<size_t>(1, length)};
            TText::drawStr(cells, x, text, 0, convAttr(vtCell));
        }
    }

    static void drawLine( TerminalSurface &surface, VTermScreen *vtScreen,
                          int y, int rowIndex, int start, int end )
    // Pre: the area must be within bounds.
    {
        dout << "drawLine(" << y << ", " << start << ", " << end << ")" << std::endl;
        TSpan<TScreenCell> cells(&surface.at(y, 0), surface.size.x);
        for (int x = start; x < end; ++x)
        {
            VTermScreenCell cell;
            if (vterm_screen_get_cell(vtScreen, {rowIndex, x}, &cell))
                convCell(cells, x, cell);
            else
                cells[x] = {};
        }
        surface.addDamageAtRow(y, start, end);
    }

} // namespace vtermemu

TerminalEmulator &VTermEmulatorFactory::create(TPoint size, Writer &clientDataWriter) noexcept
{
    return *new VTermEmulator(size, clientDataWriter);
}

TSpan<const EnvironmentVar> VTermEmulatorFactory::getCustomEnvironment() noexcept
{
    static constexpr EnvironmentVar customEnvironment[] =
    {
        {"TERM", "xterm-256color"},
        {"COLORTERM", "truecolor"},
    };

    return customEnvironment;
}

VTermEmulator::VTermEmulator(TPoint size, Writer &aClientDataWriter) noexcept :
    clientDataWriter(aClientDataWriter)
{
    // VTerm requires size to be at least 1.
    size.x = max(size.x, 1);
    size.y = max(size.y, 1);
    damageByRow.resize(size.y);

    vt = vterm_new(size.y, size.x);
    vterm_set_utf8(vt, 1);

    vtState = vterm_obtain_state(vt);
    vterm_state_reset(vtState, true);

    vtScreen = vterm_obtain_screen(vt);
    vterm_screen_enable_altscreen(vtScreen, true);
    vterm_screen_set_callbacks(vtScreen, &callbacks, this);
    vterm_screen_set_damage_merge(vtScreen, VTERM_DAMAGE_SCROLL);
    vterm_screen_reset(vtScreen, true);

    vterm_screen_callbacks_has_pushline4(vtScreen);
    // This cannot be enabled yet, it will easily crash (e.g. shrink the terminal window
    // while running an alternate buffer application):
    // vterm_screen_enable_reflow(vtScreen, true);

    vterm_output_set_callback(vt, _static_wrap(&VTermEmulator::writeOutput), this);

    // Clipboard requests from the client application (OSC 52).
    vterm_state_set_selection_callbacks( vtState, &selectionCallbacks, this,
                                         selectionDecodeBuf, sizeof(selectionDecodeBuf) );

    // far2l terminal extensions (clipboard).
    vterm_state_set_unrecognised_fallbacks(vtState, &stateFallbacks, this);
    Far2lExtensions::Callbacks far2lCallbacks;
    far2lCallbacks.write = [this] (const char *data, size_t size) {
        writeOutput(data, size);
    };
    far2lCallbacks.setClipboard = [this] (std::string text) {
        localState.clipboardChanged = true;
        localState.clipboardText.clear();
        localState.clipboardText.push(text.data(), text.size());
    };
    far2lCallbacks.requestClipboard = [this] (uint8_t requestId, uint32_t format) {
        localState.far2lClipboardRequests.push_back({requestId, format});
    };
    far2lCallbacks.getSize = [this] () {
        return getSize();
    };
    far2l.reset(new Far2lExtensions(std::move(far2lCallbacks)));

    // VTerm's cursor blinks by default, but it shouldn't.
    VTermValue val {0};
    vterm_state_set_termprop(vtState, VTERM_PROP_CURSORBLINK, &val);
}

VTermEmulator::~VTermEmulator()
{
    vterm_free(vt);
}

void VTermEmulator::handleEvent(const TerminalEvent &event) noexcept
{
    using namespace vtermemu;
    switch (event.type)
    {
        case TerminalEventType::KeyDown:
            processKey(vt, event.keyDown, localState.win32InputEnabled);
            scrollback.followTerminal();
            break;

        case TerminalEventType::Mouse:
            if (localState.mouseEnabled)
                processMouse(vt, event.mouse.what, event.mouse.mouse);
            else if (localState.altScreenEnabled && event.mouse.what == evMouseWheel)
                wheelToArrow(vt, event.mouse.mouse.wheel);
            else if (event.mouse.what == evMouseWheel)
                handleScrollbackWheel(event.mouse.mouse.wheel);
            break;

        case TerminalEventType::ClientDataRead:
        {
            auto &clientData = event.clientDataRead;
            vterm_input_write(vt, clientData.data, clientData.size);
            break;
        }

        case TerminalEventType::ViewportResize:
        {
            TPoint size = {event.viewportResize.x, event.viewportResize.y};
            setSize(size);
            break;
        }

        case TerminalEventType::FocusChange:
            if (event.focusChange.focusEnabled)
                vterm_state_focus_in(vtState);
            else
                vterm_state_focus_out(vtState);
            break;

        case TerminalEventType::ScrollBackOffsetChange:
            scrollback.setOffset(event.scrollBackOffsetChange.offset);
            break;

        case TerminalEventType::ScrollBackScroll:
        {
            int delta = event.scrollBackScroll.delta;
            if (delta <= ScrollBackScrollEvent::scrollToTop)
                scrollback.setOffset(0);
            else if (delta >= ScrollBackScrollEvent::scrollToBottom)
                scrollback.followTerminal();
            else
                scrollback.incrementOffset(delta);
            break;
        }

        case TerminalEventType::CopySelection:
            copySelection(event.copySelection.selection);
            break;

        case TerminalEventType::Paste:
            pasteText(event.paste.data, event.paste.size);
            delete[] event.paste.data;
            scrollback.followTerminal();
            break;

        case TerminalEventType::Far2lClipboardData:
        {
            auto &data = event.far2lClipboardData;
            far2l->sendClipboardData(data.requestId, data.format, {data.data, data.size});
            delete[] data.data;
            break;
        }

        default:
            break;
    }
}

void VTermEmulator::updateState(TerminalState &state) noexcept
{
    // The maximum size of the scrollback may have been reduced.
    if (scrollback.trim(max(0, (int) terminalOptions.scrollbackLines)))
        localState.scrollOverflowed = true;

    updateSurface(state.surface);

    if (localState.cursorChanged)
    {
        localState.cursorChanged = false;
        state.cursorChanged = true;
        state.cursorPos = localState.cursorPos;
        state.cursorVisible = localState.cursorVisible;
        state.cursorBlink = localState.cursorBlink;
    }

    if (localState.titleChanged)
    {
        localState.titleChanged = false;
        state.titleChanged = true;
        state.title = std::move(localState.title);
    }

    bool scrollbackChanged = ( scrollback.offset != state.scrollbackOffset ||
                               scrollback.numLines() != state.scrollbackLimit ||
                               !localState.altScreenEnabled != state.scrollbackEnabled );
    if (scrollbackChanged)
    {
        state.scrollbackChanged = true;
        state.scrollbackOffset = scrollback.offset;
        state.scrollbackLimit = scrollback.numLines();
        // Hide the scroll bar when in the alternate buffer so that it cannot
        // be confused with the client application's content.
        state.scrollbackEnabled = !localState.altScreenEnabled;
    }
    state.scrollbackDropped = scrollback.dropped;

    state.mouseEnabled = localState.mouseEnabled;

    if (localState.clipboardChanged)
    {
        localState.clipboardChanged = false;
        state.clipboardChanged = true;
        state.clipboardText = std::move(localState.clipboardText);
    }

    state.far2lExtensionsEnabled = far2l->isEnabled();

    if (!localState.far2lClipboardRequests.empty())
    {
        for (auto &req : localState.far2lClipboardRequests)
            state.far2lClipboardRequests.push_back(req);
        localState.far2lClipboardRequests.clear();
    }
}

TPoint VTermEmulator::getSize() noexcept
{
    TPoint size;
    vterm_get_size(vt, &size.y, &size.x);
    return size;
}

void VTermEmulator::setSize(TPoint size) noexcept
{
    size.x = max(size.x, 1);
    size.y = max(size.y, 1);

    if (size != getSize())
    {
        vterm_set_size(vt, size.y, size.x);
        damageByRow.resize(0);
        damageByRow.resize(size.y);
    }
}

void VTermEmulator::updateSurface(TerminalSurface &surface) noexcept
{
    using namespace vtermemu;
    // Make sure the surface has the right size.
    bool resized = false;
    TPoint size = getSize();
    if (surface.size != size)
    {
        resized = true; // The surface's previous contents have been lost.
        surface.resize(size);
    }

    // Draw the scrollback if it is visible.
    int visibleScrollLines = scrollback.getVisibleScrollLines(size.y);
    bool scrollbackAreaChanged = (
        scrollback.offset != localState.scrollOffset ||
        visibleScrollLines != localState.visibleScrollLines ||
        (localState.scrollOverflowed && scrollback.offset == 0)
    );
    localState.scrollOffset = scrollback.offset;
    localState.visibleScrollLines = visibleScrollLines;
    localState.scrollOverflowed = false;

    // When the scrollback or the size change, everything needs to redrawn.
    // Note that, unlike the terminal's display, the scrollback content's are
    // immutable,
    bool needsRedraw = scrollbackAreaChanged || resized;

    if (needsRedraw)
        for (int y = 0; y < visibleScrollLines; ++y)
            drawScrollbackLine(surface, y);

    // Draw the actual terminal display.
    vterm_screen_flush_damage(vtScreen); // Calls 'VTermEmulator::damage()'.

    for (int y = visibleScrollLines; y < size.y; ++y)
    {
        int rowIndex = y - visibleScrollLines;

        auto &damage = damageByRow[rowIndex];
        int start = needsRedraw ? 0 : max(damage.start, 0);
        int end = needsRedraw ? size.x : min(damage.end, size.x);

        if (start < end)
            drawLine(surface, vtScreen, y, rowIndex, start, end);

        damage = {};
    }
}

void VTermEmulator::writeOutput(const char *data, size_t size)
{
    clientDataWriter.write({data, size});
}

int VTermEmulator::damage(VTermRect rect)
{
    rect.start_row = min(max(rect.start_row, 0), damageByRow.size());
    rect.end_row = min(max(rect.end_row, 0), damageByRow.size());
    for (int y = rect.start_row; y < rect.end_row; ++y)
    {
        auto &damage = damageByRow[y];
        damage.start = min(rect.start_col, damage.start);
        damage.end = max(rect.end_col, damage.end);
    }
    return true;
}

int VTermEmulator::moverect(VTermRect dest, VTermRect src)
{
    dout << "moverect(" << dest << ", " << src << ")" << std::endl;
    return false;
}

int VTermEmulator::movecursor(VTermPos pos, VTermPos oldpos, int visible)
{
    localState.cursorChanged = true;
    localState.cursorPos = {pos.col, pos.row};
    return true;
}

int VTermEmulator::settermprop(VTermProp prop, VTermValue *val)
{
    dout << "settermprop(" << prop << ", " << val << ")" << std::endl;
    if (vterm_get_prop_type(prop) == VTERM_VALUETYPE_STRING)
    {
        if (val->string.initial)
            strFragBuf.clear();
        strFragBuf.push(val->string.str, val->string.len);
        if (!val->string.final)
            return true;
    }

    switch (prop)
    {
        case VTERM_PROP_TITLE:
            localState.titleChanged = true;
            localState.title = std::move(strFragBuf);
            break;
        case VTERM_PROP_CURSORVISIBLE:
            localState.cursorChanged = true;
            localState.cursorVisible = val->boolean;
            break;
        case VTERM_PROP_CURSORBLINK:
            localState.cursorChanged = true;
            localState.cursorBlink = val->boolean;
            break;
        case VTERM_PROP_MOUSE:
            localState.mouseEnabled = val->boolean;
            break;
        case VTERM_PROP_ALTSCREEN:
            localState.altScreenEnabled = val->boolean;
            if (localState.altScreenEnabled)
                scrollback.followTerminal();
            break;
#ifdef HAVE_VTERM_WIN32INPUT
        case VTERM_PROP_WIN32INPUT:
            localState.win32InputEnabled = val->boolean;
            break;
#endif
        default:
            return false;
    }
    return true;
}

int VTermEmulator::bell()
{
    dout << "bell()" << std::endl;
    return false;
}

int VTermEmulator::resize(int rows, int cols)
{
    return false;
}

int VTermEmulator::sb_pushline4(int cols, const VTermScreenCell *cells, bool continuation)
{
    if (!localState.altScreenEnabled)
        if (!scrollback.pushLine(cols, cells, continuation))
            localState.scrollOverflowed = true;
    return true;
}

int VTermEmulator::selection_set(VTermSelectionMask, VTermStringFragment frag)
// Invoked by libvterm when the client sends 'OSC 52 ; <targets> ; <base64> ST',
// with the already-decoded text, possibly split in several fragments.
// All targets (clipboard, primary selection...) go to the same clipboard.
{
    if (frag.initial)
    {
        selectionBuf.clear();
        selectionTooLarge = false;
    }
    if (!selectionTooLarge)
    {
        if (selectionBuf.size() + frag.len <= maxClipboardTextSize)
            selectionBuf.push(frag.str, frag.len);
        else
        {
            selectionBuf.clear();
            selectionTooLarge = true;
        }
    }
    if (frag.final)
    {
        // An empty text means 'clear the selection' or invalid input:
        // leave the clipboard untouched in that case.
        if (!selectionTooLarge && selectionBuf.size() > 0)
        {
            localState.clipboardChanged = true;
            localState.clipboardText = std::move(selectionBuf);
        }
        selectionBuf.clear();
        selectionTooLarge = false;
    }
    return 1;
}

int VTermEmulator::selection_query(VTermSelectionMask)
// Invoked when the client asks for the clipboard contents ('OSC 52 ; c ; ?').
// Not supported on purpose: any program could read what the user copied
// (passwords, for example). This is also the default in most terminals.
{
    return 1;
}

void VTermEmulator::pasteText(const char *data, size_t size) noexcept
// Sends pasted text to the client application. If the application enabled
// bracketed paste mode (e.g. bash, vim, far2l), the text is enclosed in
// 'ESC [200~' ... 'ESC [201~', so that it is not executed line by line;
// libvterm only sends these markers when the mode is enabled.
{
    std::string text;
    text.reserve(size);
    for (size_t i = 0; i < size; ++i)
    {
        unsigned char c = data[i];
        if (c == '\r')
        {
            // "\r\n" (Windows line ending) -> "\r".
            if (i + 1 < size && data[i + 1] == '\n')
                ++i;
            text += '\r';
        }
        else if (c == '\n')
            text += '\r';
        else if (c == '\t' || (c >= 0x20 && c != 0x7F))
            text += (char) c;
        // Other control characters are dropped. In particular ESC, so that
        // pasted text cannot end the bracketed paste early ('ESC [201~')
        // and inject commands.
    }
    vterm_keyboard_start_paste(vt);
    writeOutput(text.data(), text.size());
    vterm_keyboard_end_paste(vt);
}

int VTermEmulator::apc(VTermStringFragment frag)
// Invoked by libvterm for APC strings ('ESC _ ... ST'), possibly split in
// several fragments. Used by far2l's terminal extensions.
{
    if (frag.initial)
    {
        apcBuf.clear();
        apcTooLarge = false;
    }
    if (!apcTooLarge)
    {
        if (apcBuf.size() + frag.len <= maxApcSize)
            apcBuf.push(frag.str, frag.len);
        else
        {
            apcBuf.clear();
            apcTooLarge = true;
        }
    }
    if (frag.final)
    {
        if (!apcTooLarge)
            far2l->handleApc(apcBuf.data(), apcBuf.size());
        apcBuf.clear();
        apcTooLarge = false;
    }
    return 1;
}

void VTermEmulator::handleScrollbackWheel(uchar wheel) noexcept
{
    int lines = terminalOptions.wheelLines;
    if (wheel == mwUp)
        scrollback.incrementOffset(-lines);
    else if (wheel == mwDown)
        scrollback.incrementOffset(lines);
}

void VTermEmulator::drawScrollbackLine(TerminalSurface &surface, int y) noexcept
{
    using namespace vtermemu;
    int lineIndex = scrollback.offset + y;
    if (lineIndex < 0 || lineIndex >= scrollback.numLines())
        return;
    auto &line = scrollback.lines[lineIndex];

    TSpan<TScreenCell> rowCells(&surface.at(y, 0), surface.size.x);
    VTermScreenCell defCell = getDefaultCell();
    for (int x = 0; x < (int) rowCells.size(); ++x)
    {
        if (x < line.cols)
            convCell(rowCells, x, line.cells.get()[x]);
        else
            convCell(rowCells, x, defCell);
    }
    surface.addDamageAtRow(y, 0, rowCells.size());
}

void VTermEmulator::copySelection(SelectionRange selection) noexcept
{
    GrowArray text;
    extractSelectionText(selection, text);
    TClipboard::setText({text.data(), text.size()});
}

void VTermEmulator::getSelectionText(SelectionRange selection, std::string &out) noexcept
{
    GrowArray text;
    extractSelectionText(selection, text);
    out.assign(text.data(), text.size());
}

void VTermEmulator::extractSelectionText(SelectionRange selection, GrowArray &text) noexcept
{
    auto start = min(selection.start, selection.end);
    auto end = max(selection.start, selection.end);

    TPoint termSize = getSize();
    int maxY = scrollback.numLines() + termSize.y;

    int startY = max(0, start.y);
    int endY = min(end.y + 1, maxY);

    int padding = 0;

    for (int y = startY; y < endY; ++y)
    {
        int startX = (y == start.y) ? max(0, start.x) : 0;
        int endX = (y == end.y) ? end.x : INT_MAX;

        if (y < scrollback.numLines())
            extractScrollbackLineText(y, startX, endX, text, padding);
        else
            extractTerminalLineText(y - scrollback.numLines(), startX, endX, text, padding);
    }
}

bool VTermEmulator::isContinuationRow(int absY) noexcept
// Whether the absolute row 'absY' continues the previous one (the line was
// wrapped by the terminal's width).
{
    int sbLines = scrollback.numLines();
    if (absY <= 0)
        return false;
    if (absY < sbLines)
        return scrollback.lines[absY].continuation;
    int row = absY - sbLines;
    if (row < getSize().y)
        return vterm_state_get_lineinfo(vtState, row)->continuation;
    return false;
}

void VTermEmulator::getText( TextSnapshot &snapshot, int startRow, int endRow,
                             bool withPositions ) noexcept
{
    TPoint termSize = getSize();
    int sbLines = scrollback.numLines();
    int totalRows = sbLines + termSize.y;

    snapshot = TextSnapshot();
    snapshot.scrollbackLines = sbLines;
    snapshot.screenRows = termSize.y;
    snapshot.altScreen = localState.altScreenEnabled;

    // In the alternate screen, the scrollback belongs to another program.
    int firstRow = localState.altScreenEnabled ? sbLines : 0;
    startRow = max(startRow, firstRow);
    endRow = min(endRow, totalRows);
    if (startRow >= endRow)
        return;
    // Extend the range to whole logical lines.
    while (startRow > firstRow && isContinuationRow(startRow))
        --startRow;
    while (endRow < totalRows && isContinuationRow(endRow))
        ++endRow;

    TextLine *line = nullptr;
    int padding = 0; // Empty cells not yet added to the line.
    std::vector<SelectionAnchor> padCells;
    for (int y = startRow; y < endRow; ++y)
    {
        if (!line || !isContinuationRow(y) || y == firstRow)
        {
            snapshot.lines.emplace_back();
            line = &snapshot.lines.back();
            line->firstRow = y;
            padding = 0;
            padCells.clear();
        }
        line->lastRow = y;

        const VTermScreenCell *sbCells = nullptr;
        int cols = termSize.x;
        if (y < sbLines)
        {
            sbCells = scrollback.lines[y].cells.get();
            cols = scrollback.lines[y].cols;
        }
        for (int x = 0; x < cols; ++x)
        {
            VTermScreenCell cell;
            if (sbCells)
                cell = sbCells[x];
            else if (!vterm_screen_get_cell(vtScreen, {y - sbLines, x}, &cell))
                continue;

            if (cell.chars[0] == (uint32_t) -1) // Wide char trail.
                continue;
            if (cell.chars[0] == 0)
            {
                ++padding;
                if (withPositions)
                    padCells.push_back({x, y});
                continue;
            }
            for (int i = 0; i < padding; ++i)
            {
                if (withPositions)
                {
                    line->charCell.push_back((int) line->cells.size());
                    line->cells.push_back(padCells[i]);
                    line->cellWidth.push_back(1);
                }
                line->text.push_back(U' ');
            }
            padding = 0;
            padCells.clear();

            if (withPositions)
            {
                line->cells.push_back({x, y});
                line->cellWidth.push_back(cell.width > 1 ? 2 : 1);
            }
            for (int i = 0; i < VTERM_MAX_CHARS_PER_CELL && cell.chars[i]; ++i)
            {
                if (withPositions)
                    line->charCell.push_back((int) line->cells.size() - 1);
                line->text.push_back((char32_t) cell.chars[i]);
            }
        }
    }
}

static void extractCellText(const VTermScreenCell &cell, GrowArray &text, int &padding) noexcept
{
    // Erased cells will be handled as spaces if followed by text.
    if (cell.chars[0] == 0)
        padding++;
    // Ignore gaps behind double-width chars.
    else if (cell.chars[0] != (uint32_t) -1)
    {
        while (padding--)
            text.push(" ", 1);
        padding = 0;

        size_t length = 0;
        while (cell.chars[length] && length < VTERM_MAX_CHARS_PER_CELL)
            ++length;
        for (size_t i = 0; i < length; ++i)
        {
            char utf8[4];
            size_t utf8len = utf32To8(cell.chars[i], utf8);
            text.push(utf8, utf8len);
        }
    }
}

void VTermEmulator::extractScrollbackLineText(int y, int startX, int endX, GrowArray &text, int &padding) noexcept
// Pre: 0 <= y < scrollback.numLines()
{
    auto &line = scrollback.lines[y];
    endX = min(endX, line.cols);

    if (startX < line.cols && endX > 0)
        for (int x = startX; x < endX && x < line.cols; ++x)
            extractCellText(line.cells.get()[x], text, padding);

    if (endX == line.cols)
    {
        bool addNewline = false;
        if (y < scrollback.numLines() - 1)
            addNewline = !scrollback.lines[y + 1].continuation;
        else if (getSize().y > 0)
        {
            const VTermLineInfo *nextLineInfo = vterm_state_get_lineinfo(vtState, 0);
            addNewline = !nextLineInfo->continuation;
        }

        if (addNewline)
        {
            text.push("\n", 1);
            padding = 0;
        }
    }
}

void VTermEmulator::extractTerminalLineText(int y, int startX, int endX, GrowArray &text, int &padding) noexcept
// Pre: 0 <= y < getSize().y
{
    TPoint termSize = getSize();
    endX = min(endX, termSize.x);

    if (startX < termSize.x && endX > 0)
    {
        for (int x = startX; x < endX && x < termSize.x; ++x)
        {
            VTermScreenCell cell;
            if (vterm_screen_get_cell(vtScreen, {y, x}, &cell))
                extractCellText(cell, text, padding);
        }
    }

    if (endX == termSize.x && y < termSize.y - 1)
    {
        const VTermLineInfo *nextLineInfo = vterm_state_get_lineinfo(vtState, y + 1);
        if (!nextLineInfo->continuation)
        {
            text.push("\n", 1);
            padding = 0;
        }
    }
}

inline VTermScreenCell VTermEmulator::getDefaultCell() const
{
    VTermScreenCell cell {};
    cell.width = 1;
    vterm_state_get_default_colors(vtState, &cell.fg, &cell.bg);
    return cell;
}

bool VTermEmulator::Scrollback::trim(int maxLines)
{
    int excess = numLines() - maxLines;
    if (excess <= 0)
        return false;
    bool following = isFollowingTerminal();
    lines.erase(lines.begin(), lines.begin() + excess);
    dropped += excess;
    offset = following ? numLines() : max(offset - excess, 0);
    return true;
}

bool VTermEmulator::Scrollback::pushLine(int cols, const VTermScreenCell *src, bool continuation)
{
    int maxLines = max(0, (int) terminalOptions.scrollbackLines);
    if (maxLines == 0)
    {
        // No scrollback: drop what is left after the option was changed.
        bool trimmed = trim(0);
        return !trimmed;
    }

    // This must be checked before altering the number of lines.
    bool following = isFollowingTerminal();

    bool overflowed = false;
    if (numLines() >= maxLines)
    {
        overflowed = true;
        int excess = numLines() - maxLines + 1;
        lines.erase(lines.begin(), lines.begin() + excess);
        dropped += excess;
        if (!following)
            offset = max(offset - excess, 0);
    }

    auto *line = new VTermScreenCell[cols];
    memcpy(line, src, sizeof(VTermScreenCell)*cols);
    lines.push_back({std::unique_ptr<const VTermScreenCell[]>(line), cols, continuation});

    if (following)
        offset = numLines();

    return !overflowed;
}

void VTermEmulator::Scrollback::incrementOffset(int count)
// count < 0: show older lines
// count > 0: show newer lines
{
    if (count < 0)
        offset = max(offset + count, 0);
    else
        offset = min(offset + count, numLines());
}

void VTermEmulator::Scrollback::setOffset(int newOffset)
{
    offset = min(max(0, newOffset), numLines());
}

void VTermEmulator::Scrollback::followTerminal()
{
    offset = numLines();
}

bool VTermEmulator::Scrollback::isFollowingTerminal() const
{
    return offset == numLines();
}

int VTermEmulator::Scrollback::getVisibleScrollLines(int terminalHeight) const
{
    int available = max(numLines() - offset, 0);
    return min(available, terminalHeight);
}

int VTermEmulator::Scrollback::numLines() const
{
    return lines.size();
}

} // namespace tvterm
