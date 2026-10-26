# Bestellliste Teststand

Was Thomas bestellen muss, damit am echten Gerät gemessen werden kann.
Nach **Dringlichkeit** geordnet, nicht nach Bauteilgruppe: oben steht, was eine
offene Frage beantwortet, unten, was keine beantwortet.

**Es ist nichts bestellt, nichts in einen Warenkorb gelegt, kein Hersteller
gefragt.** Thomas bestellt und fragt selbst.

## Momentaufnahme — bitte lesen

Alle Preise und Bestände sind **am 26.10.2026** auf der jeweiligen
Händlerseite selbst abgerufen, nicht aus einem Suchindex. **Preise und
Lieferbarkeit ändern sich täglich.** Was hier steht, ist ein Stand mit Datum,
keine Zusage. Was nicht bestätigt werden konnte, steht ausdrücklich als
`ungeprüft` da — nicht als Schätzung.

**Mouser war nicht abrufbar.** Die Seite wehrt automatische Abrufe mit einer
Bot-Sperre ab; die habe ich nicht umgangen. Die drei Mouser-Zeilen stehen
darum mit dem Stand aus [einkauf.md](einkauf.md) vom **23.10.2026** und sind
als `ungeprüft (23.10.)` gekennzeichnet. Thomas sieht die heutigen Zahlen an
der Kasse.

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
Belegt in [hardware.md](hardware.md) Zeilen 318–332 und
[se050.md](se050.md) Zeilen 499–548.

Fällt die Antwort schlecht aus, fehlen drei Dinge, die der Entwurf
voraussetzt: **SSH-Schlüssel über Ed25519**, die **Server-zu-Server-
Verschlüsselung über X25519** und der **Schlüsselaustausch im Chip**. Das
betrifft die Geldbörse, die Smartcard-Dienste und den Tresor.

Die Antwort steht schon in der Doku ([hardware.md](hardware.md):334–336):
Chip anschließen, `osk_se050_open()` laufen lassen, **Merkmalbild und freien
Speicher ins Messprotokoll**. Erst danach lohnt jede weitere Messung.

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

---

## Block A — beantwortet offene Fragen (jetzt bestellen)

Das ist die vollständige Kette bis zur ersten Messung. Ohne jede einzelne
Zeile läuft nichts.

| Teil | Bestellnummer | Woher | Preis | Lager | Beantwortet welche Frage |
| --- | --- | --- | --- | --- | --- |
| **SE051 Plug & Trust Click** | `932-MIKROE-5392` | Mouser | 21,50 € netto `ungeprüft (23.10.)` | 92 | **Merkmalbild `0x27F2`: kann der Chip Ed25519, X25519, ECDH?** Dazu: freier Speicher 46 oder 104 kB |
| **nRF52840-DK (PCA10056)** | `949-NRF52840-DK` | Mouser | 41,37 € netto `ungeprüft (23.10.)` | 443 | I²C-Wirt für den Chip. Ohne ihn ist das Click ein Stück Platine. Dazu: T=1′-Rahmung, SCP03-Sitzung, Antwortzeit einer Signatur, Ruhe- und Betriebsströme, die Nordic in **keiner** Datenblattfassung angibt |
| *Ersatz für das DK* | `2842321` | **Farnell DE** | **40,88 € netto** | **67 auf Lager**, Express am nächsten Werktag, Versand frei ab 50 € netto | dasselbe, innerhalb der EU, ohne Zollrisiko (abgerufen 26.10.) |
| **Micro-USB-Kabel 1,0 m**, datenfähig | `RPI-MUSB-1` | BerryBase | 1,80 € brutto, **2 Stück = 3,60 €** | sofort | **Zwei**, weil das DK zwei Buchsen hat: `J2` Debugger, `J3` USB-Peripherie. FIDO2 über USB messen *und* gleichzeitig debuggen geht nur mit beiden |
| **Jumperkabel M-F, 20 cm** | `DUPK-40-FM-20` | BerryBase | 1,80 € brutto | **Ab Lager** (26.10.) | verbindet Click und DK |
| **Jumperkabel F-F, 50 cm** | `DUPK-40-FF-50` | BerryBase | 2,20 € brutto | **Ab Lager** (26.10.) | für die Fälle, in denen beide Seiten Stifte haben |
| **Steckbrett, 830 Kontakte** | `BB-830P` | BerryBase | 1,90 € brutto | sofort, 100+ Stück | Aufbau |
| **Stiftleiste 1 × 40, RM 2,54** | `PINH-1X40P` | BerryBase | 0,60 € brutto | Ab Lager | falls Display oder Click ungelötet kommen. 0,60 € sind billiger als eine zweite Bestellung |

