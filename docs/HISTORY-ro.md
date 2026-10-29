# HISTORY — tvterm (deep-soft)

Istoricul dezvoltării din fork-ul deep-soft/tvterm: ce s-a implementat, cum funcționează,
cum se testează și ce capcane au apărut. Document de referință, actualizat la fiecare
funcționalitate nouă. Starea curentă și pașii următori sunt în STATUS.md (în Project-ul Claude).

Limba: română. Versiunea în engleză: [HISTORY.md](HISTORY.md); cele două se actualizează împreună.
Cum se folosește tvterm acum (taste, setări, profiluri, exemple): [MANUAL-ro.md](MANUAL-ro.md) / [MANUAL.md](MANUAL.md).

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
20. Pass Ctrl+\ [ ] _ and the keypad operators to programs
21. Fix the doubled path in the profile file dialogs
22. Fix overlapping memcpy in tvision's input line history
23. Paste and copy in the text fields of dialogs
24. Keep profile positions inside the desktop
25. Tile and Cascade leave the Event Viewer and editors in place
26. Tile and Cascade use the largest area beside the fixed windows
27. Ask whether to close the open windows when loading a profile
28. Keep the scrollback following new output

### Commit-uri pe `far2l-extended`

Numerotate E1, E2, … ca să nu se schimbe când apar commit-uri noi pe `far2l-integration`.

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
E14. Log View: highlight sets (colors from tvterm.ini)
E15. Log View: the syslog highlight set also dims ISO times with ^ or a space
E16. Log View: lists next to the Include, Exclude and Highlight sets fields
E17. Ctrl-B menu: W opens the Window List, Close Term has no key

---

## 2. Funcționalități (toate testate de utilizator în lanțul real, dacă nu e menționat altfel)

### Meniu (Ctrl-B)
`N` New Term · Close Term (fără tastă) · `Tab`/`Shift-Tab` Next/Prev · `W` Window List · `T` Rename Term · `K` Alerts · `G` Go to Next Alert ·
`R` Resize/Move · `F` Maximize · `S` Select Text · `P` Paste · `E` New Editor · `O` Open File in Editor · `V` Event Viewer ·
`D` Log View · `L` Load Profile · `A` Save Profile As · `H` History · `More...` (change dir, tile, cascade, Grab Input, Edit settings,
Reload settings, About) · `U` Suspend · `Q` Exit.
Litere libere în meniu: B, C, I, J, X, Y, Z (`M` deschide More...).
- E17 (doar `far2l-extended`; în magiblot W = Close): `W` deschide Window List (înainte `J`, acum liberă); Close Term nu
  are tastă, se alege cu săgețile + Enter sau cu mouse-ul, ca o fereastră să nu fie închisă din greșeală (Window List și
  butonul de închidere din ramă închid în continuare ferestre). Actualizate: MANUAL capitolul 2 și
  `examples/profiles/example.ini`.
- **Tile / Cascade** (commit 25): aranjează doar terminalele (și celelalte ferestre aranjabile); Event Viewer și ferestrele de editor
  rămân pe loc. Celelalte folosesc cel mai mare dreptunghi care nu atinge niciuna dintre ele, fiecare fereastră fixă luată separat
  (commit 26; commit-ul 25 folosea dreptunghiul care le cuprindea pe toate, iar cu un editor stânga sus și Event Viewer în dreapta
  acesta acoperea tot Desktop-ul → terminalele erau aranjate peste ele). Fără ferestre fixe deschise sau cu dreptunghiul liber sub
  20 de coloane × 6 rânduri → tot Desktop-ul, ca înainte. Spațiul liber în formă de L nu se împarte în mai multe zone.
  Și pentru `layout` din profiluri. Cod: `TVTermDesk::freeArea` (`desk.cc`; `noFreeArea` spune apelantului că s-a folosit tot
  Desktop-ul).

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
- **Numele fișierului** (E11; același dialog salvează și din Log View): grupul `File name` între câmpul File și „What to save”,
  `Date first` (`YYYY-MM-DD-hhmm-tvterm-<titlu>.txt`, ordonate după dată în orice listă de fișiere) sau `Name first` (forma de
  mai sus). Comutarea reface imediat câmpul File, doar cât timp conține încă numele propus (un nume editat rămâne neatins);
  directorul rămâne. Alegerea de pornire vine din `save-name` din `tvterm.ini` (implicit `name-first`); alegerea de la ultima
  salvare e ținută minte până la ieșirea din tvterm (Cancel nu o schimbă), cu excepția cazului când `save-name` se schimbă
  între timp. tvterm nu rescrie niciodată `tvterm.ini`.
- **Mouse**: dublu click selectează un cuvânt (litere, cifre, `word-chars`; orice caracter non-ASCII contează ca literă),
  triplu click toată linia logică (cu sfârșitul de linie); tragerea după aceea extinde pe cuvinte/linii. **Shift+click**
  forțează selecția în programele care folosesc mouse-ul (far2l), dacă `shift-click-select = yes`. Copierea, ca până acum (Ctrl-C).
  În lanțul real, putty4far2l prinde Shift+mouse pentru selecția proprie (pe tot ecranul exterior), deci tvterm nu primește
  Shift+click cât timp acea selecție e activă în putty4far2l (utilizatorul a dezactivat-o). Selecția tvterm (fără Shift,
  în ferestre shell) funcționează și rămâne în fereastra curentă.
- Liniile vechi eliminate dintr-un istoric plin nu mai deplasează selecția sau potrivirea.
- Bara de stare (E6): click pe `/ Search`, `F3 Next`, `Shift-F3 Prev`, `F2 Save`, `Ctrl-C Copy`, `Esc Exit` = tasta respectivă
  (`F3/Shift-F3 Next/Prev` a fost împărțit în două); tastele de derulare rămân doar indicație.

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
  în frunte), chiar dacă T e oprit; **X** golește fereastra. Bara de stare le afișează, câte un element pentru fiecare tastă
  (`M Moves`, `C Clicks`, `K Keys`, `O Other`, `T Messages`, `N Notes`, `X Clear`); click pe un element = tasta lui (E6).
- Titlul arată starea: `Event Viewer [M- C+ K+ O+ T+]` (`(Stopped)` înaintea parantezelor când e pe pauză).
  Implicit: mișcările de mouse ascunse, restul afișate. Filtrul se aplică doar evenimentelor noi.
