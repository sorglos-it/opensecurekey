# Hardware

Stand 23.10.2026. Teile auf Papier ausgewählt und gegen die Datenblätter
geprüft. **Nichts gebaut, nichts gemessen.** Jede Zahl in den Einzeldokumenten
hat ihre Quelle dabei; Unbelegtes ist als `ungeprueft` gekennzeichnet.

| Dokument | Inhalt |
| --- | --- |
| [parts.md](parts.md) | Stückliste mit Bestellnummern, Preisen und Begründungen |
| [power-budget.md](power-budget.md) | Energiebudget, Akkugröße, Laufzeit |
| [../hardware/schematic.md](../hardware/schematic.md) | Schaltplan als Netzliste, Pin für Pin |
| [../hardware/pinout.md](../hardware/pinout.md) | Pinbelegung, gegen den nRF52840 geprüft |
| [../hardware/layout-notes.md](../hardware/layout-notes.md) | Bauform und Layout-Vorgaben |

## Was sich gegenüber dem Entwurf geändert hat

Sechs Punkte, alle mit Datenblattbeleg in den Einzeldokumenten:

| Entwurf | Jetzt | Grund |
| --- | --- | --- |
| `SE050C2HQ1` oder die E-Variante | **`SE051C2HQ1/Z01XDZ`** | SE050E2 hat **kein RSA**; SE051C2 hat secp256k1, Ed25519 **und** RSA, dazu 104 statt 50 kB Speicher, und ist lagernd |
| Teststand `OM-SE050ARD-E` | **MIKROE-5392** | trägt den Zielchip SE051C2 statt der falschen Variante, und kostet weniger als die Hälfte |
| `MCP73831` | **`MCP73832`** | der Statusausgang ist an einem GPIO zuverlässig lesbar |
| `DW01A` + `FS8205` | **kein Schutz-IC**, dafür Verpolschutz | käufliche Zellen haben die Schutzschaltung schon; der DW01A gäbe mit dem FS8205 ohnehin erst bei etwa 4,8 A Überstromschutz |
| LiPo 100 mAh | **150 mAh (Adafruit 1317)** | dünnste belegte Zelle (3,8 mm), einzige mit vollem Herstellerdatenblatt |
| OLED 0,96" | **0,71" (CFAL4864A-071BW)** | 13,9 × 22 × 1,26 mm statt 29 × 27 × 6,2 mm |

Dazu zwei Berichtigungen an Einzelangaben:

- **Dem nRF52840-DK liegt eine NFC-Antenne bei** (Nordic, Kit-Inhalt). Der
  Posten „Antenne dazukaufen, weil das DK keine hat" entfällt.
- Fünf der genannten Akku-Bestellnummern stimmten nicht (Adafruit 4236 hat
  420 statt 105 mAh, SparkFun PRT-13852 ist die 40-mAh-Zelle, und so weiter) —
  berichtigt in [parts.md](parts.md).

## Die drei Befunde, die den Entwurf tragen

### 1. Die Variantenfrage des Sicherheitschips ist entschieden

Gefordert waren secp256k1 (Geldbörse), Ed25519 (SSH) und RSA (PIV, OpenPGP).
**Nur die C-Varianten haben alle drei** — die neuere E-Variante hat überhaupt
kein RSA, obwohl sie sonst die modernere ist. Belegt aus AN12436 Rev. 2.4
Tabelle 1 und AN12973 Rev. 2.0 Tabelle 3, zeichengenau gegen die Kopfzeilen
geprüft.

**Mit einem Nebeneffekt, der ins Bedrohungsmodell gehört:** AN12973 Rev. 2.0
Tabelle 6 veröffentlicht die **Werks-SCP03-Schlüssel im Klartext**. Mit
Werkseinstellung ist die I2C-Verbindung also nicht geschützt — die
Erstinbetriebnahme **muss** sie ersetzen.

### 2. Die Versorgung hat eine Falle, die Reichweite kostet

