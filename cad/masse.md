# Maße und Dickenrechnung

Stand 24.10.2026, nach Thomas' Entscheidungen zu Platinenbreite und
USB-Anschluss. **Gerechnet, nichts gebaut, nichts gemessen.** Jede Zahl stammt
aus `../docs/parts.md`, `../hardware/layout-notes.md` oder einem Datenblatt, das
unten genannt ist. Was fehlt, steht als `offen` und ist **nicht geraten**.

## Das Ergebnis in Zahlen

| Maß | Wert |
| --- | --- |
| **Dicke hinten** | **9,80 mm** |
| **Dicke vorn** (Nase über dem USB-Stecker) | **11,01 mm** |
| Breite außen | **30,80 mm** |
| Länge ohne Öse | 73,80 mm |
| **Länge mit Öse** | **81,80 mm** |

Die Schätzung in `../hardware/layout-notes.md` lautete „etwa 8–9 mm fertig".
**Sie ist um 0,8–2,0 mm zu optimistisch** – darin fehlten die Ferritfolie, die
Luftspalte, die Wandstärken und der USB-Stecker.

## 1. Was feststeht

| Teil | Maß | Quelle |
| --- | --- | --- |
| Platine | **27 × 70 × 0,8 mm** | Breite von Thomas entschieden, siehe Abschnitt 4 |
| U1 Modul MDBT50Q-1MV2 | **2,05 mm** hoch | `parts.md` |
| DS1 OLED 0,71" | 13,90 × 22,00 × **1,26 mm**, aktiv 10,54 × 14,70 | `parts.md` |
| Akku **LP-401230-J** (schmal) | **4,05** × 12,0 × 30,5 mm | `parts.md`, Zeile 444 |
| Akku Adafruit 1317 (breit) | 3,80 × 19,75 × 26,02 mm | `parts.md`, Zeile 443 |
| SW1/SW2 Panasonic EVQP7C01P | 3,5 × 3,55 × **1,35 mm**, seitlich, Hub 0,2 | Übergabe Platinen-Strang |
| J1 USB-C GCT USB4085 | Körper 8,95 × 9,17 mm, **Bauhöhe 3,46 mm**, Öffnung 8,35 × 2,56 | GCT-Zeichnung, Abschnitt 6 |
| U2 SE051C2 | 3 × 3 × 0,32 mm | `layout-notes.md` |
| D1 RGB-LED | 1,6 × 1,6 × 0,5 mm | `layout-notes.md` |
| Sensor FPC2534 (Stufe 2) | **Ø 11 × 0,83 mm** | `../docs/fingerprint.md`, Zeile 78 |
| Ferritfolie | **0,3–0,5 mm**, Zielwert | `layout-notes.md`, offener Punkt 3 |

## 2. Welches Teil auf welcher Seite liegt

| Seite | Teile | Höchstes |
| --- | --- | --- |
| **Oben** (Nutzerseite) | U1 2,05 · DS1 1,26 · SW1/SW2 1,35 (seitlich) · D1 0,5 · **J1 3,46** | J1, aber nur vorn |
| **Unten** | Ferritfolie 0,4 + Zelle 4,05 · U2 0,32 · U3 Flash · Sensor 0,83 (Stufe 2) | **4,45 mm** |

Drei Dinge folgen daraus:

- **Der USB-Stecker ist mit 3,46 mm höher als alles andere oben**, aber er sitzt
  nur auf den ersten 9,2 mm. Darum bekommt das Dach dort eine Nase, statt das
  ganze Gerät dicker zu machen – Abschnitt 6.
- **Das Modul ist 0,79 mm höher als das Glas der Anzeige.** Die Anzeige liegt
  deshalb auf einem **0,8 mm dicken Klebepad** und schließt bündig mit dem Modul
  ab. Ohne das Pad sieht man durch einen Schacht.
- **Der Sensor der Stufe 2 ist mit 0,83 mm viel dünner als die Zelle.** Solange
  er **neben** ihr sitzt, ändert er die Dicke nicht.

## 3. Die Dickenrechnung