- Evenimentele ascunse se numără în continuare: golurile din `Received event #N` le arată.
- **A** (E13): listează alertele active (cele numărate de `Alerts: N`), de la cea mai veche (ordinea lui Ctrl-B G): ora,
  marcajul, titlul ferestrei; la alertele de cuvânt cheie și grupul găsit și linia (cam 150 de octeți în jurul primului
  termen). Antet `--- Active alerts (N) ---` sau `--- No active alerts ---`; afișat și în pauză sau cu T-. Element în bara
  de stare `A Alerts`; titlul `Event Viewer [M- C+ K+ O+ T+] A: alerts`. Lista o face aplicația (`listAlerts`, mesajul
  `cmShowAlerts` trimis de fereastră); fiecare `AlertTarget` dă un `AlertInfo` (ora tipului actual, marcaj, titlu, grup, linie).
- Buffer: 64000 de octeți (aproximativ 220 de evenimente cu toate liniile lor), în loc de 3840; `TTerminal` din Turbo Vision
  îl limitează la 32000, așa că `EventTerminal` din tvterm înlocuiește bufferul după construcție (poziții pe 16 biți, până la 65535).
- Profiluri: `[eventviewer]` acceptă `mouse-move`, `mouse-clicks`, `keys`, `other`, `tvterm-messages` (yes/no); Save Profile As
  le scrie pe toate cinci, cu un comentariu care explică literele. Cheile lipsă primesc valorile implicite; valorile greșite
  sunt raportate ca celelalte probleme din profil. Documentat în `examples/profiles/example.ini`.

### Fișierul de setări (`tvterm.ini`) — `far2l-extended`
- `~/.config/tvterm/tvterm.ini` (sau `$XDG_CONFIG_HOME/tvterm/`), secțiuni `[general]` (`profile`, `auto-reload`),
  `[scrollback]` (`lines`, `wheel-lines`, `word-chars`, `save-dir`, `save-name` (E11), `search-case-sensitive`, `shift-click-select`),
  `[keys]` (`history-page-up/-page-down/-top/-bottom`, ex. `Ctrl-F5`, `none`). Documentat în `examples/tvterm.ini`.
- **Profil implicit**: `profile = NUME` se deschide când tvterm pornește fără `-p`; `-p ALTUL` are prioritate; `-p none` pornește fără el.
  Dacă lipsește sau are erori: se deschide un terminal normal, iar motivul apare în Event Viewer și în bara de stare.
- Reîncărcare automată (verificare la 2 s, după data modificării și mărime) și din More... > Reload settings;
  More... > Edit settings creează fișierul (toate cheile comentate) și îl deschide în editor. Totul în afară de `profile`
  se aplică imediat (un `lines` mai mic elimină cele mai vechi linii). Valorile invalide își păstrează valoarea anterioară;
  cheile șterse (sau fișierul șters) revin la valorile implicite.
- **Mesaje**: mesajele tvterm (setări, fișiere salvate, erori de profil) apar în Event Viewer ca `tvterm: hh:mm:ss ...`
  (`YYYY-MM-DD hh:mm:ss` pentru un mesaj din altă zi, E13: notele își păstrează `time_t` și sunt formatate la afișare),
  chiar și în pauză; ultimele 200 se păstrează și se afișează la deschidere. Mesajele scurte apar 5 s în bara de stare.

### Alerte — `far2l-extended`
- Doar pentru terminalele **neselectate**. Trei tipuri, setate per terminal cu **Ctrl-B K** (Alerts...) sau în profil:
  **activitate** (orice text nou), **cuvinte cheie** (separate prin virgulă, text simplu, fără diferență între majuscule
  și minuscule pentru literele ASCII), **liniște** (N secunde fără ieșire, numărate de la pornirea terminalului sau de la
  setarea alertei, apoi de la ultima ieșire și de la părăsirea terminalului; după o alertă de liniște, re-armată de ieșire nouă).
- Afișare: marcaj în titlul ferestrei (`[!]`, `[!error]`, `[!silent]`, deci și în Window List și în dialogul de ieșire),
  `Alerts: N (Ctrl-B G)` în dreapta barei de jos. **Ctrl-B G** duce la cea mai veche alertă. Selectarea terminalului șterge alerta.
  Prioritate când apar mai multe: cuvânt cheie > liniște > activitate.
- Chei în profil: `alert-activity = yes`, `alert-keywords = error, reject`, `alert-silence = 60`; salvate de Save Profile As.
- Alarme false evitate: ieșirea e ignorată 3 s după pornirea terminalului și 1,5 s după redimensionare (tile, maximize)
  sau după ieșirea din terminal (programele redesenează atunci); această ieșire repornește totuși numărarea liniștii.
  Alerta de activitate rămâne zgomotoasă cu programe full-screen (ceasul far2l se schimbă în fiecare minut) → acolo cuvinte cheie.
- Cum: emulatorul (firul de lucru) numără octeții primiți și păstrează textul nou fără secvențe escape
  (`OutputTextFilter`: CSI/OSC/DCS/APC eliminate, mișcările cursorului devin sfârșit de linie, deci titlul ferestrei și
  datele APC ale far2l nu se potrivesc niciodată; maximum 64 KB); `TerminalView` îl preia la fiecare `cmCheckTerminalUpdates`,
  chiar dacă fereastra e ascunsă; `TerminalWindow::checkAlerts` face căutarea (cuvintele tăiate între două citiri sunt găsite).
- Test real (CentOS 7.9): activitatea și cuvintele cheie au mers; liniștea era armată doar de ieșire, deci un `tail -f`
  pe un log liniștit nu dădea alertă până nu veneau linii noi → schimbat: liniștea se numără de la pornirea
  terminalului (testat de utilizator în lanțul real).
