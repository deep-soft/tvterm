#define Uses_TDialog
#define Uses_TCheckBoxes
#define Uses_TSItem
#define Uses_TInputLine
#define Uses_TLabel
#define Uses_TStaticText
#define Uses_TButton
#define Uses_TDeskTop
#define Uses_TProgram
#define Uses_TDrawBuffer
#define Uses_TText
#define Uses_MsgBox
#include <tvision/tv.h>

#include "alerts.h"
#include "wnd.h"
#include "winlist.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace alertdlg
{

static std::string trim(const std::string &s)
{
    size_t a = s.find_first_not_of(" \t");
    if (a == std::string::npos)
        return {};
    size_t b = s.find_last_not_of(" \t");
    return s.substr(a, b - a + 1);
}

} // namespace alertdlg


std::vector<std::string> parseKeywords(const std::string &text)
{
    std::vector<std::string> keywords;
    size_t start = 0;
    while (start <= text.size())
    {
        size_t end = text.find(',', start);
        if (end == std::string::npos)
            end = text.size();
        std::string keyword = alertdlg::trim(text.substr(start, end - start));
        if (!keyword.empty())
            keywords.push_back(std::move(keyword));
        start = end + 1;
    }
    return keywords;
}

std::string joinKeywords(const std::vector<std::string> &keywords)
{
    std::string out;
    for (const auto &keyword : keywords)
    {
        if (!out.empty())
            out += ", ";
        out += keyword;
    }
    return out;
}

namespace alertdlg
{

struct DialogData
{
    ushort activity;
    char keywords[256];
    char silence[7];
};

} // namespace alertdlg

void execAlertSettings(TerminalWindow &term)
{
    using namespace alertdlg;
    TDialog *d = new TDialog(TRect(0, 0, 66, 17), "Alerts");
    d->options |= ofCentered;

    const char *title = term.getTitle(100);
    std::string heading = "Terminal: ";
    heading += title && *title ? title : "(no title)";
    d->insert(new TStaticText(TRect(3, 2, 63, 3), heading));

    auto *activity = new TCheckBoxes(TRect(3, 4, 63, 5),
        new TSItem("~A~ny new text (activity)", nullptr));
    d->insert(activity);

    auto *keywords = new TInputLine(TRect(3, 7, 63, 8), sizeof(DialogData::keywords) - 1);
    d->insert(keywords);
    d->insert(new TLabel(TRect(2, 6, 63, 7), "~K~eywords (comma-separated, case-insensitive):", keywords));

    auto *silence = new TInputLine(TRect(3, 10, 11, 11), sizeof(DialogData::silence) - 1);
    d->insert(silence);
    d->insert(new TLabel(TRect(2, 9, 63, 10), "~S~ilence: seconds without output (0: off):", silence));

    d->insert(new TStaticText(TRect(3, 12, 63, 13),
        "Alerts appear only for terminals that are not selected."));
    d->insert(new TButton(TRect(20, 14, 30, 16), "O~K~", cmOK, bfDefault));
    d->insert(new TButton(TRect(34, 14, 46, 16), "Cancel", cmCancel, bfNormal));
    activity->select();

    const AlertSettings &cur = term.getAlertSettings();
    DialogData data {};
    data.activity = cur.activity ? 1 : 0;
    snprintf(data.keywords, sizeof(data.keywords), "%s", joinKeywords(cur.keywords).c_str());
    snprintf(data.silence, sizeof(data.silence), "%d", cur.silenceSeconds);
    activity->setData(&data.activity);
    keywords->setData(data.keywords);
    silence->setData(data.silence);

    TView *p = TProgram::application->validView(d);
    if (!p)
        return;
    ushort result = TProgram::deskTop->execView(p);
    if (result == cmOK)
    {
        activity->getData(&data.activity);
        keywords->getData(data.keywords);
        silence->getData(data.silence);
    }
    TObject::destroy(p);
    if (result != cmOK)
        return;

    AlertSettings settings;
    settings.activity = data.activity & 1;
    settings.keywords = parseKeywords(data.keywords);
    char *end;
    long seconds = strtol(data.silence, &end, 10);
    std::string rest = alertdlg::trim(end);
    if (seconds < 0 || !rest.empty() || alertdlg::trim(data.silence).empty())
    {
        if (!alertdlg::trim(data.silence).empty())
            messageBox(mfWarning | mfOKButton,
                "Invalid number of seconds: '%s'. Silence alert disabled.", data.silence);
        seconds = 0;
    }
    settings.silenceSeconds = (int) seconds;
    term.setAlertSettings(std::move(settings));
}

static TerminalWindow *oldestAlert(TDeskTop &deskTop)
{
    TerminalWindow *best = nullptr;
    for (TWindow *w : getDeskTopWindows(deskTop))
        if (auto *term = dynamic_cast<TerminalWindow *>(w))
            if ( term->getAlert() != TerminalWindow::Alert::None &&
                 (!best || term->getAlertOrder() < best->getAlertOrder()) )
                best = term;
    return best;
}

bool goToNextAlert(TDeskTop &deskTop)
{
    if (TerminalWindow *term = oldestAlert(deskTop))
    {
        term->select();
        return true;
    }
    return false;
}

size_t countAlerts(TDeskTop &deskTop)
{
    size_t count = 0;
    for (TWindow *w : getDeskTopWindows(deskTop))
        if (auto *term = dynamic_cast<TerminalWindow *>(w))
            count += term->getAlert() != TerminalWindow::Alert::None;
    return count;
}

void AlertStatusLine::draw()
{
    TVTermStatusLine::draw();
    if (alertCount == 0 || TVTermStatusLine::hasMessage())
        return;
    char text[64];
    snprintf(text, sizeof(text), " Alerts: %zu (Ctrl-B G) ", alertCount);
    int width = (int) strwidth(text);
    if (width >= size.x)
        return;
    TDrawBuffer b;
    b.moveStr(0, text, mapColor(4));
    writeLine(size.x - width, 0, width, 1, b);
}
