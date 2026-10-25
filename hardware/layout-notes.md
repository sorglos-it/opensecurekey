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

**Die Grenze ist 4,45 mm.** [**Zur Zahl 4,45 siehe die Berichtigung am Ende dieser Datei: es gibt sie zweimal. Hier ist die Höhe gemeint und das ist richtig — maßgeblich für J2 ist aber die Hohlraumtiefe 4,65.**] Der PH liegt **1,05 mm darüber**, der SH **1,55 mm
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

# U1 gedreht — und dabei fiel auf, dass mein Platzierungsbeweis nichts bewies

Stand 24.10.2026, nach der Entscheidung „Antenne an die Längskante `x` = 27".

## Die Drehung: gesetzt und nachgemessen

**Der Schub ist −0,2502 mm, nicht +3,25.** Meine eigene Zahl im Vorschlag war
falsch: die 3,25 waren der Abstand des **jetzigen hohen Endes** zur Kante. Nach
der Drehung steht dort das **Antennenende**, und das reicht 3,85 mm weiter
hinaus. Gerechnet: nach 180° liegt die Körperkante bei 27,2502 — also
**0,2502 mm über** der Platine, nicht 3,25 mm darunter.

U1 steht jetzt auf **17,4998 / 17,300001, Drehung 270°**. Danach aus den 61
Pads neu gemessen, nicht aus der Rechnung übernommen:

| | gemessen |
| --- | --- |
| Padhülle | `x` 11,7503 … 23,1484 · `y` 12,2998 … 22,3000 |
| Körper daraus | `x` 11,5004 … **27,0004** · `y` 12,0499 … 22,5499 |
| Länge × Breite | **15,5000 × 10,5000** |

> **Freihaltezone neu: `x` 23,1484 … 27,0004, `y` 12,0499 … 22,5499 —
> 10,5000 breit, 3,8520 tief.** Sie liegt jetzt an der Längskante, wo der
> Entwurf sie immer haben wollte.

Die 0,0004 mm über 27 sind keine Lage, sondern die **Mil-Rasterung** des
Werkzeugs: EasyEDA rechnet die Platine intern in mil (0,0254 mm), ein
Millimeterwert ist dort nicht beliebig fein einstellbar. Die Richtung der
19 Signalpads hat mitgedreht: sie stehen jetzt bei `x` 12,05, die zwei
Massepads des Antennenendes bei 22,95.

## Der Befund, der schwerer wiegt als die Drehung

`pcb_layout_boxes` meldete nach der Drehung **`overlapCount` 0**. Weil U1s
Kasten (11,8 … 23,199) nicht zu meiner gemessenen Hülle (11,7503 … 23,1484)
passte, habe ich nachgesehen, was das Werkzeug eigentlich misst.

| | |
| --- | --- |
| Pad-**Mitten** + 0,25 | 11,7998 … 23,1989 |
| **Kupfer**hülle + 0,25 | 11,5003 … 23,3984 |
| was der Kasten sagt | **11,8000 … 23,1990** |

**Der Kasten ist die Hülle der Pad-Mitten plus Rand. Das Kupfer der Pads
selbst ist nicht darin.** Gegenprobe an C1: zwei Pads auf gleicher Höhe, also
Mittenhülle 0 hoch; der Kasten ist **genau 0,500** hoch = 2 × 0,25. Wäre das
Kupfer enthalten, müsste er um die Padhöhe höher sein.

> **Damit bedeutet `overlapCount` 0 bei `margin_mm` 0,25 nicht 0,25 mm
> Kupferabstand, sondern 0,25 mm zwischen Mittenhüllen.** Bei einem Pad von
> 0,6 mm Höhe reicht das Kupfer 0,3 mm über die Mitte hinaus — es frisst den
> ganzen Rand und mehr.

**Mein Satz „54 Bauteile, `overlapCount` 0 bei 0,25 mm Rand" war deshalb kein
Beweis.** Er stimmte als Aussage über das Werkzeug und sagte nichts über die
Platine.

## Selbst gemessen: 251 Pads, Pad gegen Pad

Aus `pcb_pads_list` die echten Kupferrechtecke (`extent_mm`, also **nach** der
Drehung) und jedes Paar verschiedener Bauteile derselben Lage verglichen:

| Paar | Netze | Abstand |
| --- | --- | --- |
| `L1.1` ↔ `X1.2` | DCCH ↔ XL2 | **−0,5362** |
| `SW2.2'` ↔ `U1.1` | GND ↔ GND | −0,2502 |
| `C9.2` ↔ `X1.1` | GND ↔ XL1 | **−0,2222** |
| `C2.1` ↔ `U4.2` | VBAT_RAW ↔ GND | **−0,1016** |
| `C2.1` ↔ `U4.3` | VBAT_RAW ↔ VBAT_RAW | −0,1016 |
| `R1.1` ↔ `U4.1` | PROG ↔ CHG_STAT | **−0,0151** |

**Vier davon liegen zwischen verschiedenen Netzen. Das sind Kurzschlüsse**,
keine Enge: die Spule des Abwärtswandlers steht auf dem Quarz, der Akku auf der
Masse. Zwei weitere überlappen im gleichen Netz — elektrisch harmlos, als
Kupfer trotzdem falsch.

Von Hand gegengerechnet, damit es nicht am Skript hängt:
`L1.1` Kupfer `x` 12,7151 … 14,3511 gegen `X1.2` `x` 12,2503 … 13,2513 →
Überdeckung 12,7151 … 13,2513 = **0,5362 mm**, in `y` über die volle Padhöhe.

Dazu **15 weitere Paare unter 0,25 mm**, davon sieben unter 0,10 mm
(`C3.2`↔`C4.1` 0,0307, `R3.2`↔`R4.1` 0,0665, `R6.2`↔`R7.1` 0,0665,
`R4.2`↔`R5.1` 0,0690, `R5.2`↔`R13.1` 0,0690, `R1.2`↔`R2.1` 0,0690,
`C3.1`↔`C5.2` 0,0992) — alle zwischen **verschiedenen** Netzen und damit unter
jeder Fertigungsgrenze, die JLCPCB anbietet.

## Was das heißt und was nicht

**Die Drehung hat das nicht verursacht.** Fünf der sechs Überlappungen liegen
auf der Unterseite und haben mit U1 nichts zu tun; nur `SW2`↔`U1.1` entstand
daraus, dass Pad 1 mitgedreht ist. **Die Platzierung war vorher schon falsch,
und die Entwarnung kam von einem Maß, das die Padgröße nicht kennt.**

**Kein Kupfer.** Masseflächen über einer Platzierung mit vier Kurzschlüssen
wären Wegwerfarbeit, und zwar eine, die die Fehler verdeckt.

**Nächster Schritt:** die Plätze neu rechnen, diesmal gegen die
**Kupferrechtecke aus `pcb_pads_list`** statt gegen `pcb_layout_boxes`, mit
einer Mindestluft, die aus dem Regelsatz kommt und nicht aus meinem Gefühl.

**Lehre, und sie ist dieselbe wie dreimal heute:** Ein Werkzeug, das eine Zahl
nennt, sagt nicht, was es gemessen hat. „overlapCount 0" klang nach einer
Aussage über Bauteile und war eine über Mittelpunkte — genau wie 25,50 nach
einer Messung klang und eine Annahme über einen Ursprung war, und wie „12 Pole"
nach einer Pinzahl klang und eine Kontaktzahl war. **Dreimal dieselbe Falle:
zwei Größen, die gleich heißen und Verschiedenes messen.**

# Die Paare auseinandergezogen: 0 Regelverstöße, und ein Fall, der keiner Rechnung weicht

Stand 24.10.2026. Maß ist jetzt das **Kupferrechteck** aus `pcb_pads_list`
(`extent_mm`, also nach der Drehung), Grenze die Vorschrift aus dem Regelsatz:
**SMD-Pad ↔ SMD-Pad = 0,15200122 mm**, Pad ↔ Platinenrand = 0,29999940 mm.

## Erst noch ein Verstoß, den mein eigener Vergleich verdeckt hatte

`J1` hat **MULTI**-Pads — Durchsteiger, die auf **beiden** Kupferlagen liegen.
Mein Vergleich hatte nach Lage gruppiert und MULTI als dritte Lage behandelt,
also **nie gegen TOP oder BOTTOM geprüft**. Mit MULTI auf beiden Lagen kommt
ein Paar hinzu:

> `J1.S1` (Schirm, GND) ↔ `R3.1` (Q1_G): **0,0092 mm.**

Damit waren es **13** Verstöße zwischen verschiedenen Netzen, nicht 12.
Dieselbe Falle wie immer: „Lage" heißt bei einem Durchsteiger etwas anderes als
bei einem SMD-Pad.

## Der Änderungssatz, vorher durchgerechnet

17 Bauteile, in einem Zug gesetzt, Zielabstand 0,25 mm statt der erlaubten
0,152 — damit die Mil-Rasterung des Werkzeugs (0,0254 mm) nichts kippen kann.

| Reihe | Teile | was geschah |
| --- | --- | --- |
| `y` ≈ 2,0 | R3, R4, R5, R13, R14 | nach rechts gestaffelt, R3 hinter `J1` |
| `y` ≈ 7,0 | C11, R6, R7 | nach rechts gestaffelt, C10 blieb |
| `y` ≈ 10–12 | C2 (+0,30 `x`), R1 (+0,85 `y`) | von `U4` weg; R1 nach oben statt zur Seite, weil rechts `Q2` steht |
| `y` ≈ 23,9 | C5, C3, C4, C8, C9, X1, L1 | ganze Reihe neu aufgeteilt, 0,25 mm Luft |

R2 blieb, wo es war: durch R1s Weg nach oben ist der Konflikt in `y` gelöst,
und nach rechts hätte R2 in `Q2` gelaufen.

## Gemessen, nachdem es gesetzt war

| | vorher | nachher |
| --- | --- | --- |
| Verstöße, verschiedene Netze | **13** | **0** |
| davon Überlappungen | 4 | **0** |
| kleinster Abstand verschiedener Netze | −0,5362 | **+0,2006** |
| Pads unter 0,300 mm am Rand | 0 | **0** |

