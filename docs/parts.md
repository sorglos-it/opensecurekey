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
| Flash | auf dem DK vorhanden | W25Q512JV (512 Mbit = 64 MByte) |
| OLED | 0,96" SSD1306, SPI | gleiches Glas, ohne Trägerplatine |
| Fingerabdruck | FPC BM-Lite Entwicklungskit | FPC BM-Lite Modul — **offener Punkt**, siehe unten |
| Laderegler | — (DK läuft am USB) | siehe `power-budget.md` |
| Akku | — | LiPo, Größe aus `power-budget.md` |
| NFC-Antenne | 13,56-MHz-Flexantenne, 2-polig | als Leiterbahn auf der Platine |
| Taster, LED | auf dem DK vorhanden | SMD |
| Messmittel | Logikanalysator 8 Kanal USB | — |

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
| Nutzspeicher, maximal nichtflüchtig | 104 kB | **104 kB** |
| RAM (beim Abwählen gelöscht) | 608 Byte | 608 Byte |

**SE051C2 erfüllt alle drei Pflichtpunkte und hat 104 kB statt 50 kB Nutzspeicher**
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
mit den 50 kB des SE050. Mit 104 kB ist der Druck geringer. Die Ableitung bleibt
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

## Fingerabdrucksensor — ehrlich: kein Kandidat erfüllt alles

Das war der schwerste Punkt, und er bleibt offen. Gefordert waren vier Dinge:

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

### Vorschlag

**Den Fingerabdruck in Stufe 1 weglassen und Taster + PIN bauen.** Begründung:

- Bei FIDO2 ersetzt die Biometrie die PIN; sie ist **kein zusätzlicher Faktor**.
  Der Sicherheitsgewinn ist Bequemlichkeit, nicht Härte.
- Taster + PIN ist der Weg jedes offenen Projekts und zertifizierbar.
- Es kostet keinen Platz, keinen Strom und keine Angriffsfläche.
- Die Platine kann den Sensor **vorsehen** (4 Leitungen plus Versorgung auf einem
  Stiftfeld), ohne dass er bestückt wird. Dann entscheidet Stufe 2.

So bleibt das Gerät klein und offen, und die Frage "BM-Lite ohne NDA?" blockiert
den Baubeginn nicht. Diese Entscheidung ist nicht meine — sie gehört dem Nutzer;
ich lege sie vor.

## Quellen

- SE050 Datenblatt, Rev. 3.8, 18. Oktober 2023 — https://www.nxp.com/docs/en/data-sheet/SE050-DATASHEET.pdf
- SE051 Datenblatt, Rev. 2.0, 8. Juli 2024 — https://www.nxp.com/docs/en/data-sheet/SE051.pdf
- AN12436 "SE050 configurations", Rev. 2.4, 8. Juli 2024 — https://www.nxp.com/docs/en/application-note/AN12436.pdf
- AN12973 "SE051 configurations", Rev. 2.0, 8. Juli 2024 — https://www.nxp.com/docs/en/application-note/AN12973.pdf
- AN12413 "SE050 APDU Specification", Rev. 2.12, 24. März 2021 — https://www.nxp.com/docs/en/application-note/AN12413.pdf
  (Kurven-Kennungen: Secp256k1 = 0x10, ID_ECC_ED_25519 = 0x40)
- FPC BM-Lite — https://www.fpc.com/products/fingerprints/fingerprint-systems/bm-lite/
- Grow R503 Handbuch V1.4.1, 09/2023; Adafruit 4651 — https://www.adafruit.com/product/4651
