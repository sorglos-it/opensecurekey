# Maße und Dickenrechnung

> **ÜBERHOLT, Stand 27.10.2026. Keine Zahl dieser Datei gilt noch.**
>
> Die ganze Rechnung hängt am Akku: der Zellenraum ist mit **4,05 mm das
> dickste Element** jeder Lagensumme, und die Aufteilung quer (Zelle
> 1,20…13,20, Sensorfeld 13,70…24,70) teilt die Breite danach auf. **Der Akku
> ist am 25.10.2026 gestrichen** (`../docs/plan.md`, Abschn. 2) — damit fallen
> Zellenraum, Zellenreserve, Rückfallzelle, Ferritfolie und Akkustecker J2
> ersatzlos weg.
>
> **Zwei Angaben kehren sich dadurch um**, und beide sind ungünstiger als man
> erwartet:
>
> - **Das dickste Teil ist jetzt der USB-Stecker J1 mit 2,90 mm über alles.**
>   Die Höhengrenze für alles andere ist also von 3,8 auf **2,90 mm gefallen**.
> - **Die Platine ist nicht mehr 24 × 70 mm, sondern 27 × 59,55 mm**
>   (obere Schranke) — die Dicke wird dünner, Breite und Länge ändern sich.
>
> **Neu gerechnet werden muss von vorn, nicht ausgebessert.** Der Text unten
> bleibt stehen, weil er belegt, *wie* gerechnet wurde — die Methode gilt
> weiter, die Zahlen nicht.

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
| Akku **Adafruit 1317** – gewählt | **3,80** × 19,75 × 26,02 mm | `parts.md`, Zeile 443 |
| Akku LP-401230-J (schmal) – Rückfallzelle | 4,05 × 12,0 × 30,5 mm | `parts.md`, Zeile 444 |
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
| **Unten** | Ferritfolie 0,4 + Zellenreserve 4,05 · U2 0,32 · U3 Flash | **4,45 mm** |

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
| Zellenreserve | 4,05 | 4,05 | dickstes Teil; gewählt ist die 3,80 mm dicke Zelle, siehe unten |
| Boden | 1,00 | 1,00 | wie Dach |
| **Summe** | **9,80** | **11,01** | |

**Nicht enthalten:** Klebeschicht unter der Zelle, Fertigungstoleranz des Drucks
(üblich ±0,1–0,2 mm je Maß), Aufquellen der Zelle über die Lebensdauer. Jedes
davon macht es dicker, keines dünner. Realistisch **9,8–10,3 mm** hinten.

### Warum 9,80 und nicht 9,55 – die 0,25 mm sind Absicht

Gewählt ist die **Adafruit 1317** mit 3,80 mm. Damit käme die Rechnung auf
**9,55 mm**. Der Hohlraum ist trotzdem **4,65 mm** tief, also auf die 4,05 mm
dicke Rückfallzelle ausgelegt. Zwei Gründe:

- **Das Gehäuse trägt dann beide Zellen.** Die Zelle hat heute dreimal
  gewechselt. Ein Gehäuse, das nur zu einer passt, wird beim nächsten Wechsel
  neu gedruckt.
- **LiPo-Zellen quellen über die Lebensdauer.** Um wie viel, steht in
  `../docs/parts.md` nicht. 0,25 mm sind das Einzige, was dafür da ist.

**Wer die 9,55 mm will, ändert eine Zeile** (`h_bot_part` von 4,45 auf 4,20)
und baut neu. Dann ist die Rückfallzelle ausgeschlossen und die Zelle hat
keinen Platz zum Quellen. **Ich empfehle, die 0,25 mm zu behalten.**

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

Lage aus der Übergabe des Platinen-Strangs. **SW1** steht neben der Anzeige,
**SW2** seit dem 24.10.2026 am USB-Ende – zwischen Funkmodul und Anzeigeglas
fehlten dem Reset 0,5491 mm, gerechnet vom Platinen-Strang. Beide an den
Längskanten, seitlich betätigt.

| Maß | SW1 | SW2 (seit 24.10.2026) |
| --- | --- | --- |
| Mitte längs | 24,50 mm | **8,6412 mm** |
| Körper belegt längs | 22,75 … 26,25 mm | **6,8912 … 10,3912 mm** |
| Pads längs | – | 6,1402 … 11,1419 mm |
| Mitte quer | 3,50 mm | 23,500001 mm |
| Nachbar oben | Anzeigeglas ab 27,00 → 0,75 mm | Funkmodul-Körper ab 12,0499 → **0,9080 mm** |
| Nachbar unten | Funkmodul endet 21,50 → 1,25 mm | Steckerstreifen J1 bis 5,9892 → 0,1510 mm |

