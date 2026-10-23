# Teileliste

Stand 23.10.2026. Alle Zahlen stammen aus den unten genannten Datenblättern, nicht
aus dem Gedächtnis. Preise und Lagerbestände sind Momentaufnahmen und mit
`ungeprueft` markiert, wo sie nicht direkt beim Händler bestätigt wurden.

Zwei Spalten: **Teststand** ist, was jetzt zu bestellen ist, um die Technik zu
beweisen. **Zielplatine** ist, was später auf die eigene Platine kommt.

## Übersicht

| Baustein | Teststand (jetzt) | Zielplatine (später) |
| --- | --- | --- |
| Rechenkern | nRF52840-DK (PCA10056) | Raytac MDBT50Q-1MV2 (Modul, vorzertifiziert) |
| Sicherheitschip | MIKROE-5392 "SE051 Plug & Trust Click" | SE051C2HQ1/Z01XDZ (HX2QFN20, 3 × 3 mm) |
| Flash | auf dem DK vorhanden | **W25Q512JVEIQ** (3 V, 512 Mbit = 64 MByte, WSON8) |
| OLED | Crystalfontz **CFAL12864N-A-B4** (0,96") | **CFAL4864A-071BW** (0,71", 13,9 × 22 × 1,26 mm) |
| Fingerabdruck | — | **nicht bestückt** (Stufe 1 ohne Sensor, Stiftfeld J5 vorgesehen) |
| Laderegler | — (DK läuft am USB) | **MCP73832T-2ACI/OT** + diskrete Lastumschaltung |
| Akkuschutz | — | **keiner** — Zelle mit PCM + Verpolschutz |
| Akku | — | **Adafruit 1317** (150 mAh) oder BerryBase **LP-401230-J** (100 mAh) |
| USB-C | auf dem DK vorhanden | **GCT USB4085-GF-A** + 2 × 5,1 kΩ + USBLC6-2SC6 |
| NFC-Antenne | **liegt dem DK bei** | als Leiterbahn auf der Platine |
| Taster | auf dem DK vorhanden | **Panasonic EVQP7A01P** (1,35 mm hoch, 2,2 N) |
| LED | auf dem DK vorhanden | **Everlight 19-337C** (1,6 × 1,6 × 0,5 mm) |
| Messmittel | Logikanalysator 8 Kanal USB | — |

Berichtigt gegenüber dem Entwurf: dem nRF52840-DK **liegt eine NFC-Antenne bei**
(Nordic, Kit-Inhalt) — der Posten „NFC-Antenne, weil das DK keine hat" entfällt.

## Sicherheitschip: SE051C2 statt SE050C2

Das war die wichtigste offene Frage des Entwurfs. Sie ist jetzt im Datenblatt
geklärt — mit einem anderen Ergebnis als gedacht.

**Gefordert:** secp256k1 (für die Krypto-Geldbörse) **und** Ed25519 (für SSH und
moderne Signaturen) **und** RSA (für PIV und OpenPGP, die beide RSA-2048 erwarten).

### Welche Variante kann was

Aus **AN12436 "SE050 configurations", Rev. 2.4, 8. Juli 2024, Tabelle 1**. Die
Spalten sind zeichengenau gegen die Kopfzeile geprüft, nicht geschätzt.
secp256k1 steckt in der Zeile "Koblitz" — secp256k1 ist eine Koblitz-Kurve.

| Fähigkeit | SE050E2 | SE050F2 | SE050A1/A2/D2 | SE050B1/B2 | SE050C1/C2 |
| --- | --- | --- | --- | --- | --- |
| Koblitz 160–256 bit (= secp256k1) | ja | ja (≥ 224 bit) | ja | — | ja |
| Twisted Edwards (Ed25519) | ja | — | — | — | ja |
| EdDSA | ja | — | — | — | ja |
| RSA bis 4096 bit | **—** | ja (≥ 2048, kein RSA plain) | — | ja | ja |
| Curve448, AES-CCM/GCM, GMAC | ja | — | — | — | — |

**Damit ist SE050E2 ausgeschieden: die E-Variante hat überhaupt kein RSA.** Der
Entwurf hielt die E-Variante für die bessere, weil sie neuer ist. Sie ist für
diesen Zweck die falsche. Nur die **C-Variante** hat alle drei Pflichtpunkte.

### Warum dann trotzdem nicht SE050C2

Zwei Gründe, beide nachprüfbar:

1. **Lieferbarkeit.** `SE050C2HQ1/Z01SDZ` wird von Beschaffungsportalen als
   "Not Recommended for New Designs" geführt (Findchips, `ungeprueft` — ein
   Aggregator, nicht NXP selbst; vor der Bestellung bei NXP bestätigen lassen).
   Bestand bei den großen Händlern dünn: Arrow 169 Stück zu 5,17 USD, Newark
   Bestand 0 (beide `ungeprueft`, über Octopart gelesen). Bei Mouser war kein
   Eintrag für die genaue Bestellnummer auffindbar.
2. **Der Nachfolger kann dasselbe und ist überall lagernd.**

### SE051C2: dieselben Kurven, mehr Speicher, lieferbar

Aus **AN12973 "SE051 configurations", Rev. 2.0, 8. Juli 2024, Tabelle 3**:

| Fähigkeit | SE051A2 | SE051C2 |
| --- | --- | --- |
| Koblitz 160–256 bit (= secp256k1) | ja | **ja** |
| Twisted Edwards (Ed25519) | — | **ja** |
| Montgomery (Curve25519) | — | ja |
| RSA bis 4096 bit | — | **ja** |
| Nutzspeicher, nichtflüchtig | 46–104 kB | **46–104 kB** |
| RAM (beim Abwählen gelöscht) | 608 Byte | 608 Byte |

