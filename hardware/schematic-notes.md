# Was beim Zeichnen des Schaltplans herauskam

Stand 24.10.2026. Gezeichnet in EasyEDA Pro 3.2.209 über die Sorglos-EDA-Brücke,
Projekt `opensecurekey`, Board1, Schaltplanseite P1 (A4).

**Stand: gezeichnet, DRC bis auf einen Punkt sauber, Netzliste abgeglichen.
Nichts gefertigt, nichts gemessen.** Das Layout kommt in einem späteren Durchgang.

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
| SW1, SW2 | Panasonic EVQP7A01P | C79167 |
| D1 | Everlight 19-337C/RSBHGHC-A88/4T | C3019536 |
| D2 | Nexperia PESD5V0X1BCL,315 | C552589 |
| Q1–Q5 | Diodes DMG2305UX, SOT-23 | C2940629 |
| X1 | Epson Q13FC13500004, 32,768 kHz | C32346 |
| L1 | Murata LQH2MCN100M52L, 10 µH, 2,0 × 1,6 mm | C703109 |
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

**Gewählt: Murata LQH2MCN100M52L**, 10 µH ±20 %, drahtgewickelt,
2,0 × 1,6 mm, **Nennstrom 200 mA**, Gleichstromwiderstand 2,96 Ω (C703109).

Begründung, und sie ist eine Korrektur meiner ersten Wahl: Beim Schaltregler
muss der **Sättigungsstrom** über dem **Spitzenstrom** liegen, nicht über dem
Dauerstrom. Mein erstes Teil (TDK MLZ1608N100LT000, 0603) hat 300 mA
Dauerstrom, geht aber schon bei **60 mA in die Sättigung** — dann bricht die
Induktivität ein, die Welligkeit steigt und der Wirkungsgrad fällt, und zwar
genau bei den Funkspitzen, wo es darauf ankommt. Die 300 mA helfen dort nichts.

Murata bemisst den Nennstrom als den kleineren von zwei Werten:
Temperaturanstieg **und** Induktivitätsabfall (|ΔL| ≤ 30 %). Die 200 mA decken
damit die Sättigung mit ab und liegen deutlich über der Vorgabe von ≥ 150 mA.

> **`ungeprueft`:** Ein eigener Zahlenwert für `Isat` steht beim Teil nicht
> dabei; belegt ist der Nennstrom von 200 mA. Wer mehr Reserve und weniger
> Widerstand will, nimmt **TDK NLCV25T-100K-EFRD** (C295388): 1008-Gehäuse
> (2,5 × 2,0 mm), **360 mA** Nennstrom, nur 1,1 Ω, AEC-Q200 — kostet
> 0,5 × 0,4 mm mehr Fläche.

**Abweichung von der Vorgabe:** Gewünscht war 0805 (2,0 × 1,25 mm). Kein
0805-Teil der Bibliothek weist einen Sättigungs- oder Nennstrom ≥ 150 mA aus;
das beste ist MLZ2012N100LT000 mit 110 mA. Das gewählte Teil ist **0,35 mm
breiter** als 0805. Fürs Layout zu beachten, aber die 150-mA-Vorgabe wiegt
schwerer als 0,35 mm.

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
| SW1/SW2 | beide Kontaktpaare verdrahtet | Das EVQP7A01P-Symbol hat vier Pole (1, 1′, 2, 2′) — zwei parallele Kontakte. Beide Seiten liegen auf demselben Netz, so wie das Teil gebaut ist. |
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
| Warnung | `FP_IRQ` ist ein Netz mit einem Pin — gewollt, Testpunkt (siehe oben) |
| **Fehler** | **1 Fehler, von der Schnittstelle nicht benannt** |

**Der eine Fehler ist offen.** EasyEDA gibt über die Schnittstelle nur Zähler
heraus, die Texte stehen allein in seinem DRC-Bereich; der ist geöffnet. Die
eigenen Prüfungen der Brücke (freie Pins, zwei Namen an einem Draht, Fahne gegen
Netzname, zwei Fahnen an einem Draht, Draht ohne Pin, doppelte Kennungen) finden
**nichts** — der Fehler kommt also aus einer Regel, die die Brücke nicht
nachbildet. Ausgeschlossen ist: doppelte oder fehlende Kennungen (54 Bauteile,
alle eindeutig, alle mit Footprint) und ein Überlauf über den Blattrand
(Verschieben der rechten Spalte ändert den Zähler nicht).

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

## Offene Punkte für den Layout-Durchgang

In der Reihenfolge, in der sie das Layout behindern:

1. **Der eine DRC-Fehler** (siehe oben) — am offenen DRC-Bereich ablesen.
2. **L1 braucht 0,35 mm mehr Breite als 0805** (2,0 × 1,6 mm statt 2,0 × 1,25).
   Alternative mit mehr Reserve: NLCV25T-100K-EFRD im 1008-Gehäuse.
3. **Pad-Maße aus dem Gerber prüfen**, nicht aus der Bibliothek — Hausregel, und
   in einem anderen Projekt lag die Bibliothek um bis zu 4,5 mm daneben.
4. **J3 und J6** auf richtige Teile bringen (1,27 mm bzw. Testpunkte).
5. **C15/C16 am Netzwerkanalysator abstimmen**, beide Zweige immer gleich.
6. **Die LED-Vorwiderstände am Muster nachstellen** (R10 1 kΩ, R11/R12 470 Ω
   sind vorläufig).
7. **Der Akku darf nicht hinter der NFC-Antenne liegen**, dazu Ferritfolie
   zwischen Akku und Antenne (`layout-notes.md`).

**Erledigt am 24.10.2026:** Die Raytac-Pinzuordnung ist am Datenblatt geprüft
(XL1/XL2 = 17/18, `pinout.md` berichtigt), dass DEC4/DECUSB nicht herausgeführt
sind, ist über die Pinzählung 48 + 5 + 8 = 61 bestätigt, und L1 ist entschieden.