**Das Gehäuse stand der neuen Lage nicht im Weg** – nachgerechnet gegen Mulde,
Steckerüberstand und Nase, alle Zahlen in [gehaeuse.md](gehaeuse.md),
Abschnitt „Die beiden Taster". Der kleinste Abstand ist 0,5588 mm zum ersten
Schnappwulst, der kleinste quer 1,2250 mm zur Mulde.

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

- Der Kragen ist an der Kante quer 0 über **21,70…27,30 mm** (SW1) und an der
  Kante quer 27 über **5,8412…11,4412 mm** (SW2) bis auf 1,65 mm Höhe
  freigestellt (1,35 Bauhöhe + 0,3 Luft).
- **Die Freistellung ist 5,6 mm lang, der Schlitz nur 4,5 mm.** Panasonic gibt
  den Körper mit 3,5 mm Länge an, DigiKey mit 4,70 mm – beide Lesarten passen
  jetzt hinein (1,05 bzw. 0,45 mm Luft je Seite).
- Drei Unterbrechungen bei 194,0 mm Umfang: 5,6 mm (SW1), 5,6 mm (SW2) und
  10,5 mm für die USB-Öffnung in der Frontkante. **172,3 mm tragen, 88,8 %** –
  am Modell abgetastet, siehe [gehaeuse.md](gehaeuse.md). Die USB-Öffnung
  fehlte in der früheren Zahl „zwei Unterbrechungen bei 192 mm".

### SW1 bekommt eine Öffnung, SW2 nicht

`../hardware/layout-notes.md` verlangt, dass der Reset von außen **nicht**
erreichbar ist:

| Taster | Kante | Was das Gehäuse macht |
| --- | --- | --- |
| SW1 (Bestätigen) | quer 0, längs 24,5 | **Schlitz durch die Seitenwand**, 4,5 × 2,4 mm |
| SW2 (Reset) | quer 27, längs 8,6412 | **Blindtasche 0,8 mm tief**, Wand bleibt 0,8 mm stehen |

Ganz zumachen geht nicht: eine geschlossene Wand stünde 0,3 mm vor dem Stößel
und könnte ihn dauerhaft gedrückt halten. Panasonic gibt „dimensions excluding
the push plate: 2,9 mm" an, der Stößel ragt also **3,55 − 2,9 = 0,65 mm**
heraus, Hub 0,2 mm, zusammen **0,85 mm**. Das Gehäuse gibt **1,10 mm** –
**0,25 mm Reserve**.

Diese 1,10 mm rechnen damit, dass die Stößelspitze mit der Platinenkante
abschließt. Nach der Übergabe steht die Tastermitte quer 3,50 mm von der Kante,
der Körper ist 3,55 mm tief, die Spitze läge also 1,075 mm **vor** der Kante
und die Reserve wäre größer. Welche der beiden Lesarten stimmt, ist am Bauteil
zu messen und **nicht gemessen** – beide sind auf der sicheren Seite. Der Umzug
von SW2 ändert quer nichts, SW1 und SW2 stehen weiter 3,50 mm von ihrer Kante.

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

**Entschieden am 24.10.2026: die Zelle liegt mittig.** Der Versatz war die
Antwort auf ein Platzproblem neben der Zelle – erst der Sicherheitschip, dann
das Sensorfeld. Seit der Sensor im Boden sitzt (Abschnitt 14), zwingt nichts
mehr an die Kante.

Mit der Adafruit 1317 (19,75 mm breit) mittig: **Zelle quer 3,62…23,38 mm**,
also 2,42 mm Luft zur Hohlraumwand auf jeder Seite – deutlich mehr als die
0,20 mm, die ich verlange. Die Ferritfolie ist wieder 19,75 mm breit.

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
6. **SW1 bei längs 22,75…26,25 mm, SW2 bei längs 6,8912…10,3912 mm** an den
   Längskanten, Mitte der Betätigungsfläche 0,7 mm über der Platinenoberseite.
