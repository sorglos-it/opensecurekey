# Bestellliste Teststand

**Entwicklungsteile** — Hardware zum Entwickeln und Messen. Keine Serienteile,
keine Stückliste für die eigene Platine.

Ziel: **Thomas soll anfangen können zu messen.** Deshalb nach **Dringlichkeit**
geordnet, nicht nach Bauteilgruppe. Je Zeile steht, **welche offene Frage das
Teil beantwortet** — und wie viel Lötarbeit es macht.

Durchgehend Entwicklungsbretter und Aufsteckplatinen, nie nackte Bauteile: ein
Breakout statt eines QFN20-Chips, ein OLED-Modul mit Stiftleiste statt eines
Glases mit Folienfahne, eine microSD-Adapterplatine statt eines Push-Push-
Schachts.

**Es ist nichts bestellt, nichts in einen Warenkorb gelegt, kein Hersteller
gefragt.** Thomas bestellt und fragt selbst.

## Momentaufnahme — bitte lesen

Alle Preise und Bestände sind **am 26.10.2026** auf der jeweiligen
Händlerseite selbst abgerufen, nicht aus einem Suchindex. **Preise und
Lieferbarkeit ändern sich täglich.** Was hier steht, ist ein Stand mit Datum,
keine Zusage. Was nicht bestätigt werden konnte, steht ausdrücklich als
`ungeprüft` da — nicht als Schätzung.

**Mouser war nicht abrufbar.** Die Seite wehrt automatische Abrufe mit einer
Bot-Sperre ab; die habe ich nicht umgangen. Die Mouser-Zeilen stehen darum mit
dem Stand aus [einkauf.md](einkauf.md) vom **23.10.2026** und sind als
`ungeprüft (23.10.)` gekennzeichnet. Thomas sieht die heutigen Zahlen an der
Kasse.

Diese Datei ist der neue Stand. [einkauf.md](einkauf.md) bleibt als Beleg des
23./24.10.2026 stehen; die Berichtigungen dazu stehen unten in einem eigenen
Abschnitt.

---

## Das eine Teil, das zuerst bestellt werden sollte

**MIKROE-5392 „SE051 Plug & Trust Click"** — Mouser `932-MIKROE-5392`,
**21,50 € netto**, 92 am Lager (`ungeprüft (23.10.)`).

Sie trägt den **Zielchip SE051C2**, nicht einen Verwandten. Damit beantwortet
sie die wichtigste offene Frage des Projekts, und zwar mit **einem Aufruf**.

Die Frage: Die NIST-Sicherheitspolicy eines FIPS-validierten SE051 nennt das
Merkmalbild `0x27F2` — darin sind **EdDSA aus, Curve25519 aus**, und im
FIPS-Modus verweigert der Baustein **ECDH grundsätzlich** (AN12413 4.10.3.1).
Die *Variante* SE051C2 kann beides (AN12973 Rev. 2.0, Tabelle 3). Was das
*gelieferte Stück* darf, wird im Werk gesetzt und ist danach unveränderlich.
Belegt in [hardware.md](hardware.md):318–332 und [se050.md](se050.md):499–548.

Fällt die Antwort schlecht aus, fehlen drei Dinge, die der Entwurf
voraussetzt: **SSH-Schlüssel über Ed25519**, die **Server-Verschlüsselung über
X25519** und der **Schlüsselaustausch im Chip**. Das betrifft die Geldbörse,
die Smartcard-Dienste und den Tresor.

Die Antwort steht schon in der Doku ([hardware.md](hardware.md):334–336):
Chip anschließen, `osk_se050_open()` laufen lassen, **Merkmalbild und freien
Speicher ins Messprotokoll**. Erst danach lohnt jede weitere Messung.

**Sie ist steckbar und braucht keine Lötarbeit** — die mikroBUS-Leisten sind
bestückt (`ungeprüft`, aus dem Produktbild; deshalb ist eine Stiftleiste für
0,60 € in der Liste).

**Und sie ist nur bei einem Händler zu haben.** Am 26.10.2026 selbst
nachgesehen:

| Quelle | Preis | Lager | Stand |
| --- | --- | --- | --- |
| **Mouser** `932-MIKROE-5392` | 21,50 € netto | **92** | `ungeprüft (23.10.)` |
| **DigiKey DE** `1471-MIKROE-5392-ND` | **22,19 € netto** / 26,41 € brutto | **0** — „1 Artikel überfällig; geschätztes Verfügbarkeitsdatum: 16.10.2026" (verstrichen) | abgerufen 26.10. |
| **Farnell DE** `4696282` | **19,47 € netto** | **0** — „Bestellbar. Voraussichtliche Lieferung nach Auftragsbestätigung." | abgerufen 26.10. |
| mikroe.com direkt | 25,00 USD | Hersteller | `ungeprüft` |

Der billigste Preis ist nicht lieferbar, der zweitbillige auch nicht. **Mouser
ist die einzige Quelle mit Bestand** — das ist der Grund, dieses Teil nicht
liegen zu lassen.

