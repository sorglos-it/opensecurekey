# Gehäuse

Stand 24.10.2026. **Gezeichnet, nichts gedruckt, nichts angepasst.** Die
Dickenrechnung steht in [masse.md](masse.md) und ist die Grundlage dieser Datei.

## Was es ist

Zwei gedruckte Schalen, die Platine dazwischen geklemmt, ohne eine einzige
Schraube. Der USB-C-Anschluss schaut vorn an der kurzen Kante heraus, die Öse
für den Schlüsselring sitzt hinten.

| Teil | Datei | Was es ist |
| --- | --- | --- |
| Obere Schale | `export/shell-top.*` | Anzeigenseite: Fenster, Lichtfenster der LED, Tasteröffnung |
| Untere Schale | `export/shell-bottom.*` | Akkuseite: Platinentasche, Akkuraum, Sensoröffnung, Öse |
| Blindstück | `export/sensor-plug.*` | schließt die Sensoröffnung, solange Stufe 1 ohne Sensor läuft |

Je Teil liegt ein **STL** (zum Drucken) und ein **STEP** (zum Weiterarbeiten
ohne Fusion) in `export/`.

## Die drei wichtigsten Befunde

### 1. Das Gerät wird 9,55 mm dick, nicht 8–9 mm

Die Schätzung in `../hardware/layout-notes.md` war eine Addition ohne Gehäuse.
Gerechnet mit Wandstärken, Luftspalten und der Ferritfolie kommen **9,55 mm**
heraus, realistisch 9,5–10,0 mm. Rechnung in [masse.md](masse.md), Abschnitt 3.

**Die schmale Zelle hilft nicht:** sie ist mit 4,05 mm dicker als die
empfohlene (3,80 mm) und macht das Gerät auf **9,80 mm**. Sie hilft nur gegen
Breite und Länge.

### 2. Der Fingerabdrucksensor passt auf 24 mm Breite nicht neben die Zelle

Gerechnet, nicht geschätzt:

| | Maß |
| --- | --- |
| Platinenbreite | 24,00 mm |
| abzüglich bauteilfreier Rand (2 × 1,2) | **21,60 mm nutzbar** |
| schmale Zelle LP-401230 | 12,00 mm |
| Sensor FPC2534 | 11,00 mm |
| **gebraucht** | **23,00 mm** |
| **Fehlbetrag** | **1,40 mm** |

Mit der empfohlenen Zelle (19,75 mm breit) fehlen sogar 9,15 mm. Der Sensor kann
auch nicht in ein anderes Drittel: vorn liegt die Chipantenne des Moduls, hinten
die NFC-Antenne – beide dürfen kein Metall und kein Einlegeteil in der Nähe
haben.

**Das ist eine Entscheidung, nicht ein Rechenfehler.** Zwei Wege:

- **(a) Platine jetzt 26 mm breit statt 24 mm.** Dann sind 23,6 mm nutzbar und
  der Sensor passt in Stufe 2 neben die schmale Zelle. Preis: das Gehäuse wird
  29,8 mm breit statt 27,8 mm. Die Platine bleibt 70 mm lang.
- **(b) Bei 24 mm bleiben.** Dann braucht Stufe 2 eine neue Platine **und** ein
  neues Gehäuse – der Zweck „ein Gehäuse für beide Stufen" ist damit weg.

Gezeichnet ist (b) mit 24 mm, weil das die derzeitige Vorgabe ist. Die
Sensoröffnung liegt an der einzigen Stelle, die das Gehäuse hergibt: Mitte
y = 16,5 mm. Weiter nach außen bricht sie in die Seitenwand. Sie überlappt
damit das Feld der schmalen Zelle um 1,5 mm – **auf der Platine ist das noch
nicht gelöst.**

### 3. Die Bauhöhe des USB-Steckers fehlt in den Unterlagen

`../docs/parts.md` nennt vom GCT USB4085 den Körper (8,94 × 9,17 mm) und die
Öffnung (3,16 mm hoch), **aber nicht, wie hoch er über der Platine steht.**

Gezeichnet ist die Frontöffnung mit **4,0 mm hoch, mittig zur Platinenmitte** –
also unter der Annahme, dass der Steckerkörper etwa gleich weit über und unter
die Platine reicht. **Das ist `ungeprueft`.**

Steht der Körper stattdessen vollständig **über** der Platine, passt er nicht in
den 2,35 mm hohen Hohlraum der Anzeigenseite, und das Gerät wird vorn etwa
1,3 mm dicker oder braucht einen anderen Stecker. **Diese Zahl ist zu besorgen,
bevor gedruckt wird.**

## Wie die Teile zusammenhalten

- **Trennebene = Oberseite der Platine.** Die Platine liegt in der unteren
  Schale auf einer 1,2 mm breiten Stufe rundum.
- Die obere Schale hat innen einen 1,2 mm breiten Kragen, dessen Unterseite
  genau auf der Trennebene liegt. Er drückt die Platine auf die Stufe – **das
  ersetzt Schrauben und Befestigungslöcher.**
