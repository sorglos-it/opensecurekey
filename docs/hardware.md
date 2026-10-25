# Hardware

Stand 23.10.2026. Teile auf Papier ausgewählt und gegen die Datenblätter
geprüft. **Nichts gebaut, nichts gemessen.** Jede Zahl in den Einzeldokumenten
hat ihre Quelle dabei; Unbelegtes ist als `ungeprueft` gekennzeichnet.

| Dokument | Inhalt |
| --- | --- |
| [parts.md](parts.md) | Stückliste mit Bestellnummern, Preisen und Begründungen |
| [power-budget.md](power-budget.md) | Versorgung aus USB (`VDDH`, 25-mA-Grenze) |
| [stromaufnahme.md](stromaufnahme.md) | Stromaufnahmen mit Datenblattstellen |
| [zellen.md](zellen.md) | Zellenwahl — erledigt, der Akku entfällt |
| [../hardware/schematic.md](../hardware/schematic.md) | Schaltplan als Netzliste, Pin für Pin |
| [../hardware/pinout.md](../hardware/pinout.md) | Pinbelegung, gegen den nRF52840 geprüft |
| [../hardware/layout-notes.md](../hardware/layout-notes.md) | Bauform und Layout-Vorgaben |

> **Der Akku entfällt (Entscheidung von Thomas, 25.10.2026).** Keine Zelle, kein
> Laderegler, keine Schutzschaltung; das Gerät läuft nur aus USB. NFC und
> Bluetooth gibt es damit nur am Rechner. Was in diesem Dokument noch vom Akku
> redet, ist als überholt gekennzeichnet; Maße und Platinenlänge stehen in
> `../hardware/`, nicht hier. Die Gründe stehen in [zellen.md](zellen.md).

## Was sich gegenüber dem Entwurf geändert hat

Sechs Punkte, alle mit Datenblattbeleg in den Einzeldokumenten:

| Entwurf | Jetzt | Grund |
| --- | --- | --- |
| `SE050C2HQ1` oder die E-Variante | **`SE051C2HQ1/Z01XDZ`** | SE050E2 hat **kein RSA**; SE051C2 hat secp256k1, Ed25519 **und** RSA, dazu 104 statt 50 kB Speicher, und ist lagernd |
| Teststand `OM-SE050ARD-E` | **MIKROE-5392** | trägt den Zielchip SE051C2 statt der falschen Variante, und kostet weniger als die Hälfte |
| `MCP73831` | ~~`MCP73832`~~ — **entfällt** | war: Statusausgang an einem GPIO lesbar. Mit dem Akku gestrichen |
| `DW01A` + `FS8205` | ~~kein Schutz-IC, dafür Verpolschutz~~ — **entfällt** | war: käufliche Zellen haben die Schutzschaltung schon. Mit dem Akku gestrichen |
| LiPo 100 mAh | ~~150 mAh (Adafruit 1317)~~ — **entfällt** | war: dünnste belegte Zelle. Mit dem Akku gestrichen, siehe [zellen.md](zellen.md) |
| OLED 0,96" | **0,71" (CFAL4864A-071BW)** | 13,9 × 22 × 1,26 mm statt 29 × 27 × 6,2 mm |

Dazu zwei Berichtigungen an Einzelangaben:

- **Dem nRF52840-DK liegt eine NFC-Antenne bei** (Nordic, Kit-Inhalt). Der
  Posten „Antenne dazukaufen, weil das DK keine hat" entfällt.
