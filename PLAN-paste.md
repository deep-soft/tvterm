# Paste Implementation Plan (v1 — upstream-master based)

## Overview
Add paste support to `tvterm` in two phases:

1. Basic paste-in: read system clipboard on the UI thread and send bytes to PTY through the existing `TerminalController` event/write pipeline.
2. Bracketed paste: when terminal DEC mode 2004 is enabled, wrap pasted data with `ESC [ 200 ~` and `ESC [ 201 ~`.

This plan is intentionally independent from `feature/text-selection` and targets upstream `magiblot/tvterm` master architecture.

## Current-State Diagnosis

### Root Cause
Paste is currently missing because all required pieces are absent:

1. No user command exists for paste (`cmPaste` is not present in [`source/tvterm/cmds.h`](/Users/james/Repos/tvterm/source/tvterm/cmds.h), [`include/tvterm/consts.h`](/Users/james/Repos/tvterm/include/tvterm/consts.h), or [`source/tvterm/wnd.cc`](/Users/james/Repos/tvterm/source/tvterm/wnd.cc)).
2. `TerminalView` forwards keyboard/mouse events to emulator but has no clipboard read path or paste command handling ([`source/tvterm-core/termview.cc`](/Users/james/Repos/tvterm/source/tvterm-core/termview.cc)).
3. `TerminalEvent` has no paste event type, and its async queue model makes pointer-based payloads from UI thread unsafe without owned storage ([`include/tvterm/termemu.h`](/Users/james/Repos/tvterm/include/tvterm/termemu.h), [`source/tvterm-core/termctrl.cc`](/Users/james/Repos/tvterm/source/tvterm-core/termctrl.cc)).
4. `VTermEmulator` handles key/mouse/PTY-read/focus/resize only; no paste action path exists ([`source/tvterm-core/vtermemu.cc`](/Users/james/Repos/tvterm/source/tvterm-core/vtermemu.cc)).

### Exact Existing Input Path (keyboard -> PTY)

1. `TerminalView::handleEvent(evKeyDown)` builds `TerminalEvent{type=KeyDown}` and calls `termCtrl.sendEvent(...)` ([`source/tvterm-core/termview.cc:69`](/Users/james/Repos/tvterm/source/tvterm-core/termview.cc:69)).
2. `TerminalController::sendEvent` pushes event into `eventQueue` and wakes writer loop ([`source/tvterm-core/termctrl.cc:139`](/Users/james/Repos/tvterm/source/tvterm-core/termctrl.cc:139)).
3. Writer loop `processEvents()` forwards event to `terminalEmulator.handleEvent(...)` under event-loop mutex ([`source/tvterm-core/termctrl.cc:225`](/Users/james/Repos/tvterm/source/tvterm-core/termctrl.cc:225)).
4. `VTermEmulator::handleEvent(KeyDown)` converts key and calls libvterm keyboard APIs ([`source/tvterm-core/vtermemu.cc:293`](/Users/james/Repos/tvterm/source/tvterm-core/vtermemu.cc:293)).
5. libvterm emits encoded bytes through output callback; `VTermEmulator::writeOutput` appends into `clientDataWriter.buffer` ([`source/tvterm-core/vtermemu.cc:389`](/Users/james/Repos/tvterm/source/tvterm-core/vtermemu.cc:389)).
6. Writer loop flushes that buffer to PTY in `writePendingData` ([`source/tvterm-core/termctrl.cc:290`](/Users/james/Repos/tvterm/source/tvterm-core/termctrl.cc:290)).

Paste must reuse this architecture and not write PTY directly from UI thread.

## Research Findings Required By Task

### 1. Sending text to PTY today
The only intentional path is:
`TerminalView` -> `TerminalController::sendEvent` -> writer loop -> `TerminalEmulator::handleEvent` -> `Writer` buffer -> `PtyMaster::writeToClient`.

### 2. How to detect bracketed paste mode 2004
In vendored libvterm:

- Mode 2004 is internal state (`state->mode.bracketpaste`) in `state.c`.
- It is **not** exposed via `VTERM_PROP_*` (`VTERM_PROP_BRACKETPASTE` does not exist in public API).
- Public keyboard helpers `vterm_keyboard_start_paste` / `vterm_keyboard_end_paste` conditionally emit markers based on that internal flag (`keyboard.c`).

Therefore, best integration is to call these helpers around pasted bytes; this is the supported API path and avoids private-header coupling.

### 3. New TerminalEvent type vs reusing KeyDown
Recommended: add a new paste event type.

Why not `KeyDown`:

- KeyDown path is key-semantic conversion, not bulk byte injection.
- Replaying paste as per-char key events is slower and can alter data semantics.
- Bracketed paste is conceptually one paste action, not N keypresses.

### 4. Clipboard read implementation (reverse of text-selection write plan)
Use platform tools from UI thread:

- macOS: `pbpaste`
- Linux fallback chain: `xclip -selection clipboard -o` then `xsel --clipboard --output`