Der engste Punkt ist jetzt `C2.1` (VBAT_RAW) gegen `U4.2` (GND) mit
**0,2006 mm** — ein Drittel über der Vorschrift.

## Stichprobe von Hand, damit es nicht am Skript hängt

| Paar | Rechnung aus den Mitten | vorher |
| --- | --- | --- |
| `L1.1` ↔ `X1.2` | 13,82256 − 13,57388 = **0,24868** | −0,5362 |
| `C9.2` ↔ `X1.1` | 10,07098 − 9,82072 = **0,25026** | −0,2222 |
| `C2.1` ↔ `U4.2` | 4,90098 − 4,70036 = **0,20062** | −0,1016 |
| `J1.S1` ↔ `R3.1` | 18,52570 − 18,27522 = **0,25048** | +0,0092 |
| `R1.1` ↔ `U4.1` | in `y`: 11,87832 − 11,64822 = **0,23010** | −0,0151 |
| `R6.2` ↔ `R7.1` | 23,69460 − 23,44272 = **0,25188** | +0,0665 |

`R1` gegen `U4` ist der lehrreiche: in `x` überlappen die beiden weiter um
0,0151 mm, der Abstand kommt allein aus `y`. **Wer nur eine Achse ansieht,
hält das für ungelöst.**

## Was es gekostet hat

`L1.1` (DCCH) steht jetzt bei `x` 14,64056, U1s DCCH-Pad bei 12,04976 — **2,59
mm in `x`, 1,1075 mm mehr als vorher.** Das ist die Schleife des
Abwärtswandlers, und länger ist dort schlechter. Der Grund ist zwingend: X1
musste zwischen L1 und C9 Platz bekommen, und X1 gehört an die Pads 17/18, die
ebenfalls am tiefen `x`-Ende liegen. **Spule und Quarz streiten um dieselbe
Ecke.** Wenn die 1,1 mm wehtun, ist die Antwort nicht eine andere Verteilung in
dieser Reihe, sondern den Quarz in eine andere Reihe zu nehmen.

## Der Fall, der keiner Rechnung weicht: SW2

`SW2` bleibt stehen, und zwar nicht aus Nachlässigkeit. Zwei Befunde:

1. `SW2.2'` (GND) überlappt `U1.1` (GND) um **0,2502 mm**. Gleiches Netz, also
   kein Kurzschluss — als Kupfer trotzdem falsch.
2. **Beide unteren Taster-Pads liegen in der Freihaltezone**: `SW2.1'` bei
   `x` 23,6994 … 24,7994 und `SW2.2'` bei 22,2008 … 23,3008, jeweils
   `y` 22,0 … 23,4, und die Zone endet oben bei `y` 22,5499.

**Und der Taster passt dort nicht, mit keinem `y`:**

| | |
| --- | --- |
| Padspanne des Tasters, gemessen | **4,9992 mm** |
| Band zwischen U1-Körper (22,5499) und Glas (27,0) | **4,4501 mm** |
| fehlen | **0,5491 mm** |

Mit der Vorschrift von 0,152 gegen den Modulkörper müsste das tiefste Pad bei
22,7019 beginnen und das höchste läge bei 27,7011 — **0,7011 mm im
Anzeigefenster.** Selbst wenn man nur gegen U1s Pads rechnet (22,3000) und den
2,2 mm hohen Modulkörper ignoriert, sind es noch 0,4512 mm.

> **Der Taster war nie dort unterzubringen, auch vor der Drehung nicht.** Das
> alte Pad 33 lag bei `y` 21,901 … 22,300 und `x` 22,901 … 23,500 und
> überlappte `SW2.2'` ebenso. Die Drehung hat den Fall nur sichtbar gemacht.

**`SW1` ist nicht betroffen:** es steht bei `x` 2,2008 … 4,7994, also links von
U1s Körper (ab 11,5004) und weit von der Zone (ab 23,1484).

**Ein Ausweichband gibt es, und zwar unter U1:** auf der Oberseite ist zwischen
`J1` (bis `y` 5,9892) und U1s Körper (ab 12,0499) nichts. Mit der Vorschrift
bleibt `y` 6,1412 … 22,3979, also **16,2567 mm für 4,9992 mm Taster — es
passt.** Das heißt aber: der Reset-Taster sitzt dann am USB-Ende, nicht
zwischen Modul und Anzeige, und SW1 und SW2 stehen nicht mehr symmetrisch.

**Das ist eine Gehäusefrage und keine Rechnung.** Drei Wege, alle mit Folgen:
`SW2` ans USB-Ende (Symmetrie weg), **beide** Taster dorthin (Symmetrie bleibt,
zwei Öffnungen wandern), oder das Anzeigefenster beginnt später als `y` 27.
**Ich habe keinen eingebaut.**

# SW2 versetzt, Masseflächen gegossen — und eine Art Kollision, die ich nie geprüft hatte

Stand 24.10.2026, später Abend.

## SW2 steht am USB-Ende

`SW2` (Reset) auf **23,500001 / 8,6412, Drehung 270°**. Pads jetzt
`x` 22,2008 … 24,7994, `y` **6,1402 … 11,1419**.

Nicht ganz an den unteren Rand, obwohl dort noch Platz wäre: am USB-Ende
liegen die **Mulde und der Steckerüberstand** des Gehäuses. Der Taster sitzt
deshalb oberhalb von J1s Streifen. Abstand nach oben zu U1s Körper (12,0499):
**0,9080 mm**.

`SW1` bleibt, wo es ist — neben der Anzeige, wo der Bestätigen-Knopf hingehört.

## Der Befund: Schlitzbereiche

Die PCB-DRC meldete **„SMD Pad to Slot Region, distance is 0mm"**. Die Taster
haben **Schlitzbereiche** — Durchbrüche durch die Platine für den Betätiger.

> **Ein Schlitz ist kein Pad.** Mein Vergleich hat Pad gegen Pad gerechnet und
> konnte diese Art Kollision **grundsätzlich nicht finden**, egal wie genau er
> war. Und weil ein Schlitz durch die ganze Platine geht, trifft er auch
> Bauteile auf der **anderen** Lage: `SW1` liegt oben, `C5` und `C3` unten.

| Paar | Lage | Zustand |
| --- | --- | --- |
| `C5_2` ↔ SW1-Schlitz | unten/oben | **0 mm — bestand schon vorher** |
| `C3_1` ↔ SW1-Schlitz | unten/oben | **0 mm — bestand schon vorher** |
| `C3_2` ↔ SW1-Schlitz | unten/oben | **0 mm — bestand schon vorher** |
| `C1_2` ↔ SW2-Schlitz | unten/oben | **0 mm — von mir verursacht, behoben** |

Nachgerechnet, wer was verursacht hat: `C5` und `C3` lagen **vor und nach**
meiner Verschiebung in SW1s `x`-Bereich (2,2008 … 4,7994). Die drei sind alt.
Die vierte entstand, als ich SW2 nach unten setzte — **C1 lag dort schon.**
`C1` ist um **1,1992 mm** nach links gerückt, auf `x` 20,8008; Luft zu `R16`
**0,9852 mm**. Danach: Clearance-Fehler von 5 auf 4.

**Die drei alten bleiben offen, und sie sind keine Rechenfrage:** `SW1` steht
bei `x` 2,2 … 4,8 und spannt `y` 22,0 … 27,0; die Bauteilreihe liegt bei
`y` 23,9 und läuft genau darunter hindurch. Wer die Reihe rechts an SW1
vorbeischiebt, beginnt bei `x` 5,10 — dann stünde `X1` bei 14,77 statt 11,82
und `L1` bei 18,52, also **6,5 mm von DCCH** statt 2,59. **Das verschlimmert
genau das, was heute schon teuer war.**

## Masseflächen: vier Lagen, Zone frei

| | |
| --- | --- |
| Lagen | TOP, INNER_1, INNER_2, BOTTOM, Netz `GND`, SOLID |
| Umriss | 0,254 … 26,746 × 0,254 … 69,746 (Regel Kupferfläche ↔ Rand = 0,254) |
| Aussparung | `x` ab **23,148**, `y` **12,05 … 22,55** — die Freihaltezone |

**„rebuilt" heißt nicht „gefüllt".** Der erste Durchgang meldete alle vier
Flächen als neu gebaut, und das Bild zeigte **kein Kupfer**. Erst nach
`doc_reload` und einem zweiten Aufbau war die Platine gefüllt. Hätte ich der
Rückmeldung geglaubt, hätte ich leere Flächen als fertig gemeldet.

**Nebenbei: der Platinenrand ist da.** Das Bild zeigt das Rechteck (0|0) …
(27|70). `pcb_lines_list` meldet für die Umrisslage weiterhin **0** — genau
der Fall, vor dem ich mich selbst gewarnt hatte. **Gut, dass ich ihn als
unbekannt behandelt und nicht als fehlend gemeldet habe.**

**Vier Massepads hängen noch:** `U2_1`, `U2_17`, `U2_19` und `C17_2`, alle
unten. Nicht 57, sondern vier — **die Fläche verbindet also.** Zwischen die
Pads eines QFN mit 0,4 mm Raster kommt kein Gießkupfer; diese vier brauchen
beim Verlegen je eine Durchkontaktierung. Das ist Verlegearbeit, kein
Gießfehler.

## Offene Punkte, die jemand anderen betreffen

1. **Gehäuse: die Öffnung für SW2 wandert.** Geplant war die Seitenwand bei
   `y` 22,75 … 26,25. SW2 liegt jetzt bei `y` 6,14 … 11,14, also am USB-Ende.
   **Der Gehäuse-Strang ist archiviert**; das muss in einen neuen Durchgang
   oder von Hand nachgezogen werden. SW1s Öffnung bleibt, wo sie war.
2. **Quarz und Spule: nicht nachjustieren.** Die Wandlerschleife ist 1,1075 mm
   länger geworden, weil X1 an die Pads 17/18 muss. **Wenn das zu viel ist,
   gehört der Quarz in eine andere Reihe** — dieselbe Reihe anders zu
   verteilen, macht es nur anders schlecht.
