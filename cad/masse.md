# Maße und Dickenrechnung

Stand 24.10.2026. **Gerechnet, nichts gebaut, nichts gemessen.** Alle Teilemaße
stammen aus `../docs/parts.md` und `../hardware/layout-notes.md`; was dort fehlt,
ist hier als `offen` markiert und **nicht geraten**.

Diese Datei kommt vor der Zeichnung. Ergibt die Rechnung, dass das Gerät zu dick
wird, ist das der wichtigste Befund – und er muss vor dem Layout bekannt sein.

## Das Ergebnis in einem Satz

**Das Gerät wird mit der empfohlenen Zelle (Adafruit 1317) mindestens 9,55 mm
dick, außen 29,8 × 81,8 mm (mit Öse).** Die Schätzung in
`../hardware/layout-notes.md` lautete „etwa 8–9 mm fertig". **Sie ist um
0,6–1,6 mm zu optimistisch**, weil darin die Ferritfolie, die Luftspalte und die
Wandstärken fehlen.

**Die schmale Zelle macht es nicht dünner, sondern dicker** (4,05 statt 3,8 mm).
Sie hilft nur gegen Breite und Länge. Wer 9,55 mm nicht will, muss an der Zelle
sparen oder eine andere finden – nicht an der schmalen Alternative.

## 1. Was feststeht

| Teil | Maß | Quelle |
| --- | --- | --- |
| Platine | **26** × 70 × **0,8 mm** | `layout-notes.md`, Bauform – Breite 24 → 26, siehe Abschnitt 4a |
| U1 Modul MDBT50Q-1MV2 | **2,05 mm** hoch | `parts.md` |
| DS1 OLED 0,71" | 13,90 × 22,00 × **1,26 mm** | `parts.md` |
| Akku Adafruit 1317 | **3,80** × 19,75 × 26,02 mm | `parts.md`, Zeile 443 |
| Akku BerryBase LP-401230-J | **4,05** × 12,0 × 30,5 mm | `parts.md`, Zeile 444 |
| SW1 Panasonic EVQP7A01P | 3,5 × 2,9 × **1,35 mm**, seitlich, Hub 0,2 mm | `parts.md` |
| J1 USB-C GCT USB4085 | Körper 8,95 × 9,17 mm, **Öffnung 8,35 × 2,56 mm**, Bauhöhe 3,16 bzw. 3,46 mm | GCT-Zeichnung, siehe Abschnitt 9 |
| U2 SE051C2 | 3 × 3 × **0,32 mm** | `layout-notes.md` |
| D1 RGB-LED | 1,6 × 1,6 × **0,5 mm** | `layout-notes.md` |
| Sensor FPC2534 (Stufe 2) | **Ø 11 × 0,83 mm** | `../docs/fingerprint.md`, Zeile 78 |
| Ferritfolie | **0,3–0,5 mm**, Zielwert | `layout-notes.md`, offener Punkt 3 |

## 2. Welches Teil auf welcher Seite liegt

Aus `layout-notes.md`: die Anzeige liegt auf der Seite, die der Nutzer ansieht,
der Akku gegenüber; die Ferritfolie liegt **zwischen Zelle und Platine**.

| Seite | Teile | Höchstes Teil |
| --- | --- | --- |
| **Oben** (Nutzerseite) | U1 Modul 2,05 · DS1 OLED 1,26 · SW1 1,35 (seitlich) · D1 0,5 | **U1 mit 2,05 mm** |
| **Unten** | Ferritfolie 0,4 + Akku 3,80 · U2 0,32 · U3 Flash · Sensorfeld 0,83 (Stufe 2) | **Ferrit + Akku mit 4,20 mm** |

Zwei Dinge fallen dabei auf:

- **Das Modul ist 0,79 mm höher als das Glas der Anzeige.** Das Glas läge also
  0,79 mm tief unter dem Gehäusefenster. Deshalb liegt die Anzeige auf einem
  **0,8 mm dicken Klebepad** und schließt dann bündig mit dem Modul ab. Ohne das
  Pad braucht das Fenster eine Stufe, und man sieht durch einen Schacht.
- **Der Sensor der Stufe 2 ist mit 0,83 mm viel dünner als der Akku.** Solange er
  **neben** der Zelle sitzt – so steht es in den Layout-Vorgaben –, ändert er die
  Dicke nicht. Das Gehäuse bleibt für Stufe 1 und 2 dasselbe.

## 3. Die Dickenrechnung

Druckerwerte: 0,4 mm Düse, 0,2 mm Schichthöhe (Abschnitt 5).

| Schicht | Maß | Begründung |
| --- | --- | --- |
| Deckel oben | **1,00 mm** | 5 Schichten à 0,2 mm, dichtes Dach |
| Luft über dem Modul | **0,30 mm** | Lötklecks, Modulrand, Montagespiel |
| U1 Modul | **2,05 mm** | höchstes Teil oben |
| **Platine** | **0,80 mm** | festgelegt wegen Platinenrand-Stecker |
| Luft unter der Platine | **0,20 mm** | Lötseite, Rückseiten der Durchsteckpins |
| Ferritfolie | **0,40 mm** | Mittelwert des Zielbereichs 0,3–0,5 |
| Akku Adafruit 1317 | **3,80 mm** | dickstes Teil überhaupt |
| Boden unten | **1,00 mm** | wie Deckel |
| **Summe** | **9,55 mm** | |

Mit der schmalen Zelle LP-401230-J (4,05 mm) sind es **9,80 mm**.

Mit der dünnsten Ferritfolie (0,3 mm) sind es **9,45 mm** – das ist die
untere Grenze, die ich belegen kann.

**Nicht enthalten:** Klebeschicht unter dem Akku, Fertigungstoleranz des Drucks
(üblich ±0,1–0,2 mm je Maß), Aufquellen der Zelle über die Lebensdauer. Jedes
davon macht es dicker, keines dünner. Realistisch sind **9,5–10,0 mm**.

### Wo noch etwas zu holen wäre

| Maßnahme | Gewinn | Preis |
| --- | --- | --- |
| Boden und Deckel 0,8 mm statt 1,0 mm | 0,40 mm | 4 Schichten – bei einem Teil, das am Schlüsselbund scheuert, zu dünn |
| Ferritfolie 0,3 mm | 0,10 mm | hängt am Material, siehe offener Punkt 3 der Layout-Vorgaben |
| Akku in eine Bodenöffnung versenken | bis 1,00 mm | Zelle liegt frei, Hülle ungeschützt – **nicht empfohlen** |
| Dünnere Zelle als 3,8 mm | direkt | in `parts.md` wurde keine belegte dünnere Zelle mit PCM gefunden |

**Empfehlung: 9,55 mm annehmen und nicht nachbessern.** Ein Autoschlüssel ist
dicker. Die 8–9 mm aus den Layout-Vorgaben waren eine Addition ohne Gehäuse.

## 4. Die Außenmaße

Seitenwand 1,6 mm, damit die Schnappverbindung als Stufe darin Platz hat
(0,8 mm innen, 0,8 mm außen). Spiel zur Platine 0,3 mm je Seite.

| Maß | Rechnung | Ergebnis |
| --- | --- | --- |
| Breite | 26,0 + 2 × (0,3 + 1,6) | **29,8 mm** |
| Länge ohne Öse | 70,0 + 2 × (0,3 + 1,6) | **73,8 mm** |
| Öse | 8,0 mm Anbau, Loch Ø 4,0 mm | **+ 8,0 mm** |
| Länge mit Öse | | **81,8 mm** |
| Dicke | Abschnitt 3 | **9,55 mm** |

**81,8 × 29,8 × 9,55 mm** – etwa wie ein Autoschlüssel mit Funkfernbedienung.

