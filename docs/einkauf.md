# Einkauf — Teststand

Alle Zahlen am **23.10.2026** auf der jeweiligen Händlerseite selbst abgerufen,
nicht aus einem Suchindex. Preise und Bestände ändern sich täglich: was hier
steht, ist eine Momentaufnahme mit Datum. Was nicht bestätigt werden konnte,
steht ausdrücklich als `ungeprüft` da — nicht als Schätzung.

**Es wurde nichts bestellt und nichts in einen Warenkorb gelegt.** Thomas
bestellt selbst.

Grundlage ist die belegte Teileauswahl in [parts.md](parts.md).

## Nachtrag 24.10.2026: Fingerabdrucksensor kommt mit

Der zweite Suchdurchgang hat einen Sensor gefunden, der alle Bedingungen
erfüllt — Einzelheiten in [fingerprint.md](fingerprint.md). Thomas hat
entschieden: **Testboard mitbestellen.**

| Teil | Bestellnummer | Woher | ca. |
| --- | --- | --- | --- |
| SparkFun Fingerprint Sensor FPC2534 (Qwiic) | `SEN-29854` | SparkFun, RobotShop | 59,95 USD |
| *Alternative in Europa* | dasselbe Board | Antratek | 72,54 EUR |

Warum das Board und nicht der Chip: Es ist lagernd, steckbar über Qwiic und
kostet keine Platinenänderung. Damit lässt sich am echten Sensor messen, bevor
die Zielplatine gezeichnet wird. Taugt er, kommt er bei Stufe 1 mit drauf —
dann als Chip `FPC2534AM` (ca. 30 USD bei Mouser).

Preise `ungeprueft`, Abrufdatum 24.10.2026. Damit liegt die Bestellung bei rund
**180–195 €** statt 125 €.

## Kurzfassung

> **Der Sicherheitschip ist wieder freigegeben.** `CONFIG_HMAC` ist bestätigt
> (AN12973 Rev. 2.0, Tabelle 3): der SE051C2 kann HMAC und HKDF, das
> Grundgeheimnis lässt sich als HMACKey anlegen. Die MIKROE-5392 ist damit
> wieder in der Bestellung.
> **Offen bleibt die Speicherausführung** — 46 kB oder 104 kB. Das ist keine
> Bestellentscheidung: es gibt nur **eine** SE051C2-Bestellnummer, und für den
> Teststand reichen 46 kB. Die Zahl wird später am Gerät gemessen, nicht beim
> Hersteller erfragt. Eigener Abschnitt weiter unten.

| | |
| --- | --- |
| Mouser (DK, Dongle, SE051-Click) | **85,93 €** brutto + Versand `ungeprüft` |
| Kleinteile (Amazon) | **22,98 €** brutto |
| Kleinteile (BerryBase) | **15,95 €** brutto inkl. Versand |
| **Zusammen** | **ca. 125 €** brutto, zzgl. Mouser-Versand |

Drei Bestellungen bei drei Händlern. Warum nicht eine: kein Händler führt alles.
Die Begründung steht unten bei jedem Posten.

**Wann liegt es auf dem Tisch:** bei Bestellung am 24.10.2026 realistisch in der
Woche ab **Montag, 27.10.2026** — BerryBase 1–3 Werktage, Amazon zum 27.10.
zugesagt, Mouser versendet taggleich aus Texas (Laufzeit 2–4 Werktage,
`ungeprüft`). Spätestens Anfang November.

## Drei Berichtigungen gegenüber der bisherigen Kaufliste

1. **Reichelt führt das nRF52840-DK nicht.** Die Suche nach `nrf52840-dk`
   ergab am 23.10.2026 **0 Treffer**; von 18 Treffern zu `nrf52840` ist kein
   einziger das DK. Die alte Liste nannte Reichelt als Quelle — falsch.
