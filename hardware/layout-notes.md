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

## Vorschlag für die Bauform

**Platine 24 × 62 mm, 0,8 mm dick, vier Lagen, beidseitig bestückt.**

Zur Dicke: **0,8 mm statt 1,6 mm ist keine freie Wahl**, wenn der USB-C-Stecker
später direkt am Platinenrand sitzen soll — alle gefundenen
Platinenrand-Stecker passen nur auf 0,8 mm. Auch wenn in Stufe 1 die Buchse
verbaut wird, lohnt es, die Dicke gleich festzulegen. Dazu: **ENIG statt HASL**,
weil die dickere HASL-Schicht das erste Stecken erschwert.

Gesamthöhe grob: 0,8 mm Platine + 3,8 mm Akku + 2,05 mm Modul auf der anderen
Seite ≈ **6,7 mm plus Gehäuse**, also etwa 8–9 mm fertig. Das ist für einen
Schlüsselanhänger vertretbar. **Nicht gerechnet**, nur zusammengezählt.

## Der Konflikt: Akku gegen NFC-Antenne

Das ist die wichtigste Layout-Entscheidung, und sie ist noch nicht gefallen.

**Der Akku darf nicht hinter der NFC-Antenne liegen** — er dämpft das Feld. Die
Begründung ist belegt: Das Taoglas-Datenblatt zur Flexantenne sagt, dass die
Reichweite **ohne Ferritlage auf leitfähigem Untergrund praktisch null** ist.
Eine LiPo-Zelle mit ihrer Aluminiumhülle ist genau so ein Untergrund.

Auf 24 × 62 mm ist das eng. Drei Wege:

### Weg 1: Räumliche Trennung — Platine länger

```
   ┌──────────────────────────────────────────────────────┐
   │ USB-C │  Modul U1  │   Akku unten    │  NFC-Antenne  │
   │  J1   │  (Antenne  │   OLED oben     │  als Schleife │
   │       │   nach     │   SE051, Flash  │  rundum       │
   │       │   außen)   │                 │  + Taster     │
   └──────────────────────────────────────────────────────┘
     0    9        25            48              70 mm
```

Antenne und Akku liegen in verschiedenen Dritteln. Kostet **etwa 8 mm mehr
Länge** (70 statt 62 mm). Kein Ferrit nötig, volle Reichweite.

**Das ist der sauberste Weg und die Empfehlung**, solange die Länge
hinnehmbar ist. 70 × 24 mm ist noch Schlüsselanhängerformat — etwa so groß wie
ein Autoschlüssel.

### Weg 2: Ferritlage zwischen Akku und Antenne

Antenne als Schleife um die ganze Platine, Akku in der Mitte, dazwischen eine
Ferritfolie. Platine bleibt bei 62 mm.

**Nachteil:** Die Reichweite sinkt, um wie viel ist **`ungeprueft`**. Dazu
kommen eine zusätzliche Folie in der Fertigung und mehr Bauhöhe.

### Weg 3: NFC in Stufe 1 weglassen

Stufe 1 nur USB und Bluetooth. Die Antennenfläche und die beiden
Abstimmkondensatoren werden **vorgesehen, aber nicht bestückt**.

**Dafür spricht mehr, als es zunächst scheint:** Die NFC-Abstimmung ist ohnehin
eine **Messaufgabe mit Netzwerkanalysator** (siehe `schematic.md`), nicht etwas,
das man einmal richtig zeichnet. Sie von der ersten Platine zu trennen, nimmt
Risiko aus dem ersten Aufbau.

**Noch nicht entschieden.** Die Entscheidung gehört dem Nutzer; sie betrifft
die Platinenlänge und damit die Gehäuseform.

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

### Fingerabdrucksensor, wenn er kommt

- Das FPC BM-Lite ist **15 × 15 × 2,8 mm** — das größte Einzelteil nach dem
  Akku und **dicker als der Akku**. Es bestimmt dann die Bauhöhe.
- Es gehört auf die **Seite, die der Finger berührt**, also dieselbe Seite wie
  der Taster oder gegenüber der Anzeige. Beides geht nicht gleichzeitig gut —
  **das ist der Grund, warum die Bauform vom Sensorentscheid abhängt.**
- In Stufe 1 bleibt nur das Stiftfeld J5; es gehört an eine Kante, damit ein
  Flachbandkabel zum Ausprobieren herausgeführt werden kann.

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

1. **Akku gegen NFC-Antenne: Weg 1, 2 oder 3?** Betrifft die Platinenlänge und
   die Gehäuseform. Entscheidung durch den Nutzer.
2. **Wird der Fingerabdrucksensor vorgesehen?** Mit ihm ist das Gerät dicker
   und die Seitenaufteilung anders.
3. **Die Freihaltezone um die Chipantenne des Moduls** ist aus dem
   Raytac-Dokument noch nicht entnommen (`ungeprueft`).
4. **Windungszahl und Fläche der NFC-Leiterbahnantenne** — nur durch Messung zu
   bestimmen.
5. Die zusammengezählte Bauhöhe von etwa 6,7 mm plus Gehäuse ist **nicht
   gerechnet**, nur addiert; Toleranzen und Klebeschichten fehlen.
6. Ob ein zweiter, passiver NFC-Pfad direkt am Sicherheitschip (dessen LA/LB
   derzeit auf Masse liegen) sinnvoll ist — das bräuchte eine zweite Antenne
   oder eine Umschaltung. **Nicht entschieden**, Einzelheiten in
   `schematic.md`.
