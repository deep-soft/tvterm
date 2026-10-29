# HISTORY — tvterm (deep-soft)

Development history of the deep-soft/tvterm fork: what was implemented, how it works,
how it is tested and which pitfalls were found. Reference document, updated with each new
feature. Romanian version: [HISTORY-ro.md](HISTORY-ro.md); both are kept in sync.
How to use tvterm today (keys, settings, profiles, examples): [MANUAL.md](MANUAL.md) / [MANUAL-ro.md](MANUAL-ro.md).

Typical setup of this fork's main user: `Far (Windows) → putty4far2l → far2l (on the server) → tvterm → bash / far2l / mc`.
The outer far2l restores the session when SSH drops; tvterm runs inside it.

## 1. Repository and branches

| Branch | Role |
|---|---|
| `far2l-integration` | **main, stable branch** (on top of `magiblot/master` 210eb23): generic parts |
| `far2l-extended` | working branch for new features, on top of `far2l-integration` |
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

### Commits on `far2l-extended`

Numbered E1, E2, … so that new commits on `far2l-integration` do not change these numbers.

E1. Add history mode (scroll, search, save), word/line selection and a settings file
E2. Add version information (--version) and an About dialog
E3. Add filters to the Event Viewer
E4. Add alerts: activity, keywords and silence
E5. Add Log View: filtered output of a command
E6. Make the status line items of the Event Viewer and History mode clickable
E7. Log View: filters on/off in the settings dialog and in profiles
E8. Add a Window List item to the status lines
E9. Show adjusted profile positions in the Event Viewer
E10. Shorter status line items, distinct dialog hot keys, a note when Tile has no room
E11. Save dialogs: choose the file name form (date first or name first)
E12. Add the user manual (docs/MANUAL.md, docs/MANUAL-ro.md)
E13. Event Viewer: list the active alerts (A), dates in messages, keyword groups and exclusions

---

## 2. Features (all tested by the user in the real setup, unless marked otherwise)

### Menu (Ctrl-B)
`N` New Term · `W` Close · `Tab`/`Shift-Tab` Next/Prev · `J` Window List · `T` Rename Term · `K` Alerts · `G` Go to Next Alert ·
`R` Resize/Move · `F` Maximize · `S` Select Text · `P` Paste · `E` New Editor · `O` Open File in Editor · `V` Event Viewer ·
`D` Log View · `L` Load Profile · `A` Save Profile As · `H` History · `More...` (change dir, tile, cascade, Grab Input, Edit settings,
Reload settings, About) · `U` Suspend · `Q` Exit.
Free menu letters: B, C, I, X, Y, Z (`M` opens More...).
- **Tile / Cascade** (commit 25): arrange only the terminals (and other tileable windows); the Event Viewer and the editor windows
  stay where they are. The others use the largest rectangle that overlaps none of them, each fixed window counted separately
  (commit 26; commit 25 used the rectangle enclosing all of them, which with an editor at the top left and the Event Viewer on
  the right covered the whole desktop → the terminals were arranged over both). No fixed window open, or the largest free
  rectangle under 20 columns × 6 rows → the whole desktop, as before. An L-shaped free space is not split into several areas.
  Also for the `layout` of profiles. Code: `TVTermDesk::freeArea` (`desk.cc`; `noFreeArea` tells the caller the whole desktop
  was used).

### History (scrollback) — `far2l-extended`
- Each terminal keeps up to `lines` (default 10000) lines that left the screen. Only the normal screen has a history:
  full-screen programs (far2l, mc, multitail, editors) use the alternate screen, where there is none.
- **History mode**: Ctrl-B H, or directly **Alt-Shift-PgUp/PgDn/Home/End** (configurable; in full-screen programs and in
  Grab Input mode these keys go to the program as usual). The title shows `(History)` and the status line the keys:
  Up/Down, PgUp/PgDn, Home/End (also Ctrl-Home/End) scroll; the mouse wheel scrolls; **Esc** clears the selection, then
  the search match, then leaves the mode; any other key leaves the mode, goes back to the bottom and reaches the program.
  Entered with a direct key, the mode also ends when scrolling back to the bottom. Output keeps arriving meanwhile.
- **Search**: `/` or F7. Plain text (case-insensitive by default, also for diacritics: `ăla știe` finds `Ăla ȘTIE`) or
  regular expression (ECMAScript, like `grep -E`). Starts from the visible lines towards older ones; **F3 / n** older,
  **Shift-F3 / N** newer; the match is shown black on yellow; wrapped lines are searched as one line.
  Ctrl-C (or Ctrl-Ins) copies the selection, or else the match. Not found: a message in the status line.
- **Save**: F2 or `s` (Ctrl-B H in a full-screen program opens it directly, for the visible screen). Proposed name:
  `tvterm-<title>-YYYY-MM-DD-hhmm.txt` in `save-dir`, otherwise in the current directory of what runs in the terminal;
  a relative name is relative to that directory. Everything (history + screen) or the selection only. Plain UTF-8 text,
  trailing spaces removed, wrapped lines joined, empty lines at the start/end left out. Existing file: Overwrite / Append / Cancel.
  The result goes to the status line and the Event Viewer.
