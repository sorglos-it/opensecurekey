# Gehäuse

Stand 24.10.2026. **Gezeichnet, nichts gedruckt, nichts angepasst.** Die
Rechnung dahinter steht in [masse.md](masse.md).

> **Dieses Modell passt zu einer Platine, die es so nicht mehr geben wird.**
> Das Gerät wird ein Stick zum direkten Einstecken, die USB-Buchse ist schon
> gegen einen Platinenrand-Stecker getauscht. Was das für das Gehäuse heißt,
> steht am Ende unter
> [Was sich durch den Steckerumbau ändert](#was-sich-durch-den-steckerumbau-ändert).
> Wer hier etwas ändert, liest das zuerst.

## Was es ist

Zwei gedruckte Schalen, die Platine dazwischen geklemmt, **ohne eine einzige
Schraube**. Vorn eine Nase über dem USB-Anschluss, hinten die Öse für den
Schlüsselring.

| Maß | Wert |
| --- | --- |
| Länge mit Öse | **81,8 mm** |
| Breite | **30,8 mm** |
| Dicke hinten | **9,80 mm** |
| Dicke vorn (Nase) | **11,01 mm** |

| Teil | Datei | Was es ist |
| --- | --- | --- |
| Obere Schale | `export/shell-top.*` | Anzeigenseite: Fenster, Lichtfenster der LED, Schlitz für SW1, Blindtasche für SW2, Nase über dem Stecker |
| Untere Schale | `export/shell-bottom.*` | Akkuseite: Platinentasche, Zellenraum, Sensoröffnung, Öse, Schnappfalz |
| Blindstück | `export/sensor-plug.*` | schließt den Sensorsitz, solange kein Sensor eingesetzt ist |

Je Teil liegt ein **STL** (zum Drucken) und ein **STEP** (zum Weiterarbeiten
ohne Fusion) in `export/`.

## Die Befunde, die der Entwurf hervorgebracht hat

### 1. Das Gerät wird 9,80 mm dick, nicht 8–9 mm

Die Schätzung in `../hardware/layout-notes.md` war eine Addition ohne Gehäuse.
Mit Wandstärken, Luftspalten und Ferritfolie sind es **9,80 mm** hinten,
realistisch 9,8–10,3 mm. Vorn kommt die Nase dazu: **11,01 mm**.

Gewählt ist die **Adafruit 1317** mit 3,80 mm. Damit käme die Rechnung auf
9,55 mm – der Hohlraum ist trotzdem 4,65 mm tief, also auf die 4,05 mm dicke
Rückfallzelle ausgelegt. **Die 0,25 mm sind Absicht:** das Gehäuse trägt damit
beide Zellen, und die Zelle hat Platz zum Quellen. Die Zelle hat heute dreimal
gewechselt; ein Gehäuse, das nur zu einer passt, wird beim nächsten Wechsel neu
gedruckt. Wer die 9,55 mm will, ändert eine Zeile – [masse.md](masse.md),
Abschnitt 3.

### 2. Der Fingerabdrucksensor hat die Platinenbreite bestimmt

Auf 24 mm passt er nicht neben die Zelle, und in ein anderes Drittel kann er
nicht – vorn die Chipantenne, hinten die NFC-Antenne.

| Platine | nutzbar quer | gebraucht | Reserve |
| --- | --- | --- | --- |
| 24 mm | 21,60 | 23,50 | **fehlt 1,90** |
| 26 mm | 23,60 | 23,50 | 0,10 |
| **27 mm** | **24,60** | 23,50 | **1,10** |

**Thomas hat 27 mm entschieden.** Das kostet 3 mm Gehäusebreite gegenüber dem
ursprünglichen Entwurf und ändert die Dicke nicht. Begründung und die Zahl, die
dafür fehlt, in [masse.md](masse.md), Abschnitt 4.

### 3. Der USB-Stecker passt nicht in den Hohlraum – darum die Nase

GCTs Zeichnung nennt das Teil „USB 2.0 Type C Receptacle, Dip Type, **PCB Top
Mount**": der Körper sitzt vollständig **auf** der Platine und trägt 3,46 mm
auf. Der Hohlraum hat 2,35 mm.

**Thomas hat entschieden, bei der Buchse zu bleiben.** Also wird das Dach über
dem Stecker angehoben:

| Maß | Wert |
| --- | --- |
| Hohlraum unter der Nase | 3,56 mm |
| Nase reicht von der Front bis | 10,20 mm |
| Stufe gegenüber dem hinteren Dach | **1,21 mm** |
| Wand über der Frontöffnung | 0,80 mm |
| **Mulde um die Öffnung** | **0,80 mm tief, 14,00 mm breit** – erzwungen, nicht Form |

Volle Breite, senkrechte Stufe, keine Fase – das druckt ohne Stütze und sieht
gewollt aus.

### 4. Der Sensor gehört in den Boden, nicht auf die Platine

Gerechnet, als der Platinen-Strang nach der Seite fragte. **Die Seite ist
„unten" – aber auf der Platine wäre der Sensor gar nicht zu erreichen:**

| Ebene | z |
| --- | --- |
| Sensorfläche, wenn er auf der Platine sitzt | −1,63 mm |
| **Bodenaußenseite – dort liegt der Finger** | **−6,45 mm** |
| **Abstand** | **4,82 mm** |

Ein kapazitiver Sensor liest durch Bruchteile eines Millimeters, nicht durch
fünf. Und der Hohlraum lässt sich nicht flacher machen: 0,2 Luft + 0,4 Ferrit
+ 4,05 Zelle = 4,65 mm, genau seine Tiefe.

Das passt zur Bauform – der FPC2534**AM** ist ein **Flex-Modul**, gemacht zum
Einkleben in ein Gehäuse. Also sitzt er **im Boden**: glattes Loch Ø 11,30 mm,
Sensor von innen eingeklebt, Fläche **0,17 mm hinter der Außenfläche**.

**Folge für die Platine:** sie braucht neben der Zelle keine 11 mm freie Fläche
mehr, nur den ZIF-Anschluss und einen Weg fürs Flachband. **Damit ist der Grund
für die 27 mm möglicherweise entfallen** – das zu entscheiden ist nicht meine
Sache, und das Gehäuse trägt jede der drei Breiten. Einzelheiten in
[masse.md](masse.md), Abschnitt 14.

### 5. Der Schnappfalz stand vor zwei Öffnungen

Gefunden beim Nachrechnen der Taster-Übergabe. Der Falz der unteren Schale
steigt 1,0 mm über die Trennebene und **stand damit mitten vor der
Tasteröffnung und vor der USB-Öffnung.** Der Taster wäre nicht zu drücken, das
Kabel nicht zu stecken gewesen.

Behoben und belegt: der Falz ist an beiden Stellen unterbrochen, und zwölf
Proben im Körper prüfen das bei jedem Bau nach.

**Das war kein Rechenfehler, sondern ein Loch in der Form** – keine
Dickenrechnung hätte es gezeigt. Aufgefallen ist es nur, weil der
Platinen-Strang nach den Tastern gefragt hat.

## Die beiden Taster

**SW1** bei **längs 22,75…26,25 mm** neben der Anzeige, **SW2** seit dem
24.10.2026 am USB-Ende bei **längs 6,8912…10,3912** (Pads 6,1402…11,1419,
Mitte 8,6412) – beide an den Längskanten, seitlich betätigt. Der Reset ist
umgezogen, weil ihm zwischen Funkmodul und Anzeigeglas **0,5491 mm** fehlten;
das hat der Platinen-Strang gerechnet, nicht das Gehäuse.

Der enge Punkt der Übergabe – 0,75 mm zum Anzeigeglas – ist **nicht die Grenze
des Rahmens**: das Fenster ist ein glatter Durchbruch im Dach ohne nach unten
gezogenen Rand und beginnt erst bei 30,15 mm, also **3,90 mm vom Taster**.

**Was der Rahmen wirklich brauchte, war etwas anderes:** der Klemmkragen läuft
1,2 mm breit am Platinenrand um – genau dort, wo ein seitlich betätigter Taster
sitzt. Er ist freigestellt bis auf 1,65 mm Höhe, an der Kante quer 0 über
**21,70…27,30 mm** (SW1) und an der Kante quer 27 über **5,8412…11,4412**
(SW2). Der Umzug ändert an der Klemmung nichts – die Unterbrechung ist gleich
lang, sie liegt nur woanders.

**Nachgemessen statt gerechnet** (Mittellinie des Kragens abgetastet, 0,6 mm
innen, Schrittweite 0,1 µm): bei 194,0 mm Umfang **tragen 172,3 mm – 88,8 %**.
Es sind **drei** Unterbrechungen, nicht zwei:

| Lücke | Länge |
| --- | --- |
| SW1-Freistellung, längs 21,70…27,30 | 5,6 mm |
| SW2-Freistellung, längs 5,841…11,441 | 5,6 mm |
| **USB-Öffnung in der Frontkante** | **10,5 mm** |

Die dritte stand in keiner früheren Kragenrechnung – die Zahl „zwei
Unterbrechungen bei rund 192 mm Umfang" hat sie übersehen, auch schon vor dem
Umzug. Durchgehend bleiben: **128,26 mm** am Stück, dann **14,09 mm** zwischen
SW2-Freistellung und USB-Öffnung um die vordere Ecke herum, dann **8,25 mm**
auf der anderen Seite der USB-Öffnung. Das kürzeste Stück ist damit nicht vom
Umzug verursacht.

**Ob das klemmt, ist damit nicht gezeigt.** Gerechnet ist Geometrie, keine
Kraft; eine Klemmkraft steht nirgends in den Unterlagen. `offen`

**Kollision mit der Frontpartie: keine.** Nachgerechnet gegen die drei Dinge,
die am USB-Ende im Weg stehen könnten:

| Geprüft | Zahl |
| --- | --- |
| Freistellung zur Mulde (endet an der Platinenkante 0,0) | **5,8412 mm** |
| Steckerkörper quer 9,03…17,97 zum Tasterkörper quer 21,725…25,275 | **3,7550 mm** |
| Mulde quer 6,50…20,50 zum Tasterkörper | **1,2250 mm** |
| Freistellung zum Funkmodul-Körper (ab längs 12,0499) | **0,6087 mm** |
| Freistellung zum ersten Schnappwulst (ab längs 12,0) | **0,5588 mm** |
| Pads 6,1402…11,1419 in der 5,6 mm langen Freistellung | **0,2990 / 0,2993 mm** |

Der Steckerkörper reicht längs bis 7,27 mm und überschneidet den Taster in
Längsrichtung um 0,3788 mm – **quer liegen sie 3,755 mm auseinander**, sie
treffen sich also nicht. Die Nase über dem Stecker beginnt erst bei 2,35 mm
Höhe, die Freistellung endet bei 1,65 mm; sie liegen übereinander, nicht
ineinander.

| Taster | Kante | Gehäuse |
| --- | --- | --- |
| SW1 (Bestätigen) | quer 0, längs 24,5 | **Schlitz durch die Wand**, 4,5 × 2,4 mm |
| SW2 (Reset) | quer 27, längs 8,6412 | **Blindtasche 0,8 mm**, von außen nicht erreichbar |

SW2 bekommt keine durchgehende Öffnung, weil `../hardware/layout-notes.md` das
verlangt. Ganz zumachen geht aber auch nicht – eine geschlossene Wand stünde
0,3 mm vor dem Stößel und könnte ihn dauerhaft gedrückt halten. Die Tasche gibt
ihm 1,10 mm, gebraucht werden 0,85 mm.

## Wie die Teile zusammenhalten

- **Trennebene = Oberseite der Platine.** Die Platine liegt in der unteren
  Schale auf einer 1,2 mm breiten Stufe rundum.
- Die obere Schale hat innen einen 1,2 mm breiten Kragen, dessen Unterseite
  genau auf der Trennebene liegt. Er drückt die Platine auf die Stufe – **das
  ersetzt Schrauben und Befestigungslöcher.**
- **Stufenfalz:** die äußere Hälfte der Seitenwand (0,8 von 1,6 mm) steigt an
  der unteren Schale 1,0 mm über die Trennebene; die obere Schale hat die
  passende Aufnahme. Das führt die Teile und hält die Fuge dicht gegen Staub.
- **Vier Schnappwülste**, zwei je lange Seite, bei 15 und 45 mm. Jeder 6,0 mm
  lang, 0,5 mm hoch, 0,4 mm vorstehend. Absichtlich **nicht an den
  Schmalseiten** – dort liegen USB-Öffnung und Öse.
- Zum Öffnen: an einer langen Seite mit dem Fingernagel in die Fuge, die Wand
  gibt 0,4 mm nach.

## Für den 3D-Druck ausgelegt

Druckerwerte in [masse.md](masse.md), Abschnitt 7 – kurz: **0,4 mm Düse, 0,2 mm
Schichthöhe, PETG.**

- **Wandstärken:** Seitenwand 1,6 mm (4 Bahnen), Dach und Boden 1,0 mm
  (5 Schichten), kleinster Steg 0,8 mm. Nichts ist dünner.
- **Kein Stützmaterial.** Jede Schale wird mit der Öffnung nach oben gedruckt.
  Alle Öffnungen in den Seitenwänden sind senkrechte Schlitze, das Fenster im
  Dach ist ein Durchbruch in einer flachen Decke – das druckt als Brücke.
- **Längste freitragende Brücke: 24,6 mm** – die Decke über dem Hohlraum. Für
  PETG unkritisch; wer es sauber will, druckt die erste Deckenschicht langsamer.
- **Die Nase ist eine senkrechte Stufe von 1,21 mm** – kein Überhang, keine
  Stütze, keine Fase nötig.
- **Überhänge:** die Auflagestufe ist ein 1,2 mm breiter waagerechter Vorsprung.
  Das trägt sich ohne Stütze; wer sichergehen will, gibt ihr im Schneider eine
  45°-Fase.
- **Keine Löcher unter Ø 1,6 mm**, keine gedruckten Gewinde.
- **Spiel Teil zu Teil 0,2 mm.** Sitzt der Falz nach dem ersten Druck zu fest
  oder zu lose, ist das `lip_t` bzw. die „horizontale Ausdehnung" im Schneider.

## Rücksicht auf die NFC-Antenne

Aus den Layout-Vorgaben: kein Metall, kein Einlegeteil in der Nähe der Antenne,
und der Akku darf nicht dahinter liegen. Das Gehäuse hält das ein:

- **Kein Einlegeteil, keine Schraube, keine Mutter, kein Gewindeeinsatz** im
  ganzen Gehäuse. Das ist der eigentliche Grund für die schraubenlose Bauweise.
- **Die Öse ist aus demselben Kunststoff** und sitzt hinter der Platine
  (längs 71,9…79,9 mm), nicht über dem Antennenfeld.
- **Über dem Antennenfeld liegt nur 1,0 mm Kunststoff** – kein Akku, kein
  Blindstück, keine Verstärkungsrippe.

**Eine Warnung, die ich nicht wegrechnen kann:** Ein **stählerner
Schlüsselring** im Ösenloch liegt etwa 10 mm von der Kante des Antennenfelds.
Wie stark das verstimmt, lässt sich nur **am Netzwerkanalysator messen**.
Empfehlung: **eine Textilschlaufe oder ein Kunststoffring**, und die Abstimmung
wird mit dem Ring am Gerät gemessen, nicht ohne.

## Parametrisch: wo die Maße herkommen

**Quelle aller Maße ist [parameters.csv](parameters.csv)** – eine Zeile je Maß,
mit Kommentar. Im Körper steht keine Zahl, die nicht von dort kommt.

```text
cad/
├── masse.md          die Rechnung - kommt vor der Zeichnung
├── gehaeuse.md       diese Datei
├── parameters.csv    alle Maße, eine Zeile je Maß
├── housing.py        Fusion-360-Skript: baut und exportiert
├── tools/
│   ├── geometry.py   Form als Quaderliste, aus der CSV gerechnet
│   └── build_mesh.py erzeugt STL und STEP ohne Fusion
└── export/           STL, STEP (und housing.f3d, sobald Fusion gelaufen ist)
```

**Eine neue Platinengröße eintragen** heißt: in `parameters.csv` die Zeilen
`pcb_len`, `pcb_wid`, `pcb_th` ändern und neu bauen. Alles andere – Außenmaße,
Hohlraum, Auflagestufe, Lage der Öse – rechnet sich daraus. Dasselbe gilt für
die Lage von Fenster, Taster, LED und Sensoröffnung (`*_cx`, `*_cy`).

```powershell
python cad\tools\build_mesh.py
```

Das schreibt STL und STEP neu und prüft dabei selbst, dass jedes Netz dicht ist,
ein positives Volumen hat und **jede Öffnung wirklich offen ist**. Ohne die
Zeile `check: ok` am Ende ist der Lauf nicht durchgelaufen.

In Fusion 360: `cad/housing.py` als Skript hinzufügen, eine **leere** neue
Konstruktion öffnen, ausführen. Es legt die Maße als Benutzerparameter an, baut
die drei Teile und schreibt STEP, STL und `housing.f3d` nach `export/`.

## Was geprüft ist und was nicht

| Stufe | Stand |
| --- | --- |
| Maße gerechnet | **ja** – [masse.md](masse.md), jede Zahl mit Quelle |
| Netz dicht und Volumen positiv | **ja** – Selbstprüfung in `build_mesh.py`, `check: ok` |
| Jede Öffnung wirklich offen | **ja** – 12 Proben im Körper (`frei` in der Öffnung, `voll` daneben). Gegenprobe gemacht: schiebt man den Falz wieder davor, fallen sie – die Probe ist also nicht hohl |
| STL erzeugt | **ja** – 3 Dateien in `export/` |
| STEP erzeugt | **ja**, aber **in keinem CAD geöffnet** – auf diesem Rechner ist keines vorhanden. Das Skript prüft nur, dass jede benutzte Nummer vergeben ist. **`ungeprueft`** |
| Fusion-Datei `housing.f3d` | **nein** – Fusion läuft hier nicht |
| `housing.py` in Fusion gelaufen | **nein** – nur auf Syntax geprüft. **`ungeprueft`** |
| Gedruckt, Platine eingelegt, Passung geprüft | **nein** – die Platine gibt es nicht |

**Das STL ist eckig gerechnet:** Gehäuseecken, Lichtfenster, Sensoröffnung und
Ösenloch sind im STL **Quadrate**. Die Rundungen (Ecken R 3 mm, Löcher rund)
macht `housing.py` in Fusion. Das STL ist damit zum **Maßprüfen** gut und zum
**Vorzeigen**; wer das fertige Teil drucken will, lässt erst Fusion laufen. Der
Grund steht im Kopf von `build_mesh.py`: ein exakter Schnitt achsparalleler
Quader ist in reinem Python machbar und nachprüfbar, ein runder nicht.

## Was das Layout wissen muss

Vollständig in [masse.md](masse.md), Abschnitt 13. Das Wichtigste:

1. **Platine 27 × 70 mm, 0,8 mm dick.** Toleranz ohne Nacharbeit ± 0,2 mm.
2. **Rundum 1,2 mm Platinenrand oben und unten frei von Bauteilen.** Dort wird
   geklemmt; ohne diesen Rand hält die Platine nicht.
3. **Platinenecken R 1,5 mm abrunden.**
4. **Höhengrenzen: 2,05 mm oben** (außer der USB-Buchse auf den ersten 10,2 mm)
   **und 4,45 mm unten**, Ferritfolie mitgerechnet.
5. **Aufteilung quer: Zelle 1,20…13,20, Sensorfeld 13,70…24,70 mm.** Der Sensor
   bleibt neben der Zelle, nie über ihr.
6. **Vorderkante der USB-Buchse bündig mit dem Muldengrund** (± 0,3 mm). Der
   Muldengrund steht **1,10 mm** vor der Platinenkante, die Buchsenmitte liegt
   damit auf längs **3,485 mm**. **Die Mulde ist keine Formgebung** – ohne sie
   lägen die Bohrlöcher der Haltestifte der Buchse 0,195 mm über der
   Platinenkante, und das ist ein Fertigungsfehler. Einzelheiten in
   [masse.md](masse.md), Abschnitt 15.
7. **Bauteile im Hohlraum beginnen bei quer 1,40 mm**, nicht bei 1,20: dort
   steht die Hohlraumwand, und gedruckte Flächen haben Toleranz.
8. **Die Anzeige braucht ein 0,8 mm dickes Klebepad**, damit ihr Glas bündig mit
   dem Modul abschließt und man nicht durch einen Schacht sieht.

## Offene Punkte

1. **Ob der Stecker 3,16 oder 3,46 mm über der Platine steht** – gerechnet ist
   mit der größeren Zahl, also der sicheren. Betrifft nur die Höhe der Nase.
2. **Wie stark die Zelle über die Lebensdauer quillt** – steht in
   `../docs/parts.md` nicht. Dafür sind die 0,25 mm Reserve im Zellenraum da.
3. **Körperlänge des EVQP7C01P** – Panasonic sagt 3,5 mm, DigiKey 4,70 mm. Die
   Freistellung ist mit 5,6 mm so gewählt, dass **beide Lesarten passen**.
   Sollte am Footprint auffallen, welche stimmt: Bescheid sagen.
4. **Länge und Abgang des Flachbands am FPC2534AM** – steht in
   `../docs/fingerprint.md` nicht. Davon hängt ab, wo der Sitz im Boden liegen
   darf. Bis dahin steht er auf längs 36,0 / quer 19,20 mm. **Wird gebraucht.**
5. **Bauhöhe des Akkusteckers J2 über der Platine.** Ich hatte das als
   *fehlende Angabe* gemeldet – **es ist ein fehlendes Teil.** Der
   Platinen-Strang hat am 24.10.2026 gemessen, dass J2 kein bestellbarer
   Artikel ist, sondern ein **Bibliotheksplatzhalter ohne Datenblatt**. Wer
   hier sucht, sucht nach einem Datenblatt, das es nie gab: **es ist zuerst ein
   Teil auszuwählen.** Die Auflage ans Layout bleibt davon unberührt – der
   Zellenraum ist 4,65 mm tief und mit der Adafruit 1317 zu 4,40 mm belegt;
   **alles neben der Zelle über 4,45 mm macht das Gerät dicker.**
6. **Ferritfolie**: Dicke 0,3–0,5 mm angenommen, Material offen. ±0,2 mm
   Gesamtdicke. Breite 12 mm, der schmalen Zelle folgend.
7. **Platz für die umgelegte FPC-Fahne der Anzeige** – der Hohlraum ist 2,35 mm
   hoch, ob das reicht, sagt erst das Layout.
8. **Freihaltezone um die Chipantenne des Moduls** – davon hängt ab, ob die
   Seitenwand dort wegbleiben muss.
9. **Welcher 3D-Drucker** – die Druckerwerte sind Annahmen.
10. **Schlüsselring aus Stahl neben der NFC-Antenne** – nur messbar, nicht
    rechenbar.

**Geschlossen am 24.10.2026:** *Liegt die Zelle außermittig?* **Nein, mittig.**
Der Versatz war zweimal die Antwort auf ein Platzproblem neben der Zelle –
erst der Sicherheitschip, dann das Sensorfeld. Seit der Sensor im Boden sitzt,
zwingt nichts mehr an die Kante. Mit der Adafruit 1317 liegt sie quer
3,62…23,38 mm, also 2,42 mm von der Hohlraumwand. Hohlraum und Ferritfolie
sind symmetrisch, die Folie wieder 19,75 mm breit.

**Geschlossen am 24.10.2026:** *Auf welcher Seite liegt das Sensorfeld?*
**Unten**, auf der Akkuseite – oben überlappte es das Anzeigeglas. Die
Sensoröffnung des Gehäuses sitzt seit dem ersten Entwurf im Boden, also auf
derselben Seite. Befund 4 sagt, was dabei zusätzlich herauskam.

**Wieder offen seit dem 24.10.2026, abends** – *Buchse oder Stecker vorn.*
Thomas hat zum **Platinenrand-Stecker** gewechselt, siehe den letzten
Abschnitt. Der Eintrag darunter ist der Stand davor und bleibt stehen, weil er
sagt, was damals gegeneinander stand:

**Geschlossen am 24.10.2026:** *Buchse oder Stecker vorn.* Thomas hat die
**Buchse** entschieden. Verworfen wurden die geätzte Zunge (ohne Metallhülle
nicht spezifikationsfähig, keine Quelle zur Konformität gefunden), der
Randstecker (14 Wochen Lieferzeit, mechanisch schwächer) und der Mid-Mount
(hätte 1,21 mm gespart, kostet aber Teiletausch und einen Platinenausschnitt).
Die vollständige Liste des Verworfenen steht in [masse.md](masse.md),
Abschnitt 9.

## Was sich durch den Steckerumbau ändert

**Nicht von mir gerechnet.** Diese Liste kommt aus der Übergabe des
Master-Strangs vom 24.10.2026, abends, nachdem dieses Modell fertig war. Sie
steht hier als **Vorwarnung**, nicht als Maß: keine der Zahlen unten ist von
mir nachgerechnet, und das Modell in `export/` ist unverändert das alte.

Thomas hat entschieden, dass das Gerät ein **Stick** wird, den man direkt
einsteckt – keine Buchse, kein Kabel. Der Platinen-Strang hat die Buchse
bereits gegen einen **Platinenrand-Stecker** getauscht, der nur **2,50 mm**
auf die Platine greift statt 9,5.

| Was am Gehäuse | Warum |
| --- | --- |
| **Die Nase vorn entfällt** | Der Randstecker trägt nicht auf, er ragt hinaus. Damit fallen die **11,01 mm** vorn weg; es bliebe durchgehend 9,80 mm – wenn sonst nichts dazukommt |
| **Die Mulde entfällt** | Sie war allein für die Haltestifte der Buchse da (Befund in [masse.md](masse.md), Abschnitt 15) |
| **Die Frontpartie wird offen statt umschließend** | Die Platine ragt vorn als Steckerzunge heraus, die Front ist kein geschlossener Rahmen mehr |
| **Der Kragen verliert die 10,5-mm-Lücke** | Die Unterbrechung der Frontkante gehörte zur USB-Öffnung. Der Punkt, den ich oben als nie gerechnet gemeldet habe, **löst sich damit vielleicht von selbst** – nachrechnen muss ihn trotzdem jemand, denn eine offene Front kann den Kragen auch ganz anders schneiden |
| **Das Gerät wird kürzer** | Sobald die Akkufrage entschieden ist. Um wie viel, steht nirgends |

**Was davon unberührt bleibt**, soweit es von vorn abhängt: der Umzug von SW2
ans USB-Ende. Der Taster sitzt bei längs 6,8912…10,3912 mm, der Randstecker
greift nur 2,50 mm auf die Platine – der Grund, aus dem der Platinen-Strang
SW2 nicht an den unteren Rand gesetzt hat, fällt mit der Buchse weg, die Lage
selbst bleibt aber gültig und kollidiert mit dem kürzeren Stecker erst recht
nicht. **Auch das ist gerechnet, nicht gemessen.**

Wer das Gehäuse umbaut: `usb_nose_x`, `usb_recess_*`, `usb_body_h` und
`usb_open_*` in [parameters.csv](parameters.csv) sind die Zahlen, die dann
keine Grundlage mehr haben.