Es gibt auch keinen bequemen Ersatz: Ein nackter SE051C2 kostet nur 5,02 €,
ist aber ein **HX2QFN20, 3 × 3 mm, 0,4 mm Raster**. Das lässt sich nicht aufs
Steckbrett löten — dafür bräuchte es eine eigene Adapterplatine, also Wochen
statt Tage.

---

## Block A — beantwortet offene Fragen (jetzt bestellen)

Die vollständige Kette bis zur ersten Messung. Ohne jede einzelne Zeile läuft
nichts.

| Teil | Bestellnummer | Woher | Preis | Lager | Löten? | Beantwortet welche Frage |
| --- | --- | --- | --- | --- | --- | --- |
| **SE051 Plug & Trust Click** | `932-MIKROE-5392` | Mouser | 21,50 € netto `ungeprüft (23.10.)` | 92 | **nein** | **Merkmalbild `0x27F2`: kann der Chip Ed25519, X25519, ECDH?** Dazu: freier Speicher 46 oder 104 kB |
| **nRF52840-DK (PCA10056)** | `949-NRF52840-DK` | Mouser | 41,37 € netto `ungeprüft (23.10.)` | 443 | **nein**, alle Leisten bestückt | I²C-Wirt für den Chip. Ohne ihn ist das Click ein Stück Platine. Dazu: T=1′-Rahmung, SCP03-Sitzung, Dauer einer Signatur, Ruhe- und Betriebsströme, die Nordic in **keiner** Datenblattfassung angibt |
| *Ersatz für das DK* | `2842321` | **Farnell DE** | **40,88 € netto** | **67 auf Lager**, Express am nächsten Werktag, frei ab 50 € netto | nein | dasselbe, innerhalb der EU, ohne Zollrisiko (abgerufen 26.10.) |
| **Micro-USB-Kabel 1,0 m**, datenfähig | `RPI-MUSB-1` | BerryBase | 1,80 € brutto, **2 Stück = 3,60 €** | sofort | nein | **Zwei je DK**, weil es zwei Buchsen hat: `J2` Debugger, `J3` USB-Peripherie. FIDO2 über USB messen *und* gleichzeitig debuggen geht nur mit beiden |
| **Jumperkabel M-F, 20 cm** | `DUPK-40-FM-20` | BerryBase | 1,80 € brutto | **Ab Lager** (26.10.) | nein | verbindet Click und DK |
| **Jumperkabel F-F, 50 cm** | `DUPK-40-FF-50` | BerryBase | 2,20 € brutto | **Ab Lager** (26.10.) | nein | für die Fälle, in denen beide Seiten Stifte haben |
| **Steckbrett, 830 Kontakte** | `BB-830P` | BerryBase | 1,90 € brutto | sofort, 100+ Stück | nein | Aufbau |
| **Stiftleiste 1 × 40, RM 2,54** | `PINH-1X40P` | BerryBase | 0,60 € brutto | Ab Lager | *sie selbst wird gelötet* | Reserve, falls Display oder Click ungelötet kommen. 0,60 € sind billiger als eine zweite Bestellung |

**Block A braucht keinen Lötkolben** — außer die Stiftleiste wird gebraucht.

**Geändert gegenüber [einkauf.md](einkauf.md):** Das dort gelistete Kabelset
`DUPK-SET-20` (4,90 €) ist am 26.10.2026 **nur noch „Vorbestellung"**, ebenso
8 der 11 DUPK-Varianten. Die beiden oben genannten Einzelsorten sind am Lager
und zusammen **4,00 € statt 4,90 €**.

---

## Block B — damit Stufe 3 und 4 laufen

[build.md](build.md):160 und [architecture.md](architecture.md):163 sagen es
selbst: das Projekt steht bei **Stufe 2 von 4 — übersetzt und im Abbild, nie
am Gerät gelaufen.**

| Teil | Bestellnummer | Woher | Preis | Lager | Löten? | Beantwortet welche Frage |
| --- | --- | --- | --- | --- | --- | --- |
| **Logikanalysator 8 Kanal, 24 MHz**, AZDelivery, mit USB-Kabel | ASIN `B01MUFRHQ2` | Amazon.de | **13,99 € brutto** | **Auf Lager**, Lieferung 28./29.10. | nein | **Liegt auf der Leitung zum Sicherheitschip Klartext?** „I2C ist verschlüsselt (SCP03)" in [threat-model.md](threat-model.md) ist bisher eine **Behauptung**. Am Bus mitgelesen wird sie ein Befund — und nur so. Dazu: die Flankenforderung aus Errata I2C.4 (SCL muss vor SDA wechseln) |
| **OLED-Modul 0,96", 128 × 64, SSD1306, SPI, 7-polig**, Hailege | ASIN `B07Z8QYCFV` | Amazon.de | **8,99 € brutto** | **nur noch 18** | **meist nein** — Leiste ist bestückt, bei manchen Chargen liegt sie lose bei (`ungeprüft`) | Anzeigepfad. `osk_ui_port_nrf.c` ist **nie übersetzt** ([display.md](display.md):24) — hier läuft er zum ersten Mal |
| **microSD-Modul, SPI, 3,3-V-Regler**, Soldered, für Karten bis 32 GB | `SOL-333050` | BerryBase | **4,30 € brutto** | **Ab Lager** | **ja**, Stiftleiste anlöten — Raster 2,54 mm, grobe Arbeit | **Neu — es fehlte.** Der SD-Pfad ist ohne Schacht nicht messbar; weder DK noch Click noch Dongle hat einen. Siehe Berichtigung 2 |
| *Billigere Möglichkeit* | `MSD-AADP` | BerryBase | 1,40 € brutto | 92 Stück | ja | dasselbe, aber ohne belegten 3,3-V-Regler im Produkttext |
| **microSD-Karte 32 GB**, SanDisk Ultra microSDHC A1 Class 10 | `619659184155` | BerryBase | **17,90 € brutto** | **Ab Lager** | nein | Karte zum Prüfen. `hardware/datentraeger.md`:21 verlangt **16 GB und mehr**; 32 GB ist zugleich die Obergrenze, die das Modul nennt |