- **File name** (E11; the same dialog saves from the Log View): a `File name` group between the File field and "What to save",
  `Date first` (`YYYY-MM-DD-hhmm-tvterm-<title>.txt`, sorted by date in any file list) or `Name first` (the form above).
  Switching rewrites the File field at once, only while it still holds the proposed name (an edited name is left alone);
  the directory stays. The starting choice comes from `save-name` in `tvterm.ini` (default `name-first`); the choice of the
  last Save is kept until tvterm exits (Cancel does not change it), unless `save-name` changes in the meantime.
  `tvterm.ini` is never rewritten by tvterm.
- **Mouse**: double click selects a word (letters, digits, `word-chars`; non-ASCII counts as a letter), triple click the
  whole logical line (with the line break); dragging afterwards extends by words/lines. **Shift+click** forces the
  selection in programs that use the mouse (far2l), if `shift-click-select = yes`. Copying works as before (Ctrl-C).
  In the real setup, putty4far2l takes Shift+mouse for its own selection (on the whole outer screen), so tvterm never
  receives Shift+click while that selection is enabled in putty4far2l (the user disabled it). tvterm's own selection
  (without Shift, in shell windows) works and stays inside the current window.
- Old lines dropped from a full history no longer shift the selection or the match.
- Status line (E6): a click on `/ Search`, `F3 Next`, `Shift-F3 Prev`, `F2 Save`, `Ctrl-C Copy`, `Esc Exit` works like the key
  (`F3/Shift-F3 Next/Prev` was split in two); the scrolling keys stay a hint.

### Version and About — `far2l-extended`
- Version number `0.1.0`, set in `CMakeLists.txt` (`TVTERM_VERSION`) and changed only by hand. Everything else is added
  automatically at **every build** by `scripts/buildinfo.cmake`: the commit (`git describe --always --dirty`: short hash,
  `-dirty` with uncommitted changes), the branch, the build date as local time of the build machine with its time zone
  (`YYYY-MM-DD hh:mm TZ`, e.g. `2026-10-25 16:40 EET`; a machine without a time zone shows `UTC`), the build type
  (`static`/`dynamic`) and the commits of the bundled tvision and libvterm. Without git (e.g. sources from an archive): `unknown`.
- `tvterm --version` (or `-V`) prints it and exits, without starting the interface.
- The first two lines in the Event Viewer: `tvterm 0.1.0 (deep-soft), <branch> <commit>` and `built <date>, <type>`.
- **More... > About tvterm...**: version, commit and branch, build date and type, tvision/libvterm commits, the settings file
  (`(not found)` if it does not exist), the last profile loaded (`(none)`), uptime (`2d 03:14`: days, hours, minutes),
  the number of terminal windows (running or disconnected) and the links (deep-soft fork, magiblot original).
  The values are read when the dialog opens. Long paths are shortened at the beginning (`...`) on narrow screens.

### Event Viewer filters — `far2l-extended`
- Keys, when the Event Viewer window is focused (they are not shown as events): **M** mouse movements, **C** mouse buttons,
  wheel and auto-repeat, **K** keys, **O** other events (commands etc.), **T** tvterm's messages — each one shows/hides
  that kind; **N** clears the window and shows again the messages kept by tvterm (the last 200, the two version lines
  first), even if T is off; **X** clears the window. The status line shows them, one item per key (`M Moves`, `C Clicks`,
  `K Keys`, `O Other`, `T Messages`, `N Notes`, `X Clear`); a click on an item works like its key (E6).
- The title shows the state: `Event Viewer [M- C+ K+ O+ T+]` (`(Stopped)` before the brackets when paused).
  Default: mouse movements hidden, everything else shown. Only new events are affected.
- Hidden events are still counted: gaps in `Received event #N` show them.
- **A** (E13): lists the active alerts (those counted by `Alerts: N`), oldest first (the order of Ctrl-B G): time, mark,
  window title; for keyword alerts also the group found and the line (about 150 bytes around the first term).
  Header `--- Active alerts (N) ---`, or `--- No active alerts ---`; shown even when paused or with T-. Status line item
  `A Alerts`; title `Event Viewer [M- C+ K+ O+ T+] A: alerts`. The application builds the list (`listAlerts`, message
  `cmShowAlerts` from the viewer); each `AlertTarget` gives an `AlertInfo` (time of the current kind, mark, title, group, line).
- Buffer: 64000 bytes (about 220 events with all their lines), instead of 3840; Turbo Vision's `TTerminal` limits it to
  32000, so tvterm's `EventTerminal` replaces the buffer after construction (positions are 16-bit, up to 65535).
- Profiles: `[eventviewer]` accepts `mouse-move`, `mouse-clicks`, `keys`, `other`, `tvterm-messages` (yes/no); Save Profile As
  writes all five, with a comment explaining the letters. Missing keys get the defaults; invalid values are reported like
  other profile problems. Documented in `examples/profiles/example.ini`.