## 4a. Warum die Platine 26 mm breit ist und nicht 24

Entschieden am 24.10.2026. Gerechnet, nicht geschätzt:

| | Maß |
| --- | --- |
| nutzbare Breite = Platinenbreite − 2 × 1,2 mm bauteilfreier Rand | |
| bei 24 mm Platine | 21,60 mm |
| **bei 26 mm Platine** | **23,60 mm** |
| gebraucht: schmale Zelle LP-401230 (12,0) + Sensor FPC2534 (11,0) | **23,00 mm** |

Bei 24 mm fehlten **1,40 mm** – der Fingerabdrucksensor der Stufe 2 hätte nicht
neben die Zelle gepasst. In ein anderes Drittel kann er nicht: vorn liegt die
Chipantenne des Moduls, hinten die NFC-Antenne, und beide dürfen kein Metall und
kein Einlegeteil in der Nähe haben.

**Mit 26 mm passt es, mit 0,60 mm Reserve.** Das kostet 2 mm Gehäusebreite
(29,8 statt 27,8 mm) und ändert die Dicke nicht. Die Länge bleibt 70 mm.

**Das setzt die schmale Zelle voraus** (LP-401230-J, 12,0 mm breit, 100 mAh).
Mit der empfohlenen Zelle (Adafruit 1317, 19,75 mm breit) wären 19,75 + 11,0 =
30,75 mm nötig – das geht auch auf 26 mm nicht. Wer den Sensor will, nimmt die
schmale Zelle und damit etwa 4,6 statt 6,8 Monate Laufzeit
(`../docs/power-budget.md`). Stufe 1 ohne Sensor kann weiter die große Zelle
nehmen; **das Gehäuse trägt beide**, weil der Akkuraum 23,6 mm breit ist.

## 5. Druckerwerte, mit denen gerechnet ist

| Wert | Annahme |
| --- | --- |
| Düse | 0,40 mm |
| Schichthöhe | 0,20 mm |
| Außenwände (Umfang) | 3 Bahnen = **1,20 mm** mindestens; hier 1,60 mm |
| Dach und Boden | 5 Schichten = **1,00 mm** |
| Material | PETG (zäher als PLA, hält die Schnapphaken aus) |
| Spiel Teil zu Teil | 0,20 mm |
| Kleinster Steg | 0,80 mm (2 Bahnen) – darunter wird nichts gezeichnet |
| Kleinstes Loch | Ø 1,6 mm – kleinere Löcher laufen beim Druck zu |
| Stützmaterial | **keines** – jeder Überhang ist senkrecht, eine Brücke unter 5 mm oder 45° angeschrägt |

Diese Werte sind **Annahmen für einen gewöhnlichen Schmelzdrucker**, nicht am
Drucker des Nutzers gemessen. Welcher Drucker es ist, steht nicht in den
Unterlagen – `offen`.

## 6. Was offen ist und nicht geraten wurde

| Nr. | Offenes Maß | Warum es fehlt | Was es ändern würde |
| --- | --- | --- | --- |
| 1 | ~~Bauhöhe des USB4085~~ **beantwortet, aber der Stecker passt nicht** | aus GCTs Zeichnung abgelesen, siehe Abschnitt 9 | Der Hohlraum ist 2,35 mm hoch, der Stecker 3,16–3,46 mm. **Es fehlen 0,81–1,11 mm.** Entscheidung liegt bei Thomas |
| 2 | **Dicke und Material der Ferritfolie** | offener Punkt 3 der Layout-Vorgaben | ±0,2 mm Gesamtdicke |
| 3 | **Freihaltezone um die Chipantenne des U1** | offener Punkt 1 der Layout-Vorgaben | wie weit die Seitenwand neben dem Modul wegbleiben muss |
| 4 | **Platz für die umgelegte FPC-Fahne der Anzeige** | nicht bemaßt | Höhe des Hohlraums hinter dem Glas |
| 5 | **Wo die Platine im Gehäuse gehalten wird** | die Platine hat keine Befestigungslöcher | Abschnitt 7 – **Vorgabe ans Layout** |
| 6 | **Genaue Lage von Fenster, Taster, Sensorfeld und LED** | das Layout ist noch nicht gezeichnet | alle Öffnungen; sie stehen als Parameter und sind in einem Zug zu ändern |
| 7 | **Welcher 3D-Drucker** | nicht dokumentiert | Abschnitt 5 |

