# Schaltplan-Entwurf

Stand 23.10.2026. **Noch nicht gezeichnet** — das macht ein späterer Strang mit
der EasyEDA-Brücke. Dieses Dokument ist so geschrieben, dass man es dort stur
abarbeiten kann: jedes Bauteil mit Kennung, jedes Netz mit beiden Enden.

Pinnamen und -nummern des Rechenkerns stehen in `pinout.md`. Bestellnummern und
Begründungen der Teilewahl in `../docs/parts.md`.

Quellen für jede Beschaltungsvorgabe sind im Text genannt. Was nicht belegt ist,
steht als `ungeprueft` dabei.

## Bauteilliste mit Kennungen

| Kennung | Teil | Gehäuse |
| --- | --- | --- |
| U1 | Raytac MDBT50Q-1MV2 (nRF52840) | Modul 15,5 × 10,5 × 2,05 mm |
| U2 | NXP SE051C2HQ1/Z01XDZ | HX2QFN20, 3 × 3 mm |
| U3 | Winbond W25Q512JVEIQ | WSON8 |
| U4 | Microchip MCP73832T-2ACI/OT | SOT-23-5 |
| U5 | ST USBLC6-2SC6 | SOT-23-6 |
| DS1 | Crystalfontz CFAL4864A-071BW (SSD1306) | FPC, 12-polig, 0,5 mm |
| J1 | GCT USB4085-GF-A | USB-C, Durchsteck |
| J2 | JST-PH 2.0, 2-polig, liegend | Akku |
| J3 | Abstimmbare NFC-Antenne, 2-polig | Flex oder Leiterbahn |
| J4 | FPC-ZIF 12-polig, 0,5 mm | Anzeige |
| J5 | Stiftfeld 1 × 6, 1,27 mm | Fingerabdrucksensor, Stufe 2 |
| J6 | Stiftfeld 1 × 4 oder Testpunkte | SWD |
| SW1 | Panasonic EVQP7A01P | SMD, 1,35 mm hoch |
| SW2 | Panasonic EVQP7A01P | Reset, optional |
| D1 | Everlight 19-337C/RSBHGHC-A88/4T | 1,6 × 1,6 × 0,5 mm, RGB |
| D2 | Nexperia PESD5V0X1BCL | DFN1006, TVS auf VBUS |
| Q1 | P-Kanal-MOSFET, Verpol- und NFC-Rückstromschutz | SOT-23 |
| Q2 | P-Kanal-MOSFET, Lastumschaltung USB | SOT-23 |
| Q3 | P-Kanal-MOSFET, Lastumschaltung Akku | SOT-23 |
| Q4 | P-Kanal-Lastschalter Anzeige | SOT-23 |
| Q5 | P-Kanal-Lastschalter Sensor | SOT-23 |
| X1 | Quarz 32,768 kHz | 3215 oder 2012 |
| L1 | Spule 10 µH, 0603, ≥ 80 mA | 0603 |

## Netzliste: Versorgung

### Die Grundentscheidung

**Der Akku und die USB-5 V gehen beide an `VDDH` des Rechenkerns, nicht an
`VDD`.** `VDD` endet bei 3,6 V (absolute Grenze 3,9 V), ein voller LiPo hat
4,2 V. `VDDH` reicht von 2,5 bis 5,5 V (nRF52840 PS v1.1, Tab. 172 und 173).
Der Chip erzeugt sein `VDD` selbst in der REG0-Stufe; **ein externer
Spannungsregler ist nicht nötig.**

Die 3,3-V-Schiene für Sicherheitschip, Flash und Anzeige kommt aus dem
**`VDD`-Ausgang** des Rechenkerns. **Damit gilt die Grenze `IEXT,HIGH` = 25 mA
bei Sendeleistung ≤ 4 dBm bzw. nur 5 mA über 4 dBm** (Abschn. 5.3.8.2).

### Entschieden: Sendeleistung höchstens 4 dBm, kein zweiter Wandler

Damit bleibt die 25-mA-Grenze gültig, und die Nebenverbraucher dürfen aus dem
internen `VDD` gespeist werden. Ein eigener Abwärts-Aufwärts-Wandler für eine
zweite 3,0-V-Schiene entfällt.

Begründung: Ein Schlüssel am Schlüsselbund redet mit einem Handy in Armlänge,
nicht durch zwei Wände. Ein Wandler, der nur Funkreichweite bringt, die niemand
braucht, wäre Vorrat — weniger Teile heißt weniger Platz, weniger Ruhestrom und
weniger Fehlerquellen.

**Was daraus folgt und eingehalten werden muss:**

- **Die Firmware begrenzt `TXPOWER` auf höchstens +4 dBm.** Das ist keine
  Empfehlung, sondern die Bedingung dafür, dass die 25 mA gelten. Über 4 dBm
  bleiben nur 5 mA, und damit ließe sich kein einziger Nebenverbraucher mehr
  betreiben.
- **Die Lastschalter Q4 (Anzeige) und Q5 (Sensor) sind Pflicht**, und die
  Firmware schaltet nie zwei große Verbraucher gleichzeitig: Anzeige **oder**
  Flash **oder** Signatur.

Der Verzicht kostet wenig: bei 0 dBm mit DC-DC-Wandler sind es 4,8 mA
Sendestrom, bei +8 dBm ohne Wandler 32,7 mA. Die verworfenen Alternativen
stehen in `../docs/power-budget.md`.

### Netze

| Netz | Bedeutung |
| --- | --- |
| `VBUS` | 5 V vom USB-Stecker |
| `VBAT` | Akku, 3,0–4,2 V, hinter Q1 |
| `VSYS` | gemeinsame Systemschiene an `VDDH`, 3,0–5,5 V |
| `VDD3V3` | `VDD`-Ausgang des Rechenkerns, 3,3 V |
| `VDD_OLED` | geschaltete 3,3 V für die Anzeige, hinter Q4 |
| `VDD_FP` | geschaltete 3,3 V für den Sensor, hinter Q5 |
| `SE_VOUT` | `Vout` des Sicherheitschips |
| `GND` | Masse |

### Ladepfad und Lastumschaltung