### Settings file (`tvterm.ini`) — `far2l-extended`
- `~/.config/tvterm/tvterm.ini` (or `$XDG_CONFIG_HOME/tvterm/`), sections `[general]` (`profile`, `auto-reload`),
  `[scrollback]` (`lines`, `wheel-lines`, `word-chars`, `save-dir`, `save-name` (E11), `search-case-sensitive`, `shift-click-select`),
  `[keys]` (`history-page-up/-page-down/-top/-bottom`, e.g. `Ctrl-F5`, `none`). Documented in `examples/tvterm.ini`.
- **Default profile**: `profile = NAME` is opened when tvterm starts without `-p`; `-p OTHER` wins; `-p none` starts without it.
  If it is missing or has errors: a normal terminal opens, the reason goes to the Event Viewer and the status line.
- Reloaded automatically (checked every 2 s, by modification time and size) and with More... > Reload settings;
  More... > Edit settings creates the file (every key commented out) and opens it in the editor. Everything except
  `profile` applies at once (a smaller `lines` drops the oldest lines). Invalid values keep their previous value;
  removed keys (or a removed file) go back to the defaults.
- **Messages**: tvterm's own messages (settings, saved files, profile errors) appear in the Event Viewer as `tvterm: hh:mm:ss ...`
  (`YYYY-MM-DD hh:mm:ss` for a message of another day, E13: the notes keep their `time_t` and are formatted when shown),
  even when it is paused; the last 200 are kept and shown when it is opened. Short messages appear in the status line for 5 s.

### Alerts — `far2l-extended`
- Only for terminals that are **not selected**. Three kinds, set per terminal with **Ctrl-B K** (Alerts...) or in profiles:
  **activity** (any new text), **keywords** (comma-separated, plain text, case-insensitive for ASCII letters),
  **silence** (N seconds without output, counted from the start of the terminal or from setting the alert, then from
  the last output and from leaving the terminal; after a silence alert, armed again by new output).
- Shown as: a mark in the window title (`[!]`, `[!error]`, `[!silent]`, so also in the Window List and in the quit dialog),
  `Alerts: N (Ctrl-B G)` on the right of the status line. **Ctrl-B G** goes to the oldest alert. Selecting a terminal clears its alert.
  Priority when several happen: keyword > silence > activity.
