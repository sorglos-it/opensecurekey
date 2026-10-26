# Datenträger auf der Platine

Stand 26.10.2026. **Nur Papier. Die Zeichnung ist nicht angefasst.** Alle Maße
stammen aus Herstellerdatenblättern, die gelesen wurden (Quellen unten), nicht
aus Schätzungen. Gerechnet gegen den Stand `akku-weg` (59,55 mm).

## Ergebnis

- **Beide Varianten passen. Keine verlängert die Platine: ±0,00 mm, es bleibt
  bei 59,55 mm.** Länge = 45,05 mm + NFC-Zone, unverändert.
- Beide gehören auf die **Unterseite, `y` 25 … 45**. Oben liegt die Anzeige
  (`y` 23,05 … 45,05). Dorthin ziehen ohnehin die sechs Teile Q4, Q5, R9, R17,
  C13, C14 (siehe `layout-notes.md`, Ende).
- **Empfehlung: Variante A, SD-NAND, 64 Gb = 8 GB.** 49,6 mm² statt 220,9 mm²,
  0,80 mm Bauhöhe statt 1,68 mm, kein Schlitz im Gehäuse.
- **Vorbehalt:** Dass das 64-Gb-Teil im Gehäuse 8 × 6,2 mm heute lieferbar ist,
  ist nicht belegt (siehe unten). Sicher belegt sind 4 GB (32 Gb) und 1 GB
  (8 Gb).

## Gigabit ist nicht Gigabyte

Die Hersteller schreiben **Gigabit**. Jede Zahl hier ist umgerechnet
(8 Gb = 1 GB).

| Teil | Gehäuse | **Gb** | **GB** | Quelle |
| --- | --- | ---: | ---: | --- |
| MK MKDV4GCL-ABB (SLC) | LGA-8, 6 × 8 | 4 | **0,481** | MK-Datenblatt |
| XTX XTSD08GLGEAG | LGA8, 8 × 6 | 8 | **1** | XTX v2.6 |
| Zetta ZDSD16GLGEAG | LGA8, 8 × 6,2 | 16 | **2** | Zetta |
| Zetta ZDSD32GLGEAG (MLC) | LGA8, 8 × 6,2 | 32 | **4** | Zetta |
| Zetta ZDSD64GLGEAG-R (MLC) | LGA8, 8 × 6,2 | 64 | **8** | Zetta |

- Die großen MLC-Teile sind **6,20 mm** breit, nicht 6,00.
- Mehr als 64 Gb = 8 GB führt kein gefundenes Blatt für SD NAND.
- **Offen:** Das Zetta-Blatt mit dem 8 × 6,2-Gehäuse für 32/64 Gb stammt aus
  Dezember 2023. Das neuere Blatt führt 8 … 64 Gb nur im Gehäuse **9 × 10 mm**
  (LQ). Ob das 64-Gb-Teil in 8 × 6,2 heute zu haben ist, klärt nur der
  Hersteller. Rückfall 9 × 10 mm = 90 mm², passt auf denselben Platz; seine
  Höhe ist nicht gemessen.
- Keine LCSC-Nummer geprüft; keine `C9900`-Nummer verwendet.

## Variante A: SD-NAND (Zetta-Blatt, Seiten 4 und 16)

| | |
| --- | --- |
| Körper | **8,00 × 6,20 mm** (7,95 … 8,05 × 6,10 … 6,30) |
| Höhe | **0,80 mm** (0,75 … 0,85) |
| Lötbild | 2 × 4 Pads an den 6,2-mm-Seiten, Raster **1,27 mm**, Pad **0,60 breit × 0,80 lang** |
| SPI | CS, DI, GND, VDD, SCLK, DO; zwei Pins ohne Funktion |
| Strom | Schreiben max. **35 mA** bei 3,3 V (Zetta), 21 mA (MK) |
| Beschaltung | 2,2 µF an VDD (MK-Blatt); Pull-ups noch nicht gelesen |
| Platz mit Kondensator und Rand | etwa 9 × 8 mm |

- 0,8-mm-Platine: unkritisch, reines SMD-Teil, keine Stifte, keine Hebelkraft.
- Lage: Unterseite, z. B. `x` 1 … 9, `y` 27,5 … 33,7 (hinter dem Körper von SW1
  bei etwa 26,5, vor dem Glasende bei 45,05).

## Variante B: microSD-Schacht (Hirose-Katalog DM3, Seiten 3, 7, 9)

| Typ | Körper | Höhe | Karte ragt |
| --- | --- | --- | --- |
| **DM3AT-SF-PEJM5**, Push-Push | **13,85 × 15,95 mm** | **1,68 mm** | eingerastet **1,6 mm**, ausgeworfen **5,6 mm** |
| DM3CS-SF, Klappe | 13,8 × 14,1 mm | 1,83 mm | Deckel muss zugänglich sein: im geschlossenen Gehäuse **ungeeignet** |
| DM3D-SF, Push-Pull | 11,95 × 11,45 mm | etwa 1,55 mm, **abgelesen, unsicher** | etwa 3,5 mm greifbar |

