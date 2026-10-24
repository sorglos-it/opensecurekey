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
| Akku (kein Bauteil) | unten | **11,075** | 38,0 | 0° | 19,75 × 26,02 mm, **außermittig: `x` 1,20 … 20,95** (entschieden 24.10.2026, siehe unten); liegt auf Ferritfolie; **Fläche darunter frei von Bauteilen halten**, damit die Zelle plan liegt |
| J2 | unten | 13,0 | 49,5 | 180° | am akkufernen Ende von Z3, Kabel läuft zur Zelle zurück; weg vom Funkteil |
| J4 | oben | 13,0 | 27,0 | 0° | FPC-Fahne der Anzeige zeigt nach `y` = 0, die Fahne wird unter das Glas gelegt |
| DS1 (Glas) | oben | 13,0 | 38,0 | 0° | 13,90 × 22,00 mm, Fenster im Gehäuse; aktive Fläche 10,54 × 14,70 mm |
| Q4, R9, C13, C14 | oben | 3 … 6 | 28 … 36 | – | Lastschalter der Anzeige, neben dem Glas, kurze Bahn zu J4 Pin 10/11 |
| U2 | unten | **22,9** | **33,0** | 0° | Sicherheitschip **im Streifen neben der Zelle** (`x` 20,95 … 24,80, 0,85 mm Luft); `SE_VOUT` von Pin 15 nach Pin 18 **so kurz wie möglich**; I2C zum Modul **rund 12 mm, auf einer Innenlage** |
| C10, C11 | unten | 22,9 | 30,0 / 36,0 | 90° | im selben Streifen, dicht an U2 Pin 12 bzw. 18 |
| U3 | unten | **17,0** | **19,5** | 0° | Flash **unter das Modul**, nicht ins Akkufeld (siehe Fund unten). Körper 8 × 6 mm → `x` 13 … 21, `y` 16,5 … 22,5. Nebenwirkung, die zählt: die sechs QSPI-Leitungen werden damit **kurz** — der Takt läuft mit bis zu 32 MHz |
| C12, R8 | unten | 13,0 / 20,0 | 24,0 | 0° | dicht an U3 Pin 8 bzw. Pin 1, außerhalb des Zellfelds (beginnt bei `y` 25) |
| R6, R7 | unten | 22,9 | 27,5 / 38,5 | 90° | I2C-Abschluss, im Streifen dicht an U2 |
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

## Antworten an den Gehäuse-Strang, 24.10.2026

### 1. Die LED: längs 66 mm, quer 13 mm, Oberseite

**Das Lichtfenster gehört an das Kopfende, nicht in die Mitte.** D1 sitzt nach
dem Platzierungsentwurf bei

> **`x` = 13,0 mm (Plattenmitte), `y` = 66,0 mm, Oberseite.**

Das ist **4 mm vom Ende** der Platine entfernt, am vom USB-Stecker
abgewandten Ende. Die LED ist **1,6 × 1,6 mm groß und 0,5 mm hoch**
(Everlight 19-337C), sie strahlt nach oben.

Die vorläufigen 24,5 mm längs gehen nicht: **dort sitzen die beiden Taster**
(`y` 22,75 … 26,25). Und die 47 mm aus dem ersten Versuch liegen mitten im
Anzeigeglas (`y` 27 … 49). Das Kopfende ist die einzige Stelle, die beides
nicht berührt — und sie ist auch die bessere: die LED zeigt an, ohne das
Anzeigefenster zu teilen.

### 2. Die Taster bleiben, wo sie sind

Die Freigabe ist angekommen: **3,90 mm Luft zum Anzeigefenster**, nicht 0,75 —
das Fenster ist ein glatter Durchbruch ohne heruntergezogenen Rand und beginnt
erst bei längs 30,15. **Kein Verschieben.**

Dazu ein Punkt, der aus dem Gehäuse kam und hier festgehalten gehört: Der
**Klemmkragen** des Gehäuses läuft 1,2 mm breit um den Platinenrand, genau
dort, wo ein seitlich betätigter Taster sitzt. Er ist an beiden Längskanten
freigestellt worden. **Beim Layout nicht zubauen.**

> **Prüfpunkt fürs Layout, aus Panasonics Datenblatt gegen den Händler:**
> Panasonic gibt den EVQP7C01P mit **3,5 × 3,55 mm** an, DigiKey mit
> **4,70 × 2,90 mm**. Hat die Bibliothek das Händlermaß genommen, steht der
> Taster **1,2 mm woanders**, als das Gehäuse erwartet. **Am Footprint
> gegenprüfen**, und zwar aus dem Gerber, nicht aus der Bibliothek. Der Stößel
> ragt 0,65 mm über den Körper, der Hub ist 0,2 mm; „side-push" ist bestätigt.

### 3. Der USB-Stecker: drei Wege, nicht zwei

Der Befund stimmt: Der **USB4085 ist „PCB Top Mount"** — der Körper sitzt
vollständig auf der Platine, 3,46 mm Profilhöhe gegen 2,35 mm Hohlraum, **rund
1,1 mm zu hoch.**