3. **J4 ist im Schaltplan getauscht, auf der Platine noch nicht.** Die DRC
   meldet **1 Netzlistenfehler**; das ist mit großer Wahrscheinlichkeit genau
   das — „Import Changes" samt „Apply Changes" steht noch aus und braucht einen
   Klick. **Nachgesehen habe ich es nicht**, also steht es hier als Vermutung
   mit ihrem Grund, nicht als Befund.

# Die Schlitzfälle gelöst — und das Ausweichen war eine Verbesserung

Stand 24.10.2026, Abschluss. **PCB-DRC: 0 Abstandsfehler.**

## C3 und C5 konnten ausweichen, und zwar nach innen

Erst nachgesehen, **was sie sind**: `schematic.md` Zeile 165/166 —
**C3 = 4,7 µF an `VSYS`, C5 = 4,7 µF an U1s VDD** (Nordic C_VDD, Abschn.
5.3.8.1). Das sind die **Vorratskondensatoren**, nicht die kurzen 100 nF; die
lokalen sind C4 (VSYS) und C12 (nahe U3 Pin 8), und die kollidierten nicht.

Dann der Fund, der die ganze Abwägung erledigt hat: **die Unterseite unter U1
ist leer.** U1 sitzt oben; sein Körper versperrt unten nichts. Im Streifen
`x` 10 … 23, `y` 12,5 … 20,5 liegt auf der Unterseite **kein einziges Bauteil**
— und die Freihaltezone beginnt erst bei `x` 23,1484, also dahinter.

| | vorher | nachher | gemessen Pad zu Pad |
| --- | --- | --- | --- |
| **C5** (U1 VDD) | 1,4989 / 23,9 | **13,5 / 15,70** | zu Pad 28: **0,75 mm** statt 13,36 |
| **C3** (VSYS) | 3,9497 / 23,9 | **13,5 / 13,85** | zu Pad 30: **1,29 mm** statt 12,11 |

> **Das Ausweichen ist kein Zugeständnis.** Die beiden Kondensatoren standen
> rund 13 mm von ihrem Pin entfernt und stehen jetzt **unter einem Millimeter**
> davon. Die Frage war, ob sie sich von ihrem Baustein entfernen müssen — sie
> kommen ihm **zwölf Millimeter näher.**

Luft zwischen beiden 0,951 mm, zu U3 2,451 mm. SW1 ist nicht bewegt worden,
X1 und L1 stehen unverändert.

## Was dabei von außen hereinkam

**Der J4-Import ist erfolgt** — offenbar hat Thomas „Apply Changes" geklickt,
während ich arbeitete. Belegt, nicht vermutet: Die Platine trägt jetzt
`J4` = **FH12-12S-0.5SH(55), C88360, 14 Pads**, mit **neuer** primitiveId
(`399480cb1723d3d8` statt `42c7d91f37824fa2`), und **der Netzlistenfehler ist
verschwunden**. Damit ist auch meine Vermutung von vorhin bestätigt: der eine
Netzlistenfehler war der ausstehende Import.

Das neue J4 landete bei `y` −10,3, also **unter** der Platine. Es steht jetzt
bei 34,5 / 36,0, neben J2, J5 und J6 — alle vier Platzhalter beieinander.
`J5` ist dabei um 2,5 mm nach oben gerückt, weil sein Schlitzbereich auf
`J2_2` lag.

## Zwei Dinge, die ich beim Messen über das Messen gelernt habe

**Nach dem Verschieben ist die Füllung veraltet.** Als C3 und C5 unter U1
standen, meldete die DRC `Copper Region(Filled) to SMD Pad, distance is 0mm` —
das Kupfer stammte noch von vorher. Erst ein erneuter Aufbau räumte es weg.
**„Gefüllt" ist kein Zustand, der hält; er gilt für eine Anordnung.**

**Mein eigenes Prüfskript schlug falschen Alarm.** Es meldete 14 Pads zu nah am
Rand — alles Pads von `J4`. Grund: die Liste der geparkten Teile im Skript
kannte nur J2, J5 und J6; **J4 kam neu dazu und galt dem Skript als auf der
Platine.** EasyEDAs DRC sagt 0. Die Zahl war richtig gerechnet und auf die
falsche Menge angewendet.

## Stand

| | |
| --- | --- |
| Abstandsfehler (EasyEDA-DRC) | **0** |
| Paare unter 0,152 mm (eigene Messung) | **0** |
| kleinster Abstand | 0,2006 mm (`C2.1` ↔ `U4.3`, **gleiches Netz**) |
| fremde Pads in der Freihaltezone | **0** |
| Masseflächen | 4 Lagen, Zone ausgespart |

**178 Verbindungsfehler bleiben — das sind die unverlegten Netze.** Es ist noch
keine einzige Leitung gezogen; das ist der nächste Arbeitsgang, nicht ein
Fehler. Darin enthalten sind die vier Massepads an U2 und C17, die
Durchkontaktierungen brauchen, und die Pins der vier geparkten Steckverbinder.

## Offener Punkt: C4 hat denselben Fehler, und zwar schärfer

Aus der Berichtigung an C3/C5 folgt eine Frage, die ich **nicht** mehr geprüft
habe: **`C4` = 100 nF an `VSYS` steht weiter in der alten Reihe**, bei
5,9689 / 23,9. Zu U1s `VDDH` (Pad 30, 12,04976 / 14,89964) sind das

> √(6,0809² + 9,0004²) = **10,86 mm.**

**Das ist der Abblockkondensator, nicht der Vorrat.** Bei ihm zählt der kurze
Weg mehr als bei den 4,7 µF, die heute von 13 mm auf unter 1 mm gerückt sind.
Dass C3 jetzt 1,29 mm von Pad 30 entfernt ist und C4 **10,86 mm**, ist die
Reihenfolge verkehrt herum.

**Nicht bewegt**, weil der Abend zu Ende war und weil es eine eigene Runde
verdient: Es gehört geprüft, ob **alle** Abblockkondensatoren bei ihrem Pin
stehen, nicht nur dieser eine. Geprüft und in Ordnung sind bisher nur
`C10`/`C11` an U2, `C12` an U3 und `C13`/`C14` an Q4 — die stehen jeweils rund
einen Millimeter entfernt. **Die anderen hat niemand nachgemessen.**

**Der Platz ist da:** die Unterseite unter U1 ist in `x` 10 … 23,
`y` 12,5 … 20,5 weiterhin fast leer.

# Massepads angebunden — drei von vier, und warum der vierte wartet

Stand 24.10.2026, nachts. **0 Abstandsfehler. Verbindungsfehler 178 → 175.**
Verlegt ist noch nichts; das hier ist nur die Masseanbindung.

## U2: über die Kühlfläche, ohne Durchkontaktierung an den Pins

`U2_1`, `U2_17` und `U2_19` liegen an einem QFN mit **0,4 mm Raster** — dort
kommt kein Gießkupfer zwischen die Pads. Statt drei Durchkontaktierungen in
eine Lücke zu quetschen, die es nicht gibt:

- **eine Durchkontaktierung in die Kühlfläche** (Pad 21, selbst `GND`,
  1,9 × 1,9 mm) bei 21,99894 / 4,50088, 0,61 / 0,305 mm;
- **drei kurze Bahnen** von den drei Pins auf die Kühlfläche, 0,2 mm breit,
  auf der Unterseite.

Die Lücke zwischen Padkranz und Kühlfläche ist **0,267 mm**; eine Bahn von
0,2 mm hält darin zu den Nachbarpads **0,199 bzw. 0,200 mm** — über der
Vorschrift von 0,152. Vorher gerechnet, danach gemessen: **alle drei sind aus
der Fehlerliste verschwunden.**

Dazwischen lag ein `doc_reload`: EasyEDA verbindet neue Durchkontaktierungen
auf einer Vier-Lagen-Platine erst nach erneutem Öffnen. Danach die Flächen neu
aufgebaut, weil neue Bahnen die Füllung veralten lassen.

## C17_2: drei Versuche, drei messbare Gründe, nichts eingebaut

| Versuch | Ergebnis |
| --- | --- |
| Durchkontaktierung bei 25,3 / 9,4996 + kurze Bahn | Pad verbunden, **aber die Durchkontaktierung selbst gilt als unverbunden** |
| daraus gefolgert: das Kupfer rechts von C17 ist eine **abgetrennte Insel** | EasyEDA entfernt eingeschlossene Inseln beim Füllen |
| stattdessen L-Bahn zu `C1_2`, einem nachweislich verbundenen Massepad | Pad verbunden, **aber die Bahn läuft durch SW2s Schlitzbereiche** |

**Der dritte Versuch ist derselbe Fehler, den ich eine Stunde vorher selbst
beschrieben hatte.** SW2 steht seit heute Abend genau dort, und seine Schlitze
liegen bei rund `x` 22,6 und 24,42 auf `y` 8,6 — mitten im einzigen Weg, den
ich gesucht hatte.

**Alles zurückgebaut.** `C17_2` hängt wieder, wie vorher — das ist der ehrliche
Zustand, nicht ein verlegter Umweg, den morgen niemand versteht.

> **Was mir dafür fehlt, und das ist der eigentliche Befund:** Ich kann die
> **Geometrie der Schlitzbereiche nicht auslesen** — kein Werkzeug der Brücke
> gibt sie her; ich erfahre sie nur, wenn die DRC sie als Fehler meldet. Und
> ich kann **nicht sehen, welche Teile einer Gießfläche zusammenhängen**. Beides
> sind Dinge, die man beim Verlegen ständig braucht. Solange ich sie nur über
> Fehlermeldungen erfahre, ist jedes Verlegen in dieser Ecke Versuch und Irrtum.

**Weg für morgen, der ohne beides auskommt:** `C17` ein Stück nach links
versetzen, aus der toten Ecke heraus in das Feld, in dem die Fläche
nachweislich trägt — dann braucht es weder Bahn noch Durchkontaktierung. Das
ist eine Platzierungsänderung und gehört an den Anfang einer Runde, nicht ans
Ende einer Nacht.

## Reihenfolge für die nächste Runde (festgelegt 24.10.2026, nachts)

**In einem Zug, vor dem Verlegen:**