- **Grupuri și excluderi la cuvintele cheie (E13)**: virgula = grupuri alternative; `+` = termeni care trebuie să fie toți
  în aceeași linie; `!termen` = excludere globală (doar ca element separat; o linie care îl conține nu dă alertă de cuvânt
  cheie); doar excluderi → orice altă linie care nu e goală (marcaj `[!*]`). `\` face caracterul următor literal
  (`\!` `\+` `\,` `\\` `\ `); spațiile neprotejate de la capetele termenilor se elimină. `!` într-un grup cu `+` sau un
  termen gol (`c++`) e eroare: dialogul Alerts arată un mesaj și rămâne deschis; profilul o raportează ca problemă (cu
  numărul liniei) și folosește elementele valide. Valorile vechi (liste simple cu virgulă) își păstrează sensul, în afară
  de cele cu `+` (se scrie `\+`). `KeywordRules` (alerts.h) păstrează textul așa cum a fost scris (salvat neschimbat în
  profil; cititorul de profil păstrează un spațiu protejat de la capătul liniei) și grupurile/excluderile (litere ASCII mici).
  Căutarea e acum **pe linie**: liniile din Log View ca înainte; în terminale, ieșirea filtrată se împarte la sfârșiturile
  de linie, iar o linie neterminată așteaptă și se verifică după 0,5 s fără ieșire nouă (prompturi), deci o excludere care
  sosește o clipă mai târziu pe aceeași linie contează (înlocuiește vechea coadă de `maxLen - 1` octeți).
  Marcaj: grupul cu `+` fără spații, scurtat la 24 de octeți (`…`, fără a tăia caractere UTF-8), între ghilimele când
  conține `[` sau `]` (`] Ban` dădea `[!] Ban]`, acum `[!"] Ban"]`). Câmpul Keywords: 1023 de caractere (era 255).
- Ora alertei (E13): fiecare fereastră reține ora (ceasul real) la care alerta a primit tipul actual (activitate → cuvânt
  cheie o actualizează), afișată de A în Event Viewer.

### Log View — `far2l-extended`
- **Ctrl-B D** deschide un dialog: titlu (gol: comanda), **comanda** (cu lista comenzilor anterioare, ↓), **include** și
  **exclude** (câte un regex, POSIX extins ca `grep -E`; alternative cu `|`, ex. `dstport=(993|465|587) `; spațiile de la final
  se păstrează), fără diferență de majuscule, linii rupte, linii maxime (0: `[logview] max-lines` din tvterm.ini, implicit 50000),
  repornire după N secunde (0: nu), **Filters on** (E7; la fel ca tasta F). Valorile greșite sunt raportate în dialog, care rămâne deschis cu focus pe câmp.
  **F4** pe o fereastră deschisă: același dialog (modal) cu **OK / Apply / Cancel**; Apply aplică setările și lasă dialogul
  deschis (ca să vezi efectul filtrelor); Cancel îl închide și renunță doar la ce nu s-a aplicat.
- Un LogView e o fereastră, nu un terminal: arată liniile scrise de comandă. Oricâte; ferestrele cu **aceeași comandă folosesc
  o singură sursă** (un proces, un buffer; bufferul ține cel mai mare număr de linii cerut). O linie trece dacă se potrivește
  cu include (sau nu există) și nu cu exclude. **Filtrele noi se aplică pe tot bufferul** (50000 de linii FortiGate: ~0,1 s).
- Sursa rulează cu `$SHELL -c comanda` (ca la terminale) într-un pseudo-terminal (`TERM=dumb`, 512 coloane), deci programe
  ca awk scriu fiecare linie imediat (în pipe țin 4 KB). Secvențele escape, `\r` și alte caractere de control se elimină;
  tab-urile se păstrează (afișate până la următorul multiplu de 8). Liniile peste 64 KB se taie; o linie fără `\n` apare
  după 1 s fără date. Citire în timpul liber al aplicației (fără thread-uri, maximum 4 MB pe trecere).
  Când comanda se termină: `-- source ended (exit N) --` (galben), notă în Event Viewer, `(ended)` în titlu; **R** o repornește
  (`-- source restarted --`), sau automat după N secunde. Închiderea ultimei ferestre care folosește o sursă trimite SIGHUP
  grupului de procese (SIGKILL după 2 s, fără blocare).
- Titlu: `Log: <titlu> [I] [E] afișate/total`, `[F-]` cu filtrele oprite, `(ended)`, `[PAUSE +N]` în pauză, `[+N]` când ai urcat
  în istoric și au venit linii noi, marcajul de alertă în față (`[!reject] Log: ...`).
- Taste (fereastra cu focus; și în meniul ei, **F9** sau click dreapta; bara de stare le arată pe cele principale, iar un click
  pe una dintre ele face ce face tasta: F9, F4, I, E, F, Space, /, Ctrl-C, F2): F4 setări,
  I / E dialogul pe include / exclude, F filtre active/oprite, D duplicat (aceeași sursă, filtre copiate, titlu + ` (2)`),
  R repornire, Space pauză/continuare, W linii rupte / derulare orizontală (←→, 8 coloane), X golește fereastra (nu sursa),
  `/` căutare (text simplu sau regex; potrivirea e negru pe galben), F3 sau n mai vechi, Shift-F3 sau N mai nou, F2 sau s salvare,
  Ctrl-C (sau Ctrl-Ins) copiere, Ctrl-E într-un editor nou, Ctrl-A adăugare la finalul unui editor deschis (ales din listă;
  focusul rămâne în LogView) — toate trei folosesc selecția sau, fără ea, linia cursorului —, rotița mouse-ului, Esc șterge
  selecția, apoi potrivirea.
- **Cursor de linie** (negru pe cyan, doar în fereastra cu focus): Sus/Jos/PgUp/PgDn/Home/End îl mută, vederea se derulează
  doar cât să-l țină pe ecran; click-ul îl pune pe o linie; căutarea îl pune pe potrivire; la filtre noi rămâne pe aceeași linie
  (dacă e ascunsă: cea mai apropiată dinaintea ei); cu W, toate rândurile liniei. **Cu cursorul pe ultima linie, liniile noi sunt
  urmărite**; mai sus, vederea rămâne fixă (`[+N]` numără liniile noi), End reia urmărirea. Rotița și bara de derulare mută doar
  vederea: la final, urmărirea reîncepe și cursorul trece pe ultima linie.
- Selecție: linii întregi; click, tragere, Shift+click, Shift+Sus/Jos/PgUp/PgDn/Home/End (de la linia cursorului); Sus/Jos fără
  Shift o șterg. Dublu click copiază un cuvânt. Clipboard-ul
  primește liniile fără ultimul sfârșit de linie (la paste în shell nu se execută nimic). Salvare: liniile afișate sau selecția.
- Alerte (Ctrl-B K, aceleași chei ca la terminale) pe liniile care trec de filtre; nimic în primele 3 s după pornirea sursei
  (ex. `tail -n 5000`). Codul alertelor e comun prin interfața `AlertTarget` (terminal și LogView).
- Window List (`Log`), Rename (Ctrl-B T), dialogul de ieșire (listează LogView-urile cu comanda lor), profiluri:
  `[logview]` cu `title`, `command`, `include`, `exclude` (între ghilimele când încep/se termină cu spații), `ignore-case`, `wrap`,
  `filters` (E7: `yes`/`no`, lipsă = `yes`, o valoare greșită e raportată și filtrele rămân active), `max-lines`,
  `auto-restart`, `alert-*`, `pos`. Un regex greșit în profil: raportat, fereastra se deschide fără el.
  Pauza și selecția nu se salvează.
- Test real (prima versiune, apoi v2, pe CentOS 7.9 și Alpine): v2 testat de utilizator și urcat. Starea F face acum parte
  din setările ferestrei (E7): F, căsuța din F4, D și profilurile folosesc aceeași valoare.

### Colorare în LogView — `far2l-extended`
- E14: culori în ferestrele LogView (doar acolo, nu în terminale), doar la afișare: filtrele, căutarea, F2 și copierea
  rămân pe textul simplu. Se colorează doar rândurile de pe ecran; culorile unei linii se păstrează cât rămâne pe ecran.
  Linia cursorului, selecția și marcajele `--` își păstrează culorile; potrivirea căutării se desenează peste colorare.
- Seturi în `tvterm.ini`, `[highlight <nume>]` (nume: litere, cifre, `-`, `_`, `.`), o regulă pe linie, în ordine:
  `line = <culoare> "<regex>" [icase]` (toată linia; prima regulă `line` potrivită dă culoarea de bază),
  `match` (toate potrivirile; = `cs_re` din multitail), `groups` (doar textul din paranteze, toate grupurile, toate
  potrivirile; = `cs_re_s` / `mcsre_s`; fără paranteze: raportat), `hash = "<regex>"` (fără culoare: culoare derivată din
  valoarea grupurilor sau, fără grupuri, a potrivirii; FNV-1a peste 8 culori fără roșu/galben/verde: bright cyan,
  bright magenta, bright blue, cyan, magenta, bright white, bright white pe albastru, bright white pe magenta),
  `include = <set> …` (regulile celuilalt set în acel loc; se pune la final, ca regulile specifice să fie primele).
  La suprapunere **câștigă prima** (`line` dă doar culoarea de bază, celelalte se desenează peste). În ghilimele `\"` e
  ghilimea; restul backslash-urilor trec neschimbate la regex (POSIX extins, ca la filtre).
