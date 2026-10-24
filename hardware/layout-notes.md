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

## Bauform: 26 × 70 mm

**Platine 26 × 70 mm, 0,8 mm dick, vier Lagen, beidseitig bestückt.**

> **Berichtigung 24.10.2026: die Breite ist 26 mm, nicht 24.** Thomas hat das
> um 15:22 entschieden, der Gehäuse-Strang hat es in `cad/masse.md` eingetragen
> (Commit 41f4ec6). Diese Datei trug die 24 mm noch als Tatsache und hat damit
> den ersten Platzierungsentwurf auf die falsche Breite gestellt.
>
> **Grund für die 26 mm:** Der Fingerabdrucksensor der Stufe 2 passt bei 24 mm
> nicht neben die Zelle — es fehlten 1,40 mm. Bei 26 mm sind quer **23,60 mm**
> nutzbar statt 21,60, es bleiben 0,60 mm Reserve.
>
> **Die Längsaufteilung ist davon nicht betroffen**, nur die `x`-Werte.

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

70 × 26 mm ist noch Schlüsselanhängerformat — etwa so groß wie ein
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

- C5 (4,7 µF an `VDD`) **unmittelbar an den Modulpin 28** — die vom Datenblatt
  vorgeschriebene Entkopplung, keine Zugabe.
  **Berichtigung 24.10.2026:** C6 (`DEC4`) und C7 (`DECUSB`) entfallen. Das
  Raytac-Modul führt diese Pins nicht heraus, die Entkopplung sitzt im Modul
  (nachgezählt: 48 GPIOs + 5 × GND + 8 Versorgungs- und Datenpins = 61).
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

## Nachtrag 24.10.2026: L1 hat ein Streufeld

Aus dem Schaltplan-Durchgang (`schematic-notes.md`): **L1 ist die Spule der
REG0-Stufe** an `DCCH` — gewählt ist die **TDK NLCV25T-100K-EFRD**, 10 µH,
Gehäuse **1008 (2,5 × 2,0 mm)**, 360 mA, 1,1 Ω. Sie ist größer als die im
Maßtabellen-Abschnitt genannten 0603, weil kein 0603- und kein 0805-Teil der
Bibliothek den nötigen Strom ausweist. **Die Maßtabelle oben ist insoweit
überholt: L1 braucht 2,5 × 2,0 mm, nicht 0603.**

**Platzierungsauflage:** Eine drahtgewickelte Spule ohne Schirmung hat ein
Streufeld. L1 gehört darum

- **weg von der Chipantenne des Moduls** (die braucht ohnehin Freiraum), und
- **weg von der NFC-Antenne und ihren Leitungen.**

Dazu bleibt sie **nah an `DCCH`** — das ist die Vorgabe aus `schematic.md` und
sie widerspricht der Trennung nicht, weil Modulpin 31 auf der modulabgewandten
Seite liegt. Das ist beim Platzieren gemeinsam mit der Regel zu lösen, dass der
**Akku nicht hinter der NFC-Antenne** liegen darf.

> **`ungeprueft`:** Dass die NLCV-Reihe drahtgewickelt und ungeschirmt ist,
> steht in keiner erreichbaren Quelle — die Produktseite von TDK antwortet mit
> HTTP 403, die Händlerangabe schweigt zur Bauart. Die Auflage gilt trotzdem:
> sie kostet nur Platzierungsarbeit, und die sicherere Variante ist hier die
> richtige. Wer das Gegenteil belegt, darf die Auflage streichen.

## Erster Layout-Schritt: was ging und was nicht (24.10.2026)

Vom Schaltplan unabhängig und darum vorgezogen:

- **Vier Kupferlagen sind gesetzt** (gemessen: vorher 2, nachher 4). Die Lagen
  heißen `TOP`, `INNER_1`, `INNER_2`, `BOTTOM`.
