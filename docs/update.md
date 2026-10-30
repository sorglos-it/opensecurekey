# Firmware-Update

Status: Entwurf. **Nichts davon ist auf Hardware gelaufen.** Fuer ARM ist
inzwischen uebersetzt worden - am 25.10.2026, nach dem Stand dieser Seite; die
Zahlen stehen an genau einer Stelle, `docs/build.md`, Abschnitt "Der erste Bau
fuer ARM". Was auf dieser Seite gemessen ist, steht am Ende unter
"Was geprueft ist und was nicht".

Dieser Teil entscheidet, ob ein Geraet im Feld reparierbar ist. Darum ist die
erste Anforderung nicht "das Update muss funktionieren", sondern:

> **Bricht ein Update ab, startet die alte Fassung weiter.**

Ein Stick, der nach einem halben Update nicht mehr startet, ist Schrott -
und er ist Schrott mit allen Passwoertern darin. Jede Entscheidung unten
faellt darum zugunsten der alten Fassung aus, sobald etwas unklar ist.

## Wie es abläuft

```
  Rechner                                   Stick
  ───────                                   ─────
  tools/sign.py   Paket unterschreiben
  tools/dfu.py    Kopf + Signatur  ───────> pruefen: Signatur, Brett, Rueckschritt
                                            (nichts wird geschrieben, bis das stimmt)
                  Abbild in Stuecken ─────> in den ZWEITEN Platz schreiben
                                            (externes QSPI-Flash, 64 MB)
                  abschliessen     ───────> Hash pruefen -> "bereit"
                                            Taste am Geraet, dann Neustart
                                              Bootloader schaltet um
                                              neue Fassung startet
                                              meldet sich -> bestaetigt
                                              meldet sich NICHT -> nach 3
                                              Versuchen zurueck auf die alte
```

Die Stufen heissen im Code `IDLE`, `WRITING`, `READY`, `TESTING`,
`CONFIRMED` (`apps/firmware/boot/osk_boot.h`). Nur `READY` fuehrt zum
Umschalten, und `READY` wird erst gesetzt, wenn jedes Byte da ist **und** der
Hash stimmt.

## Der zweite Platz liegt im externen Flash

Der nRF52840 hat 1 MB Flash. Darin liegen der Bootloader und die Firmware.
Lage der zweite Platz ebenfalls dort, blieben je Abbild rund **496 kB** -
und Zephyr mit BLE, USB, NFC und der Kryptobibliothek ist davon schon ein
grosser Teil. Im externen QSPI-Baustein sind es rund **960 kB**.
Begruendung und Zahlen: `docs/architecture.md`, Abschnitt "How much room is
there, really".

**War das noetig? Nein - und der Platz bleibt trotzdem draussen.** Die
Begruendung oben ist die Schaetzung von vor dem ersten ARM-Bau. Gemessen hat
das Abbild 157 kB und haette auch in die rund 496 kB gepasst. Der Befund steht
an einer Stelle: `docs/architecture.md`, Abschnitt "Measured, 25 October 2026".
Die Auslagerung schadet nicht und bleibt, weil die Aufteilung festliegt und
der eigene Code zu einem grossen Teil noch nicht erreichbar ist - sie ist
Reserve, nicht Notwendigkeit.

Die Aufteilung steht in `apps/firmware/boot/pm_static.yml`. Zwei Dinge daran
sind keine Geschmacksfrage:

- **Der zweite Platz liegt im externen Flash VORN, der Tresor dahinter.**
  Umgekehrt wuerde jede Aenderung der Tresorgroesse die Adresse des zweiten
  Platzes verschieben - und ein Bootloader mit der alten Adresse schreibt
  dann in den Tresor.
- **Der Zustand des Updates liegt im INTERNEN Flash**, in einer eigenen
  Seite (`osk_boot_state`). Der Bootloader muss ihn lesen koennen, bevor der
  QSPI-Baustein ueberhaupt angesprochen wurde, und eine eigene Seite, weil
  Flash seitenweise geloescht wird: liegt der Zustand mit etwas anderem auf
  einer Seite, nimmt jede Loeschung das andere mit.

