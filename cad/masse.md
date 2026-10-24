# Maße und Dickenrechnung

Stand 24.10.2026. **Gerechnet, nichts gebaut, nichts gemessen.** Alle Teilemaße
stammen aus `../docs/parts.md` und `../hardware/layout-notes.md`; was dort fehlt,
ist hier als `offen` markiert und **nicht geraten**.

Diese Datei kommt vor der Zeichnung. Ergibt die Rechnung, dass das Gerät zu dick
wird, ist das der wichtigste Befund – und er muss vor dem Layout bekannt sein.

## Das Ergebnis in einem Satz

**Das Gerät wird mit der empfohlenen Zelle (Adafruit 1317) mindestens 9,55 mm
dick, außen 27,8 × 81,8 mm (mit Öse).** Die Schätzung in
`../hardware/layout-notes.md` lautete „etwa 8–9 mm fertig". **Sie ist um
0,6–1,6 mm zu optimistisch**, weil darin die Ferritfolie, die Luftspalte und die
Wandstärken fehlen.

**Die schmale Zelle macht es nicht dünner, sondern dicker** (4,05 statt 3,8 mm).
Sie hilft nur gegen Breite und Länge. Wer 9,55 mm nicht will, muss an der Zelle
sparen oder eine andere finden – nicht an der schmalen Alternative.

## 1. Was feststeht

| Teil | Maß | Quelle |
| --- | --- | --- |
| Platine | 24 × 70 × **0,8 mm** | `layout-notes.md`, Bauform |
| U1 Modul MDBT50Q-1MV2 | **2,05 mm** hoch | `parts.md` |
| DS1 OLED 0,71" | 13,90 × 22,00 × **1,26 mm** | `parts.md` |
| Akku Adafruit 1317 | **3,80** × 19,75 × 26,02 mm | `parts.md`, Zeile 443 |
| Akku BerryBase LP-401230-J | **4,05** × 12,0 × 30,5 mm | `parts.md`, Zeile 444 |
| SW1 Panasonic EVQP7A01P | 3,5 × 2,9 × **1,35 mm**, seitlich, Hub 0,2 mm | `parts.md` |
| J1 USB-C GCT USB4085 | Körper 8,94 × 9,17 mm, Öffnung **3,16 mm** hoch | `layout-notes.md` |
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
| Breite | 24,0 + 2 × (0,3 + 1,6) | **27,8 mm** |
| Länge ohne Öse | 70,0 + 2 × (0,3 + 1,6) | **73,8 mm** |
| Öse | 8,0 mm Anbau, Loch Ø 4,0 mm | **+ 8,0 mm** |
| Länge mit Öse | | **81,8 mm** |
| Dicke | Abschnitt 3 | **9,55 mm** |

**81,8 × 27,8 × 9,55 mm** – etwa wie ein Autoschlüssel mit Funkfernbedienung.

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
| 1 | **Bauhöhe des USB4085 über der Platine** | `parts.md` nennt nur Körper 8,94 × 9,17 mm und Öffnung 3,16 mm hoch, keine Höhe | Höhe der Frontöffnung. Gezeichnet ist sie mit **4,00 mm** – `ungeprueft` |
| 2 | **Dicke und Material der Ferritfolie** | offener Punkt 3 der Layout-Vorgaben | ±0,2 mm Gesamtdicke |
| 3 | **Freihaltezone um die Chipantenne des U1** | offener Punkt 1 der Layout-Vorgaben | wie weit die Seitenwand neben dem Modul wegbleiben muss |
| 4 | **Platz für die umgelegte FPC-Fahne der Anzeige** | nicht bemaßt | Höhe des Hohlraums hinter dem Glas |
| 5 | **Wo die Platine im Gehäuse gehalten wird** | die Platine hat keine Befestigungslöcher | Abschnitt 7 – **Vorgabe ans Layout** |
| 6 | **Genaue Lage von Fenster, Taster, Sensorfeld und LED** | das Layout ist noch nicht gezeichnet | alle Öffnungen; sie stehen als Parameter und sind in einem Zug zu ändern |
| 7 | **Welcher 3D-Drucker** | nicht dokumentiert | Abschnitt 5 |

## 7. Was das Layout dafür tun muss

Das sind **Vorgaben ans Layout**, keine Wünsche:

1. **Platinenmaß 24 × 70 mm bleibt**, Dicke 0,8 mm. Das Gehäuse ist darauf
   gezeichnet. Ändert sich das Maß, ist nur der Parameter zu ändern – aber jede
   Änderung über ± 0,2 mm macht ein neues STL nötig.
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
6. **Der Taster SW1 an der langen Kante**, Mitte seiner Betätigungsfläche
   **0,7 mm über der Platinenoberseite** (halbe Bauteilhöhe 1,35 mm). Weicht das
   ab, ist der Parameter `btn_z` zu ändern.
7. **USB-C mittig an der kurzen Kante** bei Länge 0. Die Frontöffnung ist dort.
8. **Die NFC-Antenne liegt im letzten Drittel** (44–70 mm). Dort hat das Gehäuse
   **kein Einlegeteil, keine Schraube, kein Metall** – auch die Öse ist aus
   Kunststoff, und das Loch sitzt am **Gehäuseende** hinter der Platine, nicht
   über der Antenne. Siehe `gehaeuse.md`, Abschnitt 6.