## 7. Was das Layout dafür tun muss

Das sind **Vorgaben ans Layout**, keine Wünsche:

1. **Platinenmaß 26 × 70 mm**, Dicke 0,8 mm – die Breite ist von 24 auf 26 mm
   erhöht, Begründung in Abschnitt 4a. Das Gehäuse ist darauf gezeichnet. Ändert
   sich das Maß, ist nur der Parameter zu ändern – aber jede Änderung über
   ± 0,2 mm macht ein neues STL nötig.
2. **Rundum 1,2 mm Platinenrand frei von Bauteilen** (oben und unten). Dort
   liegt die Platine auf der Stufe des Gehäuses und wird zwischen den Schalen
   geklemmt – das ersetzt Schrauben und Befestigungslöcher.
3. **Ecken der Platine mit R 1,5 mm abrunden**, damit sie in die gerundeten
   Innenecken des gedruckten Gehäuses passt. Eine scharfe Ecke stößt an.
4. **Kein Bauteil höher als 2,05 mm auf der Anzeigenseite** und **keines höher
   als 4,20 mm auf der Akkuseite** (Ferrit mitgerechnet). Wer höher baut, macht
   das Gehäuse dicker.
5. **Das Sensorfeld der Stufe 2 bleibt neben der Zelle**, nicht über ihr. Sonst
   wären es 3,8 + 0,83 mm auf einer Seite und das Gehäuse müsste neu entstehen.
   Aufteilung quer: **Zelle bei y = 0…12 mm, Sensor bei y = 12,5…23,5 mm**, die
   Sensoröffnung des Gehäuses liegt auf Mitte y = 18,0 mm. Reserve: 0,60 mm.
   Das geht nur mit der schmalen Zelle – Abschnitt 4a.
6. **Die beiden Taster bei Länge 22,75…26,25 mm an den langen Kanten**, so wie
   der Platinen-Strang sie übergeben hat. Mitte der Betätigungsfläche **0,7 mm
   über der Platinenoberseite** (halbe Bauteilhöhe 1,35 mm). Weicht das ab, sind
   `btn_cx` und `btn_z` zu ändern. Einzelheiten in Abschnitt 8.
7. **USB-C mittig an der kurzen Kante** bei Länge 0. Die Frontöffnung ist dort.
8. **Die NFC-Antenne liegt im letzten Drittel** (44–70 mm). Dort hat das Gehäuse
   **kein Einlegeteil, keine Schraube, kein Metall** – auch die Öse ist aus
   Kunststoff, und das Loch sitzt am **Gehäuseende** hinter der Platine, nicht
   über der Antenne. Siehe `gehaeuse.md`, Abschnitt 6.

## 8. Die beiden Taster: der Rahmen kommt mit 0,75 mm aus

Nachgerechnet am 24.10.2026 auf die Übergabe des Platinen-Strangs. Dessen `y`
(längs, 0 an der USB-Kante) ist in dieser Datei die Längsrichtung.

| Maß | Wert |
| --- | --- |
| Taster belegt längs | 22,75 … 26,25 mm |
| Anzeigeglas beginnt | 27,00 mm → **0,75 mm Luft** |
| Funkmodul endet | 21,50 mm → **1,25 mm Luft** |