**Wichtig beim Display:** die **SPI**-Ausführung mit **7 Stiften** nehmen,
nicht die I²C-Ausführung mit 4. Die SSD1306-Module bei Reichelt
(`DEBO OLED2/3/4 0.96`) sind alle I²C und taugen nicht.

**Zum Logikanalysator, zwei Hinweise.** Erstens: Verkäufer ist ein
Drittanbieter (`3-2-1-schnaps-deer`), Versand durch Amazon — nicht Amazon
selbst, anders als [einkauf.md](einkauf.md) nahelegt. Zweitens: ein deutscher
Fachhändler ist hier keine Alternative. Am 26.10.2026 nachgesehen — BerryBase
`TOL-18627` (29,90 €) **nicht vorrätig**; Reichelts billigster echter
Logikanalysator kostet **298,90 €** (`UT M26`, 16 Kanal), der Digilent
Digital Discovery 342,95 €. Für I²C bei 400 kBit/s reichen 24 MHz reichlich.
13,99 € gegen 298,90 € ist keine knappe Entscheidung.

---

## Block C — das zweite Gerät

**Zwei Geräte sind kein Luxus, sondern Voraussetzung für eine ganze Stufe.**
Zwei Dinge sind mit einem Gerät grundsätzlich nicht prüfbar:

1. **Stick-zu-Stick-Übertragung und das Spiegeln der Passkeys** — dafür
   braucht es zwei Geräte, nicht zwei Prozesse.
2. **Datenträger-Betriebsarten 2 und 3** — eine Karte wird vom einen Stick
   beschrieben und vom anderen gelesen. Der Rückdreh-Schutz hängt genau daran:
   er „erkennt den Rückdreh nur, wenn der Empfänger die Karte schon gesehen
   hat". **Das zweite Gerät braucht also auch einen eigenen Kartenschacht und
   eine eigene Karte.**

**Ab wann:** Nicht in der ersten Bestellung. Erst wenn an *einem* Gerät steht,
dass der Chip antwortet, SCP03 hält und der SD-Pfad schreibt und liest. Vorher
verdoppelt das zweite Brett nur die Fehlersuche.

### Und dafür ist der Dongle das falsche Teil

[einkauf.md](einkauf.md) führt den nRF52840-Dongle (9,34 € netto) als
Zweitgerät. Für die Betriebsarten 2 und 3 **taugt er nicht.** Nordic schreibt
auf der eigenen Produktseite (abgerufen 26.10.2026) drei Sätze, die das
entscheiden:

- „**15 GPIO accessible from castellated solder points along the edge**" —
  Lötpunkte. Wer das microSD-Modul anschließen will, muss **erst eine
  Stiftleiste an den Platinenrand löten.** Das ist feinere Arbeit als eine
  gerade Leiste an einem Steckbrett-Modul.
- „**there is no debug support on the Dongle**, only support for programming
  the device and communicating through USB." — **kein Debugger.** Geht am
  zweiten Gerät etwas schief, sieht man es nicht.
- „The box includes an nRF52840 Dongle." — sonst nichts, keine Leisten.

| Zweitgerät | Preis | Löten | Debugger | Kartenschacht | Urteil |
| --- | --- | --- | --- | --- | --- |
| **Zweites nRF52840-DK** `949-NRF52840-DK` | 41,37 € netto | **nein** | **ja**, J-Link an Bord | ja, ohne Löten am Brett | **Empfehlung** für die Betriebsarten 2 und 3 |
| nRF52840-Dongle `949-NRF52840-DONGLE` | 9,34 € netto, 1.114 am Lager | **ja**, Leiste an Lötpunkte | **nein** | nur mit dieser Lötarbeit | billiges zweites **FIDO2-Ziel am USB** — mehr nicht |
| *Ersatz Dongle* | Farnell DE `2902521`, 9,86 € netto | — | — | — | **Lager 0**, „Lieferungen beginnen am 20.11.2026" (26.10.) |

