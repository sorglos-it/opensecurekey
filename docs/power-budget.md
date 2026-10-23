# Energiebudget

Stand 23.10.2026. **Gerechnet, nicht gemessen** — nichts davon lief auf Hardware.
Jede Datenblattzahl hat ihre Quelle dabei; jede Annahme ist als **Annahme**
gekennzeichnet. Wo das Datenblatt schweigt, steht `ungeprueft`.

Quellen der Chipzahlen:

- **nRF52840 Product Specification v1.1, Doc-ID 4413_417, 28.02.2019.**
  Die gewünschte Fassung v1.11 war nicht beschaffbar (Cloudflare 403, kein
  Spiegel gefunden). Die beiden wichtigsten Tabellen (Schlaf- und CPU-Ströme)
  wurden gegen die heutige Online-Fassung auf docs.nordicsemi.com geprüft und
  sind **Zahl für Zahl gleich**. Dass auch die übrigen Werte in v1.11
  unverändert sind, ist `ungeprueft`. Raytac beruft sich seinerseits auf v1.7.
- **SE051 Datenblatt Rev. 2.0, 8.7.2024, Tabelle 11.**
- **Winbond W25Q512JV, Revision B, 25.6.2019, Kap. 9.4.**
- **Solomon Systech SSD1306, Rev 1.1, April 2008, Tab. 12-1** und die
  Herstellerseite des gewählten Glases.
- **Microchip DS20001984H** (Laderegler), **FPC BM-Lite Produktblatt 2020**.

Alle nRF52840-Werte sind **Typ.-Werte bei 3 V, 25 °C**; das Datenblatt gibt für
die Stromszenarien weder Min. noch Max. an (PS v1.1, Abschn. 5.2).

## Die wichtigste Zahl zuerst: wie das Gerät versorgt wird

Das entscheidet alles andere, und hier steckt eine Falle, die man leicht
übersieht.

**Der nRF52840 darf nicht direkt an den Akku an `VDD`.** `VDD` endet bei
**3,6 V**, die absolute Grenze liegt bei **3,9 V** (PS v1.1, Tab. 172 und
Tab. 173). Ein voller LiPo hat 4,2 V — das zerstört den Chip.

**Richtig ist `VDDH` im High-voltage-Betrieb:** 2,5 V bis **5,5 V** (Tab. 172),
absolute Grenze 5,8 V. Der Chip erzeugt sein `VDD` dann selbst in der
REG0-Stufe. **Ein externer Spannungsregler ist nicht nötig.** Der Akku
(3,0–4,2 V) und die USB-5 V passen beide an `VDDH`.

Welche Betriebsart gilt, liest die Firmware im Register `MAINREGSTATUS`
(POWER, Offset 0x640): `Normal` = aus `VDD` gespeist, `High` = aus `VDDH`
(PS v1.1, Abschn. 5.3.7.20).

### Und hier die Falle: wie viel darf extern aus dem internen VDD fließen

PS v1.1, Abschn. 5.3.8.2, REG0-Stufe:

| Größe | Bedeutung | Grenze |
| --- | --- | --- |
| `IEXT,HIGH` | externer Stromzug, High-voltage-Betrieb, **Funk ≤ 4 dBm** | **max. 25 mA** |
| `IEXT,LOW` | externer Stromzug, High-voltage-Betrieb, **Funk > 4 dBm** | **max. 5 mA** |
| `IEXT,OFF` | externer Stromzug, System OFF | max. 1 mA |
| `VREG0,DROP` | Mindestabfall `VDDH` − `VDD` | **0,3 V** |
| `VDDOUT` | einstellbare Ausgangsspannung von REG0 | 1,8 … 3,3 V |

„Externer Stromzug" ist alles, was aus dem intern erzeugten `VDD` herausfließt —
die GPIO-Ströme **und** alles, was man als Nebenverbraucher daran hängt.

**Was das für uns heißt, mit unseren Teilen gerechnet:**

