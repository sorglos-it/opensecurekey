# Was beim Zeichnen des Schaltplans herauskam

Stand 24.10.2026. Gezeichnet in EasyEDA Pro 3.2.209 über die Sorglos-EDA-Brücke,
Projekt `opensecurekey`, Board1, Schaltplanseite P1 (A4).

**Stand: gezeichnet, DRC null Fehler, Netzliste abgeglichen. Nichts gefertigt,
nichts elektrisch gemessen.** Das Layout kommt in einem späteren Durchgang;
was dort als erstes zu tun ist, steht in `layout-notes.md`.

## Was auf der Seite steht

54 Bauteile, 59 Netze, 219 Pinverbindungen, 102 Netzfahnen. Jede Verbindung ist
mit einer kurzen Stichleitung abgesetzt und mit dem Netznamen beschriftet;
Versorgungs- und Massenetze tragen eine Fahne am Ende der Stichleitung, vom
Bauteil weg gedreht. Nichts klebt direkt am Pin.

| Kennung | Teil | LCSC |
| --- | --- | --- |
| U1 | Raytac MDBT50Q-1MV2 | C5118826 |
| U2 | NXP SE051C2HQ1/Z01XDZ | C5202100 |
| U3 | Winbond W25Q512JVEIQ | C7389628 |
| U4 | Microchip MCP73832T-2ACI/OT | C38066 |
| U5 | ST USBLC6-2SC6 | C323793 |
| J1 | GCT USB4085-GF-A | C7095263 |
| J2 | JST-PH 2-polig, liegend | C9900141337 |
| J3 | Stiftfeld 1×2, 2,54 mm (NFC-Antenne) | C492401 |
| J4 | FPC-ZIF 12-polig, 0,5 mm | C9900036520 |
| J5 | Stiftfeld 1×6, 1,27 mm (Fingerabdruck) | C41434767 |
| J6 | Stiftfeld 1×4, 2,54 mm (SWD) | C2691448 |
| SW1, SW2 | Panasonic **EVQP7C01P** | C388883 |
| D1 | Everlight 19-337C/RSBHGHC-A88/4T | C3019536 |
| D2 | Nexperia PESD5V0X1BCL,315 | C552589 |
| Q1–Q5 | Diodes DMG2305UX, SOT-23 | C2940629 |
| X1 | Epson Q13FC13500004, 32,768 kHz | C32346 |
| L1 | TDK NLCV25T-100K-EFRD, 10 µH, 1008 (2,5 × 2,0 mm), 360 mA, 1,32 Ω max | C295388 |
| C1, C2, C3, C5 | 4,7 µF 0603 X5R 16 V | C19666 |
| C4, C10, C11, C12, C14 | 100 nF 0402 X7R | C1525 |
| C8, C9 | 15 pF 0402 C0G | C307341 |
| C13 | 1 µF 0402 X5R | C52923 |
| C15, C16 | 120 pF 0402 C0G | C307442 |
| C17 | 10 nF 0402 X7R | C15195 |
| R1 | 10 kΩ 0402 | C25744 |
| R2–R5, R8, R9, R17 | 100 kΩ 0402 | C25741 |
| R6, R7 | 2,2 kΩ 0402 | C25879 |
| R10 | 1 kΩ 0402 | C11702 |
| R11, R12 | 470 Ω 0402 | C25117 |
| R13, R14 | 2,2 MΩ 0402 | C11490 |
| R15, R16 | 5,1 kΩ 0402 | C25905 |

## Symbole: keines selbst gezeichnet, und warum nicht

**Alle 54 Bauteile kamen aus der EasyEDA-Bibliothek, jedes mit der
Herstellernummer aus `../docs/parts.md`.** Selbst zeichnen war nicht möglich und
auch nicht nötig:

- Die Brücke hat **keine Werkzeuge zum Anlegen von Symbolen oder Footprints**
  (nur Lesen: `lib_symbol_get`, `lib_footprint_get`). Ein selbst gezeichnetes
  Symbol müsste Thomas im Bibliothekseditor von Hand anlegen.