- **Der Platinenumriss ist nicht gezeichnet.** EasyEDA lehnt das Anlegen von
  Linien auf dem Umriss-Layer ab: *„错误：无法创建直线图元"*. **Beide Wege
  geprüft, drei Versuche:**

  | Versuch | Werkzeug | Ergebnis |
  | --- | --- | --- |
  | 1 | `pcb_apply`, `lines[]` mit `layer: BOARD_OUTLINE`, 0,15 mm | abgelehnt, 4 von 4, auch im Wiederholungsschritt der Brücke |
  | 2 | `pcb_line_create` einzeln, mit und ohne `net`, bei 0/0 und bei 100/100 | abgelehnt |
  | 3 | `pcb_apply` erneut, 0,254 mm, vorher `dry_run` (Plan sauber: 4 Linien) | abgelehnt, 4 von 4 |

  **Gegenprobe:** dieselbe Linie auf `TOP` mit Netz `GND` gelingt auf Anhieb —
  Testspur danach gelöscht. Es liegt also nicht am Werkzeug, nicht an der
  Breite, nicht an den Koordinaten und nicht am Netz. Nachgezählt:
  `pcb_lines_list` auf dem Umriss-Layer liefert **0 Segmente**.

  > **Berichtigung 24.10.2026:** Hier stand, der Umriss-Layer komme in
  > EasyEDAs Typliste für Linien nicht vor. **Das ist falsch.** Die Liste
  > `TPCB_LayersOfLine` führt `EPCB_LayerId.BOARD_OUTLINE` ausdrücklich auf —
  > nachgelesen in der API-Referenz. Erlaubt ist der Layer also; die
  > Umsetzung lehnt ihn trotzdem ab, in fünf Spielarten (mit und ohne Netz,
  > mit vorhandenem Netz `GND`, mit und ohne Breitenangabe, bei 0/0 und bei
  > 100/100, einzeln und im Änderungssatz). **Die Ursache ist damit unbekannt,
  > nicht erklärt.** Der Befund bleibt: über die Brücke geht es nicht.

  **26 × 70 mm sind darum im Editor von Hand zu zeichnen**, bevor platziert
  wird. Die Ecken müssen exakt aufeinandertreffen, sonst ist der Rand offen.
- **Die Entwurfsregeln heißen noch „JLCPCB Capability(Two Layers Board)".**
  Auch hier beides geprüft: EasyEDAs Schnittstelle hat Leseoperationen für
  gespeicherte Regelsätze (`getAllRuleConfigurations`, `getRuleConfiguration`,
  `getCurrentRuleConfigurationName`), **aber keine, die einen davon auswählt** —
  und die Brücke bietet keine davon an (`UNKNOWN_OP`), Rohzugriff ist
  ausgeschaltet. `pcb_rules_set` kann nur den ganzen Regelbaum überschreiben;
  die Werte dafür wären geraten. **Den Vier-Lagen-Satz im Editor wählen, vor
  dem Verlegen.**

Erst danach lohnt `pcb_import_changes` — sonst landen 54 Bauteile auf einer
Platine ohne Umriss und mit den Regeln für zwei Lagen.

## Platzierungsentwurf 24.10.2026 — gerechnet, nicht gesendet

Vorbereitet, solange Umriss und Regelsatz fehlen. **Nichts davon ist in EasyEDA
eingetragen.** Wenn der Umriss steht, wird das hier abgearbeitet.

### Koordinatensystem

Nullpunkt **untere linke Ecke**, `x` quer über die Platine (0 … 26 mm), `y` in
der Länge (0 … 70 mm), **`y` = 0 an der USB-Kante**. Alle Angaben sind
Bauteilmitten in Millimetern. „oben" = Bestückungsseite mit dem Modul,
„unten" = Rückseite mit dem Akku.

### Die vier Bänder, und wie die Länge aufgeht

| Band | `y` | Länge | Inhalt |
| --- | --- | --- | --- |
| Z1 | 0 … 9,5 | 9,5 mm | USB-C-Stecker |
| Z2 | 9,5 … 25 | 15,5 mm | Funkmodul, Entkopplung, L1, Quarz |
| Z3 | 25 … 51 | 26,0 mm | Akku (unten), Anzeige (oben) |
| Z4 | 51 … 70 | 19,0 mm | NFC-Schleife, Taster |