**Geändert gegenüber [einkauf.md](einkauf.md):** Das dort gelistete Kabelset
`DUPK-SET-20` (4,90 €) ist am 26.10.2026 **nur noch „Vorbestellung"**, ebenso
8 der 11 DUPK-Varianten. Die beiden oben genannten Einzelsorten sind am Lager
und zusammen **4,00 € statt 4,90 €**.

---

## Block B — Reifegrad 3 und 4 am Gerät

[build.md](build.md):160 und [architecture.md](architecture.md):163 sagen es
selbst: das Projekt steht bei **Stufe 2 von 4 — übersetzt und im Abbild, nie
am Gerät gelaufen.** Diese Teile bringen Stufe 3 und 4.

| Teil | Bestellnummer | Woher | Preis | Lager | Beantwortet welche Frage |
| --- | --- | --- | --- | --- | --- |
| **nRF52840-Dongle (PCA10059)** | `949-NRF52840-DONGLE` | Mouser | 9,34 € netto `ungeprüft (23.10.)` | 1.114 | **Zweitgerät.** Die Kopplung zweier Geräte und das Spiegeln der Passkeys sind sonst nicht prüfbar — dafür braucht es zwei Geräte, nicht zwei Prozesse |
| *Ersatz für den Dongle* | `2902521` | Farnell DE | 9,86 € netto | **0** — „Lieferungen beginnen am 20.11.2026" (abgerufen 26.10.) | nicht lieferbar |
| **Logikanalysator 8 Kanal, 24 MHz**, AZDelivery, mit USB-Kabel | ASIN `B01MUFRHQ2` | Amazon.de | **13,99 € brutto** | **Auf Lager**, Lieferung 28./29.10. (abgerufen 26.10.) | **Ist die I²C-Leitung wirklich verschlüsselt?** Die Aussage „I2C ist verschlüsselt (SCP03)" in [threat-model.md](threat-model.md) ist bisher eine Behauptung. Am Bus mitgelesen wird sie ein Befund. Dazu: die Flankenforderung aus Errata I2C.4 (SCL muss vor SDA wechseln) |
| **OLED 0,96", 128 × 64, SSD1306, SPI, 7-polig**, Hailege | ASIN `B07Z8QYCFV` | Amazon.de | **8,99 € brutto** | **nur noch 18** (abgerufen 26.10.) | Anzeigepfad. `osk_ui_port_nrf.c` ist **nie übersetzt** ([display.md](display.md):24) — hier läuft er zum ersten Mal |
| **microSD-Modul, SPI, 3,3-V-Regler**, Soldered, für Karten bis 32 GB | `SOL-333050` | BerryBase | **4,30 € brutto** | **Ab Lager** (26.10.) | **Neu — es fehlte.** Der SD-Pfad ist ohne Schacht nicht messbar; weder DK noch Click noch Dongle hat einen. Siehe Berichtigung 2 |
| *Billigere Möglichkeit* | `MSD-AADP` | BerryBase | 1,40 € brutto | 92 Stück (26.10.) | dasselbe, aber ohne belegten 3,3-V-Regler im Produkttext |
| **microSD-Karte 32 GB**, SanDisk Ultra microSDHC A1 Class 10 | `619659184155` | BerryBase | **17,90 € brutto** | **Ab Lager** (26.10.) | Karte zum Prüfen. `hardware/datentraeger.md`:21 verlangt **16 GB und mehr**; 32 GB ist zugleich die Obergrenze, die das Modul im Produkttext nennt |

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

## Block C — später; beantwortet jetzt keine offene Frage