- Fünf der genannten Akku-Bestellnummern (Akku inzwischen gestrichen) stimmten nicht (Adafruit 4236 hat
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
(2,5–5,5 V) — dann braucht es **keinen externen Spannungsregler**, und die USB-5-V
passen dort hin. (Früher stand hier „Akku wie USB-5 V"; den Akku gibt es nicht
mehr.)

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
mehr nach dem 2,8 mm dicken Sensor. Wie der frei gewordene Platz im mittleren
Drittel genutzt wird, steht in
[../hardware/layout-notes.md](../hardware/layout-notes.md); der frühere Plan,
ihn als Abstand zwischen Akku und NFC-Antenne zu verwenden, ist mit dem Akku
überholt.

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
- ~~**U4** `MCP73832T-2ACI/OT` + diskrete Lastumschaltung~~ — entfällt
- **DS1** `CFAL4864A-071BW` — 0,71", SSD1306
- **J1** `GCT USB4085-GF-A` + 2 × 5,1 kΩ an CC1/CC2 + `USBLC6-2SC6`
- ~~Akku `Adafruit 1317`~~ — entfällt
- `EVQP7A01P` Taster, `Everlight 19-337C` RGB-LED
- NFC-Antenne als Leiterbahn, differentiell, abgestimmt auf 13,56 MHz

**Zur Funkzulassung:** Raytac liefert für CE/RED einen **Testbericht**, keine
Baumusterprüfbescheinigung. Die Modulzulassung erspart die Funkmessungen; die
**EU-Konformitätserklärung für das Endgerät muss selbst ausgestellt werden**,
und das Endprodukt braucht die vorgeschriebene Kennzeichnung.

Die Bluetooth-Fassung ist laut Herstellerdokument **5.2**; Händler werben mit
5.4 — verbindlich ist Raytac.

## Laufzeit — entfallen

Mit dem Akku gibt es keine Laufzeit mehr; die frühere Rechnung („etwa 6,8 Monate
bei zehn Benutzungen am Tag mit 150 mAh") ist erledigt. **Die drei
Entscheidungen, die sie trugen, bleiben im Entwurf, aber ihre Begründung ist
jetzt eine andere:**

- Lastschalter für die Anzeige (war: Faktor 3 der Laufzeit)
- Wecken per GPIOTE-PORT statt Event (war: Faktor 7 im Weckpfad)
- Bluetooth auf Abruf statt dauernd (war: Faktor 1,5; bleibt wegen des
  Bedrohungsmodells)

Was jede heute begründet und was von ihr offen ist, steht in
[stromaufnahme.md](stromaufnahme.md), Abschnitt „Befunde, deren Begründung
gefallen ist". Der Akku als Verschleißteil (300 Ladezyklen, etwa sechs Jahre)
war einer der Gründe, ihn zu streichen — siehe [zellen.md](zellen.md); die Frage „als
Verschleißteil benennen oder austauschbar" hat sich damit erledigt.

## Die Entscheidungen

### Sendeleistung höchstens 4 dBm, kein zweiter Spannungswandler

Damit bleibt die 25-mA-Grenze aus Befund 2 gültig, und die Nebenverbraucher
dürfen am internen `VDD` hängen. Ein Schlüssel am Schlüsselbund redet mit einem
Handy in Armlänge, nicht durch zwei Wände; ein Wandler nur für Reichweite, die
niemand braucht, wäre Vorrat.

**Bedingung, die eingehalten werden muss:** Die Firmware begrenzt `TXPOWER` auf
höchstens +4 dBm, und die Lastschalter Q4/Q5 sind Pflicht. Über 4 dBm bleiben
nur 5 mA — damit ließe sich kein einziger Nebenverbraucher mehr betreiben.

### Akku und NFC-Antenne: Trennung und Ferritfolie — überholt

Entschieden war: Die Platine wird länger, damit Akku und Antenne in
verschiedenen Dritteln liegen, und eine Ferritfolie entkoppelt die Zelle. Beides
war für den Akku gedacht — die Zelle mit ihrer Aluminiumhülle dämpft das
NFC-Feld. **Mit dem Akku fällt die Begründung.** Der Hardware-Strang (Zweig `akku-weg`) hat
nachgemessen: Die Platine wird kürzer, und die Ferritfolie entfällt ersatzlos.
Die Maße stehen in `../hardware/layout-notes.md` (nach dem Zusammenführen
dieses Zweigs) und **werden hier bewusst nicht wiederholt.**

Auch der Satz „NFC erst in Stufe 2 wäre falsch: der Akku ist überhaupt nur
deshalb im Gerät, weil NFC und Bluetooth ohne Rechner laufen sollen" ist damit
erledigt. NFC bleibt in Stufe 1, läuft aber nur mit USB-Strom.

### Ladungspumpen-Kondensatoren der Anzeige: keine nötig

Am Datenblatt geklärt (Crystalfontz CFAL4864A-071BW, Ausgabe 2017-07-05,
Abschn. 6). Die zwölf Pins des Steckverbinders sind GND, D2, D1, D0, D/C#,
/RES#, CS#, BS1, BS0, VDD, VBAT, GND — **es gibt keine Pins `C1P/C1N/C2P/C2N`
und keinen `VCC`-Pin.** Die Ladungspumpe sitzt also auf dem Modul und erzeugt
die Panelspannung selbst aus `VBAT`; bestätigt durch die Strommessung, die mit
`VCC = 7,5 V` angegeben ist, obwohl `VCC` nirgends herausgeführt ist.

**Auf unserer Platine ist nichts nötig.** Nebenbei belegt: der Strom ist
**typisch 12 mA, höchstens 25 mA** — der obere Wert, mit dem
[stromaufnahme.md](stromaufnahme.md) rechnet, ist also die echte Obergrenze.

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

## Zwei Werkseinstellungen, die nicht sind, was man erwartet

Aus AN12973 Rev. 2.0, Tabelle 3 — beide gefunden, als der SE050-Strang die
Tabelle als Bild gerendert hat, weil die Textauswertung der PDF die Spalten
verschob:

- **Der Stromsparmodus mit Zustandserhalt ist ab Werk abgeschaltet.** In der
  Tabelle steht „Power-Down (with state retention), ~430 µA – Disabled", und die
  Fußnote sagt: einschaltbar nur über das Perso-Applet (AN13015). Ohne diesen
  Schritt bleibt nur „Deep Power-Down" mit unter 5 µA, bei dem der Zustand
  **verloren** geht. Die frühere Laufzeitrechnung tat so, als stünde beides zur
  Wahl; die Laufzeit gibt es nicht mehr, aber der Suspend-Strom im Betrieb hängt
  von diesem Modus ab (siehe [stromaufnahme.md](stromaufnahme.md)). Vor der
  Platine klären.
- **Clock Stretching ist ab Werk ebenfalls aus.** Die Zeile „I2C Target, up to
  3,4 Mbit/s" gilt laut Tabelle nur „with clock stretching enabled" — auch das
  geht nur über das Perso-Applet. Ohne es ist die Höchstfrequenz niedriger. Das
  betrifft den I2C-Treiber auf dem nRF52840, nicht das Layout.

Dazu: **Der Nutzspeicher ist eine Bestellausführung, keine Eigenschaft der
Variante** — 46 kB („Full Feature") oder 104 kB („Maximum"). Welche die
Teststand-Platine trägt, wird vor der Bestellung geklärt.

Alle drei sind Einstellungen am fertigen Chip, kein Grund für ein anderes Teil.
Aber sie müssen in die Erstinbetriebnahme, sonst fallen sie erst am Gerät auf.

## Was tatsächlich hineinpasst

Gerechnet am 23.10.2026. Die Programmgrößen sind **gemessen**, nicht geschätzt:
`go build -trimpath -ldflags="-s -w"`, ohne cgo, für die drei Systeme.

| Posten | Größe |
| --- | --- |
| Programm Windows | 9,3 MB |
| Programm Linux | 9,0 MB |
| Programm macOS | 9,2 MB |
| Zweiter Platz für das Firmware-Update | 1,0 MB |
| Dateisystem des Laufwerks | ~1,0 MB |
| **fest belegt, ohne Tresor** | **29,5 MB** |
| **frei für den Tresor** | **34,5 MB** |

| Flash | frei für den Tresor | Einträge |
| --- | --- | --- |
| 32 MB | **passt nicht** — die Programme allein sind 27,5 MB | — |
| **64 MB** | 34,5 MB | **3072** (4 Profile zu 768) |
| 128 MB | 98,5 MB | mehr möglich, aber nicht nötig |
| 256 MB | 226,5 MB | dito |

**Damit ist 32 MB erledigt**: allein die drei Programme und der Tresor sprengen
es. Das war vorher eine Vermutung, jetzt ist es gerechnet.

**Und mehr als 64 MB bringt nichts**, weil 3072 Einträge der Bedarf übersteigen
und der Tresor sie in 28,2 MB unterbringt. Die alte Grenze von 256 Einträgen lag
nicht am Flash, sondern am RAM: eine ganze Indexfassung lag im Speicher. Seit
Formatfassung 4 ist der Index in Blöcke zu 64 Sätzen geteilt, von denen immer
nur einer im RAM liegt — 768 Einträge je Profil kosten damit genauso viel RAM
wie vorher 64.

Warum nicht 4096: Die Plätze zu 8 KiB machen daraus 32 MB plus 2,7 MB Index,
also 34,9 MB — über der Linie, bevor irgendein Spielraum da ist. Kleinere
Plätze würden das längste Geheimnis kürzen und jeden bestehenden Platz
verschieben. Für 25 % mehr Einträge lohnt das nicht.

**Was eine Suche kostet** (gemessen am PC gegen die Krypto-Attrappe, nicht am
Gerät): Seit der Index geteilt ist, muss eine Suche **alle** Blöcke eines
Profils entschlüsseln — 240 KiB für ein volles Profil. Am PC sind das rund
1,3 ms, am Gerät ist es offen: die 240 KiB müssen bei **jeder** Passkey-Anmeldung
vom QSPI-Flash kommen und durch AES-GCM. Das ist die erste Messung, die nach dem
Merkmalbild ansteht.

## Warum 64 MB und nicht mehr

Geprüft am 23.10.2026, Preise bei Menge 1, alle `ungeprüft` (Momentaufnahme):

| Größe | Teil | Preis | Lager |
| --- | --- | --- | --- |
| 32 MB | `W25Q256JVEIQ` | ca. 6,08 USD | 1.269 |
| **64 MB** | **`W25Q512JVEIQ`** | **ca. 15,35 USD** | 1.455 |
| 128 MB | — | kein belegter Einzelchip in dieser Bauform gefunden | — |

Der Sprung von 32 auf 64 MB kostet also das Zweieinhalbfache, nicht „kaum mehr".
Oberhalb von 64 MB wird serieller NOR-Flash unverhältnismäßig: billiger wäre
dort nur serieller NAND, und der bringt Bad-Block-Verwaltung, Fehlerkorrektur
und Seitenverwaltung mit — für ein Gerät, dessen Tresor nie eine halbe Datei
hinterlassen darf, ein schlechter Tausch.

**Der Bedarf liegt bei rund 35 MB:** drei portable Programme zu je etwa 8–12 MB,
der Tresor im einstelligen MB-Bereich, dazu rund 1 MB für den zweiten
Update-Platz von MCUboot. Bei 32 MB wäre das zu knapp, bei 64 MB ist Luft.

**Entschieden: 64 MB bleiben — und im Layout ist Platz für einen zweiten
Flash-Baustein vorgesehen, der nicht bestückt wird.** Ein unbestückter
Fußabdruck kostet nichts außer ein paar Quadratmillimetern. Reicht der Platz
eines Tages nicht, wird er bestückt, statt eine neue Platine zu zeichnen.

## Zwei Dinge, die erst das gelieferte Teil beantwortet

Beide sind im Werk gesetzt und danach **unveränderlich**. Keine lässt sich vor
der Lieferung sicher klären, beide beantwortet ein einziger Lauf von
`osk_se050_open()`, sobald der Chip da ist:

1. **Der Nutzspeicher** — 46 kB („Full Feature") oder 104 kB („Maximum").
2. **Das Merkmalbild.** Die NIST-Sicherheitspolicy für den FIPS-validierten
   SE051 nennt den Wert `0x27F2`: darin sind **EdDSA aus**, **Curve25519 aus**
   und der FIPS-Modus verfügbar — und im FIPS-Modus verweigert der Baustein
   **ECDH grundsätzlich** (AN12413 4.10.3.1).

Das ist **eine** Ausführung, nicht zwingend die bestellte: AN12973 Tabelle 3
schreibt der Variante SE051C2 sowohl Ed25519 als auch Curve25519 zu. Aber die
Variante sagt, was sie *kann*; das Merkmalbild sagt, was das *gelieferte Stück*
darf.

Trifft es zu, fehlen drei Dinge, die der Entwurf voraussetzt: SSH-Schlüssel
über Ed25519, die Verschlüsselung Server zu Server über X25519, und der
Schlüsselaustausch im Chip beim Koppeln eines zweiten Geräts. Der Treiber
meldet das bereits sauber (`OSK_ERR_UNSUPPORTED` statt einer Ablehnung, die
nach fehlender PIN aussieht), aber die Funktionen wären weg.

**Darum beim Eintreffen als Allererstes:** Chip anschließen, `osk_se050_open()`
laufen lassen, Merkmalbild und freien Speicher ins Messprotokoll schreiben.
Erst danach lohnt jede weitere Messung.

## Was gemessen werden muss, bevor die Zahlen gelten

- der tatsächliche Gesamt-Ruhestrom (das Datenblatt gibt die Weck-Szenarien nur
  als Entweder-oder an, eine belegte Summe gibt es nicht)
- der Strom der Anzeige im Betrieb: das Datenblatt nennt typisch **12 mA**,
  höchstens **25 mA** — welcher Wert bei unserer Bildschirmbelegung gilt, ist
  der größte Einzelposten je Benutzung (Laufzeit gibt es keine mehr)
- die Betriebsströme von QSPI, I2C und SPI — **Nordic spezifiziert sie in
  keiner Fassung des Datenblatts**
- die Dauer einer Signatur im Sicherheitschip
- die Abstimmung der NFC-Antenne (mit Netzwerkanalysator, nicht rechnen)
- der Spitzenstrom und ob er einen Brownout auslöst
- der Spannungseinbruch auf `VBUS` beim Einstecken und bei Lastsprüngen