9,5 + 15,5 + 26,0 + 19,0 = **70,0 mm**. Geht auf.

> ### Dabei fällt ein Rechenfehler der bisherigen Vorgaben auf
>
> Der Abschnitt „Die Aufteilung" oben sagt, der Akku ende bei 44 mm und das
> Antennenfeld sei **26 mm** breit. **Das geht mit dieser Zelle nicht.** Die
> Adafruit 1317 ist **19,75 × 26,02 mm**: Damit der Akku bei 44 endete, müsste
> seine 26-mm-Seite **quer** liegen — quer sind aber nur 26 mm Platine da, und
> mit Randabstand bleibt das zu knapp.
> Die Zelle muss also längs liegen, 25 … 51 mm, und **das Antennenfeld wird
> 19 mm statt 26 mm.**
>
> Die schmale Alternative hilft hier nicht: die LP-401230 ist **30,5 mm lang**,
> damit bliebe das Antennenfeld sogar nur 14,5 mm — sie gewinnt Breite
> (12 statt 19,75 mm), nicht Länge. **Zu entscheiden** (Vorschlag: 19 mm
> behalten und die Schleife am ganzen Rand von Z4 umlaufen lassen, Fläche
> 19 × 26 mm = 494 mm²; das ist mehr Fläche als eine Schleife im 26-mm-Feld bei halber
> Breite hätte).

### Z1 — USB-C, `y` 0 … 9,5

| Teil | Seite | x | y | Dreh. | Warum |
| --- | --- | --- | --- | --- | --- |
| J1 | oben | 13,0 | 4,6 | 0° | mittig an der kurzen Kante, Kabel geht gerade ab; die vier Haltestifte nehmen die Zugkraft auf |
| R15 | unten | 9,0 | 7,5 | 0° | dicht an J1 A5 |
| R16 | unten | 17,0 | 7,5 | 0° | dicht an J1 B5, symmetrisch zu R15 |
| U5 | unten | 13,0 | 10,5 | 90° | **das erste Bauteil nach dem Stecker** — Sinn eines ESD-Teils; D+/D− gehen von hier als Paar gleicher Länge zum Modul |
| D2 | unten | 4,0 | 11,0 | 0° | an `VBUS`, nahe am Stecker |
| C1 | unten | 21,0 | 11,0 | 0° | `VBUS`-Puffer am Ladereglereingang |

### Z2 — Funkteil, `y` 9,5 … 25

| Teil | Seite | x | y | Dreh. | Warum |
| --- | --- | --- | --- | --- | --- |
| U1 | oben | 16,75 | 17,3 | 90° | Längsachse quer, **Chipantenne zum Rand `x` = 26** (etwa 1,5 mm Abstand, Antennenkante bei `x` 24,5); die Pinreihe mit VDD/VDDH/DCCH liegt damit auf der antennenabgewandten Seite |
| C5 | unten | 7,0 | 20,0 | 0° | unmittelbar an Modulpin 28 (`VDD`) |
| C3, C4 | unten | 7,0 / 7,0 | 22,5 / 14,0 | 0° | `VSYS` an Modulpin 30 |
| L1 | unten | 4,0 | 17,3 | 90° | an `DCCH` (Pin 31), **kurze dicke Bahn**; gleichzeitig am weitesten weg von der Chipantenne (`x` 24,5) und von Z4 — das löst die Streufeld-Auflage |
| X1 | unten | 11,0 | 13,0 | 0° | an `P0.00/P0.01` (Pin 17/18), kurze symmetrische Bahnen |
| C8, C9 | unten | 11,0 | 10,5 | 0° | je an einem Quarzbein, gleich lang |
| Q2, Q3, R4, R5 | unten | 19 … 24 | 10 … 14 | – | Lastumschaltung, nahe `VBUS`/`VSYS`, aber außerhalb der Antennen-Freihaltezone |