1. **Jeden Kondensator gegen seinen Pin messen**, mit Zahl. Wohin welcher
   gehört, steht in `schematic.md`. Nachgemessen sind bisher nur C10/C11 an U2,
   C12 an U3, C13/C14 an Q4 sowie C3/C5 an U1 — **alle übrigen nicht.**
   Bekannt zu weit: **C4 mit 10,86 mm.**
2. **C17 dabei mitnehmen.** Es steht in einer toten Kupferecke und soll nicht
   angebunden, sondern **versetzt** werden. Wenn ohnehin alle Kondensatoren
   geprüft werden, fällt sein Platz in derselben Runde ab.

Der Streifen unter U1 (`x` 10 … 23, `y` 12,5 … 20,5, Unterseite) ist frei und
hat sich bei C3 und C5 bereits als der richtige Platz erwiesen.

**Erst danach verlegen**, mit den vier Auflagen: J2/J4/J5/J6 auslassen, I2C auf
eine Innenlage zwischen die Masseflächen, Wandlerschleife L1 → DCCH kurz und
breit, und nach jedem Schritt messen.

# Alle Kondensatoren gegen ihren Pin gemessen

Stand 25.10.2026. Zuordnung **aus `schematic.md` gelesen**, nicht geraten;
die Zeilennummer steht bei jedem. Gemessen ist das **nicht-Masse-Pad** des
Kondensators gegen das Zielpad, Mitte zu Mitte.

| Kond | gehört an | Quelle | vorher | **jetzt** |
| --- | --- | --- | ---: | ---: |
| C17 | U1.20 `VBAT_SENSE` | Z. 696 | 14,88 | **0,00** |
| C4 | U1.30 `VSYS` | Z. 165 | 11,10 | **0,12** |
| C5 | U1.28 U1 VDD | Z. 166 | 0,75 | 0,75 |
| C11 | U2.18 `SE_VOUT` | Z. 233 | 2,51 | **0,83** |
| C12 | U3.8 `VDD3V3` | Z. 313 | 9,77 | **0,98** |
| C2 | U4.3 `VBAT_RAW` | Z. 101 | 1,28 | 1,28 |
| C3 | U1.30 `VSYS` | Z. 165 | 1,29 | 1,29 |
| C1 | U4.4 `VBUS` | Z. 99 | 18,59 | **1,47** |
| C8 | X1.1 | Z. 188 | 3,43 | **1,65** |
| C9 | X1.2 | Z. 189 | 4,34 | **1,65** |
| C10 | U2.12 `VDD3V3` | Z. 232 | 5,24 | **2,38** |
| C13 | Q4.3 `VDD_OLED` | Z. 357 | 2,97 | 2,97 |
| C14 | Q4.3 `VDD_OLED` | Z. 357 | 3,97 | 3,97 |
| C16 | U1.54 `NFC2` | Z. 556 | 42,40 | 42,40 |
| C15 | U1.52 `NFC1` | Z. 555 | 43,46 | 43,46 |

**Acht versetzt, in einem Zug, vorher durchgerechnet und danach gemessen:**
0 Paare unter 0,152 mm, 0 Pads am Rand, Freihaltezone frei, **0
Abstandsfehler**. C1, C4, C8, C9, C10, C11, C12, C17.

**Nur drei standen nah genug. Zwölf nicht.** Dass C3 und C5 zwölf Millimeter
danebenlagen, haben wir durch einen Taster erfahren; dass C1 **18,59 mm** von
U4 stand, hätte so niemand bemerkt.

## Drei bleiben stehen, jeder aus einem anderen Grund

**C10 mit 2,38 mm — geometrisch, nicht aus Nachlässigkeit.** Pin 12 liegt an
U2s **unterer** Kante (`y` 2,949), und 0,35 mm darunter beginnt die
Widerstandsreihe bei `y` 2,0. Dort passt kein 0402 (0,541 hoch). C10 steht
jetzt links neben U2 statt rechts unten. **Näher ginge nur, wenn die
Widerstandsreihe zum Rand rückt** — das ist Platz, der da wäre (`y` 0,9 statt
2,0), aber eine eigene Änderung.

**C13 und C14 mit 2,97 und 3,97 — das Ziel ist geparkt.** Sie blocken
`VDD_OLED` ab, und der Verbraucher ist die Anzeige an **J4**, das neben der
Platine steht. Gemessen ist gegen Q4, die Quelle. **Die Zahl wird erst
belastbar, wenn J4 einen Platz hat**; vorher zu verschieben hieße, auf einen
Punkt zu optimieren, der sich noch bewegt.

**C15 und C16 mit 42 und 43 mm — das ist eine Schaltungsfrage, keine
Platzierung.** Sie stehen neben der Antennenbuchse J3 (`y` 52,5), U1s
NFC-Pins liegen bei `y` 13,5. Nordics Vorgabe setzt die Abstimm­kondensatoren
**an die NFC-Pins des Chips**. Wo sie stehen, entscheidet über die ganze
Abstimmung — und `schematic.md` nennt sie ohnehin `TBD`, weil die Abstimmung
eine Messung ist. **Nicht bewegt, gemeldet.**

# Die Zahl 4,45 gibt es zweimal — eine Breite und eine Höhe

**Gesucht war ein Platz für einen 5,5 mm hohen Akkustecker. Gefunden wurde ein
Fehler in der Grenze selbst.**

Die Zahl 4,45 kommt in dieser Datei sechsmal vor. **Hergeleitet wird sie genau
einmal**, in der Tabelle zur Zellenbreite:

> `| Adafruit mittig (19,75) | 19,75 | ja | **4,45 mm**, je Seite 2,225 |`

Das ist die **seitliche Restbreite** neben einer 19,75 mm breiten Zelle in
einem nutzbaren Band von 24,20 mm. 24,20 − 19,75 = 4,45, aufgeteilt auf zwei
Seiten zu je 2,225 mm. **Eine Breite.**

An fünf weiteren Stellen steht dieselbe Zahl als **Bauhöhe** — „jedes Bauteil
neben der Zelle, das höher als 4,45 mm baut", „baut J2 höher als 4,45 mm?",
„Grenze 4,45 mm", und zuletzt von mir selbst: „Die Grenze ist 4,45 mm. Der PH
liegt 1,05 mm darüber."

> **Keine dieser Stellen leitet eine Höhe her. Die Breite ist zur Höhe
> geworden, weil beide dieselbe Zahl tragen.**

**Was wirklich belegt ist:** Die Bauhöhe unten richtet sich nach der **Zelle
mit 3,8 mm** (Z. 100). Damit liegt der PH-Stecker nicht 1,05 mm über der
Grenze, sondern **1,70 mm über der Zelle** — und der SH mit 2,9 mm bleibt
**0,90 mm darunter**. Die Rangfolge der beiden Teile ändert sich nicht, die
Zahlen schon.

**Und die Frage nach einem Platz für 5,5 mm ist mit dem Repo nicht zu
beantworten.** Was die Hohlraumtiefe je Zone ist, steht nirgends — nur die
3,8 mm der Zelle. Ob das Gehäuse in Z1, Z2 oder Z4 unten tiefer ist als in Z3,
weiß nur der Gehäuse-Strang, und der ist archiviert. **Ich kann sagen, dass
5,5 mm die Zelle um 1,7 mm überragen. Ich kann nicht sagen, wo das erlaubt
ist.**

Daneben: `4,4501 mm` steht in dieser Datei noch einmal, als Band zwischen
U1-Körper und Anzeigeglas. **Das ist dieselbe Zahl für etwas drittes** und hat
mit der Zelle nichts zu tun.

# Drei weitere Masse-Inseln

Nach dem Versetzen hängen `R14_2` (26,4 / 2,0), `U1_1` (22,95 / 21,95) und
`C14_2` (21,52 / 33,6) ohne Verbindung zur Gießfläche. **`C17_2` hängt nicht
mehr** — das Versetzen hat es gelöst, wie vorhergesagt.

`U1_1` ist der interessante Fall: Es liegt bei `x` 22,75 … 23,15, und die
Freihaltezone beginnt bei **23,1484**. Das Kupfer endet also genau an der
Padkante. Es ist eines der beiden Massepads am Antennenende — **das muss an
Masse**, und Raytac verlangt dort ausdrücklich Durchkontaktierungen.
Nächster Arbeitsgang.

## Berichtigung meiner Berichtigung, 25.10.2026

**Der Abschnitt darüber war zur Hälfte falsch, und zwar in der Hälfte, die
zählt.** Ich hatte geschlossen, die 4,45 mm seien *nur* eine Breite und als
Bauhöhe nirgends hergeleitet. **Sie ist hergeleitet — in `cad/masse.md`:**

> Zeile 43: `| Unten | Ferritfolie 0,4 + Zellenreserve 4,05 · U2 0,32 · U3 Flash | 4,45 mm |`
> Zeile 88: `Wer die 9,55 mm will, ändert eine Zeile (h_bot_part von 4,45 auf 4,20)`

**0,4 + 4,05 = 4,45**, und `h_bot_part` ist ein Höhenparameter im
Gehäusemodell. Das ist eine Bauhöhe, sauber gerechnet.

| Zahl | bedeutet | hergeleitet in |
| --- | --- | --- |
| 4,45 mm | **Bauhöhe** der Unterseite: Ferritfolie 0,40 + Zellenreserve 4,05 | `cad/masse.md`, Z. 43 |
| 4,45 mm | **Restbreite** neben der Zelle: 24,20 − 19,75, je Seite 2,225 | `layout-notes.md` |
| 4,65 mm | **Hohlraumtiefe**: 0,2 Luft + 0,4 Folie + 4,05 Zelle | `cad/masse.md`, Z. 398 |
| 4,4501 mm | Band zwischen U1-Körper und Anzeigeglas | `layout-notes.md` |

**Viermal fast dieselbe Zahl für vier verschiedene Größen.** Dreimal exakt
4,45 — und zwei davon sind unabhängig voneinander richtig.