| von | nach | Bauteil / Wert |
| --- | --- | --- |
| J1 A4/B4/A9/B9 (VBUS) | `VBUS` | — |
| `VBUS` | U4 Pin 4 (VDD) | — |
| U4 Pin 4 | `GND` | C1 = 4,7 µF |
| U4 Pin 3 (VBAT) | `VBAT_RAW` | — |
| U4 Pin 3 | `GND` | C2 = 4,7 µF |
| U4 Pin 5 (PROG) | `GND` | **R1 = 10 kΩ → 100 mA Ladestrom** |
| U4 Pin 1 (STAT) | `P1.02` (CHG_STAT) | — |
| U4 Pin 1 | `VDD3V3` | R2 = 100 kΩ Pull-up |
| U4 Pin 2 (VSS) | `GND` | — |
| `VBAT_RAW` | J2 Pin 1 | Akku plus |
| J2 Pin 2 | `GND` | Akku minus |

Ladestrom nach `I = 1000 V / R_PROG` (Microchip DS20001984H, Abschn. 5.1.2).
Die Kondensatoren C1 und C2 sind mit **mindestens 4,7 µF** vorgeschrieben
(ebd., Abschn. 3.3/3.4). Die Typnummer `-2` liefert **4,20 V**
Ladeschlussspannung — `-3`, `-4` und `-5` würden eine normale LiPo überladen.

### Q1: Verpolschutz und NFC-Rückstromsperre in einem

| von | nach | Bauteil |
| --- | --- | --- |
| `VBAT_RAW` | `VBAT` | Q1, P-Kanal, Source an `VBAT_RAW`, Drain an `VBAT` |
| Q1 Gate | `GND` | R3 = 100 kΩ |

**Q1 hat zwei Aufgaben, und beide sind Pflicht:**

1. **Verpolschutz.** Die Schutzschaltung der Zelle schützt die Zelle, nicht die
   Platine. Eine verpolt angelötete Zelle legt die Spannung direkt auf den
   Laderegler und den Sicherheitschip.
2. **NFC-Rückstromsperre.** Das ist der Punkt, den man ohne Datenblatt nicht
   findet. nRF52840 PS v1.1, **Abschn. 6.14.11**: In einem starken NFC-Feld
   kann über parasitäre Dioden und ESD-Strukturen Strom **in die Versorgung
   zurückfließen**. Wörtlich: *„If the battery used does not tolerate return
   current, a series diode must be placed between the battery and the device in
   order to protect the battery."* **Ein LiPo verträgt Rückstrom nicht.**

Ein P-Kanal-MOSFET erfüllt beides mit deutlich weniger Spannungsabfall als eine
Diode — bei 3,0 V Zellspannung zählt jedes Millivolt.

> **`ungeprueft` und offen:** Die Bauteilauswahl für Q1 bis Q3 ist **nicht
> gerechnet**. Zu klären sind: Durchgangswiderstand bei 3,0 V
> Gate-Source-Spannung, Schwellenspannung (muss deutlich unter 3,0 V liegen),
> Rückwärts-Leckstrom im Ruhezustand (geht direkt ins 9,5-µA-Budget) und ob der
> Lastsprung beim Umschalten die Brownout-Erkennung auslöst. `VPOF,HV` ist im
> High-voltage-Betrieb zwischen 2,7 und 4,2 V einstellbar, ±5 %.

### Q2/Q3: am USB laufen, ohne den Akku zu belasten

| von | nach | Bauteil |
| --- | --- | --- |
| `VBUS` | `VSYS` | Q2, P-Kanal |
| `VBAT` | `VSYS` | Q3, P-Kanal |
| Q2/Q3 Gate | Kreuzsteuerung | R4, R5 = je 100 kΩ |

Liegt `VBUS` an, leitet Q2 und Q3 sperrt — die Last hängt am USB, der Akku wird
nur geladen. Ohne `VBUS` leitet Q3.

Diese diskrete Lösung ist bewusst gewählt: **alle vier geprüften
Power-Path-Lade-ICs brauchen Reflow** (Wärmepad muss Kontakt haben), und zwei
von ihnen (BQ24040, BQ25100) haben laut eigenem Datenblatt überhaupt keinen
Power-Path. Begründung in `../docs/parts.md`. Wenn es doch ein IC sein soll:
BQ24075.

### Entkopplung des Rechenkerns

| von | nach | Wert | Quelle |
| --- | --- | --- | --- |
| `VSYS` | U1 VDDH | — | — |
| `VSYS` | `GND` | C3 = 4,7 µF + C4 = 100 nF | — |
| U1 VDD | `GND` | **C5 = 4,7 µF** | `C_VDD` = 2,7/4,7/5,5 µF, Abschn. 5.3.8.1 |
| U1 DEC4 | `GND` | **C6 = 1 µF** | `C_DEC4` = 0,7/1/1,3 µF, ebd. |
| U1 DECUSB | `GND` | **C7 = 4,7 µF** | `C_DECUSB` = 2,35/4,7/5,5 µF, Abschn. 5.3.8.7 |
| U1 DCCH | L1 (10 µH) → U1 VDDH | **10 µH, 0603, ≥ 80 mA** | Raytac Ver. L, Kap. 8 |
| U1 VBUS | `VBUS` | — | — |

**L1 ist Pflicht, wenn der DC-DC-Wandler der REG0-Stufe genutzt werden soll** —
und er soll, denn er halbiert fast jeden Strom. Die Spule für REG1 und der
32-MHz-Quarz sind **im Raytac-Modul schon enthalten** (Raytac Ver. L, Kap. 3
und 8); nur L1 und der 32,768-kHz-Quarz sind extern.

Hinweis für den Chipaufbau, falls später ohne Modul gebaut wird: `DEC4` (Ball
B5) **muss** mit `DEC6` (Ball E24) verbunden werden, und das Die-Pad liegt auf
`VSS` und muss auf Masse. Beim Modul ist das intern erledigt (`ungeprueft`,
Pin-Tabelle prüfen).

### Uhrenquarz

