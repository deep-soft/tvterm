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

---

## 2. Funcționalități (toate testate de utilizator în lanțul real)

### Meniu (Ctrl-B)
`N` New Term · `W` Close · `Tab`/`Shift-Tab` Next/Prev · `J` Window List · `T` Rename Term · `R` Resize/Move ·
`F` Maximize · `S` Select Text · `P` Paste · `E` New Editor · `O` Open File in Editor · `V` Event Viewer ·
`L` Load Profile · `A` Save Profile As · `More...` (change dir, tile, cascade, Grab Input) · `U` Suspend · `Q` Exit.
- **Tile / Cascade** (commit 25): aranjează doar terminalele (și celelalte ferestre aranjabile); Event Viewer și ferestrele de editor
  rămân pe loc. Celelalte folosesc cel mai mare dreptunghi care nu atinge niciuna dintre ele, fiecare fereastră fixă luată separat
  (commit 26; commit-ul 25 folosea dreptunghiul care le cuprindea pe toate, iar cu un editor stânga sus și Event Viewer în dreapta
  acesta acoperea tot Desktop-ul → terminalele erau aranjate peste ele). Fără ferestre fixe deschise sau cu dreptunghiul liber sub
  20 de coloane × 6 rânduri → tot Desktop-ul, ca înainte. Spațiul liber în formă de L nu se împarte în mai multe zone.
  Și pentru `layout` din profiluri. Cod: `TVTermDesk::freeArea` (`desk.cc`; `noFreeArea` spune apelantului că s-a folosit tot
  Desktop-ul).
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
- **Câmpurile de text din dialoguri** (Rename, Keywords, nume de fișiere, …; commit 23): Shift-Ins / Ctrl-V lipesc din clipboard-ul
  sistemului, Ctrl-Ins / Ctrl-C copiază textul selectat (fără selecție, Ctrl-C nu face nimic, ca înainte). Ctrl-V nu mai comută
  modul de suprascriere (Ins îl comută în continuare). Textul lipit (și cel venit din terminalul exterior, ex. click dreapta în PuTTY)
  e curățat: doar UTF-8 valid; rândurile noi, tab-urile și spațiile multiple → un spațiu; spațiile de la capete eliminate; caracterele
  de control (ESC, NUL, C1), de direcție (bidi) și de lățime zero eliminate; se procesează doar primii 64 KiB; tăiere la lungimea
  câmpului, la granița unui caracter. Înainte, câmpul folosea clipboard-ul intern tvision (gol pe server) și nu se lipea nimic.
- **Jurnal depanare**: `TVTERM_CLIPBOARD_LOG=/tmp/tvterm-clip.log tvterm`.

### Profiluri
- `tvterm -p NUME` → `~/.config/tvterm/profiles/NUME.ini` (sau `$XDG_CONFIG_HOME/...`); `tvterm -p /cale/fisier.ini`; `tvterm --help`.
- Secțiuni: `[profile]` (`layout` = auto|manual|tile-columns|tile-rows|cascade, `focus` = index 1-based),
  `[terminal]` (`title`, `dir`, `command`, `keep-open` yes/no, `pos`), `[editor]` (`file`, `pos`), `[eventviewer]` (`pos`).
- `pos = x y lățime înălțime` în **procente** din desktop, cu zecimale. Save Profile scrie pozițiile în interiorul Desktop-ului
  (o fereastră mutată parțial în afara ecranului e salvată ca mutată înapoi înăuntru; rotunjire la o zecimală, cu x + lățime ≤ 100);
  la încărcare, o poziție în afara Desktop-ului e mutată înăuntru (și micșorată dacă e mai mare), nu respinsă (commit 24;
  înainte: `invalid pos '84.9 0 15.4 72.7'`). Ajustarea e reținută în `Profile::notes`.
- `keep-open = yes` (implicit): după terminarea comenzii sau Ctrl+C, pornește shell-ul; `no`: rămâne „(Disconnected)” cu output vizibil.
- Comenzi care rulează continuu (`watch`, `tail -f`) trebuie oprite cu Ctrl+C ca să apară shell-ul.
- `mailq` poate lipsi din PATH pentru non-root → `/usr/sbin/mailq` sau `postqueue -p`.
- **Save Profile As** (Ctrl-B A): salvează toate ferestrele (ordine, focus, titlu fix, poziții, directorul curent real din `/proc/<pid>/cwd`,
  comanda din profil sau comanda din prim-plan detectată prin `tcgetpgrp` + `/proc/<pgid>/cmdline`).
  Textul editoarelor „Untitled” nu se salvează (avertisment).
