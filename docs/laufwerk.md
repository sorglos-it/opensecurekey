# Das Laufwerk

Der Stick bringt seine eigene Verwaltungssoftware mit: ein kleines Laufwerk,
das beim Einstecken erscheint und die drei portablen Programme fuer Windows,
Linux und macOS enthaelt. Einstecken, starten, fertig - keine Installation,
keine Administratorrechte.

**Stand: uebersetzt und am PC geprueft. Kein Rechner hat dieses Laufwerk je
gesehen.** Es gibt keine Platine; alles unten ist gegen eine Attrappe im RAM
gemessen, nicht an einem USB-Anschluss.

## Es nimmt nichts an

Das Wichtigste zuerst, weil es kein Komfortmerkmal ist: **jeder Schreibbefehl
wird abgewiesen.** Ein beschreibbares Laufwerk an einem Sicherheitsschluessel
waere genau das BadUSB-Einfallstor, das dieses Geraet vermeiden soll - ein
Rechner mit Schadsoftware wuerde sonst seine eigene Software auf den Stick
legen, und der Nutzer traegt sie zum naechsten Rechner.

Abgewiesen wird nicht nur `WRITE`, sondern alles, was auf einem anderen Weg
etwas veraendern wuerde:

| Befehl | Was er taete |
| --- | --- |
| `WRITE(6/10/12/16)`, `WRITE AND VERIFY` | Bloecke ueberschreiben |
| `MODE SELECT(6/10)` | das Geraet umstellen, etwa den Schreibschutz |
| `FORMAT UNIT`, `ERASE(6/10/12)` | das Dateisystem loeschen |
| `WRITE BUFFER` | in die Firmware schreiben |
| `WRITE SAME`, `UNMAP`, `SANITIZE` | Bloecke verwerfen |
| `COMPARE AND WRITE` | lesen und schreiben in einem Schritt |
| `LOG SELECT`, `PERSISTENT RESERVE OUT` | Zustand im Geraet aendern |
| `ATA PASS-THROUGH(12/16)` | am SCSI-Satz vorbei direkt auf den Baustein |
| `REASSIGN BLOCKS` | Bloecke umlegen |

Die Antwort ist immer dieselbe: CSW `FAILED`, Sinnesdaten
`DATA PROTECT` / `WRITE PROTECTED` (0x07 / 0x27). Nicht stillschweigend
verschlucken - ein Rechner, der keine Antwort bekommt, wiederholt endlos und
der Nutzer sieht ein haengendes Fenster statt einer Meldung.

Dazu steht das Schreibschutzbit im Kopf von `MODE SENSE(6)` und
`MODE SENSE(10)`. Dort sieht ein Betriebssystem nach, **bevor** es ueberhaupt
einen Schreibbefehl formt. Eins von beidem waere zu wenig: ohne das Bit
meldet Windows „Datentraeger schreibgeschuetzt" erst nach einem Fehlschlag,
ohne die Abweisung wuerde ein Programm, das das Bit uebergeht, durchkommen.

Jeder abgewiesene Versuch wird gezaehlt (`osk_msc_write_attempts()`). Ein
Rechner, der an diesem Laufwerk schreiben will, tut etwas, das der Nutzer
sehen soll.

## Wie der Inhalt hineinkommt

**Nur ueber ein signiertes Firmware-Update.** Das Laufwerk selbst ist
Einbahnstrasse; es gibt keinen Befehl, mit dem ein Rechner etwas darauf
ablegen koennte.

Das Abbild baut `tools/mkimage.py` aus den drei gebauten Programmen:

```powershell
python tools\mkimage.py --windows dist\osk-windows.exe --linux dist\osk-linux --macos dist\osk-macos --out dist\laufwerk.img
```

Heraus kommt eine Datei, die genau so in den Flash geschrieben wird:

```
+--------------------------+ 0
| Kopf, 4 KiB              |  "OSKDRIVE", Fassung, Laenge, SHA-256
+--------------------------+ 4096
| FAT16-Dateisystem        |  Startsektor, 2 Tabellen, Stammverzeichnis, Daten
+--------------------------+
| 0xFF bis zum Sektorende  |
+--------------------------+
```

