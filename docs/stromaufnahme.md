# Stromaufnahme

Stand 25.10.2026. **Gerechnet, nicht gemessen.** Nichts davon lief auf
Hardware. Jede Zahl hat ihre Quelle dabei, jede Annahme ist als **Annahme**
gekennzeichnet, und wo das Datenblatt schweigt, steht `ungeprueft`.

## Was sich geändert hat

Thomas hat am 25.10.2026 entschieden: **Der Akku entfällt vollständig.** Keine
Zelle, kein Laderegler, keine Schutzschaltung. Das Gerät läuft ausschließlich aus
USB. Diese Datei hieß bis dahin „Energiebudget: Laufzeit je Zellenkapazität" und
rechnete, wie lange 40, 100 und 150 mAh halten. **Die Frage gibt es nicht mehr.**

Die **Stromaufnahmen** gelten weiter: Sie stammen aus Datenblättern und hängen
nicht daran, woher der Strom kommt. Sie beantworten jetzt andere Fragen:

- Was zieht das Gerät aus dem USB-Anschluss — im Ruhezustand, im Betrieb, in der
  Spitze?
- Welche Vorgaben für Firmware und Platine folgen daraus, auch wenn die
  Laufzeit keine mehr ist?

Die Versorgung selbst (Eingang `VDDH`, die 25-mA-Grenze am internen `VDD`) steht
in [`power-budget.md`](power-budget.md). Die Stromzahlen stehen **nur hier**;
andere Dokumente verweisen auf diese Datei, statt sie zu kopieren.

**Was mit dem Akku wegfällt**, damit es keiner neu rechnet: die Laufzeit in drei
Betriebsfällen, die Selbstentladung, die Alterung, der Eigenverbrauch der
Schutzplatine, das Laden. Die Entscheidung und ihre Gründe stehen in
[`zellen.md`](zellen.md). Der volle Wortlaut der alten Rechnung liegt im
Git-Verlauf dieser Datei (Commit `87a19c1`).

**Offen:** Ob NFC und Bluetooth ganz entfallen, ist nicht entschieden (ein Strang prüft es). Diese Datei behandelt beide weder als gesetzt noch als gestrichen.

**Folge für die Funkwege:** Der nRF52840 kann sich **nicht** aus dem NFC-Feld
versorgen. NFC und Bluetooth gibt es damit nur, solange USB den Stick versorgt —
am Rechner. Das ist bewusst in Kauf genommen. Was Nutzer dadurch verlieren,
steht im Handbuch ([`de/usage.md`](de/usage.md), Abschnitt NFC).

## Warum die Uhr keine Dauerversorgung braucht

Das ist die Begründung der Entscheidung, und sie gehört hierher, weil sie die
Ruhestromzahlen unten entwertet:

- Die Uhr für zeitbasierte Einmalcodes (TOTP) wird **beim Einstecken vom Rechner
  nachgestellt**, und der Code entsteht im Moment des Benutzens. Ein Gerät, das
  zwischen zwei Einsteckvorgängen stillsteht, verliert dabei nichts.
- Zeitstempel bei Signaturen holen die Zeit von einem **Zeitstempel-Server**,
  nicht von der Uhr im Gerät.
- Der Akku kaufte damit nur noch NFC/BLE ohne Rechner. Dagegen standen 4 bis
  5 mm Länge und ein Verbrauchsteil mit wenigen Jahren Lebensdauer in einem
  Gerät, das Jahrzehnte halten soll.

**Offen:** Wer dem Gerät die Zeit beim Einstecken übergibt, ist nicht
entschieden (siehe [`dongle-firmware.md`](dongle-firmware.md), zweiter offener
Punkt). Und: Eine falsch gehende Rechneruhr stellt dann auch die Geräteuhr
falsch. Beides ist Entwurf, nichts davon lief.

## 1. Die Stromaufnahmen, mit Quelle

Alle nRF52840-Werte sind **Typ.-Werte bei 3 V, 25 °C**, mit DC-DC-Wandler; das
Datenblatt gibt für die Stromszenarien weder Min. noch Max. an (PS v1.1,
Abschn. 5.2).

### Ruhe