| Nebenverbraucher | Strom im Betrieb |
| --- | --- |
| OLED 0,71" (Herstellerangabe max.) | **25 mA** |
| Flash W25Q512JV beim Lesen (50-MHz-Spalte) | **20 mA** |
| SE051C2 bei asymmetrischer Krypto | **14,4 mA** |
| Fingerabdrucksensor bei der Aufnahme | 12 mA |
| RGB-LED, eine Farbe | bis 20 mA |

**Jeder einzelne dieser Verbraucher sprengt allein schon die 5 mA, die bei
Sendeleistung über 4 dBm erlaubt sind.** Und OLED plus Flash zusammen
überschreiten auch die 25 mA.

Drei Wege, drei Preise:

| Weg | Was man tut | Kosten |
| --- | --- | --- |
| **A (empfohlen)** | Alles aus dem internen `VDD`, aber **Sendeleistung auf ≤ 4 dBm begrenzen** und per Lastschalter dafür sorgen, dass nie zwei große Verbraucher gleichzeitig laufen | Reichweite sinkt; Firmware muss die Reihenfolge einhalten |
| B | Eigene 3,0-V-Schiene aus dem Akku für OLED, Flash und Sicherheitschip | ein Abwärts-Aufwärts-Wandler mehr (LiPo 3,0–4,2 V über **und** unter 3,0 V); mehr Platz, mehr Ruhestrom |
| C | Sendeleistung frei, aber alle Nebenverbraucher an eine eigene Schiene | wie B |

**Empfehlung: Weg A.** Für einen Schlüssel, der am Körper getragen und ans
Handy gehalten wird, reichen 0 dBm. Der Gewinn ist groß: bei 0 dBm mit
DC-DC-Wandler sind es **4,8 mA** Sendestrom, bei +8 dBm ohne Wandler
**32,7 mA** — fast das Siebenfache (PS v1.1, Abschn. 6.20.15.2).

**Pflicht in jedem Fall: der DC-DC-Wandler muss an.** Er halbiert fast alles:

| | mit DC-DC | mit LDO |
| --- | --- | --- |
| CPU 64 MHz aus Flash | **3,3 mA** | 6,3 mA |
| Senden 0 dBm (nur Funk) | **4,8 mA** | 10,6 mA |
| Senden +8 dBm (nur Funk) | **14,8 mA** | 32,7 mA |
| Empfangen 1 Mbit BLE (nur Funk) | **4,6 mA** | 9,9 mA |
| CPU + Senden 0 dBm zusammen | **8,1 mA** | 15,4 mA |
| CPU + Empfangen zusammen | **8,6 mA** | 16,2 mA |

Dafür braucht das Raytac-Modul **extern eine Spule L4: 10 µH, 0603, mindestens
80 mA** (Raytac Approval Sheet Ver. L, Kap. 8). Die Spule für REG1 und der
32-MHz-Quarz sind im Modul drin.

## Ruhestrom — wo das Jahr verloren geht

### Der nRF52840 im Schlaf (PS v1.1, Abschn. 5.2.1.1)

| Betriebsart | Typ. |
| --- | --- |
| System OFF, kein RAM, Wecken per Reset | **0,40 µA** |
| System OFF, volle 256 kB RAM-Erhaltung | 1,86 µA |
| System ON, kein RAM, Wecken per Event | 0,97 µA |
| System ON, volle Erhaltung, Wecken per Event | 2,35 µA |
| System ON, kein RAM, **Wecken per RTC** | 1,50 µA |
| **System ON, volle Erhaltung, Wecken per RTC** | **3,16 µA** |
| System ON, volle Erhaltung, Wecken per **GPIOTE-PORT** | **2,36 µA** |
| System ON, volle Erhaltung, Wecken per **GPIOTE-Event** | **17,37 µA** |