Ausgetauscht wird, nicht ueberschrieben (`CONFIG_BOOT_SWAP_USING_MOVE`).
Nach dem Umschalten liegt die **alte** Fassung im zweiten Platz - das ist
der Rueckweg. Ueberschreiben waere kleiner und schneller und haette keinen.

## Das Paket

| Teil | Laenge | Inhalt |
| --- | --- | --- |
| Kopf | 72 Byte | Kennung, Format, Brett, Fassung, Zaehler, Abbildlaenge, SHA-256 des Abbilds |
| Signatur | 64 Byte | ed25519 **ueber den Kopf** |
| Abbild | Rest | die Firmware, wie MCUboot sie erwartet |

Die Signatur deckt nur den Kopf, und im Kopf steht der Hash des Abbilds.
Das hat zwei Gruende: die Herkunft ist geprueft, **bevor** das erste Byte
des Abbilds angefasst wird, und das Abbild kann im Vorbeischreiben gehasht
werden - es muss nicht in den Arbeitsspeicher passen, denn es ist groesser
als der RAM.

Alle Zahlen little endian. Der Aufbau steht an zwei Stellen und muss gleich
bleiben: `apps/firmware/boot/osk_boot.h` und `tools/sign.py`. Ein Test
vergleicht beide Zahlensaetze.

## Nur unterschriebene Pakete

> Wie der signierte Bootloader eingestellt und gebaut wird, wie der
> Schluessel von aussen hereinkommt und wie die zwei Fassungen (mit und ohne
> RSA) auseinandergehalten werden, steht in **`docs/signatur.md`**. Dort steht
> auch, warum der signierte Bootloader frueh ins Abbild muss: wird die
> Auslesesperre erst spaeter gesetzt, muesste man sie zum Nachruesten loesen,
> und das loescht den Chip.

```powershell
python tools/sign.py --gen-key
```

```powershell
python tools/sign.py dist/osk-firmware-0.0.1.0-osk_v1.hex --board osk_v1 --counter 1
```

**Der private Schluessel liegt ausserhalb des Projektordners und nie im
Repo.** Vorgabe: `%USERPROFILE%\.osk-keys\firmware-ed25519.key`, anders
ueber `--key` oder `OSK_SIGN_KEY`. `sign.py` **bricht ab**, wenn der Pfad im
Projektordner liegt - auch wenn er ausdruecklich genannt wird. Ein
Schluessel, der einmal in einem Commit steckt, ist aus der Geschichte nicht
mehr herauszuholen.

Zwei Schluessel sind im Spiel, und sie sind nicht derselbe:

| Schluessel | Wer prueft | Wo er liegt |
| --- | --- | --- |
| Paket-Schluessel | unsere Schicht, vor dem Schreiben | ausserhalb des Projekts, `sign.py` |
| MCUboot-Schluessel | MCUboot, vor dem Umschalten | ausserhalb des Projekts, `CONFIG_BOOT_SIGNATURE_KEY_FILE` |

Fehlt die zweite Zeile beim Bauen, nimmt MCUboot **seinen eigenen
Beispielschluessel** - und der ist oeffentlich bekannt. Das ist kein
kleiner Fehler, sondern das Ende der ganzen Absicherung. Der Bau muss
abbrechen, wenn die Zeile fehlt.

Den oeffentlichen Teil fuer den Bootloader:

```powershell
python tools/sign.py --pubkey-c
```

## Rueckschritt gesperrt - der Zaehler liegt im SE050

Jedes Paket tragt einen Zaehler. Ein Paket wird abgelehnt, wenn sein Zaehler
kleiner ist als der im Chip, oder wenn er gleich ist und die Fassungsnummer
nicht hoeher.

