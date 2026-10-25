# USB-C-Stecker für den Stick

Stand: 25.10.2026

Das Gerät soll ein Stick sein, den man direkt in den Rechner steckt. Dafür
braucht die Platine an der Kante einen USB-C-**Stecker** (Male), keine Buchse.
Die Platine ist **0,8 mm** dick, 27 × 70 mm, vier Lagen. Die Dicke steht fest.

Gesucht war ein Teil, das auf eine 0,8-mm-Platine passt, wenig Platz kostet,
eine Metallhülle mit Verriegelung hat und kurzfristig lieferbar ist.
USB 2.0 genügt uns; alle gefundenen Teile sind trotzdem 24-polig, weil es
24 Pole braucht, damit der Stecker in jede Buchse passt (CC-Pins).

Alle Zahlen unten stammen aus der Zeichnung des Herstellers, soweit nicht
anders vermerkt. Was nicht belegt ist, steht als `ungeprueft` da.

## Vergleich

| | **GCT USB4155-03-C** | **Same Sky UP31-CV-G-CM** | **Valcon VUSB3.1-C-P-G-EM1** | **Molex 1054440001** | **JAE DX07P024AJ5R1500** | **Wit-area W0029-24-EM1-S3** |
|---|---|---|---|---|---|---|
| **Plattendicke** | **0,8–1,0 mm**, Toleranz ±0,05 [1] | **0,60 ±0,05 mm** [2] | 0,8 mm – nur Händlerangabe, kein Datenblatt öffentlich [3] | `ungeprueft` [4] | `ungeprueft` [5] | Teilenummer existiert nicht [6] |
| **Passt auf unsere 0,8 mm?** | **ja, belegt** | **nein** | wahrscheinlich, unbelegt | `ungeprueft` | `ungeprueft` | – |
| **Länge über alles** | 15,50 ±0,30 mm [1] | 12,85 mm [2] | `ungeprueft` | `ungeprueft` | `ungeprueft` | – |
| **Überstand über die Platinenkante** | **13,0 mm** (gerechnet: 15,50 − 2,50) | `ungeprueft` | `ungeprueft` | `ungeprueft` | `ungeprueft` | – |
| **Höhe über alles** | **2,90 mm** [1] → ca. 1,05 mm über und 1,05 mm unter der Platine (gerechnet) | 3,48 mm [2] | `ungeprueft` | `ungeprueft` | `ungeprueft` | – |
| **Breite** | 8,80 mm (Hülle), 8,25 ±0,03 mm (Körper) [1] | 8,25 ±0,03 mm (Körper), 9,16 mm (Hülle) [2] | `ungeprueft` | `ungeprueft` | `ungeprueft` | – |
| **Platzbedarf auf der Platine (Tiefe ab Kante)** | **2,50 mm** (Pads max. 2,30) [1] | **6,35 mm** [2] | `ungeprueft` | `ungeprueft` | `ungeprueft` | – |
| **Platzbedarf in der Breite** | 7,40 mm [1] | 9,00 mm [2] | `ungeprueft` | `ungeprueft` | `ungeprueft` | – |
| **Pads** | 24 (12 oben, 12 unten), Raster 0,50 mm, Pad 0,20 mm breit [1] | 24 (12 oben, 12 unten), Raster 0,50 mm, Pad 0,34 mm breit, dazu große GND-Flächen [2] | 22-polig lt. Händler [3] | 24 (22 + 2 Blind) [4] | 24 [5] | – |
| **Besondere Form an der Kante** | **nein** – gerade Kante, nur Pads und je zwei ovale Löcher für die Lötfahnen (ca. 1,00 × 1,80 mm) [1] | **ja** – zwei Aussparungen in der Kante [2] | `ungeprueft` | `ungeprueft` | `ungeprueft` | – |
| **Metallhülle** | ja, Edelstahl [1] | ja, Edelstahl [2] | ja, Stahl [3] | `ungeprueft` | `ungeprueft` | – |
| **Verriegelung (Latch)** | ja, Edelstahl [1] | ja, Edelstahl [2] | `ungeprueft` | `ungeprueft` | `ungeprueft` | – |
| **Steckzyklen** | 10.000 [1] | 10.000 [2] | 10.000 lt. Händler [3] | 10.000 [4] | 10.000 lt. Händler [5] | – |
| **Steck-/Ziehkraft** | 5–20 N / 6–20 N [1] | 5 N min / 8 N min [2] | `ungeprueft` | `ungeprueft` | `ungeprueft` | – |
| **Strom** | 5,00 A VBUS, 6,25 A GND [1] | 5,0 A [2] | `ungeprueft` | `ungeprueft` | `ungeprueft` | – |
| **Lager** | **15.070 Stück** bei DigiKey DE [7] | `ungeprueft` | 480 Stück bei Toby [3] | 8.861 Stück bei DigiKey [4] | `ungeprueft` | – |
| **Lieferzeit (Hersteller)** | **7 Wochen** [7] | `ungeprueft` | sofort ab Lager, darüber hinaus auf Anfrage [3] | **14 Wochen** [4] | `ungeprueft` | – |
| **Preis** | 1,46 € (1), 1,05 € (100) [7] | `ungeprueft` | 1,49 £ (1), 1,36 £ (100) [3] | 3,10 $ (1) [4] | `ungeprueft` | – |
| **Kleinste Bestellmenge** | 1 (Cut Tape) [7] | `ungeprueft` | 1 [3] | `ungeprueft` | `ungeprueft` | – |

