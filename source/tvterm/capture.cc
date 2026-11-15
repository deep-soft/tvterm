#define Uses_TScreen
#define Uses_TGroup
#define Uses_TDeskTop
#define Uses_TBackground
#define Uses_TWindow
#include <tvision/tv.h>

#include <tvision/internal/codepage.h>

#include "capture.h"
#include "profile.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <fstream>

namespace capture
{

static const std::string defaultPath = "~/logs/tvterm/capture";
static std::string folder = defaultPath;

void setFolder(const std::string &f)
{
    folder = f.empty() ? defaultPath : f;
}

const std::string &defaultFolder()
{
    return defaultPath;
}

static std::string expandHome(const std::string &path)
{
    if (path == "~" || path.compare(0, 2, "~/") == 0)
        if (const char *home = getenv("HOME"))
            return home + path.substr(1);
    return path;
}

// The cells of a rectangle as text: one line per row, wide character
// trails skipped, empty cells as spaces.
static std::string cellsToText(const TScreenCell *cells, int stride, int x0, int y0, int w, int h)
{
    std::string out;
    for (int y = y0; y < y0 + h; ++y)
    {
        for (int x = x0; x < x0 + w; ++x)
        {
            const TScreenCell &c = cells[y * stride + x];
            if (c.character.isWideCharTrail())
                continue;
            // As tvision does before showing a cell (DisplayBuffer::
            // validateCell): an empty cell is a space, a single byte that is
            // not printable ASCII is a codepage character (the frames are
            // drawn with CP437 bytes; only the screen holds them as UTF-8).
            TStringView s = c.character.getText();
            unsigned char ch = s[0];
            if (ch == '\0')
                out += ' ';
            else if (s.size() == 1 && (ch < ' ' || ch >= 0x7F))
            {
                const char (&u)[4] = tvision::CpTranslator::toUtf8(ch);
                out.append(u, strnlen(u, 4));
            }
            else
                out.append(s.data(), s.size());
            // The second column of a wide character: on the screen tvision
            // has already turned its trail cell into a space.
            if (c.character.isWide())
                ++x;
        }
        out += '\n';
    }
    return out;
}

// Writes 'text' to <folder>/<prefix>-YYYY-MM-DD-hhmmss.txt (-2, -3... if
// the name exists); the message for the status line.
static std::string save(const char *prefix, const char *what, const std::string &text)
{
    std::string dir = expandHome(folder);
    while (dir.size() > 1 && dir.back() == '/')
        dir.pop_back();
    makeDirs(dir);
    struct stat st;
    if (stat(dir.c_str(), &st) != 0 || !S_ISDIR(st.st_mode))
        return std::string("Capture failed: cannot create ") + dir + ".";
    char stamp[32];
    time_t now = time(nullptr);
    struct tm tm;
    localtime_r(&now, &tm);
    strftime(stamp, sizeof(stamp), "%Y-%m-%d-%H%M%S", &tm);
    std::string base = dir + "/" + prefix + "-" + stamp;
    std::string path = base + ".txt";
    for (int n = 2; stat(path.c_str(), &st) == 0 && n < 100; ++n)
        path = base + "-" + std::to_string(n) + ".txt";
    std::ofstream f(path, std::ios::binary);
    if (f)
        f << text;
    if (!f)
        return "Capture failed: " + path + ": " + strerror(errno) + ".";
    return std::string(what) + " captured: " + path;
}

std::string captureDesk(TDeskTop &deskTop)
{
    const TScreenCell *cells = TScreen::screenBuffer;
    int width = TScreen::screenWidth;
    if (!cells || width <= 0)
        return "Capture failed: no screen.";
    // The desktop's rows, the whole width of the screen.
    TRect r = deskTop.getBounds();
    int y0 = max<int>(r.a.y, 0), y1 = min<int>(r.b.y, TScreen::screenHeight);
    if (y1 <= y0)
        return "Capture failed: no desktop area.";
    // The desktop background (its pattern, '\xB0' = ░) is captured as
    // spaces (E73): the background is drawn again with spaces (only its
    // uncovered parts; the window shadows are colors only, so they become
    // spaces too), the screen is read, and the pattern is drawn back. Both
    // drawings happen before the screen is next sent to the terminal, which
    // shows no change.
    TBackground *bg = deskTop.background;
    char pattern = bg ? bg->pattern : ' ';
    if (bg && pattern != ' ')
    {
        bg->pattern = ' ';
        bg->drawView();
    }
    std::string text = cellsToText(cells, width, 0, y0, width, y1 - y0);
    if (bg && pattern != ' ')
    {
        bg->pattern = pattern;
        bg->drawView();
    }
    return save("dsk", "Desk", text);
}

std::string captureWindow(TView *view)
{
    auto *window = dynamic_cast<TGroup *>(view);
    if (!window || !(window->state & sfVisible))
        return "No window to capture.";
    // The window draws itself into its own buffer (all of it, also the
    // parts another window covers). Windows are buffered by default; a
    // group that is not gets a buffer for the capture only.
    bool buffered = window->options & ofBuffered;
    window->options |= ofBuffered;
    if (!window->buffer)
        window->getBuffer();
    std::string text;
    bool ok = window->buffer != nullptr;
    if (ok)
    {
        window->lockFlag++;
        window->redraw();
        window->lockFlag--;
        text = cellsToText(window->buffer, window->size.x, 0, 0, window->size.x, window->size.y);
    }
    if (!buffered)
    {
        window->freeBuffer();
        window->options &= ~ofBuffered;
    }
    if (!ok)
        return "Capture failed: the window is not shown.";
    return save("wnd", "Window", text);
}

} // namespace capture
