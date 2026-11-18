#include "crashlog.h"

#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>

// See crashlog.h. The buffers are written by the main thread only (between
// events); the handler reads them. A crash in the middle of a copy can
// show one garbled line, nothing worse: every buffer always ends with a
// NUL byte.

namespace crashrec
{

enum
{
    infoSlots = 8,      // 7 startup lines + the last Load Profile.
    infoSize = 1024,
    contextSize = 512,
    messageSlots = 50,
    messageSize = 256,
    inputSlots = 30,
    inputSize = 128,
    pathSize = 1024,
    altStackSize = 64 * 1024,
};

static char info[infoSlots][infoSize];
static int startInfos = 0;
static char window[contextSize];
static char dialog[contextSize];
static char messages[messageSlots][messageSize];
static volatile sig_atomic_t messageCount = 0;   // Total added.
static char inputs[inputSlots][inputSize];
static volatile sig_atomic_t inputCount = 0;
static char homeDir[pathSize];
// Seconds east of UTC, taken outside the handler (localtime is not
// signal-safe).
static volatile long gmtOffset = 0;
static volatile sig_atomic_t handling = 0;
static char altStack[altStackSize];

const int signals[] = {SIGSEGV, SIGILL, SIGBUS, SIGFPE, SIGABRT};

static void copy(char *dst, size_t size, const std::string &text)
// At most size - 1 bytes, line breaks as spaces, NUL at the end.
{
    size_t n = text.size() < size - 1 ? text.size() : size - 1;
    for (size_t i = 0; i < n; ++i)
        dst[i] = text[i] == '\n' || text[i] == '\r' ? ' ' : text[i];
    dst[n] = '\0';
}

static void updateOffset()
{
    time_t now = time(nullptr);
    struct tm tm;
    if (localtime_r(&now, &tm))
        gmtOffset = tm.tm_gmtoff;
}

static std::string clockText()
// "hh:mm:ss" now (local time).
{
    time_t now = time(nullptr);
    struct tm tm;
    char buf[16] = "";
    if (localtime_r(&now, &tm))
        strftime(buf, sizeof(buf), "%H:%M:%S", &tm);
    return buf;
}

// The handler's own output: signal-safe, no allocation, no stdio.

struct Out
{
    int fd;
    void str(const char *s)
    {
        size_t n = strlen(s);
        while (n > 0)
        {
            ssize_t w = ::write(fd, s, n);
            if (w <= 0)
                return;
            s += w;
            n -= (size_t) w;
        }
    }
    void num(unsigned long long v, int width = 0)
    {
        char buf[24];
        int i = sizeof(buf);
        buf[--i] = '\0';
        do
        {
            buf[--i] = (char) ('0' + v % 10);
            v /= 10;
            --width;
        } while (v > 0 && i > 0);
        while (width-- > 0 && i > 0)
            buf[--i] = '0';
        str(buf + i);
    }
    void hex(uintptr_t v)
    {
        char buf[2 + 2 * sizeof(v) + 1];
        int i = sizeof(buf);
        buf[--i] = '\0';
        do
        {
            buf[--i] = "0123456789abcdef"[v & 15];
            v >>= 4;
        } while (v > 0);
        buf[--i] = 'x';
        buf[--i] = '0';
        str(buf + i);
    }
};

static void civil(long long days, int &y, int &m, int &d)
// Days since 1970-01-01 → date (H. Hinnant's algorithm).
{
    days += 719468;
    long long era = (days >= 0 ? days : days - 146096) / 146097;
    unsigned doe = (unsigned) (days - era * 146097);
    unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    long long yy = (long long) yoe + era * 400;
    unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    unsigned mp = (5 * doy + 2) / 153;
    d = (int) (doy - (153 * mp + 2) / 5 + 1);
    m = (int) (mp < 10 ? mp + 3 : mp - 9);
    y = (int) (yy + (m <= 2));
}

struct Stamp
{
    int y, mo, d, h, mi, s;
};

static Stamp localStamp()
{
    long long t = (long long) time(nullptr) + gmtOffset;
    long long days = t >= 0 ? t / 86400 : (t - 86399) / 86400;
    long long secs = t - days * 86400;
    Stamp st;
    civil(days, st.y, st.mo, st.d);
    st.h = (int) (secs / 3600);
    st.mi = (int) (secs / 60 % 60);
    st.s = (int) (secs % 60);
    return st;
}

static const char *signalName(int sig)
{
    switch (sig)
    {
        case SIGSEGV: return "SIGSEGV";
        case SIGILL: return "SIGILL";
        case SIGBUS: return "SIGBUS";
        case SIGFPE: return "SIGFPE";
        case SIGABRT: return "SIGABRT";
        default: return "signal";
    }
}

static int openReport(const Stamp &st)
// ~/tvterm-crash-YYYY-MM-DD-hhmmss.txt, else the same name in /tmp.
{
    char name[64];
    // "tvterm-crash-" + date + ".txt", built by hand.
    char *p = name;
    auto put = [&] (const char *s) { while (*s) *p++ = *s++; };
    auto put2 = [&] (int v) { *p++ = (char) ('0' + v / 10 % 10); *p++ = (char) ('0' + v % 10); };
    put("/tvterm-crash-");
    put2(st.y / 100); put2(st.y); *p++ = '-'; put2(st.mo); *p++ = '-'; put2(st.d); *p++ = '-';
    put2(st.h); put2(st.mi); put2(st.s);
    put(".txt");
    *p = '\0';
    const char *dirs[] = {homeDir, "/tmp"};
    for (const char *dir : dirs)
    {
        char path[pathSize + 64];
        size_t n = strlen(dir);
        if (n == 0 || n >= pathSize)
            continue;
        memcpy(path, dir, n);
        strcpy(path + n, name);
        int fd = ::open(path, O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0600);
        if (fd >= 0)
            return fd;
    }
    return -1;
}

static void writeReport(int sig, siginfo_t *si)
{
    Stamp st = localStamp();
    int fd = openReport(st);
    if (fd < 0)
        return;
    Out o {fd};
    o.str("tvterm crash report\n\nsignal:  ");
    o.str(signalName(sig));
    o.str(" (");
    o.num((unsigned) sig);
    o.str(")");
    if (si)
    {
        // si_code <= 0: sent with kill(), raise(), abort()...; > 0: by
        // the kernel, for a faulting instruction at si_addr.
        if (si->si_code <= 0)
        {
            o.str(", sent by process ");
            o.num((unsigned long long) si->si_pid);
        }
        else
        {
            o.str(", code ");
            o.num((unsigned long long) si->si_code);
            o.str(", address ");
            o.hex((uintptr_t) si->si_addr);
        }
    }
    o.str("\ntime:    ");
    o.num((unsigned) st.y, 4); o.str("-"); o.num((unsigned) st.mo, 2); o.str("-"); o.num((unsigned) st.d, 2);
    o.str(" ");
    o.num((unsigned) st.h, 2); o.str(":"); o.num((unsigned) st.mi, 2); o.str(":"); o.num((unsigned) st.s, 2);
    o.str("\npid:     ");
    o.num((unsigned long long) getpid());
    o.str("\n\n");
    for (auto &line : info)
        if (line[0])
        {
            o.str(line);
            o.str("\n");
        }
    o.str("\nwindow:  ");
    o.str(window[0] ? window : "(none)");
    o.str("\ndialog:  ");
    o.str(dialog[0] ? dialog : "(none)");
    o.str("\n\nlast Event Viewer messages, oldest first:\n");
    {
        long total = messageCount;
        long first = total > messageSlots ? total - messageSlots : 0;
        if (total == 0)
            o.str("  (none)\n");
        for (long i = first; i < total; ++i)
        {
            o.str("  ");
            o.str(messages[i % messageSlots]);
            o.str("\n");
        }
    }
    o.str("\nlast keys, clicks and commands, oldest first:\n");
    {
        long total = inputCount;
        long first = total > inputSlots ? total - inputSlots : 0;
        if (total == 0)
            o.str("  (none)\n");
        for (long i = first; i < total; ++i)
        {
            o.str("  ");
            o.str(inputs[i % inputSlots]);
            o.str("\n");
        }
    }
    o.str("\nThe signal goes on to its default action (core file, if enabled).\n");
    ::close(fd);
}

static void handler(int sig, siginfo_t *si, void *)
{
    // Once: a second signal while writing (or another thread) goes
    // straight to the default action.
    if (!handling)
    {
        handling = 1;
        writeReport(sig, si);
    }
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_DFL;
    sigemptyset(&sa.sa_mask);
    sigaction(sig, &sa, nullptr);
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, sig);
    sigprocmask(SIG_UNBLOCK, &mask, nullptr);
    raise(sig);
    // Not reached for these signals with the default action.
    _exit(128 + sig);
}

} // namespace crashrec