### Was mit den übrigen vier Kandidaten ist

- **Same Sky UP31-CV-G-CM** ist mechanisch sauber belegt, scheidet aber aus:
  die Zeichnung schreibt eine **0,60 mm** dicke Platine vor, nicht 0,8 mm.
  Er bräuchte außerdem zwei Aussparungen in der Kante und 6,35 mm Tiefe –
  mehr als doppelt so viel wie der GCT.
- **Valcon VUSB3.1-C-P-G-EM1**: Das Datenblatt liegt bei Toby hinter einer
  Anmeldung, und Valcon hat keine öffentliche Produktseite dazu. Die 0,8 mm
  und die 10.000 Zyklen sind **nur Händlerangaben**. Ohne Zeichnung kein Maß.
- **Molex 1054440001**: 14 Wochen Lieferzeit beim Hersteller – genau das Teil,
  an dem das Projekt gestern gescheitert ist. Die Plattendicke steht nicht in
  der Händlerliste; ich habe sie nicht belegt.
- **JAE DX07P024AJ5R1500**: Das Serien-Datenblatt (DX07P024xJ1R) war über die
  Händlerlinks nicht abrufbar, die Herstellerbroschüre bemaßt eine andere
  Variante und nennt die Paddle Card ausdrücklich als nicht enthalten.
  Keine belegte Plattendicke.
#### Was bei Valcon und JAE konkret fehlt

Falls später doch ein Datenblatt vorliegt: Es sind genau diese sechs Zahlen,
die der Vergleich braucht, und zwar aus der Zeichnung, nicht aus einer Liste.

1. **Vorgeschriebene Plattendicke** – steht beim GCT unter „Recommended PCB
   Layout". Nur wenn dort 0,8 mm im Fenster liegt, ist das Teil überhaupt
   im Rennen. (Beim Same Sky stand dort 0,60 mm – daran ist er gescheitert.)
2. **Höhe über alles** – zum Vergleich mit den 2,90 mm des GCT.
3. **Tiefe des Lötbilds ab Platinenkante** – zum Vergleich mit den 2,50 mm.
4. **Ob die Kante eine Sonderform braucht** – Zunge, Fase oder Aussparung.
   Der GCT braucht keine; das ist einer seiner Vorteile.
5. **Gesamtlänge und Einbautiefe**, damit sich der Überstand rechnen lässt.
6. **Ziehkraft und ob eine Verriegelung verbaut ist.**