**Also:** Wer nur ein zweites FIDO2-Gerät am USB braucht, nimmt den Dongle für
11,11 € brutto — er steckt direkt in den Rechner, ohne Kabel und ohne Löten.
Wer die Betriebsarten 2 und 3 messen will, nimmt ein **zweites DK**.

| Nachschlag für zwei vollwertige Geräte | Preis |
| --- | --- |
| Zweites nRF52840-DK | 41,37 € netto = 49,23 € brutto |
| Zweites microSD-Modul `SOL-333050` | 4,30 € |
| Zweite microSD-Karte 32 GB | 17,90 € |
| Zwei weitere Micro-USB-Kabel `RPI-MUSB-1` | 3,60 € |
| **Zusammen** | **ca. 75 €** (zzgl. Versand, wenn getrennt bestellt) |

---

## Block D — später; beantwortet jetzt keine offene Frage

| Teil | Bestellnummer | Preis | Warum nicht jetzt |
| --- | --- | --- | --- |
| **Fingerabdrucksensor-Board** SparkFun FPC2534 (Qwiic) | `SEN-29854` | 59,95 USD / 72,54 EUR `ungeprüft (24.10.)` | **Bestückungsstufe 2.** Entschieden ist: Freigabeweg ist **Taster + PIN**, Stufe 1 ohne Sensor ([hardware.md](hardware.md):67–71). Tragender Grund: **bei FIDO2 ersetzt die Biometrie die PIN — sie ist kein zusätzlicher Faktor.** Der Sensor macht das Gerät bequemer, nicht sicherer. Dazu: die Stiftleisten liegen **nicht** bei, und IRQ und RST müssen zusätzlich an GPIO — es reicht nicht, SDA und SCL zu verbinden. Siehe Berichtigung 3 |
| **OM-SE052ARD** mit **SE052F** | `OM-SE052ARD` | 69,00 USD | **Nicht bestellbar:** Status „Pending Stock", „No Distributors Available" (nxp.com, 26.10.). Und als Ausweg aus der Kurvenfrage voraussichtlich **untauglich** — der SE052F ist „the first hardware Secure Element certified to FIPS 140-3 Level 3", und genau der FIPS-Weg schaltet EdDSA und Curve25519 ab. Beobachtungsliste, nicht Bestellung |

---

## Summe

Preise so, wie der jeweilige Händler sie ausweist: Mouser und Farnell netto,
BerryBase und Amazon brutto.

### Erste Bestellung — ein Gerät, messbereit

| Händler | Posten | brutto |
| --- | --- | --- |
| **Mouser** | DK + Click (62,87 € netto + 19 % MwSt.) | **74,82 €** |
| **BerryBase** | Steckbrett 1,90 · M-F-Kabel 1,80 · F-F-Kabel 2,20 · 2 × Micro-USB 3,60 · Stiftleiste 0,60 · microSD-Modul 4,30 · Karte 32 GB 17,90 = **32,30 €** Waren + 4,95 € DHL | **37,25 €** |
| **Amazon** | Logikanalysator 13,99 · OLED-Modul 8,99 | **22,98 €** |
| | **Zusammen** | **ca. 135 €** |

zzgl. Mouser-Versand und etwaiger Einfuhrabgaben, beide `ungeprüft`.

### Beide Geräte, in einem Zug

| Händler | brutto |
| --- | --- |
| Mouser: 2 × DK + Click (104,24 € netto) | 124,05 € |
| BerryBase: 58,10 € Waren + 4,95 € DHL | 63,05 € |
| Amazon | 22,98 € |
| **Zusammen** | **ca. 210 €** |

Optional der Dongle als drittes, billiges FIDO2-Ziel: +11,11 € brutto.

### Das Mindeste, nur um die Kurvenfrage zu beantworten

Click + DK + zwei Micro-USB-Kabel + M-F-Kabel + Steckbrett: **ca. 82 € brutto**
zzgl. Versand. Logikanalysator, Display und SD-Teil kann man nachschieben.

### Zwei Zahlen bleiben offen, beide bei Mouser

1. **Versandkosten.** Mouser nennt sie ausschließlich im Warenkorb. Einen
   Warenkorb habe ich nicht angelegt.
2. **Zoll und Einfuhrumsatzsteuer.** Mouser versendet aus Mansfield, Texas,
   weist auf mouser.de aber in EUR aus. Ob die Lieferung verzollt (DDP) kommt,
   ist nicht belegt. **Das ist die größte Unsicherheit dieser Rechnung** —
   [einkauf.md](einkauf.md):113 sagt das schon selbst. Käme Zoll obendrauf,
   wären es bei 63 € Warenwert grob 12 € Einfuhrumsatzsteuer plus
   Abgabengebühr; **gerechnet, nicht belegt.**

**Wer das Zollrisiko kleinhalten will**, bestellt die DKs bei **Farnell DE**
(`2842321`, 40,88 € netto, 67 am Lager, EU, kein Zoll, frei ab 50 € netto —
bei zwei Brettern erreicht) und bei Mouser nur die Click (21,50 € netto). Dann
liegt nur ein Posten von 21,50 € in der US-Lieferung.