| von | nach | Wert | Quelle |
| --- | --- | --- | --- |
| U1 XL1 (`P0.00`) | X1 Pin 1 | — | — |
| U1 XL2 (`P0.01`) | X1 Pin 2 | — | — |
| X1 Pin 1 | `GND` | C8 | Lastkapazität nach Quarzdatenblatt |
| X1 Pin 2 | `GND` | C9 | dto. |

Anforderungen an X1 (nRF52840 PS v1.1, Abschn. 5.4.4):
**Lastkapazität max. 12,5 pF, Shuntkapazität max. 2 pF, Serienwiderstand max.
100 kΩ, Antriebsleistung max. 0,5 µW; Toleranz für BLE ±500 ppm.**
Eingangskapazität der Pins: 4 pF. Startzeit 0,25 s. Laufstrom **0,23 µA**.

**X1 ist Pflicht, nicht optional.** Drei Gründe: BLE verlangt ±500 ppm, was der
chipinterne Oszillator nur kalibriert schafft; TOTP braucht eine laufende Uhr;
und der Quarz ist mit 0,23 µA sparsamer als der interne Oszillator mit 0,7 µA.
Beim Raytac-Modul ist er **extern** zu bestücken (X2, C12, C13 im
Referenzschaltbild).

C8 und C9 ergeben sich aus der Lastkapazität des gewählten Quarzes minus den
4 pF Pineingangskapazität — **noch nicht gerechnet**, weil der Quarz noch nicht
ausgewählt ist.

## Netzliste: Sicherheitschip

Pinnamen aus dem SE051 Datenblatt Rev. 2.0, Tabelle 3 (HX2QFN20).

| SE051C2 Pin | Signal | nach | Bemerkung |
| --- | --- | --- | --- |
| 1 | ISO14443 LB | `GND` | **Kontaktlos nicht genutzt** — siehe unten |
| 2 | n.c. | offen | — |
| 3 | ISO7816 IO1 | offen | nicht genutzt (empfohlen: offen) |
| 4–8 | n.c. | offen | — |
| **9** | **I2C_SDA** | `P0.26` | I2C-Daten |
| **10** | **I2C_SCL** | `P0.25` | I2C-Takt |
| **11** | **ENA** | `P1.10` | **Deep Power-down** |
| **12** | **VIN** | `VDD3V3` | Versorgung der Pads und der Logik |
| 13 | ISO7816 CLK | offen | nicht genutzt |
| 14 | ISO7816 RST_N | `VDD3V3` | wenn ISO7816 ungenutzt: auf VCC oder VSS |
| **15** | **Vout** | `SE_VOUT` | **muss auf der Platine an VCC** |
| 16 | ISO7816 IO2 | offen | I2C-Controller-Takt, nicht genutzt |
| 17 | ISO14443 LA | `GND` | siehe unten |
| **18** | **VCC** | `SE_VOUT` | **aus Vout, nicht aus VDD3V3** |
| 19 | VSS | `GND` | — |
| 20 | n.c. | offen | — |
| Mittelpad | — | `GND` | nicht verbunden, aber thermisch empfohlen |

| von | nach | Wert |
| --- | --- | --- |
| `VDD3V3` | `GND` | C10 = 100 nF, nahe Pin 12 |
| `SE_VOUT` | `GND` | C11 = 100 nF, nahe Pin 18 |

### Die wichtigste Beschaltungsvorgabe dieses Chips

**`VCC` (Pin 18) wird aus `Vout` (Pin 15) gespeist, nicht direkt aus der
3,3-V-Schiene.** Wörtlich im SE051 Datenblatt Rev. 2.0, Abschn. 5.2: *„For
usage of Deep Power-down mode the SE050 must be supplied via pin Vin and pin
VCC needs to be supplied by pin Vout."*

**Wer das falsch macht, verliert Faktor 143 im Ruhestrom:** Deep Power-down
zieht **3 µA**, der gewöhnliche Power-down **430 µA**. Bei einem Ruhebudget von
9,5 µA entscheidet diese eine Brücke über Monate Laufzeit. Das ist eine
Hardwarevorgabe und per Software nicht zu heilen.

### I2C-Abschlusswiderstände

| von | nach | Wert |
| --- | --- | --- |
| `P0.25` (SCL) | `VDD3V3` | **R6 = 2,2 kΩ** |
| `P0.26` (SDA) | `VDD3V3` | **R7 = 2,2 kΩ** |

Die Zieladresse des Chips ist werkseitig **0x48** (SE050 DS Rev. 3.8,
Abschn. 4.1). Das Protokoll ist T=1 über I2C.

Zur Taktrate, und das ist für die Widerstandswahl entscheidend: Der SE051
schafft bis **3,4 MHz im High-Speed-Modus, aber nur mit Clock Stretching**;
ohne Clock Stretching sind es **1 MHz** (SE050 DS Rev. 3.8, Abschn. 4.1.1). In
den Konfigurationen A, B, C, D und F ist Clock Stretching werkseitig **ein**
und setzt oberhalb von 600 kHz ein.

**Vorschlag: 400 kHz fahren.** Dann greift kein Clock Stretching, und 2,2 kΩ
reichen bequem. Höhere Takte lohnen nicht: die APDUs sind kurz, und die Zeit
geht in der Krypto-Rechnung des Chips drauf, nicht auf dem Bus.

> **`ungeprueft`:** 2,2 kΩ sind aus der Busspezifikation für 400 kHz bei
> geschätzter Leitungskapazität gewählt, **nicht aus einem Datenblatt
> gerechnet**. Auf einer Platine von wenigen Zentimetern ist das unkritisch;
> bei Problemen 1,5 kΩ probieren.

### Warum die Kontaktlos-Pins auf Masse gehen

Der SE051C2 **könnte** NFC selbst: AN12973 Rev. 2.0, Tabelle 3 nennt für ihn
„Contactless: ISO/IEC 14443 passive, type A", mit eigenen Antennenpins LA/LB
und 56 pF Resonanz-Eingangskapazität (SE051 DS Rev. 2.0, Abschn. 4.2).
**„passive" heißt: aus dem Feld versorgt, ohne Akku.**

Das ist verlockend, taugt für uns aber nicht:

- Über diesen Weg antwortet **NXPs IoT-Applet**, nicht unsere CTAP2-Firmware.
  **FIDO2 über NFC ist damit nicht möglich.**
- Die Anzeige könnte nicht zeigen, was freigegeben wird, und der Taster könnte
  nicht bestätigen — das widerspricht dem Bedrohungsmodell.
- Die geforderte Feldstärke von 1,5–7,5 A/m gilt laut Fußnote **für
  Klasse-1-Antennen** (SE051 DS Rev. 2.0, Tab. 7). Klasse 1 ist Kartengröße;
  eine Anhängerantenne ist deutlich kleiner. Ob das reicht, ist `ungeprueft`.

**Darum: NFC läuft über den Rechenkern, und LA/LB gehen auf Masse** — das
Datenblatt verlangt bei Nichtnutzung ausdrücklich den Anschluss an VSS.

Ein zweiter, passiver NFC-Pfad direkt am Sicherheitschip bleibt als Möglichkeit
für später; er bräuchte eine zweite Antenne oder eine Umschaltung und ist
**nicht entschieden**. Siehe `layout-notes.md`.

## Netzliste: Flash

Pinbelegung WSON8 nach Winbond W25Q512JV Rev. B.

| U3 Pin | Signal | nach |
| --- | --- | --- |
| 1 | /CS | `P0.17` |
| 2 | DO / IO1 | `P0.22` |
| 3 | /WP / IO2 | `P0.23` |
| 4 | GND | `GND` |
| 5 | DI / IO0 | `P0.21` |
| 6 | CLK | `P0.19` |
| 7 | /HOLD / IO3 | `P0.24` |
| 8 | VCC | `VDD3V3` |

| von | nach | Wert |
| --- | --- | --- |
| `VDD3V3` | `GND` | C12 = 100 nF, nahe Pin 8 |
| `P0.17` (/CS) | `VDD3V3` | R8 = 100 kΩ Pull-up |

R8 hält den Flash abgewählt, solange der Rechenkern noch im Reset ist.

**`WSON8 hat keinen /RESET-Pin.`** Zurückgesetzt wird nur über den Befehlssatz.
Das ist zu beachten, wenn der Flash einmal in einem unklaren Zustand hängt — es
gibt keinen Draht, mit dem man ihn herausholt.

**Taktgrenze:** Der QSPI des Rechenkerns deckelt bei **32 MHz**
(SCK = 32 MHz/(SCKFREQ+1), SCKFREQ 0–15). Die 133 MHz des Flash sind also nie
das Nadelöhr. Die 2,7–3,6 V des Flash passen direkt zur 3,3-V-Schiene — **kein
Pegelwandler**.

**Firmware-Pflicht:** Befehl **`B9h`** legt den Flash in Deep Power Down
(**1 µA** statt 10 µA im Standby). /CS hochziehen genügt nicht.

## Netzliste: Anzeige

DS1 ist das Glas CFAL4864A-071BW mit SSD1306, Anschluss über J4 (FPC-ZIF,
12-polig, 0,5 mm). Betrieb im **4-Draht-SPI-Modus**.

Pinbelegung vollständig aus dem **Crystalfontz-Datenblatt CFAL4864A-071BW,
Ausgabe 2017-07-05, Abschnitt 6 „Interface PIN Function"**:

| J4 Pin | Signal | nach | Bemerkung |
| --- | --- | --- | --- |
| 1 | GND | `GND` | **Masse 1 von 2 — beide müssen angeschlossen werden** |
| 2 | D2 | **offen** | nur im I2C-Betrieb nötig |
| 3 | **D1 / SDIN** | `P0.20` | Daten |
| 4 | **D0 / SCLK** | `P0.27` | Takt |
| 5 | **D/C#** | `P1.12` | Befehl oder Daten |
| 6 | **/RES#** | `P1.13` | Reset, low ≥ 3 µs |
| 7 | **CS#** | `P1.11` | Auswahl, low aktiv |
| 8 | **BS1** | `GND` | Betriebsartwahl |
| 9 | **BS0** | `GND` | Betriebsartwahl |
| 10 | **VDD** | `VDD_OLED` | 3,3 V nominal |
| 11 | **VBAT** | `VDD_OLED` | 3,3 V nominal |
| 12 | GND | `GND` | **Masse 2 von 2** |

| von | nach | Wert |
| --- | --- | --- |
| `VDD3V3` | `VDD_OLED` | Q4, P-Kanal-Lastschalter |
| Q4 Gate | `P1.14` | über R9 = 100 kΩ |
| `VDD_OLED` | `GND` | C13 = 1 µF + C14 = 100 nF |

Drei Dinge, die man aus der Tabelle leicht überliest:

- **`BS1` = 0 und `BS0` = 0 wählen den 4-Draht-SPI-Betrieb** (Datenblatt
  Abschn. 6, Tabelle am Pin 9). Beide Pins fest auf Masse, keine GPIOs nötig.
- **`VDD` (Pin 10) und `VBAT` (Pin 11) müssen miteinander verbunden werden** —
  das Datenblatt sagt es bei beiden Pins ausdrücklich: *„Connect VDD and VBAT
  together."* Beide an `VDD_OLED`.
- **Beide Massepins müssen angeschlossen werden**, nicht nur einer: *„Ground 1
  of 2, both must be connected."*

### Die Ladungspumpen-Frage ist geklärt: keine externen Kondensatoren

Das war der offene Punkt dieses Abschnitts. Das Datenblatt beantwortet ihn
eindeutig, und zwar durch das, was **nicht** am Steckverbinder liegt:

**Die 12 Pins sind GND, D2, D1, D0, D/C#, /RES#, CS#, BS1, BS0, VDD, VBAT,
GND. Es gibt keine Pins `C1P`, `C1N`, `C2P`, `C2N` — und auch keinen
`VCC`-Pin für die Panelspannung.**

Daraus folgt: **die Ladungspumpe und ihre Kondensatoren sitzen auf dem Modul.**
Das Modul erzeugt seine Panelspannung selbst aus `VBAT`. Bestätigt wird das
durch die Strommessung im Datenblatt, die mit **`VCC = 7,5 V`** als
Betriebsbedingung angegeben ist, obwohl `VCC` nirgends herausgeführt ist — die
7,5 V entstehen also intern.

