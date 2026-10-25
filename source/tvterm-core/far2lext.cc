#include "far2lext.h"

#include <string.h>
#include <stdexcept>
#include <utility>
#include <mutex>
#include <map>

namespace tvterm
{

namespace far2lext
{

// Constants from far2l's WinPort/FarTTY.h.
enum : char
{
    interactClipboard = 'c',
    interactSetCursorHeight = 'h',
    interactGetWindowMaxSize = 'w',
    interactGetColorPalette = 'p',
    interactImage = 'i',
    interactSetFKeyTitles = 'f',
    interactChooseExtraFeatures = 'x',

    clipOpen = 'o',
    clipClose = 'c',
    clipEmpty = 'e',
    clipIsAvailable = 'a',
    clipSetDataChunk = 'S',
    clipSetData = 's',
    clipGetData = 'g',
    clipGetDataId = 'i',
    clipRegisterFormat = 'r',

    imageCaps = 'c',
};

enum : uint32_t
{
    cfText = 1,         // UTF-8 text.
    cfUnicodeText = 13, // UTF-32 text (deprecated by far2l, but still handled).
};

// Maximum size of clipboard data accepted from the client.
enum : size_t { maxClipboardSize = 16 * 1024 * 1024 };

// Arguments are serialized as a stack: values are appended to the end and
// popped from the end. Integers are little-endian.
// See far2l's utils/src/StackSerializer.cpp.

static void pushBytes(std::string &stack, const void *data, size_t size)
{
    stack.append((const char *) data, size);
}

template <class T>
static void pushNum(std::string &stack, T value)
{
    unsigned char bytes[sizeof(T)];
    for (size_t i = 0; i < sizeof(T); ++i)
        bytes[i] = (unsigned char) ((uint64_t) value >> (8*i));
    pushBytes(stack, bytes, sizeof(T));
}

static std::string popBytes(std::string &stack, size_t size)
{
    if (size > stack.size())
        throw std::range_error("far2l extensions: stack underflow");
    std::string out = stack.substr(stack.size() - size);
    stack.resize(stack.size() - size);
    return out;
}

template <class T>
static T popNum(std::string &stack)
{
    std::string bytes = popBytes(stack, sizeof(T));
    uint64_t value = 0;
    for (size_t i = 0; i < sizeof(T); ++i)
        value |= (uint64_t) (unsigned char) bytes[i] << (8*i);
    return (T) value;
}

static std::string popStr(std::string &stack)
{
    uint32_t size = popNum<uint32_t>(stack);
    return popBytes(stack, size);
}

static const char base64Chars[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static std::string encodeBase64(const std::string &in)
{
    std::string out;
    out.reserve((in.size() + 2)/3*4);
    size_t i = 0;
    for (; i + 2 < in.size(); i += 3)
    {
        uint32_t n = ((unsigned char) in[i] << 16) | ((unsigned char) in[i+1] << 8) | (unsigned char) in[i+2];
        out += base64Chars[(n >> 18) & 63];
        out += base64Chars[(n >> 12) & 63];
        out += base64Chars[(n >> 6) & 63];
        out += base64Chars[n & 63];
    }
    if (i + 1 == in.size())
    {
        uint32_t n = (unsigned char) in[i] << 16;
        out += base64Chars[(n >> 18) & 63];
        out += base64Chars[(n >> 12) & 63];
        out += "==";
    }
    else if (i + 2 == in.size())
    {
        uint32_t n = ((unsigned char) in[i] << 16) | ((unsigned char) in[i+1] << 8);
        out += base64Chars[(n >> 18) & 63];
        out += base64Chars[(n >> 12) & 63];
        out += base64Chars[(n >> 6) & 63];
        out += '=';
    }
    return out;
}

static std::string decodeBase64(const char *in, size_t size)
{
    std::string out;
    out.reserve(size/4*3);
    uint32_t n = 0;
    int bits = 0;
    for (size_t i = 0; i < size; ++i)
    {
        char c = in[i];
        int v;
        if (c >= 'A' && c <= 'Z') v = c - 'A';
        else if (c >= 'a' && c <= 'z') v = c - 'a' + 26;
        else if (c >= '0' && c <= '9') v = c - '0' + 52;
        else if (c == '+') v = 62;
        else if (c == '/') v = 63;
        else continue; // '=', whitespace...
        n = (n << 6) | v;
        bits += 6;
        if (bits >= 8)
        {
            bits -= 8;
            out += (char) ((n >> bits) & 0xFF);
        }
    }
    return out;
}

static std::string utf32ToUtf8(const std::string &in)
{
    std::string out;
    for (size_t i = 0; i + 4 <= in.size(); i += 4)
    {
        uint32_t c = (unsigned char) in[i] | ((unsigned char) in[i+1] << 8) |
                     ((unsigned char) in[i+2] << 16) | ((uint32_t) (unsigned char) in[i+3] << 24);
        if (c == 0)
            break;
        if (c < 0x80)
            out += (char) c;
        else if (c < 0x800)
        {
            out += (char) (0xC0 | (c >> 6));
            out += (char) (0x80 | (c & 0x3F));
        }
        else if (c < 0x10000)
        {
            out += (char) (0xE0 | (c >> 12));
            out += (char) (0x80 | ((c >> 6) & 0x3F));
            out += (char) (0x80 | (c & 0x3F));
        }
        else if (c < 0x110000)
        {
            out += (char) (0xF0 | (c >> 18));
            out += (char) (0x80 | ((c >> 12) & 0x3F));
            out += (char) (0x80 | ((c >> 6) & 0x3F));
            out += (char) (0x80 | (c & 0x3F));
        }
    }
    return out;
}

static std::string utf8ToUtf32(TStringView in)
{
    std::string out;
    for (size_t i = 0; i < in.size();)
    {
        unsigned char b = in[i];
        uint32_t c;
        size_t len;
        if (b < 0x80) { c = b; len = 1; }
        else if ((b & 0xE0) == 0xC0) { c = b & 0x1F; len = 2; }
        else if ((b & 0xF0) == 0xE0) { c = b & 0x0F; len = 3; }
        else if ((b & 0xF8) == 0xF0) { c = b & 0x07; len = 4; }
        else { c = 0xFFFD; len = 1; }
        if (i + len > in.size())
            { c = 0xFFFD; len = in.size() - i; }
        else
            for (size_t j = 1; j < len; ++j)
                c = (c << 6) | ((unsigned char) in[i+j] & 0x3F);
        pushNum(out, c);
        i += len;
    }
    return out;
}

// Clipboard formats other than text (e.g. far2l's 'FAR_VerticalBlock_Unicode'
// marker, which tells that the text is a vertical block) cannot be stored in
// the system clipboard, so they are kept here, together with the text they
// were copied with. They are reported as available only while the clipboard
// still contains that same text (e.g. not after copying something else in
// Windows). Shared by all terminals, which run in different threads.
class ExtraFormatStore
{
    std::mutex mutex;
    std::map<std::string, uint32_t> formatIds;
    uint32_t nextFormatId {0xC000};
    std::map<uint32_t, std::string> formats; // Format ID -> data.
    std::string formatsText;  // Text that 'formats' belong to.
    std::string knownText;    // Last text known to be in the clipboard.

    static std::string normalize(TStringView text)
    {
        // Line endings may change on the way (e.g. through Windows).
        std::string out;
        for (char c : text)
            if (c != '\r')
                out += c;
        while (!out.empty() && out.back() == '\0')
            out.pop_back();
        return out;
    }

public:

    enum : size_t { maxFormatDataSize = 1024 * 1024 };

    uint32_t registerFormat(const std::string &name)
    {
        std::lock_guard<std::mutex> lock(mutex);
        auto it = formatIds.find(name);
        if (it == formatIds.end())
            it = formatIds.emplace(name, nextFormatId++).first;
        return it->second;
    }

    void empty()
    {
        std::lock_guard<std::mutex> lock(mutex);
        formats.clear();
        formatsText.clear();
    }

    // Text set by an application: extra formats set afterwards belong to it.
    void setText(TStringView text)
    {
        std::lock_guard<std::mutex> lock(mutex);
        knownText = normalize(text);
        formats.clear();
        formatsText = knownText;
    }

    // Text read from the clipboard.
    void setKnownText(TStringView text)
    {
        std::lock_guard<std::mutex> lock(mutex);
        knownText = normalize(text);
    }

    void setFormat(uint32_t format, std::string data)
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (data.size() <= maxFormatDataSize)
            formats[format] = std::move(data);
    }

    bool getFormat(uint32_t format, std::string *data = nullptr)
    {
        std::lock_guard<std::mutex> lock(mutex);
        auto it = formats.find(format);
        if (it == formats.end() || knownText != formatsText)
            return false;
        if (data)
            *data = it->second;
        return true;
    }
};

static ExtraFormatStore extraFormats;

} // namespace far2lext

using namespace far2lext;

Far2lExtensions::Far2lExtensions(Callbacks aCallbacks) noexcept :
    callbacks(std::move(aCallbacks))
{
}

bool Far2lExtensions::handleApc(const char *data, size_t size) noexcept
{
    if (size < 6 || memcmp(data, "far2l", 5) != 0)
        return false;
    switch (data[5])
    {
        case '1': // Enable extensions.
        {
            enabled = true;
            static const char ok[] = "\x1B_far2lok\x07";
            callbacks.write(ok, sizeof(ok) - 1);
            break;
        }
        case '0': // Disable extensions.
            enabled = false;
            break;
        case ':': // Request.
            if (enabled)
            {
                std::string stack = decodeBase64(data + 6, size - 6);
                handleInteract(stack);
            }
            break;
        default:
            break;
    }
    return true;
}

void Far2lExtensions::handleInteract(std::string &request) noexcept
{
    uint8_t id = 0;
    std::string reply;
    bool replyNow = true;
    try
    {
        id = popNum<uint8_t>(request);
        char command = popNum<char>(request);
        switch (command)
        {
            case interactClipboard:
                replyNow = handleClipboard(request, reply, id);
                break;

            case interactGetWindowMaxSize:
            {
                TPoint size = callbacks.getSize();
                pushNum<int16_t>(reply, size.x);
                pushNum<int16_t>(reply, size.y);
                break;
            }

            case interactGetColorPalette:
                pushNum<uint8_t>(reply, 0);  // Reserved.
                pushNum<uint8_t>(reply, 24); // Color bits.
                break;

            case interactImage:
                if (popNum<char>(request) == imageCaps)
                {
                    pushNum<uint64_t>(reply, 0); // No image support.
                    pushNum<int16_t>(reply, 0);
                    pushNum<int16_t>(reply, 0);
                }
                else
                    pushNum<uint8_t>(reply, 0); // Failure.
                break;

            case interactSetFKeyTitles:
                pushNum<uint8_t>(reply, 0); // Not supported.
                break;

            default:
                // Other requests (cursor height, notifications, extra
                // features...) are accepted and ignored. They don't return
                // any data, but must still be answered if they have an ID.
                break;
        }
    }
    catch (const std::exception &)
    {
        reply.clear();
    }
    // far2l waits forever for the answer to requests with a non-zero ID,
    // so we must always answer them (unless the answer is deferred).
    if (replyNow)
        sendReply(reply, id);
}

bool Far2lExtensions::handleClipboard(std::string &request, std::string &reply, uint8_t id)
// Returns false if the answer is deferred.
{
    char command = popNum<char>(request);
    switch (command)
    {
        case clipOpen:
            popStr(request); // Client ID. Access is not restricted here.
            pushNum<uint64_t>(reply, 0); // Supported features: none.
            pushNum<int8_t>(reply, 1);   // Success.
            break;

        case clipClose:
            pushNum<int8_t>(reply, 1);
            break;

        case clipEmpty:
            extraFormats.empty();
            pushNum<int8_t>(reply, 1);
            break;

        case clipIsAvailable:
        {
            uint32_t format = popNum<uint32_t>(request);
            bool available = format == cfText || format == cfUnicodeText ||
                             extraFormats.getFormat(format);
            pushNum<int8_t>(reply, available);
            break;
        }

        case clipSetDataChunk:
            // Not announced as supported, so the client should not send it.
            break;

        case clipSetData:
        {
            uint32_t format = popNum<uint32_t>(request);
            uint32_t size = popNum<uint32_t>(request);
            if (size <= maxClipboardSize)
            {
                std::string data = popBytes(request, size);
                if (format == cfText || format == cfUnicodeText)
                {
                    std::string text = format == cfText ? data : utf32ToUtf8(data);
                    size_t end = text.find('\0');
                    if (end != std::string::npos)
                        text.resize(end);
                    extraFormats.setText(text);
                    callbacks.setClipboard(std::move(text));
                }
                else
                    // E.g. far2l's marker for vertical blocks.
                    extraFormats.setFormat(format, std::move(data));
            }
            pushNum<int8_t>(reply, 1);
            break;
        }

        case clipGetData:
        {
            uint32_t format = popNum<uint32_t>(request);
            if ((format == cfText || format == cfUnicodeText) && id != 0)
            {
                callbacks.requestClipboard(id, format);
                return false;
            }
            std::string data;
            if (extraFormats.getFormat(format, &data))
            {
                pushBytes(reply, data.data(), data.size());
                pushNum<uint32_t>(reply, (uint32_t) data.size());
            }
            else
                pushNum<uint32_t>(reply, 0); // No data.
            break;
        }

        case clipGetDataId:
            pushNum<uint64_t>(reply, 0);
            break;

        case clipRegisterFormat:
        {
            std::string name = popStr(request);
            pushNum<uint32_t>(reply, extraFormats.registerFormat(name));
            break;
        }

        default:
            break;
    }
    return true;
}

void Far2lExtensions::sendClipboardData(uint8_t id, uint32_t format, TStringView utf8Text) noexcept
{
    extraFormats.setKnownText(utf8Text);
    std::string reply;
    try
    {
        std::string data;
        if (format == cfUnicodeText)
        {
            data = utf8ToUtf32(utf8Text);
            pushNum<uint32_t>(data, 0); // Null terminator.
        }
        else
        {
            data.assign(utf8Text.data(), utf8Text.size());
            data += '\0';
        }
        pushBytes(reply, data.data(), data.size());
        pushNum<uint32_t>(reply, (uint32_t) data.size());
    }
    catch (const std::exception &)
    {
        reply.clear();
        pushNum<uint32_t>(reply, 0);
    }
    sendReply(reply, id);
}

void Far2lExtensions::sendReply(std::string &reply, uint8_t id) noexcept
{
    if (id == 0)
        return;
    try
    {
        pushNum<uint8_t>(reply, id);
        std::string out = "\x1B_far2l";
        out += encodeBase64(reply);
        out += '\x07';
        callbacks.write(out.data(), out.size());
    }
    catch (const std::exception &)
    {
    }
}

} // namespace tvterm