**Der teuerste Fehler, den man hier machen kann, kostet Faktor 7:** Wecken per
GPIOTE im **Event**-Modus zieht 17,37 µA, im **PORT**-Modus nur 2,36 µA. Bei
einem Gerät, dessen gesamtes Ruhebudget unter 10 µA liegen soll, ist das der
Unterschied zwischen Monaten und über einem Jahr. **Der Taster muss über
GPIOTE-PORT wecken.**

Dazu der 32,768-kHz-Quarz: **0,23 µA** (`ILFXO`, Abschn. 5.4.4). Er ist
sparsamer als der interne Oszillator (LFRC 0,7 µA) — und BLE verlangt
±500 ppm, was der interne nur kalibriert schafft. Für TOTP, das eine laufende
Uhr braucht, ist der Quarz ohnehin Pflicht.

**NFC kostet im Wartezustand praktisch nichts: 100 nA** im SENSE STATE
(`Isense`, Abschn. 6.14.14.1). Im ACTIVATED STATE 400 µA, aber das gilt nur,
während ein Feld anliegt. Das Datenblatt sagt dazu ausdrücklich: *„This current
does not apply when in NFC field."* **„Aufwachen, wenn ein NFC-Feld kommt" ist
also so gut wie gratis** — der attraktivste Posten des ganzen Entwurfs.

### Die anderen Chips im Schlaf

| Teil | Zustand | Strom | Quelle |
| --- | --- | --- | --- |
| SE051C2 | **Deep Power-down** (ENA = 0) | **3 µA** typ., 5 µA max. | SE051 DS Rev. 2.0, Tab. 11 |
| SE051C2 | Power-down (Zustand bleibt) | **430 µA** typ. | ebd. |
| W25Q512JV | **Deep Power Down** (Befehl B9h) | **1 µA** typ., 20 µA max. | W25Q512JV Rev. B, Kap. 9.4 |
| W25Q512JV | Standby (nur /CS hoch) | 10 µA typ., 60 µA max. | ebd. |
| SSD1306 | Sleep | max. **10 µA** IDD + max. 10 µA ICC | SSD1306 Rev 1.1, Tab. 12-1 |
| FPC BM-Lite | Sleep | **13 µA** | FPC Produktblatt 2020 |
| MCP73832 | Rückentladung aus dem Akku ohne USB | **< 2 µA** | DS20001984H, Abschn. 4.1/4.3 |
| Everlight RGB-LED | aus | **0** | — |

**Zwei Pflichten für die Firmware, sonst zahlt man das Zehn- bis
Hundertfache:**

1. **SE051C2 in Deep Power-down legen, nicht in Power-down.** 3 µA gegen
   **430 µA** — Faktor 143. Dafür muss die Platine aber richtig beschaltet
   sein: `Vin` versorgen, `VCC` aus `Vout` speisen, `ENA` schalten
   (SE051 DS Rev. 2.0, Abschn. 5.2). Das ist eine **Hardwarevorgabe**, keine
   Softwaresache — siehe `../hardware/schematic.md`.
2. **Flash per Befehl `B9h` schlafen legen**, nicht nur /CS hochziehen.
   1 µA gegen 10 µA.

### Ruhestrom, zusammengerechnet

**Variante A — OLED und Sensor über Lastschalter ganz abgeschaltet
(empfohlen):**

| Posten | Strom |
| --- | --- |
| nRF52840, System ON, 256 kB Erhaltung, Wecken per RTC | 3,16 µA |
| 32,768-kHz-Quarz | 0,23 µA |
| NFC im SENSE STATE | 0,10 µA |
| SE051C2 Deep Power-down | 3,00 µA |
| Flash Deep Power Down | 1,00 µA |
| Laderegler Rückentladung | < 2,00 µA |
| OLED abgeschaltet | ~0 |
| Fingerabdrucksensor abgeschaltet | ~0 |
| **Summe** | **≈ 9,5 µA** |

**Variante B — OLED und Sensor dauernd versorgt:**