Der Rechenkern darf nicht an `VDD` (höchstens 3,6 V), sondern an `VDDH`
(2,5–5,5 V) — dann braucht es **keinen externen Spannungsregler**, und Akku wie
USB-5 V passen beide dort hin.

Aber: Aus dem intern erzeugten `VDD` dürfen nur **25 mA** nach außen fließen,
und bei Sendeleistung über 4 dBm nur **5 mA**. Anzeige (25 mA), Flash (20 mA)
und Sicherheitschip (14,4 mA) sprengen das einzeln schon. **Folge: die
Sendeleistung wird auf höchstens 4 dBm begrenzt, und Lastschalter sorgen dafür,
dass nie zwei große Verbraucher gleichzeitig laufen.** Das kostet Funkreichweite
und spart einen zusätzlichen Wandler. Die Alternativen stehen in
[power-budget.md](power-budget.md).

### 3. Entschieden: Stufe 1 ohne Fingerabdrucksensor

**Der Freigabeweg ist Taster + PIN. Ein Fingerabdrucksensor wird in Stufe 1
nicht bestückt.**

**Der tragende Grund: Bei FIDO2 ersetzt die Biometrie die PIN — sie ist kein
zusätzlicher Faktor.** Das Gerät wird durch einen Sensor also nicht sicherer,
sondern bequemer. Wer den Finger hat, braucht die PIN nicht; wer ihn nicht
hat, nimmt die PIN. Es gibt keine Stelle, an der beide zusammen gefordert
werden. Darum ist der Verzicht **keine Sparmaßnahme**, sondern der Verzicht auf
Bequemlichkeit zugunsten von Platz, Strom und Angriffsfläche.

Dazu kam die Beschaffungslage: **kein geprüfter Kandidat erfüllt alle vier
Anforderungen** (Vergleich im Sensor, Kleinmenge, offene Beschreibung, klein
genug).

- **FPC BM-Lite** wäre technisch die erste Wahl: 15 × 15 × 2,8 mm, 12 mA bei
  der Aufnahme, 13 µA im Schlaf, bei Mouser lagernd. Aber der
  **Host-Befehlssatz ist „proprietary"** — für ein MIT-Projekt ein echtes
  Problem.
- **Grow R503-M22** hat ein vollständig offenes Protokoll und 2 µA
  Bereitschaft, ist aber **15 mm dick** und liefert auf Befehl auch Rohbilder.
- **Kein einziges offenes FIDO-Projekt** (Nitrokey 3, Solo2, OpenSK) hat
  überhaupt einen Fingerabdrucksensor. Es gibt keinen erprobten Weg.

**Was die Platine trotzdem vorsieht:** das Stiftfeld J5 und seine vier
Leitungen werden verlegt, aber nicht bestückt. Stufe 2 kann den Sensor dann
nachrüsten, **ohne dass eine neue Platine nötig wird**.

**Was der Verzicht einbringt:** Bauhöhe und Seitenaufteilung richten sich nicht
mehr nach dem 2,8 mm dicken Sensor. Der freie Platz im mittleren Drittel geht
in den Abstand zwischen Akku und NFC-Antenne — siehe
[../hardware/layout-notes.md](../hardware/layout-notes.md).

## Teststand (jetzt zu bestellen)

| # | Teil | Bestellnummer | Preis | Warum |
| --- | --- | --- | --- | --- |
| 1 | nRF52840-DK | PCA10056 | `ungeprueft` | Programmieradapter an Bord, NFC-Antenne **liegt bei**, alle Pins frei |
| 2 | SE051 Plug & Trust Click | Mouser 932-MIKROE-5392 | ~25 USD | **genau der Zielchip** SE051C2, I2C, 3,3 V |
| 3 | OLED 0,96" mit benanntem SSD1306 | Crystalfontz CFAL12864N-A-B4 | 7,51 USD | Controller ist belegt, anders als bei Fertigmodulen |
| 4 | Logikanalysator 8 Kanal USB | — | `ungeprueft` | um zu belegen, dass die I2C-Leitung verschlüsselt ist |
| 5 | Steckbrett, Drähte, Kabel | — | — | — |

