# HISTORY — tvterm (deep-soft)

Istoricul dezvoltării din fork-ul deep-soft/tvterm: ce s-a implementat, cum funcționează,
cum se testează și ce capcane au apărut. Document de referință, actualizat la fiecare
funcționalitate nouă. Starea curentă și pașii următori sunt în STATUS.md (în Project-ul Claude).

Limba: română. Versiunea în engleză: [HISTORY.md](HISTORY.md); cele două se actualizează împreună.

## 1. Repository și branch-uri

| Branch | Rol |
|---|---|
| `far2l-integration` | **branch principal, stabil** (peste `magiblot/master` 210eb23) |
| `master` | copie la zi a lui magiblot |

Branch-urile vechi (`evntedit`, `evntview` cu binare, `cleanup`, `ci-test`, `static-test-1`) au fost **șterse**.

**Strategie stabilită:** fiecare funcționalitate nouă într-un branch `feat/<nume>` pornit din `far2l-integration`;
după testare: `git checkout far2l-integration && git merge --ff-only feat/<nume> && git push`.
Lucrăm la **o funcționalitate pe rând** (multe ating aceleași fișiere, ex. meniul din `app.cc`).

### Commit-uri pe `far2l-integration` (de la cel mai vechi)

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
18. HISTORY împărțit în engleză (`HISTORY.md`) și română (`HISTORY-ro.md`)
19. Show what runs in each terminal when quitting
20. Pass Ctrl+\ [ ] _ and the keypad operators to programs

---

## 2. Funcționalități (toate testate de utilizator în lanțul real)

### Meniu (Ctrl-B)
`N` New Term · `W` Close · `Tab`/`Shift-Tab` Next/Prev · `J` Window List · `T` Rename Term · `R` Resize/Move ·
`F` Maximize · `S` Select Text · `P` Paste · `E` New Editor · `O` Open File in Editor · `V` Event Viewer ·
`L` Load Profile · `A` Save Profile As · `More...` (change dir, tile, cascade, Grab Input) · `U` Suspend · `Q` Exit.
Litere libere în meniu: B, C, D, G, H, I, K, M, X, Y, Z (unele au rol în More).

### Clipboard (scop principal, funcționează în toate direcțiile)
- **Editor tvterm** (Ctrl-B E): Ctrl-Ins / Shift-Ins / Shift-Del folosesc clipboard-ul sistemului (= Windows prin far2l/putty4far2l). F2 salvează.
- **OSC 52**: programele din tvterm (vim, tmux, scripturi) pot scrie în clipboard; citirea e refuzată intenționat (securitate).
  `scripts/tvterm-copy`: `comanda | tvterm-copy`.
- **Extensiile far2l (clipboard)**: tvterm răspunde la handshake `ESC _far2l1`; far2l din tvterm apare ca `TTY|F`
  și are acces complet la clipboard-ul Windows (Ctrl+C/V, Ctrl+Ins/Shift+Ins).
  - Permisiunea în far2l exterior: „Share clipboard always”.
  - Blocuri verticale far2l păstrate (marcaj `FAR_VerticalBlock_Unicode` ținut în tvterm, legat de text).
- **Shift-Ins** în terminal: lipește prin tvterm, **doar** dacă programul nu folosește extensiile far2l și nu în Grab Input.
- **Bracketed paste**: textul lipit e trimis dintr-o bucată, încadrat `ESC[200~ … ESC[201~` dacă programul cere; caracterele de control (ESC) sunt eliminate.
- **Jurnal depanare**: `TVTERM_CLIPBOARD_LOG=/tmp/tvterm-clip.log tvterm`.

### Profiluri
- `tvterm -p NUME` → `~/.config/tvterm/profiles/NUME.ini` (sau `$XDG_CONFIG_HOME/...`); `tvterm -p /cale/fisier.ini`; `tvterm --help`.
- Secțiuni: `[profile]` (`layout` = auto|manual|tile-columns|tile-rows|cascade, `focus` = index 1-based),
  `[terminal]` (`title`, `dir`, `command`, `keep-open` yes/no, `pos`), `[editor]` (`file`, `pos`), `[eventviewer]` (`pos`).
