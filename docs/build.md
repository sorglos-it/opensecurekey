# Bauen

Alle Befehle laufen im Projektordner. Python 3.9 oder neuer genügt für die Werkzeuge, sie brauchen keine Zusatzpakete.

## Was man braucht

| Teilprogramm | Werkzeug | Ohne dieses Werkzeug |
| --- | --- | --- |
| alle | Python 3 | nichts geht |
| `apps/desktop` | Go 1.21 oder neuer | wird übersprungen |
| `apps/web` | Node.js 20 oder neuer (nur zum Prüfen) | Bauen geht, Prüfen wird übersprungen |
| `apps/firmware` | nRF Connect SDK (Zephyr, `west`) | wird übersprungen |

Fehlt etwas, sagen die Skripte es klar und bauen nicht. „Übersprungen" heißt immer: **nicht gebaut, nicht geprüft.**

## Alles prüfen

```powershell
python tools/check.py
```

Prüft, was ohne Hardware geht: Versionsdateien, Werkzeug-Tests, Übersetzen und Tests je Teilprogramm, Sprachdateien (Schlüssel gleich in de, en, fr, it, es), Formatierung. Am Ende steht, wie viele Schritte liefen, was übersprungen wurde und wie viele Tests ausgeführt wurden. Der Ausstiegscode ist nur 0, wenn nichts scheiterte **und** mindestens ein Test lief.

## Bauen

```powershell
python tools/build.py desktop
```

```powershell
python tools/build.py web
```

```powershell
python tools/build.py firmware
```

```powershell
python tools/build.py all
```

Ergebnisse liegen in `dist/`: `osk-<Version>-<System>-<Architektur>[.exe]`, `osk-web-<Version>.zip`, `osk-firmware-<Version>-<Board>.hex`. Die Version kommt aus `apps/<typ>/VERSION` und wird beim Bauen eingestempelt:

- Firmware: CMake-Variable `OSK_VERSION` (die `CMakeLists.txt` der Firmware muss sie auswerten).
- Desktop: Go-Variable `main.version` (in `package main` als `var version = "dev"` anlegen).
- Web: `version` in der `manifest.json` im ZIP; die Datei im Quellbaum bleibt unberührt.

Ausstiegscodes von `build.py`: 0 gebaut, 1 Fehler, 2 nichts gebaut (alles übersprungen).

## Firmware: das nRF Connect SDK

Dieser Abschnitt beschreibt den vorgesehenen Weg. Was auf dem Rechner von
Thomas am 24./25.10.2026 **wirklich** getan werden musste, steht weiter
unten unter „Der erste Bau für ARM" — die beiden sind nicht gleich.

Zwei Wege, das Skript erkennt beide:

1. **Shell des SDKs.** Im Terminal, in dem `west` und `ZEPHYR_BASE` gesetzt sind (nRF Connect for Desktop → Toolchain Manager → „Open terminal"), dort `python tools/build.py firmware`.
2. **nrfutil.** Die SDK-Version nennen, das Skript startet `west` im Toolchain-Manager:

```powershell
$env:OSK_NCS = 'v3.0.0'
```

```powershell
python tools/build.py firmware
```

Anderes Board (Dongle):

```powershell
python tools/build.py firmware --board nrf52840dongle/nrf52840
```

## Der erste Bau für ARM (24./25.10.2026)

Bis dahin war die Firmware **nie** für den echten Chip übersetzt worden:
`tools/check.py` meldete `firmware/build` seit dem ersten Tag als
übersprungen. Hier steht, was dabei herauskam — die Zahlen, die Funde und
der Weg, der wirklich gegangen wurde.

### Das Ergebnis: es passt

Gemessen am 25.10.2026, Board `nrf52840dk/nrf52840`, frischer Bauordner.

| | belegt | Bereich | Anteil | gegen die 960 kB |
| --- | --- | --- | --- | --- |
| **FLASH** | **161.124 B** (157 kB) | 1 MB | 15,37 % | **16,4 %**, 802,7 kB Luft |
| **RAM** | **97.100 B** (95 kB) | 256 kB | 37,04 % | — |

Der Befehl, der die Zahlen erzeugt hat:

```powershell
west build --no-sysbuild -b nrf52840dk/nrf52840 -d <bauordner> apps/firmware --pristine=always -- -DOSK_VERSION=0.0.1.0 -DOSK_MESSBAU=y
```

Gegengeprüft mit einem zweiten Programm, weil eine Zahl aus einer Quelle
keine Messung ist:

```
arm-zephyr-eabi-size -B zephyr.elf
   text     data      bss
 160136   976   96082
```

`text + data` = 161.112 B. Der Binder meldet **161.124 B**, also
**12 Byte mehr** - und dieser Unterschied ist aufgeklaert, nicht ignoriert:

- 8 Byte Ausrichtungsluecke vor `rodata` (es beginnt bei 126608, der
  Abschnitt davor endet bei 126600)
- 4 Byte `tbss`, das im Flash-Bereich liegt, aber keinen Inhalt mitbringt

Nachgerechnet aus `size -A`: `rodata` endet bei 160148, dahinter liegen die
976 Byte des Datenabbilds - 160148 + 976 = **161124**, genau die Zahl des
Binders. `size` zaehlt nur Abschnittsgroessen, der Binder den belegten
Bereich samt Luecken. **Die Zahl des Binders ist die, die zaehlt**, denn sie
sagt, wieviel Flash wirklich belegt ist.

Am 24.10. stimmten beide noch aufs Byte überein (161.108 B): damals lagen
die Abschnitte zufällig lückenlos. Eine Gegenprobe, die einmal genau passt
und dann nicht mehr, ist kein Fehler der Messung - sie misst etwas leicht
anderes.

**Das Abbild enthält nicht allen eigenen Code.** Nachgesehen an den
Symbolen des ELF: von **535** eigenen Funktionen stecken nur **243** im
Abbild; **292** hat der Binder mit `--gc-sections` verworfen, weil
`src/main.c` sie nicht erreicht. In Byte:

| | Byte | |
| --- | --- | --- |
| eigener Code **im Abbild** | 30.758 B | 30 kB |
| eigener Code **insgesamt** (alle 80 Objektdateien) | 140.261 B | 137 kB |
| vom Binder verworfen | 109.503 B | 107 kB |

Daraus die ehrliche Hochrechnung: wäre **aller** eigene Code erreichbar,
läge das Abbild bei rund **270.600 B (264 kB)** — **27,5 % der 960 kB**,
also immer noch knapp 700 kB Luft. **Die Antwort „es passt“ bleibt, der
Abstand ist nur kleiner als die 16,4 % vermuten lassen.**

Das ist keine Schwäche der Messung, sondern ihre Grenze: ein Geräteabbild
enthält immer nur, was von `main()` aus erreichbar ist, und die Transporte,
die den großen Rest erreichen würden, sind noch nicht geschrieben. Wer die
Zahl später wieder misst, prüft **beide**: die Größe und wieviele der
eigenen Funktionen drin sind.

**Welcher Krypto-Unterbau drin ist, ist ebenfalls nachgesehen** und nicht
hergeleitet: `psa_generate_random` und `psa_import_key` stehen im Abbild,
`BCryptGenRandom` (Windows) und `RAND_bytes`/`EVP_CIPHER_CTX_new` (OpenSSL)
nicht. Die Krypto läuft also über PSA, wie gedacht — aber siehe die kaputte
Weiche weiter unten: sie hält nur, weil die `CMakeLists.txt`
`OSK_CRYPTO_PSA` setzt.

**Was die Zahl wert ist — und was nicht:**

- Sie ist eine **Untergrenze**. Sechs Namen gibt es auf dem Gerät noch
  nicht (siehe unten); für die Messung stehen in `apps/firmware/src/messbau.c`
  Platzhalter. Die echten Umsetzungen sind größer.
- Es fehlt, was noch nicht geschrieben ist: USB, BLE und NFC auf der
  Zephyr-Seite, der Bootlader. Das sind die großen Posten.
- Stufe **2 von 4**: übersetzt und im Abbild, **nie am Gerät gelaufen**.
  Es gibt noch keine Hardware.
- 79 Quelldateien waren dabei. Steht die Zahl nicht im Protokoll
  (`osk: <n> Quelldateien`), ist die Größe kein Ergebnis.

Nebenbefund, auch in `docs/architecture.md` nachgetragen: Mit 157 kB
hätte die Firmware auch in die rund 496 kB gepasst, die bei einem
Ersatzabbild **im** Chip geblieben wären. Die Auslagerung in den äußeren
Flash war also nicht nötig — sie schadet nicht, aber der Spielraum ist
größer als geplant.

### Was wirklich eingerichtet wurde

**Nordics eigener Weg ging nicht.** `nrfutil.exe` liegt auf
`developer.nordicsemi.com` hinter einer Bot-Prüfung; ein Abruf bekommt die
Prüfseite statt des Programms. Damit fallen **beide** Wege aus, die
`tools/build.py` kennt. Wer `nrfutil` haben will, lädt es von Hand im
Browser.

Gegangen wurde darum der Weg „manuell einrichten", den Nordic ebenfalls
beschreibt. Vier Stücke, alle auf `D:` — nichts nach `AppData\Roaming`,
das ist auf diesem Rechner virtualisiert.

| Stück | Fassung | Wo | Woher |
| --- | --- | --- | --- |
| Python | 3.12.15 | `D:\py312` | `astral-sh/python-build-standalone` (GitHub) |
| west, cmake, ninja | 1.5.0 / 3.31.6 / 1.13.2 | `D:\claude\ncs-tools\v312` | `pip` in eine eigene Umgebung |
| nRF Connect SDK | v3.4.1 (Zephyr 4.4.2) | `D:\ncs` | `west init` + `west update` von GitHub |
| Zephyr SDK | 1.0.1 | `D:\zephyr-sdk` | GitHub-Release `zephyrproject-rtos/sdk-ng` |

```powershell
west init -m https://github.com/nrfconnect/sdk-nrf --mr v3.4.1 D:\ncs
```

```powershell
west update --narrow -o=--depth=1
```

Vom Zephyr SDK reichen **zwei** der vierzig Pakete: `..._minimal.7z` und
`toolchain_gnu_windows-x86_64_arm-zephyr-eabi.7z` — 175 MB statt 1,9 GB.
Alles andere sind Werkzeugketten für Chips, die dieses Projekt nie anfasst.
Beide entpacken, dann die Werkzeugkette an die **richtige** Stelle legen
(siehe Falle 3) und bei CMake anmelden:

```powershell
D:\claude\ncs-tools\v312\Scripts\cmake.exe -P D:\zephyr-sdk\zephyr-sdk-1.0.1\cmake\zephyr_sdk_export.cmake
```

Danach vier Umgebungsvariablen, dann baut `tools/build.py`:

```powershell
$env:Path = "D:\claude\ncs-tools\v312\Scripts;$env:Path"
```

```powershell
$env:ZEPHYR_BASE = 'D:\ncs\zephyr'
```

```powershell
$env:ZEPHYR_SDK_INSTALL_DIR = 'D:\zephyr-sdk\zephyr-sdk-1.0.1'
```

```powershell
$env:ZEPHYR_TOOLCHAIN_VARIANT = 'zephyr'
```

### Die Fallen beim Einrichten

1. **Zephyr 4.4 verlangt Python 3.12.** Der Rechner hatte 3.11.9, CMake
   bricht mit `Could NOT find Python3 ... but required is at least "3.12"`
   ab. Darum die eigene Fassung auf `D:`; die 3.11 des Rechners bleibt.
2. **CMake 4.x ist zu neu.** `pip install cmake` holt 4.4; festgenagelt
   auf 3.31.6.
3. **Die Werkzeugkette gehört nach `<sdk>\gnu\<name>`**, nicht in den
   SDK-Stamm. Sonst meldet das SDK
   `Unable to find 'x86_64-zephyr-elf' or any other architecture` — gemeint
   ist „in `gnu\` liegt nichts". Die Meldung nennt eine Werkzeugkette, die
   man gar nicht haben will.
4. **`setup.cmd -c` des Zephyr SDK bricht mit Code 92 ab** und verlangt
   `7z` im PATH, obwohl `-c` nur bei CMake anmelden soll. Der Weg daran
   vorbei ist der `cmake -P`-Aufruf oben.
5. **`dtc` und `gperf` fehlen** und sind hier nicht nötig: Zephyr hat für
   die Gerätebeschreibung einen eigenen Parser in Python, und `gperf`
   braucht nur `CONFIG_USERSPACE` — das ist aus.

### Die Funde: elf Bauversuche, und was jeder gelehrt hat

**Keiner der Funde lag im C-Code selbst.** Alle 77 Quelldateien haben beim
ersten Lauf, der überhaupt bis zum Übersetzer kam, **fehlerfrei** für ARM
übersetzt — keine Typgrößen, keine Ausrichtung, keine fehlenden Header,
kein `int` gegen `int32_t`. Die Funde liegen im Zusammenbau.

#### 1. Zephyr beansprucht den Dateinamen VERSION — zweimal

Zephyr liest `apps/firmware/VERSION` auf **zwei** voneinander unabhängigen
Wegen und erwartet beide Male sein eigenes Format:

- über CMake (`cmake/modules/version.cmake`)
- über **Kconfig**: `Kconfig.zephyr` Zeile 10 enthält
  `osource "$(APPLICATION_SOURCE_DIR)/VERSION"` — fest verdrahtet, kein
  Schalter davor

Entschieden wurde: Zephyr gewinnt (`repo.md`: Toolchain vor Standard
innerhalb des Teilprogramm-Ordners). `apps/firmware/VERSION` trägt jetzt
`VERSION_MAJOR = 0` usw. — dieselbe vierstellige Nummer, nur anders
geschrieben. `tools/common.py` liest beide Schreibweisen
(`parse_version_file()`), `desktop` und `web` bleiben einzeilig.

Drei Sackgassen auf dem Weg dorthin, alle mit **derselben** Meldung
`VERSION_MAJOR must be present and in the range of 0-255 (Current: )`:

- Ein Pfad aus `$ENV{ZEPHYR_BASE}` muss durch `file(TO_CMAKE_PATH ...)`:
  `D:\ncs\zephyr` enthält `\n`, und CMake liest das als Zeilenumbruch.
- Zephyr läuft Versionsarten und -dateien **paarweise** ab. Zwei Dateien
  und eine Art ergeben ein Paar ohne Partner — und die Meldung hat dann
  eine leere Klammer.
- **Eine Vorgabe über `set()` wird weggeräumt.** `NcsConfig.cmake`
  (Zeilen 14–16 und 28–29) leiht sich `VERSION_TYPE` und `VERSION_FILE`
  für die SDK-eigene Version und setzt sie danach mit `set(VERSION_TYPE)`
  zurück. Wer dort etwas vorgeben will, braucht einen Eintrag im
  **Zwischenspeicher** (`CACHE INTERNAL`) — der überlebt, weil `set()`
  ohne Wert nur die gewöhnliche Fassung entfernt. Gemessen mit
  `variable_watch`.

#### 2. Kconfig verwirft still — der Fehler kommt 400 Zeilen später

`CONFIG_NRF_SECURITY=y` lässt sich **nicht** setzen (`def_bool`); es hängt
am Anbieter `PSA_CRYPTO_PROVIDER_CUSTOM`, der auf einem Nordic-ARM von
selbst gewählt wird. Der Schalter, der die Krypto wirklich anmacht, heißt
**`CONFIG_PSA_CRYPTO`** (`bool "PSA Crypto API"`).

Das Gefährliche ist die Wirkung, wenn er fehlt: der ganze Kconfig-Abschnitt
`if NRF_SECURITY` existiert dann nicht, und **21 von 25 `PSA_WANT_*`-Zeilen
werden still verworfen** — nur mit einer Warnung
(`was assigned the value 'y' but got the value 'n'`), nicht mit einem
Fehler. Der Bau läuft 400 Zeilen weiter und stirbt an
`psa/crypto.h: No such file or directory`, also weit entfernt von der
Ursache. **Wer einen fehlenden Header sieht, prüft zuerst den
Elternschalter.**

#### 3. Zwei Namenskollisionen zwischen Teilen

Diese zwei sind der eigentliche Gewinn des Baus, denn **der PC-Prüfsatz
kann sie nicht sehen**: unter `apps/firmware/test/` hat jeder Teil seinen
eigenen Bau (zehn getrennte `Makefile`/`CMakeLists.txt`), jeder Teil wird
einzeln gebunden, und die Kollision kann erst auftreten, wenn alles in
**ein** Abbild geht.

**`osk_apdu_parse` — doppelt vergeben, mit unvereinbarer Signatur.**

```c
applets/osk_apdu.h:   osk_result_t osk_apdu_parse(...);
transport/nfc_apdu.h: bool         osk_apdu_parse(...);
```

Der Binder nahm die `osk_result_t`-Fassung. `OSK_OK` ist **0**, und `0` ist
`false` — für die drei Aufrufer in `transport/` kehrte sich damit die
Bedeutung **exakt um**: Erfolg las als Fehlschlag und umgekehrt. In
`nfc_tag.c` lief der Weiterweg nur bei **Parse-Fehler** und las danach
`c.cla` und `c.ins` aus einer Struktur, die der Zerleger nie gefüllt hat —
im APDU-Parser, den `docs/threat-model.md` als die erste Angriffsfläche
beschreibt. Welche Fassung gewann, hing an der Reihenfolge im Archiv.

Behoben: die Transport-Fassung heißt jetzt `osk_nfc_apdu_parse`, die
Applet-Fassung behält den Namen.

**`osk_wipe` — doppelt geschrieben.** Einmal in `ctap/osk_crypto.c`, einmal
in `se050/se050_crypto_common.c`. Funktional gleich, aber die se050-Fassung
prüft auf `NULL` und `len == 0`, die ctap-Fassung lief bei `NULL` ins
Verderben. Behoben: **zusammengelegt**, die vorsichtigere gewinnt, neue
Heimat `include/osk_mem.h` und `src/osk_mem.c` — gemeinsam, weil auch
`wallet` sie aufruft. Die 268 Aufrufstellen blieben unberührt; nur die
Definition ist gewandert.

Dass `src/osk_mem.c` keinem Teil gehört, musste in **sechs** Bauvorschriften
nachgetragen werden: die `CMakeLists.txt` der Firmware und fünf
`test/*/Makefile` (ctap, guard, se050, transport, wallet). Bei `ctap` und
`transport` reicht die `CORE`-Liste nicht — dort haben einzelne Ziele eigene
Dateilisten (`test_crypto`, und alles was `$(CRYPTO)` benutzt).

**Noch offen, ein dritter Name: `osk_apdu_t`.** Der Typ ist ebenfalls
zweimal definiert, mit **verschiedenem Aufbau** — das elfte Feld ist in
`transport/nfc_apdu.h` ein `bool chaining_cla`, in `applets/osk_apdu.h` ein
`uint8_t channel`. Gleiche Größe, verschiedene Bedeutung. **Der Binder kann
das nicht sehen**, Typen werden nicht gebunden. Heute harmlos, weil keine
Datei beide Köpfe einbindet (das wäre ein Übersetzungsfehler) — aber die
Falle schlägt beim ersten gemeinsamen Einbinden zu.

#### 4. Sechs Namen, die es auf dem Gerät noch nicht gibt

Alles übersetzt, aber das Binden brach ab:

| Name | deklariert in | Lücke |
| --- | --- | --- |
| `osk_se050_platform_bus` | `se050/se050_platform.h` | **absichtlich** |
| `osk_se050_platform_scp_keys` | `se050/se050_platform.h` | **absichtlich** |
| `osk_se050_platform_channel_obj` | `se050/se050_platform.h` | **absichtlich** |
| `osk_transport_register` | `include/osk_transport.h` | ungeschrieben |
| `osk_transport_enable` | `include/osk_transport.h` | ungeschrieben |
| `osk_result_text` | `include/osk_result.h` | ungeschrieben |

Die drei `se050_platform`-Haken sind **kein Fehler**: `se050_platform.h`
sagt ausdrücklich „Es gibt absichtlich keine Vorgabe-Umsetzung: fehlt sie,
bricht das Binden — und nicht erst der Betrieb mit einem Schlüssel aus
Nullen." Sie warten auf die Platinen- und Tresorschicht.

Die anderen drei sind echte Lücken. `osk_transport_register`/`_enable` sind
der Vertrag, über den sich jedes Applet anmeldet — acht Kopfdateien
verweisen darauf, aber **keine `.c` setzt ihn um**. Auf dem Gerät gibt es
die Vermittlung der Transporte also noch nicht; sie existiert nur als
Attrappe im Prüfsatz. Dass 94.287 Prüfungen grün sind, hat das nicht
gezeigt.

#### 5. Variablen hinter -- erreichen das Abbild nicht

**Befund, Lösung offen.** NCS 3.x baut über *Sysbuild*: `west build`
richtet einen äußeren CMake-Lauf ein, der das Abbild als eigenen Lauf
startet. Was hinter `--` mitgegeben wird, bleibt **draußen**. Belegt an
beiden Zwischenspeichern:

```
<bau>/CMakeCache.txt           ->  OSK_MESSBAU=y
<bau>/firmware/CMakeCache.txt  ->  (nichts)
```

`-Dfirmware_OSK_MESSBAU=y` hilft **auch nicht** (gemessen).

Damit gibt `tools/build.py` Zeile 89 sein `-DOSK_VERSION=` an eine Stelle,
an der die Firmware es nicht sieht — und der Satz weiter unten, die
`CMakeLists.txt` müsse `OSK_VERSION` auswerten, geht so nicht auf.
Praktisch fällt es heute nicht auf, weil `apps/firmware/CMakeLists.txt` auf
`apps/firmware/VERSION` zurückfällt; auffallen würde es, sobald `build.py`
eine **andere** Version mitgeben will als die Datei enthält.

Für die Messung wurde das umgangen, nicht gelöst: **`--no-sysbuild`** baut
nur das eine Abbild, und dann landet `-D` dort, wo es hingehört. Für die
Größenmessung ist das sogar das Richtigere — gemessen wird unser Abbild
allein, und genau das wird gegen die 960 kB gestellt.

### Der Messbau

`apps/firmware/src/messbau.c` übersetzt sich zu nichts, solange
`OSK_MESSBAU` nicht gesetzt ist. Nur mit `-DOSK_MESSBAU=y` kommen sechs
Platzhalter für die fehlenden Namen dazu, damit der Binder durchkommt und
eine Größe herauskommt. Sie geben alle `OSK_ERR_UNSUPPORTED` zurück: wer
dieses Abbild versehentlich in ein Gerät baut, bekommt ein Gerät, das
nichts tut — und keines, das mit einem Schlüssel aus Nullen arbeitet.

**Ein gewöhnlicher Bau hat die Platzhalter nicht** und bricht weiter an den
sechs Namen ab. Das ist gewollt: so meldet `tools/check.py` die Lücken
weiter, statt sie zu verstecken.

### Prüfen: die Firmware-Prüfsätze brauchen WSL

Auf Windows meldet `tools/check.py` `firmware/tests` als übersprungen —
`make` und ein Host-Übersetzer fehlen. In WSL (Debian) sind `make`, `gcc`
und `cmake` da und `/mnt/d` ist eingehängt, dort läuft der ganze Satz:

```powershell
wsl -d Debian --cd /mnt/d/claude/<worktree> -- python3 tools/check.py
```

Ergebnis am 25.10.2026: 17 Schritte ok, 0 Fehler, **94.287 Prüfungen**.
Übersprungen bleiben dort `desktop` und `web` (Go und Node.js sind in der
WSL-Instanz nicht installiert) — also beide Wege fahren, wenn alles geprüft
werden soll.

Zwei Fallen beim Messen in WSL, beide selbst hineingelaufen:

- **Ein Ausstiegscode hinter `|| true` ist der von `true`.** Ein Messskript
  meldete `AUSSTIEG=0`, während der Bericht fünf Fehler auflistete. Die
  Zeile `FEHLER in:` ist der Beleg, nicht der Code.
- **WSL kann `Z:\` nicht übersetzen** (Netzlaufwerk). Aus einem
  Arbeitsordner auf `Z:` heraus scheitert jeder Aufruf mit
  `failed to translate 'Z:\GitHub'`. Darum `--cd` auf einen Pfad unter
  `/mnt/c` oder `/mnt/d` setzen.

## Was auf Windows anders ist

- Die SDK-Werkzeuge (`west`, `nrfutil`) werden aus PowerShell heraus **nicht direkt** gestartet, sondern über `cmd`. Das erledigt `tools/common.py` selbst; man muss nichts tun. Der Grund: direkt aus PowerShell scheitert die Werkzeugkette (gleiche Erfahrung wie bei `idf.py` in einem anderen Projekt).
- Pfade ohne Leerzeichen und nicht zu tief verwenden; das Zephyr-Verzeichnis hat lange Pfade.
- Auf einem Netzlaufwerk (NAS) liegende Arbeitsordner können beim Bauen Ärger machen (Symlinks, Pfadlängen); dann den Worktree auf ein lokales Laufwerk legen.
- Die Skripte schreiben Dateien mit `\n` als Zeilenende, egal auf welchem System.

## Version hochzählen

```powershell
python tools/set_version.py firmware --build
```

Weitere Arten: `--feature` (neues Feature), `--release` (neues Release), `--version` (nur auf Ansage). Statt eines Teilprogramms geht `all`. Geschrieben wird nur `apps/<typ>/VERSION`; Regeln stehen in `version.md` der Projektregeln (`v.x.y.z`, alle Stellen rechts springen auf 0).

```powershell
python tools/set_version.py --check
```

## Auf das Gerät schreiben

**Ungeprüft** – es gab beim Schreiben der Skripte keine Hardware. Die Befehle folgen der Nordic-Dokumentation, sind aber nie gegen ein Gerät gelaufen. Nach dem Schreiben die Version am Gerät kontrollieren.

DK über den Debugger (braucht `nrfutil` mit Modul `device` oder das SDK):

```powershell
python tools/flash.py dk
```

Dongle: Bootloader mit der Reset-Taste starten (Laufwerk mit `INFO_UF2.TXT` erscheint), dann:

```powershell
python tools/flash.py dongle
```

Mit eigener Datei oder bestimmtem Laufwerk: `--file <Datei>`, `--drive <Laufwerk>`.

## Release

```powershell
python tools/set_version.py all --release
```

```powershell
python tools/release.py
```

`release.py` bricht ab, wenn die Versionen nicht gleich oder kein Release sind (`y` und `z` müssen 0 sein), der Arbeitsbaum nicht sauber ist, `check.py` Fehler meldet, `[Unreleased]` im Changelog leer ist oder ein Teilprogramm nicht gebaut werden konnte. Mit `--partial` dürfen *leere* Teilprogramme fehlen. Ergebnis: `dist/release/v<Version>/` mit den Dateien, `SHA256SUMS.txt` und `RELEASE-NOTES.md`. `--write-changelog` schreibt `CHANGELOG.md` fort. Taggen und Veröffentlichen macht das Skript nie; die Befehle werden am Ende genannt.

## CI

`.github/workflows/check.yml` führt `tools/check.py` auf Linux und Windows aus und schlägt fehl, wenn kein einziger Test lief (zusätzlich zur Prüfung in `check.py` liest ein eigener Schritt den Bericht `dist/check-report.json`). Das nRF Connect SDK ist in der CI nicht installiert: die Firmware erscheint dort als „übersprungen".