**Der Zaehler liegt im SE050, nicht im Flash.** Ein Zaehler im Flash laesst
sich zuruecksetzen: Flash loeschen, Zaehler auf 0, alte Firmware mit einer
bekannten Luecke wieder aufspielbar. Wer das Geraet in der Hand hat, kommt
an den QSPI-Baustein heran. Der SE050 gibt seinen Zaehler nicht her und
laesst ihn nicht sinken - genau wie den Fehlversuchszaehler der PIN.

**Angehoben wird er erst nach der Bestaetigung**, nicht beim Schreiben. Wer
ihn frueher anhebt, mauert den Rueckweg zu, bevor bewiesen ist, dass die
neue Fassung ueberhaupt startet.

## Der Tresor bleibt erhalten

Das Update fasst die Tresor-Partition nicht an: kein Schreiben, kein
Loeschen, kein Byte. Der Flash-Treiber fragt vor jedem Zugriff
`osk_boot_may_write()`, und erlaubt ist nur der zweite Platz.

Hat sich die **Formatfassung** der Daten geaendert, wandert der Tresor beim
ersten Start der neuen Firmware (`osk_store_migrate()`, siehe
`shared/vault-format.md`). Das Update selbst wandert nichts - es weiss nicht,
ob es ueberhaupt bestaetigt wird, und ein gewanderter Tresor waere von der
alten Fassung nach einem Rueckdreh nicht mehr zu lesen.

## Notfallweg

Taste beim **Einstecken** gedrueckt halten: der Bootloader meldet sich,
statt die Firmware zu starten. Damit bleibt ein Geraet erreichbar, dessen
Firmware nicht mehr antwortet - ohne Oeffnen und ohne Debugger.

Zwei Dinge sind dabei Absicht:

- Es wird **fuenfmal** gelesen, und alle Messungen muessen "gedrueckt"
  sagen. Ein einzelner Stoerimpuls darf nicht in den Bootloader fuehren.
- Nur im Fenster **direkt nach dem Start** (500 ms). Sonst koennte ein
  Programm den Nutzer zum Druecken bringen und so in den Bootloader
  schalten.

Der Notfallweg hat Vorrang vor allem anderen - auch vor einem unlesbaren
Zustand. Genau dafuer ist er da.

## Aufspielen

```powershell
python tools/dfu.py --info dist/osk-0.0.1.0-osk_v1.oskfw
```

```powershell
python tools/dfu.py dist/osk-0.0.1.0-osk_v1.oskfw --manual
```

**Der Stick fragt vorher.** Jedes Aufspielen braucht einen Tastendruck am
Geraet; das Werkzeug kann das nicht umgehen und versucht es auch nicht.
Gesprochen wird Befehl 13 (`shared/protocol.md`) mit diesen Unterbefehlen:

| `op` | Name | Daten | Antwort |
| --- | --- | --- | --- |
| 1 | `begin` | `hdr` (136 Byte: Kopf + Signatur) | - |
| 2 | `chunk` | `off`, `b` (Stueck des Abbilds) | - |
| 3 | `finish` | - | - |
| 4 | `abort` | - | - |
| 5 | `state` | - | `stage`, `written`, `len`, `ver` |

`begin` verlangt die Freigabe am Geraet. `chunk` nimmt nur der Reihe nach
an: ein Paket mit Loechern waere nicht im Vorbeigehen zu hashen, und ein
Angreifer koennte ein schon geprueftes Abbild nachtraeglich an einer Stelle
veraendern. `abort` wird abgelehnt, sobald umgeschaltet ist - dort haengt der
Rueckweg am Zustand.

## Wogegen der signierte Bootloader NICHT schuetzt

Das ist der wichtigste Abschnitt dieser Datei.

Der signierte Bootloader verhindert **fremde** Firmware. Er verhindert
nicht eine Firmware, die **wir selbst falsch bauen**. Ein Fehler von uns -
ein Geheimnis, das versehentlich herausgegeben wird, eine Freigabe, die
nicht abgefragt wird, ein falsch gesetztes Recht im Chip - wird ordentlich
unterschrieben und vom Bootloader ohne Zoegern gestartet. Die Signatur sagt
"das kommt von uns", nicht "das ist richtig".

