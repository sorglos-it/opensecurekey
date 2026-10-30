# Die Wege hinein und hinaus

Wie Zugangsdaten in dieses Geraet hineinkommen und wie sie wieder
herauskommen. Vier Wege hinein (CXF, CSV, Browser, KeePass), einer hinaus
(CXF), und daneben die Leak-Pruefung.

Stand 30.10.2026. Alles hier ist **auf Stufe 2 von 4**: geschrieben,
uebersetzt, mit Tests gelaufen - aber auf keiner Hardware. Was gegen echte
fremde Dateien gemessen wurde, steht unten mit Datum; was nicht, steht
ebenso da.

---

## 1. Die Spannung, die nicht wegzubauen ist

Dieses Geraet sagt: **Schluessel verlassen den Chip nie.** Ein Ausgang sagt:
**Zugangsdaten koennen das Geraet verlassen.** Beides steht nebeneinander,
und beides ist wahr - weil es zwei verschiedene Dinge sind.

| | Was es ist | Kommt es heraus? |
|---|---|---|
| Ein Kennwort im Tresor | etwas, das der Anwender hineingelegt hat | **Ja** - `reveal` gibt es seit dem ersten Tag heraus, einzeln, mit Tastendruck, und jedes Mal steht es im Geraeteprotokoll |
| Ein Schluessel, im Chip erzeugt | etwas, das der Chip selbst gemacht hat | **Nein** - das Geraet gibt ihn nicht heraus, und der Ausgang macht davon keine Ausnahme |

CXF erfindet also keine neue Oeffnung. Es macht eine sichtbar, die es
gibt - und das ist ein Gewinn, kein Verlust: ein Weg hinaus, den man sieht
und freigeben muss, ist besser als einer, der sich aus `osk get` in einer
Schleife zusammenbauen laesst.

### Die drei, ohne die nichts hinausgeht

1. **PIN.** Nicht ein Merker in diesem Programm, sondern `hello` am Geraet.
   Ein gesperrtes Geraet gibt nichts heraus, und gefragt wird das Geraet.
2. **Tastendruck.** Den verlangt das Geraet je `reveal` selbst. Das Programm
   kann ihn nicht umgehen und versucht es nicht.
3. **Eine Anzeige, die sagt WOHIN.** Das ist die, die es vorher nicht gab.
   `CxfExport` nimmt ohne benanntes Ziel und ohne ausdrueckliches Ja gar
   nichts auf - `error.cxf.noconsent`, bevor der erste Eintrag geholt wird.

Die Bedingung sitzt in `CxfExport` (`classes/exchange_out.go`), **nicht** in
der Oberflaeche. Das ist der Punkt: eine Oberflaeche, die fragt, und eine
Schnittstelle, die ohne Frage liefert, waeren zusammen keine Freigabe. Wer
`/api/export` ohne Ziel aufruft, bekommt dieselbe Absage wie die
Kommandozeile und wie ein Aufruf aus dem Quelltext. Es gibt **keinen**
Parameter, der das ueberspringt - auch keinen fuer Pruefungen.

### Was nicht mitkommt, wird genannt

Eine Sicherung, die stillschweigend die Haelfte auslaesst, ist schlimmer als
keine: der Anwender glaubt dann, er sei umgezogen. Darum gibt `CxfExport`
**immer** einen Bericht zurueck, auch wenn alles gut ging, mit drei Listen:

* **mitgekommen**
* **ungenauer mitgekommen** - etwa ein Wiederherstellungscode, der als Notiz
  herauskommt, oder ein Einmalkennwort, bei dem Takt, Stellen und Verfahren
  nicht gespeichert waren
* **nicht mitgekommen**, je Zeile mit Grund

Zurueckbleiben kann ein Eintrag aus zwei verschiedenen Gruenden, und die
Reihenfolge ist wichtig:

1. Die **Liste** des Geraets sagt, dass er chipgebunden ist (`m` aus
   `list`). Dann wird gar nicht erst gefragt - das erspart Tastendruecke
   fuer Eintraege, die ohnehin nicht herauskommen.
2. Die **Absage** des Geraets auf `reveal` (`OSK_ERR_DENIED`). Das ist das
   letzte Wort, und es gehoert dem Geraet: *"Das Geraet entscheidet, nie das
   Programm"* (`shared/protocol.md`). Eine Absage bricht den Export **nicht
   ab** - sonst haette der Anwender eine halbe Datei und keine Liste.

