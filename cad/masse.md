# Maße und Höhenrechnung des Gehäuses

**Neu gerechnet am 30.10.2026, nicht ausgebessert.** Der Entwurf davor hing am
Zellenraum; der Akku ist am 25.10.2026 gestrichen (`../docs/plan.md`). Es ist
keine Zahl aus dem alten Stand übernommen.

**Gerechnet und gezeichnet — nicht gedruckt und nicht gemessen.** Von den vier
Stufen *geschrieben / übersetzt / im Abbild / am Gerät gelaufen* ist das die
erste: es gibt Körper, Lagensummen und Prüfungen, aber kein Teil in der Hand.

## Wo welche Zahl steht, und wo nicht

| Art der Zahl | steht in | Beispiel |
| --- | --- | --- |
| **Bauteilmaße** | `../hardware/` und `../docs/` | U1 ist 2,05 mm hoch |
| **Gehäusemaße** | `parameters.csv`, eine Zeile je Zahl | Dach 1,00 mm |
| **Abgeleitete Maße** | `tools/geometry.py` und `tools/budget.py` | Gehäusedicke |
| **Erklärung** | diese Datei und [gehaeuse.md](gehaeuse.md) | — |

Diese Datei **wiederholt keine Bauteilmaße**, sie verweist. Vier Bauteilhöhen
stehen trotzdem in `parameters.csv` — sie müssen dort stehen, weil ohne sie
nichts zu rechnen ist. Jede nennt in ihrem Kommentar das Teil **und** die
Unterlage, aus der sie kommt, und es ist jeweils **eine** Zahl, keine
Teileliste.

**Jede Zahl der Tabellen unten ist Ausgabe von `tools/budget.py`.** Weichen
Text und Skript voneinander ab, gilt das Skript; dann ist dieser Text falsch
und gehört geändert.

## 1. Achsen und Nullpunkt

Gleich wie in `../hardware/`, damit es nur **eine** Lesart gibt:

| | Richtung | Bereich |
| --- | --- | --- |
| `x` | **quer** | 0 … 27,00 mm |
| `y` | **längs**, 0 = USB-Kante | 0 … 59,55 mm — **aber siehe den Kasten unter Abschnitt 2** |
| `z` | **Dicke**, 0 = **Mittelebene der Platine** | — |

Der alte Entwurf hatte `x` als Längsrichtung. Das war die zweite Lesart, aus
der schon einmal fast eine Aufteilung in der falschen Achse geworden ist.

**Warum z = 0 in der Platinenmitte liegt und nicht an der Oberseite:** J1 ist
ein Straddle-Mount-Stecker, die Platine sitzt **mittig in ihm**
(`../docs/usb-stecker.md`). Die Mittelebene der Platine ist also auch die
Mittelebene des Steckers. Nur von dort aus ist ablesbar, ob der Stecker mittig
im Gerät steht — und das ist bei diesem Gerät die einzige Symmetriebedingung,
die es gibt.

## 2. Das Ergebnis in Zahlen

| Maß | Wert | Was genau es misst |
| --- | --- | --- |
| **Gehäusedicke** | **7,03 mm** | Außenfläche unten bis Außenfläche oben, Fall `stufe1-b` |
| Länge ohne Öse | 63,35 mm | Außenfläche vorn bis Außenfläche hinten, **ohne** den herausragenden Stecker |
| Breite außen | 30,80 mm | Außenfläche links bis rechts |
| **Länge mit Öse** | **71,35 mm** | dasselbe samt Ösenanbau |
| Steckerversatz | +0,435 mm | Steckermittelebene gegen Gehäusemittelebene; erlaubt sind ± 0,50 mm (`usb_center_tol`, **gesetzt, nicht gemessen**, am Muster zu prüfen) |

