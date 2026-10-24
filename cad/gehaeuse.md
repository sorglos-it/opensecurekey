# Gehäuse

Stand 24.10.2026. **Gezeichnet, nichts gedruckt, nichts angepasst.** Die
Dickenrechnung steht in [masse.md](masse.md) und ist die Grundlage dieser Datei.

## Was es ist

Zwei gedruckte Schalen, die Platine dazwischen geklemmt, ohne eine einzige
Schraube. Der USB-C-Anschluss schaut vorn an der kurzen Kante heraus, die Öse
für den Schlüsselring sitzt hinten.

| Teil | Datei | Was es ist |
| --- | --- | --- |
| Obere Schale | `export/shell-top.*` | Anzeigenseite: Fenster, Lichtfenster der LED, Schlitz für SW1, Blindtasche für SW2 |
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

### 2. Die Platine muss 26 mm breit werden, nicht 24

**Entschieden am 24.10.2026 und eingearbeitet.** Der Grund ist der
Fingerabdrucksensor der Stufe 2:

| | bei 24 mm | **bei 26 mm** |
| --- | --- | --- |
| nutzbar quer (Platine − 2 × 1,2 mm bauteilfreier Rand) | 21,60 mm | **23,60 mm** |
| gebraucht: schmale Zelle 12,0 + Sensor FPC2534 11,0 | 23,00 mm | 23,00 mm |
| | **1,40 mm zu wenig** | **0,60 mm Reserve** |

In ein anderes Drittel kann der Sensor nicht: vorn liegt die Chipantenne des
Moduls, hinten die NFC-Antenne – beide dürfen kein Metall und kein Einlegeteil
in der Nähe haben.

**Was die 2 mm kosten:** das Gehäuse wird **29,8 mm breit statt 27,8 mm**. Die
Dicke bleibt bei 9,55 mm, die Länge bei 70 mm. Das ist der ganze Preis.

**Was sie voraussetzen:** die **schmale Zelle** LP-401230-J (12,0 mm breit,
100 mAh). Mit der empfohlenen Zelle (Adafruit 1317, 19,75 mm) wären 30,75 mm
nötig – das geht auch auf 26 mm nicht. Wer den Sensor will, zahlt mit der
Laufzeit: etwa 4,6 statt 6,8 Monate (`../docs/power-budget.md`).

**Stufe 1 ohne Sensor darf weiter die große Zelle nehmen** – der Akkuraum ist
23,6 mm breit. Das Gehäuse trägt beide Stufen, und genau dafür sind die 2 mm
ausgegeben.

Aufteilung quer, als Vorgabe ans Layout: **Zelle y = 0…12 mm, Sensorfeld
y = 12,5…23,5 mm.** Die Sensoröffnung des Gehäuses liegt auf Mitte y = 18,0 mm.

### 3. Der USB-Stecker passt nicht in den Hohlraum

**Die Befürchtung hat sich bestätigt.** GCTs Zeichnung (Rev. A1, 09.04.2018)
nennt das Teil im Schriftfeld „USB 2.0 Type C Receptacle, Dip Type, **PCB Top
Mount**" – der Körper sitzt vollständig **auf** der Platine.

| Bauhöhe laut Zeichnung | Hohlraum | fehlt |
| --- | --- | --- |
| 3,16 mm (Blechgehäuse außen) | 2,35 mm | **0,81 mm** |
| 3,46 mm (0,30 mm tiefer gemessen) | 2,35 mm | **1,11 mm** |

Welche der beiden über der Platinenoberseite liegt, ist aus der Zeichnung nicht
sicher abzulesen (`ungeprueft`) – **es ändert an der Entscheidung nichts, beide
passen nicht.** Nebenbei berichtigt: die „Öffnung 3,16 mm" aus
`../hardware/layout-notes.md` ist nicht die Öffnung. Die Öffnung ist
**8,35 × 2,56 mm**, 3,16 mm ist das Blechgehäuse außen. Rechnung und Quellen in
[masse.md](masse.md), Abschnitt 9.

**Zwei Wege, und das Gehäuse hat dazu keine Meinung – es nennt nur den Preis:**