2. **Der GeeekPi-Dongle für 24,99 € lohnt nicht.** Die Hardwarebeschreibung von
   makerdiary nennt **keinen externen Flash** und einen **USB-A-Stecker**
   (42 × 17,78 mm). Nordics eigener nRF52840-Dongle kostet bei Mouser
   **9,34 €** und kann dasselbe. **Ersparnis rund 15 €.** Die alte Liste nannte
   außerdem „USB-C für den Dongle" — beide Dongles stecken direkt als USB-A,
   es braucht gar kein Kabel.
3. **Dem DK liegt kein USB-Kabel bei.** Nordic nennt als Kit-Inhalt nur Board
   und NFC-Antenne (Farnell listet zusätzlich eine CR2032). Das DK hat **zwei**
   Micro-USB-Buchsen: **J2** für den Debugger, **J3** für die USB-Peripherie des
   nRF52840 selbst. Wer FIDO2 über USB messen und gleichzeitig debuggen will,
   braucht **zwei** Micro-USB-Kabel.

---

## Mouser Deutschland — die Kernteile

Mouser ist am 23.10.2026 der **einzige** Händler, der alle drei Kernteile am
Lager hat. Preise netto (so weist Mouser aus).

| Teil | Mouser-Nr. | Menge | Einzel netto | Lager | Link |
| --- | --- | --- | --- | --- | --- |
| nRF52840-DK (PCA10056) | 949-NRF52840-DK | 1 | 41,37 € | **443** sofort lieferbar | [mouser.de](https://www.mouser.de/ProductDetail/949-NRF52840-DK) |
| SE051 Plug&Trust Click | 932-MIKROE-5392 | 1 | 21,50 € | **92** sofort lieferbar | [mouser.de](https://www.mouser.de/ProductDetail/932-MIKROE-5392) |
| nRF52840-Dongle (PCA10059) | 949-NRF52840-DONGLE | 1 | 9,34 € | **1.114** sofort lieferbar | [mouser.de](https://www.mouser.de/ProductDetail/949-NRF52840-DONGLE) |
| | | | **72,21 € netto** | | |
| 19 % MwSt. | | | 13,72 € | | |
| | | | **85,93 € brutto** | | |

- **Mindestbestellwert: keiner.** Belegt auf der Mouser-Hilfeseite
  „Bestellungen und Versand": „Es gibt keine Mindestbestellmenge."
- **Versandkosten: `ungeprüft`.** Mouser nennt sie ausschließlich im Warenkorb
  („Die Versandkosten für Ihre Bestellung finden Sie im Warenkorb"). Einen
  Warenkorb habe ich nicht angelegt. **Thomas sieht die Zahl beim Bestellen.**
- **Zoll und Einfuhrumsatzsteuer: `ungeprüft`.** Mouser versendet aus Mansfield,
  Texas, weist auf mouser.de aber in EUR aus. Ob die Lieferung verzollt (DDP)
  kommt, konnte ich auf der Seite nicht belegt finden. Die Seite, die diese
  Aussage trägt, war beim Abruf nicht lesbar.
  **Das ist die größte Unsicherheit dieser Rechnung.** Käme Zoll obendrauf,
  wären es bei 72 € Warenwert grob 14 € Einfuhrumsatzsteuer plus Abgabengebühr
  — gerechnet, nicht belegt.
- Lieferzeit ab Hersteller (für Mengen über Lager): DK 33 Wochen, Click
  4 Wochen, Dongle 21 Wochen. Für je 1 Stück ohne Bedeutung — es ist am Lager.

---

## Sicherheitschip — freigegeben, aber die Speicherausführung ist offen

Er ist der Kern des Teststands: ohne ihn ist keine Messung möglich.

**Geklärt:** `CONFIG_HMAC` ist bestätigt. AN12973 Rev. 2.0 vom 8. Juli 2024,
Tabelle 3, Zeilen „MAC – HMAC, CMAC, GMAC" und „Key Derivation (KDF) – HKDF":
beide mit Haken beim SE051C2. Das Grundgeheimnis lässt sich also als HMACKey
anlegen. **Der Chip bleibt richtig, die MIKROE-5392 ist wieder in der
Bestellung.**

| Quelle | Preis | Lager | Taugt? |
| --- | --- | --- | --- |
| **Mouser** 932-MIKROE-5392 | 21,50 € netto | **92** | **erste Wahl** |
| [mikroe.com](https://www.mikroe.com/se051-plugtrust-click) direkt | 25,00 USD | Hersteller | zweite Wahl; Versand aus Serbien, Kosten `ungeprüft`. Mikroe schreibt auf der Seite, die Produkte unterlägen keinen US-Zöllen |
| **Farnell DE** 4696282 | 19,47 € netto | **0** — „Bestellbar, Lieferung nach Auftragsbestätigung" | billigster Preis, aber nicht lieferbar |
| **DigiKey DE** 1471-MIKROE-5392-ND | 21,95 € netto | **0** — 1 Stück überfällig, Termin 16.10.2026 verstrichen | nein |
| TME | — | — | Seite wehrt automatische Abrufe ab; `ungeprüft` |

Mikroe nennt auf der eigenen Produktseite dieselben Bestände: Mouser 92,
DigiKey 0, Farnell 0. Zwei unabhängige Quellen, gleiches Bild.

### 46 kB oder 104 kB? Offen — und für den Teststand ohne Bedeutung

Der Speicher ist eine Spanne, keine Zahl: jeder Dienst im Betriebsmodus „nur
dieses Gerät" belegt ein eigenes Schlüsselpaar im Chip, bei der kleinen
Ausführung passt ungefähr die Hälfte hinein.

**Trotzdem wird hier nichts nachgefragt, und das ist kein Zögern.** Die
Entscheidung, mit Begründung:

- **Der Teststand braucht die große Ausführung nicht.** Er soll beweisen, dass
  die Technik trägt: SE051 antwortet, SCP03 steht, FIDO2 läuft über USB, NFC
  und Smartcard. Dafür braucht es eine Handvoll Schlüssel, nicht hunderte.
  **46 kB reichen für jede dieser Messungen.**
- **Die Größe entscheidet erst bei der eigenen Platine** — und dort wird der
  Chip einzeln gekauft. Dann steht die Ausführung in der Bestellnummer, und die
  Frage löst sich von selbst.
- **Gemessen schlägt erfragt.** Sobald die Platine auf dem Tisch liegt, liest
  der Treiber beim Anmelden `GetFreeMemory` aus. Dann steht die Zahl fest, und
  zwar verlässlicher als eine Auskunft per E-Mail.
- **Eine Herstelleranfrage wäre eine Nachricht nach außen in Thomas' Namen.**
  Das entscheidet er, nicht wir — und für eine Zahl, die in zwei Wochen ohnehin
  gemessen wird, lohnt es nicht.

Die offene Stelle ist also keine Lücke, sondern eine bewusst offen gelassene
Zahl mit bekanntem Termin.

Was trotzdem nachgesehen wurde, damit die Zahl nicht geraten wird:

**1. Welche Ausführung trägt die MIKROE-5392? → `ungeprüft`.**

Geprüft wurden alle drei Stellen, an denen es stehen könnte:

| Nachgesehen | Ergebnis |
| --- | --- |
| [Schaltplan v100b](https://download.mikroe.com/documents/add-on-boards/click/se051-plugtrust-click/SE051%20Plug%26Trust_click_v100b_Schematic.PDF) (heruntergeladen und gelesen) | U1 ist nur mit **„SE051"** beschriftet — keine Bestellnummer, keine 12NC, keine Speicherangabe |
| mikroe.com Produktseite | nennt nur „**SE051C2**", ohne Bestellcode |
| Datenblatt, das Mikroe verlinkt | NXPs allgemeines SE051-Produktdatenblatt, nicht platinenspezifisch |

**Es steht also nirgends — und wird nach den Gründen oben auch nicht erfragt.**
Die Zahl kommt, wenn die Platine da ist, aus `GetFreeMemory`.

**2. Welche Teilenummer als Einzelchip, und welche ist die 104-kB-Fassung?**

Es gibt **genau eine** SE051C2-Bestellnummer im Handel:

| Teil | 12NC | Mouser | DigiKey | Farnell |
| --- | --- | --- | --- | --- |
| **SE051C2HQ1/Z01XDZ** | **935414457472** | **5,02 €**, 14.140 am Lager | 5,63 € | führt sie nicht |

Geprüft am 23.10.2026: Mouser kennt unter `SE051C2HQ1` nur diese eine Nummer,
DigiKey ebenfalls (die Zusätze `…CT`, `…TR`, `…DKR` sind DigiKeys eigene
Verpackungsformen desselben Teils). Die 12NC stimmt bei beiden Händlern
überein.

**Die Speichergröße steht nicht im Namen.** Das Datenblatt (Rev. 1.4,
28.03.2022, Abschnitt 1.3, Tabelle 1) löst `SE05yagddd/Zrrff` so auf:

| Stelle | im Beispiel | Bedeutung |
| --- | --- | --- |
| y | **1** | JCOP-Version |
| a | **C** | Produktkonfiguration (A, C oder P) |
| g | **2** | Temperaturbereich −40 °C … +105 °C |
| ddd | **HQ1** | Gehäuse HX2QFN20 |
| Zrrff | **/Z01XD** | „**NXP internal code to identify individual configurations**" |

Die Unterscheidung 46/104 kB steckt also im internen Code `/Z01XD`, den das
Datenblatt nicht auflöst. **Es gibt keine zweite Nummer, zwischen der man
wählen könnte** — man bekommt, was unter `/Z01XD` geliefert wird.

**3. Preisunterschied? → gibt es nicht**, weil es keine zweite Ausführung zu
kaufen gibt.

### Vorsicht bei der Zahl in der Doku: NXP nennt selbst zwei Spannen

| Quelle | Spanne |
| --- | --- |
| SE051 Produktdatenblatt **Rev. 1.4, 28.03.2022**, Abschnitt 2.2 | „Secured user flash memory ranging from **45 kB** full featured up to **101 kB**" |
| AN12973 **Rev. 2.0, 08.07.2024**, Tabelle 3 | **46 kB** … **104 kB** |

Beides von NXP, beides zum selben Chip, unterschiedliche Dokumentstände. Welche
Zahl auch immer in die Doku kommt — **mit Dokument und Revision dahinter**,
sonst steht dort eine Zahl, die ein anderes NXP-Papier bestreitet. Die
AN12973-Fassung ist die jüngere.

Das Datenblatt sagt an derselben Stelle außerdem: „Dependent on the ordered
product configuration the feature set can vary." Es geht also nicht nur um
Speicher.

### Zwei Ausführungen desselben Namens

AN12973 Tabelle 4 unterscheidet beim SE051C2 zwei OEFs: **A8FA** mit Applet 7.2
(Datumscode ab 2201) und **A564** mit Applet 6.0 (bis 2152). Beide heißen
SE051C2, und sie haben **verschiedene Werks-SCP03-Schlüssel** (Tabelle 6).

**Für die Bestellung heißt das: nichts auswählbar.** Weder Mouser noch DigiKey
lassen Datumscode oder OEF wählen; beide führen nur `/Z01XDZ`. Welche Fassung
kommt, zeigt sich am gelieferten Teil — und die Erstinbetriebnahme muss den
passenden Schlüsselsatz nehmen. Das gehört in die Inbetriebnahme-Anleitung,
nicht in die Kaufliste.

### Wenn der SE051C2 doch noch ausfällt: was es als Platine gäbe

Vollständig durchgesehen bei mikroe.com, Mouser DE und DigiKey DE am
23.10.2026. Das Ergebnis ist unangenehm schmal.

| Platine | Chip darauf | Preis | Lager | Urteil |
| --- | --- | --- | --- | --- |
| **MIKROE-5392** SE051 Plug&Trust Click | **SE051C2** | 21,50 € netto (Mouser) | **92** | bisherige Wahl, Merkmal offen |
| **MIKROE-4656** Plug&Trust Click | **SE050C** | 11,70 USD direkt (war 18,00) | **Mouser 0, DigiKey 0, Farnell 0, TME 0** — Status **„Phase out"** | nur noch beim Hersteller, abgekündigt |
| **MIKROE-5391** A5000 Plug&Trust Click | A5000 | 14,00 USD | Hersteller | **reiner Authentifizierer**, kein allgemeiner Schlüsselspeicher — fast sicher ungeeignet |
| **OM-SE050ARD-E** | **SE050E2** | 72,23 € netto (Mouser) | **135** | **kein RSA** → kein PIV, kein OpenPGP. Schon in parts.md verworfen |
| **OM-SE050ARD** | **SE050C2** | 81,13 € netto (DigiKey) | 10, aber **„Bei DigiKey nicht mehr erhältlich"** | letzte Reste |
| **OM-SE050ARD-F** | SE050F2 | — | **0** (DigiKey) | nicht lieferbar |
| Murata LBUA0VG2BP-EVK-P | **SE051W** + QN9090 + SR150 | 158,03 € netto (DigiKey) | 384 | UWB-Kit; die einzige andere Platine mit einem SE051 darauf, aber Preis und Umfang stehen in keinem Verhältnis |

**Das heißt:** Es gibt **kein SE051-Arduino-Kit von NXP** bei Mouser oder
DigiKey — nur die Mikroe-Click. Fällt der SE051C2 aus, bleiben als Platine
praktisch nur der abgekündigte SE050C (MIKROE-4656, nirgends mehr am Lager)
oder das teure OM-SE050ARD mit dem SE050C2, von dem DigiKey noch 10 Stück
abverkauft.

**Die nackten Chips gibt es dagegen reichlich** — alle HX2QFN20, 3 × 3 mm,
bei Mouser DE:

| Teil | Preis | Lager |
| --- | --- | --- |
| SE051C2HQ1/Z01XDZ | 5,02 € | 14.140 |
| SE051A2HQ1/Z01XEZ | 4,71 € | 1.369 |
| SE051W2HQ1/Z019TZ | 5,57 € | 1.242 |
| SE050C2HQ1/Z01SDZ | 4,33 € | 0, erwartet ab 23.11.2026 |
| SE051P2HQ1/Z011AZ | 3,30 € | nur ab 3.000 Stück, 16 Wochen |

Ein 3 × 3 mm großes HX2QFN20 lässt sich nicht aufs Steckbrett löten. Diesen Weg
zu gehen hieße, eine eigene Adapterplatine fertigen zu lassen — Wochen statt
Tage. **Deshalb ist die Antwort aus AN12973 die teuerste offene Frage des
Projekts**, nicht nur die teuerste Zeile dieser Liste.

---

### Ersatzquellen für die beiden Rechenkerne

| Posten | Ersatz | Preis netto | Lager |
| --- | --- | --- | --- |
| nRF52840-DK | **Farnell DE** [2842321](https://de.farnell.com/nordic-semiconductor/nrf52840-dk/dev-kit-bluetooth-low-energy-soc/dp/2842321) | **40,88 €** | **74** am Lager, Versand **frei ab 50 € netto**, Express am nächsten Werktag |
| nRF52840-DK | DigiKey DE 1490-1072-ND | 42,98 € | 479 am Lager, Versand **frei ab 75 € netto** |
| nRF52840-Dongle | Farnell DE 2902521 | 9,86 € | **0** — Lieferung ab 20.11.2026 |
| nRF52840-Dongle | GeeekPi MDK, Amazon [B07TSJHTSY](https://www.amazon.de/dp/B07TSJHTSY) | 24,99 € brutto | lieferbar, aber 15 € teurer ohne Mehrwert |

**Farnell ist die saubere EU-Rückfalllösung für das DK:** Preis und
Versandbedingungen stehen auf der Produktseite, innerhalb der EU fällt kein
Zoll an. Nur: Farnell hat den Dongle nicht am Lager (ab 20.11.2026), und beim
DK allein bliebe die Bestellung mit 40,88 € unter der 50-€-Schwelle für den
freien Versand.

---

## Amazon.de — Display und Messmittel

Beides führt weder Mouser noch ein deutscher Elektronikhändler lieferbar.

| Teil | ASIN | Preis brutto | Lager / Lieferung |
| --- | --- | --- | --- |
| **Logikanalysator 8 Kanal, 24 MHz**, AZDelivery, mit USB-Kabel | [B01MUFRHQ2](https://www.amazon.de/dp/B01MUFRHQ2) | **13,99 €** | lieferbar, 27.10.2026, 4,5★ bei 1030 Bewertungen |
| **OLED 0,96", 128×64, SSD1306, SPI, 7-polig**, Hailege (gelb/blau) | [B07Z8QYCFV](https://www.amazon.de/dp/B07Z8QYCFV) | **8,99 €** | noch 19 Stück, 27.10.2026 |
| | | **22,98 €** | |

- **Versand:** Amazon sagt bei beiden Artikeln „GRATIS Lieferung Di., 27. Okt."
  zu, allerdings an Bedingungen geknüpft. Ohne Prime greift die
  Mindestbestellschwelle; bei 22,98 € kann ein Versandbeitrag anfallen.
  Konkrete Höhe `ungeprüft` — sie steht erst an der Kasse.
  **Abhilfe:** die BerryBase-Kleinteile stattdessen hier mitbestellen, dann ist
  die Schwelle überschritten und beide Bestellungen werden zu einer.
- **Die weiße Ausführung** des Hailege-Displays ([B07Z8QZLXS](https://www.amazon.de/dp/B07Z8QZLXS),
  7,99 €) wäre schöner als gelb/blau, hatte aber kein reguläres Angebot —
  nur ein Drittanbieter-Angebot, `ungeprüft`.
- **Wichtig:** die **SPI**-Ausführung mit **7 Stiften** nehmen, nicht die
  I2C-Ausführung mit 4. Die SSD1306-Module bei Reichelt (DEBO OLED2/3/4 0.96,
  6,99–8,85 €) sind alle **I2C** — geprüft am Produkttext von DEBO OLED3:
  „Kommunikation: I2C (Adresse 0x3C)". Taugen also nicht.

### Was Reichelt stattdessen Interessantes hat

`DEP048064A-Y` — OLED **48 × 64 px**, aktive Fläche **10,54 × 14,70 mm**,
SPI/I²C, **8,45 €**, ab Lager. Das ist **genau das Format der Zielanzeige
CFAL4864A-071BW**, nur gelb statt weiß. Für den Steckbrett-Aufbau ungeeignet
(nacktes Glas mit Folienschwanz, braucht einen FPC-Verbinder), aber die einzige
bezahlbare Möglichkeit, **die echte Bildschirmgröße der Zielplatine schon jetzt
zu prüfen**. Nicht in der Bestellung — als Hinweis für später notiert.

---

## BerryBase — Steckbrett, Kabel, Kleinkram

Deutscher Händler, alles sofort lieferbar, Preise brutto.

| Teil | Artikel-Nr. | Menge | Einzel | Summe |
| --- | --- | --- | --- | --- |
| Steckbrett, 830 Kontakte | BB-830P | 1 | 1,90 € | 1,90 € |
| Jumper-/Dupont-Kabel-Set, je 1× F-F / M-M / F-M, 20 cm | DUPK-SET-20 | 1 | 4,90 € | 4,90 € |
| Micro-USB-Kabel 1,0 m (offiziell Raspberry Pi, datenfähig) | RPI-MUSB-1 | **2** | 1,80 € | 3,60 € |
| Stiftleiste 1×40, RM 2,54, gerade | PINH-1X40P | 1 | 0,60 € | 0,60 € |
| | | | **Waren** | **11,00 €** |
| Versand DHL (0,01–199,99 €) | | | | 4,95 € |
| | | | **Zusammen** | **15,95 €** |

- Versandkosten belegt auf
  [berrybase.de/versand-und-zahlungsbedingungen](https://www.berrybase.de/versand-und-zahlungsbedingungen/):
  DHL 4,95 €, versandkostenfrei ab 200 €. Kein Mindestbestellwert.
- **Zwei** Micro-USB-Kabel, siehe Berichtigung 3 oben.
- Die Stiftleiste ist für den Fall, dass der Click oder das Display ungelötet
  kommt. Der SE051-Click hat die mikroBUS-Leisten bereits bestückt
  (`ungeprüft`, aus dem Produktbild) — 0,60 € sind billiger als eine zweite
  Bestellung.

### Was BerryBase nicht konnte

| Gesucht | Ergebnis am 23.10.2026 |
| --- | --- |
| OLED 0,96" SPI | OLED-096BY-V, 7,80 € — **„Artikel aktuell nicht lieferbar"** |
| Logikanalysator | SparkFun TOL-18627, 29,90 € — **„Nicht vorrätig"** |

Deshalb gehen diese beiden zu Amazon.

---

## Was beim Aufbau sonst noch fehlen könnte

Einmal zu viel gefragt ist billiger als zweimal bestellt.

| Frage | Antwort |
| --- | --- |
| Liegt dem DK ein USB-Kabel bei? | **Nein.** Nordic nennt als Kit-Inhalt nur Board + NFC-Antenne. Zwei Micro-USB-Kabel sind in der BerryBase-Liste. |
| Braucht das DK ein Netzteil? | **Nein.** Es läuft über USB (5-V-Aufwärtsregler, Quellen J2, J3, Akkustecker J6/P27, VIN an P20 — belegt in der DK-Doku). |
| NFC-Antenne? | **Liegt bei** (Nordic-Doku und Farnell-Kit-Inhalt, zwei Quellen). |
| SD-Karte? | **Nicht nötig.** Weder DK noch Click noch Dongle hat einen Kartenschacht. |
| Lötwerkzeug? | **Für Stufe 1 nicht nötig**, wenn Click und Display mit bestückten Stiftleisten kommen. Beim Display `ungeprüft` — manche 7-polige SSD1306-Module liegen mit loser Leiste bei. Deshalb die Stiftleiste in der Liste. Ob ein Lötkolben vorhanden ist, weiß ich nicht. |
| Dongle-Kabel? | **Keines.** Beide Dongles sind USB-A-Stecker und gehen direkt in den Rechner. |
| Debugger? | **Auf dem DK** (SEGGER J-Link an Bord). Den Dongle kann man per UF2 bzw. über die DFU-Taste flashen oder über die Debug-Out-Leiste des DK. |
| Batterie CR2032? | Farnell nennt sie im Kit-Inhalt, Nordic nicht. `ungeprüft` — für den Teststand ohne Bedeutung, das DK läuft am USB. |

---

## Gesamtsumme

| Händler | Waren brutto | Versand | Zusammen |
| --- | --- | --- | --- |
| Mouser (DK, Click, Dongle) | 85,93 € | `ungeprüft` | 85,93 € + Versand |
| Amazon | 22,98 € | 0 € zugesagt, Bedingungen `ungeprüft` | 22,98 € |
| BerryBase | 11,00 € | 4,95 € | 15,95 € |
| **Zusammen** | **119,91 €** | | **ca. 125 €** |

Gegenüber der alten Schätzung von „ca. 135 €" sind es rund 10 € weniger, mit
geprüften Zahlen statt Momentaufnahmen — obwohl zwei Kabel und eine Stiftleiste
dazugekommen sind. Der Dongle allein spart 15 €.

**Eine Zahl bleibt offen:** ob Mouser Zoll und Einfuhrumsatzsteuer schon im
Preis trägt. Schlägt beides obendrauf, liegt die Bestellung bei rund 140 € statt
125 €. Thomas sieht die Zahl an der Kasse, bevor er bestellt.

**Eine Zahl bleibt bewusst offen:** welche Speicherausführung geliefert wird.
Sie ist nicht bestellbar, für den Teststand ohne Bedeutung, und wird gemessen,
sobald die Platine da ist. Siehe Abschnitt Sicherheitschip.

## Quellen

Alle am **23.10.2026** abgerufen.

- SE051 Produktdatenblatt, Rev. 1.4, 28.03.2022 (577314) — Abschnitt 1.3
  Tabelle 1 (Namensschema), Abschnitt 2.2 (45–101 kB).
  Über mikroe.com geladen: https://download.mikroe.com/documents/datasheets/SE051_datasheet.pdf
- AN12973 „SE051 configurations", Rev. 2.0, 08.07.2024 — Tabellen 3, 4 und 6.
  Gelesen vom SE050-Treiber-Strang, nicht hier; nxp.com lieferte 404.
- MIKROE-5392 Schaltplan v100b — https://download.mikroe.com/documents/add-on-boards/click/se051-plugtrust-click/SE051%20Plug%26Trust_click_v100b_Schematic.PDF
- Mouser DE: [SE051C2HQ1/Z01XDZ](https://www.mouser.de/ProductDetail/771-SE051C2HQ1Z01XDZ) ·
  [949-NRF52840-DK](https://www.mouser.de/ProductDetail/949-NRF52840-DK) ·
  [932-MIKROE-5392](https://www.mouser.de/ProductDetail/932-MIKROE-5392) ·
  [949-NRF52840-DONGLE](https://www.mouser.de/ProductDetail/949-NRF52840-DONGLE) ·
  [Bestellungen und Versand](https://www.mouser.de/de/help/orders-shipping/)
- Farnell DE: [NRF52840-DK 2842321](https://de.farnell.com/nordic-semiconductor/nrf52840-dk/dev-kit-bluetooth-low-energy-soc/dp/2842321) ·
  [MIKROE-5392 4696282](https://de.farnell.com/mikroelektronika/mikroe-5392/development-boardseval-kits/dp/4696282) ·
  NRF52840-DONGLE 2902521
- DigiKey DE: [NRF52840-DK 1490-1072-ND](https://www.digikey.de/de/products/detail/nordic-semiconductor-asa/NRF52840-DK/8593726) ·
  [MIKROE-5392 1471-MIKROE-5392-ND](https://www.digikey.de/de/products/detail/mikroelektronika/MIKROE-5392/16910358)
- MikroElektronika: [SE051 Plug&Trust Click](https://www.mikroe.com/se051-plugtrust-click)
- BerryBase: [Versand- und Zahlungsbedingungen](https://www.berrybase.de/versand-und-zahlungsbedingungen/),
  Artikel BB-830P, DUPK-SET-20, RPI-MUSB-1, PINH-1X40P
- Reichelt: Suche `nrf52840-dk` (0 Treffer), `oled ssd1306`,
  [DEBO OLED3 0.96](https://www.reichelt.de/de/de/shop/produkt/entwicklerboards_-_display_0_96_oled-display_ssd1306-372107), `DEP048064A-Y`
- Amazon.de: ASIN B01MUFRHQ2, B07Z8QYCFV, B07Z8QZLXS, B07TSJHTSY
- Nordic: [nRF52840 DK Kit content](https://docs.nordicsemi.com/r/bundle/ug_nrf52840_dk/page/ug/dk/kit_content.html) ·
  [5 V power sources](https://docs.nordicsemi.com/r/bundle/ug_nrf52840_dk/page/ug/dk/5v_power_sources.html)
- makerdiary: [nRF52840 MDK USB Dongle, Hardware description](https://wiki.makerdiary.com/nrf52840-mdk-usb-dongle/hardware/)
