# Verdrahtung des Teststands

Stand 30.10.2026. **Alles auf dieser Seite ist Papier.** Nichts davon ist
aufgebaut, nichts gemessen. Es ist aus Unterlagen abgeleitet; jede Zeile nennt
ihre Quelle. Was nicht belegt ist, steht als `ungeprueft` da — das ist in
diesem Projekt kein Makel, sondern die Kennzeichnung für „noch nicht gemessen".

**Vor dem Einstecken den Prüfschritt ganz unten abarbeiten. Erst messen, dann
Spannung.**

## Die Teile

Welche Teile gemeint sind, steht in [`bestellliste-teststand.md`](bestellliste-teststand.md);
hier werden sie nicht noch einmal beschrieben.

- **nRF52840-DK (PCA10056)** als Wirt
- **SE051 Plug & Trust Click, MIKROE-5392**, I²C
- **OLED 0,96", 128 × 64, SSD1306, SPI, 7-polig** (Hailege)
- **microSD-Modul Soldered 333050** (`SOL-333050`), SPI

## Quellen

| Kürzel | Unterlage |
| --- | --- |
| **DK-UG** | Nordic, *nRF52840 DK Hardware User Guide*, online auf `docs.nordicsemi.com`, abgerufen 30.10.2026. Benutzte Abschnitte: „Connector interface" (samt der Bildtafel), „Buttons and LEDs", „External memory", „Virtual serial port", „Analog pin map", „Reset button", „Solder bridge configuration", „Power supply", „VDD power sources", „5 V power sources". |
| **Click-SCH** | MikroElektronika, *SE051 Plug&Trust click schematic v100*, © 2022. **Zweithand:** gelesen als Kopie auf `cdn.sparkfun.com`, nicht von einem Mikroe-Server (`mikroe.com` antwortet auf maschinelle Abrufe mit 403). Es ist die Zeichnung des Herstellers, aber der Bezugsweg ist `ungeprueft`. |
| **SD-HW** | Soldered Electronics, eigenes Hardware-Repo `SolderedElectronics/microSD-card-reader-breakout-hardware-design`, Datei `OUTPUTS/V1.2/microSD card reader breakout Schematic.pdf` (Titel „v1.1.0", 2024-12-16) und `…/Pinout.png` (v1.2.0), dazu `docs.soldered.com/microsd-reader/`. Abgerufen 30.10.2026. |
| **pinout** | [`../hardware/pinout.md`](../hardware/pinout.md) — die Belegung der eigenen Platine. |
| **strom** | [`stromaufnahme.md`](stromaufnahme.md) — alle Stromzahlen stehen dort und **nur** dort. |

Für das OLED-Modul gibt es **keine Herstellerunterlage** (Hailege
veröffentlicht keine). Was unten dazu steht, ist entsprechend gekennzeichnet.

## Drei Namen für dieselbe Leitung

Am DK heißt eine Leitung dreierlei, und genau hier entstehen die
Verwechslungen:

1. **Pinname** `P0.26` — der Name im Chip, in der Firmware **und auf dem
   Aufdruck des Bretts**.
2. **Leiste** `P3`, `P4` … — die Stiftreihe, in der der Pin sitzt.
3. **Arduino-Name** `A0` … — nur für die sechs Analogstifte.

**Steck nach dem aufgedruckten Pinnamen.** Das DK beschriftet jeden
Leistenstift mit seinem `P0.xx`/`P1.xx`-Namen; der Aufdruck ist eindeutig.
Die Leiste steht hier nur dabei, damit du weißt, **wo am Brett** du suchen
musst.

> **`ungeprueft`: die Stift*nummern*.** Die Bildtafel des DK-UG zeigt die
> **Reihenfolge** der Signale je Leiste, aber keine Nummern, und welcher Stift
> die Nummer 1 trägt, geht aus ihr nicht hervor. **Darum stehen hier keine
> Stiftnummern — nur „oben"/„unten" innerhalb der Leiste.** Eine Nummer zu
> raten wäre genau der Fehler, der Teile zerstört.

**Arduino-Namen** (DK-UG, „Analog pin map") — keiner davon wird hier gebraucht:
`A0` = `P0.03`, `A1` = `P0.04`, `A2` = `P0.28`, `A3` = `P0.29`,
`A4` = `P0.30`, `A5` = `P0.31`. Die Digitalnamen `D0`–`D13` sind `ungeprueft`.

## Wo die Pins sitzen — die Leisten des DK

Aus der Bildtafel des DK-UG, Abschnitt „Connector interface", am 30.10.2026
Zeile für Zeile abgelesen. Reihenfolge **von oben nach unten**, so wie die
Tafel sie zeigt (das Brett so gehalten, dass die beiden USB-Buchsen oben sind).

| Leiste | Signale, von oben nach unten |
| --- | --- |
| **P1** | Versorgung und Reset: `VDD`, `VDD`, `RESET`, `VDD`, `5V`, `GND`, `GND` |
| **P2** | `P0.03`, `P0.04`, `P0.28`, `P0.29`, `P0.30`, `P0.31` — die Analogstifte (A0–A5) |
| **P4** | `P0.27`, `P0.26`, `P0.02`, **`GND`**, `P1.15`, `P1.14`, `P1.13`, `P1.12`, `P1.11`, `P1.10` |
| **P3** | `P1.08`, `P1.07`, `P1.06`, `P1.05`, `P1.04`, `P1.03`, `P1.02`, `P1.01` |
| **P5** | `P0.10`, `P0.09`, `P0.08`, `P0.07`, `P0.06`, `P0.05`, `P0.01`, `P0.00` — **alle belegt** (NFC, UART, Quarz) |
| **P24** | `P0.11` … `P0.25` (Taster, LEDs, Flash, Reset), dazu `P1.00` und `P1.09`, zuletzt `GND` — **die Leiste der belegten Pins** |

**Alles, was dieser Plan braucht, sitzt auf `P3` und `P4`** — zwei Leisten
nebeneinander, dazu `VDD` auf `P1`. Das macht das Stecken übersichtlich.

## Finger weg: was am DK nicht frei ist

Alles aus **DK-UG**, Abschnitte „Connector interface", „Buttons and LEDs",
„External memory", „Virtual serial port", „Reset button", „Solder bridge
configuration".

| Pin(s) | Belegt durch | Folge |
| --- | --- | --- |
| `P0.00`, `P0.01` | 32,768-kHz-Quarz (Brücken SB1, SB2, ab Werk geschlossen) | auf `P5`, aber belegt |
| `P0.05`, `P0.06`, `P0.07`, `P0.08` | UART zum Debugger-Chip (RTS, TXD, CTS, RXD; SB50–SB53 geschlossen) | ohne sie keine serielle Ausgabe |
| `P0.09`, `P0.10` | NFC1, NFC2 | belegt |
| `P0.11`, `P0.12`, `P0.24`, `P0.25` | **Taster 1–4** | belegt |
| `P0.13`, `P0.14`, `P0.15`, `P0.16` | **LED 1–4** (SB5–SB8 geschlossen) | belegt |
| `P0.17`, `P0.19`–`P0.23` | **QSPI-Flash auf dem Brett** | Brücken SB13, SB11, SB12, SB14, SB15, SB10 — **ab Werk geschlossen** |
| `P0.18` | **Reset-Taster SW5** (über den Debugger-Chip, SB42/SB56) | belegt |
| **`P1.09`** | **SWO zum Debugger-Chip**, Brücke **SB57 ab Werk geschlossen** | **sieht frei aus, ist es nicht.** Steht auf `P24` als „TRACEDATA" — und hängt zusätzlich am Debugger |
| `P1.00` | TRACEDATA | nur auf `P24`, nicht auf `P3`/`P4` |

**Frei und auf `P2`/`P3`/`P4` erreichbar:** `P0.02`, `P0.03`, `P0.04`,
`P0.26`, `P0.27`, `P0.28`–`P0.31`, `P1.01`–`P1.08`, `P1.10`–`P1.15`.

### Der Engpass, der daraus folgt

Die Belegung der eigenen Platine teilt die Pins in **schnell** (SPI, I²C, UART,
PWM erlaubt) und **langsam** (höchstens 10 kHz) — Begründung und Liste stehen
in **pinout**, Abschnitt „Pins, die nur langsam dürfen". Legt man diese
Einteilung über die am DK freien Pins, bleiben als **frei und schnell** nur:

**`P0.26`, `P0.27`, `P1.08`** — drei Stück.

> `P1.09` wäre der vierte gewesen. **Er fällt weg:** Brücke SB57 verbindet ihn
> ab Werk mit dem Debugger-Chip (SWO), und auf `P3`/`P4` liegt er gar nicht.

Gebraucht werden sieben schnelle Leitungen: I²C-Takt und -Daten, OLED-Takt und
-Daten, SD-Takt, -Ausgang und -Eingang.

**Das geht nicht auf.** Vier Leitungen müssen auf Pins, die das Datenblatt als
„low frequency I/O only" führt. Unten ist jede davon mit ⚠ gekennzeichnet.
Die drei schnellen Pins bekommen die beiden I²C-Leitungen und den SD-Takt.

> **Entscheidung, die nicht meine ist:** Man könnte stattdessen die Lötbrücken
> **SB10–SB15 auftrennen und SB20–SB25 schließen** (DK-UG, „External memory",
> „Solder bridge configuration"). Dann wären `P0.17` und `P0.19`–`P0.23` frei —
> **genau die Pins, die `pinout.md` für den Flash vorsieht**, und alle schnell.
> **Ich empfehle das nicht von mir aus:** es ist ein nicht umkehrbarer Eingriff
> an einem 41-€-Brett, und die Pins lägen dann auf `P24`. Die Entscheidung
> liegt bei der Leitsitzung.

## Die Verdrahtung

Masse ist in jeder der drei Gruppen einzeln aufgeführt. **Alle Massen gehören
zusammen** — jedes Modul bekommt seine eigene Leitung zu einem GND-Stift des
DK (auf `P1` sind zwei, auf `P4` einer), **nicht** eine Kette von Modul zu
Modul.

### 1. SE051 Plug & Trust Click (I²C)

Das Click steckt **nicht** in einer mikroBUS-Fassung — das DK hat keine. Es
wird mit Einzeldrähten an die mikroBUS-Stifte des Clicks angeschlossen. Die
Stifte sind auf dem Click beschriftet; die Reihenfolge nach **Click-SCH**:
linke Leiste von oben `AN, RST, CS, SCK, MISO, MOSI, +3.3V, GND`, rechte
Leiste von oben `PWM, INT, TX, RX, SCL, SDA, +5V, GND`.

| Signal | DK-Pin | Leiste | Anschluss am Click | Begründung / Quelle |
| --- | --- | --- | --- | --- |
| `I2C_SCL` | **`P0.27`** | `P4`, **oberster** Stift | `SCL` (rechte Leiste) | **Abweichung von pinout** (dort `P0.25`): `P0.25` ist am DK **Taster 4**. `P0.27` ist frei und schnell |
| `I2C_SDA` | **`P0.26`** | `P4`, zweiter von oben | `SDA` (rechte Leiste) | **wie pinout** — frei und schnell |
| `SE_RST` | **`P1.02`** | `P3`, zweiter von **unten** | `RST` (linke Leiste) | Click-SCH: mikroBUS-`RST` geht direkt an SE051 Pin 14 `RST`. **pinout kennt diese Leitung nicht** — die eigene Platine führt den Reset des SE051 nicht heraus. In pinout ist `P1.02` die Ladeanzeige `CHG_STAT`, die der Teststand nicht hat. Langsam genügt |
| `SE_ENA` | **`P1.10`** | `P4`, **unterster** Stift | `CS` (linke Leiste) | **wie pinout** (Deep Power-down, SE051 Pin 11). Click-SCH: `ENA` hängt am Schiebeschalter **SW2 „ENA SEL"**, der zwischen der mikroBUS-`CS`-Leitung und festem 3V3 umschaltet |
| Versorgung | **`VDD`** (3 V) | `P1` | `+3.3V` (linke Leiste, unterster Signalstift) | Click-SCH: auf dem Click hängt **alles** an `3V3`; `+5V` ist auf der Zeichnung **nirgends angeschlossen**. **Nie an 5 V** |
| Masse | **`GND`** | `P1` oder `P4` (vierter von oben) | `GND` (einer der beiden) | — |

**Pullups: schon drauf, keine zusätzlichen.** Click-SCH zeigt an `SDA` und
`SCL` je zwei Widerstände nach `3V3`, umschaltbar über die Lötbrücken **JP1
(SDA)** und **JP2 (SCL)**:

- Stellung 1–2: **R2 / R3 = 3,3 kΩ**
- Stellung 2–3: **R6 / R7 = 680 Ω**

Die Herstellerangabe, dass JP1/JP2 die I²C-Geschwindigkeit wählen (400 kbit
gegen 1 Mbit) und **beide gleich stehen müssen**, ist `ungeprueft` — sie
stammt aus einer Händlerbeschreibung, nicht von Mikroe selbst. Die
Widerstandswerte dagegen stehen in der Zeichnung.

**Empfehlung: beide Brücken in der Werkstellung lassen und nichts
dazulöten.** Welche Werkstellung das ist, steht in keiner Unterlage —
**am Brett nachsehen**, bevor gesteckt wird.

**Spannung und Pegelwähler:** Das Click arbeitet mit **3,3 V Logik und 3,3 V
Versorgung**. Auf der Zeichnung gibt es **keinen Pegelwandler und keinen
Spannungswähler**; die einzigen Schalter sind SW2 für `ENA` und SW1 für die
NFC-Antenne. Das DK liefert an `VDD` **3 V** — darunter, nicht darüber, also
unkritisch. Ob der SE051 bei 3,0 V statt 3,3 V sauber läuft, ist `ungeprueft`.

### 2. OLED SSD1306, 0,96", SPI, 7-polig

> **Für dieses Modul gibt es keine Herstellerunterlage.** Die übliche
> Beschriftung der 7-poligen Fassung lautet, von einem Ende her:
> `GND, VCC, D0, D1, RES, DC, CS`. **Diese Reihenfolge ist `ungeprueft`** —
> nicht belegt, nur verbreitet.
>
> **Verdrahte nach dem Aufdruck auf der Modulplatine.** Jeder Stift ist
> beschriftet. `D0` ist der Takt (SCLK), `D1` die Daten (SDIN).
>
> **Das ist die Leitung, bei der die Gefahr am größten ist:** bei manchen
> Chargen sind `GND` und `VCC` gegenüber dieser Reihenfolge vertauscht.
> Verpolte Versorgung tötet das Modul.

| Signal | DK-Pin | Leiste | Anschluss am Modul | Begründung / Quelle |
| --- | --- | --- | --- | --- |
| `OLED_SCK` | **`P1.04`** ⚠ | `P3`, fünfter von oben | `D0` (SCLK) | **Abweichung von pinout** (dort `P0.27`): `P0.27` ist hier der I²C-Takt, und `P0.20` (pinout: MOSI) ist am DK Flash. ⚠ Port 1, laut Datenblatt nur langsam |
| `OLED_MOSI` | **`P1.03`** ⚠ | `P3`, dritter von unten | `D1` (SDIN) | wie oben. ⚠ nur langsam |
| `OLED_CS` | **`P1.11`** | `P4`, zweiter von unten | `CS` | **wie pinout** |
| `OLED_DC` | **`P1.12`** | `P4`, dritter von unten | `DC` | **wie pinout** |
| `OLED_RES` | **`P1.13`** | `P4`, vierter von unten | `RES` | **wie pinout** |
| Versorgung | **`VDD`** (3 V) | `P1` | `VCC` | **nicht an 5 V.** Das Modul hat keinen Pegelwandler; seine Eingänge sehen die DK-Pegel |
| Masse | **`GND`** | `P1` oder `P4` | `GND` | — |

> **`ungeprueft` und ein wahrscheinlicher erster Ärger:** Das DK liefert
> **3,0 V**, nicht 3,3 V (DK-UG, „VDD power sources": fester 3-V-Abwärtsregler).
> Der SSD1306 braucht für seine Ladungspumpe nach verbreiteter Lesart
> mindestens 3,3 V an `VBAT`. **Bleibt die Anzeige dunkel, ist das Verdacht
> Nummer eins** — dann an einer geregelten 3,3-V-Quelle versorgen und **die
> Masse weiterhin mit dem DK verbinden**. Die 3,3-V-Untergrenze ist hier
> `ungeprueft`; das SSD1306-Datenblatt lag nicht vor.
>
> **pinout führt zusätzlich `OLED_PWR` (`P1.14`, Lastschalter).** Der
> Teststand hat keinen; die Leitung entfällt, `P1.14` bleibt frei. **Für die
> Brettdefinition heißt das: der Teststand kann die Anzeige nicht abschalten** —
> Schlafstrommessungen am Anzeigepfad sind hier nicht möglich.

### 3. microSD-Modul Soldered 333050, SPI

Beschriftung der sechspoligen Leiste, von einem Ende her (**SD-HW**,
`Pinout.png` v1.2.0): `CS, MOSI, MISO, SCLK, GND, VCC`. **Die Stiftleiste muss
erst angelötet werden** (bestellliste).

| Signal | DK-Pin | Leiste | Anschluss am Modul | Begründung / Quelle |
| --- | --- | --- | --- | --- |
| `SD_SCK` | **`P1.08`** | `P3`, **oberster** Stift | `SCLK` | der letzte freie **schnelle** Pin am DK; der Takt bekommt ihn. **pinout hat keinen SD-Pfad** — die eigene Platine nimmt QSPI-Flash statt Karte, also gibt es nichts anzugleichen |
| `SD_MOSI` | **`P1.07`** ⚠ | `P3`, zweiter von oben | `MOSI` | frei. ⚠ Port 1, nur langsam |
| `SD_MISO` | **`P1.06`** ⚠ | `P3`, dritter von oben | `MISO` | frei. ⚠ nur langsam |
| `SD_CS` | **`P1.05`** | `P3`, vierter von oben | `CS` | frei; eine Auswahlleitung ist von Natur aus langsam |
| Versorgung | **`VDD`** (3 V) | `P1` | `VCC` | **siehe unten — 3 V, nicht 5 V** |
| Masse | **`GND`** | `P1` oder `P4` | `GND` | — |

#### 3,3 V oder 5 V? — **3,3 V (am DK: die 3 V von `VDD`). 5 V wäre falsch.**

Der Hersteller druckt „VCC: 5V" auf das Pinbild, und die Übersichtsseite nennt
„5V (3.3V onboard regulator)". **Für dieses Brett ist das trotzdem die falsche
Wahl**, und der Grund steht in der Zeichnung des Herstellers selbst:

- Auf dem Modul sitzt ein **TXB0104** als Pegelwandler (**SD-HW**, Block
  „VOLTAGE LEVEL TRANSLATOR"). Seine **A-Seite hängt am 3V3-Netz** (hinter dem
  Regler, zur Karte hin), seine **B-Seite hängt an `VCC`** — also an dem, was
  du an die Leiste legst.
- Die B-Seite ist die Seite zum Wirt. **Legst du 5 V an `VCC`, treibt das
  Modul 5-V-Pegel auf `MISO` in einen 3-V-Eingang des nRF52840.** Das ist die
  eine Leitung in diesem ganzen Plan, die den Chip sicher beschädigt.

Mit `VCC` = 3,0 V bleibt die Reihenfolge gewahrt, die der TXB0104 verlangt
(A-Seite hinter dem Regler ≤ B-Seite), und die Karte bekommt ihre Spannung über
den Regler **SE5218**.

> **`ungeprueft`:** Ob der SE5218 bei 3,0 V Eingang noch genug Ausgang macht,
> steht in keiner mir vorliegenden Unterlage. **Meldet sich die Karte nicht**,
> ist das Verdacht Nummer eins. Das Modul hat dafür die Lötbrücke **`JP2`
> („3V3 voltage regulator bypass", ab Werk offen)**; geschlossen geht `VCC`
> direkt auf das 3V3-Netz. **Erst wenn es nötig ist, und nicht vorher.**
> `JP1` („3V3 voltage regulator disconnect", ab Werk geschlossen) bleibt, wie
> sie ist.

## Die Zeichnung

`P3` und `P4` liegen nebeneinander auf derselben Brettseite. **Die Stifte sind
hier in der Reihenfolge gezeichnet, in der sie am Brett stehen** — oben in der
Zeichnung ist oben am Brett. `●` heißt belegt, `○` bleibt frei.

```
   nRF52840-DK                      Leiste P4         Leiste P3
   ┌──────────────────┐         ┌─────────────┐   ┌─────────────┐
   │ J2 Debug  J3 USB │  oben   │ ● P0.27     │   │ ● P1.08     │  oben
   └──────────────────┘         │ ● P0.26     │   │ ● P1.07     │
                                │ ○ P0.02     │   │ ● P1.06     │
   Leiste P1 (Versorgung)       │ ● GND       │   │ ● P1.05     │
   ┌───────────┐                │ ○ P1.15     │   │ ● P1.04     │
   │ ● VDD     │ ← 3 V          │ ○ P1.14     │   │ ● P1.03     │
   │ ○ VDD     │                │ ● P1.13     │   │ ● P1.02     │
   │ ○ RESET   │                │ ● P1.12     │   │ ○ P1.01     │
   │ ○ VDD     │                │ ● P1.11     │   └─────────────┘  unten
   │ ○ 5V      │ ← bleibt LEER  │ ● P1.10     │
   │ ● GND     │                └─────────────┘  unten
   │ ● GND     │
   └───────────┘
```

```
 SE051 Click                          DK
   SCL   ────────────────────────────  P0.27   (P4, oben)
   SDA   ────────────────────────────  P0.26   (P4)
   RST   ────────────────────────────  P1.02   (P3)
   CS    ────────────────────────────  P1.10   (P4, unten)  = ENA, Schalter SW2
   +3.3V ────────────────────────────  VDD     (P1)
   GND   ────────────────────────────  GND     (P1 oder P4)
   +5V      bleibt LEER

 OLED 0,96"   (nach Aufdruck stecken!)
   D0 (Takt) ────────────────────────  P1.04   (P3)   ⚠ nur langsam
   D1 (Daten)────────────────────────  P1.03   (P3)   ⚠ nur langsam
   CS    ────────────────────────────  P1.11   (P4)
   DC    ────────────────────────────  P1.12   (P4)
   RES   ────────────────────────────  P1.13   (P4)
   VCC   ────────────────────────────  VDD     (P1)   ← 3 V, nie 5 V
   GND   ────────────────────────────  GND

 microSD Soldered 333050
   SCLK  ────────────────────────────  P1.08   (P3, oben)
   MOSI  ────────────────────────────  P1.07   (P3)   ⚠ nur langsam
   MISO  ────────────────────────────  P1.06   (P3)   ⚠ nur langsam
   CS    ────────────────────────────  P1.05   (P3)
   VCC   ────────────────────────────  VDD     (P1)   ← 3 V, nie 5 V
   GND   ────────────────────────────  GND
```

`P1.01`, `P1.14`, `P1.15`, `P0.02` und die Analogleiste `P2` bleiben frei.
**`P5` und `P24` werden gar nicht angefasst.**

## Versorgung: reicht das?

**Alle Stromzahlen stehen in [`stromaufnahme.md`](stromaufnahme.md).** Hier
steht keine einzige davon abgeschrieben — nur, welche Zeile gilt:

| Verbraucher | wo die Zahl steht |
| --- | --- |
| nRF52840, CPU 64 MHz | strom, Abschnitt „Betrieb", Zeile *CPU 64 MHz aus Flash, mit DC-DC* |
| SE051C2 bei Krypto | strom, „Betrieb", Zeile *SE051C2 bei asymmetrischer Krypto* |
| SE051C2 in Ruhe | strom, „Ruhe", Zeile *SE051C2, Deep Power-down* |
| Anzeige | **Vorsicht, zwei Größen mit demselben Namen:** die OLED-Zeilen in strom gelten für die **0,71"**-Anzeige der eigenen Platine (Crystalfontz CFAL4864A-071BW). **Der Teststand hat eine 0,96"-Anzeige von Hailege. Für die gibt es keine Zahl** — `ungeprueft` |
| microSD-Karte | **kommt in strom nicht vor** — `ungeprueft`. Karten ziehen beim Schreiben kurzzeitig deutlich mehr als im Ruhezustand; eine belegte Zahl habe ich nicht |

**Und die Zahl, auf die es eigentlich ankäme, fehlt:**

> **Nordic nennt für das DK keinen Grenzstrom.** Die Abschnitte „Power supply",
> „5 V power sources" und „VDD power sources" des DK-UG beschreiben den festen
> **3-V-Abwärtsregler** und die Quellen, geben aber **weder eine Stromgrenze
> für `VDD` noch den Reglertyp** an. Jede Zahl dazu wäre erfunden.
> **Das ist `ungeprueft` und muss am Brett gemessen werden.**

Was sich daraus sagen lässt, ohne zu erfinden:

1. Die **Ruheströme** aller drei Module zusammen liegen im Bereich einzelner
   Mikroampere (strom, Abschnitt „Ruhe") — dort gibt es keinen Engpass.
2. Der **Spitzenbedarf** entsteht, wenn Rechenkern, Sicherheitschip, Anzeige
   und Karte gleichzeitig ziehen. Von diesen vier fehlen **zwei** Zahlen.
   **Eine belastbare Summe ist darum heute nicht möglich.**
3. **Vorgehen statt Rechnung:** die Module einzeln in Betrieb nehmen — erst
   das Click, dann die Anzeige, dann die Karte — und den Strom am DK messen
   (DK-UG, „Measure average current with an ampere meter").

## Was das für die Brettdefinition heißt

Der Teststand ist **nicht** dieselbe Belegung wie die eigene Platine. Das
gehört in eine eigene Brettdefinition, nicht als Schalter in die bestehende:

| | eigene Platine (pinout) | Teststand | Grund |
| --- | --- | --- | --- |
| `I2C_SDA` | `P0.26` | `P0.26` | gleich |
| `I2C_SCL` | `P0.25` | **`P0.27`** | `P0.25` ist am DK Taster 4 |
| `SE_ENA` | `P1.10` | `P1.10` | gleich |
| `SE_RST` | — | **`P1.02`** | gibt es auf der Platine nicht |
| `OLED_CS` / `DC` / `RES` | `P1.11` / `P1.12` / `P1.13` | gleich | gleich |
| `OLED_SCK` | `P0.27` | **`P1.04`** | `P0.27` ist jetzt I²C-Takt |
| `OLED_MOSI` | `P0.20` | **`P1.03`** | `P0.20` ist am DK Flash |
| `OLED_PWR` | `P1.14` | **entfällt** | kein Lastschalter am Teststand |
| QSPI-Flash (6 Leitungen) | `P0.17`, `P0.19`, `P0.21`–`P0.24` | **entfällt** | der Flash des DK sitzt fest an `P0.17`, `P0.19`–`P0.23` |
| SD-Karte (4 Leitungen) | — | **`P1.08`, `P1.07`, `P1.06`, `P1.05`** | gibt es auf der Platine nicht |
| Taster | `P1.01` | — | die Taster des DK liegen auf `P0.11`/`P0.12`/`P0.24`/`P0.25` |
| LEDs | `P0.06`, `P0.08`, `P0.11` | — | die LEDs des DK liegen auf `P0.13`–`P0.16` |

**Vier Leitungen (⚠) liegen auf Pins, die das Datenblatt nur für langsame
Signale freigibt.** Solange der Teststand mit niedrigem Takt läuft, ist das
vertretbar; **belegt ist es nicht.** Wenn Anzeige oder Karte zickt, ist das
nach den beiden Spannungsfragen der nächste Verdacht.

## Prüfschritt vor dem Einstecken

**In dieser Reihenfolge. Erst messen, dann einstecken, dann einschalten.**
Beide USB-Kabel bleiben währenddessen **abgezogen**.

### A. Trocken, ohne jede Spannung — mit dem Durchgangsprüfer

1. **Keine Versorgung gegen Masse.** An jedem der drei Module: zwischen
   `VCC`/`+3.3V` und `GND` messen. **Es darf kein Durchgang sein.** Piept es,
   ist ein Draht falsch oder ein Modul hat einen Kurzschluss — dann nicht
   einstecken.
2. **Dasselbe am DK:** zwischen einem `VDD`-Stift und einem `GND`-Stift auf
   `P1` darf kein Durchgang sein.
3. **Versorgung am richtigen Stift**, für jedes Modul einzeln, **von der
   DK-Seite aus** durchgepiepst:
   - der Draht von `VDD` muss am Click auf `+3.3V` ankommen — **nicht** auf
     `+5V`, nicht auf `AN`, nicht auf `GND`;
   - der Draht zur Anzeige muss auf dem **mit `VCC` beschrifteten** Stift
     ankommen (Aufdruck lesen, nicht die Lage raten);
   - der Draht zur SD muss auf `VCC` ankommen;
   - und jeweils: der Massedraht auf `GND`.
4. **Kein 5-V-Stift ist mit irgendetwas verbunden.** Der `5V`-Stift auf `P1`
   bleibt unbenutzt; am Click bleibt `+5V` leer. Nachpiepsen, dass an keinem
   der drei Module ein Draht vom `5V`-Stift kommt. **Das ist die Prüfung, die
   den nRF52840 rettet.**
5. **Keine zwei Signale auf einem Stift.** Jeden Signaldraht einmal von Ende
   zu Ende durchpiepsen: DK-Pinname ↔ Modulbeschriftung, wie in den Tabellen
   oben. Dabei prüfen, dass **kein** Signaldraht Durchgang zu `VDD` oder `GND`
   hat.
6. **Nur `P3`, `P4` und `P1` sind gesteckt.** An `P5` und `P24` darf kein
   Draht hängen; auf `P4` bleiben `P0.02`, `P1.15`, `P1.14` frei, auf `P3`
   bleibt `P1.01` frei.

### B. Mit Spannung, aber noch ohne Module

7. Module **abziehen**. Nur das DK mit dem Debugger-Kabel (J2) einstecken.
8. Mit dem Spannungsmesser an `P1`: zwischen `VDD` und `GND` sollen rund
   **3 V** stehen (DK-UG, fester 3-V-Regler). Steht dort etwas anderes, erst
   das klären.

### C. Module dazu, eines nach dem anderen

9. DK spannungsfrei machen. **Erst das Click** anstecken, einschalten, Strom
   messen, Verbindung prüfen. Dann wieder aus.
10. **Dann die Anzeige** dazu — gleiches Spiel.
11. **Dann die SD-Karte** dazu — gleiches Spiel.
12. Zuletzt das zweite USB-Kabel an **J3** (USB-Peripherie), erst wenn FIDO2
    über USB gemessen werden soll. J2 ist der Debugger und bleibt für die
    Ausgabe gesteckt.

## Offene Punkte

1. **Die Stiftnummern je Leiste** sind nicht belegt — die Bildtafel zeigt nur
   die Reihenfolge. Es gilt der Aufdruck am Brett.
2. **Die Stiftreihenfolge des OLED-Moduls** ist nicht belegt. Das ist die
   gefährlichste Stelle des Plans.
3. **Der Grenzstrom der 3 V des DK** steht in keiner Nordic-Unterlage.
4. **Zwei fehlende Stromzahlen** (0,96"-Anzeige, SD-Karte) — ohne sie keine
   Summe.
5. **Vier Leitungen auf „nur langsam"-Pins** — vertretbar, nicht belegt.
6. **SB10–SB15 auftrennen?** Würde sechs schnelle Pins freimachen und den
   Teststand näher an die eigene Platine bringen. Nicht umkehrbar. Offen.
7. **Die Werkstellung von JP1/JP2 auf dem Click** muss am Brett nachgesehen
   werden.