Druckerwerte: 0,4 mm Düse, 0,2 mm Schichthöhe (Abschnitt 7).

| Schicht | hinten | vorn | Begründung |
| --- | --- | --- | --- |
| Dach | 1,00 | 1,00 | 5 Schichten à 0,2 mm |
| Luft darunter | 0,30 | 0,10 | hinten Lötklecks und Montagespiel, vorn nur Blech |
| höchstes Teil | 2,05 | **3,46** | hinten U1, vorn der USB-Stecker |
| **Platine** | 0,80 | 0,80 | wegen des Platinenrand-Steckers festgelegt |
| Luft unter der Platine | 0,20 | 0,20 | Lötseite, Durchsteckpins |
| Ferritfolie | 0,40 | 0,40 | Mittelwert des Zielbereichs 0,3–0,5 |
| Zelle LP-401230-J | 4,05 | 4,05 | dickstes Teil überhaupt |
| Boden | 1,00 | 1,00 | wie Dach |
| **Summe** | **9,80** | **11,01** | |

**Nicht enthalten:** Klebeschicht unter der Zelle, Fertigungstoleranz des Drucks
(üblich ±0,1–0,2 mm je Maß), Aufquellen der Zelle über die Lebensdauer. Jedes
davon macht es dicker, keines dünner. Realistisch **9,8–10,3 mm** hinten.

### Warum 9,80 und nicht 9,55

Bis zur Entscheidung über den Sensor war die **Adafruit 1317** (3,80 mm) die
Zelle, und damit kam die Rechnung auf 9,55 mm. Stufe 2 braucht aber die schmale
**LP-401230-J**, und die ist **4,05 mm dick** – 0,25 mm mehr. Sie ist schmaler,
aber nicht dünner.

**Der Hohlraum ist auf 4,65 mm ausgelegt, also auf die dickere Zelle.** Mit der
Adafruit 1317 blieben dort 0,25 mm Luft. **Das Gehäuse trägt damit beide Zellen
und beide Stufen** – genau das war der Zweck.

### Wo noch etwas zu holen wäre

| Maßnahme | Gewinn | Preis |
| --- | --- | --- |
| Dach und Boden 0,8 statt 1,0 mm | 0,40 mm | 4 Schichten – bei einem Teil am Schlüsselbund zu dünn |
| Ferritfolie 0,3 mm | 0,10 mm | hängt am Material, offener Punkt 3 der Layout-Vorgaben |
| Zelle in eine Bodenöffnung versenken | bis 1,00 mm | Zelle liegt frei – **nicht empfohlen** |
| Mid-Mount-Stecker statt Buchse | 1,21 mm vorn | **verworfen**, Abschnitt 9 |

## 4. Die Platinenbreite: 27 mm

**Entschieden von Thomas am 24.10.2026.** Alle Maße dieses Abschnitts sind
**quer**, also über die kurze Seite – im Bezugssystem des Platinen-Strangs ist
das sein `x` (0…27), nicht sein `y`.

Der Grund ist der Fingerabdrucksensor der Stufe 2. Er kann in kein anderes
Drittel: vorn liegt die Chipantenne des Moduls, hinten die NFC-Antenne, und
beide dürfen kein Metall und kein Einlegeteil in der Nähe haben. Also muss er
neben die Zelle.

| Platine | nutzbar (− 2 × 1,2 Rand) | gebraucht | Reserve | Sensoröffnung |
| --- | --- | --- | --- | --- |
| 24 mm | 21,60 | 23,50 | **fehlt 1,90** | – |
| 26 mm | 23,60 | 23,50 | 0,10 | **bricht 0,40 mm in die Auflage ein** |
| **27 mm** | **24,60** | 23,50 | **1,10** | **0,60 mm Rand** |

Gebraucht: Zelle 12,0 + **Trennabstand 0,5** + Sensor 11,0 = **23,50 mm**.

**Der Trennabstand ist der Punkt, an dem diese Rechnung falsch war** – siehe
Abschnitt 10. Zwischen einer LiPo-Zelle und einem Bauteil braucht es Luft, und
0,5 mm ist knapp, nicht großzügig.

