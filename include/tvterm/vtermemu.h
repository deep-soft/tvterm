#ifndef TVTERM_VTERMEMU_H
#define TVTERM_VTERMEMU_H

#include <tvterm/termemu.h>
#include <tvterm/outfilter.h>
#include <utility>
#include <memory>
#include <deque>

#include <vterm.h>

namespace tvterm
{

class Far2lExtensions;

class VTermEmulatorFactory final : public TerminalEmulatorFactory
{
public:

    TerminalEmulator &create(TPoint size, Writer &clientDataWriter) noexcept override;
    TSpan<const EnvironmentVar> getCustomEnvironment() noexcept override;

};

class VTermEmulator final : public TerminalEmulator
{
public:

    VTermEmulator(TPoint size, Writer &aClientDataWriter) noexcept;
    ~VTermEmulator();

    void handleEvent(const TerminalEvent &event) noexcept override;
    void updateState(TerminalState &state) noexcept override;
    void getText( TextSnapshot &snapshot, int startRow, int endRow,
                  bool withPositions ) noexcept override;
    void getSelectionText(SelectionRange selection, std::string &text) noexcept override;

private:

    // The scrollback consists of a list of lines that were pushed out of the
    // terminal area (via the 'sb_pushline4' callback) and that, therefore,
    // can never again be updated by the client application.
    // Scrollback lines preserve their original width even if the terminal
    // gets resized.
    struct Scrollback
    {
        // The maximum number of lines is 'terminalOptions.scrollbackLines'.

        struct Line
        {
            std::unique_ptr<const VTermScreenCell[]> cells;
            int cols;
            bool continuation; // is 'lines[i]' the continuation of 'lines[i - 1]'?
        };

        std::deque<Line> lines;
        int offset {0}; // 0 (oldest line) to lines.size() - 1 (newest line), or lines.size() (no scrollback).
        uint64_t dropped {0}; // Lines removed from the front so far.

        bool pushLine(int cols, const VTermScreenCell *cells, bool continuation);
        bool trim(int maxLines); // Returns whether lines were removed.
        void incrementOffset(int count);
        void setOffset(int newOffset);
        void followTerminal();
        bool isFollowingTerminal() const;
        int getVisibleScrollLines(int terminalHeight) const;
        int numLines() const;
    };

    struct LocalState
    {
        bool cursorChanged {false};
        TPoint cursorPos {0, 0};
        bool cursorVisible {false};
        bool cursorBlink {false};

        bool titleChanged {false};
        GrowArray title;

        bool mouseEnabled {false};
        bool altScreenEnabled {false};

        int scrollOffset {0};
        int visibleScrollLines {0};
        bool scrollOverflowed {false};

        bool win32InputEnabled {false};

        bool clipboardChanged {false};
        GrowArray clipboardText;

        std::vector<Far2lClipboardRequest> far2lClipboardRequests;

        uint64_t outputBytes {0};
        std::string newOutputText;
    };

    OutputTextFilter outputFilter;

    // Maximum size of an APC string (far2l extensions requests).
    enum { maxApcSize = 32 * 1024 * 1024 };

    // Maximum amount of text accepted via OSC 52, to protect against
    // misbehaving applications.
    enum { maxClipboardTextSize = 4 * 1024 * 1024 };

    struct VTerm *vt;
    struct VTermState *vtState;
    struct VTermScreen *vtScreen;
    Writer &clientDataWriter;
    // Keeps track of the screen regions that were updated in VTerm.
    std::vector<RowRange> damageByRow;
    GrowArray strFragBuf;
    GrowArray selectionBuf; // OSC 52 text being received.
    bool selectionTooLarge {false};
    char selectionDecodeBuf[4096]; // Used by libvterm to decode base64.
    GrowArray apcBuf; // APC string being received.
    bool apcTooLarge {false};
    std::unique_ptr<Far2lExtensions> far2l;
    LocalState localState;
    Scrollback scrollback;

    static const VTermScreenCallbacks callbacks;
    static const VTermSelectionCallbacks selectionCallbacks;
    static const VTermStateFallbacks stateFallbacks;

    TPoint getSize() noexcept;
    void setSize(TPoint size) noexcept;
    void updateSurface(TerminalSurface &surface) noexcept;
    void handleScrollbackWheel(uchar wheel) noexcept;
    void drawScrollbackLine(TerminalSurface &surface, int y) noexcept;

    void writeOutput(const char *data, size_t size);
    int damage(VTermRect rect);
    int moverect(VTermRect dest, VTermRect src);
    int movecursor(VTermPos pos, VTermPos oldpos, int visible);
    int settermprop(VTermProp prop, VTermValue *val);
    int bell();
    int resize(int rows, int cols);
    int sb_pushline4(int cols, const VTermScreenCell *cells, bool continuation);
    int selection_set(VTermSelectionMask mask, VTermStringFragment frag);
    int selection_query(VTermSelectionMask mask);
    int apc(VTermStringFragment frag);

    VTermScreenCell getDefaultCell() const;
    void copySelection(SelectionRange selection) noexcept;
    void pasteText(const char *data, size_t size) noexcept;
    void extractScrollbackLineText(int y, int startX, int endX, GrowArray &text, int &padding) noexcept;
    void extractTerminalLineText(int y, int startX, int endX, GrowArray &text, int &padding) noexcept;
    void extractSelectionText(SelectionRange selection, GrowArray &text) noexcept;
    bool isContinuationRow(int absY) noexcept;
};

} // namespace tvterm

#endif // TVTERM_VTERMEMU_H
