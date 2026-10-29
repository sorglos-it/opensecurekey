# Das Gehäuse

**Neu gezeichnet am 30.10.2026.** Zwei Schalen, verklebt und vergossen, kein
abnehmbarer Deckel, keine Schraube, kein Schnapphaken. Die Zahlen und die
Höhenrechnung stehen in [masse.md](masse.md), die Maße selbst in
`parameters.csv` — **hier steht, wie das Gehäuse aussieht und warum.**

**Gezeichnet und gerechnet, nicht gedruckt und nicht gemessen.**

## 1. Was das Gehäuse zu leisten hat

Vier Anforderungen bestimmen diesen Entwurf, drei davon neu gegenüber dem
alten Stand:

1. **Epoxid-Verguss über allen sicherheitsrelevanten Bauteilen.** Er ersetzt
   die Tamper-Elektronik (`../docs/plan.md`, FIPS-140-3-Abschnitt). Platz,
   Höhe, Wärmeabfuhr und ein Fertigungsschritt gehören eingeplant.
   **Später nicht nachrüstbar.**
2. **Kein abnehmbarer Deckel.**
3. **Keine Debug-Schnittstelle**, und nach dem Verguss **keine erreichbaren
   Prüfpunkte**.
4. **Das Gehäuse muss den Stecker mechanisch fassen.** J1 hat keine eigene
   Zugentlastung; ohne Kragen geht die Ziehkraft von 6–20 N in eine 0,8 mm
   dünne Platine (`../docs/usb-stecker.md`).

Was daraus folgt, ist nicht Gestaltung, sondern Zwang: **der Schnapphaken des
alten Entwurfs ist weg** — eine Schnappverbindung ist genau das, was ein
abnehmbarer Deckel ist.

## 2. Der Aufbau in sechs Sätzen

- **Untere Schale**: Boden, Seitenwände, eine umlaufende **Auflage** von
  1,2 mm Breite, auf der die Platine liegt, und die äußere Hälfte des
  Klebefalzes.
- **Obere Schale**: Dach, die innere Hälfte des Klebefalzes, die
  Dammwände und die Fenster.
- Die Platine liegt **in der Trennebene**; z = 0 ist ihre Mittelebene und
  damit die Mittelebene des Steckers.
- Die **Auflage trennt den oberen vom unteren Vergussraum** — darum lässt
  sich erst unten gießen, dann zukleben, dann oben gießen.
- **Vergossen wird alles, was nicht hinter einem Damm liegt.** Diese Regel
  hängt nur an den Öffnungen, nicht an Bauteillagen; sie übersteht eine
  Umplatzierung auf der Platine.
- Hinten sitzt die **Öse** aus Kunststoff, hinter der Platine, ohne Metall —
  dort liegt die NFC-Antenne.

## 3. Die Öffnungen, und was hinter jeder liegt

Mehr Öffnungen gibt es nicht. Jede ist ein Weg nach innen und darum einzeln zu
begründen.

| Öffnung | Wo | Maß | Was dahinter liegt |
| --- | --- | --- | --- |
| **Steckerkragen** | Frontwand, mittig | Hülle + 0,1 mm je Seite | der Stecker selbst; er füllt sie aus und wird eingeklebt |
| **Anzeigefenster** | Dach | aktive Fläche + 0,5 mm Rand | Trockeninsel über dem Glas |
| **Lichtfenster** | Dach, Kopfende | Ø 2,5 mm | Trockeninsel über D1 |
| **SW1-Schlitz** | Seitenwand `x` = 0 | 4,5 × 2,4 mm | Trockeninsel um den Taster |
| **SW2-Blindtasche** | Seitenwand `x` = 27 | 0,8 mm tief, **kein Durchbruch** | Trockeninsel; 0,8 mm Wand bleiben stehen |
| **microSD-Schlitz** | Seitenwand `x` = 0 | 12,0 × 1,8 mm | Trockeninsel um den Schacht |
| **2 Eingüsse** | Dach | Ø 2,0 mm | der Vergussraum; sie werden von der Masse selbst verschlossen |
| **2 Entlüftungen** | Dach | Ø 1,0 mm | dasselbe |
| **Ösenloch** | Öse | Ø 4,0 mm | nichts, es geht durch den Kunststoff |

**Keine dieser Öffnungen führt an ein Pad.** Jede endet an einer Trockeninsel,
und jede Trockeninsel ist von Dammwänden und der Platine umschlossen. Dass das
wirklich so ist, prüfen 29 Proben im Körper — siehe Abschnitt 7.