- `pos = x y lățime înălțime` în **procente** din desktop, cu zecimale.
- `keep-open = yes` (implicit): după terminarea comenzii sau Ctrl+C, pornește shell-ul; `no`: rămâne „(Disconnected)” cu output vizibil.
- Comenzi care rulează continuu (`watch`, `tail -f`) trebuie oprite cu Ctrl+C ca să apară shell-ul.
- `mailq` poate lipsi din PATH pentru non-root → `/usr/sbin/mailq` sau `postqueue -p`.
- **Save Profile As** (Ctrl-B A): salvează toate ferestrele (ordine, focus, titlu fix, poziții, directorul curent real din `/proc/<pid>/cwd`,
  comanda din profil sau comanda din prim-plan detectată prin `tcgetpgrp` + `/proc/<pgid>/cmdline`).
  Textul editoarelor „Untitled” nu se salvează (avertisment).
- Documentație completă: `examples/profiles/example.ini`.

### Altele
- **Window List** (Ctrl-B J): Go / Rename / Close. **Rename Term** (Ctrl-B T); titlu gol = titlu automat.
- **Event Viewer** (Ctrl-B V): afișează evenimentele Turbo Vision (diagnostic taste); a doua apăsare = pauză.
- **Ieșire** (Ctrl-B Q): confirmarea listează fiecare terminal deschis și ce rulează în el, întâi cele care rulează un program:
  comanda din prim-plan (ex. `tail -f /var/log/mail.log`, pipeline-uri ca `a | b`, `bash script.sh`), `bash, idle` pentru un shell liber,
  `[stays in background]` pentru far2l. Butoane: **Quit** (Enter sau Q), **Go to** (G, Space sau dublu click: trece la terminalul
  ales și nu iese), Cancel (Esc). Editoarele cu modificări nesalvate întreabă în continuare, după aceea.
  Programele primesc SIGHUP și se opresc; **far2l rămâne în fundal** și poate fi reconectat (verificat).
  Cum se află comanda: procesele din grupul de procese din prim-plan al terminalului (`tcgetpgrp`) se citesc din `/proc/<pid>/stat`;
  shell-urile care doar rulează o linie de comandă (`sh -c ...`, ex. wrapper-ul folosit pentru comenzile din profil) sunt sărite;
  far2l își rescrie `argv[0]`, așa că numele lui se ia din `/proc/<pid>/stat`.
- Alt+Shift+Sus/Jos (selecție verticală far2l) nu ajung nici în afara tvterm → problemă în putty4far2l/Windows, nu în tvterm.
  Ocolire: Alt+Shift+Stânga/Dreapta + Alt+Shift+PgUp/PgDn.
- **Ctrl+\ [ ] _ și operatorii de pe tastatura numerică**: prin extensiile far2l ajung ca și caractere de control
  (Ctrl+\ = `0x1C`, adesea cu codul de scanare: keyCode `0x2B1C`), pe care Turbo Vision nu le transformă în tastă. tvterm
  găsește tasta după codul de scanare (sau după caracterul de control) și trimite către far2l (win32-input-mode) VK-ul corect
  (VK_OEM_5/4/6, VK_OEM_MINUS) cu caracterul de control, ca Windows, iar către celelalte programe octetul de control
  (`0x1C`, `0x1B`, `0x1D`, `0x1F`). `* + - /` de pe tastatura numerică (coduri de scanare 0x37, 0x4E, 0x4A, 0x35 + enhanced)
  primesc VK_MULTIPLY/ADD/SUBTRACT/DIVIDE (aveau `vk = 0`). În far2l: Ctrl+\ duce în `/`, Ctrl+[ / Ctrl+] inserează calea
  panoului stâng/drept.

---

## 3. Arhitectură și fișiere cheie