---

## Löten und Flashen — was auf Thomas zukommt

An diesem Gerät ist noch nie gelötet oder geflasht worden. Darum ausdrücklich:

**Gelötet werden muss genau eine Sache:** die Stiftleiste am microSD-Modul
(und, falls die Charge sie lose beilegt, am OLED-Modul). Beides ist
**Raster 2,54 mm** — grobe, gut machbare Arbeit, kein feines Werkzeug nötig.
Alles andere in Block A und B ist steckbar.

**Feines Lötwerkzeug braucht nur**, was auf dieser Liste bewusst *nicht* steht:
der nackte SE051C2 (HX2QFN20, 0,4 mm Raster), der Dongle-Platinenrand und das
48 × 64-Glas mit Folienfahne. Deshalb stehen sie nicht drauf.

**Geflasht wird über das DK selbst** — der SEGGER J-Link ist an Bord, mit
Debug-Out-Leiste, um auch ein zweites Ziel zu programmieren. Es braucht
**keinen** zusätzlichen Programmieradapter. Das Abbild baut auf Board
`nrf52840dk/nrf52840` ([build.md](build.md):84); die Skripte in `tools` kennen
den Weg, aber [build.md](build.md):512 sagt dazu: „**Ungeprüft** – es gab beim
Schreiben der Skripte keine Hardware." Genau diese Lücke schließt die
Bestellung.

---

## Bitte nachsehen, ob schon da

Einmal zu viel nachgesehen ist billiger als zweimal bestellt. **Nichts davon
ist blind mitbestellt** — was vorhanden ist, streicht die Zeile.

| Was | Steht in der Liste als | Hinweis |
| --- | --- | --- |
| **Logikanalysator** (8 Kanal USB reicht) | 13,99 € Amazon | liegt in Bastelkisten oft herum |
| **Steckbrett** | 1,90 € BerryBase | — |
| **Jumperkabel** M-F und F-F | 4,00 € BerryBase | — |
| **Micro-USB-Kabel, datenfähig** | 3,60 € (2 Stück) | **nicht** eines aus der Ladeschublade: viele Billigkabel führen nur die zwei Stromadern. Es müssen **zwei je DK** sein |
| **microSD-Karte ≥ 16 GB** | 17,90 € BerryBase | fast sicher vorhanden. Achtung: die Karte wird beim Prüfen **beschrieben und formatiert** — keine mit Daten darauf nehmen |
| **Kartenleser am PC** | nicht in der Liste | zum Gegenlesen dessen, was das Gerät geschrieben hat |
| **Lötkolben und Lötzinn** | nicht in der Liste | nur für die Stiftleiste am microSD-Modul, Raster 2,54 mm |
| **Stiftleisten 2,54 mm** | 0,60 € BerryBase | — |
| **Multimeter** | nicht in der Liste | für die Strommessungen aus [hardware.md](hardware.md):338–351 |

### Schon belegt — das braucht es *nicht*

| Frage | Antwort | Quelle |
| --- | --- | --- |
| Liegt dem DK ein USB-Kabel bei? | **Nein.** Farnell nennt als Lieferumfang „Preview-DK-Board nRF52840, 2032-Batterie, NFC-Antenne" — kein Kabel | Farnell `2842321`, 26.10.2026 |
| Netzteil für das DK? | **Nein**, es läuft über USB | Nordic DK-Doku, „5 V power sources" |
| NFC-Antenne? | **Liegt bei** — zwei Quellen. Für NFC ist also kein Teil zu kaufen | Nordic Kit-Inhalt und Farnell-Lieferumfang |
| Programmieradapter / Debugger? | **Auf dem DK**, SEGGER J-Link an Bord, mit Debug-Out | Farnell-Produkttext, 26.10.2026 |
| Batterie CR2032? | **Liegt bei** laut Farnell. Für den Teststand ohne Bedeutung — das DK läuft am USB | Farnell `2842321` |
| Kabel für den Dongle? | **Keines.** Er ist ein USB-A-Stecker und geht direkt in den Rechner | Nordic |
| Akku, Laderegler, Schutzschaltung? | **Nein.** Gestrichen — Thomas, 25.10.2026: „Keine Zelle, kein Laderegler, keine Schutzschaltung, keine Lastumschaltung. Das Gerät läuft ausschließlich aus USB." Für den Teststand ohnehin nie nötig | `hardware/layout-notes.md`:3157 |

---

## Kommt erst mit der eigenen Platine — nicht auf dieser Liste

Serienteile gehören nicht hierher. In einer Zeile, damit nichts verloren geht:
**Randstecker GCT `USB4155-03-C` (LCSC `C5183853`), microSD-Schacht als
Lötteil `DM3AT-SF-PEJM5`, QSPI-Flash `W25Q512JVEIQ`, Funkmodul Raytac
`MDBT50Q-1MV2`, das 0,71-Zoll-Glas `CFAL4864A-071BW` mit FPC-Verbinder
`FH12-12S-0.5SH(55)`, Taster, LED, Einzelwiderstände und -kondensatoren,
Vergussmasse.** Mit dem gestrichenen Akku fallen dort 14 Bauteile ersatzlos
weg (U4 MCP73832, J2, Q1–Q3, R1–R5, R13, R14, C17, C2).