Grob **50–80 €** statt der im Entwurf geschätzten 190–220 € — weil das richtige
Sicherheitschip-Board ein Viertel des NXP-Kits kostet und die NFC-Antenne
beiliegt. Alle Preise `ungeprueft`.

**Kein Fingerabdruck-Entwicklungskit bestellen** — das wären 160–178 USD für
etwas, das in Stufe 1 nicht verbaut wird. Die Zielplatinen-Teile erst nach dem
Teststand.

## Zielplatine (später)

Vollständig mit Bestellnummern in [parts.md](parts.md). Kurzfassung:

- **U1** Raytac `MDBT50Q-1MV2` — nRF52840 im vorzertifizierten Modul,
  15,5 × 10,5 × 2,05 mm, **FCC SH6MDBT50Q, IC 8017A-MDBT50Q**, NFC-Pins
  herausgeführt, 6,15 USD bei DigiKey
- **U2** `SE051C2HQ1/Z01XDZ` — HX2QFN20, 3 × 3 mm
- **U3** `W25Q512JVEIQ` — 64 MByte, 3-V-Fassung (nicht die 1,8-V-Fassung)
- **U4** `MCP73832T-2ACI/OT` + diskrete Lastumschaltung
- **DS1** `CFAL4864A-071BW` — 0,71", SSD1306
- **J1** `GCT USB4085-GF-A` + 2 × 5,1 kΩ an CC1/CC2 + `USBLC6-2SC6`
- Akku `Adafruit 1317` (150 mAh) mit eingebauter Schutzschaltung
- `EVQP7A01P` Taster, `Everlight 19-337C` RGB-LED
- NFC-Antenne als Leiterbahn, differentiell, abgestimmt auf 13,56 MHz

**Zur Funkzulassung:** Raytac liefert für CE/RED einen **Testbericht**, keine
Baumusterprüfbescheinigung. Die Modulzulassung erspart die Funkmessungen; die
**EU-Konformitätserklärung für das Endgerät muss selbst ausgestellt werden**,
und das Endprodukt braucht die vorgeschriebene Kennzeichnung.

Die Bluetooth-Fassung ist laut Herstellerdokument **5.2**; Händler werben mit
5.4 — verbindlich ist Raytac.

## Laufzeit

Mit 150 mAh, Lastschalter für die Anzeige und Bluetooth auf Abruf:
**etwa 6,8 Monate** bei zehn Benutzungen am Tag — und etwa 8,4 Monate, falls
die Anzeige nur ihren Typ.-Wert von 12 mA zieht. Gerechnet, nicht gemessen;
Rechenweg und Annahmen in [power-budget.md](power-budget.md).

**Die Laufzeit hängt nicht an der Akkugröße, sondern an drei Entscheidungen,
die nichts kosten:** Lastschalter für die Anzeige (Faktor 3), Wecken per
GPIOTE-PORT statt Event (Faktor 7 im Weckpfad), Bluetooth auf Abruf statt
dauernd (Faktor 1,5).

Der Wegfall des Fingerabdrucksensors bringt dabei nur **etwa fünf Tage** — er
war energetisch nie das Problem, solange er abschaltbar gewesen wäre.

Der Akku ist ein Verschleißteil. Bei 300 belegten Ladezyklen und einem
Nachladen je Woche sind das etwa sechs Jahre — er sollte als Verschleißteil
benannt oder austauschbar sein. **Noch zu entscheiden.**

## Die Entscheidungen

### Sendeleistung höchstens 4 dBm, kein zweiter Spannungswandler

Damit bleibt die 25-mA-Grenze aus Befund 2 gültig, und die Nebenverbraucher
dürfen am internen `VDD` hängen. Ein Schlüssel am Schlüsselbund redet mit einem
Handy in Armlänge, nicht durch zwei Wände; ein Wandler nur für Reichweite, die
niemand braucht, wäre Vorrat.

