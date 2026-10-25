# HISTORY — tvterm (deep-soft)

Istoricul dezvoltării din fork-ul deep-soft/tvterm: ce s-a implementat, cum funcționează,
cum se testează și ce capcane au apărut. Document de referință, actualizat la fiecare
funcționalitate nouă. Starea curentă și pașii următori sunt în STATUS.md (în Project-ul Claude).

Limba: română. Versiunea în engleză: [HISTORY.md](HISTORY.md); cele două se actualizează împreună.

## 1. Repository și branch-uri

| Branch | Rol |
|---|---|
| `far2l-integration` | **branch principal, stabil** (peste `magiblot/master` 210eb23): părțile generice |
| `far2l-extended` | branch de lucru pentru funcționalitățile noi, peste `far2l-integration` |
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

### Commit-uri pe `far2l-extended`

20. Add history mode (scroll, search, save), word/line selection and a settings file
21. Add version information (--version) and an About dialog
22. Add filters to the Event Viewer

---

## 2. Funcționalități (toate testate de utilizator în lanțul real)

### Meniu (Ctrl-B)
`N` New Term · `W` Close · `Tab`/`Shift-Tab` Next/Prev · `J` Window List · `T` Rename Term · `R` Resize/Move ·
`F` Maximize · `S` Select Text · `P` Paste · `E` New Editor · `O` Open File in Editor · `V` Event Viewer ·
`L` Load Profile · `A` Save Profile As · `H` History · `More...` (change dir, tile, cascade, Grab Input, Edit settings,
Reload settings, About) · `U` Suspend · `Q` Exit.
Litere libere în meniu: B, C, D, G, I, K, M, X, Y, Z (unele au rol în More).

### Istoric (scrollback) — `far2l-extended`
- Fiecare terminal păstrează până la `lines` (implicit 10000) linii ieșite de pe ecran. Doar ecranul normal are istoric:
  programele pe tot ecranul (far2l, mc, multitail, editoare) folosesc ecranul alternativ, unde nu există istoric.
- **Modul istoric**: Ctrl-B H sau direct **Alt-Shift-PgUp/PgDn/Home/End** (configurabile; în programele pe tot ecranul și în
  modul Grab Input aceste taste ajung la program, ca până acum). Titlul arată `(History)`, iar bara de stare tastele:
  Sus/Jos, PgUp/PgDn, Home/End (și Ctrl-Home/End) derulează; rotița derulează; **Esc** anulează selecția, apoi potrivirea,
  apoi iese din mod; orice altă tastă iese, revine jos și ajunge la program. Intrat cu o tastă directă, modul se închide
  și când revii jos de tot. Ieșirea programului continuă să se adune între timp.
- **Căutare**: `/` sau F7. Text simplu (implicit fără diferență între litere mari/mici, inclusiv la diacritice: `ăla știe`
  găsește `Ăla ȘTIE`) sau expresie regulată (ECMAScript, ca `grep -E`). Pornește de la liniile vizibile spre cele mai vechi;
  **F3 / n** mai veche, **Shift-F3 / N** mai nouă; potrivirea apare negru pe galben; liniile rupte se caută ca o singură linie.
  Ctrl-C (sau Ctrl-Ins) copiază selecția, altfel potrivirea. Fără rezultat: mesaj în bara de stare.
- **Salvare**: F2 sau `s` (Ctrl-B H într-un program pe tot ecranul deschide direct dialogul, pentru ecranul vizibil). Nume propus:
  `tvterm-<titlu>-YYYY-MM-DD-hhmm.txt` în `save-dir`, altfel în directorul curent al programului din terminal; un nume relativ
  e relativ la acel director. Tot (istoric + ecran) sau doar selecția. Text simplu UTF-8, fără spații la capăt, cu liniile rupte
  lipite, fără liniile goale de la început/sfârșit. Fișier existent: Overwrite / Append / Cancel.
  Rezultatul apare în bara de stare și în Event Viewer.