Zwei Dinge daraus sind trotzdem Befunde, die festgehalten sein müssen, weil
sie Fallen sind.

### `C9900…`-Nummern sind Platzhalter, keine Bauteile

Acht davon stecken in den Unterlagen: `C9900141337` (J2, Akkustecker — mit dem
Akku ohnehin hinfällig), `C9900036520` (J4, Anzeige — bereits ersetzt durch
`C88360`), `C9900282109`, `C9900245240`, `C9900001620`, `C9900262527`,
`C9900262530`, `C9900062937` (alle USB-C, verworfene Alternativen).

Sie haben **keinen Hersteller, keine Herstellernummer, kein Datenblatt**; die
LCSC-Produktseite zu `C9900141337` antwortet mit **„Part not found"**
(`hardware/layout-notes.md`:1419–1423). **Auf dieser Liste steht keine einzige
davon** — nicht weil ich sie entfernt hätte, sondern weil keine ein
Entwicklungsteil ist: alle acht liegen in `hardware/`.

Offen bleibt: `hardware/schematic-notes.md`:27 führt J4 noch als
`C9900036520`, und `hardware/netlist.enet`:540 noch die alte Buchse
`USB4085-GF-A`, obwohl `hardware/schematic.md` beide längst getauscht hat.

### Gigabit gegen Gigabyte — und ein erledigter Entwurf

Bei Speicherbauteilen schreiben Hersteller „64G" und meinen **Gigabit**. Jede
Zahl im Entwurf geprüft:

| Bauteil | Angabe | Einheit aufgelöst | Quelle |
| --- | --- | --- | --- |
| **W25Q512JVEIQ** (QSPI-Flash) | „512 Mbit = 64 MByte" | **512 Mbit = 64 MByte** — stimmt, 512 ÷ 8 = 64 | [parts.md](parts.md):16 nennt **beide** Einheiten. Hier beißt die Falle nicht |
| **Zetta `ZDSD64GLGEAG-R`** (SD-NAND) | „64G" | **64 Gbit = 8 GByte** | Zettas Bestellschlüssel: „64G = 64Gbit (8GByte)" |
| **XTX `XTSD08GLGEAG`** | „08G" | **8 Gbit = 1 GByte** | XTX-Datenblatt Rev. 3.1: „08G … 8Gbit (1GByte)" |
| **MK `MKDV4GCL-ABB`** | „4Gbit" | **4 Gbit ≈ 482 MByte** nutzbar | MK-Datenblatt zur `-ABG`: „SLC 4Gbit 482MByte" |

**Das erledigt eine Entwurfsvariante.** `hardware/datentraeger.md`:21 verlangt
**16 GB und mehr**. Die drei SD-NAND-Teile liefern 8 GB, 1 GB und 0,5 GB — und
Zetta deckelt die Reihe bei 64 Gbit, also 8 GByte. **Kein gelötetes SD-NAND
der genannten Reihen erreicht die Anforderung.** Der Kartenschacht ist damit
nicht die bequemere, sondern die **einzige** Variante, die die eigene Vorgabe
erfüllt.

Zwei Nebenbefunde, falls Variante A je wiederkommt: `datentraeger.md`:163
nennt LCSC `C915803` für `MKDV4GCL-ABB`, doch `C915803` ist `MKDV4GCL-AB` —
die `-ABB` ist `C7500178`. Und `datentraeger.md`:31–37 führt LGA8
**8,00 × 6,20 mm**, während Zettas **eigenes** Datenblatt für den
`ZDSD64GLGEAG` **LGA8 11,5 × 13 mm** nennt.

---

## Berichtigungen gegenüber `einkauf.md`

### 1. Ein SE051-Arduino-Kit von NXP gibt es doch

[einkauf.md](einkauf.md):257–258 schreibt: „Es gibt **kein SE051-Arduino-Kit
von NXP** bei Mouser oder DigiKey — nur die Mikroe-Click."

Für Mouser und DigiKey stimmt das. Als allgemeiner Satz ist es falsch:
**`OM-SE051ARD` existiert** und ist laut NXP „configured with a
**SE051C2HQ1/Z01XD** type" — also genau die Zielausführung. 69,00 USD bei NXP
direkt, dort am 26.10.2026 „In Stock", 12 Stück.

**An der Bestellung ändert das nichts, und zwar aus einem belegten Grund.**
Am 26.10.2026 selbst nachgesehen:

| Quelle | Ergebnis |
| --- | --- |
| nxp.com direkt | 69,00 USD, „In Stock", 12 Stück |
| **de.farnell.com** Bestellnummer `3923714` | **404, Seite nicht gefunden.** Die Suche nach `om-se051ard` ergibt: „Wir haben keine Produkte gefunden" |
| be.farnell.com | „No Longer Stocked", „No Longer Manufactured" |
| Mouser DE, DigiKey DE | nicht geführt |

