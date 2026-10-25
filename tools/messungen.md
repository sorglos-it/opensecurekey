# Messungen an den Werkzeugen

Was hier steht, ist **gemessen**, nicht vermutet. Jede Zeile nennt, wie sie
zustande kam. Vermutungen sind als Vermutung gekennzeichnet.

## OSK_VERSION kam bei einem Sysbuild nie im Abbild an (25.10.2026)

### Der Befund

`tools/build.py` gab die Version hinter `--` an `west build` mit:

```
west build -b <board> -d <bau> <quelle> -- -DOSK_VERSION=0.0.1.0
```

Bei einem **Sysbuild** laeuft CMake zweimal: einmal aussen und einmal je
Abbild. Alles hinter `--` landet nur im **aeusseren** Lauf. Belegt an zwei
Zwischenspeichern eines Messbaus:

| Datei | `OSK_VERSION` darin |
| --- | --- |
| `mess-build/CMakeCache.txt` | ja |
| `mess-build/firmware/CMakeCache.txt` | **nein** |

Die Firmware bekam ihre Versionsnummer also gar nicht, und `west` meldete
trotzdem Erfolg. Vorher gemessen und **ohne Wirkung**: `-Dfirmware_OSK_MESSBAU=y`
(die Variable steht wieder nur im aeusseren Bau). `west build --no-sysbuild`
umgeht es, weil dann nur ein CMake-Lauf stattfindet - damit faellt aber
MCUboot aus dem Bau, also kein Weg fuer ein Geraet mit Bootlader.

### Die Messung

Messtisch: zwei geschachtelte CMake-Laeufe (`ExternalProject_Add`, so wie
Sysbuild seine Abbilder baut), `cmake` aus WSL, kein SDK noetig. Der innere
Lauf schreibt auf, was er sieht.

| Weg | aeusserer Zwischenspeicher | innerer Lauf |
| --- | --- | --- |
| `-DOSK_VERSION=1.1.1.1` hinter `--` | ja | **nein** (`cache=[] env=[]`) |
| `-Dfirmware_OSK_VERSION=1.1.1.1` | ja | **nein** |
| Umgebungsvariable `OSK_VERSION=2.2.2.2` | - | **ja** (`env=[2.2.2.2]`) |
| Umgebung, innen mit `set(... CACHE ...)` gelesen | - | **ja**, steht dann auch im inneren Zwischenspeicher (`OSK_VERSION:STRING=3.3.3.3`) |

Das bestaetigt den Befund und nennt den Weg, der ankommt: **die Umgebung**.
Jeder geschachtelte Prozess erbt sie, der innere CMake-Lauf also auch.

### Was `build.py` jetzt tut

1. Setzt `OSK_VERSION` in der **Umgebung** des `west`-Aufrufs - der Weg, der
   gemessen ankommt.
2. Gibt zusaetzlich `-DOSK_VERSION=` mit (bei `--no-sysbuild` ist der aeussere
   Lauf der einzige) und `-Dfirmware_OSK_VERSION=` (die Schreibweise fuer ein
   einzelnes Abbild). Beide kosten nichts.
3. **Sieht nach dem Bau nach**, ob die Version wirklich im Abbild steht
   (`version_arrived()` liest die `CMakeCache.txt`-Dateien im Bauordner).
   Steht sie nicht drin, ist der Schritt **FEHLER** statt stillschweigend
   "ok": ein Abbild ohne Versionsnummer gilt nicht als gebaut.

Die `CMakeLists.txt` der Firmware muss sie so lesen:

```cmake
if(NOT OSK_VERSION AND DEFINED ENV{OSK_VERSION})
  set(OSK_VERSION "$ENV{OSK_VERSION}" CACHE STRING "Version aus apps/firmware/VERSION")
endif()
```

### Die Luecke, ehrlich

**Am echten Sysbuild ist nichts davon gemessen.** Auf diesem Rechner gibt es
kein nRF Connect SDK (`west` und `nrfutil` fehlen; WSL hat `arm-none-eabi-gcc`
und `cmake`, aber kein Zephyr), und `apps/firmware/CMakeLists.txt` existiert
noch nicht - der Firmware-Bau wird darum durchgehend uebersprungen. Der
Messtisch oben ist ein **Nachbau** von Sysbuild, nicht Sysbuild.

Nicht gemessen ist damit auch, ob die Version danach wirklich im `.hex`
steht. Punkt 3 oben ist genau dafuer da: wer den ersten echten Sysbuild
fahrt, bekommt eine Zahl statt einer Vermutung.

**Vermutung, nicht gemessen:** `apps/firmware/VERSION` steht schon in Zephyrs
eigener Schreibweise (`VERSION_MAJOR = 0` ...), Zephyr liest die Datei selbst
und erzeugt `app_version.h`. Es koennte sein, dass `OSK_VERSION` damit
ueberhaupt nicht mehr gebraucht wird. Das ist zu pruefen, wenn die
`CMakeLists.txt` entsteht.

## Namenskollisionen zwischen den Teilen (25.10.2026)

Neuer Schritt `firmware/symbole` in `tools/check.py`, Leser in
`tools/symbols.py`. Gegenprobe auf einer Kopie des Baums (im Kratzordner, das
Repo blieb unberuehrt):

| Lage | doppelt | Rueckgabetyp-Widerspruch |
| --- | --- | --- |
| Baum wie im Repo | 2 (`osk_wipe`, `osk_apdu_parse`) | 1 (`osk_apdu_parse`) |
| beide umbenannt | 0 | 0 -> Schritt **ok** |
| zwei neue gleiche Namen eingebaut | 1 gefunden | 0 |
| zwei Koepfe, `osk_result_t` gegen `bool` | - | 1 gefunden |

Danach entfernt. Eine Pruefung, die immer anschlaegt, ist so wertlos wie eine,
die nie anschlaegt - darum steht die zweite Zeile mit in der Tabelle.

**Die blinden Flecken stehen im Kopf von `tools/symbols.py`** und sind je
durch einen Test festgenagelt (`tools/tests/test_symbols.py`, Klasse
`BlindeFlecken`): fremder Code (libc, Zephyr, PSA, OpenSSL) wird nicht
gelesen, Namen aus Makros und aus Assembler fallen nicht auf, zweimal
derselbe Name in einer Datei auch nicht (das faengt der Uebersetzer), und ob
sich zwei `#if`-Bedingungen ausschliessen, ist am Text nicht zu entscheiden.
Ein Gesamt-Bindelauf bleibt die genauere Messung.