Read process stdout into `GrowArray`, then submit that data through controller event pipeline.

### 5. UTF-8 and wide-char considerations
Paste should treat clipboard data as raw UTF-8 bytes and forward unchanged.
No TScreenCell conversion is involved (unlike copy-selection extraction). Multi-byte codepoints and combining characters remain intact by byte-preserving transfer.

### 6. Security considerations

- Paste bombing risk (very large payloads): add maximum paste size guard (e.g. 1 MiB MVP cap) and drop/trim with user-visible warning.
- Control bytes/newlines are expected terminal input; do not sanitize by default or behavior diverges from standard terminals.
- Bracketed paste reduces accidental command execution for apps that enable mode 2004.

## Design Decisions

### D1: Keep paste initiation in `TerminalView` command handling
`TerminalView` already owns focused input behavior and has access to `TerminalController`.

### D2: Add dedicated `TerminalEventType::PasteData`
Represent paste as first-class event for clean separation and future extensions.

### D3: Preserve async safety with owned paste queue in `TerminalController`
Because `TerminalEvent` is copied into async queue, clipboard pointers from UI thread cannot be queued directly.
Use an internal `std::queue<GrowArray>` for ownership. `PasteData` event acts as trigger; writer loop pops owned buffer and passes temporary pointer to emulator synchronously.

### D4: Implement bracketed paste in emulator using libvterm APIs
In `VTermEmulator::handleEvent(PasteData)`:

1. `vterm_keyboard_start_paste(vt);`
2. `clientDataWriter.write(pasteBytes);`
3. `vterm_keyboard_end_paste(vt);`

libvterm decides whether wrappers are emitted based on current mode 2004.

### D5: Command routing mirrors focused-command pattern
Add `cmPaste` to `TVTermConstants` and include it in `focusedCmds()`, so command enable/disable tracks active terminal window like existing grab/release commands.

## Fix Options And Tradeoffs

### Option A (Recommended): New PasteData event + controller-owned paste queue
Pros:
- Correct threading and ownership.
- Keeps PTY writes on existing writer-loop path.
- Clean abstraction for future paste sources.

Cons:
- Slightly more code (new queue + event struct + dispatch case).

### Option B: Reuse KeyDown path by synthesizing many key events
Pros:
- Minimal structural changes.

Cons:
- Data fidelity and performance issues for large/complex paste.
- Poor bracketed-paste semantics.
- Harder to preserve exact clipboard bytes.

### Option C: Controller direct-write API from main thread (under lock)
Pros:
- Fewer event-type changes.

Cons:
- Violates current event-centric design.
- Moves PTY-write concerns outside emulator.
- Bracketed mode handling becomes awkward or duplicated.

## Files To Modify

### 1. `include/tvterm/termemu.h`

Add new event type and payload:

```cpp
enum class TerminalEventType
{
    KeyDown,
    Mouse,
    ClientDataRead,
    PasteData,      // NEW
    ViewportResize,
    FocusChange,
};

struct PasteDataEvent
{
    const char *data;
    size_t size;
};

struct TerminalEvent
{
    TerminalEventType type;
    union
    {
        ::KeyDownEvent keyDown;
        MouseEvent mouse;
        ClientDataReadEvent clientDataRead;
        PasteDataEvent pasteData; // NEW
        ViewportResizeEvent viewportResize;
        FocusChangeEvent focusChange;
    };
};
```

### 2. `include/tvterm/termctrl.h`

Expose paste submission API:

```cpp
void sendPasteData(GrowArray data) noexcept;
```

Reason: explicit API communicates ownership transfer and prevents unsafe pointer lifetime at call sites.

### 3. `source/tvterm-core/termctrl.cc`

Add `pasteQueue` to `TerminalEventLoop`:

```cpp
Mutex<std::queue<GrowArray>> pasteQueue;
```

Implement `TerminalController::sendPasteData`:

1. Early-return on empty data.
2. Move `GrowArray` into `pasteQueue`.
3. Enqueue lightweight `TerminalEvent{type=PasteData}` via `sendEvent`.

In `TerminalEventLoop::processEvents`, special-case `PasteData`:

1. Pop one `GrowArray` from `pasteQueue` into local variable.
2. Build local event with `pasteData = {buf.data(), buf.size()}`.
3. Call `terminalEmulator.handleEvent(localEvent)` synchronously while local buffer is alive.

All this runs under event-loop mutex, matching existing emulator access rules.

### 4. `source/tvterm-core/vtermemu.cc`

Handle paste event:

```cpp
case TerminalEventType::PasteData:
    vterm_keyboard_start_paste(vt);
    clientDataWriter.write({event.pasteData.data, event.pasteData.size});
    vterm_keyboard_end_paste(vt);
    break;
```

This provides Part 1 and Part 2 simultaneously:
- Mode 2004 OFF: markers not emitted.
- Mode 2004 ON: markers emitted around payload.

### 5. `include/tvterm/termview.h`

