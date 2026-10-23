# Einkauf — Teststand

Alle Zahlen am **23.10.2026** auf der jeweiligen Händlerseite selbst abgerufen,
nicht aus einem Suchindex. Preise und Bestände ändern sich täglich: was hier
steht, ist eine Momentaufnahme mit Datum. Was nicht bestätigt werden konnte,
steht ausdrücklich als `ungeprüft` da — nicht als Schätzung.

**Es wurde nichts bestellt und nichts in einen Warenkorb gelegt.** Thomas
bestellt selbst.

Grundlage ist die belegte Teileauswahl in [parts.md](parts.md).

## Kurzfassung

| | |
| --- | --- |
| Kernteile (Mouser, alles am Lager) | **85,93 €** brutto + Versand `ungeprüft` |
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

### Was gegen den SE051-Click spricht, wenn er wegfällt

Er ist der Kern des Teststands: ohne ihn ist keine Messung möglich. Deshalb
Ersatzquellen, alle am 23.10.2026 geprüft:

| Quelle | Preis | Lager | Taugt? |
| --- | --- | --- | --- |
| **Mouser** 932-MIKROE-5392 | 21,50 € netto | **92** | **erste Wahl** |
| [mikroe.com](https://www.mikroe.com/se051-plugtrust-click) direkt | 25,00 USD | Hersteller | zweite Wahl; Versand aus Serbien, Kosten `ungeprüft`. Mikroe schreibt auf der Seite, die Produkte unterlägen keinen US-Zöllen |
| **Farnell DE** 4696282 | 19,47 € netto | **0** — „Bestellbar, Lieferung nach Auftragsbestätigung" | billigster Preis, aber nicht lieferbar |
| **DigiKey DE** 1471-MIKROE-5392 | 21,95 € netto | **0** — 1 Stück überfällig, Termin 16.10.2026 bereits verstrichen | nein |
| TME | — | — | Seite wehrt automatische Abrufe ab; `ungeprüft` |

Mikroe nennt auf der eigenen Produktseite dieselben Bestände: Mouser 92,
DigiKey 0, Farnell 0. Zwei unabhängige Quellen, gleiches Bild.

**Nicht nehmen:** OM-SE050ARD-E (DigiKey, 73,75 € netto). Trägt den SE050E2 —
**kein RSA**, also kein PIV und kein OpenPGP. Stand schon in parts.md.

### Ersatzquellen für die anderen beiden Mouser-Posten

| Posten | Ersatz | Preis netto | Lager |
| --- | --- | --- | --- |
| nRF52840-DK | **Farnell DE** [2842321](https://de.farnell.com/nordic-semiconductor/nrf52840-dk/dev-kit-bluetooth-low-energy-soc/dp/2842321) | **40,88 €** | **74** am Lager, Versand **frei ab 50 € netto**, Express am nächsten Werktag |
| nRF52840-DK | DigiKey DE 1490-1072-ND | 42,98 € | 479 am Lager |
| nRF52840-Dongle | Farnell DE 2902521 | 9,86 € | **0** — Lieferung ab 20.11.2026 |
| nRF52840-Dongle | GeeekPi MDK, Amazon [B07TSJHTSY](https://www.amazon.de/dp/B07TSJHTSY) | 24,99 € brutto | lieferbar, aber 15 € teurer ohne Mehrwert |

**Farnell ist die saubere EU-Rückfalllösung für das DK:** Preis und
Versandbedingungen stehen auf der Produktseite, innerhalb der EU fällt kein
Zoll an. Nur: Farnell hat weder den Click noch den Dongle am Lager. Wer das DK
dort und den Click bei Mouser kauft, zahlt zweimal Versand und bleibt bei
Mouser unter 50 € — deshalb ist alles-bei-Mouser voraussichtlich günstiger.

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
| Mouser | 85,93 € | `ungeprüft` | 85,93 € + Versand |
| Amazon | 22,98 € | 0 € zugesagt, Bedingungen `ungeprüft` | 22,98 € |
| BerryBase | 11,00 € | 4,95 € | 15,95 € |
| **Zusammen** | **119,91 €** | | **ca. 125 €** |

Gegenüber der alten Schätzung von „ca. 135 €" also rund 10 € weniger — und das
mit geprüften Zahlen statt Momentaufnahmen, obwohl ein Posten (der Dongle) um
15 € billiger geworden ist und dafür zwei Kabel und eine Stiftleiste
dazugekommen sind.

**Unsicher bleibt genau eines:** ob Mouser Zoll und Einfuhrumsatzsteuer schon
im Preis trägt. Schlägt beides obendrauf, liegt die Bestellung bei rund 140 €
statt 125 €. Thomas sieht die Zahl an der Kasse, bevor er bestellt.

## Quellen

Alle am **23.10.2026** abgerufen.

- Mouser DE: [949-NRF52840-DK](https://www.mouser.de/ProductDetail/949-NRF52840-DK) ·
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