- Culori: `black red green yellow blue magenta cyan white`, `bright-<culoare>`, opțional `<culoare> on <culoare>` (fundal;
  implicit negru). Fără 256/RGB. Conversie din multitail: `x,,bold` → `bright-x`, `x,y,bold` → `bright-x on y`.
- Seturi incluse (o secțiune cu același nume în `tvterm.ini` le înlocuiește): convertite din `multitail.conf-user` al
  utilizatorului — `zimbra-mailbox`, `fail2ban`, `fortigate` (fără regulile de tip zimbra copiate acolo), `rsync`;
  comune — `kv` (paranteze, `;=`, `cheie=`, e-mail, IP, `IP:port`, `ua=`, `mid=`), `levels` (DEBUG…FATAL), `timestamps`;
  noi — `zimbra-mta` (roșu `status=bounced`, `reject:`, `NOQUEUE`, `SASL … authentication failed`; galben
  `status=deferred`, `warning:`; verde `status=sent`, `Passed CLEAN`; queue-ID cu `hash`), `nginx` (5xx/4xx/2xx, metoda),
  `syslog` (ora estompată, numele programului cu `hash`). Textul lor: `builtinHighlightText()` în
  `source/tvterm/highlight.cc` (format INI, se poate copia în `tvterm.ini`). Regex-uri convertite: `.*?` / `*?` → clase
  negate (`ua=(.*?);` → `ua=([^;]*);`), `\:` → `:`, fără backslash în `[ ]`. Verificat: glibc și musl acceptă `\b`, `\<` și
  `*?`, dar `*?` e lacom pe amândouă (`a.*?b` pe `aXbYb` = tot textul), deci seturile incluse nu le folosesc; cele 71 de
  expresii dau aceleași poziții (inclusiv grupurile) pe glibc și musl.
- Per fereastră: câmpul **Highlight sets** în F4 (unul sau mai multe nume, cu spații sau virgule: `zimbra-mta kv`; primul
  set are prioritate); un nume necunoscut e eroare în dialog (cu lista seturilor); seturi noi în F4 pornesc colorarea.
  **H** (și F9 > Highlight on/off) o pornește/oprește, titlu `[H-]` cât e oprită. Profil: `highlight = …`,
  `highlight-on = no` (scris doar când e oprită). D le copiază pe amândouă. Un set necunoscut în profil sau după
  reîncărcare: notă în Event Viewer (o dată pe fereastră și valoare), celelalte seturi se folosesc.
- `tvterm.ini` reîncărcat (2 s sau Reload settings): seturile se refac, ferestrele le preiau la următoarea actualizare.
  O regulă greșită (regex, culoare, cheie, ghilimea lipsă, `hash` cu culoare, `groups` fără paranteze, include
  necunoscut, set care se include pe sine): `settings: <cale>: line N: [highlight <set>] …; rule ignored` în Event Viewer.
- Șablonul din Edit settings: un exemplu comentat `[highlight my-mail]` cu tipurile de reguli.
- Amânate: o regulă care dă alertă; culori 256/RGB.
- E15 (după testul E14 al utilizatorului): setul inclus `syslog` estompează ora ISO cu `T`, `^` sau spațiu între dată și
  oră (`2026-10-29T10:06:17`, `2026-10-29^10:06:17`, `2026-10-29 10:06:17`), cu fracțiuni de secundă și fus orar opționale
  (`.123`, `+02:00`, `Z`), precum și `Oct 29 10:06:17`, ca logurile îmbinate din surse diferite să arate la fel.
  S-a schimbat doar acea regulă: `[T^ ]` în loc de `T` (un `^` care nu e primul în `[ ]` e literal în POSIX).
- E16: o săgeată lângă Include, Exclude și Highlight sets în F4 (ca la Command): click sau ↓ în câmp deschide o listă.
  Include / Exclude: valorile folosite (una înlocuiește câmpul; o cale rapidă de revenire, de ex. după ce un Exclude a
  fost oprit cu `!(…)`). Highlight sets: întâi seturile (incluse + `tvterm.ini`, sortate; unul se adaugă la sfârșitul
  câmpului cu un spațiu, dacă nu e deja acolo; cursorul la sfârșit, nimic selectat), apoi `── used before ──` și
  valorile folosite (una înlocuiește câmpul). Valorile se salvează la OK și Apply (doar cele valide), nu cele goale, cel
  mult 20 pe câmp, cea mai nouă prima, o valoare repetată urcă în capul listei; comune tuturor Log View-urilor, păstrate
  cât rulează tvterm. Cod (`logview.cc`): `FieldHistory`, `FieldHistoryButton` (un `THistory` al cărui `recordHistory` nu
  face nimic: tvision înregistrează câmpul la deschiderea listei și copiază cel mult 255 de octeți, iar Include / Exclude
  au 512), `FieldListWindow` / `FieldListViewer` (`THistoryWindow` / `THistoryViewer` cu elemente proprii; lista are
  între 5 și 12 rânduri și urcă dacă nu încape sub câmp). ID-ul de istoric 77 nu primește niciodată valori.

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
  `[terminal]` (`title`, `dir`, `command`, `keep-open` yes/no, `pos`), `[editor]` (`file`, `pos`), `[eventviewer]` (`pos`, filtre: `mouse-move`, `mouse-clicks`, `keys`, `other`, `tvterm-messages`).
