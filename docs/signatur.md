# Signierte Firmware und die zwei Fassungen

Status: Entwurf. **Nichts davon ist auf Hardware gelaufen, und der ARM-Bau
mit MCUboot hat nie stattgefunden** - auf diesem Rechner ist kein nRF Connect
SDK. Was gemessen ist und was nicht, steht am Ende unter
"Was geprueft ist und was nicht". Diese Seite beschreibt den Weg; der Ablauf
des Updates selbst steht in `docs/update.md`, die Bauwege in `docs/build.md`.

Zwei Fragen beantwortet diese Seite:

1. Wie wird **untergeschobene Firmware unmoeglich**?
2. Wie werden die **zwei Fassungen** (mit und ohne RSA) auseinandergehalten?

## Warum der signierte Bootloader frueh da sein muss

Das ist kein Zeitplan, sondern eine Einbahnstrasse.

Die Auslesesperre des Chips (APPROTECT) ist das, was den Bootloader davor
schuetzt, einfach ausgetauscht zu werden. Wird sie nicht gesetzt, laesst sich
der Bootloader ersetzen - und dann prueft niemand mehr etwas, egal wie gut der
Rest ist. Wird sie gesetzt, **loescht jedes Loesen der Sperre den Chip**.

Daraus folgt: wer den signierten Bootloader nachruesten will, nachdem die
Sperre sitzt, zahlt es mit einem geloeschten Geraet. Auf einem Geraet im Feld
heisst das: mit allen Passwoertern darin. Darum gehoert der signierte
Bootloader in das **erste** Abbild, das auf ein Geraet mit gesetzter Sperre
kommt, und nicht in das zweite.

## Zwei Schluessel, und sie sind nicht derselbe

| Schluessel | Wer prueft damit | Wann | Wo er liegt |
| --- | --- | --- | --- |
| Paket-Schluessel | unsere Schicht (`boot_package.c`) | vor dem Schreiben | ausserhalb des Projekts, `OSK_SIGN_KEY` |
| MCUboot-Schluessel | MCUboot | vor dem Umschalten | ausserhalb des Projekts, `OSK_MCUBOOT_KEY` |

Der erste weist ein Paket ab, bevor ein Byte in den Flash geht. Der zweite
entscheidet, ob ein geschriebenes Abbild ueberhaupt startet. Beide braucht es:
der erste spart das Schreiben, der zweite ist der, der nicht zu umgehen ist.

**Fehlt der MCUboot-Schluessel beim Bauen, nimmt MCUboot seinen EIGENEN
Beispielschluessel - und der ist oeffentlich bekannt.** Ein Geraet, das damit
gebaut ist, nimmt Firmware von jedem an, der die Datei aus dem Netz laedt. Es
gibt dafuer keine Warnung: der Bau wird gruen. Das ist kein kleiner Fehler,
sondern das Ende der ganzen Absicherung.

Darum bricht der Bau ab, und zwar an **zwei** Stellen:

- `tools/build.py` prueft, bevor `west` ueberhaupt startet
  (`build.mcuboot_key()`),
- `apps/firmware/CMakeLists.txt` prueft noch einmal, fuer den Fall, dass
  jemand von Hand baut.

Zwei Waende, weil ein Bau ohne Schluessel schlimmer ist als kein Bau.

### So wird gebaut

```powershell
$env:OSK_MCUBOOT_KEY = "$env:USERPROFILE\.osk-keys\mcuboot-ed25519.pem"
```

```powershell
python tools/build.py firmware --variant norsa
```

```powershell
python tools/build.py firmware --variant rsa
```

Der Pfad des Schluessels steht in **keiner** Befehlszeile und in keiner
Bauprotokollzeile: Bauprotokolle wandern in Fehlerberichte, und der Pfad
verraet, wo der Schluessel liegt. Er geht nur als Umgebungsvariable mit.

## Der Schluessel kommt nie ins Repo - drei Schichten

Ein Schluessel, der einmal in einem Commit steckt, ist aus der Geschichte
nicht mehr herauszuholen. Darum liegen drei Schichten uebereinander, und jede
faengt, was die vorige durchlaesst:

| Schicht | Wo | Was sie faengt | Was sie NICHT faengt |
| --- | --- | --- | --- |
| 1 | `.gitignore` | bekannte Namen (`*.key`, `*.pem`, `id_ed25519`, ...) | jeden anderen Namen; `git add -f` |
| 2 | `tools/sign.py`, `tools/build.py`, `CMakeLists.txt` | einen Schluessel IM Projektordner benutzen | eine Datei, die nur dort liegt |
| 3 | `tools/keyguard.py` | was WIRKLICH im Ordner liegt - nach INHALT | einen Schluessel in einem Format, das es nicht kennt |