**SE051C2 erfüllt alle drei Pflichtpunkte.** Beim Speicher ist die Tabelle
genauer zu lesen, als es dieser Abschnitt zunächst tat: sie nennt „46 kB to
104 kB", mit eigenen Zeilen für „Full Feature" (46 kB) und „Maximum" (104 kB).
Das ist eine **Bestellausführung**, keine Eigenschaft der Variante — welche die
Teststand-Platine trägt, wird vor der Bestellung geklärt. Mindestens sind es
46 kB und damit fast das Doppelte des SE050
— also doppelt so viel Platz für Schlüssel. Das Gehäuse ist dasselbe: HX2QFN20,
3 × 3 mm × 0,32 mm, 0,4 mm Raster, SOT1969-1 (SE051 Datenblatt Rev. 2.0,
Abschnitt 8). Die Platine muss für den Wechsel nicht geändert werden.

Der SE051 ist laut NXP eine "updatable extension" des SE050 — das Applet kann
im Feld aktualisiert werden. Für ein Gerät, das Jahre im Schlüsselbund liegt,
ist das ein Vorteil, kein Risiko: Aktualisierungen sind signiert.

**Entscheidung: `SE051C2HQ1/Z01XDZ` (12NC 935414457472).**
Quelle der Bestellnummer: SE051 Datenblatt Rev. 2.0, Tabelle 2.
Mouser: 18 301 Stück zu 5,84 USD bei Menge 1 (`ungeprueft`, Momentaufnahme).

### Stromaufnahme des Sicherheitschips

Aus **SE051 Datenblatt Rev. 2.0, Tabelle 11** (VSS = 0 V, −40 °C bis 105 °C):

| Zustand | typisch | maximal |
| --- | --- | --- |
| Versorgungsspannung | 1,8 V | 1,62–3,6 V |
| während der Übertragung | 3 mA | 3,7 mA |
| während symmetrischer Krypto | 6,5 mA | 7,5 mA |
| während asymmetrischer Krypto (Signatur) | **14,4 mA** | 16,5 mA |
| Power-down (Zustand bleibt) | 430 µA | 480 µA |
| **Deep Power-down (Zustand weg)** | **3 µA** | 5 µA |

Wichtig für das Energiebudget: 430 µA im Power-down sind für ein Gerät am Akku
**viel zu viel**. Der Chip muss über den ENA-Pin in Deep Power-down (3 µA)
gelegt werden, wenn nichts zu tun ist. Dafür muss laut Datenblatt Abschnitt 5.2
`Vin` versorgt und `VCC` aus `Vout` gespeist werden — das ist eine
Beschaltungsvorgabe, nicht nur Software. Siehe `hardware/schematic.md`.

### Was das für die Architektur bedeutet

`docs/architecture.md` begründet "Schlüssel werden abgeleitet, nicht gespeichert"
mit den 50 kB des SE050. Mit 46 bis 104 kB ist der Druck geringer. Die Ableitung bleibt
trotzdem richtig — sie ist der Grund, warum ein Zweitgerät möglich ist. Nur das
Argument "sonst geht der Speicher aus" ist jetzt schwächer. Das gehört dem
Architektur-Strang, nicht mir; ich melde es nur.

### Sicherheitshinweis: die Werks-SCP03-Schlüssel sind veröffentlicht

**AN12973 Rev. 2.0, Tabelle 6 "Default Platform SCP keys"** nennt die
ENC-, MAC- und DEK-Schlüssel jeder Variante im Klartext, je nach Fertigungslos
(OEF-Kennung A8FA bzw. A564 für SE051C2). Wer den Chip mit Werkseinstellung
verbaut, hat **keine** verschlüsselte I2C-Verbindung im Sinne des
Bedrohungsmodells — jeder, der das Dokument liest, kann mitlesen.

**Pflicht bei der Erstinbetriebnahme: die Plattform-SCP03-Schlüssel ersetzen.**
`docs/threat-model.md` behauptet "Die I2C-Leitung zwischen den beiden Chips ist
verschlüsselt (SCP03)". Das ist erst nach diesem Schritt wahr. Gehört dem
Firmware- und Bedrohungsmodell-Strang; gemeldet.

### Teststand-Platine für den Sicherheitschip

| Option | Chip | Preis | Bezug | Bewertung |
| --- | --- | --- | --- | --- |
| **MIKROE-5392 "SE051 Plug & Trust Click"** | **SE051C2** | ~25 USD | Mouser 932-MIKROE-5392 | **empfohlen** — genau der Zielchip, I2C, 3,3 V |
| OM-SE050ARD-E | SE050E2 | 60 USD bei NXP | NXP direkt, 48 Stück | falsche Variante: kein RSA |
| OM-SE050ARD | SE050C | — | laut Entwurf abgekündigt (`ungeprueft`) | richtige Kurven, aber alter Chip |
| MIKROE Plug&Trust Click | SE050C | — | RS Online 2475657 | alter Chip |

Der Entwurf empfahl `OM-SE050ARD-E`, weil `OM-SE050ARD` abgekündigt sei. Beide
sind die falsche Wahl: das -E-Kit trägt die E-Variante ohne RSA. **Richtig ist
MIKROE-5392** — es trägt den Zielchip SE051C2 und kostet weniger als die Hälfte.

Alle Preise und Bestände in diesem Abschnitt sind Momentaufnahmen aus dem
Suchindex und **ungeprueft**; vor der Bestellung die Händlerseite ansehen.

## Fingerabdrucksensor — kein Kandidat erfüllt alles

Das war der schwerste Punkt. **Entschieden: Stufe 1 ohne Sensor** (Begründung
am Ende des Abschnitts). Die Recherche steht hier vollständig, weil Stufe 2
sie braucht. Gefordert waren vier Dinge:

1. Der Sensor rechnet den Vergleich **selbst** und liefert nur ja/nein.
2. In Einzelstücken bei einem echten Händler zu bekommen.
3. Beschreibung ohne Geheimhaltungsvertrag.
4. Klein genug für einen Schlüsselanhänger.

**Kein geprüfter Kandidat erfüllt alle vier.** Zwei kommen nah heran.