Dazu kommen Arten, fuer die CXF 1.0 keinen Platz hat, und Arten, die
einzelne Felder verlangen, die dieser Tresor nicht haelt (siehe die Tabelle
in Abschnitt 3).

---

## 2. Das Format

**CXF, Credential Exchange Format der FIDO Alliance**, Fassung "Proposed
Standard" vom 14.08.2025, mit der Nachtragsliste vom 09.03.2026. Gebaut nach
der Beschreibung, kein fremdes Paket - `apps/desktop/go.mod` hat weiterhin
keine Abhaengigkeiten.

Entschieden wurde dabei:

* **Base64 in der Adressschreibweise, ohne Auffuellzeichen.** Die
  Beschreibung sagt nur "RFC 4648"; das Beispiel der Nachtragsliste enthaelt
  kein einziges `=`, auch nicht dort, wo die Laenge eines verlangte
  (`"MTIz"` sind drei Byte). Geschrieben wird darum ohne, **gelesen** werden
  beide. Das normale Alphabet (`+` und `/`) wird abgewiesen - es ist ein
  anderes Alphabet.
* **Unbekannte Felder in einer bekannten Art sind ein Fehler.** Wenn dieses
  Programm eine Art auseinandernimmt und dabei ein Feld findet, das es nicht
  kennt, kennt es die Art nicht vollstaendig - dann soll es die Datei nicht
  halb verstehen.
* **Unbekannte Arten werden durchgetragen, nicht weggeworfen.** Adresse,
  Karte, Ausweis, Pass, Name, Datei und alles, was ein anderes Programm
  erweitert hat, bleibt als rohes JSON liegen und kommt unveraendert wieder
  heraus. Ein Rundlauf durch dieses Programm verliert keine Zugangsdaten,
  auch keine, die der Tresor nicht halten kann.
* **Die Werte eines Feldes sind immer Text**, auch Zahl und Wahrheitswert -
  im Beispiel der Beschreibung steht der Wahrheitswert als `"false"` in
  Anfuehrungszeichen.

### CXF traegt kein Siegel

Das ist eine Eigenschaft **des Formats**, nicht dieses Lesers, und sie
gehoert ausgesprochen: eine CXF-Datei, in der jemand ein Kennwort aendert,
ist von einer echten nicht zu unterscheiden - von keinem Programm. "Eine
verfaelschte Importdatei wird abgewiesen" heisst bei CXF darum: kaputt,
abgeschnitten, ueberlang, zu tief geschachtelt, falsche Fassung, falsches
Alphabet, Steuerzeichen, zwei Dateien hintereinander. Nicht: ein geaendertes
Kennwort.

**Wer ein Siegel will, verschluesselt die Datei:** `osk encrypt` (siehe
`docs/dateiwerkzeug.md`). Dann traegt jeder Abschnitt eines, und der
Schluessel liegt im Chip.

Bei KeePass ist das anders - dort wird wirklich gesiegelt, siehe Abschnitt 4.

---

## 3. Was wohin passt

Der Tresor haelt **je Eintrag ein Geheimnis**. CXF kennt Eintraege mit
mehreren Zugangsdaten und mit einzelnen Feldern. Daraus folgt die Tabelle;
alles, was nicht glatt uebergeht, steht im Bericht statt in der Datei.

### Hinaus

| Eintragsart | CXF | Anmerkung |
|---|---|---|
| Login | `basic-auth` | |
| Einmalkennwort | `totp` | Takt/Stellen/Verfahren nur, wenn sie als `otpauth://` gespeichert waren; sonst die Werte aus RFC 6238 **und eine Zeile im Bericht** |
| SSH-Schluessel | `ssh-key` | als PKCS#8-DER, so wie das Beispiel der Beschreibung es zeigt - nicht als `BEGIN OPENSSH PRIVATE KEY` |
| Programmschluessel | `api-key` | |
| Notiz | `note` | |
| Wiederherstellung, Saat, Lizenz | `note` | **ungenauer**: die Art geht verloren |
| Karte, Ausweis, Funknetz | — | brauchen einzelne Felder (Nummer, Pruefziffer, Sicherheitsart), die der Tresor nicht haelt. Sie aus dem Geheimnis zu **raten** waere erfunden, und erfundene Felder in einer Austauschdatei sind schlimmer als eine Zeile im Bericht |
| HOTP, Zertifikat, PGP-Schluessel | — | CXF 1.0 hat dafuer keine Art |
| Datei | — | der Inhalt geht nur ueber das zweite Verfahren (CXP), das hier nicht gebaut ist |