**Warum nicht 26 mm mit einem schmaleren Kragen?** Ein Kragen von 1,0 statt
1,2 mm brächte 24,00 mm nutzbar und damit 0,50 mm Reserve. Das schwächt aber
genau die Klemmung, die die Schrauben ersetzt. **Ein Millimeter Gehäusebreite
ist billiger als eine schwächere Klemmung** – zumal die Breitentoleranz der
Zelle in keiner Unterlage steht und die Reserve sie auffangen muss.

Aufteilung quer, als Vorgabe ans Layout:

| | von | bis |
| --- | --- | --- |
| Klemmkragen (kein Bauteil) | 0,00 | 1,20 |
| **Zelle** | **1,20** | **13,20** |
| Trennabstand | 13,20 | 13,70 |
| **Sensorfeld** | **13,70** | **24,70** |
| Reserve | 24,70 | 25,80 |
| Klemmkragen (kein Bauteil) | 25,80 | 27,00 |

Die Sensoröffnung des Gehäuses liegt damit auf Mitte **19,20 mm**.

## 5. Die Außenmaße

Seitenwand 1,6 mm, damit die Schnappstufe als Falz darin Platz hat (0,8 innen,
0,8 außen). Spiel zur Platine 0,3 mm je Seite.

| Maß | Rechnung | Ergebnis |
| --- | --- | --- |
| Breite | 27,0 + 2 × (0,3 + 1,6) | **30,8 mm** |
| Länge ohne Öse | 70,0 + 2 × (0,3 + 1,6) | **73,8 mm** |
| Öse | 8,0 mm Anbau, Loch Ø 4,0 mm | + 8,0 mm |
| Länge mit Öse | | **81,8 mm** |
| Dicke | Abschnitt 3 | **9,80 / 11,01 mm** |

**81,8 × 30,8 × 9,8 mm** – etwa wie ein Autoschlüssel mit Funkfernbedienung.

## 6. Der USB-Anschluss: Buchse, und das Dach bekommt vorn eine Nase

**Entschieden von Thomas am 24.10.2026: es bleibt bei der Buchse.** Kein
Randstecker, kein Mid-Mount. Was dagegen sprach, steht in Abschnitt 9.

Abgelesen aus GCTs Zeichnung, Revision A1 vom 09.04.2018, Blatt 1/3. Im
Schriftfeld steht **„USB 2.0 Type C Receptacle, Dip Type, PCB Top Mount"** – der
Körper sitzt vollständig **auf** der Platine, nichts taucht ein.

### Welche der drei Zahlen was ist

Auf dem Blatt stehen 3,46 · 3,16 · 2,56 nebeneinander:

| Zahl | Wo | Was es ist |
| --- | --- | --- |
| **2,56 ± 0,04** | Vorderansicht, neben 8,35 | **die Steckeröffnung**. 8,35 × 2,56 mm ist das Maß des Type-C-Steckgesichts |
| **3,16** | Seitenansicht | das **Blechgehäuse außen**: 2,56 + 2 × 0,30 mm Blechdicke. Die 0,30 stehen in der Spezifikation unter „Shell: Stainless steel (T=0,30 mm)" |
| **3,46** | Schnitt A-A | dasselbe von einer **0,30 mm tiefer liegenden Bezugslinie** |

**Berichtigung zu `../hardware/layout-notes.md`:** dort steht „Öffnung 3,16 mm
hoch". Das ist nicht die Öffnung – die Öffnung ist **2,56 mm**.

**Gerechnet wird mit 3,46 mm**, der größeren Lesart. Ob die Oberkante 3,16 oder
3,46 mm über der Platine liegt, gibt die Zeichnung in meiner Auflösung nicht
her (`ungeprueft`) – die größere Zahl ist die sichere.

### Die Nase

