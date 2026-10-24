# Selbst bauen und entwickeln

Dieses Kapitel ist für alle, die das Projekt selbst bauen. Die ausführliche Technik steht auf Englisch im Ordner [`docs/`](../) (zum Beispiel `architecture.md`, `threat-model.md`, `vault.md`, `build.md`).

**Stand:** Es gibt noch keinen Stick. Gebaut und getestet wird am Rechner, mit Attrappen für Chip, Speicher und Bildschirm.

## Ordner

| Ordner | Inhalt | Sprache |
| --- | --- | --- |
| `apps/firmware` | Software im Stick (nRF52840 + SE051) | C / Zephyr |
| `apps/desktop` | Verwaltungsprogramm `osk` mit lokaler Oberfläche | Go |
| `apps/web` | Browser-Erweiterung | JavaScript |
| `shared/` | Protokoll, Tresor-Format, Bibliothek `osk-guard` | |
| `hardware/` | Schaltplan, Pinbelegung, Layout-Notizen | |
| `cad/` | Gehäuse | |
| `tools/` | Bau- und Prüfwerkzeuge | Python |

## Was du brauchst

| Teil | Werkzeug | Ohne dieses Werkzeug |
| --- | --- | --- |
| alle | Python 3 | nichts geht |
| `apps/desktop` | Go 1.24 oder neuer | wird übersprungen |
| `apps/web` | Node.js 20 oder neuer (nur zum Prüfen) | Bauen geht, Prüfen wird übersprungen |
| `apps/firmware` | nRF Connect SDK (Zephyr, `west`) | wird übersprungen |

„Übersprungen“ heißt immer: **nicht gebaut, nicht geprüft.**

## Alles prüfen

```powershell
python tools/check.py
```

- Es prüft, was ohne Hardware geht.
- Am Ende steht, wie viele Schritte liefen, was übersprungen wurde und wie viele Tests ausgeführt wurden.
- Der Ausstiegscode ist nur 0, wenn nichts scheiterte **und** mindestens ein Test lief.

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

Die Ergebnisse liegen in `dist/`.

Das Programm `osk` lässt sich auch direkt bauen. Es braucht kein fremdes Paket und kein cgo (außer für macOS).

```powershell
go build -C apps\desktop -trimpath -ldflags "-s -w" -o ..\..\dist\osk.exe .
```

## Testen ohne Stick

Der Test-Stick (die Attrappe) läuft nur im Speicher. Er verschlüsselt nichts und tut nur so, als hättest du den Knopf gedrückt. **Leg dort nie echte Geheimnisse ab.**

```powershell
go run -C apps\desktop .\tests\simulator -port 8201
```

Dann trägst du `127.0.0.1:8201` unter **Einstellungen → Adresse der Attrappe** ein. Die PIN der Attrappe ist `123456`.

Die Tests des Programms:

```powershell
go test -C apps\desktop ./...
```

## Vier Stufen von „geprüft“

Jede Aussage über „geprüft“ meint eine von vier Stufen:

1. geschrieben
2. übersetzt
3. im Abbild
4. auf dem Stick gelaufen

Dieses Projekt steht bei **Stufe 2, dazu Läufe am Rechner mit Attrappen.** Stufe 3 und 4 fehlen überall.

## Entscheidungen und ihr Grund

| Entscheidung | Grund |
| --- | --- |
| Zwei Chips: Funkchip redet, Sicherheitschip hält Geheimnisse | Der Leseschutz des Funkchips wurde schon ausgehebelt. |
| Zwei Wege der Einrichtung, einmalig wählbar | Der Sicherheitschip leitet keinen Schlüssel in sich ab. Ein im Chip entstandenes Geheimnis lässt sich nie spiegeln. |
| Bildschirm und Knopf an jeder Freigabe | Der Rechner darf nicht entscheiden, was du freigibst. |
| Keine Freigabe ohne funktionierenden Bildschirm | Du könntest nicht sehen, was du freigibst. |
| Namen und Adressen werden nie abgeschnitten | Ein abgeschnittener Empfänger ist schlimmer als kein Bildschirm. |
| Stufe 1 ohne Fingerabdrucksensor | Bei FIDO2 ersetzt er die PIN, er ergänzt sie nicht. Kein Sensor erfüllte alle vier Bedingungen. |
| Update-Platz im äußeren Speicher | Der innere Speicher des Funkchips ist knapp. |
| Eigene Schrift und eigene Prüfer für das Protokoll | Keine fremde Grafik- oder Parser-Bibliothek, harte Grenzen, kein dynamischer Speicher. |
| Kein Fremdcode | Alles ist gegen die Spezifikation geschrieben. Verschlüsselung kommt immer aus geprüften Bibliotheken. |

## Fünf Sprachen

- Kein Text steht fest im Code.
- Jede Sprache ist eine Datei im Ordner `lang/`: `de.json`, `en.json`, `fr.json`, `it.json`, `es.json`.
- Ein Test meldet fehlende Schlüssel als Fehler.
- Fehlt ein Text, gilt der englische.

## Zusammenbau

**Neu:** Dieser Abschnitt ist neu. Hier kommen weitere Auflagen für den Zusammenbau hin, sobald die Hardware auf dem Tisch liegt.

- Die Ferritfolie unter dem Akku muss **genau 19,75 mm** breit sein, **nicht breiter, auch nicht ein bisschen.**
- Sonst kippt die Zelle.

## Mitmachen

Ändere nie nur die Technik. Wenn sich Funktion, Bedienung, Einstellungen oder Aufbau ändern, gehört die Doku dazu: `docs/` und dieses Handbuch.