Weiter nicht geschuetzt:

- **Der Schluessel selbst.** Wer ihn hat, baut Firmware, die jedes Geraet
  annimmt. Er liegt auf einem Rechner, und dieser Rechner ist damit Teil
  der Absicherung. Geht er verloren, nimmt kein Geraet mehr ein Update an -
  es gibt keinen zweiten Weg, und das ist Absicht.
- **Alles vor dem Bootloader.** Wird die Auslesesperre des Chips nie
  gesetzt, laesst sich der Bootloader austauschen, und dann prueft niemand
  mehr etwas. **Darum muss der signierte Bootloader frueh da sein:** wird
  die Sperre erst spaeter gesetzt, muesste man sie zum Nachruesten loesen,
  und das loescht den Chip.
- **Die Hardware.** Der SE050 wehrt sich gegen Chipangriffe, der nRF52840
  nicht (Spannungsglitches haben seine Auslesesperre schon ueberwunden -
  `docs/architecture.md`). Ein ausgebauter, aufgeschliffener nRF52840 ist
  kein Fall fuer den Bootloader.
- **Den Nutzer.** Wer sein Geraet freiwillig mit einer Firmware von
  irgendwoher bespielt und die Taste druckt, hat alle Pruefungen auf seiner
  Seite - sie pruefen die Signatur, nicht die Absicht.

Was er schuetzt, und das ist nicht wenig: eine Schadsoftware auf dem
Rechner kann keine eigene Firmware aufspielen. Ohne ihn waere das der
einfachste Angriff auf das ganze Geraet - und zwar der, nach dem alle
anderen Absicherungen nichts mehr wert sind.

## Was geprueft ist und was nicht

Gemessen am 24.10.2026, alles am PC:

| Was | Wie | Ergebnis |
| --- | --- | --- |
| Entscheidungen des Bootloaders | `apps/firmware/test/boot/`, eigener Pruefsatz | 793 Pruefungen, 0 Fehler |
| Abbruch an jeder Stelle | Abbruch nach jedem Stueck und mitten im Schreiben an 32 Stellen | nach jedem Abbruch startet die alte Fassung |
| Tresor unberuehrt | vollstaendiger Durchlauf mit Grenzwache, Tresorbytes vorher/nachher verglichen | gleich, 0 abgelehnte Zugriffe im Durchlauf |
| falsche/fehlende Signatur, altes Abbild, fremdes Brett | je eigene Faelle, mit Zwischenpruefung | alle abgelehnt, jeweils mit dem passenden Grund |
| ed25519 in `tools/sign.py` | offizielle Testvektoren aus RFC 8032 und Vergleich mit `cryptography` | gleiches Ergebnis |
| Werkzeuge | `tools/tests/` | 76 Tests, 0 Fehler |
| Mutationsproben Bootloader | 20 absichtliche Fehler, dazu zwei Kontrollen (eine muss sterben, eine muss leben) | 17 gestorben, 1 unerreichbar (siehe unten), 1 deckte toten Code auf, 1 uebersetzte nicht und beweist darum nichts |
| Mutationsproben Werkzeuge | 10 absichtliche Fehler in `sign.py`, `dfu.py`, `ed25519.py` | alle 10 gestorben |

Drei Proben haben echte Luecken gezeigt und wurden behoben, nicht wegerklaert:

- Immer denselben Zustandsabdruck zu beschreiben blieb unentdeckt - die
  ersten drei Schreibvorgaenge sehen gleich aus, erst der vierte trennt sie.
  Fall ergaenzt.
- Ein gefaelschter Zustandsabdruck mit **richtig nachgerechneter**
  Pruefsumme, aber unbekannter Stufe, wurde nie geprueft: der alte Fall fiel
  schon an der Pruefsumme und misst damit die fruehere Wand. Fall ergaenzt.