**Bedingung, die eingehalten werden muss:** Die Firmware begrenzt `TXPOWER` auf
höchstens +4 dBm, und die Lastschalter Q4/Q5 sind Pflicht. Über 4 dBm bleiben
nur 5 mA — damit ließe sich kein einziger Nebenverbraucher mehr betreiben.

### Akku und NFC-Antenne: Trennung und Ferritfolie

**Beides zusammen.** Die Platine wird **70 statt 62 mm** lang, damit Akku und
Antenne in verschiedenen Dritteln liegen, **und** eine Ferritfolie entkoppelt
die Zelle. So geht jedes Handy mit NFC und Akku vor.

NFC erst in Stufe 2 wäre falsch: der Akku ist überhaupt nur deshalb im Gerät,
weil NFC und Bluetooth ohne Rechner laufen sollen.

**Wird das Gehäuse dadurch zu groß, weicht die Kapazität, nicht der Abstand** —
dann die schmale Zelle (BerryBase, 100 mAh, 12 mm breit) und etwa 4,5 statt 6,7
Monate Laufzeit.

### Ladungspumpen-Kondensatoren der Anzeige: keine nötig

Am Datenblatt geklärt (Crystalfontz CFAL4864A-071BW, Ausgabe 2017-07-05,
Abschn. 6). Die zwölf Pins des Steckverbinders sind GND, D2, D1, D0, D/C#,
/RES#, CS#, BS1, BS0, VDD, VBAT, GND — **es gibt keine Pins `C1P/C1N/C2P/C2N`
und keinen `VCC`-Pin.** Die Ladungspumpe sitzt also auf dem Modul und erzeugt
die Panelspannung selbst aus `VBAT`; bestätigt durch die Strommessung, die mit
`VCC = 7,5 V` angegeben ist, obwohl `VCC` nirgends herausgeführt ist.

**Auf unserer Platine ist nichts nötig.** Nebenbei belegt: der Strom ist
**typisch 12 mA, höchstens 25 mA** — der obere Wert, mit dem das Energiebudget
rechnet, ist also die echte Obergrenze.

### Fingerabdrucksensor: Stufe 1 ohne, Stufe 2 nachrüstbar

Entschieden, Begründung unter Befund 3. Die Platine verlegt das Stiftfeld J5
und seine vier Leitungen, **bestückt sie aber nicht**. Das Gehäuse bekommt an
der Stelle eine Aussparung mit Blindstück — so ist für Stufe 2 nur ein
Gehäuseteil zu ändern, nicht die Platine.

## Alle Entscheidungen sind gefallen

Nichts wartet mehr auf eine Antwort. Was bleibt, sind Rechen- und
Messaufgaben — die Feinpunkte stehen in
[../hardware/schematic.md](../hardware/schematic.md) und
[../hardware/layout-notes.md](../hardware/layout-notes.md), die Messungen
unten.

Der nächste Schritt ist das Zeichnen in EasyEDA, nach
[../hardware/schematic.md](../hardware/schematic.md) und
[../hardware/pinout.md](../hardware/pinout.md).

## Was gemessen werden muss, bevor die Zahlen gelten

- der tatsächliche Gesamt-Ruhestrom (das Datenblatt gibt die Weck-Szenarien nur
  als Entweder-oder an, eine belegte Summe gibt es nicht)
- der Strom der Anzeige im Betrieb: das Datenblatt nennt typisch **12 mA**,
  höchstens **25 mA** — welcher Wert bei unserer Bildschirmbelegung gilt,
  entscheidet über etwa einen Monat Laufzeit
- die Betriebsströme von QSPI, I2C und SPI — **Nordic spezifiziert sie in
  keiner Fassung des Datenblatts**
- die Dauer einer Signatur im Sicherheitschip
- die Abstimmung der NFC-Antenne (mit Netzwerkanalysator, nicht rechnen)
- der Spitzenstrom und ob er einen Brownout auslöst
- die Selbstentladung der Zelle — durch kein Datenblatt belegt
