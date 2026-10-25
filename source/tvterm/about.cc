#define Uses_TDialog
#define Uses_TStaticText
#define Uses_TButton
#define Uses_TDeskTop
#define Uses_TProgram
#include <tvision/tv.h>

#include "about.h"
#include "apputil.h"

#include <stdio.h>
#include <vector>
#include <algorithm>

std::string versionLine()
{
    return std::string("tvterm ") + buildinfo::version + " (deep-soft), " +
           buildinfo::branch + " " + buildinfo::commit;
}

std::string buildLine()
{
    return std::string("built ") + buildinfo::date + ", " + buildinfo::kind;
}

std::string versionText()
{
    return std::string("tvterm ") + buildinfo::version + " (deep-soft)\n" +
           "commit:   " + buildinfo::commit + " (" + buildinfo::branch + ")\n" +
           "built:    " + buildinfo::date + ", " + buildinfo::kind + "\n" +
           "tvision:  " + buildinfo::tvision + "\n" +
           "libvterm: " + buildinfo::libvterm + "\n";
}

namespace aboutdlg
{

static std::string formatUptime(long long seconds)
// "2d 03:14": days, hours, minutes.
{
    if (seconds < 0)
        seconds = 0;
    long long minutes = seconds / 60;
    char buf[64];
    snprintf( buf, sizeof(buf), "%lldd %02lld:%02lld",
              minutes / (24*60), (minutes / 60) % 24, minutes % 60 );
    return buf;
}

static std::string fitWidth(const std::string &text, int maxWidth)
// Long paths: the beginning is replaced with "...", the end (the file
// name) stays visible.
{
    if (maxWidth < 4 || strwidth(text) <= maxWidth)
        return text;
    size_t start = 0;
    while (start < text.size())
    {
        // Next UTF-8 character.
        do
            ++start;
        while (start < text.size() && ((unsigned char) text[start] & 0xC0) == 0x80);
        std::string rest = text.substr(start);
        if (strwidth(rest) + 3 <= maxWidth)
            return "..." + rest;
    }
    return "...";
}

} // namespace aboutdlg

void showAbout(const AboutInfo &info)
{
    using namespace aboutdlg;
    TRect ext = TProgram::deskTop->getExtent();
    // Frame and margins: 3 columns on each side.
    int maxText = ext.b.x - ext.a.x - 6;

    std::string profile = info.profile.empty() ? std::string("(none)") : info.profile;
    std::string settings = info.settingsPath;
    if (!info.settingsExists)
        settings += " (not found)";

    struct Line { std::string label, value; };
    std::vector<Line> lines = {
        {"Commit:", std::string(buildinfo::commit) + " (" + buildinfo::branch + ")"},
        {"Built:", std::string(buildinfo::date) + ", " + buildinfo::kind},
        {"tvision:", buildinfo::tvision},
        {"libvterm:", buildinfo::libvterm},
        {"", ""},
        {"Settings:", settings},
        {"Profile:", profile},
        {"Uptime:", formatUptime(info.uptimeSeconds)},
        {"Terminals:", std::to_string(info.terminals)},
    };
    const int labelWidth = 11;

    // '\x03' at the start of a line centers it (TStaticText).
    std::vector<std::string> rows;
    rows.push_back(std::string("\x03") + "tvterm " + buildinfo::version + " (deep-soft)");
    rows.push_back("\x03" "Terminal emulator for Turbo Vision");
    rows.push_back("");
    for (auto &line : lines)
    {
        if (line.label.empty())
        {
            rows.push_back("");
            continue;
        }
        std::string row = line.label;
        row.resize(labelWidth, ' ');
        rows.push_back(row + fitWidth(line.value, maxText - labelWidth));
    }
    rows.push_back("");
    rows.push_back(fitWidth("https://github.com/deep-soft/tvterm", maxText));
    rows.push_back(fitWidth("https://github.com/magiblot/tvterm (original)", maxText));

    int width = 36;
    std::string text;
    for (auto &row : rows)
    {
        TStringView view = row;
        if (!view.empty() && view[0] == '\x03')
            view = view.substr(1);
        width = std::max(width, strwidth(view));
        text += row;
        text += '\n';
    }
    text.pop_back();
    width = std::min(width, maxText);

    int textHeight = (int) rows.size();
    // Frame (2) + empty line + text + empty line + button (2).
    int height = textHeight + 6;
    TDialog *d = new TDialog(TRect(0, 0, width + 6, height), "About");
    d->options |= ofCentered;
    d->insert(new TStaticText(TRect(3, 2, 3 + width, 2 + textHeight), text));
    int bx = (width + 6 - 10) / 2;
    d->insert(new TButton(TRect(bx, height - 3, bx + 10, height - 1), "~O~K", cmOK, bfDefault));
    d->selectNext(False);
    execDialog(d);
}
