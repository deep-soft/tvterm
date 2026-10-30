# HISTORY — tvterm (deep-soft)

Development history of the deep-soft/tvterm fork: what was implemented, how it works,
how it is tested and which pitfalls were found. Reference document, updated with each new
feature. Romanian version: [HISTORY-ro.md](HISTORY-ro.md); both are kept in sync.

Typical setup of this fork's main user: `Far (Windows) → putty4far2l → far2l (on the server) → tvterm → bash / far2l / mc`.
The outer far2l restores the session when SSH drops; tvterm runs inside it.

## 1. Repository and branches

| Branch | Role |
|---|---|
| `far2l-integration` | **main, stable branch** (on top of `magiblot/master` 210eb23) |
| `master` | up-to-date copy of magiblot's master |

**Workflow:** each new feature goes in a `feat/<name>` branch created from `far2l-integration`;
after testing: `git checkout far2l-integration && git merge --ff-only feat/<name> && git push`.
One feature at a time (most of them touch the same files, e.g. the menu in `app.cc`).

### Commits on `far2l-integration` (oldest first)

1. `c3247bb` Add win32-input-mode support (DEC mode 9001, used by far2l)
2. `90abb42` Add TVTERM_STATIC option to build a fully static executable
3. `5eb9028` Add Event Viewer window
4. `13c43c9` Add simple text editor window
5. `45f1bb1` Add scripts/build.sh for dynamic and static builds
6. `c87c8c6` Forward OSC 52 clipboard requests to the system clipboard (+ `scripts/tvterm-copy`)
7. `0434a9b` Paste from the system clipboard with Shift-Ins
8. `01193a6` Add optional debug log for OSC 52 clipboard requests
9. `0e42052` Support far2l terminal extensions (clipboard part)
10. `9c389d6` Send pasted text at once, with bracketed paste when enabled
11. `14fdfd9` Keep far2l's vertical block marker in the clipboard
12. `ea029c3` Allow starting a terminal with a command, a working directory and a fixed title
13. `ccb85ea` Add startup profiles
14. `4e53617` Expose information about the processes running in a terminal
15. `55bd9bd` Add window list, terminal renaming and saving profiles
16. `77ad928` Save and restore editors and the event viewer in profiles
17. `73c7216` Add docs/HISTORY.md
18. Split HISTORY into English (`HISTORY.md`) and Romanian (`HISTORY-ro.md`)
19. Show what runs in each terminal when quitting
20. Pass Ctrl+\ [ ] _ and the keypad operators to programs
21. Fix the doubled path in the profile file dialogs
22. Fix overlapping memcpy in tvision's input line history
23. Paste and copy in the text fields of dialogs
24. Keep profile positions inside the desktop
25. Tile and Cascade leave the Event Viewer and editors in place
26. Tile and Cascade use the largest area beside the fixed windows
27. Ask whether to close the open windows when loading a profile
28. Keep the scrollback following new output
29. Complete directory names in the Name field of file dialogs
30. Link the whole libpthread in static builds with glibc older than 2.34
31. Quit dialog: resizable, zoomable, size kept while tvterm runs

---

## 2. Features (all tested by the user in the real setup)

### Menu (Ctrl-B)
`N` New Term · `W` Close · `Tab`/`Shift-Tab` Next/Prev · `J` Window List · `T` Rename Term · `R` Resize/Move ·
`F` Maximize · `S` Select Text · `P` Paste · `E` New Editor · `O` Open File in Editor · `V` Event Viewer ·
`L` Load Profile · `A` Save Profile As · `More...` (change dir, tile, cascade, Grab Input) · `U` Suspend · `Q` Exit.
- **Tile / Cascade** (commit 25): arrange only the terminals (and other tileable windows); the Event Viewer and the editor windows
  stay where they are. The others use the largest rectangle that overlaps none of them, each fixed window counted separately
  (commit 26; commit 25 used the rectangle enclosing all of them, which with an editor at the top left and the Event Viewer on
  the right covered the whole desktop → the terminals were arranged over both). No fixed window open, or the largest free
  rectangle under 20 columns × 6 rows → the whole desktop, as before. An L-shaped free space is not split into several areas.
  Also for the `layout` of profiles. Code: `TVTermDesk::freeArea` (`desk.cc`; `noFreeArea` tells the caller the whole desktop
  was used).
Free menu letters: B, C, D, G, H, I, K, M, X, Y, Z (some are used inside More).

