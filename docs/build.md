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