Beim **Valcon** liegt das Datenblatt bei Toby hinter einer Anmeldung, und
Valcon selbst hat keine öffentliche Produktseite dazu; angefragt wurde nichts.
Beim **JAE** ist das Serienblatt DX07P024xJ1R über die Händlerlinks nicht
abrufbar, und die öffentliche Herstellerbroschüre bemaßt die Variante MJ1,
nicht unsere AJ5R – deren Maße gelten also nicht.

- **Wit-area W0029-24-EM1-S3** gibt es nicht. Die Tabelle des Anbieters führt
  die Reihe **W0026**-24-EM1 – und das sind **Buchsen** (female), keine Stecker.
  Für einen Stick unbrauchbar. (W0026-24-EM1-S3 steht dort mit T = 0,8 mm.)

## Empfehlung

**GCT USB4155-03-C.**

Begründung, Punkt für Punkt:

1. **Die Plattendicke ist belegt.** Die Herstellerzeichnung schreibt unter
   „Recommended PCB Layout" wörtlich *Thickness 0.8 to 1.0 mm* [1]. Unsere
   0,8 mm liegen am unteren Rand des Fensters – also genau richtig, und die
   0,8 mm wurden ja gerade deswegen gewählt.
2. **Er ist der kleinste.** 2,90 mm Bauhöhe über alles; das ist das flachste
   Teil im Feld. Er kostet nur **2,50 mm Platinentiefe** – der Same Sky
   bräuchte 6,35 mm. Die 3,85 mm Unterschied sind bei 70 mm Gesamtlänge
   fast ein Zwanzigstel des Geräts.
3. **Die Kante bleibt gerade.** Keine Zunge, keine Fase, keine Aussparung –
   nur Pads und vier ovale Löcher. Das ist der einfachste Fall für die
   Fertigung und spart eine Fräskontur.
4. **Die Mechanik stimmt.** Edelstahlhülle, Edelstahl-Verriegelung,
   10.000 Steckzyklen, Ziehkraft 6–20 N nach dem Test [1].
5. **Er ist da.** 15.070 Stück bei DigiKey DE, Einzelstück ab 1,46 €,
   7 Wochen Herstellerlieferzeit [7]. Kein 14-Wochen-Problem.
6. **Der Hersteller bewirbt ihn genau für diesen Zweck** – Dongle-Entwürfe
   mit der Platine im Stecker.

### Was dagegen spricht

- **Die Zugkraft geht in die 0,8-mm-Platine.** Der Stecker hat keine eigene
  Zugentlastung. Beim Ziehen hält ihn nur, was ihn an der Platine hält: zwei
  Lötfahnen je Seite (Durchsteckbeine in den ovalen Löchern) und 24 SMD-Pads
  mit 0,20 mm Breite. Die Herstellerzeichnung nennt 6–20 N Ziehkraft – das
  wirkt als Hebel auf eine 0,8 mm dünne Platine, die täglich gesteckt wird.
  **Das Gehäuse muss den Stecker mechanisch fassen**, nicht die Platine.
  Diese Last ist nicht geprüft und wird es erst am Muster.
- **24 Pole, obwohl wir USB 2.0 fahren.** Es gibt im Feld keinen kleineren
  Stecker – ein USB-C-Stecker braucht 24 Kontakte, damit er in jeder Buchse
  erkannt wird. Wir legen also 16 Pads, die nichts tun. Das ist kein Nachteil
  im Platz, aber 16 Pads, die beim Löten trotzdem sitzen müssen.
- **Der Überstand von 13,0 mm ist gerechnet, nicht bemaßt.** In der Zeichnung
  steht die Gesamtlänge (15,50) und die Einbautiefe (2,50), aber kein Maß
  „ab Platinenkante". Vor dem Gehäuseentwurf gehört das am Muster gemessen.
