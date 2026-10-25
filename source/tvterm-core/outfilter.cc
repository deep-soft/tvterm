#include <tvterm/outfilter.h>

namespace tvterm
{

void OutputTextFilter::newLine(std::string &out) noexcept
{
    if (!out.empty() && out.back() != '\n')
        out.push_back('\n');
}

void OutputTextFilter::feed(const char *data, size_t size, std::string &out) noexcept
{
    for (size_t i = 0; i < size; ++i)
    {
        unsigned char c = (unsigned char) data[i];
        switch (state)
        {
            case Ground:
                if (c == 0x1B)
                    state = Esc;
                else if (c == '\n' || c == '\r' || c == '\v' || c == '\f')
                    newLine(out);
                else if (c == '\t')
                    out.push_back(' ');
                else if (c >= 0x20 && c != 0x7F)
                    out.push_back((char) c);
                // Other control characters (BEL, BS, ...) are ignored.
                break;

            case Esc:
                if (c == '[')
                    state = Csi;
                else if (c == ']' || c == 'P' || c == 'X' || c == '^' || c == '_')
                    state = String; // OSC, DCS, SOS, PM, APC.
                else if (c >= 0x20 && c <= 0x2F)
                    state = EscIntermediate;
                else if (c == 0x1B)
                    state = Esc;
                else
                {
                    // Two-character sequence (e.g. ESC 7, ESC M).
                    newLine(out);
                    state = Ground;
                }
                break;

            case EscIntermediate:
                if (c >= 0x30 && c <= 0x7E)
                    state = Ground;
                else if (c == 0x1B)
                    state = Esc;
                break;

            case Csi:
                if (c >= 0x40 && c <= 0x7E)
                {
                    // Colors and attributes ('m') do not separate words;
                    // cursor movements and erasures do.
                    if (c != 'm')
                        newLine(out);
                    state = Ground;
                }
                else if (c == 0x1B)
                    state = Esc;
                break;

            case String:
                if (c == 0x07)
                    state = Ground;
                else if (c == 0x1B)
                    state = StringEsc;
                break;

            case StringEsc:
                // ESC \ (String Terminator) ends the string; anything else
                // starts a new escape sequence.
                if (c == '\\')
                    state = Ground;
                else
                {
                    state = Esc;
                    --i;
                }
                break;
        }
    }
}

void appendBounded(std::string &buffer, const char *text, size_t size, size_t maxSize) noexcept
{
    if (size >= maxSize)
    {
        buffer.assign(text + (size - maxSize), maxSize);
        return;
    }
    if (buffer.size() + size > maxSize)
        buffer.erase(0, buffer.size() + size - maxSize);
    buffer.append(text, size);
}

} // namespace tvterm