- Ein Riegel in `osk_boot_may_write()` gegen den Ueberlauf des Adressraums
  war **toter Code**: die Form des Vergleichs verhindert den Ueberlauf schon,
  und kein Test konnte den Riegel erreichen. Entfernt - toter Code, der wie
  ein Riegel aussieht, ist schlimmer als keiner. Die Eigenschaft selbst wird
  weiter gemessen.

**Nicht geprueft, weil es dafuer kein Mittel gab:**

- **Dieser Teil ist nicht fuer ARM uebersetzt.** Die Pruefungen hier laufen mit
  einem PC-Uebersetzer. Die Firmware als Ganzes ist am 25.10.2026 fuer
  `nrf52840dk/nrf52840` uebersetzt worden (`docs/build.md`, "Der erste Bau fuer
  ARM"), der Bootloader-Teil war dabei nicht mit im Bau.
- **MCUboot lief nie mit.** Alle Einstellungen in
  `apps/firmware/boot/mcuboot.conf` sind nach der Dokumentation des SDK
  geschrieben, nicht nach einer Messung. Ob die Namen in der gebauten
  SDK-Fassung so heissen, sagt der erste Bau.
- **Das Umschalten selbst ist nicht gemessen.** Es macht MCUboot. Gemessen
  ist, was der Zustand davor und danach hergibt.
- **Kein QSPI-Baustein, kein SE050, keine Taste.** Flash, Signaturpruefer,
  Hash und Zaehler sind in den Pruefungen Attrappen. Der Hash ist
  ausdruecklich kein SHA-256: geprueft wird, dass ein falscher Hash zum
  Verwerfen fuehrt, nicht die Hashfunktion.
- **Der Weg ueber USB-HID in `tools/dfu.py` ist nicht gebaut.** Es gab kein
  Geraet. Das Werkzeug sagt das klar und bietet `--manual` und `--dry-run`
  an, statt einen Weg vorzutaeuschen. Gemessen ist der Ablauf gegen eine
  Gegenstelle, die das Geraet nachbildet.
- **Ein Riegel ist unerreichbar und darum ungeprueft:** der Ueberlauf der
  Folgenummer des Zustands nach 4 Milliarden Schreibvorgaengen. Ein Mutant,
  der ihn entfernt, bleibt unentdeckt - erreichen kann ihn kein Test.
- **Die Partitionsgroessen sind Datenblattzahlen.** Ob 960 kB fuer das Abbild
  reichen, ist seit dem 25.10.2026 gemessen: ja, mit Abstand - die Zahl steht
  nur in `docs/build.md`.

## Was noch fehlt

- Zwei Funktionen in `apps/firmware/include/osk_se050.h` (gehoert dem
  Master): `osk_se050_counter_read()` und `osk_se050_counter_raise()`.
  Solange sie fehlen, antwortet `apps/firmware/boot/boot_se050.c` ehrlich
  `OSK_ERR_UNSUPPORTED`, und jede Paketpruefung scheitert - lieber kein
  Update als ein Update ohne Rueckschritt-Sperre.
- ~~Drei Zeilen fuer den Bau: die Partitionsdatei, die MCUboot-Einstellungen
  und der Pfad des MCUboot-Schluessels.~~ **Erledigt am 30.10.2026**: die drei
  stehen in `apps/firmware/CMakeLists.txt`, und der Bau bricht ab, wenn der
  Schluessel fehlt. Geschrieben, nicht uebersetzt - es gibt kein SDK auf
  diesem Rechner. Einzelheiten und was daran gemessen ist:
  `docs/signatur.md`. Was noch fehlt, ist die Uebernahme der Zeilen aus
  `apps/firmware/boot/app_update.conf` nach `apps/firmware/prj.conf`.
- Der Geraetebaum des eigenen Bretts: welcher Anschluss die Taste ist.
- Die Seite der Firmware fuer Befehl 13 - die Schicht hier entscheidet,
  aber den Rahmen in CBOR auspacken muss der Transport.
