# Layout-Vorgaben

Stand 23.10.2026. Vorgaben auf Papier, **nichts gezeichnet, nichts gefertigt**.

Die Teilemaße stammen aus `../docs/parts.md`, die Netze aus `schematic.md`.

## Die Maße, aus denen sich alles ergibt

| Teil | Maße | Besonderheit |
| --- | --- | --- |
| U1 Raytac MDBT50Q-1MV2 | **15,5 × 10,5 × 2,05 mm** | Chipantenne, braucht Freiraum |
| DS1 OLED 0,71" | **13,90 × 22,00 × 1,26 mm**, aktiv 10,54 × 14,70 mm | FPC-Fahne |
| Akku Adafruit 1317 | **3,8 × 19,75 × 26,02 mm** | dickstes Teil |
| Akku BerryBase LP-401230 | **4,05 × 12,0 × 30,5 mm** | schmale Alternative |
| J1 USB-C GCT USB4085 | Körper 8,94 × 9,17 mm, Öffnung 3,16 mm hoch | Durchsteck, 4 Haltestifte |
| U2 SE051C2 | 3 × 3 × 0,32 mm | — |
| U3 Flash WSON8 | 8 × 6 mm | — |
| SW1 Taster | 3,5 × 2,9 × 1,35 mm | **seitlich betätigt** |
| D1 RGB-LED | 1,6 × 1,6 × 0,5 mm | — |
| L1 Spule | 0603 | nahe an DCCH |

**Das dickste Teil ist der Akku mit 3,8 mm, das längste mit 26 mm.** Beides
bestimmt die Bauform.

## Bauform: 24 × 70 mm

**Platine 24 × 70 mm, 0,8 mm dick, vier Lagen, beidseitig bestückt.**

Die 70 mm statt 62 mm sind die räumliche Trennung von Akku und NFC-Antenne —
siehe den nächsten Abschnitt. Die 8 mm sind bewusst ausgegeben.

Zur Dicke: **0,8 mm statt 1,6 mm ist keine freie Wahl**, wenn der USB-C-Stecker
später direkt am Platinenrand sitzen soll — alle gefundenen
Platinenrand-Stecker passen nur auf 0,8 mm. Auch wenn in Stufe 1 die Buchse
verbaut wird, lohnt es, die Dicke gleich festzulegen. Dazu: **ENIG statt HASL**,
weil die dickere HASL-Schicht das erste Stecken erschwert.

Gesamthöhe grob: 0,8 mm Platine + 3,8 mm Akku + 2,05 mm Modul auf der anderen
Seite ≈ **6,7 mm plus Gehäuse**, also etwa 8–9 mm fertig. Das ist für einen
Schlüsselanhänger vertretbar. **Nicht gerechnet**, nur zusammengezählt.

## Akku gegen NFC-Antenne: Trennung UND Ferritfolie

**Der Akku darf nicht hinter der NFC-Antenne liegen** — er dämpft das Feld. Die
Begründung ist belegt: Das Taoglas-Datenblatt zur Flexantenne sagt, dass die
Reichweite **ohne Ferritlage auf leitfähigem Untergrund praktisch null** ist.
Eine LiPo-Zelle mit ihrer Aluminiumhülle ist genau so ein Untergrund.

**Entschieden: beides zusammen** — räumlich trennen, soweit die Bauform es
hergibt, **und** eine Ferritfolie dazwischen. Das ist der Weg, den jedes Handy
mit NFC und Akku geht; die Folie kostet wenige Cent und einen halben
Millimeter. Weil die Abstimmung ohnehin am Netzwerkanalysator gemessen wird und
nicht gerechnet werden kann, planen wir mit Reserve statt auf Kante.

**NFC in Stufe 1 weglassen kommt nicht in Frage:** Der Akku ist überhaupt nur
deshalb im Gerät, weil NFC und Bluetooth ohne Rechner laufen sollen. Ein Gerät
mit Akku und ohne NFC hätte den Akku umsonst.

### Die Aufteilung

Weil Stufe 1 **ohne Fingerabdrucksensor** gebaut wird, ist das Feld neben dem
Akku frei geworden, das vorher der Sensor belegt hätte (15 × 15 mm). **Dieser
Platz geht in den Abstand zwischen Akku und Antenne** — der Akku rückt zum
Modul hin, das Antennendrittel wird größer.