Der SHA-256 im Kopf deckt das Dateisystem ab. Die Firmware kann damit vor dem
Anbieten pruefen, ob im Flash wirklich das steht, was eingespielt wurde - ein
Update, das mitten im Schreiben abbricht, wird dann gar nicht erst als
Laufwerk gezeigt, statt dem Rechner halbe Dateien anzubieten.

Das Abbild ist **reproduzierbar**: gleiche Programme, gleiche Bytes. Feste
Zeitstempel in den Verzeichniseintraegen, Datentraegernummer aus dem Inhalt.
Ohne das sieht jedes Bauen nach einer Aenderung aus und zwei Abbilder lassen
sich nicht vergleichen.

Das Werkzeug prueft zuerst sich selbst: `python tools\mkimage.py --selftest`
baut ein Abbild, liest es mit einem **eigenen, unabhaengigen** Leser wieder
aus und vergleicht byteweise. Ein Abbild, aus dem die Dateien nicht
zurueckkommen, ist keines - auch wenn alle Zahlen schoen aussehen. Dieselbe
Gegenprobe laeuft bei jedem echten Bau, bevor die Datei geschrieben wird.
Ein fertiges Abbild prueft `--check`.

## Wieviel Platz es kostet

Gerechnet am 24.10.2026. Die Programmgroessen sind **gemessen**, nicht
geschaetzt: `go build -trimpath -ldflags="-s -w"`, ohne cgo, aus
`apps/desktop` fuer die drei Systeme. Die Zahlen des Dateisystems kommen aus
`tools/mkimage.py` und sind am Abbild nachgerechnet, nicht ueberschlagen.

| Posten | Bytes | |
| --- | ---: | --- |
| `OSK-WIN.EXE` | 10 073 600 | Windows, amd64 |
| `OSK-LIN` | 9 748 640 | Linux, amd64 |
| `OSK-MAC` | 9 272 786 | macOS, arm64 |
| `LIESMICH.TXT` | 1 677 | in fuenf Sprachen |
| **Nutzdaten** | **29 096 703** | |
| Verwaltung FAT16 | 246 017 | Startsektor, 2 Tabellen zu 223 Sektoren, Stammverzeichnis 32 Sektoren, Verschnitt |
| **Dateisystem** | **29 342 720** | 57 310 Bloecke zu 512 B |
| Kopf und Auffuellung | 5 120 | |
| **Partition im Flash** | **29 347 840** | **27,99 MiB** |

**Der FAT-Overhead betraegt 251 137 Byte, also 0,86 %.** Er ist so klein, weil
die Zuordnungseinheit 512 Byte gross ist - die kleinste, die es gibt. Das
Werkzeug waehlt sie selbst: es nimmt die kleinste Einheit, mit der die Zahl
der Einheiten noch unter der FAT16-Grenze von 65 524 bleibt. Bei 29 MB sind
das 56 831 Einheiten, knapp darunter. Eine groessere Einheit waere bequemer,
kostet aber Verschnitt, und der Stick hat keinen Platz zu verschenken.