| Teil | Bestellnummer | Woher | Preis | Warum nicht jetzt |
| --- | --- | --- | --- | --- |
| **Fingerabdrucksensor-Board** SparkFun FPC2534 (Qwiic) | `SEN-29854` | SparkFun, Antratek | 59,95 USD / 72,54 EUR `ungeprüft (24.10.)` | **Stufe 2.** Entschieden ist: Freigabeweg ist **Taster + PIN**, Stufe 1 ohne Sensor ([hardware.md](hardware.md):67–71). Tragender Grund: **bei FIDO2 ersetzt die Biometrie die PIN — sie ist kein zusätzlicher Faktor.** Der Sensor macht das Gerät bequemer, nicht sicherer. Siehe Berichtigung 3 |
| **OLED 48 × 64, 0,7″**, gelb/schwarz, SPI/I²C | `DEP048064A-Y` | Reichelt | **8,45 € brutto**, ab Lager, 1–2 Werktage (abgerufen 26.10.) | **Genau das Format der Zielanzeige** CFAL4864A-071BW (aktive Fläche 10,54 × 14,70 mm), nur gelb statt weiß. Für den Steckbrettaufbau ungeeignet: nacktes Glas mit Folienschwanz, braucht einen FPC-Verbinder. Die einzige bezahlbare Möglichkeit, die echte Bildschirmgröße zu prüfen — sinnvoll erst, wenn der Anzeigepfad am 0,96″-Modul läuft |
| **OM-SE052ARD** mit **SE052F** | `OM-SE052ARD` | NXP direkt | 69,00 USD | **Nicht bestellbar:** Status „Pending Stock", „No Distributors Available" (nxp.com, abgerufen 26.10.). Und als Ausweg aus der Kurvenfrage voraussichtlich **untauglich** — der SE052F ist „the first hardware Secure Element certified to FIPS 140-3 Level 3", und genau der FIPS-Weg schaltet EdDSA und Curve25519 ab. Gehört auf die Beobachtungsliste, nicht in die Bestellung |

---

## Summe

Preise so, wie der jeweilige Händler sie ausweist: Mouser und Farnell netto,
BerryBase und Amazon brutto.

### BerryBase — abgerufen 26.10.2026, brutto

| Teil | Artikel-Nr. | Menge | Einzel | Summe |
| --- | --- | --- | --- | --- |
| Steckbrett 830 Kontakte | `BB-830P` | 1 | 1,90 € | 1,90 € |
| Jumperkabel M-F 20 cm | `DUPK-40-FM-20` | 1 | 1,80 € | 1,80 € |
| Jumperkabel F-F 50 cm | `DUPK-40-FF-50` | 1 | 2,20 € | 2,20 € |
| Micro-USB-Kabel 1,0 m | `RPI-MUSB-1` | **2** | 1,80 € | 3,60 € |
| Stiftleiste 1 × 40 | `PINH-1X40P` | 1 | 0,60 € | 0,60 € |
| microSD-Modul SPI | `SOL-333050` | 1 | 4,30 € | 4,30 € |
| microSD-Karte 32 GB | `619659184155` | 1 | 17,90 € | 17,90 € |
| | | | **Waren** | **32,30 €** |
| DHL Paket (0,01–199,99 €) | | | | 4,95 € |
| | | | **Zusammen** | **37,25 €** |