**Die 0,75 mm sind nicht die Grenze des Rahmens, weil der Rahmen dort nichts
hat.** Das Anzeigefenster ist ein glatter Durchbruch im 1,0 mm dicken Dach,
ohne nach unten gezogenen Rand und ohne Rippe. Es beginnt dort, wo die aktive
Fläche plus 0,5 mm Rand beginnt:

| Maß | Wert |
| --- | --- |
| Glas 27,00 … 49,00 mm, aktive Fläche 14,70 mm mittig | 30,65 … 45,35 mm |
| **Gehäusefenster** = aktive Fläche + 0,5 mm Rand | **30,15 … 45,85 mm** |
| Fensterrand zum Taster (26,25) | **3,90 mm** |
| Fensterrand zum Glasrand (27,00) | 3,15 mm |

**3,90 mm, nicht 0,75 mm.** Das Fenster kann den Taster nicht anschneiden, auch
nicht mit Toleranz. Quer liegt das Glas 6,05 mm vom Platinenrand, der
Klemmkragen belegt davon 1,20 mm – **4,85 mm Luft**, und das Glas berührt den
Kragen an keiner Stelle.

### Was der Rahmen stattdessen braucht

**Der Klemmkragen war das Problem, nicht das Fenster.** Er läuft 1,2 mm breit
am Platinenrand um und drückt die Platine fest – genau dort, wo ein seitlich
betätigter Taster sitzt. Gelöst im Gehäuse, nicht auf der Platine:

- Der Kragen ist an **beiden** Längskanten über 22,25 … 26,75 mm bis auf
  **1,65 mm Höhe freigestellt** (Bauhöhe 1,35 + 0,3 Luft).
- Der Kragen wird dadurch an zwei Stellen auf je 4,5 mm unterbrochen – von
  rund 192 mm Umfang. Die Klemmung trägt das.
- Die **Freistellung ist 5,6 mm lang, der Schlitz nur 4,5 mm.** Grund: Panasonic
  gibt den Körper mit 3,5 mm Länge an, DigiKey mit 4,70 mm. Beide Lesarten
  passen jetzt hinein – bei 3,5 mm bleibt 1,05 mm Luft je Seite, bei 4,70 mm
  noch 0,45 mm. Der Schlitz für den Stößel bleibt bei 4,5 mm.
- Die Freistellung reicht damit von 21,70 bis 27,30 mm. Zum Modulende (21,50)
  sind das 0,20 mm, zum Glasanfang (27,00) greift sie 0,30 mm über – **beides
  ohne Folge**, weil sie nur im 1,2 mm breiten Band am Platinenrand liegt und
  Modul wie Glas mittig sitzen (Glas 6,05 mm vom Rand, Modul 5,25 mm).

### SW1 bekommt eine Öffnung, SW2 nicht

`../hardware/layout-notes.md` sagt: der Reset-Taster **darf von außen nicht
erreichbar sein**. Die Übergabe setzt SW2 aber an die gegenüberliegende Kante,
auch seitlich betätigt. Beides geht zusammen:

| Taster | Kante | Was das Gehäuse macht |
| --- | --- | --- |
| SW1 (Bestätigen) | quer 0 | **Schlitz durch die Seitenwand**, 4,5 × 2,4 mm |
| SW2 (Reset) | quer 26 | **Blindtasche 0,8 mm tief**, die Wand bleibt 0,8 mm stehen |

Ohne die Blindtasche stünde die geschlossene Wand 0,3 mm vor dem Stößel und
könnte ihn dauerhaft gedrückt halten. Mit ihr hat der Stößel **1,10 mm ab
Platinenrand** Platz (0,3 Spiel + 0,8 Tasche).

**Beantwortet am 24.10.2026:** Panasonic gibt „dimensions excluding the push
plate: 2,9 mm" an, der Stößel ragt also **3,55 − 2,9 = 0,65 mm** heraus, Hub
0,2 mm. Zusammen **0,85 mm** – das Gehäuse gibt 1,10 mm, **0,25 mm Reserve**.
Die Blindtasche bleibt, wie sie ist.