| Weg | Dicke | Gehäuseänderung |
| --- | --- | --- |
| **A: vorn dicker** | **10,66 mm** vorn, 9,55 mm sonst | Nase über 8,95 × 7,26 mm |
| **B: Mid-Mount-Stecker** | **9,55 mm, unverändert** | **keine** |

Bei Weg B stünde der Stecker nur noch etwa 2,13 mm über der Platine und passte
in den vorhandenen Hohlraum. Preis: ein anderes Teil und ein Ausschnitt in der
Platine.

**Solange das nicht entschieden ist, bleibt die Frontöffnung so, wie sie ist**
(4,0 mm hoch, mittig zur Platinenmitte) – sie passt zu Weg B und ist bei Weg A
in einem Zug nachzuziehen.

### 4. Der Schnappfalz stand vor zwei Öffnungen – behoben

Gefunden am 24.10.2026 beim Nachrechnen der Taster-Übergabe. Der Falz der
unteren Schale steigt 1,0 mm über die Trennebene – **und stand damit mitten vor
der Tasteröffnung (Taster 0…1,35 mm) und vor der USB-Öffnung
(−2,4…+1,6 mm).** Der Taster wäre nicht zu drücken gewesen, das Kabel nicht zu
stecken.

Behoben: der Falz ist an beiden Stellen unterbrochen. Nachgewiesen mit
Stichproben im Körper (`frei` dort, wo die Öffnung ist, `voll` daneben) – die
Prüfung steht unter „Was geprüft ist".

**Das war kein Rechenfehler in den Maßen, sondern ein Loch in der Form**, das
die Dickenrechnung nicht gezeigt hätte. Es ist nur aufgefallen, weil der
Platinen-Strang nach den Tastern gefragt hat.

## Die beiden Taster

Lage aus der Übergabe des Platinen-Strangs: beide bei **längs
22,75…26,25 mm**, an den **Längskanten**, seitlich betätigt, 1,35 mm hoch.

**Der enge Punkt der Übergabe – 0,75 mm zum Anzeigeglas – ist nicht die Grenze
des Rahmens.** Das Fenster ist ein glatter Durchbruch im Dach ohne nach unten
gezogenen Rand und beginnt erst bei 30,15 mm: **3,90 mm vom Taster entfernt.**
Quer liegt das Glas 4,85 mm neben dem Klemmkragen. Die vollständige Rechnung
steht in [masse.md](masse.md), Abschnitt 8.

**Was der Rahmen wirklich brauchte, war etwas anderes:** der Klemmkragen läuft
1,2 mm breit am Platinenrand um – genau dort, wo ein seitlich betätigter Taster
sitzt. Er ist jetzt an **beiden** Längskanten über 22,25…26,75 mm bis auf
1,65 mm Höhe freigestellt. Das unterbricht die Klemmung an zwei Stellen um je
4,5 mm von rund 192 mm Umfang – das trägt sie.

| Taster | Kante | Gehäuse |
| --- | --- | --- |
| SW1 (Bestätigen) | quer 0 | **Schlitz durch die Wand**, 4,5 × 2,4 mm, 0,50 mm Luft je Seite |
| SW2 (Reset) | quer 26 | **Blindtasche 0,8 mm**, Wand bleibt 0,8 mm stehen – von außen nicht erreichbar |

SW2 bekommt keine durchgehende Öffnung, weil `../hardware/layout-notes.md` das
ausdrücklich verlangt: der Reset-Taster darf von außen nicht zu drücken sein.
Ganz zumachen geht aber auch nicht – eine geschlossene Wand stünde 0,3 mm vor
dem Stößel und könnte ihn dauerhaft gedrückt halten. Die Blindtasche gibt ihm
**1,10 mm ab Platinenrand**.

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
- **Längste freitragende Brücke: 23,6 mm** – die Decke über dem Hohlraum. Das
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
| Jede Öffnung wirklich offen | **ja** – 10 Proben im Körper (`frei` in der Öffnung, `voll` daneben). Gegenprobe gemacht: schiebt man den Falz wieder davor, fällt die Probe – sie ist also nicht hohl |
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

1. **Das Gehäuse verträgt 26 × 70 mm, 0,8 mm dick** – die Breite ist gegenüber
   den Layout-Vorgaben von 24 auf 26 mm erhöht, Begründung in Befund 2. Die
   Länge und die Dicke bleiben, wie sie dort stehen. Toleranz ohne Nacharbeit:
   **± 0,2 mm** je Maß. Jede größere
   Änderung ist eine Zeile in `parameters.csv` und ein neuer Druck, kein
   Neuentwurf.