- Für jedes Teil der Vorlage gab es das exakte Herstellerteil in der Bibliothek.
  Nur bei den Stiftfeldern J3 und J6 gibt es kein 1,27-mm-Teil mit 2 bzw. 4
  Polen; dort stehen jetzt 2,54-mm-Stiftfelder. Für den Schaltplan ist das
  gleichgültig, **fürs Layout nicht** — siehe offene Punkte.

**Die Pad-Maße sind damit noch ungeprüft.** Die Hausregel verlangt sie aus dem
Gerber-Export, nicht aus der Bibliothek. Das gehört in den Layout-Durchgang,
bevor eine Platine bestellt wird; im Schaltplan spielt das Footprint keine Rolle.

## Zwei Funde am Modul, die in `schematic.md` gehören

### 1. Das Raytac-Modul hat keine DEC4- und keine DECUSB-Pins

`schematic.md` schreibt C6 = 1 µF an `DEC4` und C7 = 4,7 µF an `DECUSB` vor.
**Das Symbol des MDBT50Q-1MV2 hat diese Pins nicht.** Die 61 Modulpins sind: 48
GPIOs, 5 × GND, VDD, VDDH, DCCH, VBUS, D+, D−, SWDIO, SWDCLK — und sonst nichts.
Das passt zu dem, was die Vorlage selbst über das Modul sagt (Spule für REG1 und
32-MHz-Quarz sind intern): die DEC-Kondensatoren sitzen ebenfalls im Modul.

**C6 und C7 sind darum nicht gezeichnet.** Die Nummern C6 und C7 bleiben frei,
damit die übrigen Kennungen zur Vorlage passen. `ungeprueft`: am gerenderten
Raytac-PDF bestätigen, bevor bestellt wird — wenn das Modul die Pins doch
herausführt, fehlen zwei Kondensatoren.

### 1a. L1 ist die REG0-Spule, nicht die Funkspule

Das Datenblatt sagt es in einem Satz: *„32MHz crystal and RF (VDD) DC/DC
inductor (Reg1) are already inside the module."* **L1 an `DCCH` ist die Spule
der REG0-Stufe** — die Funkspule für REG1 steckt im Modul. Wer das verwechselt,
sucht an der falschen Stelle, wenn der Wirkungsgrad nicht stimmt.

**Gewählt: TDK NLCV25T-100K-EFRD**, 10 µH ±10 %, Gehäuse 1008
(2,5 × 2,0 mm), **Nennstrom 360 mA**, Gleichstromwiderstand **1,32 Ω max**,
AEC-Q200 (C295388).

> **Für die Stückliste gilt der Höchstwert, nicht der typische.** Die früher
> hier genannten 1,1 Ω sind der **typische** Wert der Schwesterausführung
> **EFR**; DigiKey nennt für die **EFRD** 1,32 Ω max. Am Vergleich ändert das
> nichts: 1,32 Ω sind immer noch weniger als die Hälfte der 2,96 Ω des
> Murata-Teils.

Begründung, und sie ist eine Korrektur meiner ersten Wahl: Beim Schaltregler
muss der **Sättigungsstrom** über dem **Spitzenstrom** liegen, nicht über dem
Dauerstrom. Mein erstes Teil (TDK MLZ1608N100LT000, 0603) hat 300 mA
Dauerstrom, geht aber schon bei **60 mA in die Sättigung** — dann bricht die
Induktivität ein, die Welligkeit steigt und der Wirkungsgrad fällt, und zwar
genau bei den Funkspitzen, wo es darauf ankommt. Die 300 mA helfen dort nichts.

Mit 360 mA liegt der Nennstrom mehr als doppelt über der Vorgabe von ≥ 150 mA,
und **1,32 Ω sind weniger als die Hälfte** von dem, was der nächstbeste
Kandidat hat (Murata LQH2MCN100M52L: 2,96 Ω; TDK MLZ2012N100 hätte 300 mΩ,
geht aber schon bei 110 mA in die Sättigung). Bei einem Akkugerät zahlt der
Kupferwiderstand jede Sekunde mit. AEC-Q200 bringt die bessere
Temperaturdokumentation.