Kein MBR, keine Partitionstabelle: der Startsektor liegt bei Block 0
(„Superfloppy"). Das ist bei Wechselmedien ueblich und spart die Tabelle.

### Bleibt der Tresor bei seinen 28,2 MiB?

Ja. Das Laufwerk fasst die Tresorpartition nicht an - es hat seinen eigenen
Bereich und einen eigenen Lesezugang, der nichts von der Tresorpartition
weiss (`shared/vault-format.md` gilt unveraendert).

| Bereich | Bytes | MiB |
| --- | ---: | ---: |
| Tresor | 29 564 928 | 28,20 |
| Laufwerk | 29 347 840 | 27,99 |
| Zweiter Platz fuers Firmware-Update (MCUboot) | 1 048 576 | 1,00 |
| **belegt** | **59 961 344** | **57,18** |
| **Flash `W25Q512JVEIQ`** | **67 108 864** | **64,00** |
| **frei** | **7 147 520** | **6,82** |

**59,96 MB von 67,11 MB, also 89,3 %. Es bleiben 6,82 MiB Reserve.** Die
Summe bleibt unter 64 MB, und zwar nicht knapp.

Jede Partition belegt ganze Flash-Sektoren zu 4 KiB. Das ist keine
Schoenheit: teilten sich zwei Partitionen einen Sektor, wuerde das Loeschen
der einen die andere mitnehmen.

### Eine Abweichung von `docs/hardware.md`

Dort stehen 27,5 MB fuer die drei Programme (gerechnet am 23.10.2026),
gemessen sind heute **29,10 MB** - 1,6 MB mehr. Die Programme sind seitdem
gewachsen. Das aendert nichts an der Entscheidung fuer 64 MB, verkleinert
aber die Reserve: aus den dort genannten 34,5 MB „frei fuer den Tresor"
werden mit dem fertigen Laufwerk 28,2 MB Tresor plus 6,82 MB Reserve. Wer
den Tresor spaeter vergroessern will, hat weniger Luft als gedacht.

## Was der Rechner sieht

| | |
| --- | --- |
| Blockgroesse | 512 B |
| Bloecke | 57 310 |
| Hersteller / Produkt | `Sorglos` / `OSK Programme` |
| Seriennummer | `0001`, **fuer alle Sticks gleich** |
| Wechselmedium | ja |
| Schreibschutz | ja |

Die Seriennummer ist absichtlich bei allen Geraeten dieselbe. Dieses Laufwerk
ist an jedem fremden Rechner sichtbar, und eine eindeutige Kennung darin
waere eine Spur, die der Nutzer nie gelegt hat.

## Abschalten

Manche Rechner und BIOSe stolpern ueber viele USB-Schnittstellen. Darum laesst
sich das Laufwerk abschalten: `osk_msc_set_enabled(false)`. Danach antwortet es
auf keinen Befehl mehr.

**Das reicht aber nicht, und das ist ein offener Punkt - siehe unten.** Ein
Laufwerk, das da ist und schweigt, bringt den Rechner nicht weiter als eines,
das antwortet: die Schnittstelle steht weiter in der Geraetebeschreibung, und
genau daran scheitern die betroffenen BIOSe. Richtig abgeschaltet ist sie
erst, wenn sie beim Anmelden gar nicht erst genannt wird - und das heisst:
die Einstellung muss den Neustart ueberleben und beim Hochlaufen gelten.

## Aufbau

| Datei | Inhalt |
| --- | --- |
| `apps/firmware/src/classes/transport/usb_msc.h` | die Schnittstelle, mit der Begruendung |
| `apps/firmware/src/classes/transport/usb_msc.c` | Bulk-Only Transport und SCSI-Satz |
| `apps/firmware/test/msc/` | Pruefungen und Fuzzlauf am PC |
| `tools/mkimage.py` | baut und prueft das Abbild |

Das Modul kennt den Flash nicht. Es bekommt beim Einrichten eine Lesefunktion
und die Zahl der Bloecke; dahinter liegt auf dem Geraet der QSPI-Treiber mit
dem Versatz der Laufwerkspartition, im Test eine Attrappe im RAM. Damit laesst
sich der ganze SCSI-Satz ohne Hardware pruefen - und es gibt keinen Weg, auf
dem das Laufwerk aus Versehen in die Tresorpartition sehen koennte.

Der Parser kommt ohne dynamischen Speicher aus: ein Antwortpuffer von 512 B
und ein Blockpuffer, beide auf dem Stapel der Bearbeitung. Jede Laenge wird
geprueft, bevor an der Stelle gelesen wird, auf die sie zeigt - die Angabe,
wie lang der Befehlsblock ist, kommt vom Rechner und ist eine Behauptung.

## Was geprueft ist, und wie

Alles am PC, gegen eine Attrappe. **Kein Rechner hat dieses Laufwerk je
gesehen.**

```powershell
wsl -- sh -c "cd /mnt/d/claude/opensecurekey-msc/apps/firmware/test/msc && make"
```

| Lauf | Ergebnis |
| --- | --- |
| `test_msc` | 101 Faelle, 0 Fehler |
| `test_readonly` | 122 Faelle, 0 Fehler |
| `tools/mkimage.py --selftest` | 23 Faelle, 0 Fehler |

Uebersetzt mit `gcc`, `-Wall -Wextra -Werror -Wconversion -Wsign-conversion`
und den Sanitizern fuer Adressen und undefiniertes Verhalten.

Der Fuzzlauf beschiesst den SCSI-Parser mit erfundenen Befehlen. Er haelt
dabei nach, dass auf jeden gueltigen CBW genau ein CSW folgt, dass nie mehr
Daten hinausgehen als angefordert, dass der Rueckstand zur gesendeten Menge
passt - und am Ende, dass **kein Byte des Laufwerks sich veraendert hat**.

```
make fuzz FUZZ_SECS=20
fuzz_scsi: 66 190 051 Befehle, 50 680 174 beantwortet, 7 609 623 gut,
           43 070 551 abgewiesen, 28 082 110 Byte Daten,
           2 978 757 Datenphasen verworfen,
           17 327 782 Schreibversuche abgewehrt
```

Gemessen am 24.10.2026, Startwert `0x05c51234`, im Arbeitsbaum auf der
lokalen Platte (`D:`), unter WSL. Was die Zahl **nicht** sagt: der Lauf hat
keine Deckungsrueckmeldung, die Eingaben sind gewuerfelt, nicht gezielt
weitergezuechtet. Eine hohe Zahl heisst darum vor allem, dass die meisten
Pfade kurz sind. Aussagekraeftig sind die vier Zusicherungen und die
Verteilung: dass sowohl gute als auch abgewiesene Antworten vorkamen und
wirklich Daten hinausgingen, prueft der Lauf selbst und bricht sonst ab.

## Offene Punkte

1. **`osk_transport_enable()` hat keine Umsetzung.** Die Funktion steht in
   `apps/firmware/include/osk_transport.h`, aber im ganzen `src/`-Baum gibt es
   sie nirgends - nur in den Attrappen zweier Pruefsaetze
   (`test/applets/mocks.c`, `test/ctap/mocks.c`). Dasselbe gilt fuer
   `osk_transport_register()`. Es gibt also noch keine Transportliste; jedes
   Modul meldet sich bei etwas an, das es nur im Test gibt. Das ist nicht
   dieses Strangs Baustelle, aber es muss jemandem gehoeren.
2. **Abschalten braucht mehr als einen Schalter.** `osk_transport_enable()`
   kann im besten Fall einen Transport verstummen lassen. Fuer USB ist das zu
   wenig: eine Schnittstelle verschwindet erst, wenn sie beim Anmelden nicht
   genannt wird. Es fehlt eine Einstellung, die den Neustart ueberlebt, und
   ein Zusammenbauen der Geraetebeschreibung beim Hochlaufen. Ohne das kann
   ein Nutzer, dessen BIOS ueber fuenf USB-Schnittstellen stolpert, sich
   nicht helfen - und genau dieser Nutzer kommt nicht ins
   Verwaltungsprogramm, um es abzuschalten.
3. **Lange Dateinamen gibt es nicht.** Das Abbild fuehrt nur 8.3-Namen. Ein
   VFAT-Parser waere ein zweiter, laengerer Leseweg auf einer Flaeche, die
   jeder Rechner anfasst; vier kurze Namen tun es auch. Sollen die Programme
   einmal `openSecureKey (Windows).exe` heissen, ist das eine Entscheidung
   mit Preis.
4. **Die Programmgroessen sind gewachsen** (siehe oben). Wenn sie weiter
   wachsen, schrumpft die Reserve von 6,82 MiB. Bei etwa 36 MB Programmen
   waere sie aufgebraucht.