> **Diese Länge gilt für eine Platine, die noch nicht gezeichnet ist.**
> Gefunden beim Nachmessen am 30.10.2026.
>
> Das Gehäuse rechnet mit `pcb_len` = 59,55 mm. Das ist die **gerechnete**
> obere Schranke, nachdem der Akku entfiel. **Die gezeichnete Platine im Repo
> ist 70,0 mm lang** — gemessen am `BOARD_OUTLINE` in
> `../hardware/pcb-source-PCB1.txt` (2755,9055 mil), und hinter `y` = 59,55
> liegen **vier Bauteile mit zwölf Pads** (D1 und R10/R11/R12); die hinterste
> **Kupferkante** liegt bei **66,985 mm**, die 66,0 mm sind nur die Mitte von
> D1. **Die kürzeste Länge, bei der kein Bauteil wandern muss, ist 67,30 mm**
> — siehe `../hardware/layout-notes.md`, Weg C1.
>
> **Dieses Gehäuse ist damit 10,45 mm zu kurz für die Platine, die es
> umschließen soll.** Die Höhenrechnung ist davon nicht betroffen: alle
> Lagensummen laufen in `z`, und dort ändert die Länge nichts. Betroffen sind
> die Länge, die Öse, die Lage der LED und das Antennenfeld.
>
> **Welche der beiden Längen gilt, ist eine Entscheidung und keine Rechnung** —
> entweder wandern vier Pads und die Platine wird gekürzt, oder das Gehäuse
> wird länger. Beides ist Arbeit an der Platine, nicht am Gehäuse.
| Übertemperatur | 6,5 K | Oberfläche gegen Umgebung bei Spitzenlast, gerechnet |

Dazu kommt der Stecker, der **vor** der Frontfläche steht. Wie weit, ist
`ungeprueft`: `../docs/usb-stecker.md` nennt 13,0 mm als **gerechnet, nicht
bemaßt**, und sagt, das gehöre am Muster gemessen.

## 3. Die 2,90 mm sind nicht die Gehäusedicke

**Das ist der wichtigste Satz dieser Datei.** `../hardware/layout-notes.md`
nennt 2,90 mm eine „Höhengrenze". Die Zahl misst die **Höhe der Steckerhülle
von J1 über alles**, symmetrisch zur Platinenmittelebene — das, was in eine
USB-Buchse passen muss. Sie ist **keine** erlaubte Gehäusedicke.

**Rechnerischer Beweis, dass sie keine sein kann:** U1 allein ist 2,05 mm hoch
und steht über der 0,8 mm dicken Platine. 2,05 + 0,80 = 2,85 mm. Für Dach und
Boden blieben 0,05 mm. Ein Gehäuse von 2,90 mm Dicke ist mit diesen Bauteilen
nicht baubar, mit keinem Entwurf und bei keiner Sorgfalt.

`tools/budget.py` prüft die Lagensumme trotzdem gegen 2,90 mm, und die Prüfung
schlägt bei **jedem** wirklichen Fall an. Das ist das Ergebnis, kein Fehler.
Die Prüfung bleibt drin, weil sie die beiden Größen sichtbar auseinanderhält:

| Was gemessen wird | Grenze | Ergebnis |
| --- | --- | --- |
| **Höhe eines einzelnen Bauteils** über der Platine | 2,90 mm | eingehalten, höchstes ist U1 mit 2,05 |
| **Dicke des Gehäuses** über alles | — | 7,03 mm; eine Grenze dafür ist nicht gesetzt |

**Das ist der neunte Fall desselben Fehlermusters in diesem Projekt: zwei
Größen, die gleich heißen und Verschiedenes messen.** Die acht davor sind
Gehäusetiefe gegen Lötbildtiefe, Zellenmaße mit und ohne Schutzplatine und so
weiter. Hier sind es Bauteilhöhe und Gehäusedicke.

**Was wirklich eine Grenze ist:** Wer die 2,90 mm als Gerätedicke gelesen hat,
wollte vermutlich, dass das Gerät flach wie ein Stick ist. Dafür gibt es eine
andere Prüfung, und die ist eingehalten: der Stecker muss **mittig** im Gehäuse
stehen, sonst streift das Gerät an der Blende einer versenkten Buchse.

## 4. Die Lagensummen

Von der Platinenmittelebene nach außen, alle sechs gerechneten Fälle. **Diese
Tabelle ist die Zusammenfassung; die vollständige Aufteilung mit der
Bedeutung jeder Lage druckt `python cad/tools/budget.py`.**