- **Mouse**: dublu click selectează un cuvânt (litere, cifre, `word-chars`; orice caracter non-ASCII contează ca literă),
  triplu click toată linia logică (cu sfârșitul de linie); tragerea după aceea extinde pe cuvinte/linii. **Shift+click**
  forțează selecția în programele care folosesc mouse-ul (far2l), dacă `shift-click-select = yes`. Copierea, ca până acum (Ctrl-C).
  În lanțul real, putty4far2l prinde Shift+mouse pentru selecția proprie (pe tot ecranul exterior), deci tvterm nu primește
  Shift+click cât timp acea selecție e activă în putty4far2l (utilizatorul a dezactivat-o). Selecția tvterm (fără Shift,
  în ferestre shell) funcționează și rămâne în fereastra curentă.
- Liniile vechi eliminate dintr-un istoric plin nu mai deplasează selecția sau potrivirea.

### Versiune și About — `far2l-extended`
- Număr de versiune `0.1.0`, în `CMakeLists.txt` (`TVTERM_VERSION`), schimbat doar manual. Restul se adaugă automat la
  **fiecare build** de `scripts/buildinfo.cmake`: commit-ul (`git describe --always --dirty`: hash scurt, `-dirty` cu modificări
  necommit-uite), branch-ul, data compilării în ora locală a mașinii de build, cu fus (`YYYY-MM-DD hh:mm TZ`, ex.
  `2026-10-25 16:40 EET`; o mașină fără fus orar setat arată `UTC`), tipul build-ului (`static`/`dynamic`) și commit-urile
  tvision și libvterm incluse. Fără git (ex. surse dintr-o arhivă): `unknown`.
- `tvterm --version` (sau `-V`) le afișează și iese, fără să pornească interfața.
- Primele două linii din Event Viewer: `tvterm 0.1.0 (deep-soft), <branch> <commit>` și `built <dată>, <tip>`.
- **More... > About tvterm...**: versiunea, commit-ul și branch-ul, data și tipul build-ului, commit-urile tvision/libvterm,
  fișierul de setări (`(not found)` dacă nu există), ultimul profil încărcat (`(none)`), uptime (`2d 03:14`: zile, ore, minute),
  numărul de ferestre terminal (care rulează sau deconectate) și link-urile (fork deep-soft, originalul magiblot).
  Valorile se citesc la deschiderea dialogului. Căile lungi se scurtează la început (`...`) pe ecrane înguste.

### Filtre în Event Viewer — `far2l-extended`
- Taste, când fereastra Event Viewer are focus (nu apar ca evenimente): **M** mișcări de mouse, **C** butoane de mouse,
  rotiță și auto-repetare, **K** taste, **O** alte evenimente (comenzi etc.), **T** mesajele tvterm — fiecare afișează/ascunde
  acel tip; **N** golește fereastra și reafișează mesajele reținute de tvterm (ultimele 200, cu cele două linii de versiune
  în frunte), chiar dacă T e oprit; **X** golește fereastra. Bara de stare le afișează.
- Titlul arată starea: `Event Viewer [M- C+ K+ O+ T+]` (`(Stopped)` înaintea parantezelor când e pe pauză).
  Implicit: mișcările de mouse ascunse, restul afișate. Filtrul se aplică doar evenimentelor noi.
- Evenimentele ascunse se numără în continuare: golurile din `Received event #N` le arată.
- Buffer: 64000 de octeți (aproximativ 220 de evenimente cu toate liniile lor), în loc de 3840; `TTerminal` din Turbo Vision
  îl limitează la 32000, așa că `EventTerminal` din tvterm înlocuiește bufferul după construcție (poziții pe 16 biți, până la 65535).
- Profiluri: `[eventviewer]` acceptă `mouse-move`, `mouse-clicks`, `keys`, `other`, `tvterm-messages` (yes/no); Save Profile As
  le scrie pe toate cinci, cu un comentariu care explică literele. Cheile lipsă primesc valorile implicite; valorile greșite
  sunt raportate ca celelalte probleme din profil. Documentat în `examples/profiles/example.ini`.