| zusätzlich | Strom |
| --- | --- |
| OLED-Controller im Sleep (ungünstigster Fall) | 20 µA |
| BM-Lite im Sleep | 13 µA |
| **Summe** | **≈ 42,5 µA** |

**Das ist Faktor 4,5 — allein durch zwei Lastschalter.** Zwei P-Kanal-MOSFETs
sind billiger als die vierfache Akkukapazität.

**`ungeprueft` an dieser Summe:** Das Datenblatt gibt die Weck-Szenarien als
**Entweder-oder** an. Ein Gerät, das gleichzeitig per RTC **und** per Taster
(GPIOTE-PORT) **und** per NFC-Feld geweckt werden kann, hat keine
Datenblattzahl. Die 3,16 µA sind der größte der Einzelwerte; realistisch sind
**3–4 µA**. Das ist eine Schätzung, keine Datenblattangabe — **am Gerät
messen**.

## Aktivphasen

### Bluetooth-Bereitschaft ist der teuerste Posten

Dauerhaftes Aussenden von Werbepaketen (Advertising), gerechnet:

**Annahmen:** 3 Kanäle, zusammen 1,5 ms Funkzeit je Ereignis, mittlerer Strom
8,1 mA (CPU + Senden 0 dBm mit DC-DC, PS v1.1 Abschn. 5.2.1.11). Beides
**Annahme** — die Paketlänge hängt vom Stack ab und ist nicht gemessen.

| Abstand der Werbepakete | Mittlerer Strom |
| --- | --- |
| 1 s | **≈ 12 µA** |
| 2 s | ≈ 6 µA |
| 5 s | ≈ 2,4 µA |

Rechenweg für 1 s: 8,1 mA × 1,5 ms = 0,01215 mA·s je Ereignis; 3600 Ereignisse
je Stunde ergeben 43,7 mA·s = 0,0121 mAh/h, also 12,1 µA im Mittel.

**Befund: dauerhafte Bluetooth-Bereitschaft kostet mehr als das ganze übrige
Ruhebudget.** 12 µA gegen 9,5 µA.

**Empfehlung: Bluetooth nicht dauernd anbieten, sondern erst auf Tastendruck.**
Das Gerät ist ein Schlüssel, kein Sensor — es muss nicht gefunden werden
wollen, bis der Nutzer es will. Das ist zugleich sicherheitstechnisch besser:
ein Gerät, das nicht wirbt, ist nicht anpeilbar. Siehe `threat-model.md`.

### Ein NFC-Vorgang

| Posten | Wert |
| --- | --- |
| Feld erkannt → `FIELDDETECTED` | max. **20 µs** (`tsense`) |
| `TASKS_ACTIVATE` → bereit | max. **500 µs** (`tactivate`) |
| Strom im ACTIVATED STATE | **400 µA** |
| Trägerfrequenz / Datenrate | 13,56 MHz / 106 kbit/s |
| Max. Strom in die NFC-Pins | 80 mA (absolute Grenze) |
| Nötige Feldstärke | 1,5 … 7,5 A/m (SE051 DS Tab. 9; Klasse-1-Antenne) |

Dazu kommt der Rechenstrom: CPU 3,3 mA plus SE051 14,4 mA für die Signatur.
Ein NFC-Vorgang ist also stromstärker als die 400 µA des NFC-Blocks vermuten
lassen — aber er dauert nur Sekunden.

### Stromstoß beim Fingerabdruck-Scan und beim OLED

| Vorgang | Strom | Dauer | Ladung |
| --- | --- | --- | --- |
| BM-Lite Aufnahme + Vergleich | **12 mA** | **0,4 s** (Herstellerangabe) | 4,8 mA·s |
| OLED 0,71" an (ungünstigster Fall) | **25 mA** | 3 s (**Annahme**) | 75 mA·s |
| SE051C2 Signatur | **14,4 mA** | 0,2 s (**Annahme**, `ungeprueft`) | 2,9 mA·s |
| Flash lesen, stoßweise | 20 mA | 0,1 s (**Annahme**) | 2 mA·s |
| CPU wach | 3,3 mA | 3 s | 9,9 mA·s |
| **Eine vollständige Freigabe** | | **≈ 3 s** | **≈ 94,6 mA·s = 26,3 µAh** |