### Freihaltezone der Modulantenne — belegt, 24.10.2026

Quelle: **Raytac Approval Sheet, Abschn. 2.3 „RF Layout Suggestion (aka
Keep-Out Area)", gerenderte Seite 13/14, mit Maßbild.** Mein erster Entwurf
hatte hier 7 × 17 mm geschätzt; das war in beide Richtungen falsch.

- **„No Ground-Pad Area": 10,5 mm breit × 3,8 mm tief.** Es ist **kein Rahmen
  um das Modul**, sondern ein **Streifen an der Antennenkante**: 3,8 mm über
  die Modulkante hinaus, 10,5 mm der Breite nach.
- **In jeder Lage.** Wörtlich: *„No Ground Pad should be included in the
  corresponding position of the antenna in EACH LAYER."* Also auch `INNER_1`
  und `INNER_2` — bei vier Lagen die Stelle, an der man es leicht falsch macht.
- **Modul an den Platinenrand**, nicht in die Mitte: *„Place the module towards
  the edge of PCB to have better performance than placing it on the center."*

**Was das für dieses Brett heißt:** Die Antennenkante des Moduls liegt bei
`x` = 24,5, der Platinenrand bei 26. Von den 3,8 mm Tiefe liegen also nur
**1,5 mm auf der Platine**, der Rest fällt über die Kante hinaus. Kupferfrei zu
halten ist damit nur der Streifen

> **`x` 24,5 … 26, `y` 11 … 21,5 — in allen vier Lagen.**

Das sind 1,5 × 10,5 mm, rund 16 mm². Die Zone kostet also fast nichts — weil
das Modul am Rand sitzt. Hätte es in der Mitte gesessen, wären es 40 mm² in
jeder Lage gewesen.

**Dazu aus derselben Quelle, vor dem Gießen der Masseflächen zu beachten:**
*„Please add via holes in GROUND area as many as possible, especially around
the four corners."* Also Durchkontaktierungen in der Massefläche, besonders an
den vier Ecken.

Raytac bietet an, ein Layout als PDF gegenzulesen (`service@raytac.com`).
**Hier nur vermerkt** — Kontakt zu Herstellern nimmt Thomas selbst auf.

### Z3 — Akku unten, Anzeige oben, `y` 25 … 51

| Teil | Seite | x | y | Dreh. | Warum |
| --- | --- | --- | --- | --- | --- |
| Akku (kein Bauteil) | unten | 13,0 | 38,0 | 0° | 19,75 × 26,02 mm, liegt auf Ferritfolie; **Fläche darunter frei von Bauteilen halten**, damit die Zelle plan liegt |
| J2 | unten | 13,0 | 49,5 | 180° | am akkufernen Ende von Z3, Kabel läuft zur Zelle zurück; weg vom Funkteil |
| J4 | oben | 13,0 | 27,0 | 0° | FPC-Fahne der Anzeige zeigt nach `y` = 0, die Fahne wird unter das Glas gelegt |
| DS1 (Glas) | oben | 13,0 | 38,0 | 0° | 13,90 × 22,00 mm, Fenster im Gehäuse; aktive Fläche 10,54 × 14,70 mm |
| Q4, R9, C13, C14 | oben | 3 … 6 | 28 … 36 | – | Lastschalter der Anzeige, neben dem Glas, kurze Bahn zu J4 Pin 10/11 |
| U2 + C10, C11 | unten | 6,0 | 47,5 | 0° | Sicherheitschip, **am akkufernen Ende** (siehe Entscheidung unten); `SE_VOUT` von Pin 15 nach Pin 18 **so kurz wie möglich** |
| U3 + C12, R8 | unten | 19,0 | 47,5 | 0° | Flash gegenüber von U2, gleiches Band |
| R6, R7 | unten | 6,0 | 50,0 | 0° | I2C-Abschluss, dicht an U2 |
| C1, U4, C2, R1, R2 | unten | 4 … 22 | 6 … 12 | – | Ladepfad gebündelt **nach Z1 verlegt**, zu C1 an `VBUS` — weg vom Funkteil und weg von der Zelle |
| Q5, R17 | unten | 22,0 | 40,0 | 0° | Lastschalter Sensor; Stufe 2 |