| Fall | nach oben | nach unten | **Dicke** | Stecker mittig? |
| --- | ---: | ---: | ---: | --- |
| `leer` (Kontrollpunkt) | 1,400 | 1,400 | **2,800** | ja, ±0,000 |
| `stufe1-a` (Datenträger A) | 3,950 | 2,700 | **6,650** | **nein, +0,625** |
| **`stufe1-b` (Datenträger B)** | **3,950** | **3,080** | **7,030** | ja, +0,435 |
| `stufe2-a` (Sensor, A) | 3,950 | 2,900 | **6,850** | **nein, +0,525** |
| `stufe2-b` (Sensor, B) | 3,950 | 3,080 | **7,030** | ja, +0,435 |
| `bm-lite` (Gegenprobe) | 3,950 | 4,200 | **8,150** | ja, −0,125 |

**Gebaut ist `stufe1-b`** — Variante B ist die, die `../hardware/datentraeger.md`
empfiehlt, weil nur sie die Karte aus dem Gerät lässt.

Drei Dinge, die diese Tabelle zeigt und die man nicht erwartet:

- **Nach oben steht alles fest.** 3,950 mm in jedem Fall, weil U1 oben das
  höchste Teil ist und sich an U1 nichts ändert. Alle Unterschiede liegen
  unten.
- **Der dünnere Datenträger macht das Gehäuse schiefer, nicht besser.**
  Variante A spart 0,38 mm Dicke, treibt den Steckerversatz aber von 0,435 auf
  0,625 mm — über die gesetzte Grenze. Wird A genommen, braucht der Boden
  **0,30 mm mehr** (1,30 statt 1,00 mm); dann ist das Gehäuse 6,95 mm dick und
  der Versatz 0,475 mm. Das kostet die Hälfte der Ersparnis.
- **Das BM-Lite würde 1,12 mm kosten.** Siehe Abschnitt 6.

## 5. Was der Verguss an Höhe kostet

Der Epoxid-Verguss ist Pflicht (`../docs/plan.md`, FIPS-Punkt 1) und **später
nicht nachrüstbar**. In der Höhenrechnung steht er als **eine** Lage:

| | Wert | Was genau es misst |
| --- | --- | --- |
| `pot_cover` | **0,50 mm** | Mindestdicke des Vergusses **über** dem höchsten vergossenen Bauteil |

**Warum 0,50 und nicht weniger:** Dünner als ein halber Millimeter läuft eine
gefüllte Gießmasse nicht zuverlässig über eine Bauteilkante, und eine Deckung,
die man mit dem Fingernagel durchkratzt, deckt nichts. **Warum nicht mehr:**
jedes Zehntel geht eins zu eins in die Gehäusedicke.

**Diese 0,50 mm sind gesetzt, nicht gemessen.** Welche Masse genommen wird,
steht in keiner Unterlage — das entscheidet, wer sie bestellt. Hängt an der
Masse ein größeres Mindestmaß, wird das Gehäuse um die Differenz dicker.

**Der Verguss kostet nur oben Höhe.** Unten bestimmt der microSD-Schacht mit
1,68 mm die freie Höhe, und 0,80 mm vergossenes Teil plus 0,50 mm Deckung sind
nur 1,30 mm — der Verguss verschwindet unter dem, was der Schacht ohnehin
braucht. **Unten ist er umsonst zu haben.**

### Wärme

| | Wert | Herkunft |
| --- | --- | --- |
| Leistung bei Spitzenlast | 0,338 W | 67,5 mA aus `../docs/power-budget.md`, an 5 V aus USB |
| Außenfläche | 52,3 cm² | gerechnet aus den Außenmaßen, ohne Öse |
| **Übertemperatur** | **6,5 K** | `P / (alpha · A)`, mit alpha = 10 W/(m²·K) |

Die 10 W/(m²·K) sind eine **Annahme** für eine frei stehende Kunststofffläche
an ruhender Luft, keine Messung. 6,5 K sind unkritisch, und selbst beim
Zehnfachen der Annahmefehler bliebe es handwarm. Zwei Dinge dazu:

- **Der Verguss verbessert die Wärmeabfuhr, er verschlechtert sie nicht.**
  Gefülltes Epoxid leitet rund 0,8 W/(m·K), Luft 0,026. Ein vergossenes Gerät
  führt die Wärme von U1 besser in die Wand als ein hohles.
- **Die Spitze ist keine Dauerlast.** 67,5 mA fließen, wenn mehrere große
  Verbraucher zusammenkommen; das hält keine Minuten.

## 6. Die Luft für Stufe 2 — und der Konflikt, der dabei herauskam

**Gefragt war: wie viel Luft halte ich für den Fingerabdrucksensor vor.**

### Vorgehalten ist 1,70 mm freie Höhe unter der Platine

Das ist die Zahl. Sie deckt beides ab, was Stufe 2 unter der Platine braucht:

| Teil | braucht | Quelle |
| --- | ---: | --- |
| microSD-Schacht (Variante B) | 1,68 mm | `../hardware/datentraeger.md` |
| ZIF-Stecker für das Sensor-Flachband | **1,50 mm** | **in keiner Unterlage — Annahme** |

Die 1,70 mm sind in `parameters.csv` nicht als eigene Zahl eingetragen, weil
sie keine ist: sie ergibt sich als das Größere von beidem. Gebaut sind heute
1,68 mm. **Für den ZIF-Stecker bleibt damit 0,18 mm Luft über der Annahme** —
und die Annahme selbst ist das Wacklige daran, nicht die Luft.

### Der Sensor kostet an Höhe nichts, weil er im Boden sitzt

| Ebene | z | Was genau |
| --- | ---: | --- |
| Bodeninnenfläche | −2,08 mm | dort endet die freie Höhe |
| Bodenaußenfläche | −3,08 mm | dort liegt der Finger |
| Bodendicke | 1,00 mm | — |
| Sensorkörper FPC2534AM | 0,83 mm | `../docs/fingerprint.md` |

Der Sensor ist ein **Flex-Modul**: er wird von innen in ein glattes Loch im
Boden geklebt und schließt außen bündig ab. Mit 0,83 mm in einem 1,00 mm
dicken Boden bleibt **0,17 mm Deckschicht** über ihm — der Finger erreicht ihn.
Auf der Platine wäre er mehr als zwei Millimeter vom Finger weg und nutzlos.

**Darum kostet Stufe 2 keine Dicke.** Was sie kostet, ist der ZIF-Stecker und
der Weg des Flachbands — und beides geht in die 1,70 mm, die ohnehin da sind.

### Zwei Teile, die beide „der Sensor" heißen

**Hier liegt eine Verwechslung, und sie gehört aufgelöst, bevor jemand danach
rechnet:**

| Teil | Maß | Was es ist |
| --- | --- | --- |
| **FPC2534AM** | **Ø 11,0 × 0,83 mm** | der Sensor am Flachband, „erste Wahl" (`../docs/fingerprint.md`, SPC27317 Abschn. 2.3) |
| **FPC BM-Lite** | **15 × 15 × 2,8 mm** | ein ganzes Modul mit eigener Platine, dort als „überholt" geführt |

**Die 2,8 mm gehören dem BM-Lite, nicht dem FPC2534.** Wer „der Sensor ist
2,8 mm hoch und passt mit 0,1 mm Luft" rechnet, vergleicht das Modul mit der
Steckerhülle — also wieder zwei Dinge, die Verschiedenes messen. Der FPC2534
ist 0,83 mm dick und braucht keine Luft unter einer Grenze, weil er im Boden
sitzt.

Was das BM-Lite kosten würde, steht als Gegenprobe in der Tabelle in
Abschnitt 4: **8,15 mm statt 7,03 — 1,12 mm mehr.**

### Der wirkliche Konflikt ist die Fläche, nicht die Höhe

**Stufe 2 und Variante B wollen denselben Platz.** Gerechnet aus den Maßen
beider Unterlagen:

| | Bereich auf der Unterseite |
| --- | --- |
| microSD-Schacht, Körper | `x` 0,2 … 14,05, `y` 27,0 … 40,85 |
| Sensorsitz Ø 11,3 auf der bisherigen Stelle | `x` 13,55 … 24,85, `y` 30,35 … 41,65 |
| **Überschneidung** | **0,50 mm in x**, mit dem Damm um den Schacht **1,50 mm** |

