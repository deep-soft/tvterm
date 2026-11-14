#ifndef TVTERM_CAPTURE_H
#define TVTERM_CAPTURE_H

// Capture Desk / Capture Window (item 30, P1; E71): what the screen shows,
// as UTF-8 text with the frames and no colors, in a file of the [capture]
// folder of tvterm.ini (default ~/logs/tvterm/capture):
//   dsk-YYYY-MM-DD-hhmmss.txt  the desktop area (between the top bar and the
//                              status line), every row as wide as the
//                              screen, spaces where there is no window;
//   wnd-YYYY-MM-DD-hhmmss.txt  one window (or dialog) at its size, with its
//                              frame, its own content even where another
//                              window covers it.
// A wide character (CJK, emoji) takes two columns on the screen and is one
// character in the file. Trailing spaces are kept (an exact rectangle).

#include <string>

class TView;
class TDeskTop;

namespace capture
{

// [capture] folder ('~' allowed).
void setFolder(const std::string &folder);
const std::string &defaultFolder();

// Both return the status line message (the path, or what went wrong).
std::string captureDesk(TDeskTop &deskTop);
std::string captureWindow(TView *window);

} // namespace capture

#endif // TVTERM_CAPTURE_H