```
source/tvterm-core/        (biblioteca, potențial interesantă pentru upstream)
  vtermemu.cc              emulator (fir de lucru): libvterm, win32-input, OSC 52, APC far2l, paste
  far2lext.cc/.h           protocolul extensiilor far2l (clipboard), ExtraFormatStore (blocuri verticale)
  termview.cc              fir principal: clipboard (TClipboard, citire asincronă cu timeout 3 s), Shift-Ins, paste
  termwnd.cc               fereastra terminal: titlu fix (setCustomTitle)
  termctrl.cc              controller (fire reader/writer), getClientProcessInfo
  pty.cc                   forkpty, ClientOptions (command/dir/keepOpen; wrapper /bin/sh cu `trap : INT`)
include/tvterm/            headere publice (termemu.h: TerminalState, TerminalEvent; pty.h; termctrl.h; ...)
source/tvterm/             aplicația
  app.cc/.h                meniu, status line, comenzi, profiluri (open/save), main() cu -p/--help
  profile.cc/.h            parser/writer INI (WindowProfile, Profile)
  winlist.cc/.h            dialogul Window List, renameTerminal, getDeskTopWindows
  quitdlg.cc/.h            dialogul de confirmare a ieșirii (confirmQuit)
  procinfo.cc/.h           funcții /proc: readLink, readCommandLine, describeForeground (ce rulează într-un terminal)
  editwnd.cc/.h            editor (dialoguri Save As etc.)
  evntview.cc/.h           Event Viewer
  wnd.h                    TerminalWindow (+ launchCommand/launchDir/launchKeepOpen)
deps/vterm/patches/0001-win32-input-mode.patch   aplicat automat de CMake pe o copie (submodulul rămâne curat)
scripts/build.sh           build dinamic/static → bin/tvterm-dynamic, bin/tvterm-static (+ loguri)
scripts/tvterm-copy        copiere în clipboard prin OSC 52
examples/profiles/example.ini
```

**Fluxuri importante:**
- Fir de lucru → fir principal prin `TerminalState` (sub mutex), preluat la broadcast `cmCheckTerminalUpdates` (din `idle`).
- Fir principal → fir de lucru prin `TerminalEvent` (`termCtrl.sendEvent`); tipuri adăugate: `Far2lClipboardData`, `Paste`
  (date alocate cu `new char[]`, eliberate de receptor).
- far2l **așteaptă la nesfârșit** răspunsul la cererile cu ID ≠ 0 → tvterm răspunde întotdeauna (inclusiv timeout gol).

---

## 4. Build și flux de lucru

- Build: `scripts/build.sh [dynamic|static|both]`; static = `cmake -DTVTERM_STATIC=ON`.
  Pachete Alpine: `build-base cmake perl patch git linux-headers ncurses-dev ncurses-static gpm-dev`.
- **Livrare:** Claude generează patch-uri cu `git format-patch` peste starea de pe GitHub;
  utilizatorul: `git am ../x.patch && scripts/build.sh static && git push`. Dacă un patch e deja aplicat: `git am --skip`.
- Autor commit-uri: `deep-soft <deep-soft@users.noreply.github.com>`.
- **Build static și sisteme vechi**: executabilul static conține ncurses-ul mașinii de build, care citește la pornire
  descrierea terminalului (terminfo, ex. `xterm-256color`). ncurses de pe Alpine (musl) respinge fișierele terminfo vechi
  (ex. cel din 2011 de pe Ubuntu 12, CentOS 7.9): fișierul e găsit și deschis, apoi `newterm` eșuează („Cannot initialize
  Ncurses”). Două soluții verificate: (1) build pe Ubuntu 24 (glibc + ncurses 6.4), care merge pe Ubuntu 12 și CentOS 7.9;
  (2) un `xterm-256color` nou (ex. de pe Alpine) copiat în `~/.terminfo/x/` (un utilizator) sau `/etc/terminfo/x/` (toți).
  Programele vechi ale sistemului (ncurses 5.9) sar peste fișierul în format nou și îl folosesc pe al lor (verificat `top`, `mc`).

### Capcane cunoscute
- `.gitignore` din upstream ignoră **tot** în afară de directoare permise explicit; fișierele fără extensie trebuie permise
  (ex. `!scripts/*`). Directoare permise adăugate: `scripts/`, `examples/`, `docs/`.