- **Stufenfalz:** die äußere Hälfte der Seitenwand (0,8 von 1,6 mm) steigt an der
  unteren Schale 1,0 mm über die Trennebene; die obere Schale hat die passende
  Aufnahme. Das führt die Teile und macht die Fuge dicht gegen Staub.
- **Vier Schnappwülste**, zwei je lange Seite, bei 15 und 45 mm. Jeder ist
  6,0 mm lang, 0,5 mm hoch und steht 0,4 mm vor; in der oberen Schale sitzt die
  passende Tasche. Sie sind absichtlich **nicht an den Schmalseiten** – dort
  liegen USB-Öffnung und Öse.
- Zum Öffnen: an einer langen Seite mit dem Fingernagel oder einem dünnen
  Plättchen in die Fuge, die Wand gibt 0,4 mm nach.

## Für den 3D-Druck ausgelegt

Druckerwerte, mit denen gerechnet ist, stehen in [masse.md](masse.md),
Abschnitt 5 – kurz: **0,4 mm Düse, 0,2 mm Schichthöhe, PETG.**

- **Wandstärken:** Seitenwand 1,6 mm (4 Bahnen), Dach und Boden 1,0 mm
  (5 Schichten), kleinster Steg 0,8 mm (2 Bahnen). Nichts ist dünner.
- **Kein Stützmaterial.** Jede Schale wird mit der Öffnung nach oben gedruckt,
  der Boden liegt auf der Platte. Alle Öffnungen in den Seitenwänden sind
  senkrechte Schlitze. Das Fenster im Dach ist ein Durchbruch in einer flachen
  Decke – das druckt als Brücke.
- **Längste freitragende Brücke: 21,6 mm** – die Decke über dem Hohlraum. Das
  ist für PETG unkritisch; wer es sauber will, druckt die erste Deckenschicht
  langsamer.
- **Überhänge:** die Auflagestufe für die Platine ist ein 1,2 mm breiter
  waagerechter Vorsprung. 1,2 mm trägt sich ohne Stütze; wer auf Nummer sicher
  gehen will, gibt ihr im Schneider eine 45°-Fase.
- **Keine Löcher unter Ø 1,6 mm** und keine Schraubgewinde – gedruckte Gewinde in
  dieser Größe halten nicht.
- **Spiel Teil zu Teil 0,2 mm.** Sitzt der Falz nach dem ersten Druck zu fest
  oder zu lose, ist das der Parameter `lip_t` bzw. der Schneider-Wert
  „horizontale Ausdehnung".

## Rücksicht auf die NFC-Antenne

Aus den Layout-Vorgaben: kein Metall, kein Einlegeteil in der Nähe der Antenne,
und der Akku darf nicht dahinter liegen. Das Gehäuse hält das ein:

- **Kein Einlegeteil, keine Schraube, keine Mutter, kein Gewindeeinsatz** im
  ganzen Gehäuse – es hält allein durch Schnapp und Klemmung. Das ist der
  eigentliche Grund für die schraubenlose Bauweise.
- **Die Öse ist aus demselben Kunststoff** und sitzt hinter der Platine
  (x = 71,9…79,9 mm), nicht über dem Antennenfeld (x = 44…70 mm).
- **Über dem Antennenfeld liegt nur 1,0 mm Kunststoff** – kein Akku, kein
  Blindstück, keine Verstärkungsrippe.
- Die Ferritfolie liegt auf der Zelle, nicht im Gehäuse – das Gehäuse hat an
  dieser Stelle nichts.

**Eine Warnung, die ich nicht wegrechnen kann:** Ein **stählerner
Schlüsselring** im Ösenloch liegt etwa 10 mm von der Kante des Antennenfelds.
Das ist leitfähiges Metall in der Nähe der Antenne, und wie stark es verstimmt,
lässt sich nur **am Netzwerkanalysator messen**, nicht rechnen. Empfehlung:
**eine Textilschlaufe oder ein Kunststoffring**, und die Abstimmung der Antenne
wird mit dem Ring am Gerät gemessen, nicht ohne.

## Parametrisch: wo die Maße herkommen

**Quelle aller Maße ist [parameters.csv](parameters.csv)** – eine Zeile je Maß,
mit Kommentar. Im Körper steht keine Zahl, die nicht von dort kommt.

```text
cad/
├── masse.md          die Dickenrechnung - kommt vor der Zeichnung
├── gehaeuse.md       diese Datei
├── parameters.csv    alle Maße, eine Zeile je Maß
├── housing.py        Fusion-360-Skript: baut und exportiert
├── tools/
│   ├── geometry.py   Form als Quaderliste, aus der CSV gerechnet
│   └── build_mesh.py erzeugt STL und STEP ohne Fusion
└── export/           STL, STEP (und housing.f3d, sobald Fusion gelaufen ist)
```

**Die endgültige Platinengröße eintragen** heißt: in `parameters.csv` die Zeilen
`pcb_len`, `pcb_wid`, `pcb_th` ändern und neu bauen. Alles andere – Außenmaße,
Hohlraum, Auflagestufe, Lage der Öse – rechnet sich daraus. Dasselbe gilt für
die Lage von Fenster, Taster, LED und Sensoröffnung (`*_cx`, `*_cy`), sobald das
Layout sie festlegt.