**Auf unserer Platine sind keine Ladungspumpen-Kondensatoren nötig.** Es
braucht nur die 3,3-V-Zuführung und die Abblockkondensatoren C13/C14.

Elektrische Daten aus demselben Datenblatt:

| Größe | Wert |
| --- | --- |
| `VDD` Logik | 2,8 min / **3,3 typ** / 3,3 max |
| `VBAT`-Strom `IBAT` bei VCC = 7,5 V | **typ. 12,0 mA, max. 25,0 mA** |
| absolute Grenze `VDD` | **4 V** |
| absolute Grenze `VCC` | 15 V |
| Betriebstemperatur | −40 … +80 °C |

Die absolute Grenze von **4 V** auf `VDD` ist zu beachten: die Anzeige darf
**nicht** an `VSYS` (bis 5,5 V) hängen, sondern nur an der geregelten
3,3-V-Schiene. Das ist im Entwurf so, aber es ist eine Grenze, die man nicht
ausprobieren will.

**Warum der Lastschalter Q4:** Der Controller zieht im Schlaf bis zu 10 µA auf
`IDD` und bis zu 10 µA auf `ICC` (SSD1306 Rev 1.1, Tab. 12-1). Bei einem
Ruhebudget von 9,5 µA wären das bis zu 20 µA zusätzlich — eine Verdreifachung.
Q4 kostet ein Bauteil und bringt Faktor 3.

**Warum nicht aus `VDD3V3` dauerversorgen:** siehe `IEXT,HIGH` oben. Die
Anzeige darf mit ihren bis zu 25 mA nicht gleichzeitig mit dem Flash laufen.

## Netzliste: Bedienung und Anzeige

| von | nach | Bauteil / Wert |
| --- | --- | --- |
| SW1 Pin 1 | `P1.01` | Taster |
| SW1 Pin 2 | `GND` | — |
| SW2 Pin 1 | U1 `P0.18` (nRESET) | Reset-Taster, optional |
| SW2 Pin 2 | `GND` | — |

**Kein externer Pull-up am Taster** — der interne genügt: `R_PU` = 11/13/16 kΩ
(nRF52840 PS v1.1, GPIO-Elektrik S. 157). **Kein Entprellkondensator** — er
kostet im Schlaf Leckstrom; die 10 ms Prellzeit (Panasonic ANCTB21E) werden in
der Firmware entprellt.

Der Taster muss über **GPIOTE im PORT-Modus** wecken: 2,36 µA gegen 17,37 µA im
Event-Modus. Siehe `pinout.md`.

### RGB-LED

D1 hat **sechs Pins — drei getrennte LEDs**, weder gemeinsame Anode noch
Kathode (Everlight DSE-0021229, Rev. 4).

| von | nach | Wert |
| --- | --- | --- |
| `P0.06` | D1 Pin 4 (rot, Anode) | R10 |
| D1 Pin 3 (rot, Kathode) | `GND` | — |
| `P0.08` | D1 Pin 2 (grün, Anode) | R11 |
| D1 Pin 1 (grün, Kathode) | `GND` | — |
| `P0.11` | D1 Pin 6 (blau, Anode) | R12 |
| D1 Pin 5 (blau, Kathode) | `GND` | — |

**Die Vorwiderstände sind noch nicht festgelegt, und das hat einen Grund.**
Die Durchlassspannungen sind rot 1,7–2,4 V, **grün und blau 2,6–3,2 V** bei
20 mA. An einer 3,3-V-Schiene bleiben für grün und blau im ungünstigsten Fall
nur **0,1 V** über dem Widerstand — damit leuchten sie nicht verlässlich, und
mit entladener Zelle (3,0 V) schon gar nicht.

Drei Wege, noch zu entscheiden:

1. **Rot als Hauptfarbe** und grün/blau nur mit kleinem Strom als Nebenfarben.
   Billig, aber die Farben sind ungleich hell.
2. Grün und blau mit **wenigen Milliampere** betreiben — die Durchlassspannung
   sinkt mit dem Strom, und bei 2 mA reicht es (das Kingbright-Vergleichsteil
   ist bei 2 mA spezifiziert). Reicht für eine Statusanzeige.
3. **Nur rot und grün** verwenden und auf blau verzichten.

**Nicht gerechnet.** Das gehört in den Feinschliff am Schaltplan, sobald
entschieden ist, wie viele Zustände die LED zeigen muss.

**Beim Löten:** Der ESD-Schutz ist bei grün und blau nur **150 V** (rot
2000 V). Handlöten höchstens 350 °C für 3 s.

### Spannungsmessung am Akku

| von | nach | Wert |
| --- | --- | --- |
| `VBAT` | `P0.04` (AIN2) | R13 |
| `P0.04` | `GND` | R14 |

Der Teiler muss 4,2 V in den Messbereich des Analogwandlers bringen. **Noch
nicht gerechnet**, weil die Referenz und die Verstärkung noch nicht gewählt
sind. Wichtig: der Teiler zieht dauernd Strom. Bei 2 × 1 MΩ sind es 2,1 µA —
**fast ein Viertel des Ruhebudgets**. Darum entweder sehr hochohmig (dann
leidet die Genauigkeit) oder über einen eigenen Lastschalter nur zum Messen
einschalten. **Zu entscheiden.**

## Netzliste: USB-C

| J1 Pin | Signal | nach |
| --- | --- | --- |
| A1, A12, B1, B12 | GND | `GND` |
| A4, A9, B4, B9 | VBUS | `VBUS` |
| **A5** | **CC1** | **R15 = 5,1 kΩ nach `GND`** |
| **B5** | **CC2** | **R16 = 5,1 kΩ nach `GND`** |
| A6 | D+ | U5 Pin 1, von dort U5 Pin 6 → U1 D+ |
| A7 | D− | U5 Pin 3, von dort U5 Pin 4 → U1 D− |
| B6 | D+ | mit A6 verbinden |
| B7 | D− | mit A7 verbinden |
| A8, B8 | SBU1, SBU2 | offen |
| Schirm | — | `GND` |