- Sursele sunt găsite cu `GLOB` → după adăugarea unui fișier `.cc` nou trebuie rulat din nou `cmake`.
- Unity build: funcțiile `static` din fișiere diferite pot intra în conflict → namespace-uri proprii (`far2lext`, `far2lclip`).
- `TFileDialog`: câmpul `directory` pornește gol → numele tastate sunt relative la directorul curent; tvterm îl setează la directorul profilurilor.
- Cod win32-input protejat cu `HAVE_VTERM_WIN32INPUT` (doar cu libvterm-ul inclus, patch-uit).

---

## 5. Testare automată (pentru Claude, în container)

- Clone: `git clone -b far2l-integration --recurse-submodules https://github.com/deep-soft/tvterm.git`
  + `git remote add up https://github.com/magiblot/tvterm.git`.
- Dependențe container Ubuntu: `apt-get install libncurses-dev libgpm-dev`, `pip install cmake pyte --break-system-packages`;
  far2l: `apt-get update && apt-get install --no-install-recommends far2l` (2.6.0, are OSC52 și extensii).
- Harness: Python `pty.fork()` + `pyte` (ecran virtual); env `TERM=xterm-256color SHELL=/bin/sh XDG_CONFIG_HOME=/tmp/cfg`.
  - OSC 52 trimis de tvterm spre exterior se verifică în output-ul brut (`ESC]52;...`).
  - Lipire din exterior: `ESC[200~text ESC[201~`. Taste: Ctrl+Ins `ESC[2;5~`, Shift+Ins `ESC[2;2~`, Alt+Shift+săgeți `ESC[1;4A-D`.
  - **Text + Enter trimise în aceeași bucată sunt tratate ca paste** de tvision (Enter-ul nu confirmă dialogul) → trimise separat.
  - pyte poate arunca excepții la ieșirea tvterm → `st.feed` în try/except.
  - far2l lasă sesiuni în fundal (`/tmp/far2l_<uid>_*`) → `pkill -9 far2l; rm -rf /tmp/far2l_0_0` între teste.
  - far2l 2.6 în modul extensii: titlul conține `TTY|F`.
- Teste de regresie folosite: OSC 52 (simplu, 150 KB, query ignorat, invalid), Shift-Ins în shell, lipire nativă far2l cu diacritice,
  bloc vertical far2l, bracketed paste în bash, profiluri (poziții, Ctrl+C → shell), salvare/reîncărcare profil, quit cu editor nesalvat,
  dialogul de ieșire (comenzi din profil, pipeline, comandă tastată, script, far2l, Go to, Esc, ecran 80x24).

---

## 6. Limitări cunoscute

- Marcajul de bloc vertical nu trece prin Windows (doar între far2l-uri din același tvterm).
- `ClientOptions` și info procese: neimplementate pe Windows.
- Win32-input-mode: scan code 0; key-up trimis imediat după key-down; unele taste (simboluri cu Shift, cifrele de pe tastatura
  numerică) au `vk = 0`.
- Starea „pauză” a Event Viewer-ului nu se salvează în profil.

---

## 7. Jurnal pe etape

| Etapă | Commit-uri | Ce s-a făcut |
|---|---|---|
| Curățenie inițială | 1–5 | branch nou din upstream; win32-input-mode cu patch libvterm aplicat automat; build static din CMake; editor; Event Viewer; scripts/build.sh |
| Clipboard | 6–11 | OSC 52; Shift-Ins; jurnal depanare; extensii far2l (clipboard); bracketed paste; blocuri verticale far2l |
| Profiluri | 12–16 | ClientOptions + titlu fix; profiluri INI; info procese; Window List, Rename, Save Profile; editor și Event Viewer în profiluri |
| Organizare | 17–18 | docs/HISTORY.md, apoi împărțit în HISTORY.md (engleză) și HISTORY-ro.md (română); STATUS.md ținut în Project-ul Claude |
| Info la ieșire | 19 | confirmarea de ieșire listează ce rulează în fiecare terminal, cu Go to |
| Taste Ctrl | 20 | Ctrl+\ [ ] _ și operatorii de pe tastatura numerică ajung la far2l și la celelalte programe |
