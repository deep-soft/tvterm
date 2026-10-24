#ifndef TVTERM_EDITWND_H
#define TVTERM_EDITWND_H

#define Uses_TEditWindow
#include <tvision/tv.h>

// A simple text editor window, meant for notes and as an intermediate
// buffer for the clipboard.
//
// The editor has no private clipboard, so Cut/Copy/Paste use the system
// clipboard. When running over SSH from putty4far2l (or inside far2l),
// that is the Windows clipboard.

class TVTermEditWindow : public TEditWindow
{
public:

    TVTermEditWindow(const TRect &bounds, TStringView fileName) noexcept;

    // Must be called once at startup: installs the dialogs used by the
    // editor (Save As, 'file modified', errors...).
    static void initEditorDialogs();
};

#endif // TVTERM_EDITWND_H