In der EU ist es **nicht zu bekommen**, und die Click kostet ein Drittel und
ist am Lager. **Die Click bleibt erste Wahl.** Zu berichtigen ist nur der Satz:
er muss auf „bei Mouser und DigiKey nicht geführt, in der EU nicht lieferbar"
eingeengt werden.

### 2. „SD-Karte? Nicht nötig." ist überholt

[einkauf.md](einkauf.md):376 sagt: „**Nicht nötig.** Weder DK noch Click noch
Dongle hat einen Kartenschacht."

Der zweite Halbsatz ist weiter richtig — **und genau darum fehlt dem Teststand
etwas.** Seit der microSD-Schacht dazugekommen ist, ist der SD-Pfad ein Teil
der Firmware, der gemessen werden muss. Ohne Schacht am Teststand ist er nicht
messbar. Deshalb neu in Block B: microSD-Adapterplatine (4,30 €) und eine
Karte (17,90 €) — und in Block C je ein zweites Stück davon.

### 3. Das Fingerabdruck-Board kommt wieder heraus

Der Nachtrag [einkauf.md](einkauf.md):19–22 vom 24.10.2026 hat die SparkFun
`SEN-29854` in die Bestellung geholt und die Summe damit von 125 € auf
180–195 € gehoben. Mit der Verschiebung auf **Bestückungsstufe 2** gehört sie
heraus: **rund 60–73 € gespart.** Sie steht weiter in Block D, mit Begründung
— gestrichen ist sie nicht, nur vertagt.

### 4. Der Dongle ist kein vollwertiges Zweitgerät

Siehe Block C: keine Debug-Unterstützung, Lötarbeit am Platinenrand für jeden
GPIO. Für die Datenträger-Betriebsarten 2 und 3 ist ein **zweites DK** die
richtige Wahl.

---

## Was die Doku als Stufenfolge nicht hat

Eine Reihenfolge „Stufe 1 bis 6" **gibt es im Repo nicht** — über alle
30 Zweige gesucht, kein Treffer. Es gibt zwei getrennte Begriffe, die man
nicht verwechseln darf:

- **Bestückungsstufe 1 / 2** — Gerät ohne bzw. mit Fingerabdrucksensor.
- **„Stufe x von 4"** (Hausregel) — geschrieben / übersetzt / im Abbild /
  **am Gerät gelaufen**. Das Projekt steht bei 2.

Diese Liste erfindet darum keine Stufen, sondern ordnet nach dem, was jedes
Teil beantwortet. Ebenfalls nachgesehen, weil es die Brettwahl entscheidet:
**nRF54 und nRF54LM20 kommen im ganzen Repo nicht vor.** Der nRF52840 steht,
das nRF52840-DK ist das richtige Brett.

---

## Quellen

Händlerseiten **am 26.10.2026** selbst abgerufen, außer wo anders angegeben.

**Hersteller**