```
   ┌──────────────────────────────────────────────────────┐
   │ USB-C │  Modul U1  │  Akku unten   │   NFC-Antenne   │
   │  J1   │  (Antenne  │  + Ferritfolie│   als Schleife  │
   │       │   nach     │  OLED oben    │   + Taster      │
   │       │   außen)   │  SE051, Flash │                 │
   └──────────────────────────────────────────────────────┘
     0    9        25           44                 70 mm
            └─ Akku endet bei 44 mm ─┘
                                 └─ 26 mm Antennenfeld ─┘
```

- Antenne und Akku liegen in **verschiedenen Dritteln** der Platine, mit
  **etwa 26 mm** für das Antennenfeld statt der vorher geplanten 22 mm.
- **Ferritfolie auf der Akkuseite**, zwischen Zelle und Platine, überdeckt die
  Zellfläche und reicht in Richtung Antenne über. Genaue Fläche ergibt sich
  beim Zeichnen.
- Die Antennenschleife liegt im letzten Drittel, mit ausgenommener
  Massefläche darunter.

70 × 24 mm ist noch Schlüsselanhängerformat — etwa so groß wie ein
Autoschlüssel.

**Die Bauhöhe richtet sich jetzt nach dem Akku (3,8 mm), nicht mehr nach dem
Sensor (2,8 mm plus Fassung).** Damit ist die Höhe von der Sensorfrage
entkoppelt: Stufe 2 kann den Sensor nachrüsten, ohne dass das Gehäuse dicker
werden muss, solange er neben dem Akku und nicht über ihm sitzt.

### Wenn das Gehäuse dadurch zu groß wird

Dann **nicht** den Abstand opfern, sondern die **schmale Zelle** nehmen:
BerryBase LP-401230, 100 mAh, **4,05 × 12,0 × 30,5 mm**. Sie ist nur 12 mm
breit statt 19,75 mm und lässt neben sich Platz, sodass die Platine kürzer
werden kann. Preis: Laufzeit sinkt von etwa 6,8 auf etwa 4,6 Monate
(`../docs/power-budget.md`).

**Die Reihenfolge ist also: Abstand behalten, Kapazität opfern** — nicht
umgekehrt. Ein Schlüssel, dessen NFC nicht funktioniert, hilft niemandem;
einer, der statt sieben nur viereinhalb Monate hält, schon.

## Weitere Vorgaben

### Funkantenne des Moduls

- Die Chipantenne des U1 zeigt **zum Platinenrand**, nicht ins Innere.
- Unter und neben der Antenne **keine Kupferfläche, keine Leiterbahn, kein
  Bauteil** — die Freihaltezone nach Raytacs Vorgabe (Maße am Approval Sheet
  nachzutragen, `ungeprueft`).
- Der Akku darf auch hier nicht direkt dahinter liegen.
- Raytac warnt in Abschn. 2.6 ausdrücklich vor GPIOs nahe dem Funkteil: auf
  diesen Pins **kein SPI, I2C, UART oder PWM**. Das ist in `pinout.md`
  berücksichtigt und muss beim Verlegen eingehalten werden.

### NFC-Antenne, wenn sie als Leiterbahn kommt

- **Differentiell** an `P0.09`/`P0.10`, als Schleife mit mehreren Windungen.
- **C15 und C16 müssen gleich sein** und möglichst symmetrisch liegen — die
  Gesamtkapazität beider Zweige muss übereinstimmen (nRF52840 PS, Abschn.
  6.14.10). Also gleiche Leiterbahnlängen zu beiden Kondensatoren.
- Zielinduktivität etwa **2 µH**, dann liegen die Abstimmkondensatoren bei etwa
  130 pF. Die genaue Windungszahl ergibt sich aus der Fläche und muss
  **gemessen** werden.
- Innerhalb der Schleife **keine geschlossene Massefläche** — sie wirkt als
  Kurzschlusswindung.

### USB-C

- J1 an die **kurze Kante**, mittig, damit das Kabel gerade abgeht.
- Die vier Haltestifte sind mechanisch tragend und müssen durchkontaktiert
  sein — der Stecker nimmt die Zugkräfte auf.
- D+ und D− als **Paar mit gleicher Länge** führen, möglichst kurz zu U5 und
  von dort zum Modul.
- U5 (ESD) **so nah an die Buchse wie möglich** — nach dem Stecker, vor allem
  anderen. Das ist der Sinn des Bauteils.
- Die beiden 5,1-kΩ-Widerstände dicht an A5 und B5.

### Versorgung

- C5 (4,7 µF an `VDD`), C6 (1 µF an `DEC4`) und C7 (4,7 µF an `DECUSB`)
  **unmittelbar an die jeweiligen Modulpins** — das sind die vom Datenblatt
  vorgeschriebenen Entkopplungen, keine Zugabe.