### Warum SW2 eine Blindtasche bekommt und SW1 nicht

`../hardware/layout-notes.md` verlangt, dass der Reset von außen **nicht**
erreichbar ist. Ganz zumachen geht trotzdem nicht: eine geschlossene Wand
stünde vor dem Stößel und könnte ihn dauerhaft gedrückt halten. Die Tasche
nimmt den Stößelweg auf, die Wand bleibt mit 0,8 mm — zwei Druckbahnen —
stehen. **Von außen ist dort nichts zu sehen und nichts zu drücken.**

## 4. Die Trockeninseln und ihre Dämme

Ein Damm ist eine 0,8 mm dicke Wand, die von der Schale herunter auf die
Platine reicht und den Verguss abhält. Er steht mit 0,2 mm Luft neben dem
Bauteil, das er trocken hält.

| Insel | Seite | Dammwände | Besonderheit |
| --- | --- | --- | --- |
| Stecker J1 | oben und unten | drei, die vierte ist die Frontwand | hält Masse aus dem Kontaktfeld |
| Anzeigeglas | oben | vier | vordere Wand nur **0,40 mm**, siehe unten |
| D1 | oben | vier | — |
| SW1 | oben | drei, die vierte ist die Seitenwand | — |
| SW2 | oben | drei, die vierte ist die Seitenwand | — |
| microSD-Schacht | unten | drei, die vierte ist die Seitenwand | Karte muss hinaus |

**Die eine Engstelle:** Zwischen U1 (endet bei `y` 22,55) und dem Anzeigeglas
(beginnt bei 23,05) sind **0,50 mm**. Dort ist der Damm **0,40 mm** dünn — eine
Druckbahn — und steht mit 0,10 mm Luft vor U1. Das ist knapp und es ist das
Dünnste am ganzen Teil.

**Wandert U1 um einen halben Millimeter nach vorn, wird daraus ein normaler
Damm von 0,8 mm.** Das ist eine Bitte ans Layout, keine Vorschrift: der
0,40-mm-Damm hält Gießmasse, aber er ist nichts, was man gern druckt.

## 5. Der Verguss und die Reihenfolge beim Bauen

**Die Reihenfolge ist Teil des Entwurfs.** Wer sie ändert, bekommt entweder
Masse in den Stecker oder einen Deckel, der nicht mehr zugeht.

1. Untere Schale drucken, obere Schale drucken.
2. Platine bestücken und **prüfen** — danach ist nichts mehr zugänglich.
3. Platine in die untere Schale legen, sie liegt auf der Auflage.
4. **Unteren Vergussraum gießen**, durch die offene Oberseite. Die Dämme der
   unteren Schale halten Schacht und Stecker frei.
5. Aushärten.
6. Obere Schale aufsetzen und den Klebefalz verkleben.
7. **Oberen Vergussraum durch die zwei Eingüsse im Dach füllen**, Luft geht
   durch die zwei Entlüftungen.
8. Aushärten. Die Masse in Eingüssen und Entlüftungen verschließt sie selbst.
9. Sensor der Stufe 2 — **anderer Lauf, anderer Boden**, siehe
   [masse.md](masse.md), Abschnitt 6.

**Schritt 2 ist der Punkt ohne Rückweg.** Nach dem Verguss ist keine Messung
am Gerät mehr möglich, kein Nachlöten, kein Auslesen über Prüfpunkte. Das ist
gewollt — es ist der Manipulationsschutz — aber es heißt: **eine Platine, die
vorher nicht geprüft ist, ist nach dem Gießen Abfall.**

## 6. Was das Layout dafür tun muss

Vorgaben, keine Wünsche. Alle Lagemaße in `x` = quer, `y` = längs, 0 = USB-Kante.

1. **Platinenmaß 27,00 × 59,55 mm**, 0,8 mm dick, Ecken R 1,5 mm. Die 59,55 mm
   sind eine obere Schranke; wird kürzer gezeichnet, wird das Gehäuse kürzer.
2. **Rundum 1,2 mm Platinenrand oben und unten frei von Bauteilen.** Dort
   liegt die Platine auf und dort dichtet der Verguss. Das ist **keine
   Klemmung mehr** — gehalten wird durch Kleben und Gießen.
3. **Alle Prüfpunkte in den Vergussbereich**, keiner unter eine Öffnung und
   keiner in eine Trockeninsel. Nach Schritt 8 darf von außen kein Pad und
   kein Via erreichbar sein.
4. **Keine Debug-Schnittstelle bestücken** und keine Steckerleiste dafür
   vorsehen, die von außen erreichbar wäre.