- `pos = x y lățime înălțime` în **procente** din desktop, cu zecimale. Save Profile scrie pozițiile în interiorul Desktop-ului
  (o fereastră mutată parțial în afara ecranului e salvată ca mutată înapoi înăuntru; rotunjire la o zecimală, cu x + lățime ≤ 100);
  la încărcare, o poziție în afara Desktop-ului e mutată înăuntru (și micșorată dacă e mai mare), nu respinsă (commit 24;
  înainte: `invalid pos '84.9 0 15.4 72.7'`). Ajustarea e reținută în `Profile::notes`;
  în `far2l-extended` apare în Event Viewer (E9): `profile <cale>: pos '…' is outside the desktop, using '…'`.
- `keep-open = yes` (implicit): după terminarea comenzii sau Ctrl+C, pornește shell-ul; `no`: rămâne „(Disconnected)” cu output vizibil.
- Comenzi care rulează continuu (`watch`, `tail -f`) trebuie oprite cu Ctrl+C ca să apară shell-ul.
- `mailq` poate lipsi din PATH pentru non-root → `/usr/sbin/mailq` sau `postqueue -p`.
- **Save Profile As** (Ctrl-B A): salvează toate ferestrele (ordine, focus, titlu fix, poziții, directorul curent real din `/proc/<pid>/cwd`,
  comanda din profil sau comanda din prim-plan detectată prin `tcgetpgrp` + `/proc/<pgid>/cmdline`).
  Textul editoarelor „Untitled” nu se salvează (avertisment).
- Load Profile / Save Profile As listează directorul de profiluri cu wildcard-ul `*.ini`; `..\` și directoarele merg normal,
  iar un nume tastat e relativ la directorul afișat (commit 21; înainte, calea completă apărea dublată: „Invalid drive or directory”).
- Load Profile cu ferestre deschise (commit 27): „Close the current windows first?” Yes / No / Cancel. Yes = terminalele (și Log View-urile active) sunt
  listate ca la ieșire (titlu „Close windows”, buton Close, Go to), editoarele cu modificări nesalvate întreabă de salvare (Cancel
  oprește încărcarea), apoi toate ferestrele se închid și profilul se deschide; No = ferestrele profilului se adaugă lângă cele
  deschise (ca înainte); Cancel = nu se încarcă nimic. `confirmQuit(deskTop, closing)` în `quitdlg.cc`,
  `TVTermApp::closeWindowsBeforeLoad` în `app.cc`.
- Documentație completă: `examples/profiles/example.ini`.

### Manual — `far2l-extended`
- `docs/MANUAL.md` (engleză) și `docs/MANUAL-ro.md` (română), sincronizate (E12): cum se folosește tvterm acum —
  compilare și linia de comandă, meniul Ctrl-B, terminale și clipboard, istoric, editor, toate cheile din `tvterm.ini` cu
  valorile implicite, profiluri, Event Viewer, alerte, Log View, exemple complete (Postfix și Dovecot, fail2ban, nginx, un
  script `tail | awk`, multitail cu mai multe vederi pe logul unui firewall) și probleme cunoscute. Scris din surse (taste,
  valori implicite, mesaje), nu din memorie; exemplele folosesc `192.0.2.x` / `example.com`.
- HISTORY spune ce s-a schimbat și de ce; manualul spune cum funcționează acum. Fiecare patch următor actualizează
  secțiunile din manual pe care le atinge când se schimbă ceva vizibil (taste, setări, chei de profil, afișare, mesaje);
  corecturile interne intră doar în HISTORY.

### Altele
- **Numele din bara de stare** (E10): `Ctrl-B Open Menu` → `Ctrl-B Menu`, `Window List` → `WinList` (cu 7 coloane mai puțin,
  deci contorul `Alerts: N (Ctrl-B G)`, desenat peste capătul din dreapta, îl acoperă mai rar).
- **Literele rapide din dialoguri** (E10): două elemente ale aceluiași dialog nu mai au aceeași literă. Alerts: `K~e~ywords`
  (Alt-K era luat de `O~K~`, care câștiga); dialogurile de salvare din History / Log View: `Se~l~ection only` (Alt-S era și
  `~S~ave`). Verificate: Alerts, setările Log View (F4), dialogurile de căutare și salvare, Add to editor, Window List,
  Quit / Close windows, Rename, About.
- **Tile / Cascade fără loc** (E10): când ferestrele fixe nu lasă un dreptunghi de cel puțin 20 × 6 și se folosește tot
  Desktop-ul (commit 26), în Event Viewer apare nota „no free area beside the fixed windows, using the whole desktop”
  (`TVTermApp::noteNoFreeArea`, și pentru `layout` din profiluri).
- **Window List** (Ctrl-B W (J înainte de E17) sau click pe `WinList` în bara de stare, E8: imediat după `Ctrl-B Menu` în barele de
  terminal, LogView, Event Viewer și editor, la final în barele History, selecție și Grab Input; nu la mutare/redimensionare
  și nici în meniu; fără tastă; în Grab Input click-ul eliberează întâi intrarea): Go / Rename / Close. **Rename Term** (Ctrl-B T); titlu gol = titlu automat.
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

---

## 3. Arhitectură și fișiere cheie

```
source/tvterm-core/        (biblioteca, potențial interesantă pentru upstream)
  vtermemu.cc              emulator (fir de lucru): libvterm, win32-input, OSC 52, APC far2l, paste
  far2lext.cc/.h           protocolul extensiilor far2l (clipboard), ExtraFormatStore (blocuri verticale)
  outfilter.cc             OutputTextFilter: textul simplu din ieșirea programului (pentru alerte)
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
  alerts.cc/.h             setări alerte (Ctrl-B K), Ctrl-B G, AlertStatusLine (o TVTermStatusLine cu contorul de alerte),
                           AlertTarget (interfața ferestrelor cu alerte); logica în wnd.cc (TerminalWindow::checkAlerts)
                           și logview.cc (LogViewWindow::checkAlerts)
  logsrc.cc/.h             LogSource: o comandă într-un pseudo-terminal, linii fără secvențe escape, partajată după comandă,
                           citită în idle (LogSource::pollAll), oprită fără blocare
  logview.cc/.h            LogViewWindow (filtre, derulare, linii rupte, căutare, selecție, editor, salvare, meniul F9, alerte),
                           LogViewInterior (desenare, mouse), dialogul de setări (execLogViewDialog); regex POSIX (regcomp)
  highlight.cc/.h          seturile de colorare din LogView: regulile [highlight <nume>] (citite și compilate odată cu
                           tvterm.ini), seturile incluse, highlightLine (culorile octeților unei linii)
  procinfo.cc/.h           funcții /proc: readLink, readCommandLine, describeForeground (ce rulează într-un terminal)
  editwnd.cc/.h            editor (dialoguri Save As etc.)
  evntview.cc/.h           Event Viewer (filtre, tastele N/X; EventTerminal: TTerminal cu buffer mai mare)
  dlgclip.cc/.h            clipboard în câmpurile din dialoguri (TInputLine cu focus; apelat din TVTermApp::getEvent)
  wnd.h                    TerminalWindow (+ launchCommand/launchDir/launchKeepOpen)
  settings.cc/.h           tvterm.ini: Settings, loadSettings/applySettings, nume de taste, șablon, FileStamp
  histdlg.cc/.h            dialogurile de căutare și salvare din modul istoric; openTextSave (dialogul de salvare pentru LogView)
  statline.cc/.h           bară de stare care poate afișa un mesaj câteva secunde; elemente care trimit o tastă la click
                           (keyCommand: o comandă pentru fiecare tastă, transformată în eveniment de tastă în putEvent)