- Nordic, [nRF52840 Dongle](https://www.nordicsemi.com/Products/Development-hardware/nRF52840-Dongle) —
  „15 GPIO accessible from castellated solder points along the edge",
  „there is no debug support on the Dongle", „The box includes an nRF52840
  Dongle"
- Nordic, nRF52840 DK — Kit content, „5 V power sources"
- NXP, [OM-SE051ARD](https://www.nxp.com/design/design-center/development-boards-and-designs/OM-SE051ARD) —
  „configured with a SE051C2HQ1/Z01XD type", 69,00 USD, In Stock 12
- NXP, [OM-SE052ARD](https://www.nxp.com/design/design-center/development-boards-and-designs/OM-SE052ARD) —
  SE052F, „the first hardware Secure Element certified to FIPS 140-3 Level 3",
  „Pending Stock", „No Distributors Available"
- AN12973 „SE051 configurations", Rev. 2.0, 08.07.2024 — Tabellen 3, 4, 6
  (über den SE050-Treiber-Strang; nxp.com lieferte 404)
- AN12413, Abschn. 4.10.3.1 (ECDH im FIPS-Modus), Tabelle 5 (Merkmalbild)
- Zetta, SD-NAND-Datenblatt `ZDSD08G/16G/32G/64G` — „64G = 64Gbit (8GByte)"
- XTX, `XTSD01G/02G/04G/08G` Rev. 3.1, 15.05.2019 — „08G … 8Gbit (1GByte)"
- MK, Datenblatt zu `MKDV4GCL` — „SLC 4Gbit 482MByte"

**Händler**

- Farnell DE: [`2842321` nRF52840-DK](https://de.farnell.com/nordic-semiconductor/nrf52840-dk/dev-kit-bluetooth-low-energy-soc/dp/2842321)
  (40,88 € netto, 67 auf Lager, frei ab 50 €) ·
  [`4696282` MIKROE-5392](https://de.farnell.com/mikroelektronika/mikroe-5392/development-boardseval-kits/dp/4696282)
  (19,47 € netto, kein Lager) · `2902521` nRF52840-Dongle (9,86 € netto,
  „Lieferungen beginnen am 20.11.2026") · `3923714` OM-SE051ARD (**404**)
- DigiKey DE: [`1471-MIKROE-5392-ND`](https://www.digikey.de/de/products/detail/mikroelektronika/MIKROE-5392/16910358)
  (22,19 € netto, Lager 0)
- BerryBase: [Versand und Zahlungsbedingungen](https://www.berrybase.de/versand-und-zahlungsbedingungen/)
  (DHL 4,95 €, frei ab 200 €) · `BB-830P` · `DUPK-40-FM-20` ·
  `DUPK-40-FF-50` · `DUPK-SET-20` (Vorbestellung) · `RPI-MUSB-1` ·
  `PINH-1X40P` · `SOL-333050` · `MSD-AADP` · `619659184155` ·
  `TOL-18627` (nicht vorrätig)
- Amazon.de: ASIN `B01MUFRHQ2` (13,99 €, auf Lager) · `B07Z8QYCFV`
  (8,99 €, nur noch 18)
- Reichelt: `UT M26` (298,90 €) · `DIGIL 410-338` (342,95 €) ·
  `JOY-IT SCOPE M50` (128,30 €) — zum Vergleich beim Logikanalysator
- Mouser DE: **nicht abrufbar**, Bot-Sperre. Zahlen aus
  [einkauf.md](einkauf.md) vom 23.10.2026

**Im Repo**

- [einkauf.md](einkauf.md) — Vorstand dieser Liste, 23./24.10.2026
- [hardware.md](hardware.md):311–351 — die beiden Werksfragen und die Messliste
- [se050.md](se050.md):130–192, 499–548 — Messplan und Merkmalbild
- [parts.md](parts.md):16, 69–117 — Teileauswahl, Speicher, Ströme
- [build.md](build.md):84, 160, 512 — Board-Name, Stufe 2 von 4, Flashen ungeprüft
- [architecture.md](architecture.md):163 — Stufe 2 von 4
- [fingerprint.md](fingerprint.md):5, 624–631 — Stufe 2 und der J5-Fehler
- [display.md](display.md):24 — `osk_ui_port_nrf.c` nie übersetzt
- [threat-model.md](threat-model.md) — die SCP03-Behauptung, die zu messen ist
- `hardware/datentraeger.md` (Zweig `sd-datentraeger`) — Schacht, Karte,
  Betriebsarten
- `hardware/layout-notes.md`:1419–1423, 3157 — Platzhalter und Akku-Entscheidung

---

## Nachtrag 26.10.2026 — der Flash auf dem Brett ist kleiner als der Entwurf

Nach dem Bestellen geprüft, von der Mastersitzung, gegen alle Entscheidungen
der letzten zwei Tage. **Die Liste war aktuell** — Akku-Wegfall, Datenträger
und die drei Betriebsarten waren berücksichtigt. Auch die später entschiedenen
FIPS-140-3-Anforderungen verlangen hier nichts Zusätzliches: Die PIN-Eingabe am
Gerät braucht Taster und Anzeige, beide sind da; Verguss und der Verzicht auf
einen Debug-Anschluss betreffen die Serienfertigung, nicht den Teststand.

**Ein Punkt blieb ungeprüft, und er gehört festgehalten:**

Das nRF52840-DK trägt einen **MX25R6435F** als externen QSPI-Flash
(Nordic-SDK-Dokumentation zu QSPI/XiP nennt ihn für PCA10056; die Kapazität
von **64 Mbit = 8 MByte** folgt aus der Typbezeichnung, `ungeprueft` — nicht
aus einem Datenblatt nachgelesen).

Der Entwurf sieht **64 MByte** vor (`W25Q512JVEIQ`), also **achtmal so viel**.

| Was damit prüfbar ist | Urteil |
| --- | --- |
| Tresor, Geräteprotokoll, Formatwanderung | **geht** — der Tresor braucht keine 64 MB |
| MCUboot-Zweitplatz im externen Flash | **geht voraussichtlich**, Partitionsgrößen prüfen |
| **USB-Laufwerk mit den portablen Programmen** | **geht nicht vollständig.** Das Abbild ist **weit größer als 8 MB** — die gemessene Größe steht an genau einer Stelle, `laufwerk.md`. In 8 MB passt es nicht |

**Kein Nachkauf jetzt.** Das Laufwerk ist der letzte Prüfpunkt der Reihe; davor
stehen Sicherheitschip, FIDO2, NFC, Smartcard und Tresor, und für alle reichen
8 MB. Wer es später braucht, nimmt ein QSPI-Flash-Modul mit `W25Q`-Baustein —
beim Kauf auf die Einheit achten, die Hersteller schreiben Megabit.

**Und eine Einschränkung, die bleibt:** Was in 8 MB passt, sagt nichts darüber,
wie sich dieselbe Firmware mit 64 MB verhält — Adressbreite, Löschblockgröße
und Verschleißverteilung ändern sich mit der Größe. Ein Lauf am kleinen Flash
ist kein Beleg für den großen.