> **Warum ich die Höhe nicht gefunden habe:** `cad/` liegt **nicht auf dem
> Zweig `platine`**. Mein Arbeitsbaum hat die Datei nicht; ein `grep` über
> alles, was ich sehe, findet sie nie. **Ich habe „kommt in dieser Datei nicht
> vor" mit „ist nirgends hergeleitet" verwechselt** — und genau davor warne ich
> seit zwei Tagen bei fremden Zahlen.
>
> Die Lehre ist nicht „besser suchen", sondern: **Wenn eine Zahl aus einem
> anderen Strang stammt, liegt ihre Herleitung wahrscheinlich auch dort.**
> Dateien, die ich nicht im Arbeitsbaum habe, erreiche ich mit
> `git show origin/main:cad/masse.md` — so habe ich sie am Ende auch gelesen.

**Für J2 gilt eine dritte Zahl, keine der beiden:** Maßgeblich ist die
**Hohlraumtiefe von 4,65 mm**. Der JST-PH mit 5,5 mm ragt also **0,85 mm**
darüber hinaus — nicht 1,05, nicht die 1,70, die ich gerechnet hatte. Der SH
mit 2,9 mm bleibt **1,75 mm darunter**. Die Rangfolge der beiden Teile hat
sich nie geändert, die Zahl dreimal.

# C15/C16 an die NFC-Pins, Reihe an den Rand, C10 nach — und ein Methodenfehler

Stand 25.10.2026. **0 Abstandsfehler.** Verlegt ist nichts.

## Acht weitere versetzt

| Teil | von | nach | Abstand zum Pin |
| --- | --- | --- | --- |
| **C15** | 10,50 / 55,50 | **21,50 / 12,60** | 43,46 → **1,02** zu U1.52 |
| **C16** | 16,50 / 55,50 | **21,50 / 13,50**, 180° | 42,40 → **0,33** zu U1.54 |
| **C10** | 19,60 / 2,949 | **22,400 / 1,95** | 2,38 → **1,08** zu U2.12 |
| R3, R4, R5, R13, R14 | `y` 2,000 | **`y` 0,900** | macht den Platz für C10 frei |

Die Widerstandsreihe hält am Rand `y` 0,63 — die Vorschrift sind 0,30. Zu J1s
Schirm bleiben die 0,2505 mm von gestern, zu U2 jetzt 1,45 mm.

**C15 und C16 standen an der Antennenbuchse, 43 mm von den NFC-Pins.** Jetzt
stehen sie unter dem Modul an Pin 52 und 54. Die NFC-Leitungen laufen dafür
die 40 mm zur Buchse — das ist die richtige Richtung: **die Abstimmung gehört
an den Chip, die Antenne darf weit weg sein.**

## Der Methodenfehler, und er entwertet eine frühere Schlussfolgerung

Ich hatte geschrieben, `U1_2`, `U1_15`, `U1_33` und `U1_55` seien verbunden,
**weil sie nicht in der Fehlerliste stehen.** Das ist falsch:

> **Die DRC nennt je zusammenhängender Insel genau einen Vertreter, nicht jedes
> Pad darin.** „Nicht gelistet" heißt also nicht „verbunden" — es kann auch
> heißen „liegt auf derselben Insel wie der, der genannt ist".

Bewiesen am eigenen Fall: Nach einer Bahn von `U1_1` zu `U1_2` blieb der
Eintrag `U1_1` stehen. Nach einer zweiten Bahn nach unten **verschwand `U1_1`
und an seiner Stelle stand `Track e8`** — dieselbe Insel, ein anderer
Vertreter. Die Zahl 175 ist damit **keine Zahl unverbundener Pads, sondern eine
Zahl unverbundener Inseln.**

Das erklärt auch, warum 175 über mehrere Reparaturen hinweg gleich blieb:
gelöste Inseln und neu entstandene hoben sich auf.

## U1_1 bleibt offen, und diesmal weiß ich mehr

Drei Versuche, jeder gemessen: Durchkontaktierung **im** Pad (0,35/0,20),
Bahn zu `U1_2` auf der Oberseite, Bahn nach unten ins offene Feld. **Keiner
verbindet.** Der dritte zeigte dafür, woran es liegt:

- **Auf der Oberseite ist unter U1 überhaupt kein Gießkupfer.** Im Bild ist die
  ganze Modulfläche schwarz. Zwischen Pads im 0,8-mm-Raster bleibt nach allen
  Freiräumen nichts übrig, was zusammenhängt.
- **Unten ebenso wenig**, denn meine Bahn von `U1_1` nach `y` 19,5 wurde selbst
  als unverbunden gemeldet. Dort, wo ich offene Fläche vermutet hatte, ist
  keine.

**Warum das Kupfer dort fehlt, weiß ich nicht** — ich kann nicht sehen, welche
Teile einer Gießfläche zusammenhängen, und das Bild zeigt nur die Oberseite.
Die Bahn nach unten habe ich zurückgebaut; **Durchkontaktierung und die kurze
Bahn `U1_1`–`U1_2` bleiben**, denn sie sind für sich richtig und Raytac
verlangt an den Ecken ohnehin Durchkontaktierungen.

**Offen, mit Vertreter:** `U1_1` (Antennenende), `SW2_2'`, `C14_2` sowie die
Pins der vier geparkten Steckverbinder.

## Vier Versuche an `U1_1`, und was jeder gezeigt hat

Masse zuerst — und sie geht an derselben Stelle nicht weiter. Alle vier
Versuche gemessen, keiner verbindet; **zurückgebaut ist alles bis auf die
Durchkontaktierung im Pad und die kurze Bahn `U1_1`–`U1_2`.**

| Versuch | Ergebnis | was es ausschließt |
| --- | --- | --- |
| Durchkontaktierung **im** Pad, 0,35/0,20 | keine Verbindung | nicht die Lage „daneben" |
| Bahn `U1_1` → `U1_2` (oben) | Eintrag blieb `U1_1` | die Insel wuchs nur |
| Bahn `U1_1` → `y` 19,5 (unten) | Eintrag wurde **`Track e8`** | unten ist dort kein Kupfer |
| Durchkontaktierung bei 22,8 / 20,5 im offenen Feld + Bahn dorthin | keine Verbindung | auch die Innenlagen nicht |

**Gegenbeweis, der das Rätsel schärft statt es zu lösen:** `C17` hängt nicht
mehr, seit es bei **11,63 / 18,90** steht — und das ist **ebenfalls unter U1**
(der Modulkörper reicht von `x` 11,5 bis 27). Die Fläche trägt dort also. Bei
`x` 22,8 im selben Band trägt sie nicht.

**Was fehlt, ist kein Einfall, sondern ein Messmittel:** Ich kann nicht sehen,
welches Kupfer auf welcher Lage zusammenhängt. Das Vorschaubild zeigt nur die
Oberseite, `pcb_pours_list` nur Umrisse, und die DRC nennt je Insel einen
Vertreter — **ein Werkzeug, das mir die gefüllten Flächen je Lage zeigt, würde
die Frage in einem Blick beantworten.**

**Nicht blockiert ist der Rest:** Die Signalnetze hängen an dieser Frage nicht.
`GND` hat die Flächen; was fehlt, sind drei Inseln.

# Am Gerber gemessen: die Masse unter dem Modul ist da

Stand 25.10.2026. **Berichtigung einer Behauptung von mir**, und diesmal einer,
die ich aus einem Bild abgelesen hatte.

## Was ich behauptet hatte, und warum es falsch war

> „Auf der Oberseite ist unter U1 überhaupt kein Gießkupfer. Im Bild ist die
> ganze Modulfläche schwarz."

**Das Schwarze im Bild ist die Footprint-Zeichnung von U1, nicht fehlendes
Kupfer.** Ich habe aus einer Darstellung auf einen Zustand geschlossen — genau
der Fehler, den ich seit zwei Tagen bei fremden Zahlen anstreiche. Ein Bild
zeigt, was gezeichnet wird, nicht was gefertigt wird.

## Die Quelle, die zählt

**Gerber-Export** — das ist, was der Fertiger bekommt. Je Lage **eine** gefüllte
Region (`G36`…`G37`), bei BOTTOM 1575 Stützpunkte über 1576 Zeilen, mit genau
**einem** `D02`: eine einzige geschlossene Kontur, die sich um jede Aussparung
herumwebt. Bögen (`G02`/`G03`) habe ich mit je acht Sehnen angenähert.

**Erst den Test geprüft, dann ihm geglaubt.** Je Lage fünf Kontrollpunkte,
deren Antwort vorher feststand:

| Kontrolle | erwartet | TOP | BOTTOM |
| --- | --- | --- | --- |
| Pad fremden Netzes (`U1_3` / `R1_1`) | frei | frei ✓ | frei ✓ |
| zweites Pad fremden Netzes (`U1_31` / `C4_1`) | frei | frei ✓ | frei ✓ |
| **Freihaltezone 25,0 / 17,0** | **frei** | **frei ✓** | **frei ✓** |
| offenes Feld 5,0 / 40,0 | Kupfer | Kupfer ✓ | Kupfer ✓ |
| Massepad (`R1_2`) | Kupfer | – | Kupfer ✓ |

Eine erste Fassung des Tests schlug auf BOTTOM fehl — weil ich als Kontrolle
`U1_3` genommen hatte, ein **Oberseiten**-Pad. Unten darf dort Kupfer sein.
**Die Kontrolle war falsch, nicht der Test.**

## Das Ergebnis

| Punkt | TOP | INNER_1 | INNER_2 | BOTTOM |
| --- | --- | --- | --- | --- |
| unter U1, links (13 / 20) | Kupfer | Kupfer | Kupfer | Kupfer |
| unter U1, rechts (22 / 16) | Kupfer | Kupfer | Kupfer | Kupfer |
| `U1_1` Padmitte (22,949 / 21,951) | **Kupfer** | Kupfer | Kupfer | Kupfer |
| 22,8 / 20,5 | Kupfer | Kupfer | Kupfer | Kupfer |
| **Freihaltezone (25 / 17)** | **frei** | **frei** | **frei** | **frei** |

> **Raytacs Vorschrift ist erfüllt: unter dem Modul liegt auf allen vier Lagen
> Masse, und die Antennenzone ist auf allen vier Lagen frei.** Das war die
> Sorge, und sie ist ausgeräumt — an der Datei, die der Fertiger bekommt.

## Was damit offen bleibt