- Profile keys: `alert-activity = yes`, `alert-keywords = error, reject`, `alert-silence = 60`; saved by Save Profile As.
- False alarms avoided: output is ignored for 3 s after the terminal starts and for 1.5 s after a resize (tile, maximize)
  or after leaving the terminal (programs redraw then); this output still restarts the silence count.
  Activity alerts are still noisy with full-screen programs (far2l's clock changes every minute) → use keywords there.
- How: the emulator (worker thread) counts the bytes received and keeps the new text without escape sequences
  (`OutputTextFilter`: CSI/OSC/DCS/APC removed, cursor movements become line breaks, so the window title and far2l's
  APC data never match; at most 64 KB kept); `TerminalView` collects it on every `cmCheckTerminalUpdates`, even when
  the window is hidden; `TerminalWindow::checkAlerts` does the matching (keywords split between two reads are found).
- Real test (CentOS 7.9): activity and keywords worked; silence was first armed only by output, so a `tail -f`
  on a quiet log never alerted until new lines came → changed: silence is counted from the start of the terminal
  (tested by the user in the real setup).
- **Keyword groups and exclusions (E13)**: comma = alternative groups; `+` = terms that must all be in the same line;
  `!term` = global exclusion (only as an element of its own; a line containing it raises no keyword alert); only
  exclusions → any other non-blank line (mark `[!*]`). `\` makes the next character literal (`\!` `\+` `\,` `\\` `\ `);
  unescaped spaces at the ends of terms are dropped. `!` inside a group with `+`, or an empty term (`c++`), is an error:
  the Alerts dialog shows a message and stays open; a profile reports it as a problem (line number) and uses the valid
  elements. Old values (plain comma lists) keep their meaning, except those with `+` (write `\+`).
  `KeywordRules` (alerts.h) keeps the text as typed (saved in profiles unchanged; the profile reader keeps an escaped
  space at the end of the line) and the parsed groups/exclusions (lowercase ASCII).
  Matching is now per **line**: Log View lines as before; in terminals, the filtered output is split at its line breaks,
  a line not ended yet waits and is checked after 0.5 s without new output (prompts), so an exclusion that arrives a
  moment later on the same line still counts (this replaces the old tail of `maxLen - 1` bytes).
  Mark: the group with `+` and no spaces, shortened to 24 bytes (`…`, UTF-8 safe), in quotes when it contains `[` or `]`
  (`] Ban` gave `[!] Ban]`, now `[!"] Ban"]`). Keywords field: 1023 characters (was 255).
- Alert time (E13): each window keeps the wall-clock time at which its alert got its current kind (activity → keyword
  updates it), shown by A in the Event Viewer.

### Log View — `far2l-extended`
- **Ctrl-B D** opens a dialog: title (empty: the command), **command** (with a list of earlier commands, ↓), **include** and
  **exclude** (one regular expression each, POSIX extended like `grep -E`; alternatives with `|`, e.g. `dstport=(993|465|587) `;
  trailing spaces are kept), ignore case, wrap long lines, max lines (0: `[logview] max-lines` from tvterm.ini, default 50000),
  restart after N seconds (0: no), **Filters on** (E7; the same as the F key). Invalid values are reported in the dialog, which stays open with the focus on the field.
  **F4** on an open window: the same dialog (modal) with **OK / Apply / Cancel**; Apply applies the settings and keeps the
  dialog open (to see the effect of the filters); Cancel closes it and drops only what was not applied.
- A Log View is a window, not a terminal: it shows the lines written by the command. As many as needed; windows with
  **the same command share one source** (one process, one buffer; the buffer keeps the largest number of lines asked for).
  A line passes if it matches the include expression (or there is none) and not the exclude one. **New filters apply to
  the whole buffer** (50000 FortiGate lines: about 0.1 s).
- The source runs with `$SHELL -c command` (like terminal commands) in a pseudo-terminal (`TERM=dumb`, 512 columns), so
  programs such as awk write each line at once (in a pipe they keep 4 KB). Escape sequences, `\r` and other control
  characters are removed; tabs are kept (shown up to the next multiple of 8). Lines longer than 64 KB are cut; a line
  without `\n` is shown after 1 s without data. Read in the idle time (no threads, at most 4 MB per pass).
  When the command ends: `-- source ended (exit N) --` (yellow), a note in the Event Viewer, `(ended)` in the title;
  **R** restarts it (`-- source restarted --`), or automatically after N seconds. Closing the last window that uses a
  source sends SIGHUP to its process group (SIGKILL after 2 s, without blocking).
- Title: `Log: <title> [I] [E] shown/total`, `[F-]` with the filters off, `(ended)`, `[PAUSE +N]` while paused, `[+N]` when
  scrolled up and new lines came, the alert mark first (`[!reject] Log: ...`).
- Keys (window focused; also in its menu, **F9** or right click; the status line shows the main ones, and a click on one of
  them works like its key: F9, F4, I, E, F, Space, /, Ctrl-C, F2): F4 settings, I / E
  the dialog on include / exclude, F filters on/off, D duplicate (same source, filters copied, title + ` (2)`), R restart,
  Space pause/continue, W wrapped lines / horizontal scrolling (←→, 8 columns), X clear this window (not the source),
  `/` search (plain text or regular expression; the match is black on yellow), F3 or n older, Shift-F3 or N newer, F2 or s save,
  Ctrl-C (or Ctrl-Ins) copy, Ctrl-E to a new editor, Ctrl-A added at the end of an open editor (chosen from a list; the Log View
  keeps the focus) — the three use the selection or, without one, the cursor line —, mouse wheel, Esc clears the selection,
  then the match.
- **Cursor line** (black on cyan, only in the focused window): Up/Down/PgUp/PgDn/Home/End move it, the view scrolls only to
  keep it on the screen; a click puts it on a line; a search puts it on the match; with new filters it stays on the same line
  (if hidden: the nearest one before it); with W, all the rows of the line. **With the cursor on the last line, new lines are
  followed**; higher, the view stays (`[+N]` counts the new lines), End follows again. The mouse wheel and the scroll bar move
  only the view: at the end, following starts again and the cursor goes to the last line.
- Selection: whole lines; click, drag, Shift+click, Shift+Up/Down/PgUp/PgDn/Home/End (from the cursor line); Up/Down without
  Shift clear it. Double click copies a word. The clipboard gets
  the lines without the last line break (pasting into a shell does not run anything). Save: the lines shown, or the selection.
- Alerts (Ctrl-B K, same keys as for terminals) on the lines that pass the filters; nothing in the first 3 s after the source
  starts (e.g. `tail -n 5000`). The alert code is shared through the `AlertTarget` interface (terminal and Log View).
- Window List (`Log`), Rename (Ctrl-B T), quit dialog (lists the Log Views with their command), profiles:
  `[logview]` with `title`, `command`, `include`, `exclude` (in quotes when they start/end with spaces), `ignore-case`, `wrap`,
  `filters` (E7: `yes`/`no`, missing = `yes`, an invalid value is reported and the filters stay on), `max-lines`,
  `auto-restart`, `alert-*`, `pos`. An invalid expression in a profile: reported, the window opens without it.
  Paused state and selection are not saved.
- Real test (first version, then v2, on CentOS 7.9 and Alpine): v2 tested by the user and pushed. The F state is now part of
  the window's settings (E7): F, the F4 check box, D and profiles use the same value.

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
  `[terminal]` (`title`, `dir`, `command`, `keep-open` yes/no, `pos`), `[editor]` (`file`, `pos`), `[eventviewer]` (`pos`, filters: `mouse-move`, `mouse-clicks`, `keys`, `other`, `tvterm-messages`).
- `pos = x y width height` in **percent** of the desktop, decimals allowed. Save Profile writes positions inside the desktop
  (a window moved partly off the screen is saved as moved back inside; rounded to one decimal so that x + width ≤ 100);
  when loading, a position outside the desktop is moved inside it (and shrunk if larger) instead of being rejected (commit 24;
  before: `invalid pos '84.9 0 15.4 72.7'`). The adjustment is kept in `Profile::notes`;
  in `far2l-extended` it is shown in the Event Viewer (E9): `profile <path>: pos '…' is outside the desktop, using '…'`.
- `keep-open = yes` (default): when the command ends or is stopped with Ctrl+C, a shell starts; `no`: the terminal stays "(Disconnected)" with its output visible.
- Commands that run forever (`watch`, `tail -f`) must be stopped with Ctrl+C for the shell to appear.
- `mailq` may not be in the PATH of non-root users → `/usr/sbin/mailq` or `postqueue -p`.
- **Save Profile As** (Ctrl-B A): saves all windows (stacking order, focus, fixed title, positions, actual current directory from `/proc/<pid>/cwd`,
  the command from the profile, or the foreground command found with `tcgetpgrp` + `/proc/<pgid>/cmdline`).
  The text of "Untitled" editors is not saved (the user is warned).
- Load Profile / Save Profile As list the profiles directory with the wildcard `*.ini`; `..\` and directories work normally,
  and a typed name is relative to the directory shown (commit 21; before, the full path was doubled: "Invalid drive or directory").
- Load Profile with windows open (commit 27): "Close the current windows first?" Yes / No / Cancel. Yes = the terminals (and the running Log Views) are
  listed as when quitting (title "Close windows", button Close, Go to), editors with unsaved changes ask to save (Cancel stops the
  load), then all windows close and the profile opens; No = the profile's windows are added to the open ones (as before);
  Cancel = nothing is loaded. `confirmQuit(deskTop, closing)` in `quitdlg.cc`, `TVTermApp::closeWindowsBeforeLoad` in `app.cc`.
- Full documentation: `examples/profiles/example.ini`.

### Manual — `far2l-extended`
- `docs/MANUAL.md` (English) and `docs/MANUAL-ro.md` (Romanian), kept in sync (E12): how to use tvterm as it is now —
  building and command line, the Ctrl-B menu, terminals and clipboard, history, editor, every `tvterm.ini` key with its
  default, profiles, Event Viewer, alerts, Log View, complete examples (Postfix and Dovecot, fail2ban, nginx, a `tail | awk`
  script, multitail with several views on a firewall log) and known problems. Written from the sources (keys, defaults,
  messages), not from memory; the examples use `192.0.2.x` / `example.com`.
- HISTORY says what changed and why; the manual says how things work now. Each later patch updates the manual sections
  it touches when something visible changes (keys, settings, profile keys, display, messages); internal fixes go only
  into HISTORY.

### Other
- **Status line names** (E10): `Ctrl-B Open Menu` → `Ctrl-B Menu`, `Window List` → `WinList` (7 columns less, so the
  `Alerts: N (Ctrl-B G)` counter drawn over the right end covers it less often).
- **Dialog hot keys** (E10): no two items of a dialog share a letter. Alerts: `K~e~ywords` (Alt-K was taken by `O~K~`, which
  won); History / Log View save dialogs: `Se~l~ection only` (Alt-S was also `~S~ave`). Checked: Alerts, Log View settings (F4),
  find and save dialogs, Add to editor, Window List, Quit / Close windows, Rename, About.
- **Tile / Cascade without room** (E10): when the fixed windows leave no rectangle of at least 20 × 6 and the whole desktop is
  used (commit 26), the Event Viewer gets the note "no free area beside the fixed windows, using the whole desktop"
  (`TVTermApp::noteNoFreeArea`, also for the `layout` of profiles).
- **Window List** (Ctrl-B J, or a click on `WinList` in the status line, E8: right after `Ctrl-B Menu` in the
  terminal, Log View, Event Viewer and editor lines, at the end of the History, selection and Grab Input lines; not while
  moving/resizing or in the menu; no key; in Grab Input the click first releases the input): Go / Rename / Close. **Rename Term** (Ctrl-B T); an empty title restores the automatic one.
- **Event Viewer** (Ctrl-B V): shows Turbo Vision events (key diagnostics); pressing it again pauses it. Filters: see above.
- **Quitting** (Ctrl-B Q): the confirmation lists every open terminal and what runs in it, terminals running a program first:
  the foreground command (e.g. `tail -f /var/log/mail.log`, pipelines as `a | b`, `bash script.sh`), `bash, idle` for an idle shell,
  `[stays in background]` for far2l. Buttons: **Quit** (Enter or Q), **Go to** (G, Space or double click: goes to the chosen
  terminal and does not quit), Cancel (Esc). Editors with unsaved changes still ask afterwards.
  Programs get SIGHUP and stop; **far2l stays in the background** and can be reattached (verified).
  How the command is found: the processes of the terminal's foreground process group (`tcgetpgrp`) are read from `/proc/<pid>/stat`;
  shells that only run a command line (`sh -c ...`, e.g. the wrapper used for profile commands) are skipped; far2l rewrites its
  `argv[0]`, so its name is taken from `/proc/<pid>/stat`.
- Alt+Shift+Up/Down (far2l vertical selection) do not arrive even outside tvterm → an issue in putty4far2l/Windows, not in tvterm.
  Workaround: Alt+Shift+Left/Right + Alt+Shift+PgUp/PgDn.
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

---

## 3. Architecture and key files

```
source/tvterm-core/        (the library; potentially interesting for upstream)
  vtermemu.cc              emulator (worker thread): libvterm, win32-input, OSC 52, far2l APC, paste
  far2lext.cc/.h           far2l extensions protocol (clipboard), ExtraFormatStore (vertical blocks)
  outfilter.cc             OutputTextFilter: plain text from the client's output (for alerts)
  termview.cc              main thread: clipboard (TClipboard, asynchronous read with 3 s timeout), Shift-Ins, paste
  termwnd.cc               terminal window: fixed title (setCustomTitle)
  termctrl.cc              controller (reader/writer threads), getClientProcessInfo
  pty.cc                   forkpty, ClientOptions (command/dir/keepOpen; /bin/sh wrapper with `trap : INT`)
