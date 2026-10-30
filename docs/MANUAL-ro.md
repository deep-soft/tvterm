# Manualul tvterm (fork-ul deep-soft)

Cum se folosește tvterm în forma de acum, pe branch-ul `far2l-extended` (`far2l-integration` are doar o parte:
terminale, clipboard, editor, Event Viewer fără filtre, profiluri, Window List, Tile; nu și istoricul,
`tvterm.ini`, alertele, Log View sau `--version`).
Ce s-a schimbat și de ce: în
[HISTORY-ro.md](HISTORY-ro.md). Versiunea în engleză: [MANUAL.md](MANUAL.md); cele două se actualizează împreună.

tvterm este un emulator de terminal cu ferestre, construit pe Turbo Vision. Rulează în alt terminal
(de ex. far2l prin SSH, PuTTY, xterm) și arată pe un singur ecran multe terminale, editoare și ferestre cu loguri filtrate.

Numele din interfață (meniuri, dialoguri, mesaje) sunt în engleză, ca în program.

Cuprins:
1. [Compilare și pornire](#1-compilare-și-pornire)
2. [Ecranul și meniul Ctrl-B](#2-ecranul-și-meniul-ctrl-b)
3. [Terminale și clipboard](#3-terminale-și-clipboard)
4. [Istoricul (scrollback)](#4-istoricul-scrollback)
5. [Editorul](#5-editorul)
6. [Fișierul de setări: tvterm.ini](#6-fișierul-de-setări-tvtermini)
7. [Profiluri](#7-profiluri)
8. [Event Viewer](#8-event-viewer)
9. [Alerte](#9-alerte)
10. [Log View](#10-log-view)
11. [Exemple complete](#11-exemple-complete)
12. [Probleme cunoscute](#12-probleme-cunoscute)

---

## 1. Compilare și pornire

### Compilare

Pachete:

- Alpine: `apk add build-base cmake perl patch git linux-headers ncurses-dev ncurses-static gpm-dev`
  (`gpm-dev` e opțional).
- Debian / Ubuntu / Raspberry Pi OS: `apt install build-essential cmake perl patch git libncurses-dev libgpm-dev`.

```sh
git clone --recursive -b far2l-extended https://github.com/deep-soft/tvterm.git
cd tvterm
scripts/build.sh static      # sau: dynamic, both (implicit)
```

Rezultatul este `bin/tvterm-static` (sau `bin/tvterm-dynamic`), cu jurnalul compilării alături. Nu se instalează nimic:
executabilul se copiază unde trebuie, de ex. `cp bin/tvterm-static ~/bin/tvterm`. Build-ul static nu depinde de
biblioteci și se poate copia pe alte servere (pentru sisteme vechi, vezi [Probleme cunoscute](#12-probleme-cunoscute)).

### Linia de comandă

```
Usage: tvterm [-p PROFILE]

  -p, --profile PROFILE   open the windows described in PROFILE
                          ('none': do not open the default profile)
  -V, --version           show the version and build information
  -h, --help              show this help
```

`PROFILE` este o cale (dacă are `/`) sau un nume, căutat ca `~/.config/tvterm/profiles/NUME.ini`
(`.ini` se adaugă dacă numele nu are punct). Merge și `--profile=NUME`.

Fără `-p`, tvterm deschide profilul implicit (`profile` din `tvterm.ini`), dacă există; altfel un terminal.
Dacă profilul nu se poate deschide, pornește un terminal obișnuit și motivul este afișat.

### Fișiere

| Fișier | Rol |
|---|---|
| `~/.config/tvterm/tvterm.ini` | setări ([capitolul 6](#6-fișierul-de-setări-tvtermini)) |
| `~/.config/tvterm/profiles/*.ini` | profiluri ([capitolul 7](#7-profiluri)) |

Dacă `XDG_CONFIG_HOME` e setat, se folosește `$XDG_CONFIG_HOME/tvterm/` în loc de `~/.config/tvterm/`.
`examples/tvterm.ini` și `examples/profiles/example.ini` din surse descriu ambele formate.

### Versiunea

`tvterm --version` afișează versiunea (`0.1.0`), branch-ul și commit-ul, data, tipul (`static` / `dynamic`), arhitectura
și biblioteca C ale compilării și commit-urile tvision și libvterm incluse. De exemplu:

```
tvterm 0.1.0 (deep-soft)
commit:   1a2b3c4 (far2l-extended)
built:    2026-10-30 17:20 EET, static, armv7l, glibc 2.28
tvision:  6402631
libvterm: 62b27d1
```

Arhitectura are denumirile din `uname -m` (`x86_64`, `armv7l`, `aarch64`), iar biblioteca C e `glibc X.Y` sau `musl`
(Alpine). Amândouă descriu executabilul așa cum a fost compilat, nu mașina pe care rulează: util ca să verifici că pe un
server e copia potrivită. Aceleași informații sunt în **Ctrl-B More... > About tvterm...**,
împreună cu fișierul de setări, ultimul profil încărcat, timpul de funcționare și numărul de terminale.

---

## 2. Ecranul și meniul Ctrl-B

Ecranul are desktopul (ferestrele) și, jos, **bara de stare**. Bara se schimbă după fereastra activă și arată tastele
ei principale; un click pe un element are același efect ca tasta lui. `WinList` deschide Window List.
Când există ferestre cu alerte, în capătul din dreapta apare `Alerts: N (Ctrl-B G)`. Mesajele scurte (de ex.
„Settings reloaded.”) înlocuiesc bara de stare timp de 5 secunde.

Ferestrele se mută cu mouse-ul trăgând de titlu, se redimensionează din colțul din dreapta jos și se închid cu
butonul `[■]`, ca în orice program Turbo Vision.

### Meniul Ctrl-B

**Ctrl-B** deschide meniul în mijlocul ecranului; apoi se apasă litera (sau click). Ctrl-B nu ajunge la programe
(vezi Grab Input mai jos).

| Tasta | Element | Ce face |
|---|---|---|
| `N` | New Term | terminal nou cu shell-ul tău (`$SHELL`) |
| (fără) | Close Term | închide fereastra activă: se alege cu săgețile + Enter sau cu un click (fără tastă, ca să nu fie aleasă din greșeală) |
| `Tab` / `Shift-Tab` | Next / Previous Term | fereastra următoare / precedentă |
| `W` | Window List... | lista tuturor ferestrelor: Go, Rename, Close |
| `T` | Rename Term... | titlu fix pentru un terminal sau un Log View (gol: cel automat) |
| `K` | Alerts... | alertele terminalului sau Log View-ului activ ([capitolul 9](#9-alerte)) |
| `G` | Go to Next Alert | merge la fereastra cu cea mai veche alertă |
| `R` | Resize/Move | săgețile mută, Shift+săgeți redimensionează, Ctrl mai repede, Enter gata, Esc anulează |
| `F` | Maximize/Restore | |
| `S` | Select Text | modul de selecție într-un terminal ([capitolul 3](#3-terminale-și-clipboard)) |
| `H` | History | modul istoric într-un terminal ([capitolul 4](#4-istoricul-scrollback)) |
| `P` | Paste | lipește clipboard-ul în terminal |
| `E` | New Editor | editor gol |
| `O` | Open File in Editor... | |
| `V` | Event Viewer | îl deschide; dacă e deja deschis, îl oprește / pornește |
| `D` | Log View... | Log View nou ([capitolul 10](#10-log-view)) |
| `L` | Load Profile... | |
| `A` | Save Profile As... | salvează ferestrele deschise ca profil |
| `M` | More... | submeniu, vezi mai jos |
| `U` | Suspend | oprește tvterm și revine în shell-ul exterior; `fg` îl readuce |
| `Q` | Exit | întreabă întâi dacă rulează terminale sau Log View-uri |

**More...**: Change working dir · Tile (Columns First) · Tile (Rows First) · Cascade · Grab Input ·
Edit settings (tvterm.ini) · Reload settings · About tvterm.

### Tile și Cascade

Tile și Cascade aranjează terminalele și Log View-urile. **Event Viewer-ul și editoarele rămân pe loc**; celelalte
ferestre folosesc cel mai mare dreptunghi care nu se suprapune peste niciunul dintre ele. Dacă dreptunghiul are sub
20 de coloane × 6 rânduri (sau nu există fereastră fixă), se folosește tot desktopul, iar Event Viewer-ul primește nota
„no free area beside the fixed windows, using the whole desktop”.

### Window List

**Ctrl-B W** sau click pe `WinList`. Fiecare rând arată tipul (`Term`, `Log`, `Editor`, `Events`) și titlul,
cu marcajul de alertă, dacă există. Butoane: **Go** (Enter), **Rename**, **Close**, Cancel.

### Grab Input

**More... > Grab Input** trimite toate tastele programului din terminalul activ, inclusiv Ctrl-B (titlul arată
`(Input Grab)`). Se termină cu **Alt-End** sau cu un click în afara ferestrei.

### Ieșirea

**Ctrl-B Q** arată terminalele și Log View-urile deschise, cu ce rulează în fiecare (de ex. `tail -F /var/log/maillog`,
`bash, idle`), cele ocupate primele. Butoane: **Quit**, **Go to** (merge la fereastra aleasă, fără ieșire), Cancel.
Editoarele cu modificări nesalvate întreabă după aceea. Programele primesc SIGHUP; far2l rămâne în fundal și poate fi
reatașat.

### Dialogurile de fișiere

În câmpul pentru nume din dialogurile de fișiere (Open File in Editor, Save file as din editor, Load Profile, Save Profile
As și câmpul File din dialogul Save al istoricului și al Log View), la tastarea unei căi apare singură, sub câmp, o listă de
directoare: subdirectoarele părții dinaintea ultimului `/` al căror nume începe cu ce urmează după el.
De exemplu `/var/` + `l` → `lib/`, `local/`, `lock/`, `log/`.

- Doar directoare; cele ascunse doar după ce tastezi un `.`. Literele mari și mici diferă (`L` nu prinde `lib`).
- Un nume fără `/` e luat din directorul afișat în dialog; `~/` e directorul tău home.
- **↓** intră în listă; ↑ / ↓ / PgUp / PgDn / Home / End se mișcă în ea. **Enter** (sau dublu-click) adaugă directorul și
  un `/` în câmp și arată subdirectoarele lui, ca să continui cu tastarea sau alegerea.
  **↑** pe primul rând revine în câmp; orice altă tastă revine și ea în câmp și e tastată acolo.
- **Esc** închide lista (al doilea Esc închide dialogul); **Tab** o închide și trece la câmpul următor.
- Cu lista închisă tastele merg ca înainte: ↓ deschide istoricul câmpului, Enter deschide fișierul sau intră în director.
  Lista de fișiere din dialog nu se schimbă cât tastezi.
- Textul lipit (Shift-Ins, Ctrl-V) deschide și el lista. Se văd cel mult 10 rânduri, cu bară de derulare pentru restul.

---

## 3. Terminale și clipboard

Un terminal nou rulează `$SHELL`. Titlul lui îl stabilește programul care rulează în el, dacă nu i s-a dat un titlu fix
(Ctrl-B T sau `title` în profil). Tastele ajung la program, cu excepția lui Ctrl-B. far2l pornit în tvterm folosește
modul lui extins de tastatură (win32-input-mode) și extensiile lui de terminal: titlul lui arată `TTY|F`.

Când programul unui terminal se termină, titlul arată `(Disconnected)` și ieșirea rămâne vizibilă; orice tastă îl închide.

### Selectarea textului

- **Tragere cu mouse-ul** într-un terminal al cărui program nu folosește mouse-ul (un shell): selectează text.
  **Dublu click** selectează un cuvânt (litere, cifre și `word-chars` din `tvterm.ini`), **triplu click** toată linia;
  tragerea de după extinde selecția pe cuvinte sau linii.
- **Shift+click** selectează și în programele care folosesc mouse-ul (far2l, mc), dacă `shift-click-select = yes`
  (implicit). Atenție: putty4far2l folosește Shift+mouse pentru selecția proprie; tvterm îl primește doar dacă aceasta e
  dezactivată acolo.
- **Ctrl-B S** pornește modul de selecție, pentru programele care folosesc mouse-ul.
- Cu o selecție: **Ctrl-C** o copiază, **Esc** o anulează.

### Copiere și lipire

| Unde | Copiere | Lipire |
|---|---|---|
| terminal | selecție + Ctrl-C | **Shift-Ins**, Ctrl-B P sau lipirea terminalului exterior (de ex. click dreapta în PuTTY) |
| far2l în tvterm | tastele far2l (Ctrl-C, Ctrl-Ins) | tastele far2l (Ctrl-V, Shift-Ins) |
| editor | Ctrl-Ins (Shift-Del taie) | Shift-Ins |
| câmpurile de text din dialoguri | Ctrl-Ins sau Ctrl-C (textul selectat) | Shift-Ins sau Ctrl-V |
| modul istoric, Log View | Ctrl-C sau Ctrl-Ins | — |

Clipboard-ul este cel al sistemului. Prin far2l și putty4far2l este **clipboard-ul Windows**
(în far2l-ul exterior se permite cu „Share clipboard always”).

- Shift-Ins într-un terminal lipește prin tvterm doar când programul nu folosește extensiile far2l (altfel programul,
  de ex. far2l, lipește singur) și nu în modul Grab Input.
- Textul lipit se trimite dintr-odată; programele care cer asta (bash, vim) îl primesc ca lipire (bracketed paste), deci
  o comandă lipită nu se execută înainte de Enter. Caracterele de control se elimină.
- Lipit într-un câmp de dialog, textul devine o singură linie: sfârșiturile de linie și tab-urile devin spații,
  caracterele de control și cele invizibile se elimină, textul se taie la lungimea câmpului.
- **OSC 52**: programele dintr-un terminal pot scrie în clipboard (vim, tmux). `scripts/tvterm-copy` copiază din linia de
  comandă: `comanda | tvterm-copy`, `tvterm-copy < fisier`, `tvterm-copy "text"`. Citirea clipboard-ului prin OSC 52 e refuzată.
- Blocurile verticale far2l rămân verticale la copiere și lipire între ferestre far2l din același tvterm.
- Probleme cu clipboard-ul: `TVTERM_CLIPBOARD_LOG=/tmp/tvterm-clip.log tvterm` scrie un jurnal.

---

## 4. Istoricul (scrollback)

Fiecare terminal păstrează liniile care ies de pe ecran (`lines` în `tvterm.ini`, implicit 10000). Programele pe tot
ecranul (far2l, mc, top, editoarele) folosesc un ecran separat, fără istoric.

### Modul istoric

Se intră cu **Ctrl-B H** sau direct cu **Alt-Shift-PgUp / PgDn / Home / End** (configurabile în `[keys]`).
Titlul arată `(History)`. Între timp ieșirea continuă să vină.

| Tasta | Acțiune |
|---|---|
| Up / Down, PgUp / PgDn, rotița mouse-ului | derulare |
| Home / End (și Ctrl-Home / Ctrl-End) | început / sfârșit |
| `/` sau F7 | căutare |
| F3 sau `n` / Shift-F3 sau `N` | potrivirea următoare spre liniile mai vechi / mai noi |
| F2 sau `s` | salvare |
| Ctrl-C sau Ctrl-Ins | copiază selecția, altfel potrivirea |
| Esc | șterge selecția, apoi potrivirea, apoi iese din mod |
| orice altă tastă | iese din mod și ajunge la program |

Intrat cu o tastă directă (Alt-Shift-...), modul se termină și când se derulează înapoi până jos. Într-un program pe
tot ecranul, tastele directe ajung la program, iar Ctrl-B H deschide dialogul de salvare pentru ecranul vizibil.

### Căutarea

Text simplu sau expresie regulată (ca `grep -E`). Textul simplu ignoră implicit majusculele (`search-case-sensitive`),
inclusiv la diacritice (`știe` găsește `ȘTIE`). Căutarea pornește de la liniile vizibile spre cele mai vechi; potrivirea
apare negru pe galben. Negăsit: un mesaj în bara de stare.

### Salvarea

Dialogul propune `tvterm-<titlu>-YYYY-MM-DD-hhmm.txt` în `save-dir`, altfel în directorul curent al programului din
terminal; un nume relativ e relativ la acel director. Numele de directoare se pot completa în câmpul File
([Dialogurile de fișiere](#dialogurile-de-fișiere)).

- **File name**: `Date first` (`YYYY-MM-DD-hhmm-tvterm-<titlu>.txt`, ordonat după dată în listele de fișiere) sau
  `Name first`. Comutarea rescrie numele propus (nu și unul modificat de mână). Alegerea de pornire este `save-name`
  din `tvterm.ini`; ultima alegere se păstrează până la ieșirea din tvterm.
- **What to save**: tot (istoric + ecran) sau doar selecția.
- Text simplu UTF-8, fără culori; spațiile de la sfârșitul liniilor eliminate, liniile rupte reunite.
- Fișier existent: **Overwrite**, **Append** sau Cancel. Rezultatul apare în bara de stare și în Event Viewer.

---

## 5. Editorul

**Ctrl-B E** deschide un editor gol, **Ctrl-B O** un fișier. Editoarele se deschid în partea dreaptă (80 de coloane)
când ecranul are peste 100 de coloane. **F2** salvează (un editor fără nume cere un nume). Clipboard: Ctrl-Ins copiere,
Shift-Del tăiere, Shift-Ins lipire. Închiderea unui editor cu modificări nesalvate întreabă întâi. Tile și Cascade lasă
editoarele pe loc.

**More... > Edit settings** deschide `tvterm.ini` într-un editor.

---

## 6. Fișierul de setări: tvterm.ini

`~/.config/tvterm/tvterm.ini`. tvterm nu îl creează și nu îl rescrie niciodată singur: **More... > Edit settings** îl
creează, dacă lipsește, cu toate cheile comentate, și îl deschide. Cheile apărute în versiuni noi se adaugă de mână
(vezi `examples/tvterm.ini`).

- `#` sau `;` la începutul liniei: comentariu. Numele secțiunilor și ale cheilor nu țin cont de majuscule.
- Modificările se aplică la salvarea fișierului (verificat la 2 secunde) sau cu **More... > Reload settings**.
- O valoare greșită păstrează valoarea precedentă; o cheie (sau fișierul) ștearsă revine la valoarea implicită.
  Problemele apar în Event Viewer, cu un rezumat în bara de stare.

| Secțiune | Cheie | Implicit | Valori / sens |
|---|---|---|---|
| `[general]` | `profile` | (niciunul) | profilul deschis la pornire fără `-p` (nume sau cale); citit doar la pornire |
| | `auto-reload` | `yes` | reîncarcă fișierul când se schimbă |
| `[scrollback]` | `lines` | `10000` | linii de istoric pe terminal, 0–100000 (cam 4 KB pe linie de 100 de coloane) |
| | `wheel-lines` | `3` | linii la un pas de rotiță, 1–100 (și în Log View) |
| | `word-chars` | `-._/@:~+%=` | caractere care fac parte dintr-un cuvânt la dublu click, pe lângă litere și cifre |
| | `save-dir` | (gol) | directorul propus la salvare; gol: directorul curent al terminalului (la Log View: al lui tvterm) |
| | `save-name` | `name-first` | `name-first` sau `date-first` ([capitolul 4](#salvarea)) |
| | `search-case-sensitive` | `no` | valoarea de pornire pentru „Case sensitive” în dialogul de căutare |
| | `shift-click-select` | `yes` | Shift+click selectează în programele care folosesc mouse-ul |
| `[logview]` | `max-lines` | `50000` | linii păstrate de fiecare sursă Log View, 100–1000000 |
| `[highlight <nume>]` | reguli | seturile incluse | culorile din Log View ([capitolul 10](#culori-colorare)) |
| `[keys]` | `history-page-up` | `Alt-Shift-PgUp` | tastele directe pentru istoric |
| | `history-page-down` | `Alt-Shift-PgDn` | |
| | `history-top` | `Alt-Shift-Home` | |
| | `history-bottom` | `Alt-Shift-End` | |

Valorile da/nu acceptă și `true`/`false`, `on`/`off`, `1`/`0`. Numele tastelor: modificatorii `Ctrl-`, `Alt-`, `Shift-`
(sau cu `+`) și o tastă: `PgUp`, `PgDn`, `Home`, `End`, `Up`, `Down`, `Left`, `Right`, `Ins`, `Del`, `Tab`, `Enter`,
`Esc`, `Backspace`, `Space`, `F1`…`F12` sau un caracter; `none` dezactivează tasta.

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

## 7. Profiluri

Un profil deschide un set de ferestre cu comenzile, titlurile, alertele și pozițiile lor. Se deschide cu
`tvterm -p NUME`, cu **Ctrl-B L** sau ca profil implicit. **Ctrl-B A** salvează ferestrele deschise ca profil.

**Ctrl-B L** cu ferestre deschise întreabă „Close the current windows first?”: **Yes** arată terminalele și Log View-urile
care rulează (Close / Go to / Cancel), editoarele cu modificări nesalvate cer salvarea, apoi totul se închide și se
deschide profilul; **No** adaugă ferestrele profilului la cele deschise; **Cancel** nu încarcă nimic.

### Formatul

`#` sau `;` încep un comentariu. O secțiune `[profile]`, apoi câte o secțiune pentru fiecare fereastră, în ordinea
suprapunerii (ultima este deasupra).

`[profile]`:

| Cheie | Implicit | Sens |
|---|---|---|
| `layout` | `auto` | `auto` (`manual` dacă vreo fereastră are `pos`, altfel `tile-columns`), `manual`, `tile-columns`, `tile-rows`, `cascade` |
| `focus` | ultima fereastră | fereastra activă la pornire, 1 = prima secțiune de fereastră |

Toate secțiunile de fereastră acceptă `pos = x y lățime înălțime`, în **procente** din desktop (se acceptă zecimale).
O poziție ieșită parțial din desktop este mutată înăuntru (și micșorată dacă e mai mare), cu o notă în Event Viewer.

`[terminal]`:

| Cheie | Implicit | Sens |
|---|---|---|
| `title` | (automat) | titlu fix |
| `dir` | directorul curent | directorul de lucru (`~` = directorul personal) |
| `command` | shell interactiv | comanda, rulată ca `$SHELL -c "comanda"` |
| `keep-open` | `yes` | `yes`: când comanda se termină (sau Ctrl-C), pornește un shell în același terminal; `no`: terminalul rămâne `(Disconnected)` cu ieșirea lui |
| `alert-activity`, `alert-keywords`, `alert-silence` | oprite | alerte ([capitolul 9](#9-alerte)) |

`[logview]` ([capitolul 10](#10-log-view)):

| Cheie | Implicit | Sens |
|---|---|---|
| `title` | comanda | titlul ferestrei |
| `command` | (obligatoriu) | comanda a cărei ieșire se afișează |
| `include` | (niciunul) | expresie regulată: se afișează doar liniile care se potrivesc |
| `exclude` | (niciunul) | expresie regulată: liniile care se potrivesc sunt ascunse |
| `ignore-case` | `no` | pentru include / exclude |
| `wrap` | `no` | `yes`: liniile lungi rupte; `no`: derulate cu Left / Right |
| `filters` | `yes` | `no`: include / exclude nefolosite (ca după tasta F) |
| `max-lines` | `0` | linii păstrate; 0: `[logview] max-lines` din `tvterm.ini` |
| `auto-restart` | `0` | repornește comanda la N secunde după ce se termină (0: niciodată; maximum 86400) |
| `highlight` | (niciunul) | seturi de colorare, ex. `zimbra-mta kv` ([capitolul 10](#culori-colorare)) |
| `highlight-on` | `yes` | `no`: culorile oprite (ca după tasta H) |
| `alert-activity`, `alert-keywords`, `alert-silence` | oprite | alerte, pe liniile afișate |

`include` / `exclude` între ghilimele duble păstrează spațiile de la început sau sfârșit: `include = "dstport=(993|465) "`.

`[editor]`: `file` (fără ea, un editor gol). `[eventviewer]`: filtrele `mouse-move` (implicit `no`),
`mouse-clicks`, `keys`, `other`, `tvterm-messages` (implicit `yes`) ([capitolul 8](#8-event-viewer)).

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

### Salvarea (Ctrl-B A)

Salvează fiecare fereastră cu poziția ei (în interiorul desktopului, cu o zecimală), ordinea suprapunerii și fereastra
activă. Pentru un terminal: titlul fix, directorul curent al programului din el, comanda din profil sau, altfel, comanda
care rulează în prim-plan (de ex. `tail -f ...`, salvată cu `keep-open = yes`), alertele. Pentru un Log View: toate
setările lui. Textul editoarelor fără nume nu se salvează, iar editoarele cu modificări nesalvate păstrează doar numele
fișierului; tvterm spune asta după salvare. Dialogul propune directorul profilurilor; `.ini` se adaugă dacă lipsește.

---

## 8. Event Viewer

**Ctrl-B V** deschide Event Viewer-ul (unul singur); apăsat din nou, îl oprește / pornește (`(Stopped)` în titlu).
Se deschide în partea dreaptă (70 de coloane) când ecranul are peste 100 de coloane.

Arată evenimentele Turbo Vision (taste, mouse, comenzi; util ca să vezi ce trimite o tastă) și **mesajele tvterm**:
probleme în setări, fișiere salvate, note despre profiluri, surse Log View terminate. Mesajele arată ca
`tvterm: hh:mm:ss text` sau `tvterm: YYYY-MM-DD hh:mm:ss text` pentru un mesaj din altă zi (data se adaugă la afișare,
deci după miezul nopții mesajele mai vechi o primesc); ultimele 200 se păstrează și cât timp nu e deschis niciun Event Viewer, iar primele două linii
dau versiunea și compilarea.

Taste, cât e fereastra activă (fiecare e și element în bara de stare):

| Tasta | Acțiune |
|---|---|
| `M` | mișcările mouse-ului pornit/oprit (implicit oprit) |
| `C` | click-urile și rotița mouse-ului pornit/oprit |
| `K` | tastele pornit/oprit |
| `O` | alte evenimente (comenzi) pornit/oprit |
| `T` | mesajele tvterm pornit/oprit |
| `N` | golește fereastra și arată din nou mesajele păstrate |
| `A` | listează alertele active ([capitolul 9](#9-alerte)) |
| `X` | golește fereastra |

Titlul arată filtrele și tasta A: `Event Viewer [M- C+ K+ O+ T+] A: alerts`. Se aplică doar evenimentelor noi; evenimentele ascunse sunt totuși
numărate (`Received event #N` sare numere). Profilurile păstrează filtrele (`[eventviewer]`).

---

## 9. Alerte

Alertele anunță că s-a întâmplat ceva într-un terminal sau Log View la care **nu** te uiți. Se setează cu **Ctrl-B K**
în fereastră sau în profil.

| Tip | Dialog | Cheie în profil | Marcaj în titlu |
|---|---|---|---|
| activitate: orice text nou | Any new text | `alert-activity = yes` | `[!]` |
| cuvânt cheie găsit într-o linie nouă | Keywords | `alert-keywords = error, reject` | `[!error]` |
| liniște: fără ieșire N secunde | Silence | `alert-silence = 60` | `[!silent]` |

- Cuvintele cheie sunt text simplu (nu expresii regulate), căutate în fiecare linie nouă; fără diferență de majuscule doar
  pentru literele ASCII; spațiile de la capetele fiecărui termen se elimină. Maximum 1023 de caractere în dialog.

  | Scris | Înseamnă |
  |---|---|
  | `error, reject` | **virgula** = grupuri alternative: o linie cu oricare dintre ele |
  | `192.168.88.4 + :993` | **`+`** = toți termenii grupului trebuie să fie în aceeași linie (în orice ordine) |
  | `!someuser@someserver` | **`!`** = excludere: o linie care îl conține nu dă alertă de cuvânt cheie, oricare grup s-ar potrivi; doar ca element separat, între virgule |
  | `!debug, !info` | doar excluderi: orice altă linie nouă dă alertă (marcaj `[!*]`) |
  | `\!`, `\+`, `\,`, `\\`, `\ ` | `\` face caracterul următor text simplu (`\ ` păstrează un spațiu la capătul unui termen) |

  Exemplu: `192.168.88.4 + :993, !someuser@someserver`. O excludere într-un grup cu `+` (`a + !b`, `!a + b`) sau un termen
  gol (`c++`: se scrie `c\+\+`) e refuzată de dialog cu un mesaj (dialogul rămâne deschis); într-un profil e raportată
  ca problemă de profil, iar celelalte cuvinte cheie se folosesc.
- Marcajul arată grupul găsit, fără spații în jurul lui `+` și scurtat la 24 de caractere (`[!192.168.88.4+:993]`); un
  grup cu `[` sau `]` e pus între ghilimele (`] Ban` → `[!"] Ban"]`).
- Într-un terminal, o linie încă nescrisă complet (de ex. un prompt fără Enter) e verificată după 0,5 s fără ieșire nouă,
  deci o excludere care sosește o clipă mai târziu pe aceeași linie contează.
- Liniștea se numără de la pornirea ferestrei (sau de la setarea alertei), apoi de la ultima ieșire și de la părăsirea
  ferestrei. După o alertă de liniște, ieșirea nouă o rearmează.
- Prioritate: cuvânt cheie > liniște > activitate. Un cuvânt găsit în titlul ferestrei sau într-o secvență escape nu contează.
- Marcajul apare și în Window List și în dialogul de ieșire; bara de stare arată `Alerts: N (Ctrl-B G)`.
  **Ctrl-B G** merge la cea mai veche alertă. Selectarea ferestrei îi șterge alerta.
- **A** în Event Viewer listează alertele active (cele numărate de `Alerts: N`), de la cea mai veche (ordinea lui Ctrl-B G):
  ora la care alerta a primit tipul actual (o alertă de activitate devenită de cuvânt cheie arată ora cuvântului cheie),
  marcajul și titlul ferestrei; la cuvinte cheie și grupul și linia (în jurul primului termen, cam 150 de octeți):

  ```
  --- Active alerts (2) ---
  14:02:11 [!] Shell
  14:05:40 [!192.168.88.4+:993] Log: FortiGate
      group: 192.168.88.4 + :993
      line:  date=2026-10-29 time=14:05:40 ... srcip=192.168.88.4 ... dstport=993 ...
  ```

  Fără alerte: `--- No active alerts ---`. Orele au și data (`YYYY-MM-DD hh:mm:ss`) când nu sunt de azi.
- Fără alarme false din redesenare: ieșirea se ignoră 3 s după pornirea unui terminal și 1,5 s după redimensionare sau
  după părăsirea terminalului. La Log View, primele 3 s după pornirea sursei (de ex. liniile din `tail -n 5000`).
- Alertele de activitate sunt zgomotoase cu programele pe tot ecranul (ceasul din far2l): acolo folosește cuvinte cheie.

---

## 10. Log View

Un Log View este o fereastră care arată liniile scrise de o comandă, filtrate. Nu este un terminal: nu se scrie în el,
dar are istoricul lui (până la `max-lines`), căutare, selecție și salvare, și nu își pierde poziția.

### Deschiderea

**Ctrl-B D** deschide dialogul:

| Câmp | Sens |
|---|---|
| Title | gol: comanda |
| Command | de ex. `tail -n 5000 -F /var/log/maillog` (↓ arată comenzile anterioare) |
| Include | expresie regulată: se afișează doar liniile care se potrivesc (gol: toate; ↓ arată valorile folosite) |
| Exclude | expresie regulată: liniile care se potrivesc sunt ascunse (gol: niciuna; ↓ arată valorile folosite) |
| Ignore case, Wrap long lines, Filters on | căsuțe de bifat |
| Max lines | 0: `[logview] max-lines` din `tvterm.ini` |
| Restart after N s | 0: fără repornire automată |
| Highlight sets | culori, ex. `zimbra-mta kv` ([mai jos](#culori-colorare)); gol: fără; ↓ arată seturile și valorile folosite |

Expresiile regulate sunt POSIX extinse, ca la `grep -E`: alternative cu `|` (`reject:|bounced`), grupuri cu `( )`.
Spațiile de la sfârșitul câmpurilor se păstrează (`dstport=993 ` nu prinde `dstport=9930`). Erorile apar în dialog.

**Listele câmpurilor:** săgeata de lângă Command, Include, Exclude și Highlight sets (click sau **↓** în câmp) deschide
o listă; Enter sau dublu click ia valoarea, Esc închide lista. O valoare folosită înainte **înlocuiește** câmpul (o cale
rapidă de revenire, de ex. după ce ai oprit un Exclude pentru un timp). La Highlight sets lista începe cu seturile; un set
se **adaugă** la sfârșitul câmpului (dacă nu e deja acolo), apoi urmează valorile folosite, după `── used before ──`.
Valorile se salvează la OK și Apply (nu cele goale, cel mult 20 pe câmp, cea mai nouă prima, fără repetări), sunt comune
tuturor Log View-urilor și se păstrează cât rulează tvterm.

Comanda rulează cu `$SHELL -c` într-un pseudo-terminal, deci programe ca awk scriu fiecare linie imediat. Culorile și
secvențele escape se elimină; tab-urile se păstrează. Folosește `tail -F` (nu `-f`), ca să fie urmată rotația logurilor.

**Log View-urile cu exact aceeași comandă folosesc aceeași sursă**: un proces, un buffer (bufferul păstrează cel mai
mare `max-lines` cerut). Mai multe ferestre cu filtre diferite pe același log costă un singur `tail`.

### Titlul

`[!cuvânt] Log: <titlu> [I] [E] afișate/total (ended) [PAUSE +N]`: marcajul de alertă, `[I]` / `[E]` când există
include / exclude, `[F-]` cu filtrele oprite, `[H-]` cu culorile oprite, liniile afișate / liniile din buffer, `(ended)` când comanda s-a terminat,
`[PAUSE +N]` în pauză, `[+N]` linii noi sub zona vizibilă.

### Taste

Și în meniul ferestrei (**F9** sau click dreapta); cele principale sunt în bara de stare, cu click.

| Tasta | Acțiune |
|---|---|
| F4 | dialogul de setări (OK / **Apply** / Cancel; Apply aplică și lasă dialogul deschis) |
| `I` / `E` | dialogul, pe Include / Exclude |
| `F` | filtrele pornit/oprit (se afișează toate liniile) |
| `H` | culorile pornit/oprit ([seturi de colorare](#culori-colorare)) |
| `D` | duplicare: o fereastră nouă pe aceeași sursă, cu aceleași filtre, culori și opțiuni (fără alerte), titlu + ` (2)` |
| `R` | repornește comanda |
| Space | pauză / continuare (liniile noi se păstrează, apar la continuare) |
| `W` | liniile lungi: rupte / derulate (Left / Right, câte 8 coloane) |
| `X` | golește această fereastră (nu și sursa) |
| `/` | căutare (text simplu sau expresie regulată; potrivirea negru pe galben) |
| F3 sau `n` / Shift-F3 sau `N` | potrivirea următoare, mai veche / mai nouă |
| Up / Down, PgUp / PgDn, Home / End | mută linia cursor |
| Shift + aceleași taste, Shift+click, tragere | selectează linii |
| Ctrl-C sau Ctrl-Ins | copiază selecția, altfel linia cursor |
| Ctrl-E | trimite selecția (sau linia cursor) într-un editor nou |
| Ctrl-A | o adaugă la sfârșitul unui editor deschis (ales dintr-o listă) |
| F2 sau `s` | salvează liniile afișate sau selecția |
| Esc | șterge selecția, apoi potrivirea căutării |
| dublu click | copiază un cuvânt |
| Ctrl-B K | alerte |

**Linia cursor** (negru pe cyan): cu cursorul pe ultima linie, liniile noi sunt urmărite; oriunde mai sus, vederea
rămâne pe loc, iar `[+N]` numără liniile noi; **End** urmărește din nou. Rotița mouse-ului și bara de derulare mută doar
vederea; ajunse jos, urmărirea repornește.

Filtrele noi se aplică imediat pe tot bufferul. Clipboard-ul primește liniile fără ultimul sfârșit de linie, deci
lipite într-un shell nu execută nimic. Salvarea folosește același dialog ca la istoric ([capitolul 4](#salvarea));
directorul propus este `save-dir`, altfel directorul curent al lui tvterm.

### Culori (colorare)

Un Log View își poate colora liniile cu **seturi de colorare**: numele lor se scriu în câmpul **Highlight sets** din F4
(unul sau mai multe, cu spații sau virgule: `zimbra-mta kv`; primul set are prioritate). Săgeata de lângă câmp listează seturile.
**H** oprește și pornește culorile (`[H-]` în titlu cât sunt oprite). Se schimbă doar afișarea: filtrele, căutarea, copierea și salvarea folosesc
textul simplu. Linia cursor și selecția își păstrează culorile, iar potrivirea căutării se desenează peste celelalte.

Seturi incluse în tvterm:

| Set | Ce colorează |
|---|---|
| `zimbra-mta` | Postfix / amavis (`zimbra.log`, `maillog`): roșu `status=bounced`, `reject:`, `NOQUEUE`, `SASL … authentication failed`; galben `status=deferred`, `warning:`; verde `status=sent`, `Passed CLEAN`; fiecare queue-ID cu culoarea lui; apoi `syslog` și `kv` |
| `zimbra-mailbox` | `mailbox.log`: ESMTP, IMAPS, Ban, reject:, `<a> -> <b>`…; apoi `levels`, `timestamps`, `kv` |
| `fortigate` | FortiGate: `srccountry` (Reserved / Romania pe albastru, celelalte pe roșu), `srcip=`, nume de politici, SMTPS / IMAPS; apoi `levels`, `timestamps`, `kv` |
| `fail2ban` | fail2ban: loggerul, acțiunea, jail-ul, IP-ul, Ban / DROP; apoi `levels`, `timestamps` |
| `nginx` | nginx: codul (5xx roșu, 4xx galben, 2xx verde), metoda; apoi `timestamps`, `kv` |
| `rsync` | rsync: start, stop, sent, received, failed…; apoi `timestamps`, `kv` |
| `syslog` | ora din syslog, estompată (`Oct 29 10:06:17` sau ISO `2026-10-29` + `T`, `^` sau spațiu + `10:06:17`, cu `.123` și `+02:00` / `Z` opționale), și numele programului (fiecare cu culoarea lui) |
| `kv` | paranteze, `;` `=`, chei (`cheie=`), adrese de e-mail, adrese IP și porturi, `ua=`, `mid=` |
| `levels` | DEBUG, INFO, WARN, ERROR, NOTICE, FATAL |
| `timestamps` | formele obișnuite de dată și oră de la începutul liniei, și cea din nginx |

Seturile proprii, sau copii modificate ale acestora, se scriu în `tvterm.ini` ca secțiuni `[highlight <nume>]`; o secțiune
cu numele unui set inclus îl înlocuiește (textul lor e în `source/tvterm/highlight.cc`, `builtinHighlightText`, în același
format). O regulă pe linie, în ordinea în care se aplică:

| Regula | Ce colorează |
|---|---|
| `line = <culoare> "<regex>" [icase]` | toată linia (prima regulă `line` potrivită dă culoarea de bază) |
| `match = <culoare> "<regex>" [icase]` | fiecare potrivire |
| `groups = <culoare> "<regex>" [icase]` | doar textul din paranteze, în fiecare potrivire |
| `hash = "<regex>" [icase]` | o culoare aleasă după valoare (parantezele sau, fără ele, potrivirea): aceeași valoare are mereu aceeași culoare |
| `include = <set> …` | regulile altor seturi, în acest loc (se pune la final) |

La suprapunere **câștigă prima regulă**, deci regulile specifice se pun primele, iar `include` la final. Culori:
`black red green yellow blue magenta cyan white`, `bright-` în fața oricăreia, și `on <culoare>` pentru fundal
(`bright-yellow on red`). Expresii regulate ca la filtre (ca `grep -E`), mereu între ghilimele duble; `\"` e o ghilimea în
interior. Din multitail: `cs_re` → `match`, `cs_re_s` → `groups`, `x,,bold` → `bright-x`, `.*?` → o clasă negată
(`ua=([^;]*);`).

```ini
[highlight my-mail]
line = white on red "status=bounced"
groups = bright-yellow "from=<([^>]*)>"
hash = "client=[^[]*\[([0-9.]+)\]"
include = zimbra-mta
```

Seturile se recitesc odată cu `tvterm.ini` (la 2 secunde după salvare); o regulă greșită e ignorată și raportată în
Event Viewer, cu linia ei.

### Când comanda se termină

Fereastra arată `-- source ended (exit N) --`, titlul `(ended)`, iar Event Viewer-ul o notă. **R** o repornește
(`-- source restarted --`), sau `Restart after N s` o face automat. Închiderea ultimei ferestre a unei surse îi oprește
procesul.

Profilurile salvează Log View-urile cu toate setările, dar nu și pauza sau selecția.

---

## 11. Exemple complete

Adresele și numele de mai jos sunt exemple (`192.0.2.x`, `example.com`); căile se adaptează la logurile tale.
Logul de e-mail este `/var/log/maillog` pe CentOS / RHEL; pe Debian / Ubuntu este `/var/log/mail.log`.

### Server de e-mail (Postfix / Dovecot)

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

„MTA all”, „MTA problems” și „Dovecot” folosesc același `tail`. „Dovecot” arată autentificările IMAP / POP3; `auth failed`
apare atât în versiunile vechi (`Disconnected (auth failed, ...)`), cât și în cele noi
(`Login aborted: ... (auth failed, ...)`). Cuvântul cheie `] Ban` (sfârșitul lui `[sshd]`, apoi `Ban`) nu prinde `Unban`;
marcajul lui e `[!"] Ban"]`.
Un queue ID selectat în „MTA all” și căutat (`/`) arată tot drumul unui mesaj. Pornire: `tvterm -p mail`
sau `profile = mail` în `tvterm.ini`.

Webmail (nginx), doar erorile de server:

```ini
[logview]
title = nginx 5xx
command = tail -n 5000 -F /var/log/nginx/access.log
include = " 5[0-9][0-9] [0-9]+ "
```

### Conexiuni TLS (în locul unui script `tail | awk`)

Un script ca

```sh
tail -n 10000 -F /var/log/maillog | awk '/TLS connection established from/ { ... print ... }'
```

poate fi comanda unui Log View așa cum este: pseudo-terminalul face ca awk să scrie fiecare linie imediat, iar fereastra
adaugă căutare, selecție, salvare și alerte. Programele awk lungi se pun într-un fișier:

```awk
# ~/bin/show-tls.awk: IP (aliniat la dreapta), data, serviciul (4 litere), numele gazdei
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

Cu mawk (`awk`-ul implicit pe Debian / Ubuntu) se scrie `mawk -W interactive -f ...`: altfel mawk așteaptă un bloc
întreg de la `tail`, iar fereastra rămâne goală. gawk (CentOS, RHEL) nu are nevoie de nimic.

Fără awk, liniile brute: `command = tail -n 10000 -F /var/log/maillog` cu
`include = TLS connection established from`.

### Logul firewall-ului (FortiGate prin rsyslog), în locul multitail

multitail cu mai multe vederi pe același fișier devine câte un `[logview]` pentru fiecare vedere, toate cu **aceeași
comandă** (deci un singur `tail` și un singur buffer):

| multitail | tvterm |
|---|---|
| mai multe `-e "a" -e "b"` (oricare) | `include = a|b` |
| mai multe `-ev "a" -ev "b"` | `exclude = a|b` |
| `-i fisier` cu `-n N -f` | `command = tail -n N -F fisier` |
| `-l "comanda"` | `command = comanda` |
| `-s 2` (aranjarea) | `pos` în profil sau `layout = tile-columns` |
| `-ke "regex"` (taie text din linie) | încă nu (planificat: transformări) |

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

Atenție la spațiul de la sfârșit: `policyid=37 ` (între ghilimele) nu prinde `policyid=370`. Cu `-n 50000` și linii
FortiGate (cam 1 KB fiecare), o sursă folosește cam 50 MB.

---

## 12. Probleme cunoscute

- **„Cannot initialize Ncurses: 'newterm' failed”** pe servere vechi (CentOS 7, Ubuntu 12) cu build-ul static făcut pe
  Alpine: ncurses-ul lui respinge fișierele terminfo vechi. Fie se copiază pe server un terminfo `xterm-256color` recent
  (de ex. de pe Alpine, `/usr/share/terminfo/x/xterm-256color`) în `~/.terminfo/x/` (un utilizator) sau
  `/etc/terminfo/x/` (toți utilizatorii); programele vechi ale sistemului merg în continuare. Fie se compilează pe
  Ubuntu 24 (glibc).
- În putty4far2l, **Shift+mouse** este folosit de selecția proprie: Shift+click din tvterm merge doar dacă aceasta e
  dezactivată acolo.
- **Alt+Shift+Up/Down** (selecția verticală din far2l) nu ajung prin putty4far2l / Windows, cu sau fără tvterm.
  Se folosesc Alt+Shift+Left/Right și Alt+Shift+PgUp/PgDn.
- Cuvintele cheie ale alertelor și expresiile regulate cu „Ignore case” ignoră majusculele doar la literele ASCII
  (`ș` nu prinde `Ș`). Căutarea de text simplu din istoric ignoră majusculele și la diacritice.
- Căutarea din istoric folosește expresii regulate ECMAScript, iar Log View-ul expresii POSIX (ca `grep -E`): diferă în
  detalii (de ex. `\d` merge doar în istoric).
- Log View: bara de derulare numără linii, nu rânduri rupte; căutarea găsește prima potrivire din fiecare linie;
  repornirea unei surse `tail -n N` arată din nou acele N linii.
- `WinList` din bara de stare nu face nimic cât e deschis un dialog.
- Pe ecrane mai înguste de aproximativ 156 de coloane, contorul `Alerts: N` acoperă `WinList` în bara de stare a Log View-ului.