**Die DRC nennt `U1_1` weiter unverbunden, obwohl auf seinem Pad Kupfer liegt.**
Die beiden widersprechen sich. Möglich ist beides: eine haarfeine Trennung, die
mein Test mit acht Sehnen je Bogen nicht auflöst — oder ein Modellfehler in
EasyEDAs Verbindungsprüfung.

**Für die Fertigung ist die Lage gut**, für die Prüfung nicht geklärt. Ich
würde daraus **keinen** Anlass machen, vor dem Verlegen noch etwas umzubauen:
Was fehlte, wäre Kupfer, und das ist da.

**Mein Messmittel ist jetzt ein anderes:** Der Gerber-Export beantwortet
„liegt dort Kupfer?" je Lage — besser als jedes Vorschaubild. Die drei
Kontrollpunkte je Lage gehören zu jeder solchen Messung dazu.

# Berichtigung: die Wandlerschleife ist 10,14 mm, nicht 2,59

Stand 25.10.2026, vor dem Verlegen gemessen.

**Ich hatte gemeldet: „`L1.1` (DCCH) steht jetzt 2,59 mm in `x` von U1s
DCCH-Pad."** Das war richtig — und als Abstand gelesen falsch. **2,59 ist die
`x`-Komponente**; in `y` liegen **9,80 mm** dazwischen.

| Netz | von | nach | dx | dy | **Abstand** |
| --- | --- | --- | ---: | ---: | ---: |
| `DCCH` | L1.1 | U1.31 | −2,59 | −9,80 | **10,14** |
| `VSYS` | L1.2 | U1.30 | −4,93 | −9,00 | **10,26** |

**Die Schleife des Abwärtswandlers ist also rund 20 mm lang, hin und zurück.**
Für „kurz und breit" ist das nicht knapp, sondern das Gegenteil.

> **Das ist dieselbe Falle wie dreimal zuvor, nur von mir ausgelöst:** eine
> Zahl, die eine Achse misst, als Abstand gemeldet. Bei `R1`↔`U4` hatte ich
> noch selbst geschrieben, wer nur eine Achse ansieht, halte das für ungelöst —
> und dann habe ich genau das getan, in der anderen Richtung.

## Der Platz dafür ist da, und er ist derselbe wie bei C3 und C5

Die Unterseite unter U1 ist frei. L1 misst 3,97 × 2,16 mm; bei **16,8 / 14,3**
stünde `L1.1` **3,60 mm** von U1.31 und `L1.2` 5,94 mm von U1.30 — die Schleife
fiele von **20,4 auf 9,5 mm**. Luft zu `C5` 0,22 mm, zu `C3` ebenso; beide
bleiben, wo sie sind.

**Nicht ausgeführt** — L1 steht in der Quarzreihe, und über die Reihe war
entschieden. Es dort herauszunehmen stört den Quarz nicht, sondern macht Platz;
aber es ist eine Platzierungsänderung und keine Verlegearbeit.

## Die übrigen kritischen Netze, gemessen

| Netz | Abstand | Bemerkung |
| --- | ---: | --- |
| `XL1` X1.1 → U1.17 | 3,71 | mit C8 dazwischen bei 2,62 |
| `XL2` X1.2 → U1.18 | 4,32 | mit C9 bei 2,67 |
| `USB_DM` / `USB_DP` | 5,67 / 5,65 | **auf 0,02 mm gleich** — das Paar passt |
| `USB_DM_CON` / `_DP_CON` | 3,82 / 3,82 | zu J1, exakt gleich |
| `I2C_SCL` | 10,52 | kommt auf eine Innenlage |
| `I2C_SDA` | 18,53 | dto.; lang, aber langsam |
| `SE_ENA` | 19,08 | langsam, unkritisch |

# L1 unter U1: Schleife von 20,4 auf 9,54 mm

Stand 25.10.2026. **0 Abstandsfehler.** Vorher durchgerechnet, danach gemessen.

| Teil | von | nach |
| --- | --- | --- |
| **L1** | 15,8089 / 23,90 | **16,80 / 14,30** |
| **C3** | 13,50 / 13,85 | **13,30 / 13,85** |
| **C5** | 13,50 / 15,70 | **13,30 / 15,70** |

| Weg | vorher | **nachher** |
| --- | ---: | ---: |
| `DCCH` L1.1 → U1.31 | 10,14 | **3,587** |
| `VSYS` L1.2 → U1.30 | 10,26 | **5,949** |
| **Schleife zusammen** | **20,40** | **9,536** |

## Auflage 1: der Abstand zur Antennenzone, und wie er gemessen ist

**Gemessen von L1s äußerster Kupferkante zur Kante der Freihaltezone**, nicht
von Mitte zu Mitte:

> L1s rechtes Pad endet bei `x` **18,7865**. Die Zone beginnt bei `x`
> **23,1484**. → **4,3619 mm.**

Das ist der knappste Weg in der Plattenebene. Dazu, als Tatsache und nicht als
Beruhigung: **L1 liegt unten, die Antenne oben, dazwischen 0,8 mm Platte und
die beiden Innenlagen** — und dass dort auf allen vier Lagen Kupfer liegt, ist
am Gerber belegt, nicht angenommen.

**Dass L1 ungeschirmt ist, bleibt `ungeprueft`** (TDKs Seite antwortet mit
HTTP 403). Genau deshalb steht die Zahl hier.

## Auflage 2: mehr Luft — und zwar ohne Preis

**Die Frage war, ob L1 ein Zehntel weiter weg kann, ohne die Schleife zu
verlängern. Die Antwort ist: L1 nicht, C3 und C5 schon.**

Bei L1 ist die Luft allein von `x` bestimmt: weiter nach links geht nicht an
C3/C5 vorbei, weiter nach rechts kostet Schleife — **0,2 mm Schleife je 0,1 mm
Luft.** Also habe ich die Kondensatoren bewegt statt der Spule:

| | vorher | **nachher** |
| --- | ---: | ---: |
| Luft L1 ↔ C3 | 0,2125 | **0,4125** |
| Luft L1 ↔ C5 | 0,2125 | **0,4125** |
| C3.1 → U1.30 | 1,29 | **1,184** |
| C5.1 → U1.28 | 0,75 | **0,549** |

**Fast das Dreifache der Vorschrift (0,152), und beide Kondensatoren stehen
dabei näher an ihrem Pin als vorher.** Die Schleife bleibt bei 9,536 mm.

Die Quarzreihe verliert L1 und gewinnt dadurch Platz; X1, C8 und C9 stehen
unverändert.

# Verlegt: das USB-Paar vom Schutzbaustein zum Modul

Stand 25.10.2026. **0 Abstandsfehler, Verbindungsfehler 175 → 171.**

## Erst ein Befund: das Paar kreuzte sich

| | links | rechts |
| --- | --- | --- |
| U5 (vorher) | `USB_DP` | `USB_DM` |
| U1 Pads 34/35 | **`USB_DM`** | **`USB_DP`** |
| J1 A-Reihe (A6/A7) | `USB_DP` | `USB_DM` |
| J1 B-Reihe (B6/B7) | **`USB_DM`** | **`USB_DP`** |

**U5 war gegenüber dem Modul spiegelverkehrt.** Ein gekreuztes Differenzpaar
ist keine Verlegefrage, sondern eine Drehung: **U5 um 180° gedreht**, Lage
unverändert 13,500 / 8,300. Danach liegt `DM` auf beiden Seiten links und `DP`
rechts — **keine Kreuzung mehr zwischen U5 und U1.**

> **Zur Steckerseite bleibt eine Kreuzung, und die ist nicht wegzudrehen:**
> J1s A-Reihe ist spiegelverkehrt zur B-Reihe — so ist USB-C gebaut, damit der
> Stecker in beiden Lagen passt. `A6`↔`B6` und `A7`↔`B7` müssen beide
> verbunden werden, und die zwei Diagonalen kreuzen sich zwangsläufig.
> **Das gehört auf zwei Lagen**, eine Diagonale oben, eine unten. Steht noch
> aus.
>
> Zwischen den Pads der A-Reihe ist kein Weg: Raster 0,8484 mm, Pad 0,65 →
> **0,201 mm Lücke**, eine 0,2er Bahn bräuchte mit Abständen 0,504.

## Gelegt

| Netz | Weg | Breite |
| --- | --- | --- |
| `USB_DM` | U5.4 (12,5499/9,4499) → unten → Durchkontaktierung 13,0505/11,60 → oben → U1.34 | 0,20 |
| `USB_DP` | U5.6 (14,4499/9,4499) → unten → Durchkontaktierung 13,8506/11,60 → oben → U1.35 | 0,20 |

**Beide gleich aufgebaut**, damit das Paar gleich lang bleibt: gleiche
Lagenfolge, je eine Durchkontaktierung (0,50/0,25) auf gleicher Höhe, Abstand
der beiden 0,80 mm (Luft 0,30). Nach den Durchkontaktierungen `doc_reload`,
dann Flächen neu aufgebaut, dann gemessen.

# Das Messwerkzeug: wo es liegt und wie es läuft

**`D:\claude\working\opensecurekey-platine-werkzeuge\`**, mit `LIESMICH.md`.

```
python gerber-kupfer.py <Gerberdatei> <x> <y> [<x> <y> ...]
```

Beantwortet **„liegt an dieser Stelle Kupfer?"** am Gerber-Export — der Datei,
die der Fertiger bekommt. Je Lage eine Datei (`Gerber_TopLayer.GTL`,
`Gerber_InnerLayer1.G1`, `…2.G2`, `Gerber_BottomLayer.GBL`), zu holen mit
`pcb_export_gerber` und auspacken.

**Immer mit Kontrollpunkten aufrufen, deren Antwort vorher feststeht** — einer,
der „frei" ergeben muss (ein Pad fremden Netzes), und einer, der „KUPFER"
ergeben muss (offenes Feld). Ohne diese zwei Zahlen ist das Ergebnis keine
Messung. Beispiel für diese Platine:

```
python gerber-kupfer.py Gerber_TopLayer.GTL  25.0 17.0   5.0 40.0
   25,0000 / 17,0000  ->  frei     (Freihaltezone - muss frei sein)
    5,0000 / 40,0000  ->  KUPFER   (offenes Feld - muss Kupfer sein)