- Load Profile / Save Profile As listează directorul de profiluri cu wildcard-ul `*.ini`; `..\` și directoarele merg normal,
  iar un nume tastat e relativ la directorul afișat (commit 21; înainte, calea completă apărea dublată: „Invalid drive or directory”).
- Load Profile cu ferestre deschise (commit 27): „Close the current windows first?” Yes / No / Cancel. Yes = terminalele sunt
  listate ca la ieșire (titlu „Close windows”, buton Close, Go to), editoarele cu modificări nesalvate întreabă de salvare (Cancel
  oprește încărcarea), apoi toate ferestrele se închid și profilul se deschide; No = ferestrele profilului se adaugă lângă cele
  deschise (ca înainte); Cancel = nu se încarcă nimic. `confirmQuit(deskTop, closing)` în `quitdlg.cc`,
  `TVTermApp::closeWindowsBeforeLoad` în `app.cc`.
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
- **Istoricul urmărește ieșirea nouă** (28): un terminal cu istoric putea să nu mai urmărească ieșirea și să rămână fixat mai sus
  (liniile noi adăugate dedesubt, bara de derulare arătând linii mai jos; revenea doar cu End sau o tastă). Cauza: o cursă între
  firul terminalului și firul interfeței. Când vederea își actualiza singură bara de derulare (`setParams`), Turbo Vision trimitea
  `cmScrollBarChanged` ca și cum utilizatorul ar fi mișcat-o, iar tvterm trimitea valoarea barei înapoi emulatorului; între timp
  firul terminalului putea adăuga linii, deci valoarea veche nu mai însemna „jos”. Apărea la rafale de ieșire (`tail -f`, alerte),
  cu fereastra selectată sau nu, cât timp istoricul nu era plin. Acum anunțul produs de `updateScrollBar` e ignorat (indicatorul
  `updatingScrollBar`), iar bara de derulare dusă până jos (click, tragere, rotiță) reia urmărirea ieșirii.
  Același cod există în magiblot `master` (candidat pentru PR).
- **Completare de director în dialogurile de fișiere** (29): în câmpul Name din Open file, Save file as, Load Profile și Save
  Profile As, la tastarea unei căi apare singură, sub câmp, lista directoarelor potrivite: subdirectoarele părții dinaintea
  ultimului `/` al căror nume începe cu textul de după el (`/var/` + `l` → `lib/`, `local/`, `lock/`, `log/`). Doar directoare;
  cele ascunse doar după `.` tastat; numele comparate cu diferență de majuscule; link-urile spre directoare incluse. O cale fără
  `/` e luată din directorul afișat în dialog, `~/` din `$HOME`; fără listă la câmp gol sau cu `*` / `?` după ultimul `/`.
  Taste: ↓ (cu lista deschisă) intră în listă; ↑/↓/PgUp/PgDn/Home/End se mișcă în ea; Enter (sau dublu-click) adaugă directorul
  cu `/` în câmp și listează subdirectoarele lui; ↑ pe primul rând revine în câmp; Esc închide lista (al doilea Esc închide
  dialogul); Tab o închide și trece la câmpul următor; orice altă tastă revine în câmp. Cu lista închisă totul merge ca înainte
  (↓ deschide istoricul câmpului, Enter deschide / schimbă directorul); lista de fișiere de dedesubt nu se schimbă la tastare.
  Cursorul rămâne în câmp și cât timp te miști în listă, așa că istoricul câmpului reține doar ce reținea și înainte (mutarea
  focusului în listă adăuga fiecare text intermediar). Cel mult 10 rânduri, cu bară de derulare când sunt mai multe. Textul lipit
  în câmp (commit 23) reface și el lista. `TVTermFileDialog` (`dircompl.cc`) înlocuiește `TFileDialog` în tvterm; tvision nu
  e modificat.

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
  dlgclip.cc/.h            clipboard în câmpurile din dialoguri (TInputLine cu focus; apelat din TVTermApp::getEvent)
  dircompl.cc/.h           completare de director în câmpul Name al dialogurilor de fișiere (TVTermFileDialog, dircompl::Completion)
  wnd.h                    TerminalWindow (+ launchCommand/launchDir/launchKeepOpen)
deps/vterm/patches/0001-win32-input-mode.patch   aplicat automat de CMake pe o copie (submodulul rămâne curat)
deps/tvision-fixes.cmake   corecturi tvision: histlist.cpp copiat în directorul de build, memcpy → memmove (submodulul curat)
deps/static-pthread.cmake  build static: libpthread întreagă cu glibc mai vechi de 2.34 (std::thread)
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
- **QNAP (QTS, x64)**: build-ul static de pe Alpine pornea monocrom. QTS setează `TERMINFO=/opt/share/terminfo` (director
  inexistent fără Entware; shell-ul vede această valoare chiar dacă `.profile` are `/usr/share/terminfo`), iar singurul
  `xterm` de pe QTS e din 2004 (`/usr/share/terminfo/x/xterm`; fără `infocmp` / `tput`). Soluție verificată, locală în
  folderul tvterm: `xterm-256color` de pe Alpine în `<folder-tvterm>/terminfo/x/` și un script de pornire `tvt.sh`
  (`#!/bin/sh`, `D` = folderul scriptului, `TERMINFO="$D/terminfo"`, `TERM=xterm-256color`, `exec "$D/tvterm"`). `/etc` nu se
  atinge (QTS îl reface la repornire). `tvterm`, `mc`, `top` color.
