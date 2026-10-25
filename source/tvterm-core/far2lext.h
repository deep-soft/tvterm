#ifndef TVTERM_FAR2LEXT_H
#define TVTERM_FAR2LEXT_H

// Support for far2l's terminal extensions ("TTY extensions"), clipboard part.
//
// When far2l runs inside tvterm, it sends '\e_far2l1\e\\' to detect whether
// the terminal supports its extensions. If we answer, far2l uses requests
// like '\e_far2l:<base64>\a' to access the clipboard, so that its usual
// copy/paste shortcuts work with the system clipboard (which, with tvterm
// running in far2l/putty4far2l over SSH, is the Windows clipboard).
//
// Protocol reference: far2l's WinPort/FarTTY.h and far2l/src/vt/VTFar2lExtensios.cpp.
//
// Everything in this class runs in the terminal's worker thread.

#include <stdint.h>
#include <string>
#include <functional>

#include <tvision/tv.h>

namespace tvterm
{

class Far2lExtensions
{
public:

    struct Callbacks
    {
        // Sends data to the client application (far2l).
        std::function<void (const char *data, size_t size)> write;
        // Puts UTF-8 text in the system clipboard (asynchronously).
        std::function<void (std::string text)> setClipboard;
        // Asks for the contents of the system clipboard. The answer must be
        // delivered later with 'sendClipboardData(requestId, format, text)'.
        std::function<void (uint8_t requestId, uint32_t format)> requestClipboard;
        // Returns the current terminal size.
        std::function<TPoint ()> getSize;
    };

    explicit Far2lExtensions(Callbacks aCallbacks) noexcept;

    // Handles the contents of an APC string ('\e_' ... ST).
    // Returns whether it was a far2l sequence.
    bool handleApc(const char *data, size_t size) noexcept;

    // Answers a pending clipboard read request with UTF-8 text.
    void sendClipboardData(uint8_t requestId, uint32_t format, TStringView utf8Text) noexcept;

    bool isEnabled() const noexcept { return enabled; }

private:

    Callbacks callbacks;
    bool enabled {false};

    void handleInteract(std::string &stack) noexcept;
    bool handleClipboard(std::string &request, std::string &reply, uint8_t id);
    void sendReply(std::string &reply, uint8_t id) noexcept;
};

} // namespace tvterm

#endif // TVTERM_FAR2LEXT_H