### Fișierul de setări (`tvterm.ini`) — `far2l-extended`
- `~/.config/tvterm/tvterm.ini` (sau `$XDG_CONFIG_HOME/tvterm/`), secțiuni `[general]` (`profile`, `auto-reload`),
  `[scrollback]` (`lines`, `wheel-lines`, `word-chars`, `save-dir`, `search-case-sensitive`, `shift-click-select`),
  `[keys]` (`history-page-up/-page-down/-top/-bottom`, ex. `Ctrl-F5`, `none`). Documentat în `examples/tvterm.ini`.
- **Profil implicit**: `profile = NUME` se deschide când tvterm pornește fără `-p`; `-p ALTUL` are prioritate; `-p none` pornește fără el.
  Dacă lipsește sau are erori: se deschide un terminal normal, iar motivul apare în Event Viewer și în bara de stare.
- Reîncărcare automată (verificare la 2 s, după data modificării și mărime) și din More... > Reload settings;
  More... > Edit settings creează fișierul (toate cheile comentate) și îl deschide în editor. Totul în afară de `profile`
  se aplică imediat (un `lines` mai mic elimină cele mai vechi linii). Valorile invalide își păstrează valoarea anterioară;
  cheile șterse (sau fișierul șters) revin la valorile implicite.
- **Mesaje**: mesajele tvterm (setări, fișiere salvate, erori de profil) apar în Event Viewer ca `tvterm: HH:MM:SS ...`,
  chiar și în pauză; ultimele 200 se păstrează și se afișează la deschidere. Mesajele scurte apar 5 s în bara de stare.

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
  `[terminal]` (`title`, `dir`, `command`, `keep-open` yes/no, `pos`), `[editor]` (`file`, `pos`), `[eventviewer]` (`pos`, filtre: `mouse-move`, `mouse-clicks`, `keys`, `other`, `tvterm-messages`).
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
- **Event Viewer** (Ctrl-B V): afișează evenimentele Turbo Vision (diagnostic taste); a doua apăsare = pauză. Filtre: vezi mai sus.
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
  app.cc/.h                meniu, status line, comenzi, profiluri (open/save), main() cu -p/--help/--version
  about.cc/.h              textul versiunii și dialogul About; declarațiile informațiilor de build generate
  profile.cc/.h            parser/writer INI (WindowProfile, Profile)
  winlist.cc/.h            dialogul Window List, renameTerminal, getDeskTopWindows
  quitdlg.cc/.h            dialogul de confirmare a ieșirii (confirmQuit)
  procinfo.cc/.h           funcții /proc: readLink, readCommandLine, describeForeground (ce rulează într-un terminal)
  editwnd.cc/.h            editor (dialoguri Save As etc.)
  evntview.cc/.h           Event Viewer (filtre, tastele N/X; EventTerminal: TTerminal cu buffer mai mare)
  wnd.h                    TerminalWindow (+ launchCommand/launchDir/launchKeepOpen)
  settings.cc/.h           tvterm.ini: Settings, loadSettings/applySettings, nume de taste, șablon, FileStamp
  histdlg.cc/.h            dialogurile de căutare și salvare din modul istoric
  statline.cc/.h           bară de stare care poate afișa un mesaj câteva secunde
