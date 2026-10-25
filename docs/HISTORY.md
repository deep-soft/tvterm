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

---

## 2. Features (all tested by the user in the real setup)

### Menu (Ctrl-B)
`N` New Term · `W` Close · `Tab`/`Shift-Tab` Next/Prev · `J` Window List · `T` Rename Term · `R` Resize/Move ·
`F` Maximize · `S` Select Text · `P` Paste · `E` New Editor · `O` Open File in Editor · `V` Event Viewer ·
`L` Load Profile · `A` Save Profile As · `More...` (change dir, tile, cascade, Grab Input) · `U` Suspend · `Q` Exit.
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
- **Debug log**: `TVTERM_CLIPBOARD_LOG=/tmp/tvterm-clip.log tvterm`.

### Profiles
- `tvterm -p NAME` → `~/.config/tvterm/profiles/NAME.ini` (or `$XDG_CONFIG_HOME/...`); `tvterm -p /path/file.ini`; `tvterm --help`.
- Sections: `[profile]` (`layout` = auto|manual|tile-columns|tile-rows|cascade, `focus` = 1-based index),
  `[terminal]` (`title`, `dir`, `command`, `keep-open` yes/no, `pos`), `[editor]` (`file`, `pos`), `[eventviewer]` (`pos`).
- `pos = x y width height` in **percent** of the desktop, decimals allowed.
- `keep-open = yes` (default): when the command ends or is stopped with Ctrl+C, a shell starts; `no`: the terminal stays "(Disconnected)" with its output visible.
- Commands that run forever (`watch`, `tail -f`) must be stopped with Ctrl+C for the shell to appear.
- `mailq` may not be in the PATH of non-root users → `/usr/sbin/mailq` or `postqueue -p`.
- **Save Profile As** (Ctrl-B A): saves all windows (stacking order, focus, fixed title, positions, actual current directory from `/proc/<pid>/cwd`,
  the command from the profile, or the foreground command found with `tcgetpgrp` + `/proc/<pgid>/cmdline`).
  The text of "Untitled" editors is not saved (the user is warned).
- Full documentation: `examples/profiles/example.ini`.

### Other
- **Window List** (Ctrl-B J): Go / Rename / Close. **Rename Term** (Ctrl-B T); an empty title restores the automatic one.
- **Event Viewer** (Ctrl-B V): shows Turbo Vision events (key diagnostics); pressing it again pauses it.
- **Quitting** (verified): confirmation; programs get SIGHUP and stop; **far2l stays in the background** and can be reattached.
- Alt+Shift+Up/Down (far2l vertical selection) do not arrive even outside tvterm → an issue in putty4far2l/Windows, not in tvterm.
  Workaround: Alt+Shift+Left/Right + Alt+Shift+PgUp/PgDn.

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
  editwnd.cc/.h            editor (Save As dialogs etc.)
  evntview.cc/.h           Event Viewer
  wnd.h                    TerminalWindow (+ launchCommand/launchDir/launchKeepOpen)
deps/vterm/patches/0001-win32-input-mode.patch   applied by CMake to a copy (the submodule stays clean)
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

### Known pitfalls
- Upstream's `.gitignore` ignores **everything** except explicitly allowed directories; files without an extension must be allowed too
  (e.g. `!scripts/*`). Allowed directories added: `scripts/`, `examples/`, `docs/`.
- Sources are found with `GLOB` → after adding a new `.cc` file, run `cmake` again.
- Unity build: `static` functions from different files can clash → own namespaces (`far2lext`, `far2lclip`).
- `TFileDialog`: its `directory` field starts empty → typed names are relative to the current directory; tvterm sets it to the profiles directory.
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
- Regression tests used: OSC 52 (simple, 150 KB, query ignored, invalid), Shift-Ins in a shell, native far2l paste with diacritics,
  far2l vertical block, bracketed paste in bash, profiles (positions, Ctrl+C → shell), save/reload profile, quit with an unsaved editor.

---

## 6. Known limitations

- The vertical block marker does not survive a trip through Windows (only between far2l instances in the same tvterm).
- `ClientOptions` and process information: not implemented on Windows.
- win32-input-mode: scan code 0; key-up sent right after key-down; some keys (shifted symbols, numpad) have `vk = 0`.
- The paused state of the Event Viewer is not saved in profiles.

---

## 7. Log by stage

| Stage | Commits | What was done |
|---|---|---|
| Initial cleanup | 1–5 | new branch from upstream; win32-input-mode with an automatically applied libvterm patch; static build from CMake; editor; Event Viewer; scripts/build.sh |
| Clipboard | 6–11 | OSC 52; Shift-Ins; debug log; far2l extensions (clipboard); bracketed paste; far2l vertical blocks |
| Profiles | 12–16 | ClientOptions + fixed title; INI profiles; process information; Window List, Rename, Save Profile; editors and Event Viewer in profiles |
| Organization | 17–18 | docs/HISTORY.md, then split into HISTORY.md (English) and HISTORY-ro.md (Romanian); STATUS.md kept in the Claude Project |
