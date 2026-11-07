#ifndef TVTERM_WINDRAG_H
#define TVTERM_WINDRAG_H

#define Uses_TEvent
#include <tvision/tv.h>

class TDeskTop;

// Moving and resizing windows on the desktop (mouse, or Ctrl-F5 and the
// arrows), for every kind of window, dialogs too:
//  - the bottom edge stops at the status line. tvision itself only keeps
//    the title row on the screen. A window being moved stops there; one
//    being resized keeps its top row and gets lower.
//  - the two top corners of the focused window resize it (tvision only
//    resizes from the bottom corners; its top row moves the window).
namespace windrag
{

// Called at the start of every TVTermApp::getEvent. The drag loops of
// tvision (TView::dragView) and of handleTopCorner read their events
// through it, so the window is corrected after each step, before the
// screen is updated.
void keepAboveStatusLine(TDeskTop &desk);

// Ctrl-F5 / Resize/Move with the keyboard (cmResize for the focused
// window), as in tvision (arrows move, Shift+arrows resize, Ctrl+arrows in
// larger steps, Home / End / PgUp / PgDn to the edges, Enter ends, Esc
// restores), with the bottom edge kept at the status line. tvision's own
// loop cannot be corrected from outside: it keeps the position it read
// before each key. Returns false if the window cannot be moved or resized.
bool keyboardDrag(TDeskTop &desk);

// A left-button press on a top corner of the focused window: resizes it
// until the button is released. Returns true if the press was handled
// (the caller clears the event).
bool handleTopCorner(TDeskTop &desk, TEvent &event);

} // namespace windrag

#endif // TVTERM_WINDRAG_H