include/tvterm/termopts.h  TerminalOptions (global): mărime istoric, rotiță, caractere de cuvânt, taste, apeluri către aplicație
deps/vterm/patches/0001-win32-input-mode.patch   aplicat automat de CMake pe o copie (submodulul rămâne curat)
scripts/build.sh           build dinamic/static → bin/tvterm-dynamic, bin/tvterm-static (+ loguri)
scripts/tvterm-copy        copiere în clipboard prin OSC 52
scripts/buildinfo.cmake    rulat la fiecare build: scrie <dir. build>/buildinfo.cc (versiune, commit, branch, dată, tip)
examples/profiles/example.ini
```

**Fluxuri importante:**
- Fir de lucru → fir principal prin `TerminalState` (sub mutex), preluat la broadcast `cmCheckTerminalUpdates` (din `idle`).
- Fir principal → fir de lucru prin `TerminalEvent` (`termCtrl.sendEvent`); tipuri adăugate: `Far2lClipboardData`, `Paste`
  (date alocate cu `new char[]`, eliberate de receptor).
- far2l **așteaptă la nesfârșit** răspunsul la cererile cu ID ≠ 0 → tvterm răspunde întotdeauna (inclusiv timeout gol).
- Modul istoric e în `TerminalView` (firul principal). Istoricul aparține firului de lucru: textul se citește cu
  `TerminalController::withEmulator()` (blochează mutex-ul firului de lucru; **niciodată în interiorul `lockState()`**, firul
  de lucru le blochează în ordine inversă) și `TerminalEmulator::getText()` (linii logice, cu poziția fiecărei celule) /
  `getSelectionText()`. Derularea folosește evenimentul `ScrollBackScroll` (relativ; `scrollToTop`/`scrollToBottom`).
  `TerminalState::scrollbackDropped` numără liniile eliminate, ca vederea să-și poată ajusta coordonatele absolute.

---

## 4. Build și flux de lucru

- Build: `scripts/build.sh [dynamic|static|both]`; static = `cmake -DTVTERM_STATIC=ON`.
  Pachete Alpine: `build-base cmake perl patch git linux-headers ncurses-dev ncurses-static gpm-dev`.
- **Livrare:** Claude generează patch-uri cu `git format-patch` peste starea de pe GitHub;
  utilizatorul: `git am ../x.patch && scripts/build.sh static && git push`. Dacă un patch e deja aplicat: `git am --skip`.
- Autor commit-uri: `deep-soft <deep-soft@users.noreply.github.com>`.

### Capcane cunoscute
- `.gitignore` din upstream ignoră **tot** în afară de directoare permise explicit; fișierele fără extensie trebuie permise
  (ex. `!scripts/*`). Directoare permise adăugate: `scripts/`, `examples/`, `docs/`.
- Sursele sunt găsite cu `GLOB` → după adăugarea unui fișier `.cc` nou trebuie rulat din nou `cmake`.
- Unity build: funcțiile `static` din fișiere diferite pot intra în conflict → namespace-uri proprii (`far2lext`, `far2lclip`).
- `TFileDialog`: câmpul `directory` pornește gol → numele tastate sunt relative la directorul curent; tvterm îl setează la directorul profilurilor.
- Cod win32-input protejat cu `HAVE_VTERM_WIN32INPUT` (doar cu libvterm-ul inclus, patch-uit).
- `TDialog` se închide singur doar la comenzile standard (cmOK, cmCancel, cmYes, cmNo): butoanele proprii trebuie să le folosească.
- `TGroup::current` ascunde variabilele globale numite `current` în metodele lui `TVTermApp`.
- `TTerminal` își limitează fără avertisment bufferul la 32000 de octeți (vezi Filtre în Event Viewer).
- Event Viewer-ul înregistrează evenimentele în `TVTermApp::getEvent`, înainte să fie tratate: tastele proprii sunt sărite
  acolo (doar cât timp are focus), altfel fiecare comutare ar apărea în listă.
- Informațiile de build: generate de o țintă CMake rulată la fiecare build; fișierul se rescrie doar când conținutul se schimbă
  (`configure_file(COPYONLY)`) și e scos din unity build și din headerele precompilate, deci la recompilare se compilează
  doar acel fișier mic. Directoarele noi de pe primul nivel sunt ignorate de `.gitignore`, de aceea scriptul stă în `scripts/`.

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
- Teste istoric (feat 20): Alt-Shift-PgUp/PgDn `ESC[5;4~`/`ESC[6;4~`, Alt-Shift-Home/End `ESC[1;4H`/`ESC[1;4F`, F2 `ESC OQ`,
  F3 `ESC OR`, Shift-F3 `ESC[1;2R`, F7 `ESC[18~`; mouse în format SGR `ESC[<b;x;yM/m` (dublu/triplu click = apăsare/eliberare
  repetată în 0,1 s; tragere = butonul 32). Potrivirea apare în pyte cu fundal `brightbrown`.
  - tvision ignoră bitul Shift din rapoartele de mouse xterm, deci Shift+click nu se poate trimite din test: s-a testat cu un
    build temporar care acceptă Alt în loc (necomis). În lanțul real ajunge prin extensiile far2l.
  - `/bin/sh` (dash) nu are editare de linie: tastele trimise greșit la shell rămân în linia de comandă → întâi Ctrl+C.
  - Măsurat: căutare în 10000 de linii de 85 de caractere: 0,1 s găsit aproape, 0,9 s negăsit; RSS ≈ 75 MB cu istoricul plin.
- Teste About (feat 21): `--version`/`-V`; Ctrl-B `m` `b` deschide dialogul; conținutul fără setări/profil și cu un profil
  de 3 terminale (unul deconectat); uptime după 1 minut; cale lungă scurtată pe un ecran de 60 de coloane; Enter/Esc îl închid;
  primele note din Event Viewer; `TZ=Europe/Bucharest` → `EET`; `-dirty`; build fără git → `unknown`;
  recompilare în același minut nu compilează nimic, în minutul următor doar `buildinfo.cc`.
- Teste Event Viewer (feat 22): titlul și bara de stare implicite; X; mișcările de mouse ascunse, apoi afișate după M; tastele
  proprii neînregistrate; K, C, T (note noi de la o reîncărcare automată a setărilor); N (și cu T oprit); ~120 de evenimente
  de taste (> 32000 de octeți) păstrează primele note, ~300 le elimină pe cele mai vechi; titlul la pauză; cheile din profil,
  salvarea (comentariu + cinci chei) și reîncărcarea; valoare greșită și chei de filtru în [terminal] raportate; literele tastate
  într-un terminal nu ajung la Event Viewer. Testul trebuie să dea click în Event Viewer: un click în altă parte mută focusul
  (iar un terminal maximizat îl acoperă).

---

## 6. Limitări cunoscute

- Marcajul de bloc vertical nu trece prin Windows (doar între far2l-uri din același tvterm).
- `ClientOptions` și info procese: neimplementate pe Windows.
- Win32-input-mode: scan code 0; key-up trimis imediat după key-down; unele taste (simboluri cu Shift, numpad) au `vk = 0`.
- Starea „pauză” a Event Viewer-ului nu se salvează în profil. Un Event Viewer deschis cu Ctrl-B V pornește cu filtrele
  implicite (ultimele nu sunt reținute; profilurile le păstrează).
- Expresiile regulate fără diferență între litere mari/mici tratează doar literele ASCII (căutarea de text simplu tratează și
  literele latine, românești, grecești, chirilice).
- Căutarea citește tot istoricul la fiecare pas (merge până la limita de 100000 de linii, dar nu instantaneu).

---

## 7. Jurnal pe etape

| Etapă | Commit-uri | Ce s-a făcut |
|---|---|---|
| Curățenie inițială | 1–5 | branch nou din upstream; win32-input-mode cu patch libvterm aplicat automat; build static din CMake; editor; Event Viewer; scripts/build.sh |
| Clipboard | 6–11 | OSC 52; Shift-Ins; jurnal depanare; extensii far2l (clipboard); bracketed paste; blocuri verticale far2l |
| Profiluri | 12–16 | ClientOptions + titlu fix; profiluri INI; info procese; Window List, Rename, Save Profile; editor și Event Viewer în profiluri |
| Organizare | 17–18 | docs/HISTORY.md, apoi împărțit în HISTORY.md (engleză) și HISTORY-ro.md (română); STATUS.md ținut în Project-ul Claude |
| Info la ieșire | 19 | confirmarea de ieșire listează ce rulează în fiecare terminal, cu Go to |
| Istoric (`far2l-extended`) | 20 | modul istoric (derulare, căutare, salvare), dublu/triplu click, Shift+click, tvterm.ini cu profil implicit și reîncărcare automată |
| Versiune (`far2l-extended`) | 21 | versiunea 0.1.0 cu commit/branch/dată generate la fiecare build; `--version`; dialogul About; primele note în Event Viewer |
| Filtre Event Viewer (`far2l-extended`) | 22 | filtrele M C K O T, N (mesajele din nou), X (golire), buffer de 64000 de octeți, filtre în profiluri |
