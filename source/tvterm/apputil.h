#ifndef TVTERM_APPUTIL_H
#define TVTERM_APPUTIL_H

#define Uses_TProgram
#define Uses_TDeskTop
#define Uses_TMenuPopup
#define Uses_TKeys
#define Uses_TEvent
#include <tvision/tv.h>

inline ushort execDialog(TDialog *d)
{
    TView *p = TProgram::application->validView(d);
    if (p)
    {
        ushort result = TProgram::deskTop->execView(p);
        TObject::destroy(p);
        return result;
    }
    return cmCancel;
}

class CenteredMenuPopup : public TMenuPopup
{
public:

    CenteredMenuPopup(TMenu *aMenu) noexcept :
        TMenuPopup(TRect(0, 0, 0, 0), aMenu)
    {
        options |= ofCentered;
    }

    void calcBounds(TRect &bounds, TPoint delta) override
    {
        bounds.a.x = (owner->size.x - size.x)/2;
        bounds.a.y = (owner->size.y - size.y)/2;
        bounds.b.x = bounds.a.x + size.x;
        bounds.b.y = bounds.a.y + size.y;
    }
};

// Ctrl-W in the Editor, the Log View and the Event Viewer closes the window,
// like its close button (an editor asks to save its changes). Not in
// terminals: there Ctrl-W goes to the program (bash, far2l, mc).
// Turns the key into cmClose for 'window'; returns whether it did.
inline bool closeKeyToCommand(TView &window, TEvent &ev)
{
    if ( ev.what == evKeyDown && (window.state & sfFocused) &&
         TKey(ev.keyDown) == TKey(kbCtrlW) )
    {
        ev.what = evCommand;
        ev.message.command = cmClose;
        ev.message.infoPtr = &window;
        return true;
    }
    return false;
}

#endif // TVTERM_APPUTIL_H