**Der Streifen neben der Zelle, mit 26 mm neu gerechnet:** 26 − 19,75 =
**6,25 mm**. Mittig gelegt ergibt das **zweimal 3,13 mm** (vorher zweimal
2,13 mm).

**Reicht nicht, und zwar nachgerechnet:**

| Teil | Körper | mit 0,25 mm Abstand je Seite | passt in 3,13 mm? |
| --- | --- | --- | --- |
| U2 SE051 | 3,0 × 3,0 mm | **3,5 mm** | **nein**, 0,37 mm zu breit |
| U3 Flash | 8,0 × 6,0 mm (gedreht 6,0 breit) | **6,5 mm** | **nein**, um mehr als das Doppelte |

Die zwei Millimeter entschärfen die Engstelle also **nicht**. **U2 und U3
bleiben bei `y` 44 … 51**, der Ladepfad bleibt in Z1, und unter Z3 liegt nur
die Zelle — sie liegt dann plan auf der Ferritfolie.

### Eine Möglichkeit, die durch die 26 mm erst entsteht — zu entscheiden

Wenn die Zelle **nicht mittig**, sondern an die Kante `x` = 0 gerückt wird
(Zelle `x` 0,75 … 20,5), bleibt auf der anderen Seite ein Streifen von
**5,5 mm**, nutzbar **5,0 mm**:

- **U2 passt dort** (3,5 mm nötig, 1,5 mm Luft übrig).
- U3 passt weiterhin nicht (6,5 mm nötig).

**Was das bringt:** U2 käme von `y` 47,5 auf etwa `y` 33 — die I2C-Leitung
zwischen Modul und Sicherheitschip wird damit **rund 12 mm statt 25 mm**. Das
ist die Leitung, die ohnehin auf einer Innenlage zwischen den Masseflächen
laufen soll; sie zu halbieren ist ein echter Gewinn, nicht Kosmetik.

**Was es kostet:** Die Zelle liegt nicht mehr mittig, also auch die Ferritfolie
nicht. Das berührt die Gehäusegestaltung und das Gewicht in der Hand.

> **Nicht von mir entschieden.** Die Lage der Zelle gehört mit dem
> Gehäuse-Strang abgestimmt. Bis dahin gilt die mittige Zelle und U2 bei
> `y` 47,5.

### Z4 — NFC, `y` 51 … 70

| Teil | Seite | x | y | Dreh. | Warum |
| --- | --- | --- | --- | --- | --- |
| NFC-Schleife | oben + unten | – | 51 … 70 | – | Schleife am Rand umlaufend, Fläche etwa 19 × 26 mm; **innen keine geschlossene Massefläche** |
| C15, C16 | unten | 10,0 / 16,0 | 52,0 | 0° | symmetrisch zur Mitte, **gleiche Bahnlängen zu `P0.09`/`P0.10`** |
| D1 | oben | 13,0 | 66,0 | 0° | RGB-LED am Kopfende, von außen sichtbar; R10 … R12 daneben |
| J5 | unten | 13,0 | 56,0 | 0° | Stiftfeld Fingerabdruck, Stufe 2, **verlegt aber nicht bestückt** |
| J6 | unten | 13,0 | 67,0 | 0° | SWD am Kopfende, beim Entwickeln erreichbar |

**In Z4 steht jetzt kein Metall mehr** außer der LED am Kopfende und dem
unbestückten Stiftfeld J5 — die Taster sind herausgezogen, siehe unten.

### Die Taster: aus der Schleifenfläche heraus, `y` 24,5