| Teil | Vergleich im Sensor | Kleinmenge | ohne NDA | Baugröße | Urteil |
| --- | --- | --- | --- | --- | --- |
| **FPC BM-Lite** | ja (belegt) | ja (Mouser, Digikey, Future) | **teilweise** | ja | **erste Wahl** |
| **Grow R503-M22** | ja (belegt) | ja (Adafruit) | ja | **grenzwertig** | zweite Wahl |
| FPC1020 / FPC1021 | **nein** — reiner Bildsensor | Graumarkt | ja | ja | fällt an Punkt 1 |
| Synaptics FS7600 (Match-in-Sensor) | ja | **nein** | **nein** | ungeprueft | nicht beschaffbar |
| Goodix, Egis, Silead, Chipone | ungeprueft | **nein** | **nein** | ungeprueft | nicht beschaffbar |
| Sensor im YubiKey Bio | ungeprueft | ungeprueft | — | — | Hersteller **nicht belegt** |
| ZhianTec ZFM-20 | ja | abgekündigt | ja | ungeprueft | 120 mA — zu viel |

### FPC BM-Lite (erste Wahl)

| Eigenschaft | Wert | Quelle |
| --- | --- | --- |
| Vergleich im Modul | ja — "local match engine", Vorlagen bleiben im Modul | FPC Produktseite |
| Schnittstelle | SPI **oder** UART | FPC Produktblatt 2020 |
| Spannung | 3,3 V | FPC Produktblatt 2020 |
| Strom bei Aufnahme | 12 mA | FPC Produktblatt 2020 |
| Strom im Schlaf | 13 µA | FPC Produktblatt 2020 |
| Abmessungen | **15 × 15 × 2,8 mm** | FPC Produktblatt; bestätigt im FPC-Vortrag 2023 |
| Auflösung | 160 × 160 Punkte bei 508 dpi | FPC Produktblatt 2020 |
| Vorlagen | 50 | FPC Produktseite |
| Erkennungszeit | 0,4 s | FPC Produktblatt 2020 |
| Preis Menge 1 | 29,00 USD (Mouser) / 23,20 USD (Digikey) | Händlerseiten, `ungeprueft` |
| Bestellnummer | Mouser 252-100018754, Hersteller 100018754 | Mouser |
| Entwicklungskit | Hersteller 100022424, ca. 160–178 USD | Future Electronics |

**Der Haken:** Mouser nennt 13 mA aktiv, das Produktblatt 12 mA — ein
Widerspruch, beide `ungeprueft`. Und das Produktblatt liegt nicht mehr unter
seiner alten Adresse (404); der Inhalt ist nur über den Suchindex zu bekommen.
Vor allem aber trägt der **Development Guide** (Dok. 100021008 Rev. 7.2,
April 2020), der den Host-Befehlssatz beschreibt, den Vermerk
"proprietary to Fingerprint Cards AB" — **Punkt 3 ist also nur halb erfüllt.**

**Vor dem Kauf zu klären (Frage an den Hersteller):** Kommt der Befehlssatz ohne
Unterschrift? Praktischer Umweg: das Entwicklungskit bringt ein Python-Beispiel
mit, aus dem sich das Protokoll ablesen lässt — aber was daraus entsteht, dürfte
sich nicht offen veröffentlichen lassen. **Das ist für ein MIT-Projekt ein
echtes Problem und keine Formalie.**

### Grow R503-M22 (zweite Wahl)

| Eigenschaft | Wert | Quelle |
| --- | --- | --- |
| Vergleich im Modul | ja — eigener DSP, Befehle 0x31 Auto-Enroll, 0x32 Auto-Identify | Handbuch V1.4.1, 09/2023 |
| Schnittstelle | UART TTL, Vorgabe 57600 Baud | Handbuch V1.4.1 |
| Spannung | 3,3 V | Adafruit |
| Strom aktiv / Bereitschaft | 20 mA / **2 µA** (Berührungs-Bereitschaft) | Adafruit |
| Abmessungen R503 | Ø 28 mm, **Höhe 15,5 mm**, 35 g | Adafruit |
| Abmessungen M22 | Ø 25 / 22 mm, Höhe 15 mm | Hubtronics |
| Vorlagen | 200 | Adafruit |
| Preis | 39,95 USD (R503 bei Adafruit 4651) | Adafruit, `ungeprueft` |

**Vorteil:** Das Protokoll ist vollständig und frei dokumentiert — genau das, was
ein offenes Projekt braucht. 2 µA Bereitschaft ist hervorragend.

**Nachteile, beide ernst:**

- **15 mm Bauhöhe** bei 25 mm Durchmesser. Das ist für einen Schlüsselanhänger
  zu dick; es wäre ein Gerät in Streichholzschachtelgröße.
- Das Modul liefert **auf Befehl auch Rohbilder** an den Host. Der
  Sicherheitsgewinn gegenüber "Host rechnet" ist damit nur ein Verzicht in der
  Firmware, keine Sperre in der Hardware. Für `docs/threat-model.md` heißt das:
  eine veränderte Firmware kann Fingerabdruckbilder auslesen.
- Keine belegte Lebenderkennung.

### Was die offenen Projekte tun: sie lassen es weg

| Projekt | Fingerabdruck |
| --- | --- |
| Nitrokey 3 / 3C / 3 Mini | **nein** — nur Taster |
| SoloKeys Solo2, Somu | **nein** |
| Google OpenSK | **nein** |
| Trussed (Rahmenwerk) | keine Unterstützung belegt |

**Kein offenes FIDO-Projekt hat bisher einen Fingerabdrucksensor eingebaut.** Es
gibt also keinen erprobten Sensor, auf den man aufsetzen kann — wir wären die
Ersten. Das ist der eigentliche Grund, warum dieser Punkt schwer ist.

Token2 PIN+ Bio3 und Kensington VeriMark Guard haben einen Sensor, nennen den
Hersteller aber nicht. Beim YubiKey Bio ließ sich kein Teardown finden, der den
Chip benennt; Yubico spricht nur von einer "Drei-Chip-Architektur".