### Zwei getrennte 5,1-kΩ-Widerstände, nicht einer

**Das ist der klassische Fehler, und er ist gratis zu vermeiden.** Die
Type-C-Spezifikation verlangt je CC-Pin einen eigenen Widerstand (Release 1.4,
Abschn. 4.5.1.3.2, Figure 4-9 — normativ).

Wer beide CC-Pins auf **einen** Widerstand legt — so wie der Raspberry Pi 4 mit
seinem R79 —, bekommt mit einem e-marked Kabel (SuperSpeed oder 5 A)
**überhaupt keine Spannung**: die Quelle kann den Ra des e-markers
(800 Ω–1,2 kΩ) nicht mehr vom Rd trennen und liefert 0 V statt 5 V.

> **`ungeprueft`:** Die Toleranz von Rd nennt keine erreichbare Quelle; die
> kursierenden ±20 % bzw. ±5 % gelten für Rp auf der Quellenseite. Auch
> Abschnitt und Tabellennummer in der aktuellen Fassung (Release 2.5,
> 08.04.2026) sind nicht belegt — die Spezifikation ist nur per Download zu
> bekommen. Belegt ist Release 1.4.

Das Gerät verhandelt kein Power Delivery. Ein reiner Rd-Sink darf nur ziehen,
was die Quelle per Rp anbietet; ohne Messung der CC-Spannung gilt
„Default USB Power". Bei 100 mA Ladestrom plus einigen Milliampere Systemstrom
bleiben wir deutlich darunter.

**`22 Ω in D+/D− sind nicht nötig`** — keine Quelle fordert sie für USB 2.0.
Die Pull-Widerstände auf den Datenleitungen stellt das USB-Peripherieteil des
Rechenkerns selbst.

### ESD

| von | nach | Bauteil |
| --- | --- | --- |
| U5 Pin 1 | D+ (Steckerseite, J1 A6/B6) | USBLC6-2SC6 |
| U5 Pin 6 | D+ (Geräteseite, U1 D+) | dieselbe Leitung, Durchgang |
| U5 Pin 3 | D− (Steckerseite, J1 A7/B7) | — |
| U5 Pin 4 | D− (Geräteseite, U1 D−) | dieselbe Leitung, Durchgang |
| U5 Pin 2 | `GND` | — |
| U5 Pin 5 | `VBUS` | — |
| `VBUS` | `GND` | D2 = PESD5V0X1BCL |