Die dritte Schicht ist die, die zaehlt. Sie sieht nach dem Inhalt:

- der Inhalt ist **genau** ein 32-Byte-Seed in Hex (das Format von
  `tools/sign.py --gen-key`),
- die Datei **beginnt** mit einem PEM-, OpenSSH- oder PGP-Kopf eines privaten
  Schluessels,
- oder der Name sieht nach Schluessel aus.

Sie laeuft in `tools/check.py` als **erster** Schritt - sie braucht kein
Werkzeug und laeuft darum auf jedem Rechner, auch auf dem ohne SDK. Ein
Schluessel im Projektordner ist der einzige Fehler hier, der sich nicht mehr
zurueckdrehen laesst.

```powershell
python tools/keyguard.py
```

**Ignoriert ist nicht weg.** Eine Datei, die `.gitignore` kennt, liegt
trotzdem im Ordner: sie wandert in jede Sicherung, in jedes ZIP und in jedes
`git add -f`. Darum sagt `keyguard.py` "verschieben", nicht "eintragen".

### Was beim Schaerfen der Pruefung herauskam

Der erste Entwurf von `keyguard.py` schlug am 30.10.2026 auf **neun eigene
Quelldateien** an: auf drei Go-Pruefungen und eine C-Pruefung, weil dort ein
PEM-Kopf als Textbaustein in einer Zeichenkette steht, auf vier Quelldateien
wie `osk_ctap_key.c`, weil ein Namensmuster zu weit gefasst war - und auf
`keyguard.py` selbst. Eine Pruefung, die immer rot ist, sieht nach einer Woche
niemand mehr an, und damit ist sie schlechter als keine. Daraus wurden zwei
Regeln:

- Ein PEM- oder OpenSSH-Kopf gilt nur am **Anfang** der Datei. Eine echte
  Schluesseldatei beginnt damit; eine Quelldatei, die ihn erwaehnt, hat ihn
  mitten im Text.
- Namen von Quelldateien (`.c`, `.h`, `.go`, `.py`, ...) werden gar nicht nach
  Schluesselmustern beurteilt. Ihr **Inhalt** wird trotzdem gelesen.

## Die zwei Fassungen: mit und ohne RSA

Zwei Fassungen, eine Kconfig-Option: `CONFIG_OSK_RSA`
(`apps/firmware/Kconfig`). Sie wirkt an genau einer Stelle im Quelltext -
`osk_rsa_available()` antwortet ohne sie immer `false`, und danach biegt jede
Anwendung von selbst auf den Weg ohne RSA ab
(`src/classes/applets/osk_rsa_card.h` sagt, warum).

**Bis zum 30.10.2026 war die Option nirgends deklariert.** Der Quelltext
fragte `CONFIG_OSK_RSA` an sechs Stellen ab, aber Kconfig kannte den Namen
nicht. Ein `CONFIG_OSK_RSA=y` in einer `.conf`-Datei haette Zephyr **still
verworfen** - nur eine Warnung, kein Fehler, genau wie die 21 verworfenen
Zeilen, die in `apps/firmware/prj.conf` beschrieben sind. Jeder Bau waere die
Fassung ohne RSA geworden, und die Fassung mit RSA war damit nicht gebaut,
sondern nur geschrieben. Die Datei `apps/firmware/Kconfig` ist der Grund, dass
die Option ueberhaupt eine ist.

Beide Fassungen haben ihre eigene Baudatei, und **beide sagen ausdruecklich,
was sie meinen**:

| Datei | Inhalt |
| --- | --- |
| `apps/firmware/variant-rsa.conf` | `CONFIG_OSK_RSA=y` |
| `apps/firmware/variant-norsa.conf` | `CONFIG_OSK_RSA=n` |

Das `=n` steht da, obwohl `n` auch die Vorgabe ist. Eine Fassung, die nur
durch das **Weglassen** einer Zeile entsteht, ist von einem Versehen nicht zu
unterscheiden.

Jede Fassung baut in ihren **eigenen** Bauordner. Zwei Fassungen in einem
Ordner heisst: der zweite Bau laeuft auf den Resten des ersten, und welche
Fassung im Abbild steckt, haengt daran, was `--pristine` gerade raeumt.

### Zwei AAGUIDs, und warum