Bestätigt ist damit auch die Grundannahme des ganzen Rahmens: Panasonic nennt
das Teil **„side-push SMD"**, es wird also seitlich betätigt. Hätte das nicht
gestimmt, säße die Öffnung in der falschen Wand.

## 9. Der USB-Stecker passt nicht in den Hohlraum

Abgelesen am 24.10.2026 aus GCTs eigener Zeichnung, Revision A1 vom
09.04.2018, Blatt 1/3, heruntergeladen von `prod-parts-data.s3.amazonaws.com`.
Im Schriftfeld steht **„USB 2.0 Type C Receptacle, Dip Type, PCB Top Mount"** –
der Körper sitzt also vollständig **auf** der Platine, nichts taucht ein.

### Welche der drei Zahlen was ist

Auf dem Blatt stehen 3,46 · 3,16 · 2,56 nebeneinander. Sie sind
auseinanderzuhalten:

| Zahl | Wo sie steht | Was sie ist |
| --- | --- | --- |
| **2,56 ± 0,04** | Vorderansicht, neben 8,35 | **die Steckeröffnung**, also das Loch, in das der Stecker fährt. 8,35 × 2,56 mm ist das Maß des Type-C-Steckgesichts |
| **3,16** | Seitenansicht | die **äußere Höhe des Blechgehäuses**: 2,56 + 2 × 0,30 mm Blechdicke = 3,16. Die 0,30 mm stehen in der Spezifikation unter „Shell: Stainless steel (T=0,30 mm)" |
| **3,46** | Schnitt A-A | dasselbe, aber von einer **0,30 mm tiefer liegenden Bezugslinie** gemessen. 3,46 − 3,16 = 0,30 |

**Berichtigung:** In `../hardware/layout-notes.md` steht „Öffnung 3,16 mm hoch".
Das ist nicht die Öffnung – die Öffnung ist **2,56 mm**. 3,16 mm ist das
Blechgehäuse außen.

**Was ich nicht sicher sagen kann:** ob die Oberkante 3,16 oder 3,46 mm über der
Platinenoberseite liegt. Dafür müsste erkennbar sein, worauf die Bezugslinie in
Schnitt A-A liegt, und das gibt die Zeichnung in der Auflösung, die ich habe,
nicht her. **`ungeprueft`.**

### Es ändert an der Entscheidung nichts

| Bauhöhe | Hohlraum | fehlt |
| --- | --- | --- |
| 3,16 mm (kleinere Lesart) | 2,35 mm | **0,81 mm** |
| 3,46 mm (größere Lesart) | 2,35 mm | **1,11 mm** |

**Auch die günstigere Lesart passt nicht.** Die offene Frage muss also nicht
geklärt werden, bevor entschieden wird – sie ändert nur, wie viel fehlt.

### Die zwei Wege, gerechnet

| Weg | Was zu tun ist | Dicke | Gehäuseänderung |
| --- | --- | --- | --- |
| **A: vorn dicker** | Dach über dem Stecker um 1,11 mm anheben | **10,66 mm** vorn, 9,55 mm sonst | Nase über 8,95 × 7,26 mm; Stufe im Dach, druckbar, aber sichtbar |
| **B: Mid-Mount-Stecker** | Stecker sitzt in einem Ausschnitt der Platine | **9,55 mm, unverändert** | **keine** |

Bei Weg B steht nur noch etwa **3,46 / 2 + 0,4 = 2,13 mm** über der Platine –
das passt in die 2,35 mm, **ohne dass sich am Gehäuse etwas ändert.** Der Preis
ist ein anderes Teil und ein Ausschnitt in der Platine.

**Das Gehäuse hat keine Meinung dazu, welcher Weg richtig ist** – es sagt nur,
was jeder kostet. Entschieden wird das am Gerät, nicht am Rahmen.
