# Pinbelegung

Stand 23.10.2026. Entwurf auf Papier, gegen das Datenblatt geprüft, **nicht
aufgebaut**.

Quellen:

- **nRF52840 Product Specification v1.1, Doc-ID 4413_417, 28.02.2019**,
  Kapitel 7.1.1 „aQFN73 ball assignments", **Tabelle 142** (S. 576–578), dazu
  Abschn. 4.5.1.4 (`PSELRESET`), 4.5.1.6 (`NFCPINS`), 6.14.3 (NFC-Pins als
  GPIO). Fassung v1.11 war nicht beschaffbar; Schlaf- und CPU-Tabelle wurden
  gegen die heutige Online-Fassung als gleich geprüft, der Rest ist für v1.11
  `ungeprueft`.
- **Raytac MDBT50Q-1MV2 Approval Sheet Version L, 24.05.2023**, Abschn. 2.5
  (Pin-Tabelle), 2.6 (GPIO nahe dem Funkteil), Kap. 8 (Referenzschaltbild).

## Antwort zuerst: ja, es geht auf — mit einer Einschränkung

Gebraucht werden 20 Leitungen. Verfügbar sind 48 GPIOs, davon 21 für schnelle
Signale geeignet. **Es passt mit reichlich Luft.** Die einzige echte Kollision
ist `P0.18`, das gleichzeitig Reset-Eingang und empfohlener QSPI-Auswahlpin
ist — dazu unten eine Entscheidung.

## Wie viele Pins es gibt

- **nRF52840 im aQFN73: 48 GPIOs** — `P0.00`–`P0.31` (32) und `P1.00`–`P1.15`
  (16). Das Datenblatt nennt die Zahl nicht; sie ergibt sich aus Tabelle 142.
- **Raytac MDBT50Q-1MV2: 61 Modulpins**, nach der Pin-Tabelle sind **alle 48
  GPIOs herausgeführt**, dazu GND, VDD, VDDH, DCCH, VBUS, D+, D−, SWDIO,
  SWDCLK. Die Rechnung geht genau auf: 48 GPIOs + 5 × GND (1, 2, 15, 33, 55)
  + VDD + VDDH + DCCH + VBUS + D+ + D− + SWDIO + SWDCLK = 61.

  **`XL1` und `XL2` sind keine eigenen Modulpins**, sondern die Zweitfunktion
  von `P0.00` und `P0.01` — zwei der 48 GPIOs, keine zusätzlichen.

> **Geprüft am 24.10.2026** (Approval Sheet Abschn. 2.5, gerenderte Seite 17,
> Fassungen K und H, wortgleich): (17) P0.00/XL1, (18) P0.01/XL2, (30) VDDH,
> (31) DCCH, (32) VBUS, (34) D−, (35) D+, (40) P0.18/nRESET, (41) P0.17,
> (42) P0.19, (43) P0.21, (44) P0.20, (45) P0.23, (46) P0.22, (47) P1.00,
> (52) P0.09/NFC1, (54) P0.10/NFC2.
>
> **Berichtigung:** Die vorige Fassung dieser Datei nannte **(20) XL1 und
> (23) XL2** und bezeichnete das als sicher. Das war falsch — (20) ist
> `P0.04/AIN2`, (23) ist `P0.07/TRACECLK`. Wer danach zeichnet, legt den
> Uhrenquarz an zwei beliebige GPIOs: **die Uhr läuft nicht und TOTP ist tot.**
> Gefunden beim Zeichnen des Schaltplans, weil das Bibliothekssymbol 17/18
> zeigte; danach am Datenblatt bestätigt. Die übrigen zehn Nummern der alten
> Liste waren richtig.
>
> Die vollständige Zuordnung Modulpin → Signal steht in `schematic-notes.md`.

## Welche Pins nicht frei verwendbar sind

### Gar keine GPIOs

| Signal | Lage | Bemerkung |
| --- | --- | --- |
| `D+` / `D−` | eigene Pins (Balls AD6 / AD4) | **fest, nicht verlegbar** |
| `XC1` / `XC2` | eigene Pins | 32-MHz-Quarz — **im Raytac-Modul schon drin** |
| `SWDIO` / `SWDCLK` | eigene Pins | Debug |
| VBUS, VDDH, DCC, DCCH, DECUSB, DEC1–DEC6 | Versorgung | **Nur der nackte Chip.** Das Raytac-Modul führt von diesen nur VBUS, VDDH und DCCH heraus — **kein DECUSB, kein DEC1–DEC6**: die Entkopplung sitzt im Modul. Die Kondensatoren C6 (DEC4) und C7 (DECUSB) aus `schematic.md` entfallen darum. |

### Pins mit Vorbelegung