Declare paste helpers:

```cpp
void pasteFromClipboard() noexcept;
static bool readClipboard(GrowArray &out) noexcept;
```

### 6. `source/tvterm-core/termview.cc`

Command handling:

- Add `evCommand` case for `consts.cmPaste` -> `pasteFromClipboard()` -> `clearEvent(ev)`.

Shift+Insert binding:

- In `evKeyDown`, detect `ev.keyDown.keyCode == kbShiftIns`.
- Trigger `pasteFromClipboard()` and consume event.
- Otherwise keep existing key forwarding to emulator.

Clipboard reader (`readClipboard`):

- Unix only MVP:
  - command: `pbpaste 2>/dev/null || xclip -selection clipboard -o 2>/dev/null || xsel --clipboard --output 2>/dev/null`
  - `popen(..., "r")`, read in chunks, append to `GrowArray`.
- Return false if tool unavailable/failure.

Paste sender (`pasteFromClipboard`):

1. Read clipboard into `GrowArray`.
2. Enforce size guard (e.g. `maxPasteBytes = 1 << 20` for MVP).
3. `termCtrl.sendPasteData(std::move(data));`

### 7. `include/tvterm/consts.h`

Add command constant:

```cpp
ushort cmPaste; // NEW
```

Update `focusedCmds()` range to include `cmPaste` (and keep contiguous command block).

### 8. `source/tvterm/cmds.h`

Add command ID:

```cpp
cmPaste,
```

Place near other focused terminal commands (`cmGrabInput`, `cmReleaseInput`) to preserve grouping.

### 9. `source/tvterm/wnd.cc`

Wire new constant in `TerminalWindow::appConsts` initializer in correct field order.

### 10. `source/tvterm/app.cc`

Menu wiring:

- Add `"Paste"` menu item with `cmPaste`, key `kbShiftIns`.
- Place next to `"Copy Selection"` once that feature exists; on upstream master, place near terminal input actions in the `More...` submenu.

## Implementation Order

1. Add `cmPaste` in command/constants plumbing (`cmds.h`, `consts.h`, `wnd.cc`, `app.cc`).
2. Add paste APIs to `termview.h` and controller/emulator headers.
3. Implement clipboard read + command/key trigger in `termview.cc`.
4. Implement controller-owned paste queue and `PasteData` event dispatch in `termctrl.cc`.
5. Implement `PasteData` handling with bracketed wrappers in `vtermemu.cc`.
6. Build and run manual validation scenarios below.

## Risks

1. Clipboard tool availability on Linux varies (`xclip`/`xsel` may be missing).
2. Large paste payloads can spike memory or flood PTY.
3. Event/paste queue desynchronization if marker events and data queue ever diverge.
4. Shift+Insert handling in `TerminalView` could conflict with global command dispatch if both are active.
5. Bracketed wrappers rely on libvterm internal mode tracking; regressions in vendored libvterm would affect behavior.

## Tests To Add

### Manual Functional Tests

1. Basic paste:
   - Copy `echo test` in host clipboard.
   - Trigger menu Paste.
   - Verify text appears in terminal input and executes when pressing Enter.
2. Shift+Insert:
   - Same clipboard content.
   - Press Shift+Insert in focused terminal.
   - Verify identical behavior to menu Paste.
3. Bracketed paste ON:
   - In shell: `printf '\e[?2004h'`.
   - Run `cat -v`, paste multiline text.
   - Verify wrappers (`^[[200~` ... `^[[201~`) are observed around payload.
4. Bracketed paste OFF:
   - `printf '\e[?2004l'`.
   - Paste same payload.
   - Verify wrappers are absent.
5. UTF-8 payload:
   - Clipboard contains `你好 café 🚀`.
   - Paste and verify bytes round-trip correctly in terminal app.
6. Large paste guard:
   - Paste payload larger than cap.
   - Verify predictable behavior (drop or truncate per implementation) and app remains responsive.
7. Missing clipboard backend (Linux):
   - Temporarily run without `xclip`/`xsel`.
   - Verify paste command fails gracefully (no crash).

### Concurrency/Regression Tests

1. While process emits rapid output (`yes | head -n 200000`), trigger paste repeatedly; verify no deadlock/crash.
2. Resize window during repeated paste; verify rendering and input still function.
3. Disconnect child process, then trigger paste; verify no crash (event may be ignored due disconnected PTY).

### Build Tests

1. Clean Debug build with submodules.
2. Optional second build with `-DTVTERM_USE_SYSTEM_LIBVTERM=ON -DTVTERM_USE_SYSTEM_TVISION=ON`.

## Acceptance Criteria

1. `cmPaste` exists and is wired in constants + menu.
2. Shift+Insert triggers paste in focused terminal.
3. Paste data travels through `TerminalController` event pipeline (no direct PTY write from UI thread).
4. Bracketed wrappers are emitted only when mode 2004 is enabled by child app.
5. No regressions in existing key/mouse input or terminal update flow.