- **L1 (10 µH) dicht an `DCCH`**, kurze dicke Leiterbahn. Sie führt den
  Schaltstrom des DC-DC-Wandlers; eine lange Zuleitung macht aus ihr eine
  Störquelle neben dem Funkteil.
- Der Ladepfad (U4, C1, C2) und der Akkustecker J2 gehören **weg vom Funkteil
  und weg von der NFC-Antenne**.

### Sicherheitschip — hier geht es um mehr als Elektrik

- **U2 auf die Innenseite, möglichst mittig unter dem Modul oder unter dem
  Akku.** Grund: `../docs/threat-model.md` nennt den Angreifer, der die
  Platine öffnet und abtastet. Je schwerer die I2C-Leitung zugänglich ist, desto
  besser — auch wenn SCP03 sie verschlüsselt.
- **Die I2C-Leitung so kurz wie möglich und ohne Testpunkte.** Ein Testpunkt
  auf SDA ist eine Einladung. Wenn zum Entwickeln einer gebraucht wird: auf der
  ersten Platine ja, auf der Zielplatine nicht.
- C10 und C11 dicht an Pin 12 bzw. Pin 18.
- **Die Brücke Pin 15 (`Vout`) → Pin 18 (`VCC`) ist Pflicht** und muss kurz
  sein — ohne sie gibt es kein Deep Power-down und damit 430 µA statt 3 µA.

### Anzeige

- Die aktive Fläche ist **10,54 × 14,70 mm**. Das Gehäusefenster muss mindestens
  so groß sein, mit etwas Rand für die Fertigungstoleranz.
- **Keine Ladungspumpen-Kondensatoren nötig** — sie sitzen auf dem Modul. Am
  Datenblatt geklärt: der 12-polige Steckverbinder führt weder `C1P/C1N/C2P/C2N`
  noch `VCC` heraus. Neben der Anzeige braucht es nur C13/C14 und den
  Lastschalter Q4. Einzelheiten in `schematic.md`.
- `VDD` (Pin 10) und `VBAT` (Pin 11) werden **miteinander verbunden**, beide
  Massepins (1 und 12) **müssen** angeschlossen werden.
- Die FPC-Fahne ist biegsam, aber nicht beliebig: Der ZIF-Steckverbinder J4
  sitzt **hinter** dem Glas, nicht daneben, damit die Fahne nach innen umgelegt
  wird und nicht seitlich Platz braucht.
- Das Glas ist **1,26 mm dick** und liegt direkt unter dem Gehäusefenster.
- Die Anzeige kommt auf die Seite, die der Nutzer ansieht — gegenüber dem Akku.

### Taster