FIDO verlangt, dass ein AAGUID eine Fassung mit **denselben Faehigkeiten**
bezeichnet. Die Fassung mit RSA kann mehr als die ohne - also braucht sie
einen anderen AAGUID. Die beiden Werte stehen an **einer** Stelle:
`apps/firmware/src/classes/ctap/osk_ctap_core.c`. Dort liest sie der
Quelltext, und dort liest sie `tools/variant.py`. Wer sie an einer zweiten
Stelle hinschreibt, hat zwei Wahrheiten und irgendwann zwei verschiedene.

Beide Werte sind **vorlaeufig** und werden mit der ersten Serie festgelegt.
Sie unterscheiden sich im letzten Byte (`...0001` mit RSA, `...0002` ohne),
damit beim Nachsehen im Protokoll sofort zu erkennen ist, welche Fassung
geantwortet hat.

### Die Fassung steht an drei Stellen - und sie muessen uebereinstimmen

| Nr. | Wo | Woraus |
| --- | --- | --- |
| 1 | der **Dateiname** | `osk-firmware-<Fassung>-<Brett>-rsa.bin` bzw. `-norsa.bin` |
| 2 | der **Abbildkopf** | der AAGUID als **geschuetztes** TLV `0x00a1` im MCUboot-Kopf |
| 3 | die **`.config`** des Baus | `CONFIG_OSK_RSA`, wie Kconfig sie wirklich geschrieben hat |

**Der gefaehrlichste Fall ist ein Abbild ohne RSA mit dem AAGUID der Fassung
mit RSA.** Dann verspricht das Geraet dem Browser Faehigkeiten, die es nicht
hat, und das faellt erst beim Nutzer auf - als Anmeldung, die scheitert, oder
als Schluessel, der nie entsteht. Genau dagegen vergleicht `tools/variant.py`
die drei Stellen:

```powershell
python tools/variant.py --check dist/osk-firmware-0.0.1.0-nrf52840dk-norsa.bin --expect norsa
```

`tools/build.py` ruft das gleich nach dem Bau selbst auf. Stimmt es nicht, ist
der Bauschritt **FEHLER** und nicht "ok mit Warnung".

Drei Dinge daran sind Absicht:

- **Der Dateiname allein genuegt nicht.** Ein Dateiname ist umbenannt, bevor
  man zweimal hinsieht. Darum muss die Fassung auch im Abbild stehen.
- **Der Abbildkopf allein genuegt auch nicht.** Fehlt die Fassung im
  Dateinamen, lehnt `variant.py` ab: dann gibt es nichts zu vergleichen, und
  eine einzige Quelle stimmt immer mit sich selbst ueberein. (Ohne diesen
  Riegel ging ein Abbild namens `zephyr.bin` durch - gemessen am 30.10.2026,
  gefunden von einem eigenen Test.)
- **Der AAGUID gehoert in den GESCHUETZTEN TLV-Bereich** (`0x6908`), nicht in
  den ungeschuetzten (`0x6907`). Geschuetzt heisst: von der Signatur gedeckt.
  Im ungeschuetzten Bereich waere er austauschbar, ohne die Signatur zu
  brechen - und ein austauschbarer AAGUID ist keiner. `variant.py` lehnt ein
  Abbild ab, dessen AAGUID im ungeschuetzten Bereich steht.

Geschrieben wird das TLV von `imgtool` ueber
`CONFIG_MCUBOOT_EXTRA_IMGTOOL_ARGS` (`--custom-tlv 0x00a1 <AAGUID>`), gesetzt
in `apps/firmware/CMakeLists.txt`. **Ungeprueft:** ob `imgtool` in der
gebauten SDK-Fassung diesen Schalter so annimmt, sagt der erste echte Bau.

### Gelesen wird die `.config`, nicht die `.conf`

`.conf`-Dateien sagen, was **gewuenscht** war. Die `.config` sagt, was
**herauskam**. Eine Zeile, die Kconfig still verworfen hat, steht in der
`.conf` und nicht in der `.config`. Darum liest `build.py` nach dem Bau die
`.config` und vergleicht sie mit der gewuenschten Fassung - dieselbe Vorsicht
wie bei der Versionsnummer, und aus demselben Grund: `west` meldet Erfolg.

Drei Faelle werden unterschieden, nicht zwei: `y`, `n`, und **"steht nicht
drin"**. Der dritte ist nicht dasselbe wie `n` - wer die Zeile vergisst,
bekommt still die Vorgabe, und dann weiss niemand, ob die Fassung gewollt oder
gestolpert ist.

## Reproduzierbarer Bau

Wer nicht nachbauen kann, muss glauben. Ein reproduzierbarer Bau ist die
einzige Moeglichkeit, ein veroeffentlichtes Abbild gegen den Quelltext zu
pruefen, **ohne dem zu vertrauen, der es gebaut hat** - und bei einem
Sicherheitsschluessel ist genau das der Punkt.

