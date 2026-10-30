# tvterm manual (deep-soft fork)

How to use tvterm as it is today, on the `far2l-extended` branch (`far2l-integration` has only a part of it:
terminals, clipboard, editor, Event Viewer without filters, profiles, Window List, Tile; not the history,
`tvterm.ini`, alerts, Log View or `--version`).
What changed and why is in
[HISTORY.md](HISTORY.md). Romanian version: [MANUAL-ro.md](MANUAL-ro.md); the two are kept in sync.

tvterm is a terminal emulator with windows, built on Turbo Vision. It runs inside another terminal
(e.g. far2l over SSH, PuTTY, xterm) and shows many terminals, editors and filtered log windows on one screen.

Contents:
1. [Building and starting](#1-building-and-starting)
2. [The screen and the Ctrl-B menu](#2-the-screen-and-the-ctrl-b-menu)
3. [Terminals and the clipboard](#3-terminals-and-the-clipboard)
4. [History (scrollback)](#4-history-scrollback)
5. [Editor](#5-editor)
6. [Settings file: tvterm.ini](#6-settings-file-tvtermini)
7. [Profiles](#7-profiles)
8. [Event Viewer](#8-event-viewer)
9. [Alerts](#9-alerts)
10. [Log View](#10-log-view)
11. [Complete examples](#11-complete-examples)
12. [Known problems](#12-known-problems)

---

## 1. Building and starting

### Building

Packages:

- Alpine: `apk add build-base cmake perl patch git linux-headers ncurses-dev ncurses-static gpm-dev`
  (`gpm-dev` is optional).
- Debian / Ubuntu / Raspberry Pi OS: `apt install build-essential cmake perl patch git libncurses-dev libgpm-dev`.

```sh
git clone --recursive -b far2l-extended https://github.com/deep-soft/tvterm.git
cd tvterm
scripts/build.sh static      # or: dynamic, both (default)
```

The result is `bin/tvterm-static` (or `bin/tvterm-dynamic`), with a build log next to it. Nothing is installed:
copy the executable where you want it, e.g. `cp bin/tvterm-static ~/bin/tvterm`. The static build has no library
dependencies and can be copied to other servers (see [Known problems](#12-known-problems) for old systems).

### Command line

```
Usage: tvterm [-p PROFILE]

  -p, --profile PROFILE   open the windows described in PROFILE
                          ('none': do not open the default profile)
  -V, --version           show the version and build information
  -h, --help              show this help
```

`PROFILE` is a path (if it contains `/`) or a name, looked up as `~/.config/tvterm/profiles/NAME.ini`
(`.ini` is added when the name has no dot). `--profile=NAME` also works.

Without `-p`, tvterm opens the default profile (`profile` in `tvterm.ini`), if set; otherwise one terminal.
If the profile cannot be opened, a normal terminal opens and the reason is shown.

### Files

| File | Purpose |
|---|---|
| `~/.config/tvterm/tvterm.ini` | settings ([chapter 6](#6-settings-file-tvtermini)) |
| `~/.config/tvterm/profiles/*.ini` | profiles ([chapter 7](#7-profiles)) |

With `XDG_CONFIG_HOME` set, `$XDG_CONFIG_HOME/tvterm/` is used instead of `~/.config/tvterm/`.
`examples/tvterm.ini` and `examples/profiles/example.ini` in the sources describe both formats.

### Version

`tvterm --version` prints the version (`0.1.0`), the branch and commit, the build date, type (`static` / `dynamic`),
architecture and C library, and the commits of the bundled tvision and libvterm. For example:

```
tvterm 0.1.0 (deep-soft)
commit:   1a2b3c4 (far2l-extended)
built:    2026-10-30 17:20 EET, static, armv7l, glibc 2.28
tvision:  6402631
libvterm: 62b27d1
```

The architecture uses the names of `uname -m` (`x86_64`, `armv7l`, `aarch64`) and the C library is `glibc X.Y` or
`musl` (Alpine). Both describe the executable as it was built, not the machine it runs on: useful to check that the copy
on a server is the right one. The same information is in **Ctrl-B More... > About tvterm...**,
together with the settings file, the last profile loaded, the uptime and the number of terminals.

---

## 2. The screen and the Ctrl-B menu

The screen has the desktop (the windows) and, at the bottom, the **status line**. The status line changes with the
focused window and shows its main keys; a click on an item works like its key. `WinList` opens the Window List.
When windows have alerts, `Alerts: N (Ctrl-B G)` appears at its right end. Short messages (e.g. "Settings reloaded.")
replace the status line for 5 seconds.

Windows are moved with the mouse by their title, resized by their lower right corner and closed with the `[■]` button,
as in any Turbo Vision program.

### The Ctrl-B menu

**Ctrl-B** opens the menu in the middle of the screen; then press the letter (or click). Ctrl-B is not sent to programs
(see Grab Input below). Another key, or several, can open the menu instead: `menu-key` in `tvterm.ini`
([Global keys](#global-keys)); the status line then shows the first one (e.g. `` Ctrl-` Menu ``). This manual always
writes Ctrl-B.

| Key | Item | What it does |
|---|---|---|
| `N` | New Term | new terminal with your shell (`$SHELL`) |
| (none) | Close Term | closes the focused window: choose it with the arrows + Enter, or click it (no key, so that it is not chosen by mistake) |
| `Tab` / `Shift-Tab` | Next / Previous Term | next / previous window |
| `W` | Window List... | list of all windows: Go, Rename, Close |
| `T` | Rename Term... | fixed title for a terminal or Log View (empty: the automatic one) |
| `K` | Alerts... | alerts of the focused terminal or Log View ([chapter 9](#9-alerts)) |
| `G` | Go to Next Alert | goes to the window with the oldest alert |
| `R` | Resize/Move | arrows move, Shift+arrows resize, Ctrl faster, Enter done, Esc cancel |
| `Z` | Zoom (Maximize/Restore) | maximizes the focused window; again: back to its size (`F`, the former letter, also works) |
| `S` | Select Text | selection mode in a terminal ([chapter 3](#3-terminals-and-the-clipboard)) |
| `H` | History | history mode in a terminal ([chapter 4](#4-history-scrollback)) |
| `P` | Paste | pastes the clipboard into the terminal |
| `E` | New Editor | empty editor |
| `O` | Open File in Editor... | |
| `V` | Event Viewer | opens it; if already open, pauses / resumes it |
| `D` | Log View... | new Log View ([chapter 10](#10-log-view)) |
| `L` | Load Profile... | |
| `A` | Save Profile As... | saves the open windows as a profile |
| `M` | More... | submenu, see below |
| `U` | Suspend | stops tvterm and returns to the outer shell; `fg` brings it back |
| `Q` | Exit | asks first if terminals or Log Views are running |

**More...**: Change working dir · Tile (Columns First) · Tile (Rows First) · Cascade · Grab Input ·
Edit settings (tvterm.ini) · Reload settings · About tvterm.

### Tile and Cascade

Tile and Cascade arrange the terminals and Log Views. The **Event Viewer and the editors stay where they are**; the other
windows use the largest rectangle that overlaps none of them. If that rectangle is smaller than 20 columns × 6 rows
(or there is no fixed window), the whole desktop is used and the Event Viewer gets the note
"no free area beside the fixed windows, using the whole desktop".

### Window List

**Ctrl-B W** or a click on `WinList`. Each line shows the kind (`Term`, `Log`, `Editor`, `Events`) and the title,
with the alert mark if any. Buttons: **Go** (Enter), **Rename**, **Close**, Cancel.

### Grab Input

**More... > Grab Input** sends every key to the program in the focused terminal, Ctrl-B and the global keys included
(the title shows `(Input Grab)`). **Alt-End** or a click outside the window ends it.

### Global keys

Keys set in `[keys]` of `tvterm.ini` ([chapter 6](#6-settings-file-tvtermini)) that work in every window, without the menu:

| Key in `tvterm.ini` | Default | What it does |
|---|---|---|
| `menu-key` | `Ctrl-B` | opens the Ctrl-B menu |
| `key-zoom` | (none) | Zoom: maximizes the focused window; again: back to its size (like Ctrl-B Z) |
| `key-window-left` / `-right` / `-up` / `-down` | (none) | goes to the window beside the focused one in that direction |

- A comma separates several keys: `` menu-key = Ctrl-B, Ctrl-` ``. Modifiers `Ctrl-`, `Alt-`, `Shift-` (or with `+`, any case and
  order: `shift-alt-left` = `Alt-Shift-Left`). Keys: `F1`…`F12`, `Left`, `Right`, `Up`, `Down`, `Home`, `End`, `PgUp`, `PgDn`,
  `Ins`, `Del`; only with Ctrl or Alt (otherwise they would block typing): `Tab`, `Enter`, `Backspace`, `Space`, `a`…`z`,
  `0`…`9` and `` ` `` (the key left of 1; also written `backquote` or `„`, the same key on the Romanian layout).
  `Esc` cannot be used.
- **`:not-far2l`** after a key (e.g. `key-zoom = F12:not-far2l`): while the focused terminal runs far2l with its
  extensions, the key goes to far2l; in the other windows it is a global key. far2l turns its extensions on at start and
  off at exit (also through ssh); if far2l is killed abruptly, the key keeps going to that terminal until it is restarted.
- Removing `Ctrl-B` from `menu-key` sends Ctrl-B to the programs again (e.g. far2l, tmux, `less`).
- A global key wins over the window's own use of it (e.g. `Alt-Shift-Left` in the editor or in far2l: vertical block).
  It is not taken in dialogs, menus and Grab Input, nor while a window is being moved or resized.
- The neighbouring window: among the windows in that direction, first those beside the focused one (sharing rows for
  left/right, columns for up/down), then the nearest; where windows overlap, the one on top. At the edge of the screen
  nothing happens (no wrap-around). The Event Viewer and editors are included.
- An invalid key is ignored and reported in the Event Viewer; a key used by two settings is kept for the first one
  (order: `menu-key`, `key-zoom`, left, right, up, down). Without a valid `menu-key`, Ctrl-B is used.
- Avoid **Ctrl+Alt+arrows**: Windows (often the graphics driver's shortcuts, or another program) may take them before
  they reach SSH (seen: Left/Right on Windows 11, Up/Down on Windows 7), so they do not arrive at all
  ([chapter 12](#12-known-problems)).

Recommended (tested in the real setup, putty4far2l → far2l → tvterm): in far2l, Alt+arrows stay far2l's keys and
Shift+Alt+arrows move between windows; in the other windows both move between windows.

```ini
[keys]
menu-key = Ctrl-B, Ctrl-`
key-zoom = F12:not-far2l
key-window-left = Alt-Left:not-far2l, Shift-Alt-Left
key-window-right = Alt-Right:not-far2l, Shift-Alt-Right
key-window-up = Alt-Up:not-far2l, Shift-Alt-Up
key-window-down = Alt-Down:not-far2l, Shift-Alt-Down
```

### Quitting

**Ctrl-B Q** lists the open terminals and Log Views with what runs in each (e.g. `tail -F /var/log/maillog`,
`bash, idle`), the busy ones first. Buttons: **Quit**, **Go to** (goes to the chosen window, does not quit), Cancel.
Editors with unsaved changes ask afterwards. Programs receive SIGHUP; far2l stays in the background and can be reattached.

The dialog opens large enough to show every line whole (when the screen allows) and can be changed while it is open:

- **Resize** with the mouse (bottom right corner) or with **Ctrl-F5**: arrows move it, Shift+arrows change its size,
  Enter or Esc end. It can also be moved by its title. The list grows with it; the buttons stay at the bottom.
  Lines that do not fit end with `…`.
- **F5** or the zoom button of the frame: whole screen; again: back.
- It never leaves the screen; if the screen gets smaller while it is open, it is moved and shrunk to fit.
- The size you choose and the zoomed state are kept until tvterm exits (the dialog always opens centered). On a smaller
  screen it opens shrunk; the size you chose comes back on a larger one.
- The same holds for the "Close windows" question of Load Profile.

### File dialogs

In the name field of the file dialogs (Open File in Editor, the editor's Save file as, Load Profile, Save Profile As,
and the File field of the Save dialog of the history and the Log View), a list of directories opens by itself below the
field while you type a path: the subdirectories of the part before the last `/` whose names begin with what follows it.
For example `/var/` + `l` → `lib/`, `local/`, `lock/`, `log/`.

- Only directories; hidden ones only after you type a `.`. Upper and lower case differ (`L` does not match `lib`).
- A name without `/` is taken from the directory shown in the dialog; `~/` is your home directory.
- **Down** moves into the list; Up / Down / PgUp / PgDn / Home / End move in it. **Enter** (or a double click) adds the
  directory and a `/` to the field and shows its subdirectories, so you can go on typing or choosing.
  **Up** on the first row goes back to the field; any other key also goes back to the field and is typed there.
- **Esc** closes the list (a second Esc closes the dialog); **Tab** closes it and moves to the next field.
- With the list closed the keys work as before: Down opens the field's history, Enter opens the file or changes to the
  directory. The list of files of the dialog does not change while you type.
- Pasted text (Shift-Ins, Ctrl-V) opens the list too. At most 10 rows are shown, with a scroll bar for the rest.

---

## 3. Terminals and the clipboard

A new terminal runs `$SHELL`. Its title is set by the program running in it, unless a fixed title was given
(Ctrl-B T, or `title` in a profile). Keys go to the program, except Ctrl-B and the [global keys](#global-keys). far2l running inside tvterm uses its
extended keyboard mode (win32-input-mode) and its terminal extensions: its title shows `TTY|F`.

When the program of a terminal ends, the title shows `(Disconnected)` and the output stays visible; any key closes it.

### Selecting text

- **Drag with the mouse** in a terminal whose program does not use the mouse (a shell): selects text.
  **Double click** selects a word (letters, digits and the `word-chars` of `tvterm.ini`), **triple click** a whole line;
  dragging afterwards extends by words or lines.
- **Shift+click** selects even in programs that use the mouse (far2l, mc), if `shift-click-select = yes` (default).
  Note: putty4far2l uses Shift+mouse for its own selection; tvterm receives it only if that is turned off there.
- **Ctrl-B S** starts the selection mode, for programs that use the mouse.
- With a selection: **Ctrl-C** copies it, **Esc** cancels it.

### Copy and paste

| Where | Copy | Paste |
|---|---|---|
| terminal | selection + Ctrl-C | **Shift-Ins**, Ctrl-B P, or the outer terminal's paste (e.g. right click in PuTTY) |
| far2l inside tvterm | far2l's own keys (Ctrl-C, Ctrl-Ins) | far2l's own keys (Ctrl-V, Shift-Ins) |
| editor | Ctrl-Ins (Shift-Del cuts) | Shift-Ins |
| text fields of dialogs | Ctrl-Ins or Ctrl-C (selected text) | Shift-Ins or Ctrl-V |
| history mode, Log View | Ctrl-C or Ctrl-Ins | — |

The clipboard is the system clipboard. Through far2l and putty4far2l it is the **Windows clipboard**
(in the outer far2l, allow it with "Share clipboard always").

- Shift-Ins in a terminal pastes through tvterm only when the program does not use far2l's extensions (otherwise
  the program, e.g. far2l, pastes by itself) and not in Grab Input mode.
- Pasted text is sent at once; programs that ask for it (bash, vim) receive it as a paste (bracketed paste), so a
  pasted command is not run before Enter. Control characters are removed.
- Pasted into a dialog field, text becomes one line: line breaks and tabs become spaces, control and invisible
  characters are removed, the text is cut at the field's length.
- **OSC 52**: programs in a terminal can write to the clipboard (vim, tmux). `scripts/tvterm-copy` copies from the command
  line: `command | tvterm-copy`, `tvterm-copy < file`, `tvterm-copy "text"`. Reading the clipboard with OSC 52 is refused.
- far2l's vertical blocks stay vertical when copied and pasted between far2l windows in the same tvterm.
- Clipboard problems: `TVTERM_CLIPBOARD_LOG=/tmp/tvterm-clip.log tvterm` writes a log.

---

## 4. History (scrollback)

Each terminal keeps the lines that leave the screen (`lines` in `tvterm.ini`, default 10000). Full-screen programs
(far2l, mc, top, editors) use a separate screen without history.

### History mode

Enter it with **Ctrl-B H**, or directly with **Alt-Shift-PgUp / PgDn / Home / End** (configurable in `[keys]`).
The title shows `(History)`. Output keeps arriving meanwhile.

| Key | Action |
|---|---|
| Up / Down, PgUp / PgDn, mouse wheel | scroll |
| Home / End (also Ctrl-Home / Ctrl-End) | top / bottom |
| `/` or F7 | search |
| F3 or `n` / Shift-F3 or `N` | next match towards older / newer lines |
| F2 or `s` | save |
| Ctrl-C or Ctrl-Ins | copy the selection, or else the match |
| Esc | clears the selection, then the match, then leaves the mode |
| any other key | leaves the mode and goes to the program |

Entered with a direct key (Alt-Shift-...), the mode also ends when you scroll back to the bottom. In a full-screen
program the direct keys go to the program, and Ctrl-B H opens the save dialog for the visible screen.

### Search

Plain text or regular expression (like `grep -E`). Plain text ignores case by default (`search-case-sensitive`),
including diacritics (`știe` finds `ȘTIE`). The search starts from the visible lines towards older ones; the match is
shown black on yellow. Not found: a message in the status line.

### Save

The dialog proposes `tvterm-<title>-YYYY-MM-DD-hhmm.txt` in `save-dir`, or else in the current directory of the program
in the terminal; a relative name is relative to that directory. Directory names can be completed in the File field
([File dialogs](#file-dialogs)).

- **File name**: `Date first` (`YYYY-MM-DD-hhmm-tvterm-<title>.txt`, sorted by date in file lists) or `Name first`.
  Switching rewrites the proposed name (not one you edited). The starting choice is `save-name` in `tvterm.ini`;
  the last choice is kept until tvterm exits.
- **What to save**: everything (history + screen) or the selection only.
- Plain UTF-8 text, without colors; trailing spaces removed, wrapped lines joined.
- Existing file: **Overwrite**, **Append** or Cancel. The result is shown in the status line and the Event Viewer.

---

## 5. Editor

**Ctrl-B E** opens an empty editor, **Ctrl-B O** a file. Editors open on the right side (80 columns) when the screen
is wider than 100 columns. **F2** saves (an untitled editor asks for a name). Clipboard: Ctrl-Ins copy, Shift-Del cut,
Shift-Ins paste. Closing an editor with unsaved changes asks first. Tile and Cascade leave editors in place.

**More... > Edit settings** opens `tvterm.ini` in an editor.

---

## 6. Settings file: tvterm.ini

`~/.config/tvterm/tvterm.ini`. tvterm never creates or rewrites it by itself: **More... > Edit settings** creates it,
if missing, with every key commented out, and opens it. Keys added in newer versions are added by hand
(see `examples/tvterm.ini`).

- `#` or `;` at the start of a line: comment. Section and key names ignore case.
- Changes apply when the file is saved (checked every 2 seconds), or with **More... > Reload settings**.
- An invalid value keeps the previous one; a removed key (or file) goes back to the default.
  Problems are reported in the Event Viewer, with a summary in the status line.

| Section | Key | Default | Values / meaning |
|---|---|---|---|
| `[general]` | `profile` | (none) | profile opened at startup without `-p` (name or path); only read at startup |
| | `auto-reload` | `yes` | reload the file when it changes |
| `[scrollback]` | `lines` | `10000` | history lines per terminal, 0–100000 (about 4 KB per line of 100 columns) |
| | `wheel-lines` | `3` | lines per mouse wheel step, 1–100 (also in Log Views) |
| | `word-chars` | `-._/@:~+%=` | characters that belong to a word on double click, besides letters and digits |
| | `save-dir` | (empty) | directory proposed when saving; empty: the terminal's current directory (Log View: tvterm's) |
| | `save-name` | `name-first` | `name-first` or `date-first` ([chapter 4](#save)) |
| | `search-case-sensitive` | `no` | starting value of "Case sensitive" in the search dialog |
| | `shift-click-select` | `yes` | Shift+click selects in programs that use the mouse |
| `[logview]` | `max-lines` | `50000` | lines kept by each Log View source, 100–1000000 |
| `[highlight <name>]` | rules | built-in sets | Log View colors ([chapter 10](#colors-highlight)) |
| `[keys]` | `history-page-up` | `Alt-Shift-PgUp` | direct history keys |
| | `history-page-down` | `Alt-Shift-PgDn` | |
| | `history-top` | `Alt-Shift-Home` | |
| | `history-bottom` | `Alt-Shift-End` | |
| | `menu-key` | `Ctrl-B` | keys that open the menu ([Global keys](#global-keys)) |
| | `key-zoom` | (none) | Zoom key |
| | `key-window-left`, `-right`, `-up`, `-down` | (none) | keys that go to the neighbouring window |

Yes/no values also accept `true`/`false`, `on`/`off`, `1`/`0`. Key names: modifiers `Ctrl-`, `Alt-`, `Shift-`
(or with `+`) and a key: `PgUp`, `PgDn`, `Home`, `End`, `Up`, `Down`, `Left`, `Right`, `Ins`, `Del`, `Tab`, `Enter`,
`Esc`, `Backspace`, `Space`, `F1`…`F12` or one character; `none` disables the key. The global keys accept a list and
fewer keys ([Global keys](#global-keys)).

```ini
[general]
profile = mail-server

[scrollback]
lines = 20000
save-dir = ~/logs
save-name = date-first

[keys]
history-page-up = Ctrl-F5
history-page-down = Ctrl-F6
```

---

## 7. Profiles

A profile opens a set of windows with their commands, titles, alerts and positions. Open it with `tvterm -p NAME`,
with **Ctrl-B L**, or as the default profile. **Ctrl-B A** saves the open windows as a profile.

**Ctrl-B L** with windows open asks "Close the current windows first?": **Yes** lists the running terminals and Log Views
(Close / Go to / Cancel), editors with unsaved changes ask to save, then everything closes and the profile opens;
**No** adds the profile's windows to the open ones; **Cancel** loads nothing.

### Format

`#` or `;` start a comment. One `[profile]` section, then one section per window, in stacking order
(the last one is in front).

`[profile]`:

| Key | Default | Meaning |
|---|---|---|
| `layout` | `auto` | `auto` (`manual` if any window has `pos`, otherwise `tile-columns`), `manual`, `tile-columns`, `tile-rows`, `cascade` |
| `focus` | last window | window focused at start, 1 = the first window section |

All the window sections accept `pos = x y width height`, in **percent** of the desktop (decimals allowed). A position
partly outside the desktop is moved inside (and shrunk if larger), with a note in the Event Viewer.

`[terminal]`:

| Key | Default | Meaning |
|---|---|---|
| `title` | (automatic) | fixed title |
| `dir` | current directory | working directory (`~` = home) |
| `command` | interactive shell | command, run as `$SHELL -c "command"` |
| `keep-open` | `yes` | `yes`: when the command ends (or Ctrl-C), a shell starts in the same terminal; `no`: the terminal stays `(Disconnected)` with its output |
| `alert-activity`, `alert-keywords`, `alert-silence` | off | alerts ([chapter 9](#9-alerts)) |

`[logview]` ([chapter 10](#10-log-view)):

| Key | Default | Meaning |
|---|---|---|
| `title` | the command | window title |
| `command` | (required) | command whose output is shown |
| `include` | (none) | regular expression: only matching lines are shown |
| `exclude` | (none) | regular expression: matching lines are hidden |
| `ignore-case` | `no` | for include / exclude |
| `wrap` | `no` | `yes`: long lines wrapped; `no`: scrolled with Left / Right |
| `filters` | `yes` | `no`: include / exclude not used (as after the F key) |
| `max-lines` | `0` | lines kept; 0: `[logview] max-lines` of `tvterm.ini` |
| `auto-restart` | `0` | restart the command N seconds after it ends (0: never; up to 86400) |
| `highlight` | (none) | highlight sets, e.g. `zimbra-mta kv` ([chapter 10](#colors-highlight)) |
| `highlight-on` | `yes` | `no`: colors off (as after the H key) |
| `alert-activity`, `alert-keywords`, `alert-silence` | off | alerts, on the lines shown |

Put `include` / `exclude` in double quotes to keep spaces at the start or end: `include = "dstport=(993|465) "`.

`[editor]`: `file` (without it, an empty editor). `[eventviewer]`: the filters `mouse-move` (default `no`),
`mouse-clicks`, `keys`, `other`, `tvterm-messages` (default `yes`) ([chapter 8](#8-event-viewer)).

```ini
[profile]
layout = manual
focus = 1

[terminal]
title = Shell
dir = ~
pos = 0 0 50 100

[logview]
title = Mail errors
command = tail -n 5000 -F /var/log/maillog
include = "status=(bounced|deferred)|reject:"
alert-keywords = bounced
pos = 50 0 50 60

[eventviewer]
pos = 50 60 50 40
```

### Saving (Ctrl-B A)

Saves every window with its position (inside the desktop, one decimal), the stacking order and the focused window.
For a terminal: the fixed title, the current directory of the program running in it, the command from the profile or
else the command running in the foreground (e.g. `tail -f ...`, saved with `keep-open = yes`), the alerts.
For a Log View: all its settings. The text of untitled editors is not saved, and editors with unsaved changes keep only
their file name; tvterm says so after saving. The dialog proposes the profiles directory; `.ini` is added if missing.

---

## 8. Event Viewer

**Ctrl-B V** opens the Event Viewer (only one); pressing it again pauses / resumes it (`(Stopped)` in the title).
It opens on the right side (70 columns) when the screen is wider than 100 columns.

It shows the Turbo Vision events (keys, mouse, commands; useful to check what a key sends) and **tvterm's messages**:
settings problems, saved files, profile notes, ended Log View sources. Messages look like
`tvterm: hh:mm:ss text`, or `tvterm: YYYY-MM-DD hh:mm:ss text` for a message of another day (the date is added when the
message is shown, so after midnight the older messages get it); the last 200 are kept, even while no Event Viewer is open, and the first two lines give
the version and build.

Keys, while it is focused (each one is also a status line item):

| Key | Action |
|---|---|
| `M` | mouse movements on/off (default off) |
| `C` | mouse clicks and wheel on/off |
| `K` | keys on/off |
| `O` | other events (commands) on/off |
| `T` | tvterm's messages on/off |
| `N` | clears the window and shows the kept messages again |
| `A` | lists the active alerts ([chapter 9](#9-alerts)) |
| `X` | clears the window |

The title shows the filters and the A key: `Event Viewer [M- C+ K+ O+ T+] A: alerts`. They apply to new events only; hidden events are still
counted (`Received event #N` skips numbers). Profiles keep the filters (`[eventviewer]`).

---

## 9. Alerts

Alerts tell you that something happened in a terminal or Log View you are **not** looking at. Set them with
**Ctrl-B K** in the window, or in a profile.

| Kind | Dialog | Profile key | Mark in the title |
|---|---|---|---|
| activity: any new text | Any new text | `alert-activity = yes` | `[!]` |
| keyword found in a new line | Keywords | `alert-keywords = error, reject` | `[!error]` |
| silence: no output for N seconds | Silence | `alert-silence = 60` | `[!silent]` |

- Keywords are plain text (not regular expressions), looked for in each new line; case is ignored for ASCII letters only;
  spaces at the ends of each term are dropped. Up to 1023 characters in the dialog.

  | Written | Meaning |
  |---|---|
  | `error, reject` | **comma** = alternative groups: a line with any of them |
  | `192.168.88.4 + :993` | **`+`** = every term of the group must be in the same line (any order) |
  | `!someuser@someserver` | **`!`** = exclusion: a line containing it raises no keyword alert, whatever group it matches; only as an element of its own, between commas |
  | `!debug, !info` | only exclusions: any other new line raises the alert (mark `[!*]`) |
  | `\!`, `\+`, `\,`, `\\`, `\ ` | `\` makes the next character plain text (`\ ` keeps a space at the end of a term) |

  Example: `192.168.88.4 + :993, !someuser@someserver`. An exclusion inside a group with `+` (`a + !b`, `!a + b`) or
  an empty term (`c++`: write `c\+\+`) is refused by the dialog with a message (the dialog stays open); in a profile it
  is reported as a profile problem and the other keywords are used.
- The mark shows the group found, without spaces around `+` and shortened to 24 characters (`[!192.168.88.4+:993]`); a
  group with `[` or `]` is put in quotes (`] Ban` → `[!"] Ban"]`).
- In a terminal, a line still being written (e.g. a prompt without Enter) is checked after 0.5 s without new output,
  so an exclusion arriving a moment later on the same line still counts.
- Silence is counted from the start of the window (or from setting the alert), then from the last output and from
  leaving the window. After a silence alert, new output arms it again.
- Priority: keyword > silence > activity. A keyword found inside a window title or escape sequence does not count.
- The mark also appears in the Window List and the quit dialog; the status line shows `Alerts: N (Ctrl-B G)`.
  **Ctrl-B G** goes to the oldest alert. Selecting the window clears its alert.
- **A** in the Event Viewer lists the active alerts (those counted by `Alerts: N`), oldest first (the order of Ctrl-B G):
  the time the alert got its current kind (an activity alert that became a keyword alert shows the keyword time),
  the mark and the window title; for keywords also the group and the line (around the first term, about 150 bytes):

  ```
  --- Active alerts (2) ---
  14:02:11 [!] Shell
  14:05:40 [!192.168.88.4+:993] Log: FortiGate
      group: 192.168.88.4 + :993
      line:  date=2026-10-29 time=14:05:40 ... srcip=192.168.88.4 ... dstport=993 ...
  ```

  Without alerts: `--- No active alerts ---`. The times have the date (`YYYY-MM-DD hh:mm:ss`) when not from today.
- No false alarms from redrawing: output is ignored for 3 s after a terminal starts, and for 1.5 s after a resize or
  after leaving the terminal. For a Log View, the first 3 s after its source starts (e.g. the lines of `tail -n 5000`).
- Activity alerts are noisy with full-screen programs (far2l's clock): use keywords there.

---

## 10. Log View

A Log View is a window that shows the lines written by a command, filtered. It is not a terminal: you cannot type
into it, but it has its own history (up to `max-lines`), search, selection and save, and it never loses its place.

### Opening

**Ctrl-B D** opens the dialog:

| Field | Meaning |
|---|---|
| Title | empty: the command |
| Command | e.g. `tail -n 5000 -F /var/log/maillog` (↓ shows earlier commands) |
| Include | regular expression: only matching lines are shown (empty: all; ↓ shows values used before) |
| Exclude | regular expression: matching lines are hidden (empty: none; ↓ shows values used before) |
| Ignore case, Wrap long lines, Filters on | check boxes |
| Max lines | 0: `[logview] max-lines` of `tvterm.ini` |
| Restart after N s | 0: no automatic restart |
| Highlight sets | colors, e.g. `zimbra-mta kv` ([below](#colors-highlight)); empty: none; ↓ shows the sets and values used before |

Regular expressions are POSIX extended, like `grep -E`: alternatives with `|` (`reject:|bounced`), groups with `( )`.
Spaces at the end of the fields are kept (`dstport=993 ` does not match `dstport=9930`). Errors are shown in the dialog.

**Lists of the fields:** the arrow next to Command, Include, Exclude and Highlight sets (a click, or **↓** in the field)
opens a list; Enter or a double click takes the value, Esc closes the list. A value used before **replaces** the field
(a quick way back, e.g. after turning an Exclude off for a while). At Highlight sets the list starts with the sets; a set
is **added** to the end of the field (unless it is there already), then the values used before follow, after
`── used before ──`. The values are saved by OK and Apply (not empty ones, at most 20 per field, the newest first, without
repeats), are shared by all Log Views and are kept while tvterm runs.

The command runs with `$SHELL -c` in a pseudo-terminal, so programs like awk write each line at once. Colors and
escape sequences are removed; tabs are kept. Use `tail -F` (not `-f`), so that log rotation is followed.

**Log Views with exactly the same command share one source**: one process, one buffer (the buffer keeps the largest
`max-lines` asked for). Several windows with different filters on the same log cost one `tail`.

### Title

`[!keyword] Log: <title> [I] [E] shown/total (ended) [PAUSE +N]`: the alert mark, `[I]` / `[E]` when include / exclude are
set, `[F-]` when the filters are off, `[H-]` when the colors are off, lines shown / lines in the buffer, `(ended)` when the command has ended,
`[PAUSE +N]` while paused, `[+N]` new lines below the view.

### Keys

Also in the window's menu (**F9** or right click); the main ones are in the status line, clickable.

| Key | Action |
|---|---|
| F4 | settings dialog (OK / **Apply** / Cancel; Apply applies and keeps the dialog open) |
| `I` / `E` | the dialog, on Include / Exclude |
| `F` | filters on/off (all the lines shown) |
| `H` | colors on/off ([highlight sets](#colors-highlight)) |
| `D` | duplicate: a new window on the same source, with the same filters, colors and options (not the alerts), title + ` (2)` |
| `R` | restart the command |
| Space | pause / continue (new lines are kept, shown on continue) |
| `W` | long lines: wrapped / scrolled (Left / Right, 8 columns) |
| `X` | clear this window (not the source) |
| `/` | search (plain text or regular expression; match black on yellow) |
| F3 or `n` / Shift-F3 or `N` | next match, older / newer |
| Up / Down, PgUp / PgDn, Home / End | move the cursor line |
| Shift + those keys, Shift+click, drag | select lines |
| Ctrl-C or Ctrl-Ins | copy the selection, or else the cursor line |
| Ctrl-E | send the selection (or the cursor line) to a new editor |
| Ctrl-A | add it at the end of an open editor (chosen from a list) |
| F2 or `s` | save the lines shown, or the selection |
| Esc | clears the selection, then the search match |
| double click | copies a word |
| Ctrl-B K | alerts |

**Cursor line** (black on cyan): with the cursor on the last line, new lines are followed; anywhere higher, the view
stays put and `[+N]` counts the new lines; **End** follows again. The mouse wheel and the scroll bar move only the view;
at the bottom, following starts again.

New filters apply to the whole buffer at once. The clipboard gets the lines without the last line break, so pasting
into a shell runs nothing. Saving uses the same dialog as the history ([chapter 4](#save)); the proposed directory is
`save-dir`, or else tvterm's current directory.

### Colors (highlight)

A Log View can color its lines with **highlight sets**: write their names in the **Highlight sets** field of F4 (one or
more, with spaces or commas: `zimbra-mta kv`; the first set has priority). The arrow next to the field lists the sets. **H** turns the colors off and
on (`[H-]` in the title while off). Only the display changes: filters, search, copy and save use the plain text. The cursor line and the
selection keep their own colors, and the search match is drawn over the others.

Sets built into tvterm:

| Set | Colors |
|---|---|
| `zimbra-mta` | Postfix / amavis (`zimbra.log`, `maillog`): red `status=bounced`, `reject:`, `NOQUEUE`, `SASL … authentication failed`; yellow `status=deferred`, `warning:`; green `status=sent`, `Passed CLEAN`; each queue-ID in its own color; then `syslog` and `kv` |
| `zimbra-mailbox` | `mailbox.log`: ESMTP, IMAPS, Ban, reject:, `<a> -> <b>`…; then `levels`, `timestamps`, `kv` |
| `fortigate` | FortiGate: `srccountry` (Reserved / Romania on blue, others on red), `srcip=`, policy names, SMTPS / IMAPS; then `levels`, `timestamps`, `kv` |
| `fail2ban` | fail2ban: the logger, the action, the jail, the IP, Ban / DROP; then `levels`, `timestamps` |
| `nginx` | nginx: the status (5xx red, 4xx yellow, 2xx green), the method; then `timestamps`, `kv` |
| `rsync` | rsync: start, stop, sent, received, failed…; then `timestamps`, `kv` |
| `syslog` | the syslog time, dimmed (`Oct 29 10:06:17` or ISO `2026-10-29` + `T`, `^` or a space + `10:06:17`, with optional `.123` and `+02:00` / `Z`), and the program name (each in its own color) |
| `kv` | brackets, `;` `=`, keys (`key=`), e-mail addresses, IP addresses and ports, `ua=`, `mid=` |
| `levels` | DEBUG, INFO, WARN, ERROR, NOTICE, FATAL |
| `timestamps` | the usual date and time forms at the start of the line, and nginx's |

Your own sets, or changed copies of these, go in `tvterm.ini` as `[highlight <name>]` sections; a section with the name
of a built-in set replaces it (their text is in `source/tvterm/highlight.cc`, `builtinHighlightText`, in the same
format). One rule per line, in this order of use:

| Rule | Colors |
|---|---|
| `line = <color> "<regex>" [icase]` | the whole line (the first `line` rule that matches gives the base color) |
| `match = <color> "<regex>" [icase]` | every match |
| `groups = <color> "<regex>" [icase]` | only the text in parentheses, in every match |
| `hash = "<regex>" [icase]` | a color chosen from the value (the parentheses, or else the match): the same value always gets the same color |
| `include = <set> …` | the rules of other sets, at this place (put it last) |

Where rules overlap, **the first one wins**, so the specific rules go first and the `include`s at the end. Colors:
`black red green yellow blue magenta cyan white`, `bright-` before any of them, and `on <color>` for the background
(`bright-yellow on red`). Regular expressions as in the filters (like `grep -E`), always in double quotes; `\"` is a quote
inside them. From multitail: `cs_re` → `match`, `cs_re_s` → `groups`, `x,,bold` → `bright-x`, `.*?` → a negated class
(`ua=([^;]*);`).

```ini
[highlight my-mail]
line = white on red "status=bounced"
groups = bright-yellow "from=<([^>]*)>"
hash = "client=[^[]*\[([0-9.]+)\]"
include = zimbra-mta
```

The sets are read again with `tvterm.ini` (2 seconds after it is saved); an invalid rule is ignored and reported in the
Event Viewer with its line.

### When the command ends

The window shows `-- source ended (exit N) --`, the title `(ended)`, and the Event Viewer a note. **R** restarts it
(`-- source restarted --`), or `Restart after N s` does it automatically. Closing the last window of a source stops its
process.

Profiles save Log Views with all their settings, but not the pause or the selection.

---

## 11. Complete examples

The addresses and names below are examples (`192.0.2.x`, `example.com`); adapt the paths to your logs.
The mail log is `/var/log/maillog` on CentOS / RHEL; on Debian / Ubuntu it is `/var/log/mail.log`.

### Mail server (Postfix / Dovecot)

`~/.config/tvterm/profiles/mail.ini`:

```ini
[profile]
layout = manual
focus = 1

[terminal]
title = Shell
dir = ~
pos = 0 0 50 34

[logview]
title = MTA all
command = tail -n 5000 -F /var/log/maillog
exclude = amavis|clamd
pos = 0 34 50 66

[logview]
title = MTA problems
command = tail -n 5000 -F /var/log/maillog
include = status=(bounced|deferred)|reject:|NOQUEUE|authentication failed
alert-keywords = bounced, authentication failed
pos = 50 0 50 34

[logview]
title = Dovecot
command = tail -n 5000 -F /var/log/maillog
include = dovecot: (imap|pop3)-login:
alert-keywords = auth failed
pos = 50 34 50 33

[logview]
title = fail2ban
command = tail -n 2000 -F /var/log/fail2ban.log
include = " (Ban|Unban) "
alert-keywords = ] Ban
pos = 50 67 50 33
```

"MTA all", "MTA problems" and "Dovecot" share one `tail`. "Dovecot" shows the IMAP / POP3 logins; `auth failed`
appears both in older (`Disconnected (auth failed, ...)`) and newer (`Login aborted: ... (auth failed, ...)`)
versions. The keyword `] Ban` (the end of `[sshd]`, then `Ban`) does not match `Unban`; its mark is `[!"] Ban"]`. Selecting a queue ID in
"MTA all" and searching for it (`/`) shows the whole story of a message. Start it with `tvterm -p mail`, or set
`profile = mail` in `tvterm.ini`.

Webmail (nginx), server errors only:

```ini
[logview]
title = nginx 5xx
command = tail -n 5000 -F /var/log/nginx/access.log
include = " 5[0-9][0-9] [0-9]+ "
```

### TLS connections (instead of a `tail | awk` script)

A script such as

```sh
tail -n 10000 -F /var/log/maillog | awk '/TLS connection established from/ { ... print ... }'
```

can be the command of a Log View as it is: the pseudo-terminal makes awk write each line at once, and the window adds
search, selection, save and alerts. For long awk programs, put them in a file:

```awk
# ~/bin/show-tls.awk: IP (right-aligned), date, service (4 letters), host name
/TLS connection established from/ {
    split($9, a, "["); split(a[2], b, "]")
    split($3, s, "/")
    printf "%15s\t%s\t%s\t%s\n", b[1], $1, substr(s[2], 1, 4), a[1]
}
```

```ini
[logview]
title = TLS
command = tail -n 10000 -F /var/log/maillog | awk -f ~/bin/show-tls.awk
```

With mawk (the default `awk` on Debian / Ubuntu), write `mawk -W interactive -f ...`: otherwise mawk waits for
a full block of input from `tail` and the window stays empty. gawk (CentOS, RHEL) needs nothing.

Without awk, the raw lines: `command = tail -n 10000 -F /var/log/maillog` with
`include = TLS connection established from`.

### Firewall log (FortiGate through rsyslog), instead of multitail

multitail with several views on the same file translates to one `[logview]` per view, all with **the same command**
(so one `tail` and one buffer):

| multitail | tvterm |
|---|---|
| several `-e "a" -e "b"` (any of them) | `include = a|b` |
| several `-ev "a" -ev "b"` | `exclude = a|b` |
| `-i file` with `-n N -f` | `command = tail -n N -F file` |
| `-l "command"` | `command = command` |
| `-s 2` (layout) | `pos` in the profile, or `layout = tile-columns` |
| `-ke "regex"` (cut text out of the line) | not yet (planned: transformations) |

```ini
[profile]
layout = tile-columns

[logview]
title = policy 37
command = tail -n 50000 -F /var/log/fortigate.log
include = "policyid=37 "
highlight = fortigate

[logview]
title = mail ports
command = tail -n 50000 -F /var/log/fortigate.log
include = "dstport=(993|465|587) "
exclude = GEO-BLOCK|MAIL-993-ALLOW|MAIL-465-ALLOW|LAN-TO-WAN

[logview]
title = GEO block
command = tail -n 50000 -F /var/log/fortigate.log
include = GEO-BLOCK
alert-keywords = srcip=192.0.2.15

[logview]
title = script
command = bash ~/scripts/fg-log.sh -v srccountry=Reserved
```

Mind the trailing space: `policyid=37 ` (in quotes) does not match `policyid=370`. With `-n 50000` and FortiGate lines
(about 1 KB each), one source uses about 50 MB.

---

## 12. Known problems

- **"Cannot initialize Ncurses: 'newterm' failed"** on old servers (CentOS 7, Ubuntu 12) with the static build made on
  Alpine: its ncurses rejects the old terminfo files. Either copy a recent `xterm-256color` terminfo (e.g. from Alpine,
  `/usr/share/terminfo/x/xterm-256color`) to `~/.terminfo/x/` (one user) or `/etc/terminfo/x/` (all users) on the server;
  the old programs of the system keep working. Or build on Ubuntu 24 (glibc).
- In putty4far2l, **Shift+mouse** is used by its own selection: tvterm's Shift+click works only if that is disabled there.
- **Alt+Shift+Up/Down** arrive at tvterm through putty4far2l, so `key-window-up/down` set to them work.
- **Ctrl+Alt+arrows** may be taken by Windows before SSH (Left/Right on Windows 11, Up/Down on Windows 7, depending on the
  machine: graphics driver shortcuts or another program); they then reach neither tvterm nor the programs in it. Do not
  use them as global keys.
- **`` Ctrl-` ``** is recognized only through far2l's extensions (putty4far2l → far2l → tvterm); over a plain SSH terminal it
  arrives as Ctrl+Space (a NUL byte) and does nothing, so keep `Ctrl-B` in `menu-key` as well.
- Global keys and far2l: `F12` (far2l's screens), `Alt-Shift-arrows` (vertical block) and other far2l keys no longer reach
  far2l when set as global keys; add `:not-far2l` to such keys.
- Alert keywords and regular expressions with "Ignore case" fold only ASCII letters (`ș` does not match `Ș`).
  The plain text search of the history also folds diacritics.
- The history search uses ECMAScript regular expressions and the Log View POSIX ones (like `grep -E`): they differ in
  details (e.g. `\d` works only in the history).
- Log View: the scroll bar counts lines, not wrapped rows; a search finds the first match of each line; restarting a
  `tail -n N` source shows those N lines again.
- `WinList` in the status line does nothing while a dialog is open.
- On screens narrower than about 156 columns, the `Alerts: N` counter covers `WinList` in the Log View status line.