| Posten | Strom | Quelle |
| --- | --- | --- |
| nRF52840, System ON, 256 kB RAM-Erhaltung, **Wecken per RTC** | **3,16 µA** | nRF52840 PS v1.1, Abschn. 5.2.1.1 |
| nRF52840, System OFF, kein RAM, Wecken per Reset | 0,40 µA | ebd. |
| nRF52840, System OFF, volle 256 kB Erhaltung | 1,86 µA | ebd. |
| nRF52840, System ON, volle Erhaltung, Wecken per **GPIOTE-PORT** | 2,36 µA | ebd. |
| nRF52840, System ON, volle Erhaltung, Wecken per **GPIOTE-Event** | 17,37 µA | ebd. |
| 32,768-kHz-Quarz (`ILFXO`) | **0,23 µA** | ebd., Abschn. 5.4.4 |
| interner Oszillator (LFRC), zum Vergleich | 0,7 µA | ebd. |
| NFC im SENSE STATE (`Isense`) | **0,10 µA** | ebd., Abschn. 6.14.14.1 |
| SE051C2, **Deep Power-down** (ENA = 0) | **3,00 µA** typ., 5 µA max. | SE051 DS Rev. 2.0, Tab. 11 |
| SE051C2, Power-down (zum Vergleich) | 430 µA typ. | ebd. |
| W25Q512JV, **Deep Power Down** (Befehl `B9h`) | **1,00 µA** typ., 20 µA max. | W25Q512JV Rev. B, Kap. 9.4 |
| W25Q512JV, Standby (zum Vergleich) | 10 µA typ., 60 µA max. | ebd. |
| OLED über Lastschalter abgeschaltet | ~0 | — |
| OLED-Controller im Sleep (zum Vergleich) | max. 10 µA IDD + max. 10 µA ICC | SSD1306 Rev 1.1, Tab. 12-1 |
| FPC BM-Lite im Schlaf (Stufe 2, nicht bestückt) | 13 µA | FPC Produktblatt 2020 |
| RGB-LED aus | 0 | — |
| **Summe Ruhe** (RTC-Wecken, Quarz, NFC, SE051C2 tief, Flash tief) | **≈ 7,5 µA** | Summe der fett gesetzten Zeilen |

Der Quarz bleibt Pflicht: Bluetooth verlangt ±500 ppm, was der interne
Oszillator nur kalibriert schafft, und die TOTP-Uhr braucht ihn ohnehin.

**Die Summe hat sich geändert:** Die frühere Fassung rechnete mit 9,5 µA. In den
9,5 µA steckten **2 µA Rückentladung des Ladereglers**, und den gibt es nicht
mehr. Die Zahl 9,5 µA steht noch in `../hardware/`; wer sie dort findet, findet
einen Stand vor dem Entfall des Akkus.

**Was an dieser Summe offen bleibt:** Das Datenblatt gibt die Weck-Szenarien als
**Entweder-oder** an. Ein Gerät, das per RTC **und** Taster **und** NFC-Feld
geweckt werden kann, hat keine Datenblattzahl; die 3,16 µA sind der größte
Einzelwert, realistisch 3–4 µA. Das ist eine Schätzung — **am Gerät messen**.
Und: Was der nRF52840 im **USB-Suspend** tatsächlich zieht, ist nirgends
gerechnet.

### Befunde, deren Begründung gefallen ist

Die vier Beschaltungen und Einstellungen unten waren als größte Gewinne des
Entwurfs begründet, **weil sie die Akkulaufzeit retteten**. Die Begründung gilt
nicht mehr. Der Befund — die Zahl aus dem Datenblatt — gilt weiter, und was aus
ihm folgt, steht jeweils dabei.

**1. SE051C2 in Deep Power-down (3 µA statt 430 µA, Faktor 143).**
`ENA` liegt auf `P1.10` (`../hardware/schematic.md`), der Chip wird über `Vin`
versorgt und `VCC` aus `Vout` gespeist — genau die Beschaltung, die das
Datenblatt für Deep Power-down verlangt (SE051 DS Rev. 2.0, Abschn. 5.2). Sie
ist auf der Platine gezeichnet und **bleibt richtig**.

- *Alte Begründung (gefallen):* Ohne sie hielte das Gerät mit 150 mAh elf Tage
  statt Monate.