### Hinein

Gelesen und abgelegt werden `basic-auth`, `generated-password`, `note`,
`api-key`, `totp` und `ssh-key`. Alles andere wird **genannt** und nicht
stillschweigend verschluckt.

---

## 4. Die offene Luecke: Passkeys

**CXF-Passkeys koennen gelesen und wieder geschrieben, aber nicht abgelegt
werden.** Das ist kein Versehen und auch keine Entscheidung dieses Strangs,
sondern ein Befund:

Die Eintragsarten in `classes/items.go` kommen aus
`apps/firmware/include/osk_store.h` (`osk_item_type_t`). Dort gibt es
**keine Art fuer Passkeys**. Eine hier zu erfinden hiesse, eine Zahl an
einer zweiten Stelle zu fuehren - genau das, was dieses Projekt nicht tut
("eine Zahl nur an einer Stelle"), und der Fehler faellt erst am Geraet auf.

Was daraus folgt:

* Beim **Einlesen** wird ein Passkey mit `cxf.in.nopasskey` genannt, mit
  seinem `rpId`, damit der Anwender weiss, welcher Zugang fehlt.
* Beim **Herausschreiben** kommt kein Passkey vor - der Tresor haelt keine.
* Ein Rundlauf durch dieses Programm **verliert** einen Passkey nicht: er
  wird vollstaendig gelesen und unveraendert wieder geschrieben, samt
  `fido2Extensions`.

Das ist **die Luecke, die noch offen ist**. Sie zu schliessen heisst: eine
Eintragsart in der Firmware, und dann hier. Nicht umgekehrt.

---

## 5. Die Importwege

### CSV und Browser

Erkannt wird **nach Spaltennamen**, nicht je Programm. Jeder Browser und
jede Verwaltung schreibt eine eigene Spaltenfolge, aber fast alle benutzen
dieselben Woerter: `url`, `username`, `password`, `name`, `notes`,
`login_totp`. Eine Namenstabelle deckt damit Chrome, Edge, Firefox, Safari,
Bitwarden, KeePass und 1Password ab und bricht nicht, wenn eines davon die
Reihenfolge aendert. Eine Liste "wenn Chrome, dann Spalte 4" waere am Tag
der naechsten Fassung falsch, ohne dass es auffaellt.

**Geraten wird nichts.** Findet sich keine Kennwortspalte, sagt das Programm,
welche Spalten dastanden, und tut nichts - eine geratene Kennwortspalte waere
ein Tresor voll Unsinn, und die Quelldatei hat der Anwender danach vielleicht
schon geloescht. Spalten, die nicht verwendet wurden, stehen im Bericht.

Ein Einmalkennwort in derselben Zeile wird ein **eigener Eintrag**, denn der
Tresor haelt je Eintrag ein Geheimnis.

### KeePass (KDBX 3.0, 3.1 und 4)

Nach der Beschreibung gebaut. Die Teile, die dafuer in der
Standardbibliothek fehlen, sind daneben entstanden - jeder mit den
Testwerten **seiner eigenen Beschreibung**:

| Baustein | Datei | Gemessen gegen |
|---|---|---|
| BLAKE2b | `classes/blake2b.go` | RFC 7693 Anhang A und die offizielle Werteliste |
| Argon2d/i/id | `classes/argon2.go` | RFC 9106 Abschnitt 5, alle drei Spielarten |
| Salsa20 | `classes/salsa20.go` | ECRYPT-Testsatz 6, Vektor 0 |

Das ist nicht Zierde. **Der Salsa20-Test war beim Bauen rot**: in der
Rundenfunktion waren zwei Woerter vertauscht. Das Verfahren lief, lieferte
einen Strom und entschluesselte jede KeePass-Datei zu Unsinn. Im Quelltext
sah es richtig aus, und kein Kommentar haette es gezeigt. Ohne einen Wert
von aussen waere es nicht aufgefallen.