### Clipboard (main goal; works in every direction)
- **tvterm editor** (Ctrl-B E): Ctrl-Ins / Shift-Ins / Shift-Del use the system clipboard (= the Windows clipboard through far2l/putty4far2l). F2 saves.
- **OSC 52**: programs inside tvterm (vim, tmux, scripts) can write to the clipboard; reading is refused on purpose (security).
  `scripts/tvterm-copy`: `command | tvterm-copy`.
- **far2l extensions (clipboard)**: tvterm answers the `ESC _far2l1` handshake; far2l inside tvterm shows `TTY|F`
  and has full access to the Windows clipboard (Ctrl+C/V, Ctrl+Ins/Shift+Ins).
  - Permission in the outer far2l: "Share clipboard always".
  - far2l vertical blocks are preserved (the `FAR_VerticalBlock_Unicode` marker is kept by tvterm, tied to the text).
- **Shift-Ins** in a terminal: pastes through tvterm **only** if the program does not use far2l's extensions, and not in Grab Input mode.
- **Bracketed paste**: pasted text is sent at once, enclosed in `ESC[200~ … ESC[201~` if the program asks for it; control characters (ESC) are removed.
- **Text fields in dialogs** (Rename, Keywords, file names, …; commit 23): Shift-Ins / Ctrl-V paste from the system clipboard,
  Ctrl-Ins / Ctrl-C copy the selected text (without a selection, Ctrl-C does nothing, as before). Ctrl-V no longer toggles
  the overwrite mode (Ins still does). Pasted text (also from the outer terminal, e.g. PuTTY's right click) is cleaned up:
  valid UTF-8 only; line breaks, tabs and runs of spaces → one space; spaces at both ends removed; control characters (ESC, NUL, C1),
  bidirectional and zero-width characters removed; only the first 64 KiB are looked at; cut at the field's length on a character
  boundary. Before, the field used tvision's internal clipboard (empty on a server) and nothing was pasted.
- **Debug log**: `TVTERM_CLIPBOARD_LOG=/tmp/tvterm-clip.log tvterm`.

### Profiles
- `tvterm -p NAME` → `~/.config/tvterm/profiles/NAME.ini` (or `$XDG_CONFIG_HOME/...`); `tvterm -p /path/file.ini`; `tvterm --help`.
- Sections: `[profile]` (`layout` = auto|manual|tile-columns|tile-rows|cascade, `focus` = 1-based index),
  `[terminal]` (`title`, `dir`, `command`, `keep-open` yes/no, `pos`), `[editor]` (`file`, `pos`), `[eventviewer]` (`pos`).
- `pos = x y width height` in **percent** of the desktop, decimals allowed. Save Profile writes positions inside the desktop
  (a window moved partly off the screen is saved as moved back inside; rounded to one decimal so that x + width ≤ 100);
  when loading, a position outside the desktop is moved inside it (and shrunk if larger) instead of being rejected (commit 24;
  before: `invalid pos '84.9 0 15.4 72.7'`). The adjustment is kept in `Profile::notes`.
- `keep-open = yes` (default): when the command ends or is stopped with Ctrl+C, a shell starts; `no`: the terminal stays "(Disconnected)" with its output visible.
- Commands that run forever (`watch`, `tail -f`) must be stopped with Ctrl+C for the shell to appear.
- `mailq` may not be in the PATH of non-root users → `/usr/sbin/mailq` or `postqueue -p`.
- **Save Profile As** (Ctrl-B A): saves all windows (stacking order, focus, fixed title, positions, actual current directory from `/proc/<pid>/cwd`,
  the command from the profile, or the foreground command found with `tcgetpgrp` + `/proc/<pgid>/cmdline`).
  The text of "Untitled" editors is not saved (the user is warned).
- Load Profile / Save Profile As list the profiles directory with the wildcard `*.ini`; `..\` and directories work normally,
  and a typed name is relative to the directory shown (commit 21; before, the full path was doubled: "Invalid drive or directory").
- Load Profile with windows open (commit 27): "Close the current windows first?" Yes / No / Cancel. Yes = the terminals are
  listed as when quitting (title "Close windows", button Close, Go to), editors with unsaved changes ask to save (Cancel stops the
  load), then all windows close and the profile opens; No = the profile's windows are added to the open ones (as before);
  Cancel = nothing is loaded. `confirmQuit(deskTop, closing)` in `quitdlg.cc`, `TVTermApp::closeWindowsBeforeLoad` in `app.cc`.
- Full documentation: `examples/profiles/example.ini`.

### Other
- **Window List** (Ctrl-B J): Go / Rename / Close. **Rename Term** (Ctrl-B T); an empty title restores the automatic one.
- **Event Viewer** (Ctrl-B V): shows Turbo Vision events (key diagnostics); pressing it again pauses it.
- **Quitting** (Ctrl-B Q): the confirmation lists every open terminal and what runs in it, terminals running a program first:
  the foreground command (e.g. `tail -f /var/log/mail.log`, pipelines as `a | b`, `bash script.sh`), `bash, idle` for an idle shell,
  `[stays in background]` for far2l. Buttons: **Quit** (Enter or Q), **Go to** (G, Space or double click: goes to the chosen
  terminal and does not quit), Cancel (Esc). Editors with unsaved changes still ask afterwards.
  Programs get SIGHUP and stop; **far2l stays in the background** and can be reattached (verified).
  How the command is found: the processes of the terminal's foreground process group (`tcgetpgrp`) are read from `/proc/<pid>/stat`;
  shells that only run a command line (`sh -c ...`, e.g. the wrapper used for profile commands) are skipped; far2l rewrites its
  `argv[0]`, so its name is taken from `/proc/<pid>/stat`.
  Size (commit 31; also the "Close windows" variant): resized with the mouse (bottom right corner), moved by the title,
  **Ctrl-F5** = resize/move with the keyboard (arrows move, Shift+arrows resize, Enter/Esc end), **F5** or the zoom button =
  whole screen and back. The list grows with the dialog, the buttons stay at the bottom, centered; minimum 44 × 12 (or the
  screen). It cannot leave the screen (`dmLimitAll`); when the screen shrinks while it is open, it is moved inside, then
  shrunk (zoomed: it follows the screen). Initial size: every line whole (at least 76 columns) and one line per terminal,
  never larger than the screen. A size chosen by the user (mouse or Ctrl-F5) and the zoomed state are kept while tvterm
  runs (not saved); the dialog always opens centered; on a smaller screen it opens shrunk, the kept size stays; automatic
  adjustments do not change the kept size. Lines cut by the width end with `…`. Code: `QuitDialog` in `quitdlg.cc`
  (`calcBounds`, `changeBounds`, `zoom`, `execute`, `RememberedSize`).
- Alt+Shift+Up/Down (far2l vertical selection) do not arrive even outside tvterm → an issue in putty4far2l/Windows, not in tvterm.
  Workaround: Alt+Shift+Left/Right + Alt+Shift+PgUp/PgDn. Later (test of E20 on `far2l-extended`, 2026-10-30): Alt+Shift+Up/Down
  do arrive at tvterm through putty4far2l (used as global keys); the note above holds at most for far2l outside tvterm.
- **Ctrl+\ [ ] _ and the keypad operators**: through far2l's extensions they arrive as control characters (Ctrl+\ =
  `0x1C`, often with the scan code: keyCode `0x2B1C`), which Turbo Vision does not turn into a key. tvterm finds the key by
  scan code (or by the control character) and sends far2l (win32-input-mode) the right VK (VK_OEM_5/4/6, VK_OEM_MINUS)
  with the control character, as Windows does, and other programs the control byte (`0x1C`, `0x1B`, `0x1D`, `0x1F`).
  Keypad `* + - /` (scan codes 0x37, 0x4E, 0x4A, 0x35 + enhanced) get VK_MULTIPLY/ADD/SUBTRACT/DIVIDE (they had `vk = 0`).
  In far2l: Ctrl+\ goes to `/`, Ctrl+[ / Ctrl+] insert the path of the left/right panel.
- **Scrollback follows new output** (28): a terminal with scrollback could stop following its output and stay fixed higher up
  (new lines added below, the scroll bar showing more lines underneath; back only with End or a key). Cause: a race between the
  terminal thread and the UI thread. When the view updated its own scroll bar (`setParams`), Turbo Vision broadcast
  `cmScrollBarChanged` as if the user had moved it, and tvterm sent the bar's value back to the emulator; by then the terminal
  thread could have added lines, so the old value no longer meant "at the bottom". Seen with bursts of output (`tail -f`, alerts),
  focused or not, while the scrollback was not yet full. Now the broadcast caused by `updateScrollBar` is ignored (flag
  `updatingScrollBar`), and moving the scroll bar to the very bottom (click, drag, wheel) follows the output again.
  The same code exists in magiblot `master` (candidate for the PR).
- **Directory completion in file dialogs** (29): in the Name field of Open file, Save file as, Load Profile and Save Profile As,
  a list of the matching directories opens by itself below the field while a path is typed: the subdirectories of the part
  before the last `/` whose names begin with the text after it (`/var/` + `l` → `lib/`, `local/`, `lock/`, `log/`). Only
  directories; hidden ones only after a typed `.`; names compared case sensitively; links to directories included. A path
  without `/` is taken from the directory shown in the dialog, `~/` from `$HOME`; no list for an empty field or `*` / `?` after
  the last `/`. Keys: Down (list open) moves into the list; Up/Down/PgUp/PgDn/Home/End move in it; Enter (or a double click)
  adds the directory with `/` to the field and lists its subdirectories; Up on the first row goes back to the field; Esc closes
  the list (a second Esc closes the dialog); Tab closes it and moves to the next field; any other key goes back to the field.
  With the list closed everything works as before (Down opens the field's history, Enter opens/changes directory); the list of
  files below does not change while typing. The cursor stays in the field also while moving in the list, so the field's history
  records only what it recorded before (moving the focus into the list had added every intermediate text). At most 10 rows,
  with a scroll bar when there are more. Text pasted into the field (commit 23) also refreshes the list. `TVTermFileDialog`
  (`dircompl.cc`) replaces `TFileDialog` in tvterm; tvision is not changed.

---

## 3. Architecture and key files

```
source/tvterm-core/        (the library; potentially interesting for upstream)
  vtermemu.cc              emulator (worker thread): libvterm, win32-input, OSC 52, far2l APC, paste
  far2lext.cc/.h           far2l extensions protocol (clipboard), ExtraFormatStore (vertical blocks)
  termview.cc              main thread: clipboard (TClipboard, asynchronous read with 3 s timeout), Shift-Ins, paste
  termwnd.cc               terminal window: fixed title (setCustomTitle)
  termctrl.cc              controller (reader/writer threads), getClientProcessInfo
  pty.cc                   forkpty, ClientOptions (command/dir/keepOpen; /bin/sh wrapper with `trap : INT`)
include/tvterm/            public headers (termemu.h: TerminalState, TerminalEvent; pty.h; termctrl.h; ...)
source/tvterm/             the application
  app.cc/.h                menu, status line, commands, profiles (open/save), main() with -p/--help
  profile.cc/.h            INI parser/writer (WindowProfile, Profile)
  winlist.cc/.h            Window List dialog, renameTerminal, getDeskTopWindows
  quitdlg.cc/.h            quit confirmation dialog (confirmQuit)
  procinfo.cc/.h           /proc helpers: readLink, readCommandLine, describeForeground (what runs in a terminal)
  editwnd.cc/.h            editor (Save As dialogs etc.)
  evntview.cc/.h           Event Viewer
  dlgclip.cc/.h            clipboard in dialog text fields (focused TInputLine; called from TVTermApp::getEvent)
  dircompl.cc/.h           directory completion in the Name field of file dialogs (TVTermFileDialog, dircompl::Completion)
  wnd.h                    TerminalWindow (+ launchCommand/launchDir/launchKeepOpen)
deps/vterm/patches/0001-win32-input-mode.patch   applied by CMake to a copy (the submodule stays clean)
deps/tvision-fixes.cmake   tvision fixes: histlist.cpp copied to the build directory, memcpy → memmove (submodule clean)
deps/static-pthread.cmake  static builds: whole libpthread with glibc older than 2.34 (std::thread)
scripts/build.sh           dynamic/static build → bin/tvterm-dynamic, bin/tvterm-static (+ logs)
scripts/tvterm-copy        copy to the clipboard with OSC 52
examples/profiles/example.ini
```

**Important flows:**
- Worker thread → main thread through `TerminalState` (under a mutex), picked up on the `cmCheckTerminalUpdates` broadcast (from `idle`).
- Main thread → worker thread through `TerminalEvent` (`termCtrl.sendEvent`); added types: `Far2lClipboardData`, `Paste`
  (data allocated with `new char[]`, freed by the receiver).
- far2l **waits forever** for the answer to requests with ID ≠ 0 → tvterm always answers (an empty answer on timeout).

---

## 4. Build and workflow

- Build: `scripts/build.sh [dynamic|static|both]`; static = `cmake -DTVTERM_STATIC=ON`.
  Alpine packages: `build-base cmake perl patch git linux-headers ncurses-dev ncurses-static gpm-dev`.
- **Delivery:** patches made with `git format-patch` on top of the state on GitHub;
  applied with `git am ../x.patch && scripts/build.sh static && git push`. If a patch is already applied: `git am --skip`.
- Commit author: `deep-soft <deep-soft@users.noreply.github.com>`.
- **Static builds and old systems**: the static executable contains the ncurses of the build machine, which reads the
  terminal description (terminfo, e.g. `xterm-256color`) at startup. Alpine's (musl) ncurses rejects old terminfo files
  (e.g. the 2011 one of Ubuntu 12, CentOS 7.9): the file is found and opened, then `newterm` fails ("Cannot initialize
  Ncurses"). Two verified solutions: (1) build on Ubuntu 24 (glibc + ncurses 6.4), which works on Ubuntu 12 and CentOS 7.9;
  (2) copy a new `xterm-256color` (e.g. from Alpine) to `~/.terminfo/x/` (one user) or `/etc/terminfo/x/` (all users).
  The system's old programs (ncurses 5.9) skip the new-format file and keep using theirs (`top`, `mc` verified).
- **QNAP (QTS, x64)**: the static Alpine build started monochrome. QTS sets `TERMINFO=/opt/share/terminfo` (a directory that
  does not exist without Entware; the shell sees this value even if `.profile` says `/usr/share/terminfo`), and the only
  `xterm` entry on QTS is from 2004 (`/usr/share/terminfo/x/xterm`; no `infocmp` / `tput`). Verified solution, local to the
  tvterm folder: Alpine's `xterm-256color` in `<tvterm folder>/terminfo/x/` and a start script `tvt.sh` (`#!/bin/sh`,
  `D` = the script's folder, `TERMINFO="$D/terminfo"`, `TERM=xterm-256color`, `exec "$D/tvterm"`). `/etc` is not touched
  (QTS restores it at reboot). `tvterm`, `mc`, `top` in color.
- **Static builds with glibc older than 2.34 (commit 30):** on Raspbian 10 (Raspberry Pi 3, `armv7l`, glibc 2.28)
  `tvterm-static` stopped right at startup (exit code 134, "terminate called without an active exception", then
  "recursively"); `--version` and the dynamic build worked. Cause: before 2.34, glibc keeps the thread functions in a
  separate `libpthread.a`; a static link takes only the objects referenced directly, and `std::thread` (`termctrl.cc`) fails.
  `deps/static-pthread.cmake` (static builds only) detects glibc < 2.34 and links the whole archive
  (`-Wl,--whole-archive -lpthread -Wl,--no-whole-archive`; configure message: "glibc older than 2.34: linking the whole
  libpthread"). musl and glibc ≥ 2.34 (libpthread merged into libc) are unchanged. If the option is already in
  `CMAKE_EXE_LINKER_FLAGS` (the manual workaround), it is not added again: twice would define every pthread symbol twice.
  `-DTVTERM_OLD_GLIBC=ON/OFF` overrides the check. The workaround was verified by the user before the commit (manual build;
  far2l 2.8.0 runs inside it).
- **"Illegal instruction" in file dialogs (Alpine, commit 22):** tvision's input line history (`histlist.cpp`: `deleteString`,
  `insertString`) moved entries with `memcpy` over overlapping memory (the DOS original used `movmem`). With `_FORTIFY_SOURCE`
  on musl (fortify-headers), an overlapping `memcpy` stops the program with `ud2` = "Illegal instruction"; glibc does not check.
  Trigger: a name chosen again in a dialog field with history after a longer one (e.g. Load Profile `a1.ini`, `longername.ini`,
  `a1.ini`), or a full history (1024 bytes). The history is empty at startup, so the crash looked random.
  `deps/tvision-fixes.cmake` uses a copy of `histlist.cpp` with `memmove` (configure message: "Applied tvision fix");
  if upstream changes the file, the original is used. Not applied with `TVTERM_USE_SYSTEM_TVISION`.

### Known pitfalls
- Upstream's `.gitignore` ignores **everything** except explicitly allowed directories; files without an extension must be allowed too
  (e.g. `!scripts/*`). Allowed directories added: `scripts/`, `examples/`, `docs/`.
- Sources are found with `GLOB` → after adding a new `.cc` file, run `cmake` again.
- Unity build: `static` functions from different files can clash → own namespaces (`far2lext`, `far2lclip`).
- `TFileDialog`: its `directory` field starts empty → typed names are relative to the current directory; tvterm sets it to the profiles directory.
  The wildcard must be a bare pattern (`*.ini`): tvision joins it to the directory (bottom pane, Name field for a focused directory).
  A full path there is doubled → the dialog is created with `fdNoLoadDir`, then `directory` is set and the list read (`profiledlg::newDialog`, `app.cc`).
- win32-input code is guarded by `HAVE_VTERM_WIN32INPUT` (only with the bundled, patched libvterm).

---

## 5. Automated testing (in Claude's container)

- Clone: `git clone -b far2l-integration --recurse-submodules https://github.com/deep-soft/tvterm.git`
  + `git remote add up https://github.com/magiblot/tvterm.git`.
- Ubuntu container dependencies: `apt-get install libncurses-dev libgpm-dev`, `pip install cmake pyte --break-system-packages`;
  far2l: `apt-get update && apt-get install --no-install-recommends far2l` (2.6.0, has OSC52 and the extensions).
- Harness: Python `pty.fork()` + `pyte` (virtual screen); env `TERM=xterm-256color SHELL=/bin/sh XDG_CONFIG_HOME=/tmp/cfg`.
  - OSC 52 sent by tvterm to the outer terminal is checked in the raw output (`ESC]52;...`).
  - Paste from the outer terminal: `ESC[200~text ESC[201~`. Keys: Ctrl+Ins `ESC[2;5~`, Shift+Ins `ESC[2;2~`, Alt+Shift+arrows `ESC[1;4A-D`.
  - **Text + Enter sent in the same chunk are treated as a paste** by tvision (Enter does not confirm the dialog) → send them separately.
  - pyte may raise exceptions when tvterm exits → wrap `st.feed` in try/except.
  - far2l leaves sessions in the background (`/tmp/far2l_<uid>_*`) → `pkill -9 far2l; rm -rf /tmp/far2l_0_0` between tests.
  - far2l 2.6 in extensions mode: its title contains `TTY|F`.
- Clipboard without far2l: a fake `xclip` in `PATH` (reads/writes a file) + `DISPLAY=:99` → tvision uses it for
  TClipboard (dialog fields: Shift-Ins, Ctrl-V, Ctrl-Ins, Ctrl-C). `tmux paste-buffer -p` = bracketed paste from the outer terminal.
- Regression tests used: OSC 52 (simple, 150 KB, query ignored, invalid), Shift-Ins in a shell, native far2l paste with diacritics,
  far2l vertical block, bracketed paste in bash, profiles (positions, Ctrl+C → shell), save/reload profile, quit with an unsaved editor,
  quit dialog (profile commands, pipeline, typed command, script, far2l, Go to, Esc, 80x24 screen).
- tmux harness notes (28): in a shell inside tvterm, `tmux send-keys Enter` may not run the command → send `C-j`.
  `tmux send-keys` cannot deliver mouse events to tvterm; writing SGR sequences (`ESC[<0;x;yM` / `...m`, wheel `64`/`65`)
  into the pane's tty with `ioctl(TIOCSTI)` works (`#{pane_tty}`; x/y 1-based). Scrollback race test: a script printing
  bursts of lines with small delays, then a marker; the marker must be visible at the end (the base build got stuck 7/7).
- Directory completion (29): `dircompl::matches` checked by a small C++ test linked with `dircompl.cc` and `libtvision.a`
  (prefix, case, hidden, relative / `~/` / absolute paths, links, wildcards, limit). In tmux, characters typed in one
  `send-keys` arrive together and count as a paste; the first burst into a field whose text is selected (e.g. `*` when the
  dialog opens) keeps only the first character, also before 29 → characters are sent one at a time. The field's history
  (Down with the list closed) must not contain intermediate texts.
- Static libpthread (30): `deps/static-pthread.cmake` tried alone in a small CMake project: gcc (glibc 2.39) and `musl-gcc`
  → off; a fake `features.h` with glibc 2.28 / 2.33 → on, 2.34 / 3.0 → off; option already in `CMAKE_EXE_LINKER_FLAGS` → not
  added twice; `-DTVTERM_OLD_GLIBC=ON/OFF` respected. tvterm's static link command on glibc 2.39 is identical to the one
  before 30; forced with `-DTVTERM_OLD_GLIBC=ON` on glibc 2.39, the static build links and starts. Real test: Raspberry Pi (user).
- Quit dialog (31), in tmux (keys with `send-keys`: `C-F5`, `S-Left`, `F5`; screen size with `resize-window`; mouse with SGR
  sequences through `TIOCSTI`), the dialog's frame found in `capture-pane`: initial width from the longest line (150-column
  screen) and 76 columns otherwise; Ctrl-F5 + Shift+arrows (minimum height 12, `…` on cut lines, buttons centered); kept size
  after Esc and reopening; arrows and Shift+arrows pushed past the edges stay inside; mouse: corner dragged 20 columns left,
  title dragged past the right edge (inside), size kept; screen 150×40 → 70×20 while open (moved and shrunk to 70) → back
  (kept size used at the next opening); F5 zoom, reopened zoomed, zoomed dialog follows a smaller screen, F5 restores a
  size that fits; screen 40×11 (smaller than the minimum: whole screen, as before); Go to and Quit unchanged.