- *Neue Begründung:* Zwei Gründe, beide **kleiner** als der alte.
  (a) Der Chip hat ab Werk **nur** diesen Modus; Power-down mit Zustandserhalt
  (430 µA) ist ab Werk abgeschaltet und nur über das Perso-Applet einschaltbar
  (`hardware.md`, AN12973 Rev. 2.0 Tab. 3). Die Beschaltung kostet also nichts
  und ist der Normalfall, kein Sondergewinn.
  (b) Ein USB-Gerät im Suspend darf nur wenig Strom ziehen; das USB-2.0-Limit für
  ein ausgesetztes Gerät liegt nach meiner Kenntnis bei **2,5 mA**. Diese Zahl
  stammt **nicht** aus einem hier gelesenen Dokument: `ungeprueft`. Gilt sie,
  wären 430 µA ein Sechstel davon, 3 µA ein Tausendstel.
- *Preis des Modus:* Der Zustand geht verloren, der Chip muss nach dem Wecken
  neu aufgesetzt werden. Wie lange das dauert, ist nicht gemessen.

**2. Taster über GPIOTE-PORT wecken (2,36 µA statt 17,37 µA, Faktor 7).**
Der Befund steht im Datenblatt (PS v1.1, Abschn. 5.2.1.1). Die Begründung war
die Laufzeit. Sie fällt. Der PORT-Modus kostet nichts und bleibt die Vorgabe;
eine **Pflicht** ist er nicht mehr, solange der Suspend-Strom unter dem
USB-Limit liegt (`ungeprueft`, siehe oben).

