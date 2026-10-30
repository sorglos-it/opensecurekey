# OpenSecureKey – Handbuch

**Dieses Gerät gibt es noch nicht zu kaufen, und bisher hat es niemand benutzt.**

Alles in diesem Handbuch beschreibt, was gebaut wird. Nichts davon ist je an einem echten Stick gelaufen. Es ist noch kein Stick gebaut. Wo etwas nur am Rechner geprüft wurde, steht es dabei.

## Was ist das?

Ein Sicherheitsstick am Schlüsselanhänger. Er bewahrt deine Zugänge auf, und nichts kommt heraus, wenn du nicht selbst am Stick den Knopf drückst.

Das heißt konkret:

- Er meldet dich bei Webseiten an, ohne dass du ein Passwort tippst (Passkeys).
- Er speichert Passwörter, Codes, Schlüssel und Zertifikate.
- Er kann sich wie eine Chipkarte verhalten.
- Er kann Zahlungen für Bitcoin und Ethereum unterschreiben.
- Er kann ein Programm nur laufen lassen, solange er steckt (Dongle).

Der Stick hat einen eigenen kleinen Bildschirm. Dort steht, **was** du gerade freigibst. Der Rechner kann dir da nichts vormachen.

## So ist dieses Handbuch gebaut

| Datei | Wofür |
| --- | --- |
| [installation.md](installation.md) | Erste Einrichtung, Schritt für Schritt. Dort steht die Frage, die sich nie mehr ändern lässt. |
| [usage.md](usage.md) | Alltag: anmelden, Passwort holen, Code, unterschreiben. |
| [settings.md](settings.md) | Was sich einstellen lässt. |
| [how-it-works.md](how-it-works.md) | Wie der Stick innen arbeitet. Und was er **nicht** kann. |
| [troubleshooting.md](troubleshooting.md) | Wenn etwas nicht klappt. Und: **wenn der Stick weg ist.** |
| [development.md](development.md) | Für alle, die das Projekt selbst bauen. |

## Das Wichtigste vorab

- **Eine Frage bei der Einrichtung lässt sich nie mehr ändern:** Soll es später einen Ersatz-Stick geben können? Lies sie in Ruhe. Siehe [installation.md](installation.md).
- **Ohne Ersatz-Stick und ohne Sicherung ist ein verlorener Stick ein Totalverlust.**
- **Wer Hauptkennwort und alle Sticks verliert, verliert alles.** Dafür gibt es keine Hintertür. Siehe [troubleshooting.md](troubleshooting.md).
- **Der Notfallzettel bei der Einrichtung ist nicht überspringbar.** Genau deswegen. Siehe [installation.md](installation.md).
- Es gibt **keine Prüfung von außen** und **keinen Test an echter Hardware.** Verlass dich noch auf nichts, was du nicht ersetzen kannst.

## Was der Stick nicht kann

Ausführlich in [how-it-works.md](how-it-works.md). Kurz:

- Er hat in der ersten Ausbaustufe **keinen Fingerabdrucksensor.** Und für keinen der geprüften Sensoren ist belegt, dass er einen Fingerabdruck-Nachbau erkennt.
- Er schützt **nicht** davor, dass dich jemand zwingt, den Knopf zu drücken.
- Er schützt **nicht** vor veränderter Firmware.
- Er rettet dich **nicht**, wenn Hauptkennwort und alle Sticks weg sind.
- **Er gibt keinen privaten Schlüssel als Datei heraus. Nie.** Software, die zwingend eine Schlüsseldatei (`.pem`, `.pfx`) verlangt, kann ihn grundsätzlich nicht bedienen. Das Zertifikat darf heraus, der Schlüssel nicht. Siehe [usage.md](usage.md).
- Er kennt **keine Profile für Passkeys.** Beim Profilwechsel meldest du dich bei den Diensten neu an.
- **„Einstecken und starten“ heißt über eine Minute warten.** Der Stick gibt seine Daten langsam heraus.
- Bei der geplanten Speicherkarte verschlüsselt **eine von drei Betriebsarten nicht.** Und eine Unterschrift beweist Unverändertheit, nicht Urheberschaft. Siehe [how-it-works.md](how-it-works.md).

## Stand der Teile

| Teil | Stand |
| --- | --- |
| Gerät (Platine, Gehäuse) | noch nicht gebaut |
| Firmware | geschrieben, auf dem Rechner mit Attrappen getestet, nie auf dem Stick |
| Programm `osk` am Rechner | gegen eine Test-Attrappe geprüft, nie gegen einen echten Stick |
| Windows und Linux | übersetzt und mit der Attrappe getestet |
| macOS | nie ausprobiert |
| Browser und Webseiten | nie ausprobiert |

Tiefer gehende Technik (englisch, für Entwickler): [`docs/`](../) im Projektordner.