**Der OLED ist mit Abstand der größte Einzelposten einer Benutzung** — mehr als
der Fingerabdrucksensor, die Signatur und der Flash zusammen. Zwei Folgerungen:

- **Die Anzeige so kurz wie möglich einschalten.** Jede Sekunde Anzeige kostet
  etwa so viel wie 45 Minuten Ruhe.
- Die 25 mA sind die Herstellerobergrenze für das 0,71"-Glas; der tatsächliche
  Verbrauch bei 3,3 V mit laufender Ladungspumpe ist aus **keiner** gelesenen
  Quelle belegt (`ungeprueft`). Für das 0,96"-Glas nennt derselbe Hersteller
  16 mA typisch. **Messen, sobald das Glas da ist** — dieser eine Wert
  verschiebt die Laufzeit erheblich.

### Spitzenstrom — und die Lücke, die bleibt

Der ungünstigste gleichzeitige Fall, wenn die Firmware nicht aufpasst:

| gleichzeitig | Strom |
| --- | --- |
| CPU + Senden 0 dBm | 8,1 mA |
| OLED an | 25 mA |
| SE051C2 Signatur | 14,4 mA |
| Flash lesen | 20 mA |
| **Summe** | **67,5 mA** |

**Das ist die wichtigste offene Frage des Energiebudgets**, und sie hat zwei
Seiten:

1. **Es überschreitet die 25 mA `IEXT,HIGH`** der REG0-Stufe. Darum die
   Reihenfolge erzwingen: Anzeige **oder** Flash **oder** Signatur, nie
   gleichzeitig. Das muss in die Firmware, und es gehört in `architecture.md`.
2. **Kein einziges Akku-Datenblatt nennt einen Pulsstrom.** Belegt ist nur der
   Dauerstrom: Adafruit 1317 = 1 C = 150 mA, und die Schutzschaltung der
   EEMB-Zelle begrenzt auf **0,3 A dauernd, 0,8 A für 3 s**. 67,5 mA liegen
   darunter, aber mit dem Innenwiderstand (≤ 180 mΩ bei der SparkFun-Zelle,
   ≤ 190 mΩ bei EEMB) fällt bei 67,5 mA rund **12 mV** ab — unkritisch. Bei
   einem Einschaltstoß von 300 mA wären es 57 mV. **Nicht gemessen.**

## Laden am USB

| Größe | Wert | Quelle |
| --- | --- | --- |
| Ladestrom | **I = 1000 V / R_PROG** | DS20001984H, Abschn. 5.1.2 |
| R_PROG = 10 kΩ | **100 mA** | ebd. |
| R_PROG = 20 kΩ | **50 mA** | ebd. |
| Ladeschlussspannung (Typ `-2`) | **4,20 V** | ebd., Bestellschlüssel |
| Max. Ladestrom Adafruit 1317 | **150 mA**, empfohlen 100 mA | Adafruit |
| USB-Ruhestrom des nRF52840-USB-Reglers | **170 µA** aus VBUS bei aktivem USBD | PS v1.1, Abschn. 5.3.8.5 |
| VBUS erkannt (steigend) | 3,4 / 4,0 / 4,3 V | ebd. |
| VBUS weg (fallend) | 3,0 / 3,6 / 3,9 V | ebd. |

**Ladezeit, gerechnet** (150 mAh, R_PROG = 10 kΩ → 100 mA): die
Konstantstromphase bis etwa 80 % dauert 150 mAh × 0,8 / 100 mA ≈ **1,2 h**,
danach kommt die Konstantspannungsphase. Mit ihr insgesamt **etwa 2 bis
2,5 Stunden** (die Dauer der CV-Phase ist eine **Annahme**, kein
Datenblattwert). Mit 50 mA entsprechend etwa 3,5 bis 4,5 Stunden.