```powershell
python cad\tools\build_mesh.py
```

Das schreibt STL und STEP neu und prüft dabei selbst, dass jedes Netz dicht ist
(jede Kante genau zweimal, gegenläufig) und ein positives Volumen hat. Ohne die
Zeile `check: ok` am Ende ist der Lauf nicht durchgelaufen.

In Fusion 360: `cad/housing.py` als Skript hinzufügen, eine **leere** neue
Konstruktion öffnen, ausführen. Es legt die Maße als Benutzerparameter an, baut
die drei Teile und schreibt STEP, STL und `housing.f3d` nach `export/`.

## Was geprüft ist und was nicht

| Stufe | Stand |
| --- | --- |
| Maße gerechnet | **ja** – [masse.md](masse.md), jede Zahl mit Quelle |
| Netz dicht und Volumen positiv | **ja** – Selbstprüfung in `build_mesh.py`, `check: ok` |
| STL erzeugt | **ja** – 3 Dateien in `export/` |
| STEP erzeugt | **ja**, aber **in keinem CAD geöffnet** – auf diesem Rechner ist keines vorhanden. Das Skript prüft nur, dass jede benutzte Nummer vergeben ist. **`ungeprueft`** |
| Fusion-Datei `housing.f3d` | **nein** – Fusion läuft hier nicht. Entsteht beim ersten Lauf von `housing.py` |
| `housing.py` in Fusion gelaufen | **nein** – nur auf Syntax geprüft (`py_compile`). **`ungeprueft`** |
| Gedruckt, Platine eingelegt, Passung geprüft | **nein** – die Platine gibt es nicht |

**Das STL ist eckig gerechnet:** Gehäuseecken, Lichtfenster, Sensoröffnung und
Ösenloch sind im STL **Quadrate**. Die Rundungen (Ecken R 3 mm, Löcher rund)
macht `housing.py` in Fusion. Das STL ist damit zum **Maßprüfen** gut und zum
**Vorzeigen**; wer das fertige Teil drucken will, lässt erst Fusion laufen.
Der Grund steht offen im Kopf von `build_mesh.py`: ein exakter Schnitt
achsparalleler Quader ist in reinem Python machbar und nachprüfbar, ein runder
nicht.

## Was das Layout wissen muss

Die vollständige Liste steht in [masse.md](masse.md), Abschnitt 7. Das
Wichtigste:

1. **Das Gehäuse verträgt 24 × 70 mm, 0,8 mm dick** – genau das Maß aus den
   Layout-Vorgaben. Toleranz ohne Nacharbeit: **± 0,2 mm**. Jede größere
   Änderung ist eine Zeile in `parameters.csv` und ein neuer Druck, kein
   Neuentwurf.
2. **Rundum 1,2 mm Platinenrand oben und unten frei von Bauteilen.** Dort wird
   geklemmt. Ohne diesen Rand hält die Platine nicht.
3. **Platinenecken R 1,5 mm abrunden.**
4. **Höhengrenzen: 2,05 mm auf der Anzeigenseite, 4,20 mm auf der Akkuseite**
   (Ferritfolie mitgerechnet). Wer höher baut, macht das Gerät dicker.
5. **Will Stufe 2 den Sensor, muss die Platine 26 mm breit werden** – siehe
   Befund 2. Diese Entscheidung fällt jetzt, nicht später.
6. **Die Bauhöhe des USB4085 über der Platine ist zu besorgen** – siehe
   Befund 3.
7. **Die Anzeige braucht ein 0,8 mm dickes Klebepad**, damit ihr Glas bündig mit
   dem Modul abschließt und man nicht durch einen Schacht sieht.

## Offene Punkte

1. **Bauhöhe des USB4085** – Befund 3. Ohne sie ist die Frontöffnung geraten.
2. **Sensor und Platinenbreite** – Befund 2, Entscheidung (a) oder (b).
3. **Lage von Fenster, Taster, LED und Sensoröffnung** ist **vorläufig**
   (`*_cx`, `*_cy` in der CSV sind mit `VORLAEUFIG` gekennzeichnet). Sie kommt
   aus dem Layout, sobald es gezeichnet ist.
4. **Ferritfolie**: Dicke 0,3–0,5 mm angenommen, Material offen (offener Punkt 3
   der Layout-Vorgaben). ±0,2 mm Gesamtdicke.
5. **Platz für die umgelegte FPC-Fahne der Anzeige** ist nicht bemaßt – der
   Hohlraum ist 2,35 mm hoch, ob das reicht, sagt erst das Layout.
6. **Freihaltezone um die Chipantenne des Moduls** (offener Punkt 1 der
   Layout-Vorgaben) – davon hängt ab, ob die Seitenwand dort wegbleiben muss.
7. **Welcher 3D-Drucker** – die Druckerwerte sind Annahmen für einen
   gewöhnlichen Schmelzdrucker.
8. **Schlüsselring aus Stahl neben der NFC-Antenne** – nur messbar, nicht
   rechenbar.