namespace crashlog
{

void install()
{
    using namespace crashrec;
    const char *home = getenv("HOME");
    copy(homeDir, sizeof(homeDir), home && *home ? home : "/tmp");
    updateOffset();
    // An alternate stack, so that a stack overflow can still be reported
    // when this handler is called directly (before or after tvision's).
    stack_t ss;
    memset(&ss, 0, sizeof(ss));
    ss.ss_sp = altStack;
    ss.ss_size = sizeof(altStack);
    sigaltstack(&ss, nullptr);
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = &handler;
    sa.sa_flags = SA_SIGINFO | SA_ONSTACK;
    sigemptyset(&sa.sa_mask);
    for (int sig : signals)
        sigaction(sig, &sa, nullptr);
}

void setInfo(const std::string &text, bool loadedProfile)
{
    using namespace crashrec;
    if (loadedProfile)
        copy(info[infoSlots - 1], infoSize, text);
    else if (startInfos < infoSlots - 1)
        copy(info[startInfos++], infoSize, text);
}

void setContext(const std::string &aWindow, const std::string &aDialog)
{
    using namespace crashrec;
    copy(window, sizeof(window), aWindow);
    copy(dialog, sizeof(dialog), aDialog);
}

void addMessage(const std::string &text)
{
    using namespace crashrec;
    updateOffset();
    copy(messages[messageCount % messageSlots], messageSize, clockText() + " " + text);
    messageCount = messageCount + 1;
}

void addInput(const std::string &text)
{
    using namespace crashrec;
    copy(inputs[inputCount % inputSlots], inputSize, clockText() + " " + text);
    inputCount = inputCount + 1;
}

} // namespace crashlog