**Empfehlung: R_PROG = 10 kΩ (100 mA).** Das ist genau der von Adafruit
empfohlene Wert für die 1317 und bleibt unter ihren 150 mA Grenze. Ein
USB-Anschluss ohne PD-Verhandlung darf 100 mA ohnehin immer liefern.

**Nicht gerechnet (`ungeprueft`):** die Wärme im Laderegler. Bei 5 V Eingang,
3,2 V Zellspannung und 100 mA fallen etwa **180 mW** im SOT-23-5 an. Das ist
für dieses Gehäuse viel und muss noch nachgerechnet werden.

## Laufzeit

**Annahmen, alle frei gewählt und zu prüfen:**

- nutzbar 80 % der Nennkapazität (Abschaltung oberhalb der Zellschutzschwelle
  von 3,0 V)
- 10 Benutzungen am Tag, je 3 s, nach der Rechnung oben 26,3 µAh
- **Selbstentladung 2 % im Monat** — **in keinem Datenblatt belegt.** Beide
  gelesenen Zelldatenblätter nennen nur „Kapazitätserhalt nach 28 Tagen"
- Bluetooth nicht dauernd an

### Adafruit 1317, 150 mAh → 120 mAh nutzbar

| Posten | je Tag |
| --- | --- |
| Ruhe (9,5 µA × 24 h) | 0,228 mAh |
| 10 Benutzungen × 26,3 µAh | 0,263 mAh |
| Selbstentladung (2 %/Monat von 150 mAh) | 0,100 mAh |
| **Summe** | **0,591 mAh** |

**120 mAh / 0,591 mAh ≈ 203 Tage ≈ 6,7 Monate**

### Dieselbe Zelle, Bluetooth dauernd bereit (1 s)

Ruhe steigt um 12 µA → +0,288 mAh/Tag → 0,879 mAh/Tag
→ **136 Tage ≈ 4,5 Monate**

### BerryBase LP-401230, 100 mAh → 80 mAh nutzbar

**80 / 0,591 ≈ 135 Tage ≈ 4,5 Monate**

### Was passiert, wenn man die Lastschalter weglässt (Variante B)

Ruhe 42,5 µA → 1,02 mAh/Tag → mit 150 mAh: **0,891 mAh/Tag + ...**
→ 120 / 1,386 ≈ **87 Tage ≈ 2,9 Monate**

### Übersicht

| Fall | Zelle | Laufzeit |
| --- | --- | --- |
| **Lastschalter, BLE auf Abruf** | **150 mAh** | **≈ 6,7 Monate** |
| Lastschalter, BLE auf Abruf | 100 mAh | ≈ 4,5 Monate |
| Lastschalter, BLE dauernd bereit | 150 mAh | ≈ 4,5 Monate |
| ohne Lastschalter, BLE auf Abruf | 150 mAh | ≈ 2,9 Monate |
| 2 Benutzungen/Tag statt 10 | 150 mAh | ≈ 11 Monate |
| 30 Benutzungen/Tag | 150 mAh | ≈ 3,5 Monate |
| theoretische Obergrenze (nur Ruhe, keine Selbstentladung) | 150 mAh | 150 mAh / 9,5 µA ≈ **1,8 Jahre** |

## Die Antwort auf „wie groß muss der Akku sein"

**150 mAh (Adafruit 1317) sind richtig, und mehr bringt wenig.**

Begründung:

- Mit 150 mAh und sauberer Firmware hält das Gerät **gut ein halbes Jahr** ohne
  Rechner. Das ist für einen Schlüssel, der fast täglich am USB hängt und dabei
  nachlädt, mehr als genug — in der Praxis wird er nie leer.