include/tvterm/termopts.h  TerminalOptions (global): mărime istoric, rotiță, caractere de cuvânt, taste, apeluri către aplicație
deps/vterm/patches/0001-win32-input-mode.patch   aplicat automat de CMake pe o copie (submodulul rămâne curat)
deps/tvision-fixes.cmake   corecturi tvision: histlist.cpp copiat în directorul de build, memcpy → memmove (submodulul curat)
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
- **Build static și sisteme vechi**: executabilul static conține ncurses-ul mașinii de build, care citește la pornire
  descrierea terminalului (terminfo, ex. `xterm-256color`). ncurses de pe Alpine (musl) respinge fișierele terminfo vechi
  (ex. cel din 2011 de pe Ubuntu 12, CentOS 7.9): fișierul e găsit și deschis, apoi `newterm` eșuează („Cannot initialize
  Ncurses”). Două soluții verificate: (1) build pe Ubuntu 24 (glibc + ncurses 6.4), care merge pe Ubuntu 12 și CentOS 7.9;
  (2) un `xterm-256color` nou (ex. de pe Alpine) copiat în `~/.terminfo/x/` (un utilizator) sau `/etc/terminfo/x/` (toți).
  Programele vechi ale sistemului (ncurses 5.9) sar peste fișierul în format nou și îl folosesc pe al lor (verificat `top`, `mc`).
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
- Unity build: un fișier `.cc` nou schimbă grupurile de fișiere compilate împreună și poate scoate la iveală un include lipsă
  în alt fișier (E14: `histdlg.cc` folosea `MAXPATH` adus de un fișier dinaintea lui; acum include `compat/borland/dir.h`).
- `TFileDialog`: câmpul `directory` pornește gol → numele tastate sunt relative la directorul curent; tvterm îl setează la directorul profilurilor.
  Wildcard-ul trebuie să fie doar un tipar (`*.ini`): tvision îl lipește la director (panoul de jos, câmpul Name pentru un director focusat).
  O cale completă acolo apare dublată → dialogul e creat cu `fdNoLoadDir`, apoi se setează `directory` și se citește lista (`profiledlg::newDialog`, `app.cc`).
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
- Clipboard fără far2l: un `xclip` fals în `PATH` (citește/scrie un fișier) + `DISPLAY=:99` → tvision îl folosește pentru
  TClipboard (câmpurile din dialoguri: Shift-Ins, Ctrl-V, Ctrl-Ins, Ctrl-C). `tmux paste-buffer -p` = bracketed paste din terminalul exterior.
- Teste de regresie folosite: OSC 52 (simplu, 150 KB, query ignorat, invalid), Shift-Ins în shell, lipire nativă far2l cu diacritice,
  bloc vertical far2l, bracketed paste în bash, profiluri (poziții, Ctrl+C → shell), salvare/reîncărcare profil, quit cu editor nesalvat,
  dialogul de ieșire (comenzi din profil, pipeline, comandă tastată, script, far2l, Go to, Esc, ecran 80x24).
- Note pentru testele cu tmux (28): într-un shell din tvterm, `tmux send-keys Enter` poate să nu ruleze comanda → se trimite `C-j`.
  `tmux send-keys` nu poate trimite evenimente de mouse către tvterm; secvențele SGR (`ESC[<0;x;yM` / `...m`, rotița `64`/`65`)
  scrise în tty-ul panoului cu `ioctl(TIOCSTI)` merg (`#{pane_tty}`; x/y de la 1). Testul pentru cursa istoricului: un script care
  scrie rafale de linii cu mici pauze, apoi un marcaj; la final marcajul trebuie să fie vizibil (build-ul de bază s-a blocat 7/7).
- Teste istoric (feat E1): Alt-Shift-PgUp/PgDn `ESC[5;4~`/`ESC[6;4~`, Alt-Shift-Home/End `ESC[1;4H`/`ESC[1;4F`, F2 `ESC OQ`,
  F3 `ESC OR`, Shift-F3 `ESC[1;2R`, F7 `ESC[18~`; mouse în format SGR `ESC[<b;x;yM/m` (dublu/triplu click = apăsare/eliberare
  repetată în 0,1 s; tragere = butonul 32). Potrivirea apare în pyte cu fundal `brightbrown`.
  - tvision ignoră bitul Shift din rapoartele de mouse xterm, deci Shift+click nu se poate trimite din test: s-a testat cu un
    build temporar care acceptă Alt în loc (necomis). În lanțul real ajunge prin extensiile far2l.
  - `/bin/sh` (dash) nu are editare de linie: tastele trimise greșit la shell rămân în linia de comandă → întâi Ctrl+C.
  - Măsurat: căutare în 10000 de linii de 85 de caractere: 0,1 s găsit aproape, 0,9 s negăsit; RSS ≈ 75 MB cu istoricul plin.
- Teste About (feat E2): `--version`/`-V`; Ctrl-B `m` `b` deschide dialogul; conținutul fără setări/profil și cu un profil
  de 3 terminale (unul deconectat); uptime după 1 minut; cale lungă scurtată pe un ecran de 60 de coloane; Enter/Esc îl închid;
  primele note din Event Viewer; `TZ=Europe/Bucharest` → `EET`; `-dirty`; build fără git → `unknown`;
  recompilare în același minut nu compilează nimic, în minutul următor doar `buildinfo.cc`.