- **Build static cu glibc mai vechi de 2.34 (commit 30):** pe Raspbian 10 (Raspberry Pi 3, `armv7l`, glibc 2.28)
  `tvterm-static` se oprea imediat la pornire (cod de ieșire 134, „terminate called without an active exception”, apoi
  „recursively”); `--version` și build-ul dinamic mergeau. Cauza: înainte de 2.34, glibc ține funcțiile pentru fire de execuție
  într-un `libpthread.a` separat; linkul static ia doar obiectele cerute direct, iar `std::thread` (`termctrl.cc`) eșuează.
  `deps/static-pthread.cmake` (doar la build static) detectează glibc < 2.34 și leagă toată arhiva
  (`-Wl,--whole-archive -lpthread -Wl,--no-whole-archive`; mesaj la configurare: „glibc older than 2.34: linking the whole
  libpthread”). musl și glibc ≥ 2.34 (libpthread inclusă în libc) rămân neschimbate. Dacă opțiunea e deja în
  `CMAKE_EXE_LINKER_FLAGS` (ocolirea manuală), nu se mai adaugă: de două ori ar defini de două ori fiecare simbol pthread.
  `-DTVTERM_OLD_GLIBC=ON/OFF` înlocuiește detectarea. Ocolirea a fost verificată de utilizator înainte de commit (build
  manual; far2l 2.8.0 rulează în el).
- **„Illegal instruction” în dialogurile de fișiere (Alpine, commit 22):** istoricul câmpurilor din tvision (`histlist.cpp`:
  `deleteString`, `insertString`) muta intrările cu `memcpy` peste zone care se suprapun (originalul DOS folosea `movmem`).
  Cu `_FORTIFY_SOURCE` pe musl (fortify-headers), un `memcpy` cu suprapunere oprește programul cu `ud2` = „Illegal instruction”;
  glibc nu verifică. Declanșator: un nume ales din nou într-un câmp cu istoric după unul mai lung (ex. Load Profile `a1.ini`,
  `longername.ini`, `a1.ini`) sau istoricul plin (1024 de octeți). Istoricul e gol la pornire, de aceea blocarea părea aleatoare.
  `deps/tvision-fixes.cmake` folosește o copie a `histlist.cpp` cu `memmove` (mesaj la configurare: „Applied tvision fix”);
  dacă fișierul se schimbă upstream, se folosește originalul. Nu se aplică cu `TVTERM_USE_SYSTEM_TVISION`.

### Capcane cunoscute
- `.gitignore` din upstream ignoră **tot** în afară de directoare permise explicit; fișierele fără extensie trebuie permise
  (ex. `!scripts/*`). Directoare permise adăugate: `scripts/`, `examples/`, `docs/`.
- Sursele sunt găsite cu `GLOB` → după adăugarea unui fișier `.cc` nou trebuie rulat din nou `cmake`.
- Unity build: funcțiile `static` din fișiere diferite pot intra în conflict → namespace-uri proprii (`far2lext`, `far2lclip`).
- `TFileDialog`: câmpul `directory` pornește gol → numele tastate sunt relative la directorul curent; tvterm îl setează la directorul profilurilor.
  Wildcard-ul trebuie să fie doar un tipar (`*.ini`): tvision îl lipește la director (panoul de jos, câmpul Name pentru un director focusat).
  O cale completă acolo apare dublată → dialogul e creat cu `fdNoLoadDir`, apoi se setează `directory` și se citește lista (`profiledlg::newDialog`, `app.cc`).
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
- Clipboard fără far2l: un `xclip` fals în `PATH` (citește/scrie un fișier) + `DISPLAY=:99` → tvision îl folosește pentru
  TClipboard (câmpurile din dialoguri: Shift-Ins, Ctrl-V, Ctrl-Ins, Ctrl-C). `tmux paste-buffer -p` = bracketed paste din terminalul exterior.
- Teste de regresie folosite: OSC 52 (simplu, 150 KB, query ignorat, invalid), Shift-Ins în shell, lipire nativă far2l cu diacritice,
  bloc vertical far2l, bracketed paste în bash, profiluri (poziții, Ctrl+C → shell), salvare/reîncărcare profil, quit cu editor nesalvat,
  dialogul de ieșire (comenzi din profil, pipeline, comandă tastată, script, far2l, Go to, Esc, ecran 80x24).
