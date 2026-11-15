# tvterm (deep-soft fork)

This is [deep-soft/tvterm](https://github.com/deep-soft/tvterm), a fork of
[magiblot/tvterm](https://github.com/magiblot/tvterm) extended for daily work on servers, typically in the chain
`Far (Windows) → putty4far2l → far2l (on the server) → tvterm → bash / far2l / mc`. The original README follows
below, unchanged.

## Branches

| Branch | Content |
|---|---|
| `far2l-extended` (default) | the working branch: everything below |
| `far2l-integration` | generic fixes and the far2l integration only, kept small for pull requests to magiblot |
| `master` | a copy of magiblot's `master` |

## Main additions (`far2l-extended`)

- Clipboard in every direction (OSC 52, far2l extensions, bracketed paste), far2l keys and vertical blocks.
- Profiles (windows, positions, commands), desktops 1-9, Window List, top bar, Tile / Cascade around fixed windows.
- History (scrollback) with search and save; an editor with Find / Replace, undo, backups, read only.
- Log View: `tail -F` windows with filters, highlight and transformation sets, tables, alerts (levels 1-5, actions,
  notifications, mute).
- Settings file `~/.config/tvterm/tvterm.ini` (never written by tvterm itself), Event Viewer, Capture Desk / Window.

How to use it: [docs/MANUAL.md](docs/MANUAL.md). What changed, how and why: [docs/HISTORY.md](docs/HISTORY.md).

## Building

`scripts/build.sh [dynamic|static|both]` builds `bin/tvterm-dynamic` and / or `bin/tvterm-static` (the required
packages are listed at the top of the script; a static build made on Alpine runs on most Linux systems).
`tvterm --version` shows the version, commit and build date.

## Versions

Signed tags `vX.Y.Z` with GitHub [Releases](https://github.com/deep-soft/tvterm/releases) at milestones. The first
line of a `tvterm.ini` made by tvterm says which format and version made it, e.g.
`## format 6, created by tvterm v0.3.0 (<commit>, built <date>)`.

## License

MIT, as the original: see [COPYRIGHT](COPYRIGHT).

---

# Original README (magiblot/tvterm)

A terminal emulator that runs in your terminal. Powered by Turbo Vision.

![htop, turbo and notcurses-demo running in tvterm](https://user-images.githubusercontent.com/20713561/137407902-27538f99-dc0e-47a8-9bf2-0705733d8a8c.png)

`tvterm` is an experimental terminal emulator widget and application based on the [Turbo Vision](https://github.com/magiblot/tvision) framework. It was created for the purpose of demonstrating new features in Turbo Vision such as 24-bit color support.

`tvterm` relies on Paul Evan's [libvterm](http://www.leonerd.org.uk/code/libvterm/) terminal emulator, also used by [Neovim](https://github.com/neovim/libvterm) and [Emacs](https://github.com/akermu/emacs-libvterm).

Additionally, `tvterm` supports both Unix and Windows (Windows 10 1809 or later).

The original location of this project is https://github.com/magiblot/tvterm.

# Building

First of all, you should clone this repository along its submodules with the `--recursive` option of `git clone` (or run `git submodule init && git submodule update` if you have already cloned it).

Then, make sure the following dependencies are installed:

* CMake.
* A compiler supporting C++14.
* `libvterm`:
    * If you initialized the submodules, you can build `libvterm` as part of `tvterm`. Perl is needed for building `libvterm`.
    * Otherwise, a system-provided `libvterm` (e.g. `libvterm-dev` in Ubuntu) can be used if enabling the CMake option `-DTVTERM_USE_SYSTEM_LIBVTERM=ON`.
* [Turbo Vision](https://github.com/magiblot/tvision#build-environment)'s dependencies:
    * `libncursesw` (Unix only) (e.g. `libncursesw5-dev` in Ubuntu).
    * `libgpm` (optional, Linux only) (e.g. `libgpm-dev` in Ubuntu).
* Turbo Vision itself:
    * If you initialized the submodules, you can build Turbo Vision as part of `tvterm`.
    * Otherwise, clone [Turbo Vision](https://github.com/magiblot/tvision) separately and follow its [build](https://github.com/magiblot/tvision#build-environment) and [install](https://github.com/magiblot/tvision#build-cmake) instructions. Make sure you don't use a version of Turbo Vision older than the one required by `tvterm` (specified in the [`tvision` submodule](https://github.com/magiblot/tvterm/tree/master/deps)). When building `tvterm`, enable the CMake option `-DTVTERM_USE_SYSTEM_TVISION=ON`.

`tvterm` can be built with the following commands:

```sh
cmake . -B ./build -DCMAKE_BUILD_TYPE=Release && # Could also be 'Debug', 'MinSizeRel' or 'RelWithDebInfo'.
cmake --build ./build
```

CMake versions older than 3.13 may not support the `-B` option. You can try the following instead:

```sh
mkdir -p build; cd build
cmake .. -DCMAKE_BUILD_TYPE=Release &&
cmake --build .
```

# Features

This project is still WIP. Some features it may achieve at some point are:

- [x] UTF-8 support.
- [x] fullwidth and zero-width character support.
- [x] 24-bit color support.
- [x] Windows support.
- [x] Scrollback.
- [x] Text selection.
- [ ] Find text.
- [ ] Send signal to child process.
- [ ] Text reflow on resize.
- [ ] Having other terminal emulator implementations to choose from.
- [ ] Better dependency management.