Gelesen wird: AES-256-CBC und ChaCha20 als Verfahren, AES-KDF, Argon2d und
Argon2id als Schluesselableitung, gzip, Salsa20 und ChaCha20 zum Verdecken
der Kennwoerter im XML.

**Nicht** gelesen wird, jeweils mit klarer Meldung statt eines Fehlschlags:
Twofish, RC4, Schluesseldateien, der alte Behaelter KDB (Fassung 1 und 2)
und Argon2 in der Fassung 0x10.

Dabei behandelt:

* **Der Verlauf.** KeePass legt alte Fassungen eines Eintrags unter
  `History` ab. Sie werden **nicht** uebernommen - sonst laegen alle alten
  Kennwoerter mit im Tresor. Die Falle: ihre verdeckten Werte zaehlen im
  Schluesselstrom **mit**. Wer sie beim Lesen ueberspringt, entschluesselt
  alles danach falsch, und zwar unauffaellig - das naechste Kennwort ist
  einfach Unsinn.
* **Der Papierkorb.** Was dort liegt, hat der Anwender geloescht. Es wird
  genannt und nicht hereingeholt; ein Umzug, der die geloeschten Zugaenge
  wiederbringt, ist kein Umzug.
* **Notizen.** Hat ein Eintrag Kennwort *und* Notiz, kommt das Kennwort mit
  und die Notiz steht im Bericht. Sie anzuhaengen waere ein Kennwort, das
  beim naechsten Anmelden falsch ist; sie als zweiten Eintrag anzulegen
  wuerde den Tresor verdoppeln.
* **Anhaenge** kommen nicht mit und werden gezaehlt.

### Die Echtheit einer KDBX-Datei - und ein gefundener Fehler

Hier wird wirklich gesiegelt, anders als bei CXF:

| Fassung | Was den Dateikopf deckt | Was die Daten deckt |
|---|---|---|
| 4 | SHA-256 **und** HMAC, vor dem Entschluesseln | HMAC je Abschnitt |
| 3.1 | SHA-256 in `Meta/HeaderHash` - **im verschluesselten XML**, also erst nach dem Entschluesseln zu haben | SHA-256 je Abschnitt |
| 3.0 | **nichts** | SHA-256 je Abschnitt |

Dass die Fassung 3.1 ihre Kopfpruefung im XML traegt, ist leicht zu
uebersehen, und genau das war hier zuerst der Fall. Der Test, der jede
Stelle der Datei einzeln verbiegt, hat es gefunden: ein Bit auf Stelle 145
wurde **angenommen**. Dort steht der Schluessel, mit dem die Kennwoerter im
XML verdeckt sind. Die Folge waere eine Datei gewesen, die sich oeffnen
laesst, deren Eintraege alle dastehen - und deren Kennwoerter still falsch
sind. Jetzt wird der Wert geprueft.

Die Fassung **3.0** kennt ihn noch nicht. Sie wird gelesen, und der fehlende
Schutz wird gesagt (`kdbx.warn.noheaderhash`). Ihn fuer die ganze Fassung 3
zu verlangen hiesse, alte Datenbanken abzuweisen; ihn zu uebergehen hiesse,
einen Tresor mit moeglicherweise falschen Kennwoertern fuer geprueft
auszugeben. Dass es diesen Fall ueberhaupt gibt, hat eine echte Datei
gezeigt - siehe Abschnitt 7.

### Eine fremde Datei ist eine fremde Eingabe

Der Dateikopf einer KDBX-Datei ist ein Baukasten: welches Verfahren, wie
viel Speicher, wie viele Durchgaenge, wie lang jeder Block. **Jede dieser
Zahlen ist eine Angabe des Angreifers.** Darum:

* Obergrenzen fuer Argon2 (1 GiB Speicher, 16384 Durchgaenge, 1024 Spuren)
  und fuer AES-KDF (100 Millionen Durchgaenge). Eine Datei, die vier
  Terabyte verlangt, wird **abgewiesen, nicht gerechnet** - ohne das waere
  ein Dateikopf von zwei Kilobyte der ganze Angriff.
* Eine Obergrenze fuer das entpackte XML (256 MiB). Ein Gigabyte aus zwei
  Kilobyte gzip ist eine bekannte Falle.
* Eine Obergrenze fuer die Schachtelung der Gruppen.
* Jede Laenge wird gegen das geprueft, was wirklich noch da ist.

---

## 6. Die Leak-Pruefung