- Lötbild DM3AT: 14,1 … 15,1 tief, etwa 14,05 breit, nur SMD.
- Die 15 × 15 mm der ersten Schätzung sind zu knapp: **gemessen 13,85 × 15,95**.
- Lage: quer gedreht, Einschub über die **linke lange Kante**, weg von der
  Funkantenne. `x` 0,2 … 16,15, `y` 27,0 … 40,85. Passt in 25 … 45.
  Daneben, `x` 17 … 27, bleibt Platz für die sechs Teile.
- Das Gehäuse braucht in der Kante einen Schlitz von mindestens 12 × 1,8 mm
  und 5,6 mm Auswurfweg. Er liegt **0,5 mm über dem seitlichen Taster SW1**
  (`x` 2,75 … 4,25, `y` bis 26,5). Gehäuse-Strang prüft das.
- Bauhöhe an der Stelle: 0,8 + 1,68 + 1,26 (Glas oben) = **3,74 mm**.
- Nicht belegt: Verhalten der 0,8-mm-Platine unter der Steckkraft; Abstand des
  Stahlgehäuses zur Funkantenne (etwa 9 mm, `ungeprueft`).
- Die Karte verlässt das Gerät: der Inhalt muss verschlüsselt sein.

## Anschlüsse (gegen `netlist.enet` gemessen)

SPI braucht drei schnelle Leitungen und eine langsame. Am Modul U1 sind frei:

| Funktion | Pin | Modulpin | Bemerkung |
| --- | --- | --- | --- |
| SD_SCK | `P1.08` | 25 | schnell erlaubt (Raytac nimmt P1.08/P1.09 aus der Sperre) |
| SD_MOSI | `P1.09` | 26 | dito |
| SD_MISO | `P0.12` | 29 | Trace-Pin, nicht gebraucht |
| SD_CS | `P0.07` | 23 | Trace-Pin, nicht gebraucht |
| SD_PWR, optional | `P0.04` | 20 | durch Akku-Räumung frei |
| SD_DETECT, nur B | `P1.02` | 50 | durch Akku-Räumung frei |

- Es sind **genau vier freie schnelle Pins**: `P0.07`, `P0.12`, `P1.08`,
  `P1.09`, also genau eine SPI-Schnittstelle. Danach bleibt kein schneller Pin
  übrig.
- Anzeige (`P0.27`, `P0.20`, `P1.11` … `P1.14`) und Fingerabdruck J5
  (`P0.13` … `P0.16`, `P1.03`, `P1.15`) berühren keinen davon: **es geht
  zusammen auf.**
- Der nRF52840 hat kein SDIO. Nur SPI-Modus: etwa 25 MHz, grob 2 … 3 MB/s
  Obergrenze. USB Full Speed (0,4 … 0,5 MB/s) bleibt die Bremse.
- Ungeprüft: ob die Leiterbahnen von den Modulpins 23, 25, 26, 29 auf die
  Unterseite durchkommen.
- Die `netlist.enet` im Zweig führt noch `VBAT_SENSE` und `CHG_STAT` auf
  `P0.04` und `P1.02`; gemeint sind sie laut `layout-notes.md` als frei.

## Nicht gemacht

- Zeichnung nicht angefasst, nichts bestellt, kein Hersteller gefragt.
- Kollision mit der FPC-Fahne der Anzeige (J4 „hinter dem Glas") nicht
  gerechnet.
- Stufe 2 (Fingerabdrucksensor, +11 mm in der Länge) liegt oben, berührt die
  Unterseite nicht.

## Quellen

- Zetta ZDSD08G/16G/32G/64G, LGA8 8 × 6,2 (Datenblatt vom Dezember 2023):
  <http://www.zettadevice.com/uploads/files/64%20%E5%A4%A7SD%20NAND/8~64Gb%20%20SD%2012.12/1702351868c8dadbb332ddbeb7.pdf>
- XTX XTSD01G … 08G v2.6 (Adafruit-Ablage):
  <https://cdn-shop.adafruit.com/product-files/4899/2005251034_XTX-XTSD04GLGEAG_C558839(2).pdf>
- MK SD NAND (LCSC C915803):
  <https://datasheet.lcsc.com/datasheet/pdf/390c222bcea642d7728282ca392972ba.pdf?productCode=C915803>
- Hirose DM3, microSD Card Connectors:
  <https://mm.digikey.com/Volume0/opasdata/d220001/medias/docus/9054/DM3.pdf>