- **Die Pads liegen auf beiden Seiten.** Der Stecker ist ein Straddle Mount:
  12 Pads oben, 12 unten. Das heißt zwei Lötvorgänge bzw. Reflow in zwei
  Durchgängen, und die Zeichnung verlangt dafür eigens eine Vorrichtung
  (Tray und Gewicht, siehe Blatt 3 der Zeichnung) [1].
- **Die 2,90 mm sind nicht die Gerätehöhe.** Die Platine sitzt mittig im
  Stecker. Baut das Gehäuse dicker als 2,90 mm, steht der Stecker nicht
  mittig im Gerät – das ist Sache des Gehäuseentwurfs.

## Die zweite Möglichkeit: Stick mit kurzem festem Kabel

Der Nutzer hat selbst ein Gerät mit wenigen Zentimetern fest verbundenem
Kabel genannt. Das wurde geprüft.

**Befund: Es gibt dafür kein Bauteil, das man wie den GCT bestellen kann.**

- Was der Handel führt, sind **Konsumteile**: USB-C-Stecker mit 2, 4 oder 6
  abisolierten Litzen, 20–22 AWG, 25–30 cm lang, über Amazon/eBay [8].
  Kein Datenblatt, keine Spezifikation, kein Hersteller, der dafür geradesteht.
  Für ein Sicherheitsgerät ist das nichts.
- Ein ordentliches Teil wäre eine **kundenspezifische Kabelbaugruppe**:
  umspritzter Stecker, definierte Länge, Zugentlastung. Das heißt Werkzeug,
  Mindestabnahme und eine Anfrage beim Hersteller – und Anfragen stellt der
  Nutzer selbst, nicht ich. Die Lieferzeit dafür ist `ungeprueft`, liegt aber
  erfahrungsgemäß über den 7 Wochen des GCT.

**Ein Fund, der gegen die billige Variante spricht:** Die Konsumteile werden
mit **2, 4 oder 6 Adern** verkauft, und die weitaus meisten haben **zwei** –
rot und schwarz, reine Stromversorgung, ausdrücklich „no data function" [8].
Die taugen für uns nicht einmal als Muster. Wir brauchen mindestens **sechs**
Adern: VBUS, GND, D+, D− und **beide CC-Leitungen**, denn die CC-Beschaltung
muss bis auf unsere Platine durchkommen – am Kabelende sitzt sie nicht.
Welche Beschaltung dort genau hingehört, ist `ungeprueft`; die Zeichnung des
GCT nennt A5 und B5 schlicht „Configuration Channel" und sagt nichts darüber.
Das heißt: Die 6-adrigen Teile wären das Mindeste, und auch bei denen steht
nirgends, wie die beiden CC-Adern innen aufgelegt sind. Ohne diese Auskunft
ist keines der Handelsteile verwendbar – auch nicht zum Ausprobieren.

**Was es an Platz kostet, gegenüber dem direkten Stecker:**

| | direkter Stecker (GCT) | kurzes festes Kabel |
|---|---|---|
| Platz auf der Platine | 2,50 mm Tiefe, 7,40 mm breit, Pads | 4–6 Lötpads oder ein Draht-an-Platine-Steckverbinder, dazu eine Zugentlastung, die die Litzen festhält – `ungeprueft`, aber nicht kleiner |
| Platz im Gehäuse | keiner, der Stecker ragt heraus | das Kabel muss irgendwo hin: angelegt, eingeklappt oder aufgewickelt. Eine Mulde oder ein Scharnier im Gehäuse kostet mehr als die 2,50 mm auf der Platine |
| Länge des Geräts | 70 mm + 13,0 mm Stecker | 70 mm + Kabellänge, die man nicht wegklappen kann, ohne Mechanik dafür zu bauen |
| Mechanik | Zugkraft geht in die Platine | **Zugkraft geht ins Kabel und in die Zugentlastung, nicht in die Platine** – das ist der eine echte Vorteil |
| Beschaffung | ab Lager, 1,46 € | Sonderanfertigung, Werkzeugkosten, Mindestmenge |