Versandkosten belegt auf
[berrybase.de/versand-und-zahlungsbedingungen](https://www.berrybase.de/versand-und-zahlungsbedingungen/),
abgerufen 26.10.2026: DHL 4,95 €, versandkostenfrei ab 200,00 €. Kein
Mindestbestellwert.

### Amazon.de — abgerufen 26.10.2026, brutto

| Teil | ASIN | Preis |
| --- | --- | --- |
| Logikanalysator 8 Kanal 24 MHz | `B01MUFRHQ2` | 13,99 € |
| OLED 0,96″ SPI 7-polig | `B07Z8QYCFV` | 8,99 € |
| | **Zusammen** | **22,98 €** |

Versand: beide Artikel nennen „GRATIS Lieferung Donnerstag, 29. Oktober **für
qualifizierte Erstbestellung**". Ohne Prime greift bei 22,98 € womöglich eine
Mindestbestellschwelle; die Höhe steht erst an der Kasse — `ungeprüft`.

### Mouser — `ungeprüft (23.10.2026)`, netto

| Teil | Mouser-Nr. | Menge | Einzel netto |
| --- | --- | --- | --- |
| SE051 Plug & Trust Click | `932-MIKROE-5392` | 1 | 21,50 € |
| nRF52840-DK | `949-NRF52840-DK` | 1 | 41,37 € |
| nRF52840-Dongle | `949-NRF52840-DONGLE` | 1 | 9,34 € |
| | | **netto** | **72,21 €** |
| 19 % MwSt. | | | 13,72 € |
| | | **brutto** | **85,93 €** |

### Alles zusammen

| Händler | brutto | Versand | Stand |
| --- | --- | --- | --- |
| Mouser | 85,93 € | `ungeprüft` | 23.10.2026 |
| BerryBase | 37,25 € | enthalten (4,95 €) | 26.10.2026 |
| Amazon | 22,98 € | `ungeprüft` | 26.10.2026 |
| **Zusammen** | **ca. 146 €** | zzgl. Mouser-Versand | |

**Zwei Zahlen bleiben offen, beide bei Mouser.**

1. **Versandkosten.** Mouser nennt sie ausschließlich im Warenkorb. Einen
   Warenkorb habe ich nicht angelegt.
2. **Zoll und Einfuhrumsatzsteuer.** Mouser versendet aus Mansfield, Texas,
   weist auf mouser.de aber in EUR aus. Ob die Lieferung verzollt (DDP) kommt,
   ist nicht belegt. **Das ist die größte Unsicherheit dieser Rechnung** —
   [einkauf.md](einkauf.md):113 sagt das schon selbst. Käme Zoll obendrauf,
   wären es bei 72 € Warenwert grob 14 € Einfuhrumsatzsteuer plus
   Abgabengebühr; **gerechnet, nicht belegt.** Dann liegt die Bestellung bei
   rund 160 € statt 146 €.

**Wer das Zollrisiko halbieren will**, bestellt das DK bei **Farnell DE**
(`2842321`, 40,88 € netto, 67 am Lager, EU, kein Zoll) und bei Mouser nur
Click und Dongle (30,84 € netto). Farnell ist unter 50 € netto nicht
versandkostenfrei, der Vorteil schmilzt also — die Entscheidung gehört
Thomas, nicht dieser Liste.

**Das Mindeste, um nur die Kurvenfrage zu beantworten:** Click + DK + zwei
USB-Kabel + M-F-Kabel + Steckbrett, also **ca. 82 € brutto** zzgl. Versand.
Logikanalysator, Dongle, Display und SD-Teil kann man nachschieben.

---

## Bitte nachsehen, ob schon da

Einmal zu viel nachgesehen ist billiger als zweimal bestellt. **Nichts davon
ist blind mitbestellt** — was vorhanden ist, streicht die Zeile.

| Was | Steht in der Liste als | Hinweis |
| --- | --- | --- |
| **Logikanalysator** (8 Kanal USB reicht) | 13,99 € Amazon | liegt in Bastelkisten oft herum |
| **Steckbrett** | 1,90 € BerryBase | — |
| **Jumperkabel** M-F und F-F | 4,00 € BerryBase | — |
| **Zwei datenfähige Micro-USB-Kabel** | 3,60 € BerryBase | **nicht** eines aus der Ladeschublade: viele Billigkabel führen nur die zwei Stromadern. Es müssen **zwei** sein |
| **microSD-Karte ≥ 16 GB** | 17,90 € BerryBase | fast sicher vorhanden. Achtung: die Karte wird beim Prüfen **beschrieben und formatiert** — keine mit Daten darauf nehmen |
| **Kartenleser am PC** | nicht in der Liste | zum Gegenlesen dessen, was das Gerät geschrieben hat |
| **Lötkolben und Lötzinn** | nicht in der Liste | für Block A **nicht nötig**, wenn Click und Display mit bestückten Stiftleisten kommen. Beim Hailege-Display ist das `ungeprüft` — manche 7-poligen SSD1306-Module liegen mit loser Leiste bei. Das microSD-Modul und das Fingerabdruck-Board (Stufe 2) brauchen Löten |
| **Multimeter** | nicht in der Liste | für die Strommessungen aus [hardware.md](hardware.md):338–351 |

### Schon belegt — das braucht es *nicht*

| Frage | Antwort | Quelle |
| --- | --- | --- |
| Liegt dem DK ein USB-Kabel bei? | **Nein.** Farnell nennt als Lieferumfang „Preview-DK-Board nRF52840, 2032-Batterie, NFC-Antenne" — kein Kabel | Farnell `2842321`, abgerufen 26.10.2026 |
| Netzteil für das DK? | **Nein**, es läuft über USB | Nordic DK-Doku, „5 V power sources" |
| NFC-Antenne? | **Liegt bei** — zwei Quellen | Nordic Kit-Inhalt und Farnell-Lieferumfang |
| Programmieradapter / Debugger? | **Auf dem DK**, SEGGER J-Link an Bord, mit Debug-Out für den Dongle | Farnell-Produkttext, abgerufen 26.10.2026 |
| Batterie CR2032? | **Liegt bei** laut Farnell. Für den Teststand ohne Bedeutung — das DK läuft am USB | Farnell `2842321` |
| Kabel für den Dongle? | **Keines.** Der Dongle ist ein USB-A-Stecker und geht direkt in den Rechner | — |
| Akku, Laderegler, Schutzschaltung? | **Nein, entfallen.** Siehe nächster Abschnitt | Thomas, 25.10.2026 |

---

## Was durch Entscheidungen weggefallen ist

**Der Akku ist gestrichen** — Thomas, 25.10.2026, festgehalten in
`hardware/layout-notes.md`:3157 auf Zweig `akku-weg`: „Keine Zelle, kein
Laderegler, keine Schutzschaltung, keine Lastumschaltung. Das Gerät läuft
ausschließlich aus USB."

Für diese Liste heißt das: **nichts zu streichen** — Akkuzubehör stand in
[einkauf.md](einkauf.md) ohnehin nie, weil das DK am USB läuft. Zwei Dinge
verschieben sich trotzdem:

- 14 Bauteile fallen aus der **Zielplatine** (U4 MCP73832, J2, Q1–Q3, R1–R5,
  R13, R14, C17, C2), 8 Netze verschwinden. Das gehört in
  `docs/bestellliste-platine.md`, nicht hierher.
- **Die Begründung für NFC ändert sich:** „NFC und Bluetooth gibt es damit
  **nur am Rechner**" (ebenda). NFC wird dadurch nicht unwichtig, aber die
  Messung braucht kein zusätzliches Teil — die Antenne liegt dem DK bei.

**Die Fingerabdruck-Stiftreihe J5 bleibt** auf der Platine verlegt und
unbestückt, damit Stufe 2 ohne neue Platine geht. Dazu ein offener Punkt, der
nicht mir gehört: [fingerprint.md](fingerprint.md):628–631 sagt, **J5 passt
nicht zum gesetzten Sensor** — 1 × 6 mit 1,27 mm gegen einen 12-poligen
ZIF-Flex. Solange das so ist, trägt die Zusage „Stufe 2 ohne neue Platine"
nicht.

---

## Berichtigungen gegenüber `einkauf.md`

### 1. Ein SE051-Arduino-Kit von NXP gibt es doch

[einkauf.md](einkauf.md):257–258 schreibt: „Es gibt **kein SE051-Arduino-Kit
von NXP** bei Mouser oder DigiKey — nur die Mikroe-Click."

Für Mouser und DigiKey stimmt das. Als allgemeiner Satz ist es falsch:
**`OM-SE051ARD` existiert** und ist laut NXP „configured with a
**SE051C2HQ1/Z01XD** type" — also genau die Zielausführung. 69,00 USD bei NXP
direkt, dort am 26.10.2026 als „In Stock", 12 Stück, „Normally ships 1-2
business days".

**An der Bestellung ändert das nichts, und zwar aus einem belegten Grund.**
Am 26.10.2026 selbst nachgesehen:

| Quelle | Ergebnis |
| --- | --- |
| nxp.com direkt | 69,00 USD, „In Stock", 12 Stück |
| **de.farnell.com** Bestellnummer `3923714` | **404, Seite nicht gefunden.** Die Suche nach `om-se051ard` ergibt: „Wir haben keine Produkte gefunden" |
| be.farnell.com | „No Longer Stocked", „No Longer Manufactured" |
| Mouser DE, DigiKey DE | nicht geführt |

In der EU ist es also **nicht zu bekommen**, und die Click kostet ein Drittel
und ist am Lager. **Die Click bleibt erste Wahl.** Zu berichtigen ist nur der
Satz: er muss auf „bei Mouser und DigiKey nicht geführt, in der EU nicht
lieferbar" eingeengt werden.

### 2. „SD-Karte? Nicht nötig." ist überholt

[einkauf.md](einkauf.md):376 sagt: „**Nicht nötig.** Weder DK noch Click noch
Dongle hat einen Kartenschacht."

Der zweite Halbsatz ist weiter richtig — **und genau darum fehlt dem Teststand
etwas.** Seit der microSD-Schacht dazugekommen ist (`DM3AT-SF-PEJM5`,
Push-Push, in `hardware/datentraeger.md`), ist der SD-Pfad ein Teil der
Firmware, der gemessen werden muss. Ohne Schacht am Teststand ist er nicht
messbar. Deshalb neu in Block B: microSD-Modul am SPI (4,30 €) und eine
Karte (17,90 €).

### 3. Das Fingerabdruck-Board kommt wieder heraus

Der Nachtrag [einkauf.md](einkauf.md):19–22 vom 24.10.2026 hat die SparkFun
`SEN-29854` in die Bestellung geholt und die Summe damit von 125 € auf
180–195 € gehoben. Mit der Verschiebung auf **Stufe 2** gehört sie heraus.
**Das ist die größte Einsparung dieser Überarbeitung: rund 60–73 €.**

Sie steht weiter in Block C, mit Begründung — gestrichen ist sie nicht, nur
vertagt.

---

## Offene Punkte statt Bestellzeilen

Diese Dinge gehören **nicht** auf eine Bestellliste, weil man sie nicht
bestellen kann. Sie gehören in Arbeit.

### Acht `C9900…`-Nummern — Platzhalter, keine Teile

`C9900…`-Nummern sind **Bibliotheksplatzhalter von LCSC**: kein Hersteller,
keine Herstellernummer, kein Datenblatt. Belegt in
`hardware/layout-notes.md`:1419–1423 — die LCSC-Produktseite zu
`C9900141337` antwortet mit **„Part not found"**, die Seite existiert nicht.
Wörtlich dort: „Die `C9900…`-Nummern sind Platzhalter der Bibliothek, keine
bestellbaren Teile."

**Auf dieser Liste steht keine einzige davon** — und zwar nicht, weil ich sie
entfernt hätte, sondern weil **keine zum Teststand gehört**. Alle acht stehen
in `hardware/` und betreffen die Zielplatine:

| Nummer | Bauteil | Stand |
| --- | --- | --- |
| `C9900141337` | J2, JST-PH 2-polig (Akkustecker) | **doppelt hinfällig** — Platzhalter, und das Bauteil entfällt mit dem Akku |
| `C9900036520` | J4, FPC-ZIF 12-polig (Anzeige) | **bereits ersetzt** durch Hirose `FH12-12S-0.5SH(55)` = `C88360` |
| `C9900282109` | „TYPEC-24PIN L=15.5MM" | Platzhalter, nicht verwenden |
| `C9900245240` | „USB4155-03C" ohne Bindestrich | Platzhalter — das echte Teil ist `C5183853` |
| `C9900001620` | `USB-TYPE_C16P`, für 1,6-mm-Platine | verworfen |
| `C9900262527` | U22410-02, für 1,6-mm-Platine | verworfen |
| `C9900262530` | U22409-01, wasserdicht | verworfen |
| `C9900062937` | HYC-1160-003 | verworfen |

Zwei davon sind damit schon geklärt, sechs sind verworfene Alternativen.
**Zu tun bleibt:** `hardware/schematic-notes.md`:27 führt J4 noch als
`C9900036520`, obwohl `hardware/schematic.md` längst getauscht hat. Gehört in
`docs/bestellliste-platine.md`.

### Der echte USB-Stecker und ein Widerspruch

Der gewechselte Randstecker **GCT `USB4155-03-C`** ist LCSC **`C5183853`** —
mit Hersteller, Datenblatt und 3D-Modell. Händler: DigiKey DE, Artikel
`14305161`, am 26.10.2026 über die Suche belegt mit **10.460 Stück** und
**1,39 € netto** je Stück (regionale Seiten; eine USD-Zeile war nicht
auffindbar). Die von [usb-stecker.md](usb-stecker.md):36 genannten 1,46 € bei
15.070 Stück stammen vom 25.10. — die Zahlen liegen nah beieinander.

**Widerspruch, der aufzulösen ist:** `hardware/netlist.enet`:540 und
`hardware/schematic-notes.md`:24 führen noch die **alte** Buchse
`USB4085-GF-A` (`C7095263`), während
`hardware/exports/netlist-nach-steckertausch.enet` den Tausch schon
ausgeführt hat. Gehört der Platinensitzung.

### Gigabit gegen Gigabyte — und ein erledigter Entwurf

Bei Speicherbauteilen schreiben Hersteller „64G" und meinen **Gigabit**.
Jede Zahl im Entwurf geprüft:

| Bauteil | Angabe | Einheit aufgelöst | Quelle |
| --- | --- | --- | --- |
| **W25Q512JVEIQ** (QSPI-Flash, Zielplatine) | „512 Mbit = 64 MByte" | **512 Mbit = 64 MByte** — stimmt, 512 ÷ 8 = 64 | [parts.md](parts.md):16 nennt **beide** Einheiten. Hier beißt die Falle nicht |
| **Zetta `ZDSD64GLGEAG-R`** (SD-NAND, Variante A) | „64G" | **64 Gbit = 8 GByte** | Zettas Bestellschlüssel: „64G = 64Gbit (8GByte)" |
| **XTX `XTSD08GLGEAG`** | „08G" | **8 Gbit = 1 GByte** | XTX-Datenblatt Rev. 3.1: „08G … 8Gbit (1GByte)" |
| **MK `MKDV4GCL-ABB`** | „4Gbit" | **4 Gbit ≈ 482 MByte** nutzbar | MK-Datenblatt zur `-ABG`: „SLC 4Gbit 482MByte" |

**Das erledigt eine Entwurfsvariante.** `hardware/datentraeger.md`:21
verlangt **16 GB und mehr**. Die drei genannten SD-NAND-Teile liefern 8 GB,
1 GB und 0,5 GB — und Zetta deckelt die Reihe bei 64 Gbit, also 8 GByte.
**Kein gelötetes SD-NAND der genannten Reihen erreicht die Anforderung.**
Der Kartenschacht (Variante B) ist damit nicht die bequemere, sondern die
**einzige** Variante, die die eigene Vorgabe erfüllt. Das ist ein Befund für
`docs/bestellliste-platine.md` und für `hardware/datentraeger.md`.

Zwei Nebenbefunde dazu, beide für die Platinensitzung:

- **LCSC-Nummer verwechselt.** `hardware/datentraeger.md`:163 nennt
  `C915803` für `MKDV4GCL-ABB`. `C915803` ist aber `MKDV4GCL-AB` (ohne
  zweites B); die `-ABB` ist `C7500178`. Zwei Teile, zwei Nummern.
- **Gehäusemaß widersprüchlich.** `datentraeger.md`:31–37 führt LGA8
  **8,00 × 6,20 × 0,80 mm**. Händler listen LGA-8 (6,2 × 8). Zettas **eigenes**
  Datenblatt nennt für den `ZDSD64GLGEAG` dagegen **LGA8 11,5 × 13 mm**, eine
  Sekorm-Zeile sogar WSON 11,5 × 13 mm. Vor einem Layout zu klären — betrifft
  nur Variante A, die nach dem Befund oben ohnehin ausscheidet.

### Was die Doku als Stufenfolge nicht hat

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

- NXP, [OM-SE051ARD](https://www.nxp.com/design/design-center/development-boards-and-designs/OM-SE051ARD) —
  „configured with a SE051C2HQ1/Z01XD type", 69,00 USD, In Stock 12
- NXP, [OM-SE052ARD](https://www.nxp.com/design/design-center/development-boards-and-designs/OM-SE052ARD) —
  SE052F, „the first hardware Secure Element certified to FIPS 140-3 Level 3",
  69,00 USD, „Pending Stock", „No Distributors Available"
- AN12973 „SE051 configurations", Rev. 2.0, 08.07.2024 — Tabellen 3, 4, 6
  (über den SE050-Treiber-Strang; nxp.com lieferte 404)
- AN12413, Abschn. 4.10.3.1 (ECDH im FIPS-Modus), Tabelle 5 (Merkmalbild)
- Zetta, SD-NAND-Datenblatt `ZDSD08G/16G/32G/64G` — Bestellschlüssel
  „64G = 64Gbit (8GByte)"
- XTX, `XTSD01G/02G/04G/08G` Rev. 3.1, 15.05.2019 — „08G … 8Gbit (1GByte)"
- MK, Datenblatt zu `MKDV4GCL` — „SLC 4Gbit 482MByte"

**Händler**

- Farnell DE: [`2842321` nRF52840-DK](https://de.farnell.com/nordic-semiconductor/nrf52840-dk/dev-kit-bluetooth-low-energy-soc/dp/2842321)
  (40,88 € netto, 67 auf Lager, frei ab 50 €) ·
  [`4696282` MIKROE-5392](https://de.farnell.com/mikroelektronika/mikroe-5392/development-boardseval-kits/dp/4696282)
  (19,47 € netto, kein Lager) · `2902521` nRF52840-Dongle (9,86 € netto,
  „Lieferungen beginnen am 20.11.2026") · `3923714` OM-SE051ARD (**404**)
- DigiKey DE: [`1471-MIKROE-5392-ND`](https://www.digikey.de/de/products/detail/mikroelektronika/MIKROE-5392/16910358)
  (22,19 € netto, Lager 0) · `14305161` GCT USB4155-03-C
- BerryBase: [Versand und Zahlungsbedingungen](https://www.berrybase.de/versand-und-zahlungsbedingungen/) ·
  `BB-830P` · `DUPK-40-FM-20` · `DUPK-40-FF-50` · `DUPK-SET-20`
  (Vorbestellung) · `RPI-MUSB-1` · `PINH-1X40P` · `SOL-333050` ·
  `MSD-AADP` · `619659184155` · `TOL-18627` (nicht vorrätig)
- Amazon.de: ASIN `B01MUFRHQ2` (13,99 €, auf Lager) · `B07Z8QYCFV`
  (8,99 €, nur noch 18)
- Reichelt: `DEP048064A-Y` (8,45 €, ab Lager) · `UT M26` (298,90 €) ·
  `DIGIL 410-338` (342,95 €) · `JOY-IT SCOPE M50` (128,30 €)
- Mouser DE: **nicht abrufbar**, Bot-Sperre. Zahlen aus
  [einkauf.md](einkauf.md) vom 23.10.2026

**Im Repo**

- [einkauf.md](einkauf.md) — Vorstand dieser Liste, 23./24.10.2026
- [hardware.md](hardware.md):311–351 — die beiden Werksfragen und die Messliste
- [se050.md](se050.md):130–192, 499–548 — Messplan und Merkmalbild
- [parts.md](parts.md):16, 69–117 — Teileauswahl, Speicher, Ströme
- [build.md](build.md):160, [architecture.md](architecture.md):163 — Stufe 2 von 4
- [fingerprint.md](fingerprint.md):5, 624–631 — Stufe 2 und der J5-Fehler
- [usb-stecker.md](usb-stecker.md):36–39, 84 — der gewechselte Stecker
- `hardware/datentraeger.md` (Zweig `sd-datentraeger`) — Schacht und Karte
- `hardware/layout-notes.md`:1419–1423, 3157 — Platzhalter und Akku-Entscheidung