```

Daneben liegen `pruef.py` (Pad gegen Pad, MULTI auf beiden Lagen), `kond.py`
(Kondensator zu seinem Pin) und die Skripte, mit denen Verschiebungen **vor**
dem Senden durchgerechnet werden.

**Arbeitskopie:** Der Worktree liegt seit dem 25.10. in
`D:\claude\working\opensecurekey-platine`; `core.fileMode` ist dort auf `false`
gesetzt (Regel aus `repo.md`). Dieser Zweig führt **keine** ausführbaren
Dateien, es war also nichts zu verlieren.

# Was die Platine an Länge verlöre, wenn der Akku kleiner wird

Stand 25.10.2026. **Gerechnet, nicht umgebaut.** Grundlage ist nicht die
Bandtabelle, sondern eine Messung: **wo liegt auf der Platine überhaupt
Kupfer?**

## Die Messung zuerst

Alle Pads, geparkte Steckverbinder ausgenommen, nach belegten `y`-Bändern:

| von | bis | was |
| ---: | ---: | --- |
| 0,456 | 2,221 | J1, Widerstandsreihe, C10 |
| 2,616 | 8,600 | U2, U5, J1-Schirm, Ladepfad |
| 8,913 | 11,699 | Q1/Q2/Q3, U4, C2 |
| 11,878 | **24,799** | U1 samt allem darunter, U3, X1, C8/C9, SW1 |
| 25,599 | **33,870** | SW1-Oberkante, Q4/Q5, R9/R17, C13, C14 |
| 33,870 | 51,500 | **frei — hier liegt die Zelle** (Unterseite) |
| 51,500 | 53,499 | J3, Antennenbuchse |
| 53,499 | 70,000 | Antennenfeld, darin nur R10–R12 und die LED |

> **Die ganze Schaltung endet bei `y` 33,870.** Alles darüber ist Zelle,
> Antennenbuchse und Antennenfeld. **Zwei Drittel der Platine tragen keine
> Schaltung** — darin hat der Master recht.

## Die drei Rechnungen

Die Zelle beginnt bei `y` 25,0 (die Unterseite ist ab 24,799 frei). Maßgeblich
ist, **was weiter reicht: die Zelle oder die Schaltung.**

| Zelle | Länge | endet bei | **Platine** bei heutigem Feld | **Feld**, wenn 70 mm bleiben | Quelle der Maße |
| --- | ---: | ---: | ---: | ---: | --- |
| Adafruit 1317, 150 mAh **(verbaut)** | 26,02 | 51,02 | **70,00** | 18,98 | Datenblatt |
| SparkFun PRT-13853, 110 mAh | 25,00 | 50,00 | **68,98** | 20,00 | Datenblatt |
| *hypothetisch* | 20,00 | 45,00 | **63,98** | 25,00 | **keine** |
| SparkFun PRT-13852, 40 mAh | ~12,00 | 37,00 | **55,98** | 33,00 | **geschätzt** |
| Grenzfall | 8,87 | 33,87 | **52,85** | 36,13 | rechnerisch |

**Zwei Dinge, die aus der Tabelle fallen und wichtiger sind als die Zahlen:**

1. **Unter rund 9 mm Zellenlänge bringt eine kleinere Zelle gar nichts mehr.**
   Dann endet die Schaltung weiter oben als die Zelle, und **die Schaltung ist
   das Maß.** Die Platine wird nicht kürzer als **52,85 mm**, solange die
   Bauteile stehen, wo sie stehen.
2. **Im Repo gibt es keine kürzere Zelle als 25,0 mm mit belegten Maßen.** Die
   untersuchte Tabelle (`docs/parts.md`) führt fünf Zellen; die kürzeste ist
   die SparkFun mit 25,0 mm. Die 40-mAh-Zelle **PRT-13852 kommt dort nur in
   einer Berichtigung vor, ohne ein einziges Maß.** Die Zeile mit 12 mm steht
   auf einer Schätzung, nicht auf einem Datenblatt — **wer darauf plant, plant
   auf einer Zahl, die niemand nachgesehen hat.**

## Was das Antennenfeld dabei macht

Es **wächst**, wenn die Platine lang bleibt: von 18,98 auf bis zu 36,13 mm.
Das ist die bessere Richtung — `layout-notes.md` hält seit dem ersten Entwurf
fest: **„Fläche schlägt Länge."** Wird die Platine stattdessen kürzer, bleibt
das Feld bei 18,98 und nichts wird schlechter.

**Beides zugleich geht nicht.** Jeder Millimeter, den die Zelle kürzer wird,
ist **entweder** ein Millimeter weniger Platine **oder** ein Millimeter mehr
Antennenfeld.

## Nicht gerechnet, weil offen

**Z1 (0 … 9,5) hängt am Stecker.** Wird aus der Buchse ein Randstecker oder
wird die Platine selbst zum Stecker, ändert sich dieses Band — nach oben wie
nach unten. Alle Zahlen oben lassen Z1 unverändert; **sie sind damit die
obere Schranke für die Länge, nicht die untere.**

# Quarz verlegt — und die Wandlerschleife geht so nicht

Stand 25.10.2026. **0 Abstandsfehler, Verbindungsfehler 171 → 165.**

## Quarz: liegt, beide Netze vollständig

| Netz | Weg |
| --- | --- |
| `XL1` | X1.1 → C8.1 → unten links herum → Durchkontaktierung **11,20 / 20,50** → oben → U1.17 |
| `XL2` | X1.2 → C9.1 → unten → Durchkontaktierung **11,20 / 19,70** → oben → U1.18 |

**Warum die Durchkontaktierungen links neben dem Modul sitzen und nicht am
Pad:** U1s linke Padspalte läuft bei `x` 12,05 im **0,8-mm-Raster
ununterbrochen** von `y` 12,5 bis 22,1 durch. Dort ist für eine
Durchkontaktierung kein Platz; sie muss links daneben, ab `x` ≤ 11,45.
Sechs Einträge weniger — genau die sechs Pads der beiden Netze.

## Wandlerschleife: Versuch, Fehler, Rückbau

Ich hatte die Durchkontaktierungen **in L1s eigene Pads** gesetzt und oben
geradeaus zu U1.31 und U1.30 gezogen — der kürzeste denkbare Weg. **Ergebnis:
14 Abstandsfehler.**

> **Der Grund ist eine Padreihe, die ich nicht aufgelistet hatte.** U1 hat am
> unteren Rand **zwei versetzte Reihen**: `y` 12,6492 **und `y` 13,5509**.
> Ich hatte die Spalten geprüft und „der Modulinnenraum ist leer" geschlossen —
> für die zweite Reihe hatte ich nie nachgesehen. Meine Bahnen liefen mitten
> hindurch.

**Gemessen, nachdem es schiefging** — die Korridore auf der Oberseite links am
Modul:

| von | bis | frei |
| --- | --- | ---: |
| versetzte Reihe (Oberkante 13,8504) → Pad 31 (13,9000) | | **0,0496** |
| Pad 31 (14,2995) → Pad 30 (14,7001) | | **0,4006** |
| zwischen zwei Pads der versetzten Reihe | | **0,401** |

**Keiner davon trägt eine brauchbare Bahn:** 0,4 mm Lücke erlaubt mit zweimal
0,152 Abstand eine Bahn von **0,09 mm**. Die Oberseite ist dort dicht.

**Alles zurückgebaut**, beide Bahnen und beide Durchkontaktierungen. Stand
wieder 0 Abstandsfehler.

## Was daraus folgt

**An U1s linke Spalte kommt man nur von links.** Genau so liegt der Quarz
jetzt. L1 steht aber rechts (`x` 14,8 … 18,8), und der Weg nach links ist unten
durch C3, C4 und C5 versperrt — die 0,4125 mm Luft, die wir mit Absicht
geschaffen haben, sind für eine Bahn zu wenig.

**Zwei Wege, beide mit Preis, keiner ausgeführt:**

1. **Unter C3 herum**, bei `y` ≈ 13,0 nach links bis `x` 11,2, dort
   Durchkontaktierung, oben ins Pad. Rechnerisch **rund 7 mm statt 3,59** —
   immer noch deutlich besser als die 10,14 von gestern, aber es gibt die
   Hälfte des Gewinns zurück.
2. **C3 und C5 noch einmal versetzen**, damit bei `y` ≈ 14,1 ein Korridor
   entsteht. Dann bleibt die Schleife kurz, aber die beiden Kondensatoren
   rücken wieder von ihren Pins weg — genau das, was wir heute früh behoben
   haben.

**Das ist eine Abwägung zwischen Abblockung und Schaltschleife**, und sie
gehört nicht ans Ende einer Runde. Gemeldet, nicht entschieden.

# Der Steckerwechsel bringt weniger Länge, als die Bänder versprechen

Stand 25.10.2026. **Gerechnet gegen die Messung, nicht gegen die Bandtabelle.**

## Die naive Rechnung und warum sie nicht stimmt

> Z1 war 9,5 mm, der Stecker greift nur 2,50 mm auf die Platine → 7,00 mm
> Gewinn, Platine 63,00.

**So ist es nicht.** `J1` war nie allein in Z1. Gemessen liegt **alles unter
dem Modul**, ohne J1, zwischen `y` **0,629 und 12,419 — 11,790 mm**, und die
Widerstandsreihe steht schon bei 0,629, also **unterhalb** von J1s Pads.

| | |
| --- | --- |
| J1 belegte | `x` 8,725 … 18,275, `y` 0,456 … 5,989 |
| Gruppe darunter/daneben (ohne J1) | `y` 0,629 … 12,419 |
| Summe der Hüllflächen | 84,48 mm² |
| untere Schranke bei 26,4 mm Breite | **3,20 mm** |
| höchstes Einzelteil | **SW2 mit 5,00** — zweithöchstes U2 mit 3,77 |

**J1 saß mitten in dieser Gruppe.** Es verschwinden zu lassen gibt einen
Streifen von 9,55 × 5,53 mm frei, **aber die Gruppe muss neu gepackt werden,
damit daraus Länge wird** — und sie muss hinter den Stecker, der die ersten
2,50 mm der Platine für sich braucht.

## Was dabei herauskommt

| Annahme | Gruppe liegt dann | **Gewinn** | **Platine** |
| --- | --- | ---: | ---: |
| zwei Reihen, SW2 bleibt unten (5,00 + 3,77) | `y` 2,80 … 11,82 | **+0,60** | **69,40** |
| zwei Reihen, **SW2 woanders** (3,77 + 2,60) | `y` 2,80 … 9,42 | **+3,00** | **67,00** |
| *naive Rechnung* | — | *+7,00* | *63,00* |

In einer Reihe geht es nicht: die Teile brauchen nebeneinander rund **52 mm**,
die Platine ist 27 breit.

> **Der Hebel ist nicht der Stecker, sondern `SW2`.** Der Reset-Taster ist mit
> **5,00 mm das höchste Teil** in diesem Band — und er ist ein Knopf, keine
> Schaltung. Liegt er woanders, wird die Gruppe 2,4 mm flacher, und **der
> Gewinn verfünffacht sich von 0,60 auf 3,00 mm.**

**Nicht ausgeführt.** Das ist eine Neuplatzierung der ganzen unteren Partie und
gehört in eine eigene Runde — zumal die Lage des Steckers auf der Platine noch
nicht feststeht.

## Der Stecker selbst: Pins ausgelesen, nicht geschlossen

**GCT `USB4155-03-C`, LCSC `C5183853`** — mit Hersteller, Datenblatt,
„Gender: Male", 24P.

> **Von drei Treffern der Bibliothekssuche sind zwei `C9900`-Platzhalter**
> (`C9900282109`, `C9900245240`). Nur der dritte ist ein echtes Teil. Wer den
> ersten Treffer nimmt, baut wieder einen Platzhalter ein.

**26 Pins:** A1 … A12, B1 … B12 und **zwei Schirmpins 26 und 27** (die Buchse
hatte dafür ein `S1` mit vier Pads).

| Signal | Buchse J1 | **Stecker** |
| --- | --- | --- |
| VBUS | A4, A9, B4, B9 | gleich |
| GND | A1, A12, B1, B12 | gleich |
| D+ | A6, B6 | **A6 `DP1`, B6 `DP2`** |
| D− | A7, B7 | **A7 `DN1`, B7 `DN2`** |
| CC | A5 `CC1`, B5 `CC2` | gleich benannt |
| Schirm | S1 (4 Pads) | **26, 27** |

## Eine Schaltungsfrage, die der Tausch aufwirft

**`R15` und `R16` sind je 5,1 kΩ von CC1 und CC2 nach Masse.** Das ist die
Beschaltung einer **Buchse**: beide CC-Leitungen brauchen Rd, weil das Kabel
gedreht eingesteckt werden kann.

**Beim Stecker ist das nicht dasselbe.** Ein Gerät mit festem Stecker ist in
der mating-Verbindung nur über **eine** CC-Leitung verbunden; die zweite ist
`VCONN`. Zwei Rd an einem Stecker ist nicht die Beschaltung, die die
Spezifikation dafür vorsieht.

> **Das ist eine Schaltplanfrage, keine Layoutfrage** — dieselbe Art wie die
> Polzahl bei J4. **Ich habe nichts geändert**, bevor das entschieden ist:
> ob Rd nur auf CC1 bleibt, was aus `R16` wird, und ob `VCONN` offen bleibt.

# Zwei Eigenheiten von U1, die jeden Fehlversuch kosten

Stand 25.10.2026. Beide am eigenen Fehlschlag gelernt, beide gemessen.

## 1. U1 hat am unteren Rand zwei versetzte Padreihen

| Reihe | `y` der Mitten | Kupfer von … bis |
| --- | --- | --- |
| vordere | **12,6492** | 12,3498 … 12,9486 |
| **hintere** | **13,5509** | **13,2514 … 13,8504** |

**Wer nur die Spalten prüft, hält den Modulinnenraum für leer.** Genau das
habe ich getan und zwei Bahnen mitten durch die hintere Reihe gezogen —
**14 Abstandsfehler.**

Zwischen zwei Pads derselben Reihe sind **0,401 mm**. Mit zweimal 0,152 mm
Abstand trägt das eine Bahn von **0,09 mm**. **Da geht nichts durch.**

## 2. An U1s linke Padspalte kommt man nur von links

Die Spalte steht bei `x` 12,05 im **0,8-mm-Raster ununterbrochen** von `y` 12,5
bis 22,1. Dazwischen ist kein Platz für eine Durchkontaktierung, und oberhalb
wie unterhalb der einzelnen Pads bleiben nur:

| zwischen | frei |
| --- | ---: |
| hinterer Reihe (13,8504) und Pad 31 (13,9000) | **0,0496** |
| Pad 31 (14,2995) und Pad 30 (14,7001) | **0,4006** |

**Die Durchkontaktierung muss also links neben die Spalte**, bei `x` ≤ 11,45 —
so liegen jetzt die beiden Quarznetze (Durchkontaktierungen bei `x` 11,20).

# Weg 1 für die Wandlerschleife ist versperrt — von C12

**Entschieden war: unter C3 herum, rund 7 mm.** Beim Durchrechnen des Wegs
stellt sich heraus, dass er so nicht existiert.

Die Durchkontaktierung müsste bei **11,20 / 14,30** liegen (links neben der
Spalte, auf Höhe von Pad 31). Sie selbst passt dorthin — **aber von unten
kommt man nicht heran:**

| Weg zur Durchkontaktierung | versperrt durch | Lücke |
| --- | --- | ---: |
| rechts an C12 vorbei | C12 (bis `x` 11,52) gegen C4 (ab 11,798) | **0,278** |
| links an C12 vorbei | U3 (bis 9,9493) gegen C12 (ab 10,18) | **0,231** |

Beide zu schmal: eine 0,2er Bahn braucht mit Abständen **0,504 mm**.

**Der einzige freie Weg führt oben herum** — von L1 nach Norden auf `y` ≈ 17,
nach links auf `x` 11,2, dann hinunter zur Durchkontaktierung. Gerechnet
**rund 10,7 mm** statt der veranschlagten 7 — **schlechter als beide
Schätzungen und kaum besser als die 10,14 vom Anfang.**

> **Nicht verlegt.** Eine Schleife, die nach dem Umweg so lang ist wie vorher,
> ist den Umweg nicht wert. **Der Blockierer ist `C12`**, der 100-nF-Kondensator
> an U3 Pin 8 — er steht bei 10,85 / 13,594 genau im einzigen Durchgang.
>
> Ihn zu verschieben hieße, ihn von U3 Pin 8 wegzurücken (heute 0,98 mm).
> **Das ist dieselbe Abwägung wie bei C3 und C5, nur mit einem anderen
> Kondensator** — und diesmal nenne ich den Preis vorher, statt „rückt weg"
> zu schreiben: **jede Verschiebung von C12 nach unten oder oben kostet
> genau diesen Abstand**, und zu messen ist er erst, wenn ein Zielpunkt
> feststeht.

# Der Stecker: Datenblatt gelesen, und es sagt zu CC nicht, was wir wissen wollten

Stand 25.10.2026. Quelle: **GCT-Zeichnung USB4155, Blatt 1 und 2 von 4**,
Ausgabe 12.04.2021, Rev. A4 — als PDF geholt und selbst angesehen, nicht über
eine Beschreibung.

## Was belegt ist

| | |
| --- | --- |
| Produkt | „USB3.2 Gen2 **Type C Plug**, Horizontal, SMT" |
| **Plattendicke** | **0,8 bis 1,0 mm** — passt auf unsere 0,8 |
| Platinentiefe im Lötbild | **2,50 mm** ab „PCB Edge" |
| Strom | 5,00 A für VBUS zusammen, 6,25 A für GND, **1,25 A für A5/B5** |
| Spannung | 48 V DC |
| **SHELL** | **GND** — die beiden Schirmpins liegen auf Masse |
| Lebensdauer | 10 000 Steckzyklen |

## Was das Datenblatt **nicht** sagt

Die Pintabelle führt:

| Pin | Signal | Description |
| --- | --- | --- |
| **A5** | **CC1** | Configuration Channel |
| **B5** | **CC2** | Configuration Channel |

> **Beide heißen „Configuration Channel". Das Wort `VCONN` kommt in der
> Zeichnung nicht vor.** Welcher der beiden Pins beim Stecken die CC-Leitung
> führt und welcher VCONN wird, **steht dort nicht.**

**Das ist genau der Fall, für den die Ansage galt: melden, statt zu wählen.**

## Die Entscheidung und was sie trägt

**Thomas hat entschieden: Rd 5,1 kΩ nur auf `CC1` (A5), `R16` entfällt, `CC2`
bleibt offen.**

Dazu, was ich dazu sagen kann und was nicht:

- **Stützend:** Die übliche Festlegung für Stecker ist CC auf **A5**, VCONN auf
  B5. Die Entscheidung stimmt damit überein.
- **Nicht belegt:** Diese Festlegung steht **nicht in dieser Zeichnung**. Sie
  ist Konvention, nicht Herstellerangabe.

> **Was schiefginge, wenn es umgekehrt wäre:** Der Rd läge auf VCONN, die
> CC-Leitung bliebe offen — das Gerät würde vom Host **gar nicht erkannt** und
> bekäme keinen Strom. Das merkt man erst am fertigen Brett.
>
> **Bleibt als offener Punkt stehen**, bis es aus einer Quelle kommt, die den
> Pin benennt — Steckerbelegung aus der USB-C-Spezifikation oder eine Auskunft
> von GCT. Die Zeichnung allein reicht nicht.

## Für die Stückliste

**GCT `USB4155-03-C`, LCSC `C5183853`.**

> **Achtung beim Nachbestellen:** Die Bibliothekssuche nach „USB4155" liefert
> **drei** Treffer, davon **zwei Platzhalter** — `C9900282109`
> („TYPEC-24PIN L=15.5MM") und `C9900245240` („USB4155-03C", ohne Bindestrich
> vor dem C). **Nur `C5183853` hat Hersteller, Datenblatt und Maße.**