- SW1 ist **seitlich betätigt** (Panasonic-Bauform „Side-operational"). Er
  gehört an die **lange Kante**, nicht auf die Fläche — dann drückt man ihn mit
  dem Daumen am Rand, so wie bei einem Autoschlüssel.
- Platz so wählen, dass man beim Drücken **die Anzeige nicht verdeckt** — der
  Sinn der Anzeige ist, dass man beim Freigeben liest, was freigegeben wird.
- Auf der gegenüberliegenden Seite kein Bauteil, das beim Drücken Druck
  bekommt.
- SW2 (Reset) ist nur zum Entwickeln gedacht — er darf von außen **nicht**
  erreichbar sein, sonst wird er versehentlich gedrückt. Als Lötpad oder unter
  dem Gehäuse.

### LED

- D1 neben die Anzeige, unter einen Lichtleiter oder ein mattes Fenster.
- Drei getrennte Leitungen (sechs Pins, keine gemeinsame Elektrode), also drei
  Vorwiderstände dicht an der LED.

### Fingerabdrucksensor: Stufe 1 ohne, Platz für Stufe 2

**Entschieden: Stufe 1 wird ohne Sensor gebaut** (Begründung in
`../docs/hardware.md`). Der Freigabeweg ist Taster + PIN.

Was die Platine dennoch tut, damit Stufe 2 keine neue Platine braucht:

- **J5 (Stiftfeld 1 × 6, 1,27 mm) wird verlegt, aber nicht bestückt**, samt den
  vier schnellen Leitungen `P0.13`–`P0.16`, der Versorgung über Q5 und
  `FP_IRQ`. Nachträgliches Verlegen in ein fertiges Layout ist teurer als ein
  paar ungenutzte Leiterbahnen.
- **Der Lastschalter Q5 wird bestückt**, auch ohne Sensor. Er kostet ein
  Bauteil und spart Stufe 2 eine Änderung; ohne ihn brächte der Sensor 13 µA
  Dauerstrom mit — mehr als unser gesamtes Ruhebudget.
- **Das Sensorfeld bleibt frei von hohen Bauteilen.** Es liegt auf der
  **Akkuseite**, im Stück des mittleren Drittels neben der Zelle — dort, wo der
  Finger später hinfasst. Mit der schmalen Zelle (12 mm) passt das BM-Lite
  (15 × 15 mm) bequem; mit der 19,75 mm breiten wird es eng.
- **Das Gehäuse bekommt an dieser Stelle von Anfang an eine Aussparung**, in
  Stufe 1 mit einem Blindstück geschlossen. So ist für Stufe 2 nur ein
  Gehäuseteil zu ändern.
- J5 gehört an die **Kante dieses Felds**, damit zum Ausprobieren ein
  Flachbandkabel herausgeführt werden kann, ohne dass das Gehäuse im Weg ist.

**Wichtig für Stufe 2:** Das BM-Lite muss **neben** die Zelle, nicht über sie.
Mit 2,8 mm ist es dünner als der Akku (3,8 mm) — nebeneinander bestimmt also
weiter der Akku die Höhe, und das Gehäuse bleibt gleich. Übereinander wären es
3,8 + 2,8 = **6,6 mm** auf dieser Seite, und das Gehäuse müsste neu entworfen
werden. Darum liegt das Sensorfeld neben dem Akku.

### Lagenaufbau

Vier Lagen vorgeschlagen:

| Lage | Inhalt |
| --- | --- |
| 1 (oben) | Bauteile, kurze Signale |
| 2 | **durchgehende Massefläche** — außer unter den Antennen |
| 3 | Versorgung `VSYS`, `VDD3V3`, geschaltete Zweige |
| 4 (unten) | Akku, Anzeige, restliche Signale |

Zwei Lagen würden auch gehen, aber die durchgehende Massefläche ist bei einem
Funkgerät mit 13,56-MHz-Antenne und 2,4-GHz-Modul das, was Ärger erspart.

**Unter beiden Antennen ist die Massefläche ausgenommen** — sonst wirkt sie als
Kurzschlusswindung bzw. verstimmt die Chipantenne.

### Zwei Regeln aus früheren Platinen

Sie stehen hier, weil sie schon einmal Zeit gekostet haben:

- **Pad-Größen kommen aus dem Gerber, nicht aus der Bibliothek.** Die
  Bibliotheksmaße waren schon um bis zu 4,5 mm daneben.
- **Netzfahnen mit einer Stichleitung absetzen**, nie direkt an den Pin kleben.
- Flächen **0,354 mm von der Kontur weg**; eingeschlossene Massepads brauchen
  eine eigene Durchkontaktierung.
- EasyEDA löscht Kupferinseln in der Ansicht; die echte Füllung steht erst im
  Export.

## Offene Punkte

1. **Die Freihaltezone um die Chipantenne des Moduls** ist aus dem
   Raytac-Dokument noch nicht entnommen (`ungeprueft`).
2. **Windungszahl und Fläche der NFC-Leiterbahnantenne** — nur durch Messung zu
   bestimmen.
3. **Welche Ferritfolie und wie dick?** Die Entscheidung „mit Folie" steht,
   das Material nicht. Zu klären: Permeabilität bei 13,56 MHz, Dicke
   (Zielwert etwa 0,3–0,5 mm) und Bezugsquelle in Kleinmenge. Die Angabe
   „wenige Cent und ein halber Millimeter" ist `ungeprueft`.
4. Die zusammengezählte Bauhöhe von etwa 6,7 mm plus Folie plus Gehäuse ist
   **nicht gerechnet**, nur addiert; Toleranzen und Klebeschichten fehlen.
5. Ob ein zweiter, passiver NFC-Pfad direkt am Sicherheitschip (dessen LA/LB
   derzeit auf Masse liegen) sinnvoll ist — das bräuchte eine zweite Antenne
   oder eine Umschaltung. **Nicht entschieden**, Einzelheiten in
   `schematic.md`.

**Entschieden und eingearbeitet:** Akku und NFC-Antenne werden räumlich
getrennt **und** durch eine Ferritfolie entkoppelt (Platine 70 statt 62 mm);
wird das Gehäuse zu groß, weicht die Kapazität, nicht der Abstand.
**Stufe 1 wird ohne Fingerabdrucksensor gebaut** — das Stiftfeld samt
Leitungen und Lastschalter Q5 wird verlegt, der Sensor nicht bestückt, und der
dadurch frei gewordene Platz geht in den Abstand zur Antenne (Antennenfeld
26 statt 22 mm). Die Ladungspumpen-Kondensatoren der Anzeige entfallen.