> **`ungeprueft` — und das bleibt offen:** **Ein Sättigungsstrom aus
> TDK-Unterlagen ist für dieses Teil nicht zu bekommen.** LCSC führt `Isat`
> ausdrücklich als „-", und TDKs eigene Produktseite antwortet auf Abruf mit
> **HTTP 403** — aus zwei Sitzungen heraus. Womit TDK den Nennstrom bemisst
> (Temperaturanstieg, Induktivitätsabfall oder der kleinere der beiden), steht
> an keiner erreichbaren Stelle; jede Aussage dazu wäre erfunden.
>
> **Was es doch gibt, und mit welchem Gewicht:** **Arrow** nennt für die EFRD
> „Maximum Saturation Current **0,36 A**" — dieselbe Zahl wie der Nennstrom.
> Das ist eine **Händlerangabe, die sehr wahrscheinlich nur den Nennstrom
> wiederholt**, und damit kein unabhängiger Beleg. Sie steht hier, weil „eine
> Händlerzahl, die passt" mehr ist als „nichts auffindbar" — aber sie ersetzt
> das Datenblatt nicht.
>
> **Für die Entscheidung reicht es:** 360 mA gegen einen Spitzenstrom in der
> Größenordnung einiger zehn Milliampere ist Reserve, egal nach welcher der
> beiden Regeln gemessen wird.
>
> Quellen: [DigiKey](https://digikey.com/en/products/detail/NLCV25T-100K-EFRD/445-181381-1-ND/9740690)
> (DCR 1,32 Ω max), [Arrow](https://www.arrow.com/en/products/nlcv25t-100k-efrd/tdk?region=nac)
> (Saturation Current 0,36 A), LCSC C295388 (Nennstrom 360 mA, `Isat` leer).

**Abweichung von der Vorgabe:** Gewünscht war 0805 (2,0 × 1,25 mm). **Kein
0805-Teil der Bibliothek weist einen Sättigungs- oder Nennstrom ≥ 150 mA aus**
(bestes: MLZ2012N100LT000 mit 110 mA). Das gewählte Teil ist 1008, also
0,5 mm länger und 0,75 mm breiter als 0805. Bewusst bezahlt.

### 2. XL1/XL2 liegen anders als in `pinout.md`

Die Modulpinnummern stammen aus dem Bibliothekssymbol. **Zehn von zwölf
Nummern, die `pinout.md` als „einzeln nachgelesen und sicher" nennt, stimmen
genau** (40 = P0.18/nRESET, 41 = P0.17, 42 = P0.19, 43 = P0.21, 44 = P0.20,
45 = P0.23, 46 = P0.22, 47 = P1.00, 52 = P0.09/NFC1, 54 = P0.10/NFC2).

**Zwei stimmen nicht:** `pinout.md` nannte (20) XL1 und (23) XL2, das
Bibliothekssymbol hat **17 = P0.00/XL1 und 18 = P0.01/XL2**. Gezeichnet ist nach
dem Symbol.

**Am 24.10.2026 am Datenblatt entschieden: das Symbol hat recht.** Approval
Sheet Abschn. 2.5, gerenderte Seite 17, in den Fassungen K und H wortgleich:
(17) P0.00/XL1 und (18) P0.01/XL2, beide „Connection for 32.768 kHz crystal".
(20) ist `P0.04/AIN2`, (23) ist `P0.07/TRACECLK`. Hätte ich der Vorlage
geglaubt, läge der Quarz an zwei beliebigen GPIOs — Uhr tot, TOTP tot.
`pinout.md` ist berichtigt.

Die übrigen Modulpinnummern, die `pinout.md` offenließ, stehen jetzt fest:
3 = P1.10, 4 = P1.11, 5 = P1.12, 6 = P1.13, 7 = P1.14, 8 = P1.15, 16 = P0.27,
19 = P0.26, 20 = P0.04, 22 = P0.06, 24 = P0.08, 27 = P0.11, 28 = VDD, 30 = VDDH,
31 = DCCH, 32 = VBUS, 33 = GND, 34 = D−, 35 = D+, 36 = P0.14, 37 = P0.13,
38 = P0.16, 39 = P0.15, 48 = P0.24, 49 = P0.25, 50 = P1.02, 51 = SWDIO,
53 = SWDCLK, 60 = P1.03, 61 = P1.01; GND = 1, 2, 15, 33, 55.

## Was ich entscheiden musste

| Stelle | Entscheidung | Grund |
| --- | --- | --- |
| C8, C9 (Quarzlast) | **15 pF** | X1 hat 12,5 pF Lastkapazität, die Pins 4 pF: C = 2 × (12,5 − 4) = 17 pF, nächster Normwert 15 pF. Streukapazität der Leiterbahn kommt hinzu, darum nach unten gerundet. |
| X1 | Epson Q13FC13500004 | erfüllt alle drei Vorgaben der Vorlage: Lastkapazität 12,5 pF (max. 12,5), ESR 70 kΩ (max. 100), ±20 ppm (BLE braucht ±500) |
| C15, C16 (NFC) | **120 pF** als Richtwert | Nordic nennt für 2 µH Antenneninduktivität „etwa 130 pF je Pin"; 120 pF ist der nächste Normwert darunter. **Der Wert ist zu messen, nicht zu glauben** — die Vorlage sagt das selbst. |
| Netzname `VBAT` | heißt jetzt **`VBAT_SW`** | EasyEDA verrechnet den Namen `VBAT` mit GND: alle vier Pins des Akkunetzes landeten auf Masse. Mit `VBAT_SW` ist das Netz sauber. Das ist eine Eigenheit des Werkzeugs, keine Änderung der Schaltung. |
| Netzname `nRESET` | heißt **`NRESET`** | EasyEDA schreibt Netznamen groß. |
| Q2/Q3-Kreuzsteuerung | Q2-Gate über R4 an `VBAT_SW`, Q3-Gate über R5 an `VBUS` | Die Vorlage sagt nur „Kreuzsteuerung". So leitet bei liegendem USB Q2 und Q3 sperrt (Gate 5 V über Source 4,2 V), ohne USB leitet Q3 (Gate auf Masse). |
| FP_IRQ (`P1.03`) | **Netz mit einem Pin**, Testpunkt | J5 hat nur 6 Pole, alle belegt. Steht als DRC-Warnung drin und soll dort stehen bleiben, bis entschieden ist, ob J5 auf 1×7 wächst. |
| J4 Pin 13/14 | an `GND` | Das gewählte FPC-Teil hat **14 Pole**: 12 Signale plus zwei Haltelaschen. Die Laschen gehören mechanisch auf Masse. Die Vorlage kennt nur 12. |
| J2 Pin 3/4 | an `GND` | dasselbe beim Akkustecker: zwei Haltelaschen. |
| SW1/SW2 | beide Kontaktpaare verdrahtet | Das Symbol hat vier Pole — zwei parallele Kontakte. Beide Seiten liegen auf demselben Netz, so wie das Teil gebaut ist. |
| SW1/SW2 Typ | **EVQP7C01P statt EVQP7A01P** | Beim A-Typ passen Symbol- und Padnamen nicht zusammen; das war der eine DRC-Fehler. Einzelheiten im DRC-Abschnitt. |
| U2 Pin 21, U3 Pin 9 | an `GND` | Mittelpad (EP). Bei U2 verlangt das Datenblatt es nicht, empfiehlt es aber thermisch; bei U3 ist es üblich. |
| J3, J6 | 2,54-mm-Stiftfelder | Für 1,27 mm gibt es in der Bibliothek kein 2- und kein 4-poliges Teil. Fürs Layout zu ersetzen. |
| Unbenutzte Pins | 29 **No-Connect-Fahnen** | 16 an U1 (ungenutzte GPIOs und Trace-Pins), 10 an U2 (n.c. und ISO7816), J1 A8/B8 (SBU), J4 Pin 2 (D2, nur für I2C-Betrieb). So bleibt „offen" eine Aussage und kein Versehen. |

## Abgleich gegen `schematic.md`

Die Netzliste aus EasyEDA (59 Netze) ist Pin für Pin gegen die Tabellen in
`schematic.md` gelesen worden. **Jede Verbindung, die dort steht, ist gezeichnet;
keine, die dort nicht steht, ist gezeichnet** — mit den oben genannten
Abweichungen, die alle einen Grund und eine Quelle haben.

Die drei Vorgaben, auf die die Vorlage besonders hinweist, stehen im Plan:

- **`SE_VOUT`** verbindet U2 Pin 15 (Vout) mit U2 Pin 18 (VCC) und C11 — und
  **nicht** mit `VDD3V3`. Nur so greift Deep Power-down mit 3 µA statt 430 µA.
  `ENA` (Pin 11) hängt an `P1.10`.
- **`VDDH`** (Modulpin 30) hängt an `VSYS`, nicht `VDD`. Die 3,3-V-Schiene
  `VDD3V3` kommt aus Modulpin 28.
- **Zwei getrennte 5,1-kΩ-Widerstände** an CC1 und CC2 (R15, R16), kein
  gemeinsamer.

Dazu die Berichtigung, die dieser Durchgang gefunden hat und die in
`schematic.md` nachgetragen ist: **U5 Pin 5 ist VBUS, nicht GND**, und das Teil
ist ein Durchgangsteil (I/O1 = Pin 1 und 6, I/O2 = Pin 3 und 4). Die vorige
Fassung hätte beim ersten Einstecken `VBUS` gegen Masse kurzgeschlossen.

## DRC

| | |
| --- | --- |
| **Fehler** | **0** — `passed: true` |
| Warnung | `FP_IRQ` ist ein Netz mit einem Pin — gewollt, Testpunkt (siehe oben) |

### Der Fehler, der eine Runde gekostet hat: der Taster passte nicht zu sich selbst

Zwei Tage lang stand hier „1 Fehler, von der Schnittstelle nicht benannt".
EasyEDA gibt über die Brücke nur Zähler heraus; den Wortlaut hat Thomas aus dem
DRC-Bereich vorgelesen:

```
[Error] The pin of the component EVQP7A01P does not correspond to the pad
(Pin has no corresponding pad : 1'、2' ; Pad has no corresponding pin: 1'、2')
: $1I42, $1I43
```

Das waren SW1 und SW2. **Dieselben zwei Namen stehen auf beiden Seiten** —
einmal als „Pin ohne Pad", einmal als „Pad ohne Pin". Es fehlt also kein Pad;
der **Namensvergleich** scheitert.

**Gemessen, soweit die Brücke es hergibt:**

- Die **Symbolseite** des EVQP7A01P führt die vier Pole als `1`, `2`, `1'`,
  `2'`. Die Brücke gibt die beiden letzten als `1&apos;` und `2&apos;` heraus —
  `&apos;` ist die HTML-Entität für das **gerade Hochkomma U+0027**.
- Die **Padseite ist nicht auslesbar.** `lib_footprint_get` liefert nur
  Kopfdaten (Name, Klassifikation, Beschreibung), keine Padliste; eine Padliste
  gibt es erst, wenn das Footprint auf einer Platine liegt. **Welches Zeichen
  die Padnamen tragen, ist mit der Brücke nicht festzustellen** — insoweit
  bleibt die Ursache `ungeprueft`.
- **Ein Beleg dafür, dass in dieser Teilefamilie verschiedene Zeichen umlaufen,
  liegt aber vor:** Der Schwestertyp **EVQP7C01P** benennt seine Pole
  ``1``, ``2``, ``1` ``, ``2` `` — mit **Backtick U+0060**, nicht mit Hochkomma.
  Zwei Panasonic-Taster derselben Reihe, zwei verschiedene Zeichen. Dass beim
  A-Typ Symbol und Footprint auf verschiedene Zeichen fallen, ist damit die
  naheliegende Erklärung, und sie passt genau zur Meldung.

**Lösung: Tausch auf den EVQP7C01P** (C388883). Gleicher Hersteller, gleiche
Reihe, **gleiche Maße** 3,5 × 3,55 × 1,35 mm, gleiche Betätigungskraft 2,2 N,
50 mA, seitlich betätigt, gleiches 3D-Modell — **das Layout muss nicht umgeplant
werden.** Das Symbol selbst zu berichtigen war kein Weg: die Brücke kann
Bibliotheksteile nur lesen.

**So ist der Tausch geprüft worden, in dieser Reihenfolge:**

1. Den C-Typ **zuerst als Wegwerfteil SW3** auf die Seite gesetzt und den DRC
   laufen lassen: Fehlerzahl blieb **1** (also der A-Typ), der C-Typ brachte
   **keinen** dazu. Damit war belegt, dass er mit seinem Footprint
   zusammenpasst, *bevor* die alten Taster gelöscht wurden.
2. SW3 und die alten SW1/SW2 gelöscht, die neuen an **dieselben Koordinaten**
   gesetzt. Die Pinabstände sind identisch (±7,62 mm und ±2,54 mm), darum
   greifen die vorhandenen Stichleitungen wieder: der Änderungssatz meldet
   **8 Stiche übersprungen** statt neu gezogen.
3. Die acht Pins gegen EasyEDAs Netzliste zurückgelesen: **null Abweichungen**.
   `BUTTON`, `NRESET` und `GND` stehen wie vorher.
4. `sch_drc_run`: **`passed: true`, 0 Fehler, 1 Warnung** (FP_IRQ).

Die Haltelaschen `1'`/`2'` liegen beim neuen Teil wie vorher auf demselben Netz
wie der jeweilige Hauptkontakt — es sind zwei parallele Kontakte, nicht
Befestigungspunkte. (Bei J2 und J4 war es anders: dort sind die Zusatzpole
wirklich Laschen und liegen auf `GND`.)

## Eigenheiten des Werkzeugs, die Zeit gekostet haben

Für den nächsten Durchgang festgehalten:

1. **Bauteile müssen auf dem 1,27-mm-Raster sitzen.** Liegt ein Pin daneben,
   scheitert jede Stichleitung an diesem Pin mit „create failed" — ohne
   verwertbare Meldung. Alle Koordinaten als Vielfache von 1,27 mm (besser 2,54)
   angeben.
2. **Stichleitungen mit Namen werden selbsttätig verlängert**, bis der Name
   darauf passt. Zwei Bauteile, deren Pins sich zugewandt sind, brauchen darum
   **mindestens 30 mm Abstand**, sonst treffen sich die Leitungen und die Netze
   verschmelzen still. Genau so sind beim ersten Versuch 51 Pins auf dem
   Nachbarnetz gelandet.
3. **Eine einzige Berührung zieht ein ganzes Netz mit.** Q1 stand 2,5 mm unter
   der GND-Fahne von U5 — damit lag das komplette Akkunetz auf Masse. Der
   Abgleich der Netzliste nach dem Zeichnen ist deshalb nicht Zierde, sondern
   Pflicht.
4. **Ein Bauteil verschieben reißt seine Stichleitungen ab.** Nach jedem
   Verschieben muss der Verdrahtungssatz mit `clear` neu laufen.
5. **`VBAT` ist bei EasyEDA ein belegter Netzname** und landet auf GND.
6. **Ein Bibliotheksteil kann zu sich selbst unverträglich sein.** Beim
   EVQP7A01P tragen Symbol- und Padnamen verschiedene Hochkomma-Zeichen, und
   EasyEDA meldet das als Fehler, dessen Text nur im DRC-Bereich steht. Prüfung
   ohne Handarbeit: **das Teil als Wegwerfbauteil setzen und den DRC laufen
   lassen** — bringt es keinen Fehler dazu, passt es. So ist es belegt, bevor
   man etwas Funktionierendes löscht.

## Offene Punkte für den Layout-Durchgang

In der Reihenfolge, in der sie das Layout behindern:

1. **L1 ist 1008 statt 0805** (2,5 × 2,0 mm statt 2,0 × 1,25) und hat als
   drahtgewickelte Spule ein Streufeld — Platzierungsauflage in
   `layout-notes.md`.
2. **Pad-Maße aus dem Gerber prüfen**, nicht aus der Bibliothek — Hausregel, und
   in einem anderen Projekt lag die Bibliothek um bis zu 4,5 mm daneben.
3. **J3 und J6** auf richtige Teile bringen (1,27 mm bzw. Testpunkte).
   **Entschieden: nicht jetzt, aber vor dem ersten Gerber.** Der Schaltplan
   trägt die Verbindungen, nicht die Maße — ein Teiletausch jetzt kostet eine
   Runde und bringt nichts. Auf einem Schlüsselanhänger sind 2,54-mm-Stiftfelder
   aber nicht unterzubringen, also muss es weg, bevor gefertigt wird.
4. **C15/C16 am Netzwerkanalysator abstimmen**, beide Zweige immer gleich.
5. **Die LED-Vorwiderstände am Muster nachstellen** (R10 1 kΩ, R11/R12 470 Ω
   sind vorläufig).
6. **Der Akku darf nicht hinter der NFC-Antenne liegen**, dazu Ferritfolie
   zwischen Akku und Antenne (`layout-notes.md`).

**Erledigt am 24.10.2026:** Die Raytac-Pinzuordnung ist am Datenblatt geprüft
(XL1/XL2 = 17/18, `pinout.md` berichtigt), dass DEC4/DECUSB nicht herausgeführt
sind, ist über die Pinzählung 48 + 5 + 8 = 61 bestätigt, und L1 ist entschieden.

## Die Dateien im Repo: was gesichert ist und was nicht

**Eine Platine, die nur im Editor existiert, ist nicht gesichert.** Darum liegen
neben den Beschreibungen jetzt die Ausgaben aus EasyEDA selbst im Repo. Alle am
**24.10.2026** über die Brücke erzeugt, nicht aus einem Ordner aufgesammelt:

| Datei | Größe | sha256 (Anfang) | Was drin ist |
| --- | --- | --- | --- |
| `schematic-source-P1.txt` | 951 132 B | `6afda8097b4edb46` | Quelltext der Schaltplanseite P1, `doc_export_source` |
| `pcb-source-PCB1.txt` | 27 595 B | `8604aa6593795821` | Quelltext der Leiterplatte (vier Kupferlagen, noch ohne Umriss und Bauteile) |
| `netlist.enet` | 175 557 B | `02f52680d33e89a4` | Netzliste, `sch_netlist_export`, Format EASYEDA_PRO |
| `schematic.pdf` | 364 317 B | `6f25a4713d51f0d5` | der Schaltplan zum Ansehen, `sch_export` |

Herkunft der Seitenquelle, aus ihrem eigenen Kopf gelesen:
`docType: SCH_PAGE`, `uuid: da22d6d3f4f24622`, `updateTime:`
**2026-10-24T17:01:53Z**, `editVersion:` **3.2.209**. Geöffnet wird sie mit
EasyEDA Pro ab dieser Fassung.

### Was der Quelltext trägt — und was nicht

**Er trägt die Seite, nicht die Bauteile.** Nachgezählt im Export:

- **157 `COMPONENT`-Einträge** (54 Bauteile + 102 Netzfahnen + 1 Blattrahmen),
- **219 `WIRE`** und 219 `LINE` — das sind die Stichleitungen,
- **2507 `ATTR`** — Kennungen, Werte, Netznamen,
- **keine einzige `PIN`-Zeile und keine Symbolgeometrie.**

Jedes Bauteil steht nur als Verweis da (`partId: "pid…"`). **Damit ist der
Export kein wiederherstellbares Projekt:** Topologie, Positionen, Netznamen und
Bauteilverweise sind vollständig, die Symbole und Footprints selbst liegen im
Projekt und in den Bibliotheken. Wer die Seite ohne das Projekt öffnet, hat
Verweise auf Teile, die er erst wieder beschaffen muss. Für die hier benutzten
Teile ist das machbar — es sind alles LCSC-Systembibliotheksteile, jedes mit
Herstellernummer in der Tabelle oben —, aber es ist **Arbeit, kein Doppelklick**.

**Vollständig wäre allein die Projektdatei `opensecurekey.eprj2`**, die EasyEDA
beim Speichern selbst schreibt. Die liegt im Arbeitsordner von EasyEDA, nicht
hier; die Brücke kann sie nicht erzeugen (es gibt keine Projektexport-Operation,
nur `doc_export_source` je Dokument). **Sie gehört zusätzlich ins Repo** — den
Griff muss jemand mit Zugang zu diesem Ordner tun.

Was mit den vier Dateien oben allein möglich ist: der Schaltplan ist
**lesbar** (PDF), die Verdrahtung ist **vollständig belegt** (Netzliste, 59
Netze), und die Teile sind über ihre Bestellnummern **eindeutig bestimmt**.
Ein Neuaufbau wäre damit deterministisch — aber von Hand.

**Für die Zukunft:** nach jedem Zeichenabschnitt exportieren und einchecken,
nicht erst am Ende.