| Maß | Wert |
| --- | --- |
| Hohlraum hinten | 2,35 mm – **zu niedrig für den Stecker** |
| Hohlraum unter der Nase | **3,56 mm** (3,46 + 0,10 Luft) |
| Dach darüber | 1,00 mm |
| **Dicke vorn** | **11,01 mm**, Stufe **1,21 mm** gegenüber hinten |
| Nase reicht von der Front bis | **10,20 mm** (Steckerkörper 9,17 tief + 1,0 Rand) |
| Frontöffnung | 10,50 × 3,76 mm, mittig |
| Wand über der Frontöffnung | **0,80 mm** – zwei Druckbahnen, das Mindestmaß |

Die Nase geht über die **volle Breite** und hat eine **senkrechte Stufe**, keine
Fase. Das druckt ohne Stütze, braucht keine Nacharbeit und sieht gewollt aus –
vorn eine Kante, hinten flach.

**Vorgabe ans Layout dazu:** die Vorderkante der Buchse muss **bündig mit der
Gehäusefront** sitzen (± 0,3 mm). Steht sie zu weit innen, kommt der Stecker
nicht bis zum Anschlag; steht sie vor, scheuert sie.

## 7. Druckerwerte, mit denen gerechnet ist

| Wert | Annahme |
| --- | --- |
| Düse | 0,40 mm |
| Schichthöhe | 0,20 mm |
| Außenwände | 3 Bahnen = 1,20 mm mindestens; hier 1,60 mm |
| Dach und Boden | 5 Schichten = 1,00 mm |
| Material | PETG (zäher als PLA, hält die Schnapphaken aus) |
| Spiel Teil zu Teil | 0,20 mm |
| Kleinster Steg | 0,80 mm (2 Bahnen) |
| Kleinstes Loch | Ø 1,6 mm |
| Stützmaterial | **keines** |

Diese Werte sind **Annahmen für einen gewöhnlichen Schmelzdrucker**, nicht am
Drucker des Nutzers gemessen – welcher es ist, steht nicht in den Unterlagen,
`offen`.

## 8. Die beiden Taster

Lage aus der Übergabe des Platinen-Strangs: beide bei **längs 22,75…26,25 mm**,
an den Längskanten, seitlich betätigt.

| Maß | Wert |
| --- | --- |
| Taster belegt längs | 22,75 … 26,25 mm |
| Anzeigeglas beginnt | 27,00 mm → 0,75 mm Luft |
| Funkmodul endet | 21,50 mm → 1,25 mm Luft |

**Die 0,75 mm sind nicht die Grenze des Rahmens, weil der Rahmen dort nichts
hat.** Das Anzeigefenster ist ein glatter Durchbruch im 1,0 mm dicken Dach, ohne
nach unten gezogenen Rand:

| Maß | Wert |
| --- | --- |
| Glas 27,00…49,00, aktive Fläche 14,70 mittig | 30,65 … 45,35 mm |
| **Gehäusefenster** = aktive Fläche + 0,5 mm Rand | **30,15 … 45,85 mm** |
| **Fensterrand zum Taster** | **3,90 mm** |

Quer liegt das Glas 6,55 mm vom Platinenrand, der Klemmkragen belegt davon
1,20 mm – **5,35 mm Luft**.

### Was der Rahmen stattdessen brauchte

**Der Klemmkragen war das Problem, nicht das Fenster.** Er läuft 1,2 mm breit am
Platinenrand um – genau dort, wo ein seitlich betätigter Taster sitzt.

- Der Kragen ist an **beiden** Längskanten über **21,70…27,30 mm** bis auf
  1,65 mm Höhe freigestellt (1,35 Bauhöhe + 0,3 Luft).
- **Die Freistellung ist 5,6 mm lang, der Schlitz nur 4,5 mm.** Panasonic gibt
  den Körper mit 3,5 mm Länge an, DigiKey mit 4,70 mm – beide Lesarten passen
  jetzt hinein (1,05 bzw. 0,45 mm Luft je Seite).
- Zwei Unterbrechungen von je 5,6 mm bei rund 194 mm Umfang. Die Klemmung trägt
  das.

### SW1 bekommt eine Öffnung, SW2 nicht

`../hardware/layout-notes.md` verlangt, dass der Reset von außen **nicht**
erreichbar ist:

| Taster | Kante | Was das Gehäuse macht |
| --- | --- | --- |
| SW1 (Bestätigen) | quer 0 | **Schlitz durch die Seitenwand**, 4,5 × 2,4 mm |
| SW2 (Reset) | quer 27 | **Blindtasche 0,8 mm tief**, Wand bleibt 0,8 mm stehen |

Ganz zumachen geht nicht: eine geschlossene Wand stünde 0,3 mm vor dem Stößel
und könnte ihn dauerhaft gedrückt halten. Panasonic gibt „dimensions excluding
the push plate: 2,9 mm" an, der Stößel ragt also **3,55 − 2,9 = 0,65 mm**
heraus, Hub 0,2 mm, zusammen **0,85 mm**. Das Gehäuse gibt **1,10 mm** –
**0,25 mm Reserve**.

Bestätigt ist damit auch die Grundannahme des ganzen Rahmens: Panasonic nennt
das Teil **„side-push SMD"**.

## 9. Was geprüft und verworfen wurde

Damit in einem halben Jahr niemand dieselben Wege noch einmal geht:

| Verworfen | Warum |
| --- | --- |
| **Platine 24 mm breit** | Sensor der Stufe 2 passt nicht neben die Zelle, es fehlten 1,90 mm |
| **Platine 26 mm breit** | ginge auf 0,10 mm auf; die Sensoröffnung bräche 0,40 mm in die Auflagestufe ein |
| **Kragen 1,0 statt 1,2 mm** | schwächt die Klemmung, die die Schrauben ersetzt – teurer als 1 mm Breite |
| **Geätzte Zunge als Stecker** | ohne Metallhülle **nicht spezifikationsfähig**; keine Quelle zur Konformität gefunden |
| **Randstecker (Molex 1054440001)** | **14 Wochen Lieferzeit** und mechanisch schwächer als die Durchsteckbuchse |
| **Mid-Mount-Stecker** | spart 1,21 mm vorn, kostet aber Teiletausch und einen Ausschnitt in der Platine – Thomas hat die Buchse vorgezogen |
| **Zelle in eine Bodenöffnung versenken** | spart bis 1,0 mm, lässt die Zellhülle aber frei liegen |
| **Schrauben statt Schnappverbindung** | Metall im Gehäuse, und die NFC-Antenne verträgt keines |

## 10. Wo diese Rechnung falsch war

Zweimal, beide Male vom Platinen-Strang gefunden. Das gehört hierher, weil es
zeigt, worauf beim nächsten Mal zu achten ist:

1. **Der Trennabstand fehlte in der Summe, nicht in der Aufteilung.** Die
   Aufteilung nannte Zelle bis 13,20 und Sensor ab 13,70 – der halbe Millimeter
   war da. Die Summenzeile darüber rechnete 12,0 + 11,0 = 23,00 und nannte
   0,60 mm Reserve statt 0,10. **Zwei Zahlen im selben Dokument, die sich
   widersprachen**, und gemeldet wurde die falsche.
2. **„y" stand für „quer", obwohl der Rest des Dokuments „längs/quer" sagt.**
   Beim Platinen-Strang ist `y` die Längsrichtung. Daraus wäre fast eine
   Aufteilung in der falschen Achse geworden.

**Die Lehre aus beiden: eine Zahl, die in zwei Formen im Dokument steht, ist
eine Zahl zu viel.**

## 11. Was offen ist und nicht geraten wurde

| Nr. | Offenes Maß | Was es ändern würde |
| --- | --- | --- |
| 1 | **Ob der Stecker 3,16 oder 3,46 mm über der Platine steht** | bis zu 0,30 mm an der Nase. Gerechnet ist mit der größeren Zahl |
| 2 | **Breitentoleranz der Zelle** | steht in `parts.md` nicht; sie ist das, was die 1,10 mm Reserve auffangen muss |
| 3 | **Dicke und Material der Ferritfolie** | ±0,2 mm Gesamtdicke |
| 4 | **Freihaltezone um die Chipantenne des U1** | ob die Seitenwand neben dem Modul wegbleiben muss |
| 5 | **Platz für die umgelegte FPC-Fahne der Anzeige** | Höhe des Hohlraums hinter dem Glas |
| 6 | **Lage der Sensoröffnung längs** | steht vorläufig auf 36,0 mm |
| 7 | **Welcher 3D-Drucker** | Abschnitt 7 |
| 8 | **Ob die Zelle außermittig liegt** | der Platinen-Strang rechnet das gerade neu, nachdem die Zelle von 19,75 auf 12 mm Breite gewechselt ist – Abschnitt 12 |