| Teil | Seite | x | y | Dreh. | Warum |
| --- | --- | --- | --- | --- | --- |
| SW1 | oben | 3,5 | 24,5 | 90° | seitlich betätigt zur Kante `x` = 0; liegt **zwischen Modul (endet 21,5) und Anzeigeglas (beginnt 27)** — dem natürlichen Daumenplatz, wenn der Schlüssel in der Hand liegt |
| SW2 | oben | 22,5 | 24,5 | 270° | Reset, gegenüberliegende Kante, damit er nicht versehentlich gedrückt wird |

Das löst den Einwand von vorhin: **beide Taster sind aus der Antennenfläche
heraus**, ohne die Bedienbarkeit zu verschlechtern — sie sitzen jetzt neben
der Anzeige, die man ohnehin ansieht, statt am Kopfende.

- Platzrechnung: Taster 3,5 mm lang → `y` 22,75 … 26,25. Modul endet bei 21,5,
  Glas beginnt bei 27. **Bleibt 1,25 mm Luft zum Modul und 0,75 mm zum Glas.**
  Eng, aber es passt; beim Zeichnen nachmessen.
- SW2 liegt bei `x` 20,5 und damit **außerhalb** des Freihaltestreifens
  (`x` 24,5 … 26, `y` 11 … 21,5).

### Bedingungen für die NFC-Abstimmung — gehören ins Messprotokoll

Nicht nur hierher: **eine Abstimmung, die unter anderen Bedingungen gemessen
wurde, ist wertlos.** Beim Wobbeln müssen am Brett sein:

1. **Der Akku eingelegt**, mit der Ferritfolie darunter — die Zelle ist
   leitfähig und dämpft.
2. **Das Gehäuse geschlossen**, wenn es eines gibt.
3. **C15 = C16**, immer gleich gehalten.

Dass die Taster bestückt sein müssen, ist mit dem Verschieben erledigt — sie
liegen nicht mehr in der Schleifenfläche.

### Was an diesem Entwurf noch offen ist

1. **Die genauen Pinlagen** aller Bauteile ergeben sich erst aus den
   Footprints. **Pad-Maße aus dem Gerber prüfen**, nicht aus der Bibliothek.
2. **1,25 mm / 0,75 mm Luft um die Taster** — beim Zeichnen nachmessen.
   **Die 0,75 mm zum Anzeigeglas sind ein Punkt fürs Gehäuse, nicht nur fürs
   Layout:** Glasrand und Gehäuserahmen haben beide Toleranz, und 0,75 mm sind
   schnell weg. Der Taster wird **seitlich** betätigt, braucht also eine
   Öffnung in der Rahmenwand bei `y` 22,75 … 26,25 an beiden Längskanten —
   dort, wo das Anzeigefenster bei `y` 27 beginnt. **An den Gehäuse-Strang
   übergeben** (Übergabenotiz im gemeinsamen Notizordner,
   `uebergabe-gehaeuse-taster.md`).
3. **Die Reichweite der kleineren Antennenschleife** ist nicht bekannt und wird
   geringer sein als mit 26 mm Feld. **Gehört ehrlich ins Handbuch**, nicht in
   eine Enttäuschung beim Antippen. Ist an den Handbuch-Strang zu übergeben.

**Entschieden am 24.10.2026 (Master):**

- **Antennenfeld 19 mm**, Schleife am ganzen Rand von Z4. Fläche schlägt Länge;
  die schmale Zelle zu nehmen, um Länge zu verlieren, wäre ein schlechter
  Tausch. **C15/C16 sind damit endgültig ein Messwert am fertigen Brett**, kein
  Rechenwert — die 120 pF sind nur der Startpunkt.
- **U2 und U3 nach `y` 44 … 51.** Das stärkere Argument ist nicht der Platz,
  sondern dass die Zelle dann **plan auf der Ferritfolie liegt**.
  **Auflage:** Die I2C-Leitung zwischen Modul und U2 wird dadurch rund 25 mm
  lang. **Sie läuft auf einer Innenlage zwischen den beiden Masseflächen**,
  nicht außen. Die Verbindung ist mit SCP03 verschlüsselt, aber eine lange, von
  außen anstechbare Leitung ist trotzdem die bequemste Angriffsstelle auf der
  ganzen Platine — und sie zu vergraben kostet nichts.