- Die Zelle ist mit **3,8 mm** die dünnste belegte und bestimmt damit die
  Bauhöhe des Geräts. 250 mAh gäbe es nur mit 5,3 mm (+40 % Dicke) für +67 %
  Kapazität — bei einem Schlüsselanhänger ist Dicke teurer als Laufzeit.
- **Die Laufzeit hängt nicht an der Kapazität, sondern an drei
  Entscheidungen:** Lastschalter für OLED und Sensor (Faktor 4,5),
  GPIOTE-PORT statt Event (Faktor 7 im Weckpfad), Bluetooth auf Abruf statt
  dauernd (Faktor 1,5). Alle drei kosten nichts. Eine größere Zelle kostet
  Bauhöhe und löst keines der drei Probleme.

**Wenn es dünner sein muss:** BerryBase LP-401230, 100 mAh, nur 12 mm breit —
4,5 Monate. Für ein schmales Anhängergehäuse der bessere Kompromiss.

## Was gemessen werden muss, bevor diese Zahlen gelten

Nach den Vier Stufen von „geprüft": alles hier ist **gerechnet**, nichts ist
**ausgeführt**.

1. **Der tatsächliche Gesamt-Ruhestrom.** Die Entweder-oder-Natur der
   Weck-Szenarien im Datenblatt lässt keine belegte Summe zu.
2. **Der OLED-Modulstrom bei 3,3 V mit laufender Ladungspumpe.** Größter
   Einzelposten je Benutzung, und durch keine Quelle belegt. Spanne 16–25 mA.
3. **Die Betriebsströme von QSPI, I2C und SPI.** Nordic spezifiziert sie in
   **keiner** Fassung des Datenblatts — für diese drei Peripherien gibt es
   keinerlei Stromangabe. Zum Vergleich nennt Nordic für einen einzelnen
   TIMER 418 µA und für den SAADC 1,24 mA. **Ein QSPI-Zugriff liegt mit
   Sicherheit darüber, lässt sich aber nicht belegen.**
4. **Die Dauer einer SE051C2-Signatur.** Ohne sie ist die Ladung je Benutzung
   eine Annahme.
5. **Der Spitzenstrom im ungünstigsten Fall** und ob der Spannungseinbruch die
   Brownout-Erkennung auslöst (`VPOF,HV` ist im High-voltage-Betrieb zwischen
   2,7 und 4,2 V einstellbar, ±5 %).
6. **Die Selbstentladung der Zelle.** Durch kein Datenblatt belegt; 2 %/Monat
   sind geraten und machen ein Sechstel des Tagesverbrauchs aus.
7. **Die Wärme im Laderegler** bei 100 mA.

## Quellen

- nRF52840 Product Specification v1.1, Doc-ID 4413_417, 28.02.2019 —
  Abschn. 5.2.1.1 (Schlaf), 5.2.1.3 (CPU), 5.2.1.11 (zusammengesetzt),
  5.3.8.2 (REG0), 5.4.4 (LFCLK), 6.14.14.1 (NFCT), 6.20.15.2/3 (Funk),
  Tab. 172 (Betriebsbedingungen), Tab. 173 (absolute Grenzwerte)
- nRF52840 PS, Online-Fassung, abgerufen 23.10.2026 —
  https://docs.nordicsemi.com/bundle/ps_nrf52840
- SE051 Datenblatt Rev. 2.0, 8. Juli 2024, Tab. 11, Abschn. 5.2
- Winbond W25Q512JV, Revision B, 25. Juni 2019, Kap. 9.4
- Solomon Systech SSD1306, Rev 1.1, April 2008, Tab. 12-1
- Microchip DS20001984H, MCP73831/2, Rev. H, Abschn. 4.1, 4.3, 5.1.2
- Raytac MDBT50Q-1MV2 Approval Sheet Ver. L, 24.05.2023, Kap. 8
- FPC BM-Lite Produktblatt 2020