include/tvterm/            public headers (termemu.h: TerminalState, TerminalEvent; pty.h; termctrl.h; ...)
source/tvterm/             the application
  app.cc/.h                menu, status line, commands, profiles (open/save), main() with -p/--help/--version
  about.cc/.h              version text and About dialog; declarations of the generated build information
  profile.cc/.h            INI parser/writer (WindowProfile, Profile)
  winlist.cc/.h            Window List dialog, renameTerminal, getDeskTopWindows
  quitdlg.cc/.h            quit confirmation dialog (confirmQuit)
  alerts.cc/.h             alert settings (Ctrl-B K), Ctrl-B G, AlertStatusLine (a TVTermStatusLine with the alert counter),
                           AlertTarget (interface of windows with alerts); logic in wnd.cc (TerminalWindow::checkAlerts)
                           and logview.cc (LogViewWindow::checkAlerts)
  logsrc.cc/.h             LogSource: a command in a pseudo-terminal, lines without escape sequences, shared by command,
                           read in idle (LogSource::pollAll), stopped without blocking
  logview.cc/.h            LogViewWindow (filters, scrolling, wrap, search, selection, editor, save, F9 menu, alerts),
                           LogViewInterior (drawing, mouse), the settings dialog (execLogViewDialog); POSIX regex (regcomp)
  procinfo.cc/.h           /proc helpers: readLink, readCommandLine, describeForeground (what runs in a terminal)
  editwnd.cc/.h            editor (Save As dialogs etc.)
  evntview.cc/.h           Event Viewer (filters, N/X keys; EventTerminal: TTerminal with a larger buffer)
  dlgclip.cc/.h            clipboard in dialog text fields (focused TInputLine; called from TVTermApp::getEvent)
  wnd.h                    TerminalWindow (+ launchCommand/launchDir/launchKeepOpen)
  settings.cc/.h           tvterm.ini: Settings, loadSettings/applySettings, key names, template, FileStamp
  histdlg.cc/.h            search and save dialogs of history mode; openTextSave (the save dialog for Log Views)
  statline.cc/.h           status line that can show a message for a few seconds; items that send a key when clicked
                           (keyCommand: a command per key, turned into the key event in putEvent)