**Bewertung:** Das Kabel löst genau ein Problem – die Zugkraft auf der dünnen
Platine – und schafft dafür vier neue: keine Normteile, Sonderanfertigung,
mehr Gehäuse, und der Nutzer wollte ausdrücklich etwas, das man ohne Kabel
mitnimmt. Der direkte Stecker ist die bessere Lösung, **solange das Gehäuse
den Stecker fasst**. Erweist sich das am ersten Muster als zu schwach, ist
das Kabel der Rückfallweg – dann aber als Sonderanfertigung mit Vorlauf.

## Quellen

| Nr. | Quelle | Abgerufen |
|---|---|---|
| [1] | GCT, Zeichnung USB4155, Rev. A4 vom 21.09.26, Blatt 1–4 von 4 — `https://gct.co/files/drawings/usb4155.pdf` | 25.10.2026 |
| [2] | Same Sky (vormals CUI Devices), Datenblatt UP31-CV-G-CM vom 09.12.2024, Blatt 1–4 — `https://www.sameskydevices.com/product/resource/up31-cv-g-cm.pdf` | 25.10.2026 |
| [3] | Toby Electronics, Produktseite VUSB3.1-C-P-G-EM1 (Händlerangabe, Datenblatt nur nach Anmeldung) — `https://www.toby.co.uk/signal-to-board-connectors/usb-connectors/vusb31-valcon-usb-type-c-pcb-edge-mount-plug/` | 25.10.2026 |
| [4] | DigiKey, Produktseite Molex 1054440001 (Händlerangabe) — `https://www.digikey.com/en/products/detail/molex/1054440001/5843889` | 25.10.2026 |
| [5] | DigiKey/LCSC, Listungen DX07P024AJ5R1500 (Händlerangabe); Herstellerdatenblatt nicht abrufbar | 25.10.2026 |
| [6] | Wit-area, Leitfaden Straddle-Mount-Buchsen (Tabelle führt W0026, nicht W0029; „Detailed dimensional drawings can be provided upon request") — `https://www.wit-area.com/blog/USB-Type-C-Straddle-Mount-Female-Connector-A-Complete-Technical-Guide/` | 25.10.2026 |
| [7] | DigiKey DE, Produktseite USB4155-03-C: Lager, Preis, Lieferzeit — `https://www.digikey.de/en/products/detail/gct/USB4155-03-C/14305161` | 25.10.2026 |
| [8] | Handelslistungen USB-C-Stecker mit offenen Litzen (Amazon, eBay) – ohne Datenblatt | 25.10.2026 |

**Nicht abrufbar:** Mouser (Bot-Schutz), Valcon-Datenblatt (Anmeldung bei
Toby), JAE DX07P024xJ1R-Datenblatt. Dort steht überall `ungeprueft`.

## Was als Nächstes zu tun ist

1. Der Schaltplan-Strang tauscht die Buchse GCT USB4085-GF-A gegen den
   Stecker GCT USB4155-03-C. Pinbelegung: A1–A12 / B1–B12 wie in [1],
   Blatt 2. Für USB 2.0 werden gebraucht: VBUS (A4, A9, B4, B9),
   GND (A1, A12, B1, B12), D+ (A6, B6), D− (A7, B7), CC1 (A5), CC2 (B5).
   Die acht SuperSpeed-Pins und die beiden SBU-Pins bleiben offen.
2. Das Platinen-Layout bekommt an der Kante das Lötbild aus [1], Blatt 2:
   24 Pads im 0,50-mm-Raster auf beiden Seiten, max. 2,30 mm tief, dazu
   vier ovale Löcher für die Lötfahnen. Kante bleibt gerade.
3. Der Gehäuse-Strang muss den Stecker fassen, nicht die Platine.
4. Am ersten Muster messen: Überstand ab Platinenkante, und ob die
   Steckverbindung die Platine hält.