- Teste Event Viewer (feat E3): titlul și bara de stare implicite; X; mișcările de mouse ascunse, apoi afișate după M; tastele
  proprii neînregistrate; K, C, T (note noi de la o reîncărcare automată a setărilor); N (și cu T oprit); ~120 de evenimente
  de taste (> 32000 de octeți) păstrează primele note, ~300 le elimină pe cele mai vechi; titlul la pauză; cheile din profil,
  salvarea (comentariu + cinci chei) și reîncărcarea; valoare greșită și chei de filtru în [terminal] raportate; literele tastate
  într-un terminal nu ajung la Event Viewer. Testul trebuie să dea click în Event Viewer: un click în altă parte mută focusul
  (iar un terminal maximizat îl acoperă).
- Teste alerte (feat E4): activitate, cuvânt cheie, cuvânt tăiat între scrieri, cuvânt doar în titlu OSC, liniște și
  re-armare, liniște de la pornire pentru un terminal care nu scrie nimic (și din profil), ieșirea din primele secunde
  repornește numărarea liniștii, Ctrl-B G/K, tile fără alarme false, profil salvat, un mesaj temporar ascunde contorul
  de alerte, CPU cu un terminal inundat + test unitar pentru OutputTextFilter.
- Teste LogView (feat E5), cu tmux (`tmux send-keys`, `capture-pane -p`, `-e` pentru culori) și linii FortiGate generate:
  două ferestre pe același `tail -F` → un singur proces; include cu spațiu final (între ghilimele) + exclude = `grep -E | grep -Ev`
  (60/200, 10047/50000; fără diferență de majuscule 9996/50000); linii noi; rotația logului (mesajele lui `tail -F`, apoi
  fișierul nou); Space (`[PAUSE +N]`), F (`[F-]`), PgUp + linii noi (`[+N]`), End, W, X; căutare cu evidențiere (fundal `103`)
  și „No older match”; Shift+Sus → Ctrl-E (editor nou), Ctrl-A (listă, adăugare; fișier salvat: 6 linii cu LF); salvare
  (implicit selecția); culori/`\r`/tab eliminate sau extinse; cod de ieșire; repornire automată după 2 s; alertă cuvânt cheie,
  contor, Ctrl-B G, Ctrl-B K; dialogul Ctrl-B D; D duplicat (proces comun); Save Profile As (ghilimele păstrate) și reîncărcare;
  dialogul de ieșire listează LogView-urile și procesele se opresc; regex greșit în profil; `[logview] max-lines` din tvterm.ini
  (și o valoare greșită); 3 × 50000 de linii (~38 MB) încărcate în < 1 s, RSS ≈ 59 MB.
  După primul test al utilizatorului (E5 refăcut): click-uri de mouse trimise ca secvențe SGR (`printf '\033[<0;X;Ym'`,
  shell-ul testului e `sh`); culorile citite celulă cu celulă din `capture-pane -e` (fundalul cursorului, 46, colorează și rama
  ferestrei); cursor pe ultima linie și urmărit, Sus/Jos (vedere fixă, `[+N]` = doar liniile noi; înainte le număra pe toate),
  End; Shift+Sus ×2 → 3 linii copiate, Sus → 1 linie (a cursorului); Home, PgDn, Shift-End; click pe o linie + Sus; linie ruptă
  (3 rânduri cu cursor); cursorul păstrat la filtre pornite/oprite/schimbate (linie ascunsă → cea dinainte); click-uri în bara
  de stare: F9, F4, I, F, Space, /, Ctrl-C, F2; un click pe un mesaj temporar doar îl închide; F4: Apply (dialog deschis,
  `[I] 10/300`), Apply cu `(` (eroare, dialog deschis), Cancel după Apply (filtrul aplicat rămâne), OK cu `(` (eroare, deschis),
  OK (`[I] 111/300`). Build dinamic și static.
- Teste bară de stare cu click (E6): Event Viewer — click pe M, C (titlu `[M+ C- …]`, și înapoi), X (golit), N (notele din nou);
  History — `Esc Exit` iese din mod (fără `(History)` în titlu), `/ Search` deschide dialogul de căutare.
- Teste filtre on/off (E7): dialogul nou arată `[X] Filters on`; debifat + Apply → `[F-] 300/300`, dialogul rămâne deschis;
  Cancel o păstrează; F → `[I] 100/300`, iar F4 arată din nou `[X]`; D copiază `[F-]`; Save Profile As scrie `filters = no`;
  profil cu `filters = no` → `[F-]`, fără cheie → filtre active, `filters = maybe` → „invalid filters 'maybe'”, filtre active.
- Teste element Window List (E8): click pe el în barele de terminal, LogView, Event Viewer, editor, History, selecție și
  Grab Input deschide lista (Grab Input: `(Input Grab)` dispare din titlu); lățime 60 de coloane: fără blocare.
- Teste colorare (E14): un test C++ legat cu `highlight.cc` (35 de verificări: culorile seturilor incluse pe linii de probă,
  câștigă prima regulă, set înlocuit, avertismente cu fișier și linie, ciclu de include, `\"`, `icase`, potriviri goale, o
  linie de 100 KB); un program C compilat cu gcc și cu `musl-gcc -static` (pachetul `musl-tools`) afișează toate potrivirile
  și grupurile celor 71 de expresii incluse pe 17 linii de probă: ieșire identică. În tmux, culorile citite celulă cu celulă
  din `capture-pane -e` (**atributele continuă pe linia următoare**: parserul le păstrează, altfel un rând care începe doar
  cu o schimbare de culoare a textului pare să aibă fundalul implicit): culorile la pornire, linii noi, H și `[H-]` (doar
  fereastra respectivă), cursorul și selecția fără culori, potrivirea căutării peste ele, `tvterm.ini` schimbat (culoare
  nouă în ~3 s, regulă greșită raportată cu linia), set necunoscut raportat o singură dată, F4 (set necunoscut: eroare cu
  lista; seturi valide aplicate; seturi noi pornesc H), D, Save Profile As (`highlight`, `highlight-on = no`) și
  reîncărcare, linie ruptă cu fundal (fără culoare după sfârșitul liniei). 2 ferestre × 50000 de linii FortiGate
  (`fortigate`, una cu linii rupte) + 2000 de linii/s timp de 10 s: 0,46 s de CPU. Build dinamic și static.