---

## 6. Known limitations

- The vertical block marker does not survive a trip through Windows (only between far2l instances in the same tvterm).
- `ClientOptions` and process information: not implemented on Windows.
- win32-input-mode: scan code 0; key-up sent right after key-down; some keys (shifted symbols, keypad digits) have `vk = 0`.
- The paused state of the Event Viewer is not saved in profiles.
- tvision (for a PR to magiblot): `histlist.cpp` memcpy → memmove (fixed locally, commit 22); `TFileList::getText` appends `\` after copying up to 255 characters into a 256-byte buffer
  → one byte too many for a directory name of exactly 255 bytes; `hotKeyStr` (tinputli.cpp) receives a null pointer from menu separators;
  `fmtstr.cpp` `vFormatStr`: GCC 15 + fortify-headers (Alpine) warn about `vsprintf` → proposal `vsnprintf(buf, n + 1, …)`.

---

## 7. Log by stage

| Stage | Commits | What was done |
|---|---|---|
| Initial cleanup | 1–5 | new branch from upstream; win32-input-mode with an automatically applied libvterm patch; static build from CMake; editor; Event Viewer; scripts/build.sh |
| Clipboard | 6–11 | OSC 52; Shift-Ins; debug log; far2l extensions (clipboard); bracketed paste; far2l vertical blocks |
| Profiles | 12–16 | ClientOptions + fixed title; INI profiles; process information; Window List, Rename, Save Profile; editors and Event Viewer in profiles |
| Organization | 17–18 | docs/HISTORY.md, then split into HISTORY.md (English) and HISTORY-ro.md (Romanian); STATUS.md kept in the Claude Project |
| Quit info | 19 | the quit confirmation lists what runs in each terminal, with Go to |
| Ctrl keys | 20 | Ctrl+\ [ ] _ and keypad operators reach far2l and other programs |
| File dialogs | 21 | profile dialogs: wildcard `*.ini` + directory set separately (no doubled path, `..\` works) |
| tvision history | 22 | `memmove` in the input line history (cause of "Illegal instruction" in file dialogs on Alpine) |
| Dialog clipboard | 23 | Shift-Ins / Ctrl-V / Ctrl-Ins / Ctrl-C in dialog text fields, pasted text cleaned up |
| Profile positions | 24 | positions saved inside the desktop; out-of-range positions moved inside when loading, not rejected |
| Tile / Cascade | 25 | the Event Viewer and editors stay in place; the other windows use the area they leave free |
| Tile area | 26 | the largest rectangle beside the fixed windows, each counted separately (editor at the top left no longer covered) |
| Load Profile | 27 | asks whether to close the open windows first (Yes / No / Cancel) |
| Scrollback follow | 28 | the view no longer stops following output after bursts; the scroll bar moved to the bottom follows again |
| Directory completion | 29 | a list of the matching directories below the Name field of the file dialogs (Down, Enter, Esc, Tab) |
| Static libpthread | 30 | static builds with glibc < 2.34 (Raspbian 10) link the whole libpthread; no longer abort at startup |
| Quit dialog size | 31 | the quit dialog can be resized (mouse, Ctrl-F5), zoomed (F5) and moved; initial size from its content; size kept while tvterm runs |