2. **Rundum 1,2 mm Platinenrand oben und unten frei von Bauteilen.** Dort wird
   geklemmt. Ohne diesen Rand hält die Platine nicht.
3. **Platinenecken R 1,5 mm abrunden.**
4. **Höhengrenzen: 2,05 mm auf der Anzeigenseite, 4,20 mm auf der Akkuseite**
   (Ferritfolie mitgerechnet). Wer höher baut, macht das Gerät dicker.
5. **Stufe 2 mit Sensor braucht die schmale Zelle** (LP-401230-J, 12 mm) und die
   Aufteilung quer: Zelle y = 0…12 mm, Sensorfeld y = 12,5…23,5 mm. Siehe
   Befund 2. Stufe 1 ohne Sensor darf die große Zelle nehmen.
6. **Die Bauhöhe des USB4085 über der Platine ist zu besorgen** – siehe
   Befund 3.
7. **Die Anzeige braucht ein 0,8 mm dickes Klebepad**, damit ihr Glas bündig mit
   dem Modul abschließt und man nicht durch einen Schacht sieht.

## Offene Punkte

1. **Welcher USB-Weg** – Befund 3. Entscheidung A oder B liegt bei Thomas. Rein
   aus Sicht des Gehäuses ist B besser: es ändert nichts und bleibt bei 9,55 mm.
   Offen bleibt dabei nur, ob 3,16 oder 3,46 mm über der Platine stehen – das
   braucht erst Weg A, für die Entscheidung reicht es nicht.
2. **Zellenwahl für Stufe 2** – die Platinenbreite ist entschieden (26 mm,
   Befund 2), die Zelle nicht: der Sensor erzwingt die schmale mit 100 mAh.
   Welche Laufzeit es sein soll, ist nicht meine Entscheidung.
3. **Körperlänge des EVQP7C01P** – Panasonic sagt 3,5 mm, DigiKey 4,70 mm. Die
   Freistellung ist mit 5,6 mm so gewählt, dass **beide Lesarten passen**
   (1,05 bzw. 0,45 mm Luft je Seite). Sollte am Footprint auffallen, welche
   stimmt: Bescheid sagen, dann kann die Freistellung wieder schrumpfen.
   Der Stößel ist geklärt – 0,65 mm plus 0,2 mm Hub, Reserve 0,25 mm.
4. **Lage der LED** ist vorläufig auf längs 24,5 mm gesetzt, quer mittig –
   zwischen Modulende (21,5) und Glasanfang (27). Vom Layout nicht genannt.
   **Wird gebraucht**, weil das Lichtfenster im Dach danach sitzt.
5. **Lage der Sensoröffnung** ist vorläufig (längs 36, quer 18). Fenster und
   Taster stehen jetzt auf den Werten der Übergabe des Platinen-Strangs.
6. **Ferritfolie**: Dicke 0,3–0,5 mm angenommen, Material offen (offener Punkt 3
   der Layout-Vorgaben). ±0,2 mm Gesamtdicke.
7. **Platz für die umgelegte FPC-Fahne der Anzeige** ist nicht bemaßt – der
   Hohlraum ist 2,35 mm hoch, ob das reicht, sagt erst das Layout.
8. **Freihaltezone um die Chipantenne des Moduls** (offener Punkt 1 der
   Layout-Vorgaben) – davon hängt ab, ob die Seitenwand dort wegbleiben muss.
9. **Welcher 3D-Drucker** – die Druckerwerte sind Annahmen für einen
   gewöhnlichen Schmelzdrucker.
10. **Schlüsselring aus Stahl neben der NFC-Antenne** – nur messbar, nicht
    rechenbar.
11. **Der Platinen-Strang rechnet noch mit 24 mm Breite.** Thomas hat 26 mm
    entschieden (Befund 2); die Taster-Übergabe nennt quer 0…24. Die Längsmaße
    der Taster sind davon nicht betroffen, die Querkante schon: SW2 sitzt dann
    an quer 26, nicht 24. **Muss beim Layout ankommen.**