## Stufe 2 mit Sensor: verträglich quer, nicht in der Länge

Der Gehäuse-Strang gibt für Stufe 2 die schmale Zelle **LP-401230-J** vor und
die Aufteilung **Zelle 0 … 12 mm, Sensorfeld 12,5 … 23,5 mm**. Hier
nachgerechnet, ohne es in den Entwurf einzuarbeiten:

### Zuerst: die Achse ist zu klären

Die Vorgabe nennt diese Werte als `y`. **In meinem Bezugssystem kann das nicht
sein:** `y` 0 … 12 ist bei mir das USB-Band, dort sitzt der Stecker. Zwei
Anzeichen, dass die Werte **quer** gemeint sind, also meinem `x` entsprechen:

1. **12 mm ist die Breite der LP-401230**, nicht ihre Länge (12 × 30,5 mm).
2. **12 + 0,5 + 11 = 23,5 mm** — und quer sind bei 26 mm Platine **23,60 mm**
   nutzbar. Das ist genau die Rechnung, mit der die 26 mm begründet wurden.

> **Zu bestätigen:** Ist die Vorgabe quer gemeint (mein `x`)? Dann passt sie,
> siehe unten. Falls sie wirklich längs gemeint ist, stimmt etwas anderes
> nicht, und wir müssen die Nullpunkte vergleichen, bevor jemand zeichnet.
> **Zwei Zahlen aus verschiedenen Bezugssystemen nebeneinanderzulegen hat
> heute schon einmal eine falsche Geschichte ergeben** — siehe die
> Zeitstempel in `schematic-notes.md`.

### Quer: verträglich, mit 0,10 mm Luft

Zelle 12 mm + 0,5 mm Abstand + Sensorfeld 11 mm = **23,5 mm** bei **23,60 mm**
nutzbarer Breite. Es passt, aber **die Reserve ist 0,10 mm, nicht 0,60 mm** —
die 0,60 mm gelten für den Sensor allein, nicht für die Summe mit dem
Trennabstand. Zu prüfen, wer von beiden recht hat, bevor es knapp wird.

### In der Länge: das wirft meine Bandaufteilung um

**Die LP-401230 ist 30,5 mm lang.** In Z3 stehen 26 mm zur Verfügung (`y`
25 … 51), weil die Adafruit-Zelle 26,02 mm lang ist.

| | Länge der Zelle | Z3 wird | Antennenfeld Z4 wird |
| --- | --- | --- | --- |
| Stufe 1, Adafruit 1317 | 26,02 mm | 25 … 51 | **19 mm** |
| Stufe 2, LP-401230-J | **30,5 mm** | 25 … 55,5 | **14,5 mm** |

**Stufe 2 kostet also 4,5 mm Antennenfeld**, nicht nichts. Das ist derselbe
Zusammenhang wie bei der ersten Entscheidung: die schmale Zelle gewinnt Breite
und verliert Länge.

**Mein Entwurf für Stufe 1 bleibt davon unberührt** — er ist auf die Adafruit
1317 gerechnet. Aber **wer Stufe 2 will, muss das Antennenfeld noch einmal
ansehen**, und zwar bevor die Platine gefertigt wird, denn die Schleife wird
als Leiterbahn gezeichnet und ist später nicht verschiebbar.

> **Offen, für den Gehäuse-Strang und den Master:** Soll die Platine schon in
> Stufe 1 so aufgeteilt werden, dass Stufe 2 ohne Neuzeichnen passt (dann
> Antennenfeld gleich 14,5 mm und die kleinere Schleife von Anfang an) — oder
> bleibt Stufe 1 auf 19 mm und Stufe 2 bekommt ein eigenes Layout? **Das ist
> eine Entscheidung über Reichweite gegen Aufwand, keine Rechenfrage.**