**3. Bluetooth nur auf Tastendruck, nicht in Dauerbereitschaft.**
Dauerwerbung kostete bei 1 s Abstand etwa 12 µA (Rechenweg unten, Abschnitt
„Bluetooth-Werbung"). Die Laufzeitbegründung fällt. **Die Empfehlung bleibt
aus einem anderen Grund bestehen:** Ein Gerät, das nicht wirbt, ist nicht
anpeilbar ([`threat-model.md`](threat-model.md)). Das war schon damals der
zweite Grund und trägt jetzt allein.

**4. Lastschalter für die Anzeige (Faktor 3 im Ruhestrom).**
Der Controller im Sleep zieht bis zu 20 µA gegen etwa 0 abgeschaltet. Das war
als Faktor 3 der Laufzeit begründet; die Begründung fällt. Ob der Lastschalter
`Q5` auf der Platine bleibt, entscheidet der Hardware-Strang, nicht diese Datei.
Die Lastschalter haben noch einen zweiten Zweck, der bleibt: Sie sorgen dafür,
dass nie zwei große Verbraucher gleichzeitig laufen (siehe „Spitzenstrom").

**Zwei Fallen aus dem Datenblatt, die weiter gelten:**

- Wecken per **GPIOTE-PORT** kostet 2,36 µA, per **GPIOTE-Event** 17,37 µA
  (PS v1.1, Abschn. 5.2.1.1).
- „Aufwachen, wenn ein NFC-Feld kommt" kostet **100 nA**. Das Datenblatt sagt
  zum ACTIVATED-Strom von 400 µA ausdrücklich: *„This current does not apply
  when in NFC field."* NFC-Bereitschaft ist praktisch gratis.

### Betrieb

| Posten | Strom | Quelle |
| --- | --- | --- |
| CPU 64 MHz aus Flash, mit DC-DC | **3,3 mA** | PS v1.1, Abschn. 5.2.1.3 |
| CPU 64 MHz aus Flash, mit LDO | 6,3 mA | ebd. |
| Senden 0 dBm, nur Funk, mit DC-DC | **4,8 mA** | ebd., Abschn. 6.20.15.2 |
| Senden +8 dBm, nur Funk, mit DC-DC | 14,8 mA | ebd. |
| Empfangen 1 Mbit BLE, nur Funk, mit DC-DC | **4,6 mA** | ebd. |
| CPU + Senden 0 dBm zusammen, mit DC-DC | **8,1 mA** | ebd., Abschn. 5.2.1.11 |
| CPU + Empfangen zusammen, mit DC-DC | 8,6 mA | ebd. |
| NFC im ACTIVATED STATE | **400 µA** | ebd., Abschn. 6.14.14.1 |
| NFC: Feld erkannt → `FIELDDETECTED` | max. 20 µs (`tsense`) | ebd. |
| NFC: `TASKS_ACTIVATE` → bereit | max. 500 µs (`tactivate`) | ebd. |
| NFC: Trägerfrequenz / Datenrate | 13,56 MHz / 106 kbit/s | ebd. |
| NFC: Strom in die NFC-Pins | max. 80 mA (absolute Grenze) | ebd. |
| NFC: nötige Feldstärke | 1,5 … 7,5 A/m (Klasse-1-Antenne) | SE051 DS Tab. 9 |
| SE051C2 bei asymmetrischer Krypto | **14,4 mA** | SE051 DS Rev. 2.0, Tab. 11 |
| W25Q512JV beim Lesen (50-MHz-Spalte) | **20 mA** | W25Q512JV Rev. B, Kap. 9.4 |
| OLED 0,71", `IBAT` bei `VCC` = 7,5 V | typ. **12,0 mA**, max. **25,0 mA** | Crystalfontz CFAL4864A-071BW, 2017-07-05, „Electrical Characteristics" |
| FPC BM-Lite Aufnahme + Vergleich (Stufe 2) | 12 mA, 0,4 s | FPC Produktblatt 2020 |
| RGB-LED, eine Farbe | bis 20 mA | Hersteller, übernommen aus der früheren Fassung von `power-budget.md` |
| USB-Regler des nRF52840 aus `VBUS` bei aktivem USBD | **170 µA** | PS v1.1, Abschn. 5.3.8.5 |

**Der DC-DC-Wandler muss an**, er halbiert fast alles (Zeile „mit LDO" oben).
Dafür braucht das Raytac-Modul extern eine Spule — Angaben in
[`power-budget.md`](power-budget.md).

Gerechnet wird unten mit den **25 mA** der Anzeige, also mit der Obergrenze.
Welcher der beiden Werte gilt, hängt davon ab, wie viele Pixel leuchten, und
ist **zu messen**.

## 2. Ladung je Vorgang

Das war die Grundlage der Laufzeitrechnung. Als **Ladung** ist sie weiter richtig
und zeigt, was einen Vorgang teuer macht — nur ist das jetzt keine Laufzeitfrage
mehr, sondern eine der Stromspitzen und der Wärme im Gehäuse.

**Ein NFC-Vorgang mit Bestätigung.** Annahmen: 1,5 s Funkbetrieb, 2 s Anzeige,
0,2 s Signatur, 0,1 s Flash lesen — alle frei gewählt und zu prüfen. Das
Datenblatt belegt nur die 20 µs und die 500 µs oben, nicht, wie lange ein ganzer
Vorgang dauert.

| Posten | Strom | Dauer | Ladung |
| --- | --- | --- | --- |
| NFC ACTIVATED | 0,4 mA | 1,5 s | 0,6 mA·s |
| CPU 64 MHz | 3,3 mA | 1,5 s | 4,95 mA·s |
| SE051C2 Signatur | 14,4 mA | 0,2 s (**Annahme**) | 2,88 mA·s |
| Flash lesen | 20 mA | 0,1 s (**Annahme**) | 2,0 mA·s |
| Anzeige | 25 mA | 2,0 s (**Annahme**) | 50,0 mA·s |
| **Summe mit Anzeige** | | | **60,4 mA·s** |
| Summe **ohne** Anzeige | | | 10,4 mA·s |

**Die Anzeige ist 50 von 60 mA·s** — mehr als Signatur und Flash zusammen. Das
ist der Befund. Die frühere Folgerung „Anzeige so kurz wie möglich, weil jede
Sekunde 45 Minuten Ruhe kostet" **fällt mit dem Akku**: Aus USB kostet die
Anzeigedauer keine Laufzeit mehr. Wie lange die Anzeige steht, bestimmt allein
die Bedienung (lesbar, nicht hektisch) — siehe [`display.md`](display.md).

### Bluetooth-Werbung

Annahmen: 3 Kanäle, zusammen 1,5 ms Funkzeit je Ereignis, mittlerer Strom 8,1 mA
(CPU + Senden 0 dBm mit DC-DC). Die Paketlänge hängt vom Stack ab und ist
**nicht gemessen**.

| Abstand der Werbepakete | Mittlerer Strom |
| --- | --- |
| 1 s | ≈ 12 µA |
| 2 s | ≈ 6 µA |
| 5 s | ≈ 2,4 µA |

Rechenweg für 1 s: 8,1 mA × 1,5 ms = 0,01215 mA·s je Ereignis; 3600 Ereignisse je
Stunde ergeben 0,0121 mAh/h, also 12,1 µA im Mittel. Das ist mehr als der ganze
übrige Ruhestrom. Warum Bluetooth trotzdem nicht dauernd werben soll, steht
oben unter „Befunde", Punkt 3.

## 3. Spitzenstrom

Der ungünstigste gleichzeitige Fall, wenn die Firmware nicht aufpasst:

| gleichzeitig | Strom |
| --- | --- |
| CPU + Senden 0 dBm | 8,1 mA |
| OLED an | 25 mA |
| SE051C2 Signatur | 14,4 mA |
| Flash lesen | 20 mA |
| **Summe** | **67,5 mA** |

Das überschreitet die 25 mA, die der interne `VDD` nach außen abgeben darf
([`power-budget.md`](power-budget.md)). Darum gilt unverändert: Anzeige **oder**
Flash **oder** Signatur, nie gleichzeitig.

**Neu durch USB als einzige Quelle:** Ein USB-Anschluss ohne Aushandlung darf
**100 mA** immer liefern (so stand es in der früheren Fassung von
`power-budget.md`). 67,5 mA liegen darunter. Die Frage, die der Akku früher
stellte — kein Datenblatt nennt einen Pulsstrom für die Zelle — entfällt. Dafür
kommt eine andere: Wie stark bricht `VBUS` ein, wenn der Stick ansteckt und die
Kondensatoren laden? **Nicht gerechnet, nicht gemessen.** Gehört zum
Platinen-Strang.

## 4. Was nicht belegt ist

Nach den Vier Stufen von „geprüft": alles hier ist **gerechnet**, nichts ist
**gemessen**.

1. **Der tatsächliche Gesamt-Ruhestrom**, einschließlich USB-Suspend (siehe
   oben).
2. **Das USB-2.0-Limit von 2,5 mA im Suspend.** Aus der Kenntnis der
   Spezifikation, hier nicht nachgelesen: `ungeprueft`.
3. **Welcher Anzeigestrom gilt**, typ. 12 oder max. 25 mA. Größter Einzelposten
   je Benutzung.
4. **Die Betriebsströme von QSPI, I2C und SPI.** Nordic spezifiziert sie in
   **keiner** Fassung des Datenblatts. Ein QSPI-Zugriff liegt mit Sicherheit über
   den 418 µA eines TIMER, lässt sich aber nicht belegen.
5. **Die Dauer einer SE051C2-Signatur** (0,2 s angenommen) und **die Dauer eines
   NFC-Vorgangs** (1,5 s angenommen).
6. **Der Spannungseinbruch auf `VBUS`** beim Einstecken und bei Lastsprüngen.
7. **Wie lange der SE051C2 nach Deep Power-down zum Wiederaufsetzen braucht.**

## Quellen

- nRF52840 Product Specification v1.1, Doc-ID 4413_417, 28.02.2019 —
  Abschn. 5.2.1.1 (Schlaf), 5.2.1.3 (CPU), 5.2.1.11 (zusammengesetzt),
  5.3.8.5 (USB-Regler), 5.4.4 (LFCLK), 6.14.14.1 (NFCT), 6.20.15.2 (Funk)
- SE051 Datenblatt Rev. 2.0, 08.07.2024, Tab. 11, Abschn. 5.2
- NXP AN12973 Rev. 2.0, Tab. 3 (Werkseinstellung Power-down)
- Winbond W25Q512JV, Revision B, 25.06.2019, Kap. 9.4
- Solomon Systech SSD1306, Rev 1.1, April 2008, Tab. 12-1
- Crystalfontz CFAL4864A-071BW, Ausgabe 2017-07-05, „Electrical Characteristics"
- FPC BM-Lite Produktblatt 2020
- Eigenes Repo: [`power-budget.md`](power-budget.md) (Versorgung, REG0-Grenzen),
  [`zellen.md`](zellen.md) (erledigte Zellenwahl),
  [`../hardware/schematic.md`](../hardware/schematic.md) (ENA auf `P1.10`)