7. **Die Vorderkante der USB-Buchse bündig mit der Gehäusefront** (± 0,3 mm),
   mittig auf der kurzen Kante.
8. **D1 auf längs 66,0 / quer 13,5 mm**, Oberseite – so übergeben.
9. **Die NFC-Antenne liegt im letzten Drittel.** Dort hat das Gehäuse **kein
   Einlegeteil, keine Schraube, kein Metall**; die Öse ist aus Kunststoff und
   sitzt hinter der Platine.

## 14. Der Sensor gehört in den Boden, nicht auf die Platine

Gerechnet am 24.10.2026, als der Platinen-Strang fragte, auf welcher Seite das
Sensorfeld liegt. **Die Antwort auf die Seitenfrage ist „unten". Die Rechnung
dahinter bringt aber noch etwas anderes ans Licht.**

### Auf der Platine ist der Sensor nicht zu erreichen

| Ebene | z |
| --- | --- |
| Platinenunterseite | −0,80 mm |
| Sensorfläche, wenn er auf der Platine sitzt (0,83 dick) | −1,63 mm |
| Bodeninnenseite | −5,45 mm |
| **Bodenaußenseite – dort liegt der Finger** | **−6,45 mm** |

**Zwischen Sensorfläche und Finger lägen 4,82 mm.** Ein kapazitiver
Fingerabdrucksensor liest durch Bruchteile eines Millimeters Deckschicht, nicht
durch fünf Millimeter Luft und Kunststoff. **Auf der Platine ist er nutzlos.**

Den Hohlraum einfach flacher zu machen geht nicht: er ist randvoll. 0,2 mm
Luft + 0,4 mm Ferritfolie + 4,05 mm Zelle = 4,65 mm, und genau 4,65 mm ist er
tief.

### Darum sitzt er im Boden

Das passt zur Bauform: `../docs/fingerprint.md` nennt den FPC2534**AM** ein
**Flex-Modul** – „Sensor auf Flex, 12-poliger ZIF-Anschluss". Der Sensor hängt
am Flachband und ist dafür gemacht, in ein Gehäuse geklebt zu werden, nicht auf
eine Platine gelötet.

| Maß | Wert |
| --- | --- |
| Sitz im Boden | **Ø 11,30 mm**, glattes Loch |
| Sensor | Ø 11,00 × 0,83 mm |
| Klebefuge ringsum | 0,15 mm |
| Bodendicke | 1,00 mm |
| **Sensorfläche hinter der Außenfläche** | **0,17 mm** – der Finger erreicht sie |

Der Sensor wird **von innen eingesetzt und eingeklebt**, bündig nach außen. Das
Flachband läuft von dort zum ZIF-Anschluss auf der Platine.

Das Blindstück der Stufe 1 geht denselben Weg: Körper Ø 11,10 × 1,00 mm, innen
ein Bund Ø 12,80 mm, der es festhält. Außen schließt es bündig mit dem Boden ab.

### Was das für die Platine bedeutet

**Die Platine braucht neben der Zelle keine 11 mm freie Fläche mehr** – nur den
ZIF-Anschluss und einen Weg für das Flachband. Das Flachband ist dünn und
biegsam; es darf über anderes hinweglaufen.

**Damit ist der Grund, aus dem die Platine von 24 auf 27 mm gewachsen ist,
möglicherweise entfallen.** Das ist keine Entscheidung, die ich treffe:

- **Die 27 mm kosten 1 mm Gehäusebreite** gegenüber 26 und 3 mm gegenüber 24 –
  mehr nicht. Dicke und Länge sind unberührt.
- **Das Gehäuse funktioniert bei jeder der drei Breiten.** Eine Zeile in
  `parameters.csv`, ein neuer Lauf.
- Ob die Platine die gewonnene Fläche braucht, weiß der Platinen-Strang, nicht
  ich. Auf einer Platine, auf der es eng zugeht, sind 11 mm Breite viel.

**Nicht geprüft, und es fehlt dafür eine Angabe:** wie lang das Flachband des
FPC2534AM ist und wohin es abgeht. Steht in `../docs/fingerprint.md` nicht.
Davon hängt ab, wo der Sitz im Boden liegen darf, ohne dass das Band zu kurz
oder zu lang wird. **Diese Zahl wird gebraucht**, bevor der Sitz festgelegt
wird; bis dahin steht er auf längs 36,0 / quer 19,20 mm.