- Test `syslog` (E15): un test C++ legat cu `highlight.cc` (38 de verificări; 14 dintre ele pică pe E14) pe linii cu
  `T`, `^`, spațiu, `.123`, `+02:00`, `Z` și `Oct 29` (ora estompată până la spațiu, numele programului cu `hash`), plus
  o linie fără spațiu după oră (neestompată); expresia nouă dă aceleași grupuri pe glibc și musl; în tmux, un LogView cu
  `syslog` pe un fișier cu toate formele.
- Teste liste în câmpuri (E16), în tmux cu `LANG=C.UTF-8` (fără el, tvision afișează UTF-8 tastat prin tmux ca `�`, și
  în câmpuri, și în lista de la Command): săgeți lângă cele patru câmpuri; listă goală `(nothing used yet)`, Enter lasă
  câmpul neschimbat; 22 de valori Exclude aplicate → `v22` prima, `v03` ultima; o valoare goală aplicată nu se
  înregistrează; o valoare aplicată din nou urcă în capul listei; seturile în ordine; `kv` apoi `syslog` adăugate →
  `kv syslog`, `kv` din nou → neschimbat; Enter pe `── used before ──` → neschimbat; o valoare din istoric înlocuiește
  câmpul; a doua fereastră vede aceleași liste; un Include de 310 octeți luat din listă și confirmat cu OK → fără eroare,
  câmpul se termină ca originalul; valori UTF-8 (`ășț─x`) afișate corect; click cu mouse-ul (SGR prin `TIOCSTI`) pe
  săgeți deschide listele. Build dinamic și static.

---

## 6. Limitări cunoscute

- Marcajul de bloc vertical nu trece prin Windows (doar între far2l-uri din același tvterm).
- `ClientOptions` și info procese: neimplementate pe Windows.
- Win32-input-mode: scan code 0; key-up trimis imediat după key-down; unele taste (simboluri cu Shift, cifrele de pe tastatura
  numerică) au `vk = 0`.
- Starea „pauză” a Event Viewer-ului nu se salvează în profil. Un Event Viewer deschis cu Ctrl-B V pornește cu filtrele
  implicite (ultimele nu sunt reținute; profilurile le păstrează).
- Expresiile regulate fără diferență între litere mari/mici tratează doar literele ASCII (căutarea de text simplu tratează și
  literele latine, românești, grecești, chirilice).
- Căutarea citește tot istoricul la fiecare pas (merge până la limita de 100000 de linii, dar nu instantaneu).
- LogView: bara de derulare numără linii, nu rânduri rupte; căutarea găsește prima potrivire din fiecare linie (F3 trece la
  linia următoare); repornirea unei surse cu `tail -n N` arată din nou acele N linii; regex-urile din căutarea în istoric
  (ECMAScript) și din LogView (POSIX, ca `grep -E`) diferă în detalii (ex. `\d` doar în ECMAScript).
- tvision (pentru un PR la magiblot): `histlist.cpp` memcpy → memmove (corectat local, commit 22); `TFileList::getText` adaugă `\` după ce copiază până la 255 de caractere într-un buffer de 256
  → un octet în plus la un director cu numele de exact 255 de octeți; `hotKeyStr` (tinputli.cpp) primește pointer nul de la separatoarele meniului.

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
| Istoric (`far2l-extended`) | E1 | modul istoric (derulare, căutare, salvare), dublu/triplu click, Shift+click, tvterm.ini cu profil implicit și reîncărcare automată |
| Versiune (`far2l-extended`) | E2 | versiunea 0.1.0 cu commit/branch/dată generate la fiecare build; `--version`; dialogul About; primele note în Event Viewer |
| Filtre Event Viewer (`far2l-extended`) | E3 | filtrele M C K O T, N (mesajele din nou), X (golire), buffer de 64000 de octeți, filtre în profiluri |
| Alerte (`far2l-extended`) | E4 | alerte de activitate / cuvânt cheie / liniște (liniștea numărată de la pornire), marcaj în titlu, contor în bara de jos, Ctrl-B K și G, chei în profil |
| LogView (`far2l-extended`) | E5 | Ctrl-B D: ieșirea filtrată a unei comenzi, surse partajate, include/exclude, pauză, linii rupte, căutare, cursor de linie, selecție în clipboard/editor, salvare, meniul F9, bară de stare cu click, setări cu Apply, alerte, profiluri |
| Bară de stare cu click (`far2l-extended`) | E6 | Event Viewer (un element pentru fiecare tastă) și modul istoric: click pe un element = tasta lui |
| Filtre LogView în F4 (`far2l-extended`) | E7 | căsuța „Filters on” în F4 (sincronizată cu F), cheia `filters` în `[logview]`, copiată de D |
| Window List în bara de stare (`far2l-extended`) | E8 | element `Window List` cu click după `Ctrl-B Open Menu` în toate barele, în afară de mutare/redimensionare și meniu |
| Note pentru pozițiile din profil (`far2l-extended`) | E9 | o poziție mutată în interiorul Desktop-ului la încărcarea unui profil apare în Event Viewer |
| Corecturi după testul 23–25 (`far2l-extended`) | E10 | `Ctrl-B Menu` și `WinList` în barele de stare; Alerts `K~e~ywords`, dialogurile de salvare `Se~l~ection only`; notă în Event Viewer când Tile nu are loc |
| Numele fișierului salvat (`far2l-extended`) | E11 | „File name: Date first / Name first” în dialogurile de salvare (istoric, ecran, Log View), `save-name` în `tvterm.ini` |
| Manual (`far2l-extended`) | E12 | `docs/MANUAL.md` + `docs/MANUAL-ro.md`: manualul de utilizare al comportamentului curent, cu exemple complete |
| Lista alertelor (`far2l-extended`) | E13 | tasta A în Event Viewer (alertele active cu ora, grupul, linia); `hh:mm:ss` / `YYYY-MM-DD hh:mm:ss` în mesaje; cuvinte cheie cu grupuri `+`, excluderi `!`, `\` |
| Colorare (`far2l-extended`) | E14 | culori în LogView: seturi `[highlight <nume>]` în `tvterm.ini` (line, match, groups, hash, include), 10 seturi incluse (conversii multitail + zimbra-mta, nginx, syslog), câmp în F4, H, chei de profil |
| Formele orei în `syslog` (`far2l-extended`) | E15 | setul `syslog` estompează ora ISO cu `T`, `^` sau spațiu (cu fracțiuni și fus orar) |
| Liste în câmpurile F4 (`far2l-extended`) | E16 | ↓ lângă Include, Exclude (valorile folosite) și Highlight sets (seturi + valorile folosite) |
| Tastele meniului (`far2l-extended`) | E17 | Ctrl-B W = Window List (înainte J); Close Term fără tastă |