Gemessen wird, nicht angenommen: zweimal bauen, Hashwerte vergleichen.

```powershell
python tools/repro.py --pack
```

```powershell
python tools/repro.py --firmware --variant norsa
```

Gerechnet wird mit `git hash-object`, nicht mit einer eigenen Hashrechnung:
das ist dieselbe Zahl, die git in jedem Commit fuehrt, und sie laesst sich von
Hand nachrechnen.

Zwei Dinge, die in diesem Projekt schon einmal schiefgingen und auf die darum
geachtet wird:

- Ein Modul-ZIP trug die **Bauzeit** statt eines festen Zeitstempels. Zwei
  Baeufe im Abstand einer Sekunde waren verschieden, und niemand konnte sagen,
  ob sich der Inhalt geaendert hatte.
- Der **Bauordner** steht im Abbild: Uebersetzer schreiben Pfade hinein.
  Darum baut `repro.py --firmware` zweimal in Ordnern mit verschieden
  **langen** Namen. Ein Bau, der nur im selben Ordner reproduzierbar ist, ist
  es nicht - und gleich lange Pfade wuerden den Unterschied verdecken.

Scheitert der Vergleich, nennt `repro.py` die **erste** abweichende Stelle und
was dort steht. Das ist meistens der ganze Befund: ein Zeitstempel sieht wie
ein Zeitstempel aus, ein Pfad wie ein Pfad.

## Was geprueft ist und was nicht

Gemessen am 30.10.2026, alles am PC. **Es gibt kein nRF Connect SDK auf diesem
Rechner und keine Hardware.**

| Was | Wie | Ergebnis |
| --- | --- | --- |
| Schluesselschutz schlaegt an | zwei echte Schluesseldateien angelegt: eine `*.key`, eine namens `notizen.txt` | beide gefunden; danach geloescht |
| `.gitignore` allein genuegt nicht | `git check-ignore` auf beide Dateien | `*.key` gefangen, `notizen.txt` **nicht** - die Schicht darunter fing sie |
| `sign.py` nimmt keinen Schluessel aus dem Projektordner | ausdruecklich mit `--key` darauf gezeigt | abgebrochen, Code 1 |
| Bau ohne Schluessel | `OSK_MCUBOOT_KEY` nicht gesetzt | **FEHLER**, Code 1 - nicht "uebersprungen" |
| Bau mit Schluessel im Projektordner | Pfad im Repo angegeben | FEHLER, Code 1 |
| Bau mit Schluessel, der nicht da ist | Pfad auf eine fehlende Datei | FEHLER, Code 1 |
| Gegenprobe: Schluessel aussen und vorhanden | Pfad ausserhalb des Projekts | kommt durch, bleibt am fehlenden SDK haengen |
| Der Schluesselpfad steht in keiner Meldung | Pfad mit auffaelligem Ordnernamen, Meldungen und Befehlszeile durchsucht | kommt nicht vor |
| Abbruch beim Aufspielen | `apps/firmware/test/boot/`, ueber WSL gebaut und gelaufen | **793 Pruefungen, 0 Fehler, Ausstiegscode 0** |
| davon Abbruch mitten im Schreiben | eigene Faelle im Pruefsatz oben | nach jedem Abbruch startet die alte Fassung |
| AAGUID-Verwechslung schlaegt an | echte Datei: `-norsa` im Namen, RSA-AAGUID im Kopf | abgelehnt, Code 1, mit dem Grund im Klartext |
| die andere Richtung auch | `-rsa` im Namen, norsa-AAGUID im Kopf | abgelehnt |
| Gegenprobe: beide Fassungen richtig | je ein Abbild mit passendem AAGUID | beide kommen durch |
| fremder AAGUID | Abbild mit 16 Nullbytes | abgelehnt |
| AAGUID im ungeschuetzten TLV-Bereich | Abbild mit `0x6907` statt `0x6908` | abgelehnt |
| Abbild ohne Fassung im Dateinamen | `zephyr.bin` mit gueltigem AAGUID | abgelehnt (dieser Riegel fehlte zuerst) |
| Reproduzierbarkeit des Pakets | zweimal verpacken, `git hash-object` | gleich |
| Reproduzierbarkeit des Abbildkopfs | zweimal bauen, beide Fassungen | gleich |
| Werkzeuge | `tools/tests/`, darin `test_signatur.py` mit 36 Faellen | 155 Tests, 0 Fehler |
| Gesamtlauf | `python tools/check.py` | 17 Schritte ok, 3 uebersprungen, 0 Fehler, 437 Tests |