### Zwei Nebenfolgen

- **Die Frage, was Ferrit unter dem Sensor anrichtet, stellt sich nicht mehr.**
  Die Ferritfolie liegt zwischen Zelle und Platine, der Sensor sitzt 3,8 mm
  tiefer im Boden. Sie können einander nicht überlagern. Die vorsichtige
  Lesart kostet also nichts und darf bleiben.
- **Das Sensorfeld ist 11 × 11 mm, nicht 11 × 30,5 mm.** Der FPC2534 ist rund,
  Ø 11,0 mm. Die ganze Länge der Zelle dafür zu sperren wäre rund 20 mm zu viel.
  Was wirklich Platz braucht, ist der ZIF-Anschluss und der Weg des Flachbands.

## 15. Zwei Maße, die das Layout von mir braucht

### d: die Gehäusefront steht 1,90 mm vor der Platinenkante

Gefragt vom Platinen-Strang am 24.10.2026, um die USB-Buchse zu setzen statt zu
schätzen.

| Ebene | längs |
| --- | --- |
| Platinenkante | 0,00 mm |
| Innenseite der Frontwand (Spiel 0,30) | −0,30 mm |
| **Außenseite der Frontwand** (Wand 1,60) | **−1,90 mm** |

**Ohne Mulde wäre d = 1,90 mm** und die Buchsenmitte 2,685 mm. **Die Prüfung am
Footprint ist durchgefallen** – siehe unten. Darum hat die Frontfläche jetzt
eine Mulde, und es gilt:

| | Wert |
| --- | --- |
| Muldentiefe | 0,80 mm |
| **d im Muldengrund** | **1,10 mm** |
| **Buchsenmitte = 9,17/2 − d** | **3,485 mm** |
| Überstand des Körpers über die Platinenkante | 1,10 mm |
| Restwand an der Öffnung | **0,80 mm** = zwei Druckbahnen |

### Warum die Mulde da ist – nicht aus Formgründen

**Das ist der Vermerk für den, der sie später wegmachen will.** Bei einer
Durchsteckbuchse darf der **Körper** über die Platinenkante ragen, die **Pins
nicht.** Der Platinen-Strang hat am gesetzten Footprint gemessen:

| bei d = 1,90 (bündig, ohne Mulde) | reicht bis | über die Kante |
| --- | --- | --- |
| Pad eines Haltestifts | −0,344 | **0,344 mm** |
| **Bohrloch** | −0,195 | **0,195 mm** |

**Ein Bohrloch auf der Platinenkante ist kein Schönheitsfehler, sondern ein
Fertigungsfehler.** Für 0,2 mm Pad-Rand muss die Buchsenmitte bei ≥ 3,23 mm
liegen; die bündige Vorderkante verlangte 2,685 mm. **Die beiden Auflagen
widersprachen sich um gut einen halben Millimeter.**

Mit der Mulde liegt die Mitte auf 3,485 mm: das vordere Pad beginnt bei +0,456,
das Loch bei +0,605 – **0,256 mm Luft** über der Forderung. Der Platinen-Strang
hat J1 bereits dort gesetzt.

**Wer die Mulde entfernt, holt sich das Bohrloch auf der Platinenkante zurück.**
Die Alternativen wären eine dünnere Frontwand als 0,8 mm oder die ± 0,3 mm
aufzugeben – beides schlechter.

| Maß der Mulde | Wert |
| --- | --- |
| tief | 0,80 mm (von 1,60 mm Wand) |
| breit | 14,00 mm (Öffnung 10,5 + 1,75 je Seite) |
| hoch | −0,60 … 4,16 mm um die Trennebene |

**Die Dicke ändert sie nicht:** die Mulde liegt in der Frontfläche, nicht im
Dach. Vorn bleiben **11,01 mm**.

### Bauteile beginnen bei quer 1,40, nicht bei 1,20

Die Wand des Hohlraums steht bei **1,20 mm** – dort hört der Klemmkragen auf.
Ein Bauteil, das genau bei 1,20 beginnt, **berührt die Wand.**

Gedruckte Flächen haben je 0,1–0,2 mm Toleranz. **Zelle und Ferritfolie sollten
also bei 1,40 mm beginnen**, damit 0,2 mm Luft bleibt. Das kostet nichts: die
Aufteilung quer hat 1,10 mm Reserve.