5. **Kein Bauteil höher als 2,05 mm auf der Anzeigenseite.** Jedes höhere Teil
   geht eins zu eins in die Gehäusedicke.
6. **Keines höher als 1,70 mm auf der Unterseite** — das ist, was der
   microSD-Schacht ohnehin braucht; darüber wird das Gehäuse dicker.
7. **Korridor von 0,5 mm rings um das Anzeigeglas frei halten**, lieber 1,0 mm.
   Dort steht der Damm.
8. **Vorderkante von J1 so, dass die Hülle durch den Kragen passt** — die
   Hülle sitzt im Kragen, nicht die Platine.
9. **Die NFC-Antenne liegt hinten.** Dort hat das Gehäuse kein Einlegeteil,
   keine Schraube, kein Metall; die Öse ist aus Kunststoff.
10. **D1 bleibt am Kopfende**, 4,00 mm vor der hinteren Kante. Wandert die
    Spitze, wandert D1 mit — das Lichtfenster rechnet damit.

## 7. Was geprüft ist

| | |
| --- | --- |
| `python cad/tools/budget.py` | druckt jede Lagensumme und prüft sie |
| `python cad/tools/budget.py --selftest` | 9 Kontrollpunkte, **4 müssen anschlagen** |
| `python cad/tools/build_mesh.py` | baut STL und STEP ohne Fusion, 29 Proben im Körper |
| `cd cad/tools && python -m unittest test_cad` | 7 Tests, darunter **3 Gegenproben** |

Die Proben im Körper tasten Punkte ab, an denen etwas **frei** sein muss, und
Punkte, an denen etwas **voll** sein muss — 14 und 15. Das ist die Prüfung, die
eine Dickenrechnung nicht leistet: **ob eine Öffnung wirklich offen ist.**

**Sie hat beim ersten Lauf dieses Entwurfs drei Fehler gefunden**, alle am
Klebefalz: in seiner Höhe gehört die äußere Wandhälfte der unteren und die
innere der oberen Schale, und die Proben hatten in der falschen Hälfte
gesucht. Der Falz selbst war richtig — aber das war erst nach dem Nachsehen
klar, und ohne die Proben wäre es nicht aufgefallen.

Die drei **Gegenproben** machen den Entwurf absichtlich kaputt: Tasterschlitz
entfernt, Damm um den Schacht entfernt, Bauteil höher als die Steckerhülle.
Jede muss anschlagen, und jede tut es.

**Was damit NICHT geprüft ist:** nichts ist gedruckt. Keine Wandstärke, kein
Spiel, keine Klebefuge, kein Vergussvorgang und kein Steckvorgang ist am
Gegenstand gemessen. Die Druckerwerte unten sind Annahmen.

## 8. Druckerwerte, mit denen gerechnet ist

| Wert | Annahme |
| --- | --- |
| Düse | 0,40 mm |
| Schichthöhe | 0,20 mm |
| Dach und Boden | 5 Schichten = 1,00 mm |
| Seitenwand | 1,60 mm = 4 Bahnen |
| Kleinster Steg | 0,80 mm; **einmal 0,40 mm**, siehe Abschnitt 4 |
| Material | PETG |
| Stützmaterial | **keines** — alle Stufen sind senkrecht |

**Nicht am Drucker des Nutzers gemessen**; welcher es ist, steht in keiner
Unterlage. Für ein vergossenes Seriengerät wäre ohnehin Spritzguss die
Bauweise, und dann ändern sich Wandstärken und Entformungsschrägen.

## 9. Was aus dem alten Entwurf nicht übernommen ist

Damit niemand danach sucht:

| Weg | Warum |
| --- | --- |
| **Zellenraum, Zellenreserve, Ferritfolie, Akkustecker** | der Akku ist gestrichen |
| **Schnappwülste und Schnappstufe** | ein Schnappdeckel ist ein abnehmbarer Deckel |
| **Klemmkragen** | der Verguss hält; die Auflage dichtet nur noch |
| **Mulde in der Frontfläche** | sie war wegen der Bohrlöcher der alten Buchse da; der Randstecker hat keine |
| **Nase über dem Dach** | die alte Buchse baute 3,46 mm über der Platine, der Randstecker 1,05 mm |
| **Blindstück für den Sensorsitz** | Stufe 1 hat kein Loch im Boden, also auch nichts zu verschließen |
| **Aufteilung quer** (Zelle neben Sensorfeld) | hing ganz am Akku |
| **`x` als Längsrichtung** | zweite Lesart derselben Achse |