## 12. Darf die Zelle aus der Mitte?

Gefragt vom Platinen-Strang: rückt die Zelle an eine Kante, bleibt gegenüber ein
Streifen für den Sicherheitschip, und die I2C-Leitung wird 12 statt 25 mm kurz.

**Vom Gehäuse aus: ja, mit einer Einschränkung.** Der bauteilfreie Bereich quer
ist **1,20 … 25,80 mm**. Der Vorschlag begann bei 0,75 – das liegt 0,45 mm im
Klemmkragen, dort drückt das Gehäuse auf die Platine. **Ab 1,20 ist frei.**

Der Hohlraum ist ein glatter Kasten; nichts im Gehäuse hält die Zelle an einer
bestimmten Stelle, und die Klemmung läuft am Platinenrand, nicht über der Zelle.
**Außermittig kostet mich nichts.**

**Fühlt es sich schief an?** Bei der breiten Zelle wanderte die Zellenmitte um
1,93 mm – 6,3 % der Gehäusebreite, 2,4 % der Länge. Bei einem Anhänger, der
dreimal so lang wie breit ist und am Ring hängt, halte ich das für nicht
spürbar. **Das ist eine Einschätzung, keine Messung** – die Masse der Zelle und
des fertigen Geräts stehen in keiner Unterlage.

**Mit der schmalen Zelle (12 statt 19,75 mm) ist die Lage eine andere**, und der
Platinen-Strang rechnet sie neu. Bis dahin ist der Hohlraum nicht festgelegt.

Eines gehört zum Layout: **die Ferritfolie wandert mit der Zelle.** Ob sie dort
noch die Zellfläche überdeckt und Richtung Antenne übersteht, wie
`../hardware/layout-notes.md` verlangt, muss der Platinen-Strang prüfen.

## 13. Was das Layout dafür tun muss

Das sind **Vorgaben ans Layout**, keine Wünsche:

1. **Platinenmaß 27 × 70 mm**, Dicke 0,8 mm. Toleranz ohne Nacharbeit
   **± 0,2 mm**; darüber hinaus ist der Parameter zu ändern und neu zu bauen.
2. **Rundum 1,2 mm Platinenrand oben und unten frei von Bauteilen.** Dort wird
   geklemmt – das ersetzt Schrauben und Befestigungslöcher.
3. **Ecken der Platine mit R 1,5 mm abrunden.**
4. **Kein Bauteil höher als 2,05 mm auf der Anzeigenseite** (außer der USB-Buchse
   auf den ersten 10,2 mm) und **keines höher als 4,45 mm auf der Akkuseite**
   (Ferritfolie mitgerechnet).
5. **Aufteilung quer wie in Abschnitt 4**: Zelle 1,20…13,20, Sensorfeld
   13,70…24,70. Der Sensor bleibt **neben** der Zelle, nie über ihr.
6. **Die beiden Taster bei längs 22,75…26,25 mm** an den Längskanten, Mitte der
   Betätigungsfläche 0,7 mm über der Platinenoberseite.
7. **Die Vorderkante der USB-Buchse bündig mit der Gehäusefront** (± 0,3 mm),
   mittig auf der kurzen Kante.
8. **D1 auf längs 66,0 / quer 13,5 mm**, Oberseite – so übergeben.
9. **Die NFC-Antenne liegt im letzten Drittel.** Dort hat das Gehäuse **kein
   Einlegeteil, keine Schraube, kein Metall**; die Öse ist aus Kunststoff und
   sitzt hinter der Platine.