include/tvterm/termopts.h  TerminalOptions (global): scrollback size, wheel, word chars, keys, callbacks to the application
deps/vterm/patches/0001-win32-input-mode.patch   applied by CMake to a copy (the submodule stays clean)
deps/tvision-fixes.cmake   tvision fixes: histlist.cpp copied to the build directory, memcpy → memmove (submodule clean)
scripts/build.sh           dynamic/static build → bin/tvterm-dynamic, bin/tvterm-static (+ logs)
scripts/tvterm-copy        copy to the clipboard with OSC 52
scripts/buildinfo.cmake    run at every build: writes <build dir>/buildinfo.cc (version, commit, branch, date, type)
examples/profiles/example.ini
```

**Important flows:**
- Worker thread → main thread through `TerminalState` (under a mutex), picked up on the `cmCheckTerminalUpdates` broadcast (from `idle`).
- Main thread → worker thread through `TerminalEvent` (`termCtrl.sendEvent`); added types: `Far2lClipboardData`, `Paste`
  (data allocated with `new char[]`, freed by the receiver).
- far2l **waits forever** for the answer to requests with ID ≠ 0 → tvterm always answers (an empty answer on timeout).
- History mode lives in `TerminalView` (main thread). The scrollback belongs to the worker thread: text is read with
  `TerminalController::withEmulator()` (locks the worker's mutex; **never call it inside `lockState()`**, the worker locks
  them in the opposite order) and `TerminalEmulator::getText()` (logical lines with the position of every cell) /
  `getSelectionText()`. Scrolling uses the `ScrollBackScroll` event (relative; `scrollToTop`/`scrollToBottom`).
  `TerminalState::scrollbackDropped` counts the dropped lines, so that the view can shift its absolute coordinates.

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
- `TDialog` only closes itself on standard commands (cmOK, cmCancel, cmYes, cmNo): custom buttons must use them.
- `TGroup::current` hides globals named `current` inside `TVTermApp` methods.
- `TTerminal` silently limits its buffer to 32000 bytes (see Event Viewer filters).
- The Event Viewer logs events in `TVTermApp::getEvent`, before they are handled: its own keys are skipped there
  (only while it is focused), otherwise every toggle would appear in the list.
- Build information: generated by a custom target that runs at every build; the file is rewritten only when its contents
  change (`configure_file(COPYONLY)`), and it is kept out of the unity build and the precompiled headers, so a rebuild
  compiles only that small file. New top-level directories are ignored by `.gitignore`, so the script lives in `scripts/`.

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
- History tests (feat E1): Alt-Shift-PgUp/PgDn `ESC[5;4~`/`ESC[6;4~`, Alt-Shift-Home/End `ESC[1;4H`/`ESC[1;4F`, F2 `ESC OQ`,
  F3 `ESC OR`, Shift-F3 `ESC[1;2R`, F7 `ESC[18~`; mouse in SGR format `ESC[<b;x;yM/m` (double/triple click = repeated
  press/release within 0.1 s; drag = button 32). The match is the `brightbrown` background in pyte.
  - tvision ignores the Shift bit of xterm mouse reports, so Shift+click cannot be sent from the harness: it was tested with
    a temporary build that accepts Alt instead (not committed). In the real setup it arrives through far2l's extensions.
  - `/bin/sh` (dash) has no line editing: keys sent to the shell by mistake stay in the command line → send Ctrl+C first.
  - Measured: search in 10000 lines of 85 characters: 0.1 s found nearby, 0.9 s not found; RSS ≈ 75 MB with a full history.
- About tests (feat E2): `--version`/`-V`; Ctrl-B `m` `b` opens the dialog; its contents without settings/profile and with
  a profile of 3 terminals (one disconnected); uptime after 1 minute; long path shortened on a 60-column screen;
  Enter/Esc close it; the first notes in the Event Viewer; `TZ=Europe/Bucharest` → `EET`; `-dirty`; build without git → `unknown`;
  rebuild in the same minute compiles nothing, in the next minute only `buildinfo.cc`.
- Event Viewer tests (feat E3): default title and status line; X; mouse movements hidden, then shown after M; own keys
  not logged; K, C, T (new notes from an automatic settings reload); N (also with T off); ~120 key events (> 32000 bytes)
  keep the first notes, ~300 drop the oldest; pause title; profile keys, saving (comment + five keys) and reloading;
  invalid value and filter keys in [terminal] reported; letters typed in a terminal do not reach the viewer.
  The test must click inside the viewer: a click elsewhere moves the focus (and a maximized terminal covers it).
- Alerts tests (feat E4): activity, keyword, keyword split between writes, keyword only in an OSC title, silence and
  re-arming, silence from the start of a terminal that never writes (also from a profile), output in the first seconds
  restarts the silence count, Ctrl-B G/K, tile without false alarms, saved profile, a temporary message hides the alert
  counter, CPU with a flooding terminal + a unit test of OutputTextFilter.
- Log View tests (feat E5), with tmux (`tmux send-keys`, `capture-pane -p`, `-e` for colors) and generated FortiGate lines:
  two windows on the same `tail -F` → one process; include with a trailing space (in quotes) + exclude = `grep -E | grep -Ev`
  (60/200, 10047/50000; ignore case 9996/50000); new lines; log rotation (`tail -F` messages, then the new file); Space
  (`[PAUSE +N]`), F (`[F-]`), PgUp + new lines (`[+N]`), End, W, X; search with highlight (`103` background) and "No older match";
  Shift+Up → Ctrl-E (new editor), Ctrl-A (list, appended; saved file: 6 lines with LF); save (selection by default);
  colors/`\r`/tab removed or expanded; exit code; auto-restart after 2 s; keyword alert, counter, Ctrl-B G, Ctrl-B K;
  Ctrl-B D dialog; D duplicate (shared process); Save Profile As (quotes kept) and reload; quit dialog lists the Log Views
  and the processes stop; invalid expression in a profile; `[logview] max-lines` from tvterm.ini (also an invalid value);
  3 × 50000 lines (~38 MB) loaded in < 1 s, RSS ≈ 59 MB.
  After the user's first test (E5 redone): mouse clicks sent as SGR sequences (`printf '\033[<0;X;Ym'`, the test shell is
  `sh`); the colors read cell by cell from `capture-pane -e` (the cursor's background, 46, also colors the window frame);
  cursor on the last line and followed, Up/Down (view fixed, `[+N]` = only the new lines; before, it counted all of them),
  End; Shift+Up ×2 → 3 lines copied, Up → 1 line (the cursor's); Home, PgDn, Shift-End; click on a line + Up; wrapped line
  (3 rows with the cursor); cursor kept with filters on/off/changed (hidden line → the one before); status line clicks: F9,
  F4, I, F, Space, /, Ctrl-C, F2; a click on a temporary message only removes it; F4: Apply (dialog open, `[I] 10/300`), Apply
  with `(` (error, dialog open), Cancel after Apply (applied filter kept), OK with `(` (error, open), OK (`[I] 111/300`).
  Dynamic and static builds.
- Clickable status line tests (E6): Event Viewer — clicks on M, C (title `[M+ C- …]`, back), X (cleared), N (notes again);
  History — `Esc Exit` leaves the mode (no `(History)` in the title), `/ Search` opens the search dialog.
- Filters on/off tests (E7): new dialog shows `[X] Filters on`; unchecked + Apply → `[F-] 300/300`, dialog still open;
  Cancel keeps it; F → `[I] 100/300` and F4 shows `[X]` again; D copies `[F-]`; Save Profile As writes `filters = no`;
  a profile with `filters = no` → `[F-]`, without the key → filters on, `filters = maybe` → "invalid filters 'maybe'", filters on.
- Window List item tests (E8): a click on it in the terminal, Log View, Event Viewer, editor, History, selection and Grab Input
  lines opens the list (Grab Input: `(Input Grab)` gone from the title); 60 columns wide: no crash.

---

## 6. Known limitations

- The vertical block marker does not survive a trip through Windows (only between far2l instances in the same tvterm).
- `ClientOptions` and process information: not implemented on Windows.
- win32-input-mode: scan code 0; key-up sent right after key-down; some keys (shifted symbols, keypad digits) have `vk = 0`.
- The paused state of the Event Viewer is not saved in profiles. An Event Viewer opened with Ctrl-B V starts with the
  default filters (the last ones are not remembered; profiles keep them).
- Case-insensitive regular expressions only fold ASCII letters (plain text search also folds Latin, Romanian, Greek, Cyrillic).
- History search reads the whole history on every step (fine up to the 100000 lines limit, but not instant).
- Log View: the scroll bar counts lines, not wrapped rows; a search finds the first match of each line (F3 goes to the next
  line); restarting a source with `tail -n N` shows those N lines again; the regular expressions of the history search
  (ECMAScript) and of the Log View (POSIX, like `grep -E`) differ in details (e.g. `\d` only in ECMAScript).
- tvision (for a PR to magiblot): `histlist.cpp` memcpy → memmove (fixed locally, commit 22); `TFileList::getText` appends `\` after copying up to 255 characters into a 256-byte buffer
  → one byte too many for a directory name of exactly 255 bytes; `hotKeyStr` (tinputli.cpp) receives a null pointer from menu separators.

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
| History (`far2l-extended`) | E1 | history mode (scroll, search, save), double/triple click, Shift+click, tvterm.ini with default profile and automatic reload |
| Version (`far2l-extended`) | E2 | version 0.1.0 with commit/branch/build date generated at every build; `--version`; About dialog; first notes in the Event Viewer |
| Event Viewer filters (`far2l-extended`) | E3 | M C K O T filters, N (messages again), X (clear), 64000-byte buffer, filters in profiles |
| Alerts (`far2l-extended`) | E4 | activity / keyword / silence alerts (silence counted from the start), title mark, status line counter, Ctrl-B K and G, profile keys |
| Log View (`far2l-extended`) | E5 | Ctrl-B D: filtered output of a command, shared sources, include/exclude, pause, wrap, search, cursor line, selection to clipboard/editor, save, F9 menu, clickable status line, settings with Apply, alerts, profiles |
| Clickable status line (`far2l-extended`) | E6 | Event Viewer (one item per key) and History mode: a click on an item works like its key |
| Log View filters in F4 (`far2l-extended`) | E7 | "Filters on" check box in F4 (synchronized with F), `filters` key in `[logview]`, copied by D |
| Window List in the status line (`far2l-extended`) | E8 | clickable `Window List` item after `Ctrl-B Open Menu` in every status line except moving/resizing and the menu |
| Profile position notes (`far2l-extended`) | E9 | a position moved inside the desktop when loading a profile is reported in the Event Viewer |
| Fixes after the test of 23–25 (`far2l-extended`) | E10 | `Ctrl-B Menu` and `WinList` in the status lines; Alerts `K~e~ywords`, save dialogs `Se~l~ection only`; Event Viewer note when Tile has no room |
| Save file name (`far2l-extended`) | E11 | "File name: Date first / Name first" in the Save dialogs (history, screen, Log View), `save-name` in `tvterm.ini` |
| Manual (`far2l-extended`) | E12 | `docs/MANUAL.md` + `docs/MANUAL-ro.md`: user manual of the current behavior, with complete examples |
| Alert list (`far2l-extended`) | E13 | Event Viewer key A (active alerts with time, group, line); `hh:mm:ss` / `YYYY-MM-DD hh:mm:ss` in messages; keywords with `+` groups, `!` exclusions, `\` escapes |