**Was die Bibliothek an Mid-Mount-Buchsen hergibt** (in diesen Katalogen heißt
Mid-Mount **沉板**, „Sinkplatte"):

| Teil | LCSC | Angabe |
| --- | --- | --- |
| `USB-TYPE_C16P沉` | C9900001620 | Footprint „TYPE_C **沉板1.6**" — für **1,6 mm** Platine |
| U22410-02 | C9900262527 | „16pin **沉板1.6** 露舌" — für **1,6 mm** Platine |
| U22409-01 | C9900262530 | „16pin 防水沉板" (wasserdicht), Dicke nicht genannt |
| HYC-1160-003 | C9900062937 | „沉板TYPE-C", Dicke nicht genannt |
| GCT USB4110-GF-A | C5143397 | gleiche Reihe wie unser Teil, 16-polig, USB 2.0 — **aber nirgends als Mid-Mount belegt** |

**Hier ist der Haken, und er ist größer als die Teilefrage:** **Unsere Platine
ist 0,8 mm dick.** Die beiden Teile, die ihre Dicke nennen, sind für **1,6 mm**
gemacht. Ein Mid-Mount sitzt in einem Ausschnitt und stützt sich auf der
Plattendicke ab — auf 0,8 mm sitzt ein 1,6-mm-Teil nicht richtig. Bei den
anderen zwei ist die Dicke nicht genannt, also `ungeprueft`.

**Eine Ausschnittgröße kann ich nicht nennen.** Die Footprints sind über die
Brücke nicht auslesbar (`lib_footprint_get` gibt nur Kopfdaten), und ich würde
sonst eine Zahl erfinden. **Dafür braucht es das Datenblatt des gewählten
Teils.**

**Und darum sind es drei Wege, nicht zwei:**

1. **Gerät vorn dicker machen.** Schaltplan und Teile bleiben, wie sie sind.
   Kostet Bauhöhe, löst alles andere nicht aus.
2. **Mid-Mount-Buchse.** Teiletausch **plus Ausschnitt im Umriss** — und
   **vorher ist zu klären, ob es sie für 0,8 mm gibt.** Sonst müsste die
   Platine auf 1,6 mm, und das widerspricht dem, warum 0,8 mm gewählt wurden.
3. **Der Weg, für den die 0,8 mm überhaupt da sind:** Dieser Abschnitt sagt
   oben selbst, *„0,8 mm statt 1,6 mm ist keine freie Wahl, wenn der
   USB-C-Stecker später direkt am Platinenrand sitzen soll — alle gefundenen
   Platinenrand-Stecker passen nur auf 0,8 mm."* Das Gerät ist als
   **Schlüsselanhänger mit USB-C-Stecker** geplant, nicht mit Buchse; die
   Buchse ist die Stufe-1-Lösung. **Ein Stecker am Platinenrand baut über der
   Platte fast nichts auf** — genau der Fall, für den die Dicke festgelegt
   wurde. Dann löst sich der Hohlraum-Konflikt von selbst, und die 2,35 mm
   reichen.

> **Mein Vorschlag, aber Thomas' Entscheidung:** Weg 3 prüfen, bevor Weg 2
> gerechnet wird. Weg 2 kostet Teiletausch, Umrissausschnitt und
> möglicherweise die Plattendicke — Weg 3 kostet einen Teiletausch, der
> sowieso vorgesehen war. Was Weg 3 verlangt: **ENIG** (steht schon fest) und
> **Gold auf den Kontaktflächen am Rand**, sowie ein Umriss, der die
> Steckerzunge formt. Das gehört gerechnet, bevor jemand den Rand zeichnet.

## Weg 3 geprüft: er zerfällt in zwei, und einer davon ist unbelegt

Geprüft am 24.10.2026 gegen das, was im Repo steht und was von außen abrufbar
war. **Die Quelle meiner eigenen 0,8-mm-Behauptung ist `docs/parts.md`,
Abschnitt „Buchse oder Stecker"** — nicht die Erinnerung, aber auch nicht ein
Datenblatt.

### 3a — Stecker als Bauteil am Platinenrand

**Es gibt ein konkretes Teil, und es steht schon im Repo:**

| Teil | Art | Preis | Bezug |
| --- | --- | --- | --- |
| **Molex 1054440001** | **Stecker, Straddle Mount am Platinenrand** | 2,72 € | Digi-Key, **14 Wochen Lieferzeit** |

Das ist ein Bauteil, das den Platinenrand umgreift — kein geätzter Rand. Die
Platine steckt in ihm, nicht in der Buchse des Rechners.

- **Die 0,8 mm beziehen sich auf genau diese Bauart.** `docs/parts.md` nennt
  als belegte Randbedingungen „Leiterplattendicke 0,8 mm statt 1,6 mm (alle
  gefundenen Edge-Mount-Stecker passen nur auf 0,8 mm)" und „ENIG statt HASL
  (die dickere HASL-Schicht macht das erste Stecken schwer)".
- **`ungeprueft`:** Die Datei nennt **kein Datenblatt** und außer Molex keine
  Teilenummern. Die Zahl ist also auf ein Dokument im Repo zurückführbar, nicht
  auf eine Herstellerangabe. **Bauhöhe über der Platine, Ausschnitt und
  Lötbild sind nicht bekannt** — das muss aus dem Molex-Datenblatt kommen.
- **Die 14 Wochen Lieferzeit sind der eigentliche Preis dieses Weges**, nicht
  die 2,72 €.

### 3b — Zunge direkt in die Platine geätzt, ohne Bauteil

**Das ist der Weg, der nicht belegt ist, und `docs/parts.md` sagt es selbst:**

> *„Für einen direkt in die Platine geätzten Stecker ohne Bauteil wurde keine
> Quelle zur Konformität mit der Type-C-Steckerspezifikation gefunden —
> `ungeprueft`."*

Dazu kommt, was gegen ihn spricht und was man ohne Spezifikation sehen kann:
Ein USB-C-Stecker trägt **24 Kontakte auf beiden Seiten einer Zunge** und
braucht eine **Metallhülle mit Verriegelung**. Eine nackte Platinenkante
liefert die Hülle nicht. Bei USB-A geht das, weil dort eine einfache
rechteckige Hülle genügt; bei USB-C ist die Zunge Teil eines Steckers mit
eigener Mechanik. **Die Zungendicke der Spezifikation habe ich nicht belegen
können** — die Type-C-Spezifikation ist nur per Download zu bekommen (steht
schon in `schematic.md` beim CC-Widerstand).

### Was die Fertigung dazu sagt — auch nicht belegt

Abgerufen wurde JLCPCBs Fähigkeitsseite. **Sie nennt zu Randkontakten
nichts Verwertbares:**

- **Goldstärke für Randkontakte: nicht angegeben.**
- **Unterstützte Dicken für Randkontakte: nicht angegeben.**
- **Fase (Anschrägung der Zunge): nicht angegeben**, kein Winkel genannt.
- Belegt ist nur: **0,8 mm ist eine Standarddicke** („0.4/0.6/0.8/1.0/1.2/1.6/
  2.0 mm"), und metallisierte Ränder werden „copper-plated and ENIG treated".

**Das gehört beim Hersteller erfragt** — Kontakt zu Herstellern nimmt Thomas
selbst auf.

### Die Bilanz, beide Seiten

**Was wegfällt:**

- **Die Abschirmung um die Datenleitungen.** Die Buchse bringt einen Schirm
  mit, der auf `GND` liegt; am Platinenrand gibt es ihn nicht.
- **Die mechanische Entlastung.** Die vier Haltestifte des USB4085 nehmen die
  Zugkräfte auf. Beim Randstecker trägt die **Platine selbst** — 0,8 mm dünn,
  und sie wird bei jedem Stecken belastet.
- **20 000 Steckzyklen**, die der USB4085 verbürgt. Für Randkontakte ist keine
  Zahl bekannt.
- Die einfache Handlötbarkeit (Durchsteckteil).

**Was dazukommt:**

- **Der Hohlraumkonflikt verschwindet** — kein Körper, der 3,46 mm aufbaut.
- **Ein Bauteil weniger** auf der Platine.
- **Die Bauform, die der Plan von Anfang an nennt:** direkt in den Rechner
  stecken, ohne Kabel.
- Weniger Bauhöhe am vorderen Ende.

> **Mein Fazit, als Vorlage für Thomas' Entscheidung:** **3b ist nicht
> entscheidungsreif** — ohne Konformitätsquelle würde man eine Platine
> fertigen, von der niemand weiß, ob sie in eine USB-C-Buchse passt. **3a ist
> entscheidungsreif, kostet aber 14 Wochen Lieferzeit** und ist mechanisch ein
> Rückschritt gegenüber der Buchse (keine Zugentlastung, kein Schirm). Weg 1
> (Gerät vorn dicker) bleibt der einzige, der **heute** funktioniert und
> nichts Unbekanntes enthält.

### Was beim Hersteller zu erfragen ist — vergeblich gesucht

Nur nötig, wenn Weg 3a (Randstecker) verfolgt wird. **Herstellerkontakt macht
Thomas.** Hier steht, wonach er fragen müsste, weil ich es nicht gefunden habe:

**An JLCPCB (oder den gewählten Fertiger):**

1. **Bietet ihr Randkontakte („gold fingers") an, und für welche
   Plattendicken?** Die Fähigkeitsseite nennt 0,8 mm als Standarddicke, sagt
   aber nicht, ob Randkontakte darauf möglich sind.
2. **Welche Goldstärke** haben diese Kontakte (in Mikrozoll), und ist es
   **Hartgold** oder ENIG? Die Seite nennt zu Randkontakten keine Zahl; zu
   metallisierten Rändern steht nur „copper-plated and ENIG treated".
3. **Wird eine Fase (Anschrägung) an der Steckkante angeboten, und mit welchem
   Winkel?** Nicht erwähnt.
4. **Gibt es eine Angabe zu Steckzyklen** für solche Kontakte? Zum Vergleich:
   die Buchse GCT USB4085 verbürgt 20 000.

**An Molex, zum Teil 1054440001:**

5. **Bauhöhe über und unter der Platine** bei 0,8 mm Plattendicke — das ist die
   Zahl, die über den 2,35-mm-Hohlraum entscheidet.
6. **Lötbild und nötiger Ausschnitt** am Platinenrand.
7. **Lieferzeit** — Digi-Key nennt 14 Wochen; gibt es einen kürzeren Weg?

Was ich abgerufen habe und was dabei herauskam, steht im Abschnitt darüber.
**GCTs eigene Seite und TDKs Produktseite antworten auf Abruf mit HTTP 403** —
beide Hersteller sind von hier aus nicht lesbar. Das ist kein Hinweis auf die
Teile, nur auf den Weg dorthin.

## Außermittige Zelle: entschieden, gerechnet — und ein Fund dabei

**Entschieden am 24.10.2026** (Platinen- und Gehäuse-Strang, vom Master
getragen): Die Zelle rückt an die Kante, damit U2 in den Streifen daneben
passt und die I2C-Leitung **12 statt 25 mm** lang wird.

### Die Kante liegt bei 1,20, nicht bei 0,75

Mein erster Vorschlag (Zelle ab `x` 0,75) lag **0,45 mm im Klemmkragen** des
Gehäuses. Nachgemessen vom Gehäuse-Strang; der bauteilfreie Bereich quer ist
**1,20 … 24,80 mm**, darunter liegt die Auflage, auf der die Platine geklemmt
wird.

| | quer |
| --- | --- |
| **Zelle** | **1,20 … 20,95** (19,75 breit), Mitte `x` = 11,075 |
| **Streifen daneben** | **20,95 … 24,80 = 3,85 mm** |
| U2, 3,0 mm breit | **0,85 mm Luft** — reicht |

### Die Ferritfolie: geprüft, sie geht — aber sie verliert ihre Querreserve

**Die Auflage lautet** (Abschnitt „Akku gegen NFC-Antenne"): die Folie
überdeckt die Zellfläche **und** reicht in Richtung Antenne über.

- **Richtung Antenne heißt in der Länge**, nicht quer: die Antenne liegt in Z4
  (`y` 51 … 70), die Zelle endet bei `y` 51. **Die Zelle rückt nur quer, ihre
  Lage in `y` ändert sich nicht** — der Überstand zur Antenne ist also
  unberührt. **Die Auflage ist erfüllbar.**
- **Quer ist die Folie jetzt eingeklemmt:** nach außen endet der bauteilfreie
  Bereich bei 1,20 (dahinter der Klemmkragen), nach innen sitzt ab 20,95 der
  Streifen mit U2. **Die Folie muss also auf die Zellbreite geschnitten werden,
  19,75 mm, ohne Überstand nach den Seiten.** Vorher lag sie mittig mit
  3,13 mm Luft auf jeder Seite.
- **Das ist kein schlechter Tausch, aber eine Fertigungsauflage:** Eine von
  Hand zugeschnittene Folie, 0,45 mm zu breit aufgelegt, liegt entweder im
  Kragen oder auf U2 — und dann kippt die Zelle. **Gehört in die Bauanleitung,
  nicht nur hierher.**

> **`ungeprueft` bleibt:** welche Folie, welche Dicke, welche Permeabilität bei
> 13,56 MHz. Das steht schon als offener Punkt oben.

### Der Fund: `y` 44 … 51 liegt unter der Zelle

**U2 und U3 standen bei `y` 47,5 — das ist unter der Zelle.** Die Zuweisung
„`y` 44 … 51, dort ist die Zelle zu Ende" stammt aus der Zeit, als der Akku
noch bei 44 mm enden sollte. **Seit die Zelle als 25 … 51 gerechnet ist, ist
dieser Platz Zellfeld** — und die Regel „Fläche darunter frei von Bauteilen
halten" war damit verletzt. Keinem von uns beiden ist es aufgefallen, mir
eingeschlossen, obwohl ich die 25 … 51 selbst ausgerechnet habe.

**Aufgelöst:**

- **U2 in den Streifen** neben der Zelle (`x` 22,9, `y` 33) — das war ohnehin
  der Sinn der Verschiebung, und es löst den Fehler mit.
- **U3 unter das Modul** (`x` 17, `y` 19,5, Unterseite, Körper `x` 13 … 21,
  `y` 16,5 … 22,5). In den 3,85-mm-Streifen passt der Flash nicht, er braucht
  6 mm. Unter dem Modul ist Platz, das Zellfeld beginnt erst bei `y` 25, und
  die **sechs QSPI-Leitungen werden dadurch kurz** — bei bis zu 32 MHz Takt
  ist das ein Gewinn, nicht nur ein Ausweichen. Der Freihaltestreifen der
  Antenne (`x` 24,5 … 26) ist nicht berührt.
- **Unter der Zelle steht damit nichts mehr.**

> **Zu bestätigen:** Die Verlegung von U3 weicht von der Zuweisung „U2 und U3
> nach `y` 44 … 51" ab. Sie muss abweichen, weil die Zuweisung auf der alten
> Zellenlänge beruhte. Wenn jemand einen besseren Platz sieht, jetzt sagen.

### Zwei Zahlen aus der Gehäuseantwort, hier festgehalten

- **Die Achse ist bestätigt: die Stufe-2-Vorgaben sind quer gemeint.** Meine
  Vermutung war richtig.
- **Die Reserve der Stufe-2-Aufteilung ist 0,10 mm, nicht 0,60.** Das
  Gehäusedokument enthielt zwei widersprüchliche Zahlen; die Summenzeile hatte
  den halben Millimeter Trennabstand nicht mitgerechnet. **Bei 24 mm Breite
  hätten 1,90 mm gefehlt, nicht 1,40.**

> ### Der Umriss wartet: 27 mm statt 26 steht zur Entscheidung
>
> Weil 0,10 mm keine Reserve sind und die Breitentoleranz einer von Hand
> eingeklebten Zelle niemand kennt, empfiehlt der Gehäuse-Strang **27 mm**,
> falls Stufe 2 den Sensor wirklich bekommt. **Thomas entscheidet.**
>
> **Bis dahin wird der Umriss nicht gezeichnet** — 26 oder 27 trifft dieselbe
> Kante wie alles andere heute. Die `x`-Werte in diesem Entwurf gelten für
> **26 mm**; bei 27 mm wandern die mittenbezogenen Teile um 0,5 mm und der
> Freihaltestreifen auf `x` 25,5 … 27.

# Platzierung für 27 mm und Stufe 2 — gerechnet 24.10.2026

**Diese Fassung gilt.** Die Tabellen weiter oben haben `x`-Werte für **26 mm**
und sind damit überholt; die Bänder in `y` ändern sich hier ebenfalls, weil die
Zelle gewechselt ist. Entschieden von Thomas: **Platine 27 mm breit**,
**Buchse bleibt** (USB4085, Gerät vorn gut einen Millimeter dicker),
**Sensor der Stufe 2 von Anfang an eingeplant**.

## Die Bänder, nachgerechnet statt überschlagen

| Band | `y` | Länge | Inhalt |
| --- | --- | --- | --- |
| Z1 | 0 … 9,5 | 9,5 | USB-C, Ladepfad |
| Z2 | 9,5 … 25,0 | 15,5 | Funkmodul, Entkopplung, Quarz, **und was aus Z3 weichen musste** |
| Z3 | 25,0 … **55,5** | **30,5** | Zelle **und** Sensorfeld, nebeneinander |
| Z4 | **55,5** … 70 | **14,50** | NFC-Schleife |

9,5 + 15,5 + 30,5 + 14,50 = **70,00 mm.**

**Die 14,5 mm stimmen** — und sie waren vorhin tatsächlich nur ein Überschlag.
Nachgerechnet: die LP-401230-J ist **30,5 mm lang**, Z3 beginnt bei 25,0, endet
also bei 55,5; 70 − 55,5 = **14,50**. Zum Vergleich: mit der Adafruit 1317
wären es 18,98 mm gewesen.

**Was das die Antenne kostet, auf gleicher Grundlage gerechnet** (nutzbare
Breite, nicht Plattenbreite — der Klemmkragen nimmt 1,2 mm je Seite):

| | Schleifenfläche |
| --- | --- |
| Stufe 1 bei 26 mm, Zelle 26,02 lang | 19,0 × 23,6 = **448 mm²** |
| **jetzt: 27 mm, Zelle 30,5 lang** | 14,5 × 24,6 = **357 mm²** |

**Rund ein Fünftel weniger Fläche.** Meine 494 mm² von vorhin waren mit der
voll­en Plattenbreite gerechnet — die steht nicht zur Verfügung. Thomas kennt
den Preis und hat ihn angenommen.

> **Diese Zahl bleibt hier und kommt nicht ins Handbuch** (entschieden
> 24.10.2026). Dem Nutzer sagt eine Flächenangabe nichts; er braucht die
> Folge, und die steht im Handbuch schon: näher halten als eine Karte, und
> keine Zahl, bis gemessen wurde. **Eine Quadratmillimeterzahl daneben würde
> Genauigkeit vortäuschen, wo die Reichweite gar nicht bekannt ist.** Die
> 357 mm² gehören hierher — und an die Stelle, wo später die gemessene
> Reichweite eingetragen wird.

## Quer: Zelle und Sensorfeld nebeneinander

Nutzbar quer ist **1,20 … 25,80 = 24,60 mm** (Klemmkragen 1,2 mm je Seite).

| von | bis | Breite | was |
| --- | --- | --- | --- |
| 1,20 | 13,20 | 12,00 | **Zelle LP-401230-J** |
| 13,20 | 13,70 | 0,50 | Trennabstand |
| 13,70 | 24,70 | 11,00 | **Sensorfeld, Stufe 2 — bleibt leer** |
| 24,70 | 25,80 | **1,10** | Reserve |

23,50 belegt von 24,60 → **1,10 mm Reserve** statt der 0,10 mm bei 26 mm. Das
war der Grund für die 27 mm, und er geht auf.

> **Das Sensorfeld wird in Stufe 1 freigehalten, nicht bestückt.** Der Sensor
> ist nicht einmal ausgewählt. **Wer das Feld zubaut, nimmt Thomas'
> Entscheidung zurück.**

## Die Folge, die man leicht übersieht: die Unterseite von Z3 ist vergeben

Zelle und Sensorfeld belegen quer **1,20 … 24,70** über die **ganze Länge von
Z3**. Übrig bleibt ein Streifen von **1,10 mm** — zu schmal für jedes Bauteil.

**Damit ist die Unterseite von Z3 vollständig vergeben.** Alles, was dort
stand, muss weg:

| Teil | stand | muss | warum |
| --- | --- | --- | --- |
| U2 Sicherheitschip | `x` 22,9 / `y` 33 | **nach Z2** | Streifen daneben gibt es nicht mehr |
| U3 Flash | war schon in Z2 | bleibt in Z2 | — |
| C10, C11, R6, R7 | im Streifen | **nach Z2** | mit U2 |
| **J2 Akkustecker** | `x` 13 / `y` 49,5 | **neu zu setzen** | liegt mitten im Zellfeld |

**U2 kommt damit von sich aus nah ans Funkmodul** — das war das Ziel der
außermittigen Zelle, und es ergibt sich jetzt ohne sie. **Die außermittige
Zelle ist damit erledigt:** sie war die Antwort auf „neben der 19,75 mm
breiten Zelle passt nichts". Mit 12 mm Zelle und einem reservierten Sensorfeld
daneben gibt es diesen Streifen nicht mehr, und U2 sitzt ohnehin in Z2.

## Vorläufige Plätze, und was dafür noch fehlt

**Unterseite Z2** (`y` 9,5 … 25, quer 1,20 … 25,80, abzüglich Freihaltezone)
muss jetzt tragen: U2 (3 × 3), U3 (8 × 6), C10, C11, C12, R6, R7, R8, dazu was
schon dort war — L1, X1, C8, C9, C3, C4, C5, Q2, Q3, R4, R5.

**Flächenprobe:** Z2-Unterseite ist 24,6 × 15,5 = **381 mm²**; die Körper
belegen zusammen rund **100 mm²**. **Es passt von der Fläche**, aber nicht
beliebig.

| Teil | Seite | `x` | `y` | Stand |
| --- | --- | --- | --- | --- |
| U1 | oben | 17,75 | 17,3 | fest; Körper `x` 10,0 … 25,5, Antennenkante **25,5** |
| U3 Flash | unten | 18 | 19,5 | **vorläufig** — kurze QSPI-Wege bleiben das Ziel |
| U2 | unten | 23 | 17 | **vorläufig** — unter dem Modul, kurze I2C |
| J1 | oben | **13,5** | **2,685** | quer mittig. Längs aus `d` = 1,90 gerechnet: `y` = 4,585 − 1,90. **Der Körper ragt damit 1,90 mm über die Platinenkante** — Pinreihen prüfen (siehe unten). Auflage ± 0,3 mm |
| J4, DS1 (Glas) | oben | **13,5** | 27 / 38 | mittig |
| D1 LED | oben | **13,5** | 66 | am Kopfende; **liegt jetzt in Z4** |
| SW1 / SW2 | oben | 3,5 / **23,5** | 24,5 | an den Längskanten |
| C15, C16 | unten | 10,5 / 16,5 | 56,5 | symmetrisch zur Mitte, in Z4 |
| J2 Akkustecker | **offen** | — | — | **siehe unten** |

> **Was fehlt, um die vorläufigen Werte festzumachen:** die **Footprints**. Ich
> habe bisher mit Gehäusemaßen gerechnet; für Abstände von einem Millimeter
> brauche ich die Pad-Umrisse, und die sind über die Brücke nicht auslesbar.
> **Erst nach `pcb_import_changes` sind U2, U3 und die Passiven in Z2
> kollisionsfrei zu setzen** — vorher wäre jede Zahl auf 0,1 mm erfunden.

## Drei offene Punkte, die aus dieser Umstellung kommen

1. **Auf welcher Seite liegt das Sensorfeld?** Die Vorgabe nennt nur
   Querwerte. Ein Fingerabdrucksensor wird berührt, muss also nach außen
   zeigen. **Liegt das Feld unten, ist die Unterseite von Z3 vergeben (so
   gerechnet); liegt es oben, kollidiert es mit dem Anzeigeglas**
   (`x` 6,55 … 20,45, `y` 27 … 49). Die zweite Lesart geht nicht auf —
   **ich rechne mit „unten" und bitte um Bestätigung.**
2. **Wohin kommt J2, der Akkustecker?** In Z3 ist kein Platz mehr. Möglich
   wären: oben neben dem Glas (`x` ≈ 23, `y` ≈ 52), oder in das Band
   `y` 22,6 … 25 zwischen Modul und Zellfeld — dort sind aber nur 2,4 mm, und
   der JST-PH ist größer. **Das hängt davon ab, wo die Zelle ihr Kabel
   herausführt** und gehört mit dem Gehäuse-Strang entschieden.
3. **Wie lang ist das Sensorfeld?** Angegeben ist nur die Breite (11 mm). Ich
   reserviere **die ganze Länge von Z3**, also 11 × 30,5 = **336 mm²** — das
   ist die sichere Lesart. Wenn der Sensor kürzer ist, wird Platz frei, den
   Stufe 1 nutzen könnte. **Solange der Sensor nicht ausgewählt ist, bleibt es
   reserviert.**

## Ferritfolie und Freihaltezone, mitgezogen

**Ferritfolie:** Die Zelle ist jetzt **12 mm breit** und liegt an
`x` 1,20 … 13,20.

- **Richtung Antenne (in `y`) unverändert erfüllbar** — die Zelle endet bei
  `y` 55,5, die Schleife beginnt dort.
- **Quer hat die Folie 0,5 mm Luft nach innen** (der Trennabstand bis 13,70)
  und **keine nach außen** (Klemmkragen ab 1,20). Sie wird also **12 mm breit
  geschnitten, höchstens 12,5**. Besser als bei der breiten Zelle, wo quer gar
  keine Reserve blieb — aber immer noch eine Fertigungsauflage für die
  Bauanleitung.
- **Neu zu bedenken:** Die Folie darf **nicht unter das Sensorfeld** reichen.
  Was ein Fingerabdrucksensor von einer Ferritlage unter sich hält, weiß ich
  nicht — `ungeprueft`, aber der Abstand kostet nichts.

> **Wichtig gegen ein Missverständnis:** Dass die *Entscheidung* „außermittige
> Zelle" hinfällig ist, gibt der Folie **keine Querreserve zurück**. Die Zelle
> liegt weiterhin an der Kante — nicht mehr, weil wir sie dorthin gerückt
> haben, sondern weil **das Sensorfeld die andere Seite belegt**. Nach außen
> beginnt bei 1,20 der Klemmkragen, nach innen bei 13,70 das Sensorfeld.
> **Die Auflage „nicht breiter" bleibt also, nur ihr Grund hat sich geändert.**

**Fürs Handbuch, in einem Satz:** **Ferritfolie 12,0 mm breit, bündig an die
Außenkante der Zelle, höchstens 12,5 mm** — breiter reicht sie unter das
Sensorfeld.

**Die Länge der Folie kann ich nicht als Zahl angeben.** Sie soll Richtung
Antenne überstehen, und die Antennenschleife beginnt jetzt unmittelbar dort,
wo die Zelle endet (`y` 55,5). Wie weit die Folie unter die Schleife reichen
darf, **ist Teil der Abstimmungsmessung** — Ferrit unter einer NFC-Schleife ist
üblich und nötig, verschiebt aber die Resonanz. **Daraus folgt eine harte
Bedingung: die Folie muss beim Abstimmen schon liegen**, sonst gilt die
Messung nicht. Sie steht bei den Bedingungen für die NFC-Abstimmung.

**Freihaltezone des Moduls:** Antennenkante **25,5**, Plattenrand **27**. Von
den 3,8 mm Tiefe liegen wieder nur **1,5 mm auf der Platine**:

> **`x` 25,5 … 27, `y` 12,0 … 22,6 — kupferfrei in allen vier Lagen.**

**Prüfzahl für den Umriss: 194 mm** in vier Segmenten (2 × 27 + 2 × 70).

## U3 unter dem Modul: nachgerechnet, und ich nehme es zurück

Der Einwand war berechtigt. Raytacs Layout-Abschnitt verlangt unter dem Modul
eine **durchgehende Massefläche**, in der Zeichnung auf Seite 14 ausdrücklich
auch auf der Unterseite („Bottom layer: Ground Area"), dazu *„Please add via
holes in GROUND area as many as possible, especially around the four
corners."*

**Die Zahlen, nicht das Gefühl.** Modulschatten `x` 10,0 … 25,5 × `y` 12,0 …
22,6 = **164,30 mm²**. U3 mit 8 × 6 mm an `x` 18 / `y` 19,5, plus 0,25 mm
Abstandsring zur Fläche:

| | mm² | Anteil |
| --- | --- | --- |
| Modulschatten | 164,30 | 100 % |
| U3 mit Abstandsring (8,5 × 6,5) | **55,25** | **33,6 %** |
| **Massefläche bleibt** | **109,05** | **66,4 %** |

**Ein Drittel der Fläche wäre weg**, die der Hersteller als durchgehend
verlangt.

**Und die Eckvias?** Links bleiben 3,75 mm, rechts 3,25 mm, unten 4,25 mm —
dort passen Durchkontaktierungen. **Oben nicht:** der Abstandsring reicht bis
`y` 22,75, die Modulkante liegt bei 22,6. **Rechnerisch −0,15 mm, also kein
Platz, sondern Überstand.** Die Oberkante des Modulschattens wäre nicht nur
viafrei, U3 ragte dort hinaus.

**Zur eigentlichen Frage — reicht eine durchgehende Fläche auf `INNER_1`,
wenn `BOTTOM` unterbrochen ist? `ungeprueft`.** Raytacs Zeichnung zeigt eine
**zweilagige** Platine; eine Aussage für vier Lagen steht dort nicht, und ich
habe keine gefunden. **Ich lege das nicht zu meinen Gunsten aus.**

> **Damit nehme ich U3 unter dem Modul zurück.** Es gilt die Hausregel: bei
> zwei Wegen und unbelegter Lage die sicherere Variante. Ein langsamerer Flash
> ist ein Ärgernis, schlechter Funk macht das halbe Gerät wertlos.

**Neuer Platz: `x` 5,45 / `y` 18,0, Unterseite** — im Streifen **links neben
dem Modulschatten** (`x` 1,20 … 10,0, 8,80 mm breit × 15,5 lang). U3 mit
Abstandsring braucht 8,5 mm, es bleiben **0,30 mm** übrig. Eng, aber es passt,
und die Massefläche unter dem Modul bleibt **ganz**.

**Was das kostet:** Die sechs QSPI-Leitungen werden einige Millimeter länger,
weil sie am Modulschatten vorbei müssen. **Immer noch ein Bruchteil der 25 mm**,
die es aus Z3 geworden wären. Die genaue Länge ergibt sich erst, wenn die
Pad-Lagen des Moduls bekannt sind.

**Verdrängt werden dadurch** L1, C3, C4, C5, X1, C8, C9 aus genau diesem
Streifen. Alle sieben sind klein (0402/0603 und ein 3,2 × 1,5-Quarz), und in
Z2 bleiben rund 280 mm² frei — **aber ihre neuen Plätze setze ich erst mit den
Footprints**, nicht vorher.

## Zwei Befunde vom Umriss, die in die Notizen gehören

**1. Der Umriss ist ein POLY, keine Linie — und meine Prüfzahl war die
falsche.** Thomas hat gezeichnet, aus dem Quelltext gelesen steht dort

```
{"type":"POLY","polyType":"BOARD_OUTLINE","layerId":11,
 "path":["R",0,2755.9055,1062.9921,2755.9055,0,0]}
```

In mil, umgerechnet **27,0000 × 70,0000 mm**, Umfang **194,0 mm** — genau die
Prüfzahl, und das `R` heißt Rechteck, also geschlossen.

> **Die Lehre, und sie korrigiert auch meine eigene Beweisführung weiter oben:**
> `pcb_lines_list` auf `BOARD_OUTLINE` meldet **count 0, obwohl der Umriss da
> ist** — er ist ein POLY, kein LINE. Ich hatte diese Null oben als Belegstück
> aufgeführt („nachgezählt: 0 Segmente"). **Als Beweis, dass nichts angelegt
> wurde, war sie nie tauglich**; der eigentliche Beleg ist die Fehlermeldung
> der Schnittstelle. **Geprüft wird ein Umriss mit `doc_export_source`, nicht
> mit `pcb_lines_list`.**

**2. Der Regelsatz steht:** „JLCPCB Capability(**Multiple Layers Board**)",
ausgelesen, nicht mehr „Two Layers".

**Damit sind die Vorbedingungen für `pcb_import_changes` erfüllt:** Rand da,
Regelsatz passend, vier Kupferlagen gesetzt.

## Widerspruch, der vor dem Platzieren weg muss: mittige Zelle gegen Sensorfeld

Vom Gehäuse-Strang kamen am 24.10.2026 zwei Bestätigungen, die sich
gegenseitig ausschließen:

1. **„Das Sensorfeld liegt unten, neben der Zelle"** — quer: Zelle 12,0 +
   Trennabstand 0,5 + Sensorfeld 11,0 = 23,50 von 24,60 nutzbar.
2. **„Die Zelle bleibt mittig."**

**Beides geht nicht.** Mittig gelegt (Zelle `x` 6,80 … 18,80) bleiben **je
Seite 6,30 mm**. Das Sensorfeld braucht mit Trennabstand **11,50 mm.** Es passt
auf **keiner** der beiden Seiten.

| Lage der Zelle | Platz links | Platz rechts | Sensorfeld (11,5 nötig) |
| --- | --- | --- | --- |
| mittig | 6,30 | 6,30 | **passt nicht** |
| an der Kante (`x` 1,20 … 13,20) | 0 | **11,50** | **passt genau** |

**Die Zelle liegt also an der Kante — nicht mehr als Kunstgriff, sondern weil
das Sensorfeld die andere Seite braucht.** Der Satz „der Versatz war die
Antwort auf ein Platzproblem, das die 12-mm-Zelle nicht mehr hat" gilt für den
**alten** Grund (U2 neben der Zelle); der **neue** Grund ist das Sensorfeld und
er wiegt schwerer.

> **Zu entscheiden, und nicht von mir:** Entweder die Zelle liegt an der Kante
> und Stufe 2 bleibt möglich — so ist dieser Entwurf gerechnet — **oder** die
> Zelle liegt mittig und **das Sensorfeld entfällt**, womit Thomas' Entscheidung
> von heute zurückgenommen wäre. **Ich zeichne nach der ersten Lesart** und
> warte auf Widerspruch.

## Die Zelle der Stufe 2 ist dicker, nicht nur schmaler

**LP-401230-J: 4,05 mm dick**, Adafruit 1317: 3,80 mm. **Das Gerät wächst um
0,25 mm**, obwohl die Zelle schmaler ist.

**Damit sind die Höhenangaben weiter oben überholt**, soweit sie mit 3,8 mm
rechnen (Maßtabelle, „das dickste Teil", die Höhenaddition, der
Sensorvergleich). Richtig ist jetzt: **dickstes Teil ist die Zelle mit
4,05 mm**; grobe Addition 0,8 Platine + 4,05 Zelle + 2,05 Modul ≈ **6,90 mm**
statt 6,65, plus Ferritfolie und Gehäuse.

**Wer die schmale Zelle für die rundum kleinere hält, rechnet falsch.**

## USB-Buchse bündig zur Gehäusefront: was ich dafür brauche

Die Auflage ist **± 0,3 mm** auf die Vorderkante der Buchse — nach dem
Spritzen nicht mehr zu korrigieren. Der Steckerkörper ist **9,17 mm tief**.

**Die Rechnung ist einfach, es fehlt nur eine Zahl:** Sei `d` der Abstand, um
den die **Gehäusefront vor der Platinenkante `y` = 0** liegt. Dann muss die
Vorderfläche der Buchse bei `y` = −`d` sitzen, also

> **J1-Mitte `y` = 4,585 − `d`**

| `d` | J1-Mitte `y` | Bedeutung |
| --- | --- | --- |
| 0,00 | **4,585** | Gehäusefront liegt auf der Platinenkante |
| 1,03 | 3,555 | Front 1,03 mm davor (Differenz Nase 10,20 − Körper 9,17) |

**Mein Entwurf hat J1 bei `y` = 4,6** — das entspricht `d` = −0,015, die
Buchse stünde also 0,015 mm hinter der Platinenkante. **Innerhalb der ± 0,3 mm,
aber nur, wenn `d` = 0 ist.**

> **Frage an den Gehäuse-Strang, eine Zahl:** **Wie viele Millimeter liegt die
> Gehäusefront vor der Platinenkante `y` = 0?** Danach setze ich J1 auf
> 4,585 − `d` und nicht auf einen Schätzwert. Bei einer Durchsteckbuchse kann
> der Körper über die Platinenkante hinausragen, die Pins aber nicht — wenn
> `d` groß wird, ist das eine eigene Prüfung.

# Sensor gehört nicht auf die Platine — was dadurch wieder möglich wird

Gerechnet 24.10.2026, nachdem der Gehäuse-Strang gemessen hat, dass zwischen
Platinenunterseite und Finger **4,82 mm** liegen und der Hohlraum nicht flacher
geht (0,2 Luft + 0,4 Folie + 4,05 Zelle = 4,65 mm). Ein kapazitiver Sensor
liest durch Bruchteile eines Millimeters, nicht durch fünf.

**Unsere eigene Doku sagt es ebenfalls:** `docs/fingerprint.md` führt den
FPC2534AM als **Flex-Modul**, „Sensor auf Flex, **12-poliger ZIF-Anschluss**",
und schreibt an anderer Stelle: „Der Flex-Anschluss (12-polig, ZIF) braucht
einen Steckverbinder". Er ist zum Einkleben gedacht, nicht zum Löten.

**Die Platine braucht davon: einen ZIF-Steckverbinder und einen Weg fürs
Flachband. Kein 11-mm-Feld.**

## 1. Ja, die große Zelle kann zurück — und das Antennenfeld gewinnt deutlich

| | Zelle | Z3 endet | **Antennenfeld Z4** | Schleifenfläche |
| --- | --- | --- | --- | --- |
| jetzt | LP-401230-J, 30,5 lang, 4,05 dick | 55,50 | **14,50 mm** | 357 mm² |
| **mit Adafruit 1317** | 26,02 lang, **3,80 dick** | 51,02 | **18,98 mm** | **467 mm²** |

**+4,48 mm Antennenfeld und +31 % Schleifenfläche** — und das Gerät wird
**0,25 mm dünner**, weil die Adafruit die flachere Zelle ist.

> **Damit war das Opfer tatsächlich unnötig.** Thomas hat 4,5 mm Antennenfeld
> und 0,25 mm Bauhöhe für ein Sensorfeld gegeben, das der Sensor nie gebraucht
> hätte. **Das ist die Zahl, auf die es ankam.**

## 2. Ja, die Zelle darf wieder mittig

Ohne Sensorfeld zwingt nichts sie an die Kante. Quer bleibt dann je Seite:

| Platinenbreite | nutzbar | je Seite neben der Adafruit |
| --- | --- | --- |
| 24 mm | 21,60 | 0,925 mm |
| 26 mm | 23,60 | 1,925 mm |
| **27 mm** | 24,60 | **2,425 mm** |

**Und die Ferritfolie bekommt ihre Toleranz zurück:** 19,75 mm breit mit rund
2,4 mm Luft je Seite statt „12,0 mm, nicht breiter".

> **Darum jetzt keine Folienzahl ans Handbuch.** Die 12,0 mm wären die dritte
> Zahl in einem Tag. **Erst wenn die Zelle entschieden ist**, geht eine Zahl
> raus — dann 19,75 mm mit Toleranz, oder 12,0 mm ohne.

## 3. Braucht die Platine die 27 mm noch? Für Bauteile nein

**Ehrliche Antwort: die gewonnene Breite braucht kein Bauteil.** U2 und U3
liegen beide in Z2 (siehe unten), J2 passt in keinen Seitenstreifen, und der
Streifen neben der Zelle bleibt leer.

**Luft ist sie aber nicht:** Sie ist **Toleranz für zwei von Hand eingelegte
Teile** — die LiPo-Zelle (Breitentoleranz nennt niemand) und die zugeschnittene
Ferritfolie. Bei 24 mm sind es 0,925 mm je Seite; bei einem handgebauten Gerät
ist das knapp.

> **Mein Rat, Thomas entscheidet:** **27 mm behalten.** Der Umriss ist mit
> 27,0000 × 70,0000 gezeichnet und geprüft; ihn zu ändern kostet einen weiteren
> Handgriff und bringt 3 mm Gehäusebreite. Der Gegenwert — 2,4 mm Toleranz je
> Seite statt 0,9 — ist mehr wert als 3 mm Gehäuse. **26 mm wäre das vernünftige
> Minimum, 24 mm möglich aber knapp.**

## 4. U2 kommt ebenfalls unter dem Modul heraus — nach meiner eigenen Regel

Im Entwurf stand U2 bei `x` 23 / `y` 17, also **unter dem Modulschatten**. Mit
0,25-mm-Ring wären das **12,25 mm² = 7,5 %** der Massefläche, die der
Hersteller als durchgehend verlangt. Weniger als U3s 33,6 %, **aber derselbe
unbelegte Vier-Lagen-Fall** — und wer bei 33,6 % die sicherere Variante nimmt,
kann bei 7,5 % nicht anders entscheiden, nur weil es weniger weh tut.

**U2 geht in den linken Streifen** (`x` 1,20 … 10,0), wo U3 schon liegt: U3
belegt 6,5 der 15,5 mm Länge, es bleiben **9,0 mm** — U2 braucht mit Ring 3,5.
Beide passen, die Massefläche bleibt ganz.

**Vorläufig: U3 bei `x` 5,45 / `y` 18,0, U2 bei `x` 5,45 / `y` 11,5.** Die
I2C-Leitung wird dadurch länger als unter dem Modul; sie läuft weiterhin auf
einer Innenlage.

## 5. Zwei Dinge, die dieser Befund neu aufmacht

**a) J5 ist im Schaltplan das falsche Bauteil.** Dort steht ein **Stiftfeld
1 × 6** mit VDD, GND, SCK, MOSI, MISO, CS. Der FPC2534AM hat einen
**12-poligen ZIF-Anschluss**. **Das ist keine Layoutfrage, das ist der
Schaltplan:** es braucht einen 12-poligen FPC-Steckverbinder mit der
**richtigen Pinbelegung des Sensors** — und die hat bisher niemand
nachgelesen. Solange steht `ungeprueft` darüber.

**b) Die Bauhöhe von J2 fehlt im Repo.** Der Zellenraum ist randvoll: jedes
Bauteil neben der Zelle, das **höher als 4,45 mm** baut, macht das Gerät
dicker. Beim liegenden JST-PH ist das nicht ausgeschlossen. **J2 bleibt
unplatziert**, bis die Zahl da ist.

**Nichts davon ist zurückgebaut.** Der Entwurf steht weiter auf 27 mm und der
schmalen Zelle, bis entschieden ist — Umriss und Regelsatz sind geprüft, und
beides noch einmal anzufassen muss sich lohnen.

## Zwei Zahlen vom Gehäuse nachgezogen, 24.10.2026

### `d` = 1,90 mm: die Buchse wandert 1,91 mm nach hinten

Mit Herkunft, nicht als nackte Zahl:

| Bezug | längs |
| --- | --- |
| Platinenkante | 0,00 |
| Innenseite der Frontwand (Spiel 0,30) | −0,30 |
| Außenseite der Frontwand (Wand 1,60) | **−1,90** |

**`d` = 1,90** → **J1-Mitte `y` = 9,17/2 − 1,90 = 2,685**. Mein Entwurf stand
auf 4,60, das entsprach `d` = 0 — **einem Gehäuse ohne Frontwand.** Nachgezogen
ist nur diese eine Zeile; weil sie als `vorlaeufig` mit der Formel dastand, hat
niemand mit der 4,60 weitergerechnet.

**Daraus eine echte Prüfung, keine Vorsorge: der Körper ragt 1,90 mm über die
Platinenkante.** Die Pinreihen müssen weiter hinten liegen als 1,90 mm, sonst
sitzen Lötpads im Nichts. 1,90 von 9,17 mm Körpertiefe ist für eine randnahe
Buchse üblich — **„üblich" ist kein Beleg.** `ungeprueft`, **und mit den
Lesewerkzeugen der Brücke nicht zu klären**: Footprints geben nur Kopfdaten
her. **Die Prüfung geht erst nach `pcb_import_changes` am gesetzten Footprint**
(`pcb_pads_list`), und sie gehört zu den ersten Dingen danach.

> **Vorbereiteter Ausweg, falls sie durchfällt:** eine flache Mulde um die
> Öffnung, 0,8 mm tief → `d` = 1,10 und J1-Mitte **3,485**. Der Gehäuse-Strang
> hat sie **nicht** eingebaut, damit hier kein wandernder Wert steht; ein Wort
> genügt. **Nicht einrechnen, bis sie bestellt ist.**

### Bauteile beginnen quer bei 1,40, nicht bei 1,20

Die Hohlraumwand steht bei 1,20; gedruckte Flächen haben 0,1 … 0,2 mm
Toleranz, eine Zelle genau an 1,20 berührt sie.

**Angegeben wurde nur die eine Seite.** Ich rechne **symmetrisch** weiter, das
ist die sicherere Lesart:

| | Band | nutzbar |
| --- | --- | --- |
| bisher | 1,20 … 25,80 | 24,60 |
| **jetzt** | **1,40 … 25,60** | **24,20** |

> **Zu bestätigen:** Gilt die 0,20 auch am anderen Rand (dann 25,60) oder nur
> vorn? Der Unterschied sind 0,20 mm.

**Es kostet nichts, beide Fälle tragen:**

| Fall | belegt | nutzbar 24,20 | Rest |
| --- | --- | --- | --- |
| Stufe 2 quer (Zelle 12,0 + 0,5 + Sensorfeld 11,0) | 23,50 | ja | **0,70 mm** |
| Adafruit mittig (19,75) | 19,75 | ja | **4,45 mm**, je Seite 2,225 |

**Das Band ist symmetrisch um die Plattenmitte** — (1,40 + 25,60)/2 = **13,50**.
Eine mittige Zelle sitzt also auf `x` 13,50 und spannt 3,625 … 23,375.

## J2: gemessen, und das Ergebnis ist ein anderes Problem als erwartet

**Unser Teil hat keine Bauhöhe, weil es kein Teil ist.** Gemessen am
24.10.2026 auf drei Wegen, alle erfolglos:

| Weg | Ergebnis |
| --- | --- |
| `lib_device_by_lcsc` auf **C9900141337** | **kein Hersteller, keine Herstellernummer, kein Datenblatt.** Als „Manufacturer Part" steht die Beschreibung selbst: „JST Right-Angle Connector - SMD 2-Pin (Black)" |
| LCSC-Produktseite zu C9900141337 | **„Part not found"** — die Seite existiert nicht |
| `lib_3dmodel_get` auf das 3D-Modell | nur Name und Klassifikation, **keine Maße** |

**Die `C9900…`-Nummern sind Platzhalter der Bibliothek, keine bestellbaren
Lagernummern.** Damit ist die Frage „baut J2 höher als 4,45 mm?" nicht
beantwortbar — **es gibt kein Datenblatt, aus dem eine Höhe käme.**

**Dasselbe gilt für ein zweites Teil, und das ist das wichtigere:**

| Kennung | gewähltes Teil | Nummer | Art |
| --- | --- | --- | --- |
| J2 | „JST Right-Angle Connector - SMD 2-Pin" | **C9900141337** | **Platzhalter** |
| **J4** | FPC-05mm-12P-14241555_2 (Anzeigeanschluss) | **C9900036520** | **Platzhalter** |

**J4 ist der Steckverbinder der Anzeige** — und seine 14 Pole, mit denen ich
die zwei Haltelaschen an `GND` gelegt habe, stammen aus genau diesem
Platzhaltersymbol. Die übrigen Steckverbinder sind echte Teile (J1 C7095263,
J3 C492401, J5 C41434767, J6 C2691448).

> **Was daraus folgt, und es ist mehr als eine Höhenfrage:** **J2 und J4
> müssen durch bestellbare Teile ersetzt werden**, bevor irgendeine Höhe,
> Padgröße oder Polzahl belegt ist. Beim Ersetzen kann sich die Polzahl von J4
> ändern — dann ändert sich der Schaltplan.

**Zur Höhe, was belegt ist und was nicht:** Für die flache seitliche
SMD-Ausführung der PH-Reihe, **S2B-PH-SM4-TB (C295747)**, nennen DigiKey,
Arrow und Octopart **≈ 5,5 mm** über der Platine (0,217") — **über der Grenze
von 4,45 mm.** Das ist ein **verwandtes Teil, nicht unseres**; die PH-Reihe
gibt es senkrecht und seitlich mit erheblich verschiedenen Maßen. Dieselben
Händler nennen für die Tiefe **7,6 und 8,6 mm** — ein ganzer Millimeter
Unterschied bei einem Maß, das über das Platzieren entscheidet. **Beim Setzen
gilt der Footprint, am Ende das Gerber.**

> **Nicht gelöst, gemeldet.** Wenn ein PH-Stecker über 4,45 mm baut, sind es
> drei Wege: **flacherer Stecker**, **anderer Platz** (dann nicht neben der
> Zelle), oder **Akkukabel direkt anlöten** — das macht das Gerät dünner und
> den Akku **nicht mehr tauschbar.** Der Plan hat diese Frage schon gestellt
> („tauschbar bauen oder als Verbrauchsteil ansagen"); sie gehört Thomas.

**J2 bleibt unplatziert.**

# Platziert: 54 Bauteile, `overlapCount` 0 — und die Pinprüfung von J1 ist durchgefallen

Stand 24.10.2026, 19:57. Der Import war durch (54 Bauteile, alle neben der
Platine), die Teile sind jetzt gesetzt. **Gemessen, nicht geschätzt:**

| Beleg | Ergebnis |
| --- | --- |
| Bauteile auf der Platine | **54** |
| `pcb_layout_boxes`, Rand 0,25 mm | **`overlapCount` 0** |
| Freihaltestreifen `x` 25,5 … 27 bei `y` 12,0 … 22,6 | **leer** — kein Bauteil liegt dort |
| Bauteile im Modulschatten (`x` 10,0 … 25,5, `y` 12,05 … 22,55, Unterseite) | **keines** |

## Der Fund: die Haltestifte von J1 ragen über die Platinenkante

**Die Prüfung, die ich als „erst am gesetzten Footprint möglich" angekündigt
hatte, ist gelaufen — und sie fällt durch.** `pcb_pads_list` auf J1:

Die vier Haltestifte sind ovale Pads. Die **vorderen zwei** sitzen bei
`y` = 0,505 mit 1,699 mm Padlänge und 1,4 mm Lochlänge. Bei der bündigen Lage
(J1-Mitte 2,685, `d` = 1,90) ergibt das:

| | von | bis | |
| --- | --- | --- | --- |
| Pad | **−0,344** | +1,355 | **0,344 mm über die Kante hinaus** |
| Loch | **−0,195** | +1,205 | **0,195 mm über die Kante hinaus** |

**Ein Loch auf der Platinenkante ist kein Schönheitsfehler, das ist ein
Fertigungsfehler.** Die Stiftmitte liegt 2,180 mm hinter der J1-Mitte; damit
das vordere Pad 0,2 mm Rand behält, muss die J1-Mitte **≥ 3,23 mm** liegen.

### Damit widersprechen sich zwei Auflagen um rund einen halben Millimeter

- **Bündige Vorderkante** (± 0,3 mm) verlangt J1-Mitte **2,685**.
- **Die Platinenkante** verlangt J1-Mitte **≥ 3,23**.

**Die vorbereitete Mulde ist damit keine Möglichkeit mehr, sondern nötig.**
Mit der Mulde (0,8 mm tief, `d` = 1,10) ist die J1-Mitte **3,485**, und dann
gilt:

| | Wert |
| --- | --- |
| vorderes Pad beginnt bei | **+0,456 mm** |
| vorderes Loch beginnt bei | **+0,605 mm** |
| Abstand zur Forderung „≥ 0,2" | **0,256 mm Luft** |

> **J1 steht jetzt auf `y` = 3,485**, also auf der Mulden-Lage. Das ist die
> einzige Lage, die **beide** Auflagen erfüllt. **Ohne die Mulde ist die
> bündige Vorderkante nicht herstellbar** — dann müsste die Frontwand dünner
> werden oder die Toleranz ± 0,3 fallen. **Der Gehäuse-Strang muss die Mulde
> bauen.**

## Wie die Plätze zustande kamen: drei Runden, jede gemessen

Meine Gehäusemaße waren **kleiner als die echten Footprints** — vor allem beim
SOT-23 (Pads machen aus 1,3 mm Körperbreite rund 2,6 mm Belegung) und beim
Flash, dessen 8 × 6 mm **quer zu meiner Annahme** liegen (6 mm in `x`, 8 in
`y`).

| Runde | `overlapCount` | was behoben wurde |
| --- | --- | --- |
| 1 | **15** | Erstplatzierung nach Gehäusemaßen |
| 2 | **2** | Entkopplung von U2/U3 gelöst, Transistorreihen entzerrt, Modulband in **eine** Reihe gelegt |
| 3 | **0** | L1 gegen X1, Q3 gegen U5 — und Q1/Q2/Q3 aus dem Modulschatten gezogen |

**Die Überlappungsmeldung gibt die Beträge an**, und damit ließ sich gezielt
nachrücken statt zu raten. Das ist der Grund, warum drei Runden genügten.

**U2 musste dabei den linken Streifen verlassen:** U3 belegt dort `y` 11,5 …
19,5, und zwischen U3 und dem Modulrand bleiben nur 3,05 mm für ein 3,5 mm
breites Teil. U2 sitzt jetzt mit C10, C11, R6, R7 in der **rechten Tasche**
(`x` 18,2 … 25,6, `y` 1 … 11,9). **Die I2C-Leitung wird dadurch rund 8 bis
10 mm lang** statt der 12, die ich mit der außermittigen Zelle gerechnet
hatte — weiterhin ein Bruchteil der 25 mm aus Z3, und sie läuft auf einer
Innenlage.

## Drei Bauteile stehen bewusst neben der Platine

| Teil | geparkt bei | Grund |
| --- | --- | --- |
| **J2** Akkustecker | `x` 33, `y` 10 | Bauhöhe unbekannt (Platzhalterteil, kein Datenblatt), Grenze 4,45 mm |
| **J5** Sensoranschluss | `x` 33, `y` 20 | falsches Bauteil — 1 × 6 Stiftfeld für einen 12-poligen ZIF |
| **J6** SWD | `x` 33, `y` 30 | 2,54-mm-Durchsteckfeld und NFC-Schleife schließen sich aus; wird zu Testpunkten |

**Das ist Absicht und kein Versehen** — sie liegen sichtbar beieinander
rechts neben dem Brett, nicht verstreut.

## Was als nächstes gemessen werden muss

1. **Die Ausdehnung von U1** aus seinen 61 Pads: liegt die Antennenkante
   wirklich bei `x` 25,5, und bleibt das Modul innerhalb der Platine?
   `ungeprueft` — bisher aus dem Gehäusemaß gerechnet.
2. **Die Kupferlagen des Freihaltestreifens**: kein Bauteil liegt darin, aber
   die Masseflächen sind noch nicht gegossen. Beim Gießen muss der Streifen in
   **allen vier** Lagen ausgespart bleiben.
3. **J2, J5, J6** brauchen Entscheidungen, nicht Plätze.

# Stückliste durchgezählt: genau zwei Platzhalter — und für beide gibt es echte Teile

Stand 24.10.2026. Grundlage ist die **Bauteilliste der Platine selbst**
(`pcb_components_list`, alle 54 Einträge mit LCSC-Nummer), nicht meine
Erinnerung.

## Das Loch ist genau zwei Teile groß

| Kennung | Nummer | Art |
| --- | --- | --- |
| **J2** | `C9900141337` | **Platzhalter** |
| **J4** | `C9900036520` | **Platzhalter** |

**Alle anderen 52 Bauteile tragen echte Lagernummern** mit Hersteller: die
fünf Halbleiter (C5118826, C5202100, C7389628, C38066, C323793), die fünf
MOSFETs (C2940629), Quarz (C32346), Spule (C295388), LED (C3019536), TVS
(C552589), USB-C (C7095263), die drei Stiftfelder (C492401, C41434767,
C2691448), die Taster (C388883) und sämtliche Passiven.

**Es sind also beide Steckverbinder, die keine Teile sind — und kein
Halbleiter.** Der Satz „die Steckverbinder sind die schwächste Stelle dieses
Entwurfs" ist damit gezählt, nicht gefühlt.

## J4 — echtes Teil gefunden, **und es hat dieselben 14 Pins**

| | |
| --- | --- |
| Teil | **Hirose FH12-12S-0.5SH(55)** |
| Nummer | **C88360**, Hersteller HRS, **mit Datenblatt** |
| Kontakte | **12** (dazu zwei Haltelaschen, zusammen **14 Pins**) |
| Raster | 0,5 mm |
| Bauhöhe über der Platine | **2 mm** |
| Bauart | Deckelverschluss, **Unterkontakt**, Folie 0,3 mm, Goldkontakte |

> **Das ist die Schaltplanfrage, nicht eine Layoutfrage:** Der Platzhalter hat
> **14** Pads, das echte Teil **12**. Die beiden zusätzlichen Pole, auf die ich
> die „Haltelaschen" an `GND` gelegt habe, **gibt es an diesem Teil nicht.**
> Beim Tausch fallen `J4.13` und `J4.14` aus der Netzliste — das sind zwei
> Verbindungen weniger, nicht zwei andere.
>
> **Das passt zur Anzeige:** Das Crystalfontz-Glas hat 12 Pole, und genau die
> zwölf stehen in `schematic.md`. **Die 14 waren die Erfindung des
> Platzhalters, nicht eine Eigenschaft der Anzeige.**

> **BERICHTIGUNG 24.10.2026, noch am selben Abend — der Kasten darüber ist
> falsch, und zwar sein ganzer Schluss.** Ich hatte die **Kontaktzahl** des
> echten Teils („Number of Contacts: 12P“) mit der **Pinzahl** des
> Platzhalters verglichen. Das sind zwei verschiedene Größen.
>
> **Gemessen, nicht vermutet:** Das Hirose-Teil als Wegwerfteil gesetzt und
> seine Pins ausgelesen. Es hat **14 Pins** — 1 bis 12 die Kontakte, 13 und
> 14 die Haltelaschen. Genau wie der Platzhalter, dessen Name selbst „12P“
> lautet. **An der Netzliste ändert sich nichts;** `J4.13` und `J4.14` liegen
> weiter an `GND`. Nach dem Tausch gezählt: 54 Bauteile, 59 Netze, 219
> Verbindungen, DRC 0 Fehler.
>
> **Es ist derselbe Fehler, den ich zwei Absätze weiter unten bei J2
> aufgedeckt hatte** — dort waren 7,9 und 8,6 mm nicht zwei widersprechende
> Angaben, sondern Länge und Tiefe. Zwei Zahlen zu vergleichen, die dasselbe
> Ding verschieden messen, fällt nicht auf, solange beide plausibel sind.
> Dass ich die Falle beim Nachbarteil erkannt und beim eigenen Befund
> übersehen habe, ist der Grund, warum eine Pinzahl nachgesehen wird und
> nicht erschlossen.

## J2 — zwei echte Teile, und eines löst das Höhenproblem

| | **JST S2B-PH-SM4-TB** | **JST SM02B-SRSS-TB** |
| --- | --- | --- |
| Nummer | C295747 | **C160402** |
| Reihe | PH, 2,00 mm Raster | **SH, 1,00 mm Raster** |
| **Bauhöhe** | **5,5 mm** | **2,9 mm** |
| Grundfläche | 7,9 × 8,6 mm | **4,0 × 4,32 mm** |
| Strom | 2 A | 1 A |

**Die Grenze ist 4,45 mm.** Der PH liegt **1,05 mm darüber**, der SH **1,55 mm
darunter** — und braucht nur ein Viertel der Fläche. 1 A gegen 100 mA
Ladestrom ist reichlich.

> **Nebenbei löst sich der Widerspruch, der die Quellenlage so dünn aussehen
> ließ:** Die Händler nannten 7,6 und 8,6 mm für „die Tiefe". Die
> Bibliotheksangabe zeigt, dass es **zwei verschiedene Maße** sind — Länge 7,9
> und Tiefe 8,6. **Sie haben sich nicht widersprochen, sondern verschiedene
> Kanten gemessen.**

**Was der SH kostet, und das ist die Entscheidung:** Die Adafruit 1317 wird
**mit PH-Stecker am Kabel** geliefert. Mit einem SH auf der Platine passt das
Kabel der Zelle nicht — entweder wird es **umgecrimpt**, oder es braucht eine
Zelle mit SH-Stecker, oder ein kurzes Zwischenkabel.

> **Damit sind es nicht mehr drei unbestimmte Wege, sondern zwei klare:**
> **(a) SH-Stecker (2,9 mm, passt) und das Zellkabel anpassen**, oder
> **(b) PH behalten (5,5 mm) und das Gerät 1,05 mm dicker machen.**
> Der dritte Weg — Kabel direkt anlöten — bleibt möglich, macht den Akku aber
> unaustauschbar. **Thomas entscheidet; ich habe keinen der Wege eingebaut.**

**J2 und J4 bleiben bis dahin neben der Platine geparkt.** Ein Tausch ändert
Footprint und bei J4 die Netzliste; das gehört in einen eigenen Durchgang,
nicht zwischen zwei Platzierungsrunden.

# U1 gemessen — und die Antenne zeigt in die falsche Richtung

Stand 24.10.2026. Anlass: die Antennenkante bei `x` 25,5 war **gerechnet, nicht
gemessen**, und auf ihr steht die Freihaltezone — die einzige Stelle, an der
uns Raytac eine Vorschrift macht.

## Was gemessen ist

Alle **61 Pads** über `pcb_pads_list`, Hülle aus `extent_mm` (dem Maß **nach**
der Drehung), nicht aus `width/height`:

| | gemessen |
| --- | --- |
| Padhülle `x` | **12,0992 … 23,4999** = 11,4007 mm |
| Padhülle `y` | **12,2998 … 22,3000** = 10,0002 mm |
| Bauteilmitte | 17,75 / 17,30, Drehung **90°** |

**Die Mitte des Bauteils ist die Mitte der Padhülle, nicht die Mitte des
Körpers.** Genau daraus entstand die alte Zahl: 17,75 + 15,5/2 = **25,50**.
Diese Rechnung setzt voraus, dass der Ursprung in der Körpermitte liegt. **Er
liegt dort nicht** — bei einem Modul mit Antenne an einem Ende sitzen die Pads
nicht symmetrisch im Körper.

## Wie der Körper daraus folgt

Körpermaß **15,5 × 10,5 mm** aus dem Datenblatt (nicht gemessen, das ist der
eine eingesetzte Fremdwert). Alles andere ist Rechnung auf gemessenen Zahlen:

- Quer: (10,50 − 10,0002) / 2 = **0,2499 mm** Pad-Einzug an den Längsseiten.
- Längs: 15,50 − 11,4007 − 0,2499 = **3,8494 mm** Leerstreifen am pad-armen Ende.

Damit:

| | daraus | Datenblatt |
| --- | --- | --- |
| Körper `x` | 8,2498 … 23,7498 = **15,5000** | 15,5 |
| Körper `y` | 12,0499 … 22,5499 = **10,5000** | 10,5 |

**Beide Maße treffen auf vier Stellen.** Der Pad-Einzug von 0,25 mm ist aus dem
Querschnitt gewonnen und am Längsschnitt bestätigt — das ist die Gegenprobe,
nicht bloß eine Umstellung derselben Gleichung.

**Und die unabhängige Bestätigung:** Raytacs Freihaltezone ist mit
**10,5 × 3,8 mm** angegeben. Der Leerstreifen misst **10,50 breit** (die ganze
Modulbreite) und **3,85 tief**. Die 10,5 waren in der Rechnung nie enthalten,
die 3,8 nur zufällig nah. **Die Zone ist dieser Streifen.**

## Welches Ende die Antenne ist — und warum das sicher ist

| Ende | Pads |
| --- | --- |
| `x` ≈ 23,2 (hoch) | **19** (Pads 15–33, darunter XL1/XL2, VBUS, DCCH) |
| `x` ≈ 12,1 (tief) | **2** (Pads 1 und 55, beide `GND`) |

Ein 10,5 mm breites Ende mit zwei Massepads, dahinter 3,85 mm Körper ohne
jedes Pad — **das ist die Antenne.** Am Ende mit 19 Signalpads kann keine
„No Ground-Pad Area" liegen. Dass die beiden Eckpads Masse sind, passt zu
Raytacs Regel von den Durchkontaktierungen „especially at the four corners".

> **Die Freihaltezone liegt bei `x` 8,25 … 12,10, `y` 12,05 … 22,55 —
> nicht bei `x` 25,5 … 27.** Sie liegt damit **mitten auf der Platine**, und
> die Antenne zeigt **nach innen**, nicht an eine Kante.

## Was daraus folgt, und was ich nicht getan habe

Raytac verlangt das Modul **zur Platinenkante hin**. Die Antennenkante liegt
jetzt **8,25 mm von `x` = 0** und das andere Ende **3,25 mm von `x` = 27**.
Die Antenne steht also am weitest möglichen Punkt von jeder Kante entfernt,
und in ihrer Zone liegt Platz, der für Kupfer und Bauteile verplant ist.

**Die alte Zahl war nicht nur falsch, sie war in der Richtung falsch.** Eine
Freihaltezone, die ich bei 25,5 … 27 freigehalten hätte, hätte am
Antennenende nichts bewirkt und am Anschlussende Platz verschenkt.

**Ich habe nichts verschoben.** U1 ist das größte Bauteil; es zu drehen oder zu
versetzen heißt, die Plätze neu zu rechnen — und wohin die Antenne zeigen soll,
ist eine Frage an den Entwurf, nicht an das Werkzeug. Die Masseflächen warten
ebenfalls: ihre Aussparung hängt an dieser Zone, und solange die Zone wandert,
wäre jedes Kupfer eine Wegwerfarbeit.

**Lehre, und sie ist dieselbe wie bei J1:** Die 25,50 sah nach einer Messung
aus, weil sie vier Stellen hatte. Sie war eine Annahme über den Ursprung des
Footprints — und Annahmen über Ursprünge fallen erst auf, wenn man das Bauteil
aus seinen Pads nachbaut und prüft, ob das Datenblattmaß herauskommt.