### Entschieden: Stufe 1 ohne Sensor

**Der Freigabeweg ist Taster + PIN. In Stufe 1 wird kein Fingerabdrucksensor
bestückt.**

**Der tragende Grund: Bei FIDO2 ersetzt die Biometrie die PIN — sie ist kein
zusätzlicher Faktor.** Das Gerät wird durch einen Sensor nicht sicherer,
sondern bequemer. Es gibt keine Stelle im Protokoll, an der Finger **und** PIN
zusammen gefordert werden. Der Verzicht ist darum keine Sparmaßnahme.

Dazu:

- Taster + PIN ist der Weg jedes offenen Projekts und zertifizierbar.
- Es kostet keinen Platz, keinen Strom und keine Angriffsfläche.
- Die Beschaffungslage ist ungelöst: Das aussichtsreichste Teil (BM-Lite) hat
  keinen offenen Befehlssatz, das offene Teil (R503-M22) ist 15 mm dick und
  gibt Rohbilder heraus.

**Die Platine sieht den Sensor trotzdem vor:** vier Leitungen plus Versorgung
auf dem Stiftfeld J5, verlegt aber nicht bestückt. **Stufe 2 kann nachrüsten,
ohne dass eine neue Platine nötig wird.**

**Nicht bestellen:** das BM-Lite-Entwicklungskit (160–178 USD) — in Stufe 1
wird es nicht gebraucht.

## Flash: W25Q512JVEIQ (3 V), nicht die 1,8-V-Fassung

Quelle: **Winbond W25Q512JV, Revision B, 25. Juni 2019**.

| Größe | Wert | Abschnitt |
| --- | --- | --- |
| Versorgung | **2,7–3,6 V** | Kap. 1 |
| Standby ICC1 | typ. 10 µA, max. 60 µA | Kap. 9.4 |
| **Deep Power Down ICC2** | **typ. 1 µA**, max. 20 µA | Kap. 9.4 |
| Lesestrom bei 50 MHz | typ. 20 mA, max. 35 mA | Kap. 9.4 |
| Lesestrom bei 104 MHz | typ. 25 mA, max. 40 mA | Kap. 9.4 |
| Schreiben (Page Program) | typ. 20 mA, max. 25 mA | Kap. 9.4 |
| Löschen (Sector/Block) | typ. 20 mA, max. 25 mA | Kap. 9.4 |
| Takt | 133 MHz bei 3,0–3,6 V | Kap. 9.6 |