### Nicht geprueft, weil es dafuer kein Mittel gab

Das ist der Abschnitt, der zaehlt.

- **Kein ARM-Bau. Es ist kein Abbild entstanden.** Das nRF Connect SDK fehlt
  auf diesem Rechner, darum ueberspringt `tools/check.py` die Schritte
  `firmware/build`, `firmware/tests` und `firmware/format`. Keine der beiden
  Fassungen ist je uebersetzt worden. Was hier "beide Fassungen geprueft"
  heisst, bedeutet: die Weiche, die Baudateien, die Tafel der AAGUIDs und
  jede Pruefung darum sind gemessen - **nicht das Abbild**.
- **Die Reproduzierbarkeit der FIRMWARE ist nicht gemessen.** Gemessen ist
  das Verpacken und der Abbildkopf. Zwei Abbilder, von denen keines gebaut
  wurde, sind nicht gleich - sie sind nicht da. `tools/repro.py` sagt das
  selbst und endet mit Code 2, nicht 0.
- **MCUboot lief nie mit.** Alle Einstellungen in `boot/mcuboot.conf` und der
  ganze Block in `apps/firmware/CMakeLists.txt` sind nach der Dokumentation
  des SDK geschrieben, nicht nach einer Messung. Ob die Namen
  (`mcuboot_EXTRA_CONF_FILE`, `CONFIG_MCUBOOT_EXTRA_IMGTOOL_ARGS`,
  `PM_STATIC_YML_FILE`) in der gebauten SDK-Fassung so heissen und so wirken,
  sagt der erste Bau. Darum stehen fuer die Einstellungsdatei absichtlich
  **beide** Schreibweisen da.
- **Der Abbruch des CMake-Laufs ohne Schluessel ist nicht gemessen.**
  Gemessen ist der Abbruch in `tools/build.py` (die erste Wand). Die zweite
  Wand in `CMakeLists.txt` ist geschrieben und nie ausgefuehrt - es gibt
  keinen CMake auf diesem Rechner.
- **Das AAGUID-TLV ist von `imgtool` nie geschrieben worden.** Die Abbilder,
  an denen die Pruefung gemessen ist, hat `variant.build_image()` gebaut - ein
  Stueck, das ausdruecklich kein Ersatz fuer `imgtool` ist und nichts
  unterschreibt. Dass `variant.py` einen echten, von `imgtool` gebauten Kopf
  genauso liest, ist **nicht** gemessen.
- **Das Umschalten selbst ist nicht gemessen.** Das macht MCUboot.
- **Kein QSPI-Baustein, kein SE050, keine Taste.** In den Abbruch-Pruefungen
  sind Flash, Signaturpruefer, Hash und Zaehler Attrappen (`docs/update.md`
  sagt das genauer).
- **`keyguard.py` kennt nur die Formate, die es kennt.** Ein Schluessel in
  einem anderen Format - roh binaer, in einem ZIP, in einer Datei ueber
  64 kB - faellt durch. Die Schicht darunter (`sign.py`, `build.py`) faengt
  ihn erst, wenn jemand ihn benutzt.

### Die vier Stufen

Dieses Projekt unterscheidet: **geschrieben / uebersetzt / im Abbild / am
Geraet gelaufen.**

- Der Signaturweg in `tools/` steht auf **gelaufen am PC**.
- Die Abbruchsicherheit des Updates steht auf **gelaufen am PC** (793
  Pruefungen).
- Der Bau-Block in `apps/firmware/CMakeLists.txt`, `boot/mcuboot.conf` und
  die beiden `variant-*.conf` stehen auf **geschrieben**.
- Das Abbild und das Geraet: **nichts davon**.

## Was noch fehlt

- Der erste ARM-Bau beider Fassungen. Erst er sagt, ob die
  Einstellungsnamen stimmen, ob `imgtool` das TLV schreibt und ob zwei Baeufe
  dasselbe Abbild ergeben.
- Der MCUboot-Schluessel selbst. Er wird mit `imgtool keygen` erzeugt und
  gehoert nach `%USERPROFILE%\.osk-keys\` - nie ins Repo, und nie in eine
  Sicherung, die weitergegeben wird.
- Die endgueltigen AAGUIDs. Beide Werte sind vorlaeufig und werden mit der
  ersten Serie festgelegt (`docs/plan.md`, offener Punkt 10).
- Zwei Pruefläufe und zwei Signaturen je Veroeffentlichung - `tools/release.py`
  kennt die Fassungen noch nicht.
- Der Geraetebaum des eigenen Bretts: welcher Anschluss die Taste ist.