Ausweichen geht nicht ohne Weiteres:

- **nach hinten** (`y` 45 … 59,55) liegt das NFC-Feld. Dort darf kein Metall
  und kein Einlegeteil hin (`../hardware/layout-notes.md`).
- **nach vorn** ist die Unterseite unter U1 nur `y` 12,5 … 20,5 frei — 8 mm,
  der Sitz braucht 11,3.
- **nach oben** liegt die Anzeige.

**Daraus folgt eine Entscheidung, die nicht mir gehört:** Stufe 2 und der
microSD-Schacht gehen zusammen nur, wenn eines von beiden umzieht. Mit
Variante A (gelötet, 8 × 6,2 mm) ist der Platz frei und der Sitz passt, wo er
bisher stand.

**Im gebauten Körper ist der Sensorsitz darum nicht enthalten.** Stufe 1 hat
keinen Sensor, und ein geschlossener Boden ist für den Manipulationsschutz
besser als ein Loch, das niemand braucht. Stufe 2 ist ein **eigener Lauf mit
eigenem Boden** — die Höhe dafür steht bereit, die Fläche ist zu entscheiden.

## 7. Was geprüft ist und was das heißt

`python cad/tools/budget.py --selftest` und
`cd cad/tools && python -m unittest test_cad`.

| Prüfung | Was sie misst | Stand |
| --- | --- | --- |
| **9 Kontrollpunkte** der Höhenrechnung | jede mit feststehender Antwort, **4 davon müssen anschlagen** | alle wie erwartet |
| **29 Proben im Körper** | ist jede Öffnung offen und jede Wand voll — **15 müssen voll sein, 14 frei** | alle bestanden |
| **Dichtheit** der Netze | jede Kante genau zweimal, gegenläufig | beide Schalen dicht |
| **Volumen** beider Schalen | Vorzeichen, also Richtung der Normalen | positiv |
| **3 Gegenproben** | Schlitz entfernt, Damm entfernt, Bauteil zu hoch | **jede schlägt an** |

**Eine Prüfung, die nie angeschlagen hat, ist keine.** Darum steht zu jedem
bestandenen Fall einer, der anschlagen muss — und die drei Gegenproben in
`tools/test_cad.py` machen den Entwurf absichtlich kaputt und verlangen, dass
es gemerkt wird.

**Das ist alles gerechnet.** Kein Teil ist gedruckt, keine Wandstärke ist
gemessen, kein Stecker ist eingesteckt. Was die Prüfungen sagen, gilt für die
Beschreibung des Körpers — nicht für ein Stück Kunststoff.

## 8. Was offen ist und nicht geraten wurde

| Nr. | Offen | Was es ändern würde |
| --- | --- | --- |
| 1 | **Breite der Steckerhülle von J1** (`usb_shell_w`, heute 8,70) | Weite des Kragens; die Zeichnung bemaßt sie nicht, am Muster zu messen |
| 2 | **Wie weit der Stecker vor der Front steht** | nichts am Gehäuse, aber die Gesamtlänge; 13,0 mm sind gerechnet, nicht bemaßt |
| 3 | **Bauhöhe des ZIF-Steckers** der Stufe 2 | über 1,70 mm wird das Gehäuse dicker |
| 4 | **Welche Vergussmasse** | Mindestdeckung, Wärmeleitwert, Fertigungsschritt |
| 5 | **Stufe 2 gegen microSD-Schacht** (Abschnitt 6) | welcher von beiden umzieht |
| 6 | **Lage von D1 nach dem Kürzen** | heute 4,00 mm vor der hinteren Kante, abgeleitet aus „wandert mit der Spitze" |
| 7 | **Welcher 3D-Drucker** | Wandstärken und Spiele, siehe [gehaeuse.md](gehaeuse.md) |
| 8 | **Platinenlänge 59,55 mm** ist eine **obere Schranke** | wird sie kürzer, wird das Gehäuse kürzer; eine Zeile in `parameters.csv` |