Ist dieses Kennwort in einer bekannten Sammlung gestohlener Zugangsdaten
aufgetaucht?

**Das Kennwort verlaesst den Rechner nicht.** Hinaus gehen die ersten fuenf
Zeichen des SHA-1-Hashwerts, also 20 Bit. Zurueck kommt die Liste aller
Hashwerte mit diesem Anfang - einige hundert bis einige tausend. Darin wird
**lokal** gesucht. Die Gegenseite erfaehrt, dass jemand ein Kennwort aus
dieser Gruppe geprueft hat, und nicht welches. Das ist k-Anonymitaet; das k
ist die Groesse der Gruppe, und sie wird angezeigt.

Wer stattdessen den ganzen Hashwert schickt, hat genau das getan, wovor das
Geraet schuetzen soll. Es ist **eine Zeile Unterschied**, und beide sehen im
Quelltext richtig aus. Der Nachweis ist darum kein Blick in den Quelltext:
`tests/leak_test.go` stellt einen eigenen Lauscher hin - absichtlich keinen
HTTP-Server, sondern einen, der die **rohen Byte** aus der Leitung behaelt,
so wie ein Mitleser sie saehe. Danach wird in diesen Byte gesucht:

* das Kennwort darf nicht vorkommen, in keiner Schreibweise
* der ganze Hashwert nicht
* der Rest des Hashwerts nicht - **auch nicht die ersten acht Zeichen
  davon**, denn schon die machen die Gruppe so klein, dass die Zusage nichts
  mehr wert ist
* und nach dem Kopf darf kein Rumpf stehen

Der Hashwert wird im Test selbst gerechnet, mit `crypto/sha1`. Ihn von
`leak.go` rechnen zu lassen hiesse, die Behauptung mit sich selbst zu
pruefen. Gegenprobe gemacht: mit absichtlich ganzem Hashwert schlaegt der
Test an.

### Zustimmung

Der Plan sagt: **kein Server, keine Wolke, keine Telemetrie.** Dies ist die
einzige Stelle im Programm, an der eine Anfrage nach draussen geht, und sie
geht nur mit ausdruecklicher Zustimmung - jedes Mal oder dauerhaft
eingeschaltet, nie stillschweigend. Die Bedingung sitzt in
`LeakChecker.Check`, nicht in der Oberflaeche: ohne Zustimmung geht nicht
einmal die Anfrage mit den fuenf Zeichen hinaus. Gemessen wird auch das am
Lauscher - er darf keine einzige Verbindung gesehen haben.

Vor der Frage steht, **was** hinausgeht: nicht "wir pruefen sicher", sondern
die Zahl der Zeichen und die Adresse. Und danach steht da, was wirklich
hinausgegangen ist.

Ein Fehlschlag ist **nie** "nicht betroffen". Ein toter Dienst, eine
unlesbare Antwort, eine leere Gruppe - alles das gibt eine Meldung. Ein
stiller Fehlschlag saehe wie ein gutes Ergebnis aus.

"Nicht gefunden" heisst ausserdem nicht "sicher", nur "nicht in dieser
Sammlung". Das steht so auch im Text, den der Anwender liest.

---

## 7. Was gemessen wurde - und was nicht

### Die fremde Gegenprobe fuer CXF

`tests/vektoren/cxf-fido-beispiel.json` ist **die Beispieldatei aus der
Nachtragsliste der FIDO Alliance** vom 09.03.2026. Die Zahlen und Namen
darin stammen nicht aus diesem Programm. Geprueft wird daran Feld fuer Feld:
14 Eintraege, 15 Zugangsdaten, alle 15 vorkommenden Arten, die Kennungen als
Rohdaten, `algorithm: "sha256"` klein geschrieben, `issuer: "GitHub"` (die
Nachtragsliste hat das von Google geaendert), die durchgetragenen
`fido2Extensions`.

Ein **ausfuehrbares** fremdes Gegenstueck - ein anderes Programm, das diese
Datei schreibt oder liest - gab es nicht zu finden: CXF ist seit 8/2025
Proposed Standard, und die Verwaltungen liefern noch nicht aus. **Dieser
Beleg fehlt.** Was der eigene dann wert ist: die Datei der Beschreibung wird
vollstaendig gelesen, und was dieses Programm schreibt, erfuellt dieselbe
Beschreibung Feld fuer Feld - aber dass ein anderes Programm es auch
annimmt, ist **nicht gemessen**. Das ist die Grenze, und sie faellt erst,
wenn es ein zweites Programm gibt.