> **Berichtigung 24.10.2026 (Strang „Schaltplan zeichnen").** Die vorige Fassung
> dieser Tabelle legte **Pin 5 auf `GND`** und `VBUS` auf Pin 6, und sie führte
> D+ und D− auf je nur einen Pin. Beides war falsch: Pin 5 des USBLC6-2SC6 ist
> **die VBUS-Klemme**, und das Teil ist ein **Durchgangsteil** — je Datenleitung
> ein Pin zur Steckerseite und einer zur Geräteseite (I/O1 = Pin 1 und 6,
> I/O2 = Pin 3 und 4, GND = Pin 2, VBUS = Pin 5). So gebaut hätte die Platine
> beim ersten Einstecken **`VBUS` gegen Masse kurzgeschlossen**. Gefunden beim
> Durchrechnen der Netzliste vor dem Zeichnen; Zuordnung nach ST-Datenblatt.

U5 schützt beide Datenleitungen und klemmt `VBUS` mit (3,5 pF Kapazität je
Leitung, 150 nA Leckstrom, IEC 61000-4-2 Stufe 4: ±15 kV Luft, ±8 kV Kontakt).
Mit 5 A Stoßstrom ist er für `VBUS` dünn, darum D2 zusätzlich — und dabei
**VRWM 5,5 V** nehmen, nicht 5,0 V: ein Teil mit Durchbruch ab 5,8 V liegt zu
knapp an den USB-Toleranzen.

**Offen, und für einen Sicherheitsschlüssel nicht nebensächlich:** Das
**ESD-Konzept für die CC-Leitungen** und die Frage, ob der Schirm der Buchse
direkt oder über RC an Masse geht, sind **nicht recherchiert**. Beides gehört
geklärt, bevor gezeichnet wird.

## Netzliste: NFC-Antenne

| von | nach | Bauteil |
| --- | --- | --- |
| U1 `P0.09` (NFC1) | J3 Pin 1 | Antennenspule |
| U1 `P0.10` (NFC2) | J3 Pin 2 | — |
| `P0.09` | `GND` | **C15 = Abstimmkondensator** |
| `P0.10` | `GND` | **C16 = Abstimmkondensator** |

Vorgaben aus nRF52840 PS v1.1, **Abschn. 6.14.10 „NFCT antenna
recommendations"** — wörtlich:

1. *„The NFCT antenna coil must be connected differential between NFC1 and NFC2
   pins of the device."* — **differentiell, nicht einseitig gegen Masse.**
2. *„Two external capacitors should be used to tune the resonance of the antenna
   circuit to 13.56 MHz."* — **ein Abstimmkondensator je Pin.**
3. Richtwert: *„An antenna inductance of L_ant = 2 µH will give tuning
   capacitors in the range of 130 pF on each pin."*
4. *„The total capacitance on NFC1 and NFC2 must be matched."* — **beide Zweige
   müssen gleich sein.** C15 = C16, immer.

**Eine externe Schutzbegrenzung verlangt Nordic nicht** — der Schutz sitzt im
Chip und wird über `NFCPINS.PROTECT = NFC` eingeschaltet (schließt die Pins bei
etwa 2 V Differenz kurz). Grenzwerte: Strom in die NFC-Pins **max. 80 mA**
(absolute Grenze), Erkennungsschwelle `Vsense` = 1,2 V Spitze.

**Was dagegen Pflicht ist: die Serien-Diode bzw. Q1 im Akkupfad** gegen den
Rückstrom aus dem Feld (Abschn. 6.14.11) — siehe oben.

### Die Abstimmung gehört gemessen, nicht gerechnet

**Die Formel selbst ist im PDF eine Grafik und textlich nicht auslesbar —
insoweit `ungeprueft`.** Belegt sind nur das Ziel 13,56 MHz und das Wertepaar
2 µH → etwa 130 pF je Pin.

Eine Plausibilitätsrechnung zeigt, warum man nicht rechnen soll: zwei gleiche
Abstimmkondensatoren in Reihe über der Spule ergeben 65 pF, damit käme man auf
**13,96 MHz** — nicht 13,56. Die Lücke schließen die parasitären und die
chipinternen Kapazitäten (`C_PAD_NFC` = 4 pF). **Die Abstimmung ist also eine
Messung.**

Nordics Verfahren (DevZone „NFC tag antenna tuning"): NFCT zuerst scharf
schalten — für den nRF52840 `0x4000568C = 0x38D48`, dann `0x40005000 = 1` —,
dann mit Ein-Port-Netzwerkanalysator, Koppelspule 1–2 cm entfernt, bei etwa
−10 dBm über 10–15 MHz wobbeln und die S11-Senke suchen. C15 und C16 immer
gleich halten.

> **Warnung aus der Quelle selbst:** Der Nordic-Blogbeitrag ist in der
> Richtungsangabe (Kapazität erhöhen oder senken) **in sich widersprüchlich**.
> An der eigenen Messung orientieren, nicht am Text.

**Es gibt keine eigene Application Note von Nordic zum NFC-Antennenentwurf für
den nRF52840** — nur den PS-Abschnitt und den Blogbeitrag. Ein Whitepaper
nWP-026 wird erwähnt, war aber nicht auffindbar (`ungeprueft`, und es gehört
wohl zum nRF52833).

### Referenz: das Entwicklungsboard

Das nRF52840-DK (PCA10056) ist das offizielle Referenzdesign: NFC-Antenne an
Steckverbinder **J5**. **Dem Board liegt eine NFC-Antenne bei** (Nordic,
Kit-Inhalt) — der Entwurf nahm an, man müsse eine dazukaufen; das stimmt nicht.

Um die NFC-Pins dort als normale GPIOs zu nutzen, müssen **R44 und R46
unbestückt und R43/R45 mit 0 Ω gebrückt** werden. Im DK-Schaltplan sollen
**300 pF** Abstimmkondensatoren von NFC1 bzw. NFC2 nach Masse sitzen.

> **`ungeprueft`:** Diese Bauteilnummern und die 300 pF stammen aus einem
> DevZone-Beitrag und der Board-Dokumentation, **nicht aus einem selbst
> geöffneten Schaltplan**. Am Schaltplan der eigenen Board-Revision nachsehen.

## Netzliste: Fingerabdrucksensor (Stufe 2, Platz vorgesehen)

J5, Stiftfeld 1 × 6 im Raster 1,27 mm. **Entschieden: in Stufe 1 nicht
bestückt.** Der Freigabeweg ist Taster + PIN.

Der tragende Grund steht in `../docs/hardware.md`: **bei FIDO2 ersetzt die
Biometrie die PIN, sie ist kein zusätzlicher Faktor.** Dazu kam, dass kein
Kandidat alle vier Anforderungen erfüllt und der Host-Befehlssatz des
aussichtsreichsten Teils (FPC BM-Lite) nicht offen ist (`../docs/parts.md`).

| J5 Pin | Signal | nach |
| --- | --- | --- |
| 1 | `VDD_FP` | über Q5 aus `VDD3V3` |
| 2 | `GND` | `GND` |
| 3 | FP_SCK | `P0.13` |
| 4 | FP_MOSI | `P0.14` |
| 5 | FP_MISO | `P0.15` |
| 6 | FP_CS | `P0.16` |

| von | nach | Wert |
| --- | --- | --- |
| `VDD3V3` | `VDD_FP` | Q5, P-Kanal-Lastschalter |
| Q5 Gate | `P1.15` | über R17 = 100 kΩ |
| `P1.03` | FP_IRQ | eigener Draht oder Testpunkt |

**Was trotzdem bestückt wird: Q5 und R17.** Der Lastschalter kostet zwei
Bauteile und spart Stufe 2 eine Änderung — ohne ihn brächte der Sensor 13 µA
Dauerstrom mit, mehr als unser gesamtes Ruhebudget von 9,5 µA. Das Stiftfeld J5
und die sechs Leitungen werden **verlegt, aber nicht bestückt**; das kostet
fast nichts und hält die Tür offen.

## SWD-Programmierstecker

| J6 Pin | Signal |
| --- | --- |
| 1 | `VDD3V3` |
| 2 | SWDIO |
| 3 | SWDCLK |
| 4 | `GND` |

Als Testpunkte oder Stiftfeld. **Wichtig für später:** Der signierte
Startlader muss stehen, **bevor** der Leseschutz eingeschaltet wird — danach
gibt es keinen Weg zurück. Das gehört dem Firmware-Strang; hier steht nur, dass
die Kontakte auf der Platine erreichbar bleiben müssen, solange entwickelt
wird.

## Was offen bleibt

In der Reihenfolge, in der es das Zeichnen behindert:

1. **Die Vorwiderstände der LED** und ob blau überhaupt sinnvoll ist.
2. **Die Bauteilauswahl für Q1 bis Q5** samt Leckstrom im Ruhezustand und dem
   Nachweis, dass die Lastumschaltung keinen Brownout auslöst.
3. **Der Spannungsteiler für die Akkumessung** und ob er geschaltet wird.
4. **Die Lastkapazität des Uhrenquarzes** — Quarz noch nicht ausgewählt.
5. **Das ESD-Konzept für die CC-Leitungen** und die Schirmanbindung.
6. **Die Raytac-Modulpinnummern** für die Signale, die in `pinout.md` noch
   keine haben — am gerenderten PDF nachzutragen, bevor das Bauteil gezeichnet
   wird.
7. Die Werte der NFC-Kondensatoren C19/C21 im Raytac-Referenzschaltbild stehen
   nur in der Grafik und waren nicht auslesbar (`ungeprueft`).

**Erledigt:** Die Ladungspumpen-Frage der Anzeige ist am Datenblatt geklärt —
die Kondensatoren sitzen auf dem Modul, auf unserer Platine ist nichts nötig.
Die Sendeleistung ist auf höchstens 4 dBm entschieden, ein zweiter
Spannungswandler entfällt.

## Festlegungen vom 24.10.2026 (vor dem Zeichnen)

Die Punkte 1 bis 3 der Liste „Was offen bleibt" sind entschieden, Punkt 5
ebenfalls. Damit ist der Schaltplan zeichenbar.

| Punkt | Festlegung | Begründung |
| --- | --- | --- |
| Q1–Q5 | **Diodes DMG2305UX**, SOT-23, P-Kanal, −20 V, −3,8 A, Schwelle ab −0,9 V | logiktauglich an 3,0 V Gate-Source, gut lieferbar, billig |
| R10 (rot) | **1 kΩ** `vorlaeufig` | am Muster feinabstimmen; LED leuchtet am Schlüsselbund, nicht im Serverschrank |
| R11 (grün), R12 (blau) | **je 470 Ω** `vorlaeufig` | dto.; alle drei Farben werden gezeichnet, die 19-337C hat sie ohnehin im Gehäuse |
| R13, R14 | **je 2,2 MΩ**, dazu **C17 = 10 nF** von `VBAT_SENSE` nach `GND` | 0,95 µA Dauerstrom statt 2,1 µA bei 2 × 1 MΩ; C17 lädt den Abtast-Kondensator des Wandlers trotz hoher Quellimpedanz |
| Lastschalter für den Teiler | **entfällt** | spart die letzte µA, kostet aber Bauteil, Leitung und Fehlerquelle an einer Stelle, die nur Komfort liefert |
| Q2/Q3-Kreuzsteuerung | Q2-Gate über R4 an `VBAT`, Q3-Gate über R5 an `VBUS` | übliche Lastumschaltung; in der vorigen Fassung nur „Kreuzsteuerung" genannt, nicht gezeichnet |
| FP_IRQ (`P1.03`) | **Testpunkt**, nicht an J5 | J5 hat 6 Pins, alle belegt |
| CC1/CC2 | **kein eigener ESD-Schutz** | der USBLC6 deckt die Datenleitungen ab; CC führt keine schnellen Signale |
| Schirm von J1 | direkt an `GND` | RC-Anbindung bleibt eine Layout-Frage |
| C8/C9, C15/C16 | als `TBD` gezeichnet | Quarz nicht ausgewählt, NFC-Abstimmung ist eine Messung |

Nachtrag zur Bauteilliste: **C17 = 10 nF** kommt hinzu (Akkumessung).

## Offene Punkte an den Steckverbindern (24.10.2026)

### J4 getauscht: echtes Teil, gleiche Pins

`J4` war ein Bibliotheks-Platzhalter (`C9900036520`, Name
`FPC-05mm-12P-14241555_2`). **Jetzt eingebaut: Hirose
`FH12-12S-0.5SH(55)`, LCSC `C88360`** — mit Datenblatt, 0,5 mm Raster,
**2 mm Bauhöhe**, Deckelverschluss, Unterkontakt, Folie 0,3 mm, Goldkontakte.

**An der Netzliste ändert sich nichts.** Beide Teile haben **14 Pins**: die
zwölf Kontakte und zwei Haltelaschen. Die Pinbelegung dieses Abschnitts bleibt
also gültig, `J4.13` und `J4.14` liegen weiter an `GND`.

> **Berichtigung 24.10.2026:** Ich hatte gemeldet, das echte Teil habe „12
> Pole statt 14", und daraus geschlossen, die beiden Laschen fielen weg. **Das
> war falsch.** Verglichen hatte ich die **Kontaktzahl** des echten Teils
> („Number of Contacts: 12P") mit der **Pinzahl** des Platzhalters. Das sind
> zwei verschiedene Größen, und EasyEDAs Ausgabedatei führt beide in derselben
> Zeile: `Number of Contacts: 12P` neben 14 Pins.
>
> **Es ist genau der Fehler, den ich einen Absatz vorher bei J2 aufgedeckt
> hatte** — dort waren 7,9 und 8,6 mm nicht zwei widersprüchliche Angaben,
> sondern Länge und Tiefe. Zwei Zahlen zu vergleichen, die dasselbe Ding
> verschieden messen, ist dieselbe Art Fehler wie zwei Zeitangaben ohne ihre
> Zeitzone nebeneinanderzulegen.

Gegengeprüft nach dem Tausch, aus EasyEDAs eigener Ausgabe gezählt:
**54 Bauteile, 59 Netze, 219 Verbindungen** — dieselben Zahlen wie vorher.
DRC: **0 Fehler**, eine Warnung (`FP_IRQ` mit einem Pin, der gewollte
Testpunkt).

### J5 passt nicht zum vorgesehenen Sensor — ungelöst

`J5` ist in diesem Dokument ein **Stiftfeld 1 × 6 im Raster 1,27 mm**. Das
passt zum FPC **BM-Lite**. Der Sensor, auf den `../docs/fingerprint.md`
hinausläuft, ist aber der **FPC2534AM als Flex-Modul mit einem 12-poligen
ZIF-Anschluss** (dort Abschnitt 7.1 und der Punkt 7 der Schlussliste:
„J5 passt nicht zum AllKey").

> **Das ist eine Schaltplanfrage, keine Layoutfrage.** Ein 1 × 6 Stiftfeld
> und ein 12-poliger Flex sind nicht dasselbe Bauteil und nicht dieselbe
> Netzliste. Welcher Sensor es wird, steht nicht fest, und **solange das offen
> ist, ist jede Festlegung an J5 geraten.**

**Nicht geändert.** J5 bleibt als 1 × 6 im Plan und in Stufe 1 unbestückt.
Entschieden werden muss zuerst der Sensor; danach ist J5 entweder richtig
(BM-Lite) oder er wird gegen einen 12-poligen FPC-Steckverbinder getauscht,
samt der Netzliste dazu. **Was dann an Signalen über die zwölf Pole läuft,
ist aus dem Datenblatt zu holen und nicht aus der Zahl 12 zu erraten.**