| Pin | Vorbelegung | Was zu beachten ist |
| --- | --- | --- |
| **`P0.18`** | **nRESET** | Zuordnung über UICR `PSELRESET[0]` (0x200) und `PSELRESET[1]` (0x204). **Beide Register müssen denselben Wert tragen**, sonst gibt es gar keine Reset-Funktion. Werkswert: PIN = 18, PORT = 0. |
| **`P0.09`** | **NFC1** | siehe unten |
| **`P0.10`** | **NFC2** | siehe unten |
| **`P0.00` / `P0.01`** | **XL1 / XL2** | 32,768-kHz-Quarz. Beim Raytac-Modul **extern** nötig (X2, C12, C13). Für TOTP Pflicht. |
| `P0.07`, `P1.09`, `P0.11`, `P0.12`, `P1.00` | TRACECLK, TRACEDATA3..0 | nur nötig, wenn man Ablaufverfolgung will — nehmen wir nicht |

### Pins, die nur langsam dürfen — das ist die eigentliche Falle

Tabelle 142 markiert viele Pins mit **„Standard drive, low frequency I/O
only"**, mit der Fußnote: *„Low frequency I/O is signals with a frequency up to
10 kHz."*

Raytac wird in Abschn. 2.6 noch deutlicher und nennt den Punkt, auf den es
ankommt:

> *„SPI, I2C, UART, PWM are NOT low frequency I/O."*

**Auf diesen Pins darf also kein SPI, kein I2C, kein UART und kein PWM
liegen.** Betroffen sind:

- **der gesamte Port 1 außer `P1.08` und `P1.09`** — also `P1.00`–`P1.07` und
  `P1.10`–`P1.15`
- **alle acht Analogeingänge**: `P0.02` (AIN0), `P0.03` (AIN1), `P0.04` (AIN2),
  `P0.05` (AIN3), `P0.28` (AIN4), `P0.29` (AIN5), `P0.30` (AIN6), `P0.31` (AIN7)
- **die NFC-Pins** `P0.09` und `P0.10`

Das ist auch der Grund, warum eine **mit PWM gedimmte LED nicht auf Port 1
darf** — ein Punkt, den man leicht übersieht, weil die LED „ja nur eine LED"
ist.

### Daraus ergibt sich das Pinbudget

**Für schnelle Signale (SPI, I2C, QSPI, UART, PWM) geeignet — 21 Pins:**

`P0.06`, `P0.07`, `P0.08`, `P0.11`, `P0.12`, `P0.13`, `P0.14`, `P0.15`,
`P0.16`, `P0.17`, `P0.19`, `P0.20`, `P0.21`, `P0.22`, `P0.23`, `P0.24`,
`P0.25`, `P0.26`, `P0.27`, `P1.08`, `P1.09`
(dazu `P0.18`, wenn man den Reset aufgibt)

**Nur für langsame Signale (Taster, Enable, Reset, Statusleitungen, Analog) —
24 Pins:**

`P0.02`–`P0.05`, `P0.28`–`P0.31` (die acht Analogeingänge),
`P1.00`–`P1.07`, `P1.10`–`P1.15`

## Die Kollision auf P0.18 und ihre Auflösung

`P0.18` ist zugleich:

- der **Reset-Eingang** im Werkszustand (`PSELRESET`), und
- in Tabelle 142 als **„Recommended usage: QSPI/CSN"** ausgewiesen.

Nordics QSPI-Empfehlung lautet insgesamt: `P0.18` = CSN, `P0.19` = SCK,
`P0.21`, `P0.22`, `P0.23`, `P1.00` = Datenleitungen.

**Entscheidung: der Reset bleibt auf `P0.18`, die Auswahlleitung des Flash
wandert auf `P0.17`.**

Begründung:

- Die Signalzuordnung des nRF52840 ist frei („All pins can be individually
  mapped to interface blocks for layout flexibility"); QSPI ist **nicht** fest
  verdrahtet.
- Von den sechs QSPI-Leitungen ist **CSN die unkritischste** — sie wechselt
  einmal je Zugriff, während SCK mit bis zu 32 MHz läuft. SCK und die vier
  Datenleitungen bleiben auf den empfohlenen Pins.
- Der Reset ist beim Entwickeln wertvoll, und `P1.00` fällt als Datenleitung
  ohnehin weg (Port 1, nur langsam) — es müssen also sowieso Pins verschoben
  werden.

> **`ungeprueft`:** Dass CSN auf `P0.17` genauso schnell ist wie auf `P0.18`,
> steht in keinem Datenblatt. Nordic nennt die Pins als Empfehlung, **nicht**
> als Leistungsaussage; die QSPI-Zeittabelle (Abschn. 6.19.11) enthält nur
> Zeiten und keine Pin-Beschränkung. Falls der Flash Probleme macht, ist das
> der erste Verdacht — dann den Reset aufgeben und CSN auf `P0.18` legen.

**Hinweis:** `P1.00` ist in Nordics QSPI-Empfehlung enthalten, liegt aber auf
Port 1 und darf damit laut Raytac kein schnelles Signal führen. Das ist ein
**Widerspruch zwischen Nordics Empfehlung und Raytacs Warnung**; wir folgen
Raytac, weil es das verbaute Modul beschreibt, und legen die vierte
Datenleitung auf `P0.24`.

## Die NFC-Pins

`P0.09` (NFC1, Ball L24) und `P0.10` (NFC2, Ball J24).

**Im Werkszustand sind sie NFC-Pins:** Das UICR-Register `NFCPINS` (Offset
0x20C) hat den Rücksetzwert 0xFFFFFFFF, Feld `PROTECT` ist damit `NFC (1)`.
Wer sie als GPIO will, muss UICR **aktiv schreiben** — und **es ist ein Reset
nötig**, denn „the content of the NFCPINS register is reloaded at every reset"
(Abschn. 6.14.3). UICR wird wie Flash geschrieben und nur per Chip-Erase
gelöscht.

**Wir brauchen NFC, also bleibt `PROTECT = NFC`.** Das ist auch die sichere
Wahl: in dieser Stellung schließt eine Schutzschaltung im Chip die beiden Pins
kurz, sobald die Differenzspannung etwa 2 V übersteigt. Bei `PROTECT =
Disabled` ist der Chip **nicht** gegen starke NFC-Felder geschützt.

**Wenn man sie doch einmal als GPIO brauchen sollte**, zwei Fallen aus
Abschn. 6.14.3 und der GPIO-Elektrik (S. 157):

- höhere Pad-Kapazität: `C_PAD_NFC` = **4 pF** gegen 3 pF normal
- **`I_NFC_LEAK` = 1 µA typisch, 10 µA maximal** Leckstrom zwischen den beiden
  Pads, wenn sie auf **unterschiedliche** Pegel getrieben werden. Nordic
  schreibt ausdrücklich, dass vor dem Schlafen beide Pins auf **denselben**
  Pegel zu legen sind. Bei unserem Ruhebudget von 9,5 µA wären 10 µA Leckstrom
  **mehr als eine Verdopplung** — das ist die teuerste Kleinigkeit im ganzen
  Entwurf.

## Die Festlegung

| nRF52840 | Raytac-Pin | Signal | Ziel | Art |
| --- | --- | --- | --- | --- |
| **Takt und Reset** | | | | |
| `P0.00` | **(17)** | XL1 | 32,768-kHz-Quarz | Quarz |
| `P0.01` | **(18)** | XL2 | 32,768-kHz-Quarz | Quarz |
| `P0.18` | (40) | **nRESET** | Reset-Taster / Programmierstecker | langsam |
| **Flash, QSPI** | | | | |
| `P0.17` | (41) | QSPI_CSN | Flash /CS | **schnell** |
| `P0.19` | (42) | QSPI_SCK | Flash CLK | **schnell** |
| `P0.21` | (43) | QSPI_IO0 | Flash DI/IO0 | **schnell** |
| `P0.22` | (46) | QSPI_IO1 | Flash DO/IO1 | **schnell** |
| `P0.23` | (45) | QSPI_IO2 | Flash /WP, IO2 | **schnell** |
| `P0.24` | — | QSPI_IO3 | Flash /HOLD, IO3 | **schnell** |
| **Sicherheitschip, I2C** | | | | |
| `P0.25` | — | I2C_SCL | SE051C2 Pin 10 | **schnell** |
| `P0.26` | — | I2C_SDA | SE051C2 Pin 9 | **schnell** |
| `P1.10` | — | SE_ENA | SE051C2 Pin 11 (Deep Power-down) | langsam |
| **Anzeige, SPI** | | | | |
| `P0.27` | — | OLED_SCK | SSD1306 D0/SCLK | **schnell** |
| `P0.20` | (44) | OLED_MOSI | SSD1306 D1/SDIN | **schnell** |
| `P1.11` | — | OLED_CS | SSD1306 /CS | langsam |
| `P1.12` | — | OLED_DC | SSD1306 D/C | langsam |
| `P1.13` | — | OLED_RES | SSD1306 /RES | langsam |
| `P1.14` | — | OLED_PWR | Lastschalter Anzeige | langsam |
| **Bedienung und Anzeige** | | | | |
| `P1.01` | — | **BUTTON** | Taster EVQP7A01P | langsam, **GPIOTE-PORT** |
| `P0.06` | — | LED_R | RGB-LED rot | schnell (PWM) |
| `P0.08` | — | LED_G | RGB-LED grün | schnell (PWM) |
| `P0.11` | — | LED_B | RGB-LED blau | schnell (PWM) |
| **Versorgung überwachen** | | | | |
| `P0.04` | — | VBAT_SENSE (AIN2) | Spannungsteiler am Akku | **nur analog** |
| `P1.02` | — | CHG_STAT | MCP73832 STAT, Pull-up | langsam |
| **NFC** | | | | |
| `P0.09` | (52) | NFC1 | Antennenspule | **NFC** |
| `P0.10` | (54) | NFC2 | Antennenspule | **NFC** |
| **USB** | | | | |
| `D+` | — | USB_DP | USB-C A6/B6 | fest |
| `D−` | — | USB_DM | USB-C A7/B7 | fest |
| **Fingerabdruck (vorgesehen, Stufe 2)** | | | | |
| `P0.13` | — | FP_SCK | BM-Lite SPI SCK | **schnell** |
| `P0.14` | — | FP_MOSI | BM-Lite SPI MOSI | **schnell** |
| `P0.15` | — | FP_MISO | BM-Lite SPI MISO | **schnell** |
| `P0.16` | — | FP_CS | BM-Lite /CS | **schnell** |
| `P1.03` | — | FP_IRQ | BM-Lite Interrupt | langsam |
| `P1.15` | — | FP_PWR | Lastschalter Sensor | langsam |

### Was nicht verdrahtet werden muss

- **VBUS-Erkennung braucht keinen GPIO.** Der nRF52840 erkennt USB selbst; die
  Firmware liest `USBREGSTATUS`. Schwellen: erkannt bei steigend
  3,4/4,0/4,3 V, weg bei fallend 3,0/3,6/3,9 V (Abschn. 5.3.8.5). Das spart
  einen Pin und einen Teiler.
- `SWDIO`/`SWDCLK` liegen auf eigenen Pins.

### Bilanz

| | gebraucht | vorhanden | frei |
| --- | --- | --- | --- |
| schnelle Pins | 14 (davon 4 für den Sensor) | 21 | **7** |
| langsame Pins | 9 | 24 | **15** |
| Analogeingänge | 1 | 8 | 7 |
| NFC | 2 | 2 | 0 |

**Es geht auf, auch mit Fingerabdrucksensor.** Ohne Sensor bleiben 11 schnelle
und 17 langsame Pins frei — reichlich Platz für eine zweite Ausbaustufe.

## Zwei Dinge, die in die Firmware müssen

Sie stehen hier, weil sie aus der Pinwahl folgen:

1. **Der Taster muss über GPIOTE im PORT-Modus wecken, nicht im Event-Modus.**
   2,36 µA gegen **17,37 µA** (PS v1.1, Abschn. 5.2.1.1). Bei einem
   Ruhebudget von 9,5 µA ist der Event-Modus allein teurer als das ganze
   übrige Gerät. GPIOTE hat 8 Kanäle, das reicht.
2. **Die Prellzeit des Tasters ist mit max. 10 ms angegeben** (Panasonic
   ANCTB21E). Entprellen gehört in die Firmware, nicht in die Hardware — ein
   Kondensator am Taster kostet im Schlaf Leckstrom.

Dazu die eingebauten Pull-Widerstände: `R_PU` = `R_PD` = **11/13/16 kΩ**
(S. 157). Ein interner Pull-up am Taster reicht; ein externer ist unnötig.

## Offene Punkte

1. **Erledigt am 24.10.2026.** Die Raytac-Pinzuordnung ist am gerenderten PDF
   geprüft (Abschn. 2.5, Seite 17, Fassungen K und H). Dabei fiel der Fehler
   bei XL1/XL2 auf (oben berichtigt). Alle Modulpinnummern, die hier fehlten,
   stehen jetzt in `schematic-notes.md` und sind so gezeichnet.
2. **`P1.00` als QSPI-Datenleitung**: Nordic empfiehlt es, Raytac verbietet
   schnelle Signale auf Port 1. Wir folgen Raytac. Ob Nordics Empfehlung
   dennoch gilt, ist `ungeprueft`.
3. **CSN auf `P0.17` statt `P0.18`**: als gleichwertig nicht belegt.
4. **Die Betriebsströme von QSPI, I2C und SPI gibt Nordic in keiner Fassung
   an.** Jede Zahl dazu wäre erfunden. Muss gemessen werden.
5. Ob die Analogmessung des Akkus einen Spannungsteiler braucht, hängt von der
   eingestellten Referenz ab — noch nicht gerechnet.