### Die fremde Gegenprobe fuer KeePass

Hier gab es eine, und sie war die wertvollste Messung dieses Strangs. Am
30.10.2026 gelesen, mit Dateien, die **KeePassXC geschrieben hat**:

| Datei | Fassung | Kennwort | Ergebnis |
|---|---|---|---|
| `NewDatabase.kdbx` | 3.1, AES-KDF, AES-CBC, Salsa20 | `a` | `Sample Entry` / `Password`, `Subgroup Entry` / `SecurePassword` |
| `Format300.kdbx` | 3.0 | `a` | `Sample Entry` / `Password` |
| `Format400.kdbx` | 4, Argon2 | `t` | `Format400` / `Format400` |

Alle drei mit genau den Werten, die KeePassXC in seinen **eigenen**
Pruefungen erwartet. Dazu 201 einzeln verbogene Stellen je Datei - alle
abgewiesen.

Diese Dateien liegen **nicht im Repo**: sie gehoeren KeePassXC und stehen
unter der GPL, dieses Projekt ist MIT. Die Pruefungen im Repo bauen ihre
Dateien darum selbst.

**Was das heisst, ohne Beschoenigung:** ein Leser, der gegen den eigenen
Schreiber geprueft wird, beweist nur, dass beide dieselbe Vorstellung vom
Aufbau haben. Dass die Vorstellung **stimmt**, beweist der Lauf gegen die
fremden Dateien, und nur er. Die Pruefungen im Repo halten fest, dass es so
bleibt; die Bausteine darunter haengen an den Testwerten ihrer eigenen
Beschreibung.

Wer das nachstellen will, laedt die drei Dateien aus
`keepassxreboot/keepassxc`, Zweig `develop`, Ordner `tests/data/`, in einen
leeren Ordner und ruft `osk import --format keepass --in <datei>` auf.

### Fuzzing

Die drei Leser fremder Dateien haben Fuzzer (`classes/exchange_fuzz_test.go`).
Am 30.10.2026 je 100 Sekunden gelaufen, zusammen rund 59 Millionen
Durchlaeufe, **kein Fund**. Als Fehler zaehlt dabei nicht, dass ein Fehler
zurueckkommt - das ist die richtige Antwort auf eine kaputte Datei -,
sondern ein Absturz, ein Eintrag, den das Geraet abweisen wuerde, ein
Eintrag ohne Geheimnis und ein stillschweigend abgeschnittenes Geheimnis.

Laenger suchen:

```powershell
go test ./classes/ -fuzz FuzzKdbxImport -fuzztime 10m
```

### Was nicht gemessen ist

* **Nichts davon lief auf Hardware.** Der SE051C2 ist bestellt und nicht
  geliefert. Alle Messungen laufen gegen die Attrappe in `tests/simulator`.
* Dass ein **anderes Programm** eine hier geschriebene CXF-Datei annimmt
  (siehe oben).
* Die Oberflaeche wurde am 30.10.2026 im Browser aufgerufen und bedient
  (die Seite laedt, das Kennwortfeld erscheint bei KeePass); ein Export per
  Mausklick wurde **nicht** durchgespielt.
* Twofish, Schluesseldateien, KDB, CXP.

---

## 8. Aufrufe

```powershell
osk export --to "USB-Stick E:" --out sicherung.json
```

Zeigt das Ziel, fragt, und schreibt erst dann. Der Bericht geht nach
stderr - mit leerem `--out` laeuft die Datei durch die Leitung, und ein
Bericht darin machte sie unlesbar.

```powershell
osk import --format cxf --in sicherung.json
```

```powershell
osk import --format csv --in chrome-passwoerter.csv
```

```powershell
osk import --format keepass --in datenbank.kdbx
```

Das Kennwort der KeePass-Datei wird von der Standardeingabe gelesen und
steht nie in einem Aufrufparameter - der waere in der Prozessliste zu sehen.

```powershell
osk leaks --all
```

Jeder dieser Wege zeigt erst, was passiert, und fragt dann. Das Format wird
immer genannt; geraten wird es nicht.

In der Oberflaeche liegt dasselbe unter **Hinein und hinaus**.
