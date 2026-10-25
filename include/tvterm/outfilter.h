#ifndef TVTERM_OUTFILTER_H
#define TVTERM_OUTFILTER_H

#include <stddef.h>
#include <string>

namespace tvterm
{

// Extracts the plain text from the data written by a client application:
// escape sequences (CSI, OSC, DCS, APC, ...) and control characters are
// removed. Line breaks, carriage returns and cursor movements become a
// single '\n', so that words from different places of the screen are not
// joined together. The state is kept between calls, so a sequence may be
// split across several calls.
class OutputTextFilter
{
public:

    // Appends the text found in 'data' to 'out'.
    void feed(const char *data, size_t size, std::string &out) noexcept;

private:

    enum State { Ground, Esc, EscIntermediate, Csi, String, StringEsc };
    State state {Ground};

    static void newLine(std::string &out) noexcept;
};

// Appends 'text' to 'buffer', keeping at most 'maxSize' bytes: when it
// would be exceeded, the oldest bytes are dropped.
void appendBounded(std::string &buffer, const char *text, size_t size, size_t maxSize) noexcept;

} // namespace tvterm

#endif // TVTERM_OUTFILTER_H