**Warum die JV und nicht die NW:** Der nRF52840 arbeitet mit VDD 1,7–3,6 V
(nominal 3,0 V); seine GPIO-Pegel richten sich nach VDD. Bei 3,3 V passt das
direkt zu den 2,7–3,6 V der JV — **kein Pegelwandler**. Die NW (1,65–1,95 V)
würde eine 1,8-V-Schiene erzwingen, was Sicherheitschip und OLED auf der
3,3-V-Schiene ausschließt. Vom NW-Datenblatt war außerdem **keine Fassung
beschaffbar** (Winbond-Seite: „part no. is not available"); alle Ströme des NW
sind `ungeprueft`.

**Wichtig fürs Energiebudget:** Der QSPI des nRF52840 deckelt bei **32 MHz**
(SCK = 32 MHz/(SCKFREQ+1), SCKFREQ 0–15). Die 133 MHz der JV sind also nie das
Nadelöhr. Einen Datenblattwert für 32 MHz gibt es nicht (`ungeprueft`, nicht
interpoliert) — gerechnet wird mit den 20 mA der 50-MHz-Spalte.

Und: der Flash muss per Befehl `B9h` **schlafen gelegt** werden (1 µA), nicht
nur über /CS abgewählt (das wären 10 µA). Das ist eine Firmware-Pflicht.

| Händler | Bestellnummer | Preis 1 St. | Bestand |
| --- | --- | --- | --- |
| Digi-Key | 256-W25Q512JVEIQ-TUBE-ND | 21,19 USD | **nicht angezeigt**, Ersatztyp vorgeschlagen |

**Offen:** Die Lieferbarkeit ist unbestätigt — Digi-Key zeigt für diese Nummer
keinen Bestand und schlägt einen Ersatztyp vor. Mouser war während der
Recherche nicht erreichbar, LCSC-Preise nicht abrufbar. **Vor der Bestellung
klären.** WSON8 hat außerdem **keinen /RESET-Pin**.

## Laden: MCP73832, nicht MCP73831

Quelle: **Microchip DS20001984H**, MCP73831/2, Rev. H.

| Größe | Wert | Abschnitt |
| --- | --- | --- |
| Ladestrom | **I = 1000 V / R_PROG** | 5.1.2, S. 15 |
| R_PROG-Bereich | 2 kΩ … 67 kΩ | DC-Tabelle S. 4 |
| Bereich | 15 mA … 500 mA | S. 1 |
| **Rückentladung aus dem Akku ohne USB** | **< 2 µA** | 4.1, 4.3, S. 13 |
| Ruhestrom, Ladung beendet | max. 200 µA | S. 3 |
| PROG offen (aus) | max. 5 µA | S. 3 |
| Gehäuse | SOT-23-5 (`/OT`), 0,95 mm Raster | S. 1 |

**Ladeschlussspannung über die Typnummer:** `-2` = **4,20 V** (richtig für eine
normale LiPo), `-3` = 4,35 V, `-4` = 4,40 V, `-5` = 4,50 V.
**Achtung: `-3`/`-4`/`-5` überladen eine normale 4,2-V-Zelle.**

**Bestellnummer: `MCP73832T-2ACI/OT`** — Digi-Key DE `MCP73832T-2ACI/OTCT-ND`,
**0,67 € netto**. Mit R_PROG = 10 kΩ → 100 mA, 20 kΩ → 50 mA. Für eine
100–150-mAh-Zelle ist 50–100 mA richtig.

**Warum der '32 und nicht der '31:** Sie unterscheiden sich nur am STAT-Pin.
Beim '31 (Tri-State) heißt „fertig" High, beim '32 (Open-Drain) hochohmig — also
wie „kein USB". Der '31 trennt die Fälle, aber ein Tri-State-Pegel ist an einem
nRF52840-GPIO ohne Pull nicht zuverlässig zu lesen. **Saubere Lösung: STAT am
'32 mit Pull-up lesen und VBUS getrennt erfassen** — dann braucht es die
Unterscheidung nicht.

### Power-Path: diskret, nicht als IC

Der Wunsch „am USB laufen, ohne den Akku zu belasten" heißt Power-Path. Vier
ICs wurden geprüft:

| | MCP73832 | BQ24040 | BQ25100 | BQ24075 | MCP73871 |
| --- | --- | --- | --- | --- | --- |
| echter Power-Path | nein | **nein** | **nein** | **ja** | ja |
| Gehäuse | **SOT-23-5** | WSON-10 | DSBGA 1,6×0,9 | VQFN-16 + Pad | QFN-20 + Pad |
| handlötbar | **ja** | nein | nein | nein | nein |
| Akkuruhestrom | < 2 µA | ~1 µA | 75 nA | 4,3 µA | 30–40 µA |
| Thermistor nötig | **nein** | ja | ja | ja | ja |
| Preis Digi-Key DE | **0,67 €** | 0,83 € (20 Wochen) | 1,83 € | 2,13 € | 2,12 € |

**Zwei Befunde:** BQ24040 und BQ25100 haben laut ihren eigenen Datenblättern
**gar keinen** Power-Path — die Last hängt parallel zum Akku. Echtes DPPM haben
nur BQ24075 und MCP73871, und **beide brauchen Reflow** (Wärmepad muss Kontakt
haben). Handlötbar ist allein der MCP73832 im SOT-23-5.

**Empfehlung:** MCP73832 + Lastumschaltung aus zwei P-Kanal-MOSFETs
(Ideal-Diode-Paar): liegt VBUS an, speist USB die Last und der Akkupfad sperrt.
Zwei handlötbare SOT-23-Teile. Bei unserem Lastprofil (Milliampere, nicht
Hunderte) ist das billiger und kleiner als ein Power-Path-IC.

**`ungeprueft` und offen:** Diese Umschaltung ist **nicht durchgerechnet** —
keine Bauteilauswahl, kein belegter Spannungsabfall, und kein Nachweis, dass
der Lastsprung beim Umschalten die Brownout-Erkennung des nRF52840 nicht
auslöst. Soll es doch ein IC sein, dann **BQ24075** (nicht MCP73871): echtes
DPPM, 4,3 µA statt 30 µA, 3 × 3 statt 4 × 4 mm, fast gleicher Preis.

## Akkuschutz: keiner auf der Platine — aber Verpolschutz

Der Entwurf nannte `DW01A` + `FS8205`. Das ist für dieses Gerät die falsche
Wahl, und zwar aus einem einfachen Grund: **die käufliche Zelle hat den Schutz
schon eingebaut.**

| Zelle | Schutz | Belegte Schwellen |
| --- | --- | --- |
| Adafruit 1317 (150 mAh) | **ja** | Abschaltung bei 3,0 V, Kurzschlussschutz |
| SparkFun 400 mAh | **ja** | keine Zahlen genannt |
| EEMB LP502030-**PCM**-LD | **ja** | Überladung 4,28 ± 0,05 V, Tiefentladung 3,0 ± 0,10 V, Dauerstrom 0,3 A, Spitze 0,8 A / 3 s |

Ein zweites `DW01A` + `FS8205` in Serie kostet Platz, zwei weitere
Durchgangswiderstände im Strompfad und eine weitere Fehlerquelle. Dazu kommt:
die Überstromschwelle des DW01A ist eine **Spannung über die MOSFETs**
(200 mV typ.); mit dem FS8205A (2 × 21 mΩ) sind das rund **4,8 A** (gerechnet,
keine Datenblattangabe) — für ein Milliampere-Gerät also gar kein
Überstromschutz, nur Kurzschlussschutz.

**Was die Zell-PCM aber nicht kann: Verpolschutz.** Eine verpolt angelötete
Zelle legt die Spannung direkt auf V_BAT des Ladereglers und auf den
Sicherheitschip. Dagegen hilft nur eine Maßnahme auf der Platine.

**Entscheidung:**

1. Zelle **mit belegtem Schutz** beschaffen (Modellname muss `PCM` enthalten —
   EEMB führt dieselbe Bauform **auch ohne**!).
2. **Kein zweites Schutz-IC** auf der Platine.
3. **Verpolschutz am Akkueingang** (Serien-MOSFET) — Pflicht.
4. **Firmware-Abschaltschwelle oberhalb der PCM-Schwelle** der verbauten Zelle,
   sonst schaltet die PCM mitten in einer Signatur ab. Bei Adafruit 1317 liegt
   die PCM bei 3,0 V; die Firmware muss vorher aufhören.

Soll die Platine für **beliebige, auch nackte** Zellen taugen, dann **S-8261**
(nicht DW01A): Schwellen in 5-mV-Schritten einstellbar, ±25 mV statt ±50 mV,
0,1 µA statt 1,5 µA im Tiefentladezustand, gleiches SOT-23-6. **Aber:** bei
Digi-Key nur als 3000er-Rolle mit 16 Wochen Lieferzeit — **Einzelbezug offen**.
Ein Schutz-IC **mit** eingebauten MOSFETs wurde nicht gefunden; S-8261, BQ2970
und die Ricoh-Teile brauchen alle ein externes Dual-FET.

## Akku

Fünf Angaben des Entwurfs waren falsch und sind berichtigt: Adafruit 4236 hat
**420 mAh** (nicht 105), Adafruit 3898 ist 7,8 mm **zu dick**, SparkFun
PRT-13852 ist die **40-mAh**-Zelle, PRT-10718 leitet auf die 400-mAh-Zelle, und
EEMB LP401230 hat **100 mAh** (nicht 120).

| # | Zelle | Bestellnr. | Preis | Kapazität | Dicke × Breite × Länge | Anschluss | Schutz |
| --- | --- | --- | --- | --- | --- | --- | --- |
| **1** | **Adafruit LiPo 150 mAh** | **1317** | **5,95 USD** | 150 mAh | **3,8 × 19,75 × 26,02 mm** | JST-PH 2.0 | **ja** |
| **2** | **BerryBase PKCELL LP-401230** | **LP-401230-J** | **3,40 €** | 100 mAh | **4,05 × 12,0 × 30,5 mm** | JST-PH 2.0 | **ja** |
| 3 | Adafruit LiPo 100 mAh | 1570 | 5,95 USD | 100 mAh | 3,8 × 11,5 × 31 mm | JST-PH 2.0 | ja |
| 4 | SparkFun Li-Ion 110 mAh | PRT-13853 | 7,53 USD | 110 mAh | max 4,0 × 15,0 × 25,0 mm | JST-PH 2.0 | ja |
| 5 | EEMB LP502030-PCM-LD | — | `ungeprueft` | 250 mAh | max 5,3 × 20,5 × 31 mm | **blanke Lötfahnen** | ja |

**Empfehlung: Adafruit 1317 (150 mAh).** Mit 3,8 mm die dünnste belegte Zelle,
und die einzige mit vollständigem Herstellerdatenblatt (PKCELL QA.S.0228,
Ausgabe A, 28.07.2014): 1 C = 150 mA Dauerentladung und > 300 Zyklen sind
belegt, Ladestrom höchstens 150 mA, empfohlen 100 mA.

**Für EU-Beschaffung: BerryBase LP-401230-J (100 mAh, 3,40 €).** Der schmale
12-mm-Streifen passt unter eine Anhängerplatine, wo die 20-mm-Zellen nicht
hinkommen. EU-Händler heißt außerdem: kein Luftfracht- und Zollthema.

**Nicht empfohlen:** die EEMB mit 250 mAh — 5,3 mm und blanke Lötfahnen. Sie
hat das beste Datenblatt und die beste Zyklenzahl; sie ist die Wahl, falls
250 mAh wichtiger werden als 4 mm Bauhöhe.

**Offen und wichtig:**

- **Der Pulsstrom ist bei keiner einzigen Zelle belegt.** Kein Datenblatt nennt
  einen. Für ein Gerät, das beim Funken Stromstöße zieht, ist das die größte
  Lücke. Siehe `power-budget.md`.
- **Die Selbstentladung pro Monat ist bei keiner Zelle belegt** — beide
  gelesenen Datenblätter nennen nur „Kapazitätserhalt nach 28 Tagen". Die
  Laufzeitrechnung muss daher mit einer Annahme arbeiten.
- **Keine Zelle hat einen Thermistor** (Adafruit sagt das ausdrücklich). Darum
  passt der MCP73832 ohne TS-Pin, und die Power-Path-ICs mit NTC-Eingang nicht.
- Versand: einzelne Zellen sind **UN3480** und seit 01.01.2022 **nur als
  Luftfracht, ≤ 30 % Ladezustand** erlaubt. Kauf bei einem EU-Händler umgeht
  das ganz.

## USB-C

### Die Widerstände

| Punkt | Wert |
| --- | --- |
| Rd je CC-Leitung | **5,1 kΩ** |
| **Je CC-Pin ein eigener Widerstand** | Pflicht (Type-C Spec 1.4, Abschn. 4.5.1.3.2, Fig. 4-9 — normativ) |

**5,1 kΩ je CC ist richtig — aber es müssen zwei getrennte sein.** Der
Raspberry Pi 4 hat beide CC-Pins auf **einen** Widerstand gelegt; Folge: mit
einem e-marked Kabel (SuperSpeed oder 5 A) **lädt das Gerät nicht**, weil der
Lader den Ra des e-markers (800 Ω–1,2 kΩ) nicht mehr vom Rd trennen kann und
0 V statt 5 V liefert. Das ist der klassische Fehler, und er ist gratis zu
vermeiden.

**Ein reiner Rd-Sink ohne PD darf nur ziehen, was die Quelle per Rp anbietet**,
und muss dazu die CC-Spannung messen. Wer nicht messen will, muss mit
„Default USB Power" rechnen.

`ungeprueft`: die Rd-Toleranz (keine erreichbare Quelle nennt eine — die
kursierenden ±20 %/±5 % gelten für Rp auf der Quellenseite), die genauen
Stromgrenzen ohne PD, 22 Ω in D+/D− (keine Quelle fordert sie für USB 2.0), und
Abschnitt/Tabelle in der aktuellen Spezifikation (Release 2.5, 08.04.2026 — PDF
nur per Download). Belegt ist Release 1.4.

### Buchse oder Stecker

| Teil | Art | Preis | Bestellnr. |
| --- | --- | --- | --- |
| **GCT USB4085-GF-A** | USB 2.0 Buchse, **Durchsteck** | **0,80 €** | Digi-Key 2073-USB4085-GF-ACT-ND, 84 113 am Lager |
| GCT USB4105 | gleiche Elektrik, **SMT** | — | — |
| Molex 1054440001 | **Stecker**, Straddle Mount am Platinenrand | 2,72 € | Digi-Key, **14 Wochen** |

USB4085: 16 Durchsteckpins Ø 0,65 mm + 4 Haltestifte, 5 A VBUS, **20 000
Steckzyklen**, Körper 8,94 × 9,17 mm, Schirm = GND. Für Handlötung die klar
beste Wahl.

**Zum Platinenrand-Stecker** (wie bei bekannten FIDO-Sticks): machbar, aber
belegte Randbedingungen sind **Leiterplattendicke 0,8 mm** statt 1,6 mm (alle
gefundenen Edge-Mount-Stecker passen nur auf 0,8 mm) und **ENIG statt HASL**
(die dickere HASL-Schicht macht das erste Stecken schwer). Für einen direkt in
die Platine geätzten Stecker ohne Bauteil wurde **keine Quelle zur Konformität
mit der Type-C-Steckerspezifikation gefunden — `ungeprueft`.**

### ESD-Schutz

| Teil | Daten | Preis |
| --- | --- | --- |
| **ST USBLC6-2SC6** | 2 Datenleitungen + VBUS, **3,5 pF**, 150 nA Leckstrom, IEC 61000-4-2 Level 4 (±15 kV Luft, ±8 kV Kontakt), SOT-23-6 | ~0,23 € (RS, 10er-Pack) |
| Nexperia PRTR5V0U2X | **1 pF** — deutlich besser, SOT143B | ~0,38 USD |
| Nexperia PESD5V0X1BCL | VBUS, VRWM **5,5 V**, DFN1006 | `ungeprueft` |

Der USBLC6-2 klemmt VBUS über Pin 3 mit, ist dafür mit 5 A aber dünn — ein
eigenes VBUS-Teil ist sinnvoll. Dabei **VRWM 5,5 V** nehmen, nicht 5,0 V: ein
5-V-Teil mit Durchbruch ab 5,8 V liegt zu knapp an den USB-Toleranzen.

**Offen, und für einen Sicherheitsschlüssel wichtig:** Das **ESD-Konzept für
die CC-Leitungen und die Schirm-Anbindung der Buchse** (direkt an GND oder über
RC) wurde nicht recherchiert.

## OLED: 0,71 Zoll statt 0,96 Zoll

Der Entwurf nannte ein 0,96"-SSD1306-Modul. Für ein Anhängergerät gibt es ein
deutlich besser passendes Glas.

| Teil | Größe | Auflösung | Modulmaß | Preis | Bestand |
| --- | --- | --- | --- | --- | --- |
| **Crystalfontz CFAL4864A-071BW** | **0,71"** | 48 × 64 | **13,90 × 22,00 × 1,26 mm**, aktiv 10,54 × 14,70 mm | **4,59 USD** | **914 St.** |
| Crystalfontz CFAL12864N-A-B4 | 0,96" | 128 × 64 | 26,70 × 19,26 mm | 7,51 USD | 176 St. |
| Adafruit 326 | 0,96" | 128 × 64 | 29,2 × 26,7 × **6,2 mm** | 17,50 USD | — |

**Empfehlung Zielplatine: CFAL4864A-071BW.** 1,26 mm dick, Controller
**SSD1306 namentlich genannt**, 3,3-V-Logik, SPI und I2C, Anschluss über
FPC mit 12-poligem ZIF-Steckverbinder, 0,5 mm Raster. Jede Zahl von der
Herstellerseite gelesen.

**Empfehlung Teststand: CFAL12864N-A-B4** statt Adafruit 326 — das
Adafruit-Board ist mit 6,2 mm und 5-V-Regler plus Aufwärtswandler für einen
Schlüssel zu dick, und Adafruit **nennt den Controller nicht einmal** (es kann
ein SSD1315 sein).

**SH1106 nicht nehmen:** 132 statt 128 Spalten heißt Offset-Gefummel im
Treiber, und er braucht mit VDD1 + VDD2 + VPP eine Schiene mehr.

**Wichtig für das Energiebudget:** Die oft zitierten 50 µA / 430 µA aus dem
SSD1306-Datenblatt (Rev 1.1, April 2008, Tab. 12-1) sind die **IC-Ströme bei
extern angelegten 12 V ohne Panel-Last** — nicht der Modulstrom. Der reale
Verbrauch entsteht fast vollständig im Ladungspumpen- und Panelzweig.
Crystalfontz nennt für das 0,96"-Glas 16 mA typisch, für das 0,71" max. 25 mA.
Sleep ist bei beiden Controllern max. 10 µA.

**Offen:** Der Modulstrom eines 0,71"-Glases bei 3,3 V mit aktiver
Ladungspumpe ist aus keiner gelesenen Quelle belegt. Das Energiebudget rechnet
darum mit den 25 mA der Herstellerseite als obere Grenze.

## Taster und LED

| Teil | Daten | Preis | Bestellnr. |
| --- | --- | --- | --- |
| **Panasonic EVQP7J01P** | 3,5 × 2,9 mm, **Höhe 1,35 mm**, seitlich betätigt, **1,6 N**, Hub 0,2 mm, 100 000 Zyklen, Prellzeit max. 10 ms | `ungeprueft` | — |
| Panasonic EVQP7A01P | gleich, aber **2,2 N** | **0,25 €** | Digi-Key P16763CT-ND |

Quelle: Panasonic-Katalog ANCTB21E, Juli 2025. **Von allen geprüften Tastern
erfüllen nur diese beiden die Vorgabe „unter 2 mm" mit belegtem Datenblatt.**
C&K PTS810 und Alps SKRPACE010 sind 2,5 mm hoch.

Die 1,6-N-Fassung ist angenehmer, die 2,2-N-Fassung macht versehentliches
Drücken im Hosensack unwahrscheinlicher. **Für einen Sicherheitsschlüssel, bei
dem ein Druck eine Signatur freigibt, ist 2,2 N die richtige Wahl** — und
`EVQP7A01P` ist zudem die mit belegtem Preis. Die 10 ms Prellzeit gehören in
die Firmware.

### RGB-LED: diskret, nicht adressierbar

| Teil | Daten |
| --- | --- |
| **Everlight 19-337C/RSBHGHC-A88/4T** | **1,6 × 1,6 × 0,5 mm**, 6 Pins = **drei getrennte LEDs** (weder gemeinsame Anode noch Kathode), VF rot 1,7–2,4 V, blau/grün 2,6–3,2 V bei 20 mA, IF max. 25 mA |

Quelle: Everlight DSE-0021229, Rev. 4, 08.11.2022.

**Keine WS2812B/SK6812.** Grund, belegt aus dem Worldsemi-Datenblatt
WS2812B-Mini-V3/W: **Ruhestrom < 0,6 mA im Dauerbetrieb** — allein davon wäre
eine 100-mAh-Zelle in etwa 7 Tagen leer (eigene Rechnung). Dazu braucht sie
mindestens 3,7 V, was eine halb entladene LiPo nicht mehr liefert, und ihr
3535-Gehäuse ist fast fünfmal so groß. Die diskrete LED hat **null**
Ruhestrom.

**Achtung beim Löten:** Der ESD-Schutz der Everlight ist bei blau und grün nur
**150 V** (rot 2000 V). Handlöten höchstens 350 °C / 3 s.

**Offen:** Blau und grün haben VF bis 3,2 V — an einer 3,3-V-Schiene ist das
knapp, mit entladener Zelle (3,0 V) reicht es nicht. Entweder mit reduziertem
Strom betreiben, oder rot als Hauptfarbe nutzen, oder einen Aufwärtswandler
prüfen. **Nicht gerechnet** — gehört in den Schaltplan-Feinschliff.
Der Einzelstückpreis ist `ungeprueft`.

## Was die Recherche nicht belegen konnte

Ehrlich gesagt, damit niemand auf einer Zahl aufbaut, die es nicht gibt:

- **Mouser war durchgehend nicht erreichbar.** In den Abschnitten Versorgung,
  USB-C, OLED, Taster und LED steht darum **keine Mouser-Nummer**. LCSC-Preise
  laden per JavaScript und waren nicht abrufbar; die LCSC-Nummern C424093,
  C38066, C165948, C7519, C12333 sind belegt, ihre **Preise nicht**.
- Ebenfalls gesperrt (403/Timeout): st.com, infineon.com, buydisplay.com,
  ckswitches.com, der Download der USB-Type-C-Spezifikation.
- **W25Q512NW: kein Datenblatt beschaffbar.** Alle Ströme, die WSON8-Belegung
  und die Bestellcode-Auflösung fehlen.
- **Der diskrete Power-Path ist nicht durchgerechnet.**
- **Der Pulsstrom ist bei keiner Zelle belegt**, die Selbstentladung bei keiner.
- **Die thermische Auslegung des MCP73832 ist nicht gerechnet** (~180 mW im
  SOT-23-5 bei 5 V / 3,2 V / 100 mA).
- `SE050C2`-Abkündigung und das „not recommended" für LCSC C424093 stammen von
  Aggregatoren, nicht von den Herstellern.

## Quellen

- SE050 Datenblatt, Rev. 3.8, 18. Oktober 2023 — https://www.nxp.com/docs/en/data-sheet/SE050-DATASHEET.pdf
- SE051 Datenblatt, Rev. 2.0, 8. Juli 2024 — https://www.nxp.com/docs/en/data-sheet/SE051.pdf
- AN12436 "SE050 configurations", Rev. 2.4, 8. Juli 2024 — https://www.nxp.com/docs/en/application-note/AN12436.pdf
- AN12973 "SE051 configurations", Rev. 2.0, 8. Juli 2024 — https://www.nxp.com/docs/en/application-note/AN12973.pdf
- AN12413 "SE050 APDU Specification", Rev. 2.12, 24. März 2021 — https://www.nxp.com/docs/en/application-note/AN12413.pdf
  (Kurven-Kennungen: Secp256k1 = 0x10, ID_ECC_ED_25519 = 0x40)
- FPC BM-Lite — https://www.fpc.com/products/fingerprints/fingerprint-systems/bm-lite/
- Grow R503 Handbuch V1.4.1, 09/2023; Adafruit 4651 — https://www.adafruit.com/product/4651
- Winbond W25Q512JV, Revision B, 25. Juni 2019 — https://www.winbond.com/resource-files/W25Q512JV%20SPI%20RevB%2006252019%20KMS.pdf
- Microchip DS20001984H, MCP73831/2, Rev. H — https://ww1.microchip.com/downloads/en/DeviceDoc/MCP73831-Family-Data-Sheet-DS20001984H.pdf
- TI BQ24075 SLUS810N (Okt. 2021), BQ24040 SLUS941H (Feb. 2021), BQ25100 SLUSBV8C
- Fortune DW01A+G-DS-10_EN, Rev. 1.0, Okt. 2013; FS8205A-DS-12_EN, Rev. 1.2, Aug. 2009
- ABLIC S-8261, Rev. 5.5_00 — https://www.ablic.com/en/doc/datasheet/battery_protection/S8261_E.pdf
- PKCELL „Li-Polymer 402025 150mAh 3.7V with PCM", QA.S.0228, Ausgabe A, 28.07.2014
- EEMB LP502030-PCM-LD, ZJQM-RD-SPC-A03249, Ver. 0.0, 15.12.2017
- Solomon Systech SSD1306, Rev 1.1, April 2008; SH1106 V2.2 (ohne Datum)
- Crystalfontz CFAL4864A-071BW — https://crystalfontz.com/product/cfal4864a071bw
- GCT USB4085, Zeichnung Rev. B, 28.02.2023 — https://gct.co/files/drawings/usb4085.pdf
- Panasonic Taster-Katalog ANCTB21E, Juli 2025
- Everlight 19-337C, DSE-0021229, Rev. 4, 08.11.2022
- Worldsemi WS2812B-Mini-V3/W — https://learn.watterott.com/kb/ws281x/WS2812B-Mini-v3.pdf
- nRF52840 Product Specification, Online-Fassung, abgerufen 23.10.2026 — https://docs.nordicsemi.com/r/bundle/ps_nrf52840
- nRF52840 DK, Kit-Inhalt (NFC-Antenne liegt bei) — https://docs.nordicsemi.com/r/bundle/ug_nrf52840_dk/page/ug/dk/kit_content.html