- Note pentru testele cu tmux (28): într-un shell din tvterm, `tmux send-keys Enter` poate să nu ruleze comanda → se trimite `C-j`.
  `tmux send-keys` nu poate trimite evenimente de mouse către tvterm; secvențele SGR (`ESC[<0;x;yM` / `...m`, rotița `64`/`65`)
  scrise în tty-ul panoului cu `ioctl(TIOCSTI)` merg (`#{pane_tty}`; x/y de la 1). Testul pentru cursa istoricului: un script care
  scrie rafale de linii cu mici pauze, apoi un marcaj; la final marcajul trebuie să fie vizibil (build-ul de bază s-a blocat 7/7).
- Completarea de director (29): `dircompl::matches` verificat cu un mic test C++ legat cu `dircompl.cc` și `libtvision.a`
  (prefix, majuscule, ascunse, căi relative / `~/` / absolute, link-uri, `*`, limită). În tmux, caracterele trimise într-un singur
  `send-keys` sosesc împreună și contează ca paste; prima rafală într-un câmp cu textul selectat (ex. `*` la deschiderea
  dialogului) păstrează doar primul caracter, și înainte de 29 → caracterele se trimit pe rând. Istoricul câmpului (↓ cu lista
  închisă) nu trebuie să conțină texte intermediare.
- libpthread la build static (30): `deps/static-pthread.cmake` încercat singur într-un proiect CMake mic: gcc (glibc 2.39) și
  `musl-gcc` → neactivat; un `features.h` fals cu glibc 2.28 / 2.33 → activat, 2.34 / 3.0 → neactivat; opțiunea deja în
  `CMAKE_EXE_LINKER_FLAGS` → nu se dublează; `-DTVTERM_OLD_GLIBC=ON/OFF` respectat. Comanda de link statică a lui tvterm pe
  glibc 2.39 e identică cu cea dinainte de 30; forțat cu `-DTVTERM_OLD_GLIBC=ON` pe glibc 2.39, build-ul static se leagă și
  pornește. Testul real: Raspberry Pi (utilizatorul).

---

## 6. Limitări cunoscute

- Marcajul de bloc vertical nu trece prin Windows (doar între far2l-uri din același tvterm).
- `ClientOptions` și info procese: neimplementate pe Windows.
- Win32-input-mode: scan code 0; key-up trimis imediat după key-down; unele taste (simboluri cu Shift, cifrele de pe tastatura
  numerică) au `vk = 0`.
- Starea „pauză” a Event Viewer-ului nu se salvează în profil.
- tvision (pentru un PR la magiblot): `histlist.cpp` memcpy → memmove (corectat local, commit 22); `TFileList::getText` adaugă `\` după ce copiază până la 255 de caractere într-un buffer de 256
  → un octet în plus la un director cu numele de exact 255 de octeți; `hotKeyStr` (tinputli.cpp) primește pointer nul de la separatoarele meniului;
  `fmtstr.cpp` `vFormatStr`: GCC 15 + fortify-headers (Alpine) avertizează la `vsprintf` → propunere `vsnprintf(buf, n + 1, …)`.

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
| Dialoguri de fișiere | 21 | dialogurile de profil: wildcard `*.ini` + directorul setat separat (fără cale dublată, `..\` merge) |
| Istoric tvision | 22 | `memmove` în istoricul câmpurilor (cauza „Illegal instruction” din dialogurile de fișiere pe Alpine) |
| Clipboard în dialoguri | 23 | Shift-Ins / Ctrl-V / Ctrl-Ins / Ctrl-C în câmpurile din dialoguri, textul lipit curățat |
| Poziții în profiluri | 24 | poziții salvate în interiorul Desktop-ului; cele în afară, mutate în interior la încărcare, nu respinse |
| Tile / Cascade | 25 | Event Viewer și editoarele rămân pe loc; celelalte ferestre folosesc zona lăsată liberă |
| Zona pentru Tile | 26 | cel mai mare dreptunghi lângă ferestrele fixe, fiecare luată separat (editorul stânga sus nu mai e acoperit) |
| Load Profile | 27 | întreabă dacă se închid întâi ferestrele deschise (Yes / No / Cancel) |
| Urmărirea istoricului | 28 | vederea nu mai încetează să urmărească ieșirea după rafale; bara de derulare dusă jos reia urmărirea |
| Completare director | 29 | lista directoarelor potrivite sub câmpul Name din dialogurile de fișiere (↓, Enter, Esc, Tab) |
| libpthread static | 30 | build-urile statice cu glibc < 2.34 (Raspbian 10) leagă toată libpthread; nu se mai opresc la pornire |
