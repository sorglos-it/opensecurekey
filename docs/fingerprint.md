# Fingerabdrucksensor — zweiter Durchgang: es gibt jetzt einen

> **Stand 25.10.2026: Der Akku entfällt.** Die Abschnitte unten, die die Bauhöhe am
> Akku (3,8 mm) messen, die Zellenbreite oder die Akkukapazität erwähnen, sind
> Verlauf: Die Höhe bestimmt jetzt nicht mehr die Zelle. Was die Bauhöhe jetzt
> bestimmt, steht in `../hardware/layout-notes.md`, nicht hier. Die Sensorströme
> (Ruhe 14 µA, Erkennung 17 mA) gelten weiter, siehe
> [stromaufnahme.md](stromaufnahme.md).

Der erste Durchgang (siehe [parts.md](parts.md), Abschnitt „Fingerabdrucksensor")
kam zu dem Ergebnis: **kein geprüfter Kandidat erfüllt alle vier Bedingungen.**
Daraufhin wurde entschieden: Stufe 1 ohne Sensor, Stiftfeld J5 vorgesehen, nicht
bestückt.

**Dieser Durchgang kommt zu einem anderen Ergebnis.** Zwei Dinge haben sich
geändert — eines, weil tiefer gesucht wurde, eines, weil der Markt sich bewegt
hat:

1. **Der Host-Befehlssatz des BM-Lite ist offen** — nicht in dem Dokument, das
   der erste Durchgang gefunden hat, sondern als Quelltext unter Apache-2.0,
   mit **Fingerprint Cards AB als Urheber**. Punkt 3 war beim BM-Lite nie ein
   Lizenzproblem, sondern ein Fundproblem.
2. **FPC hat seit 2024 eine neue Baureihe: AllKey (FPC2530).** Sie ist
   **0,83 mm** dick statt 2,8 mm, rechnet selbst, hat eine vollständig
   öffentliche Beschreibung mit Dokumentnummer, ist bei DigiKey und Mouser
   einzeln gelistet — und es gibt ein Fertigboard von SparkFun mit
   MIT-Bibliothek. **Sie erfüllt alle vier Bedingungen.**

Die vier Bedingungen, unverändert:

1. Der Sensor rechnet den Vergleich **selbst** und liefert nur ja/nein.
2. In Einzelstücken bei einem echten Händler zu bekommen.
3. Beschreibung ohne Geheimhaltungsvertrag.
4. Klein genug für einen Schlüsselanhänger.

## Zwei Fragen von Thomas, am Datenblatt nachgesehen

**„Der FPC2534AM ist doch nur der Sensor am Flachband — fehlt da nicht die
Ansteuerung, die beim SparkFun-Board drauf ist?"**

Nein. Der Sensor spricht selbst, SPC27317-8 Abschnitt 6.5:

| Pin | SPI-Modus | UART-Modus |
| --- | --- | --- |
| `SPI_MOSI` / `MISO` / `SCK` / `CS_N` | vollständiges SPI, bis 10 MHz | GPIO |
| `UART_TX` / `RX` | GPIO | TX / RX, ab 2,4 kBaud |
| `IRQ` | Interrupt zum Wirt | Host-Weckruf |
| `IF_CFG_1` / `2` | wählt die Schnittstelle, per Pegel | dito |

Dazu Abschnitt 1: „several different host interfaces, **pre-loaded software and
is ready to use on delivery**", und 2.2: „On board authentication software",
„Encrypted template storage". **Vergleich und Vorlagen sind im Sensor.** Er kann
direkt an den nRF52840, ohne Zwischencontroller.

Das SparkFun-Board ist damit eine reine Trägerplatine: ZIF-Buchse für den Flex,
Spannungsversorgung, Qwiic-Stecker. Die 30 USD Aufpreis kaufen, dass am
Teststand nichts gelötet werden muss — nicht Elektronik, die dem Chip fehlt.

**„Wie macht es Yubico beim YubiKey Bio? Die haben doch auch keinen richtigen
Sensor drauf."**

Yubico baut es anders, mit **drei** Chips: Sensor, ein **eigenes Secure Element
für die Biometrie**, und der Hauptchip. Der Vergleich läuft im zweiten Chip,
nicht im Sensor; die Verbindung dazwischen ist verschlüsselt, und das
biometrische Teilsystem läuft unabhängig vom Rest (Yubico-Pressemitteilung zum
Marktstart der Bio-Reihe, `ungeprueft` — ein Teardown, der die Teile benennt,
war nicht zu finden).

Für uns heißt das: **Unser Weg kommt mit einem Chip weniger aus**, weil der
AllKey das kann, wofür Yubico ein zweites Secure Element verbaut. Die
Fingerabdruckdaten liegen im Sensor und erreichen unseren SE051 nie.

**Die ehrliche Grenze dabei**, SPC27317-8 Abschnitt 3.4: Der AllKey kennt einen
Aufnahmemodus, in dem sich ein Bild auslesen lässt (`0x0053 Image Data`). Das
Datenblatt sagt dazu ausdrücklich „the image cannot be used for biometric
operations; this state is intended for debugging purposes". Ein Bild kommt also
heraus, taugt aber nicht zum Vergleich — besser als beim Grow R503, wo das Bild
das echte ist, aber **kein** Riegel gegen eine veränderte Firmware. Das gehört
ins Bedrohungsmodell, wenn der Sensor kommt.

## Übersicht

| Teil | 1. rechnet selbst | 2. Kleinmenge | 3. ohne NDA | 4. Baugröße | Urteil |
| --- | --- | --- | --- | --- | --- |
| **FPC2534 AllKey Pro** | ja (belegt) | ja (belegt) | **ja (belegt)** | **Ø 11 × 0,83 mm** | **erste Wahl** |
| **FPC2532 AllKey** | ja (belegt) | ja (belegt) | **ja (belegt)** | Ø 11 × 0,83 mm | gleichwertig, weniger Schutz |
| FPC BM-Lite | ja | ja | **ja — korrigiert** | 15 × 15 × 2,8 mm | brauchbar, aber überholt |
| Grow R503-M22 | ja | ja | ja | 15 mm hoch | fällt an Punkt 4 |
| Laptop-Sensoren (Elan, Egis, Goodix) | ja | ja | teilweise | — | **fällt an USB** (s. u.) |
| IDEX / Zwipe / Kartensensoren | ungeprueft | **nein** | **nein** | — | nicht beschaffbar |
| Suprema SFM Slim | ja | ungeprueft | ungeprueft | **13,5 mm dick** | fällt an Punkt 4 |

## 1. Die Lizenzfrage beim BM-Lite ist beantwortet — und zwar mit „offen"

Das war der Punkt, an dem der erste Durchgang gescheitert ist. Er hat den
**Development Guide** (Dok. 100021008 Rev. 7.2, 03.04.2020) gefunden, der den
Vermerk „proprietary to Fingerprint Cards AB" trägt, und daraus geschlossen, der
Befehlssatz sei nicht offen zu haben.

**Der Befehlssatz liegt als Quelltext offen vor, veröffentlicht unter
Apache-2.0, mit FPC selbst als Urheber.**

Belege, zwei voneinander unabhängige:

| Fund | Datei | Urheberrechtsvermerk | Lizenz |
| --- | --- | --- | --- |
| `github.com/andpp/BM-Lite`, Zweig `master` | `BMLite_sdk/inc/fpc_hcp_common.h` | **„Copyright (c) 2020 Fingerprint Cards AB"** | **Apache-2.0** |
| `github.com/embenix/esp32c3_bmlite`, Zweig `main` | `components/fpc_bmlite/include/fpc_hcp_common.h` | **„Copyright (c) 2020 Fingerprint Cards AB"** | **Apache-2.0** |

Beide Dateien, abgerufen am **24.10.2026**, tragen denselben Kopf: FPC als
Urheber, Apache-2.0 als Lizenz. Das ist nicht die Lizenz eines Dritten, der
fremden Code weitergibt — **FPC ist dort als Urheber genannt.** Der Weg dahin
ist nachvollziehbar: Die Produktseite des Entwicklungskits nennt als
Lieferbestandteil „BM-Lite host protocol source code and host Python script"
(`ungeprueft`, über den Suchindex gelesen — die Seite selbst ist inzwischen auf
`fpc.com` umgezogen und trägt den Inhalt nicht mehr). Diese Quelldateien tragen
offenbar den Apache-Kopf.

Der Dateiinhalt ist genau das, was fehlte — die vollständige Befehlstabelle:

```
CMD_CAPTURE  = 0x0001   CMD_ENROLL   = 0x0002   CMD_IDENTIFY = 0x0003
CMD_MATCH    = 0x0004   CMD_IMAGE    = 0x0005   CMD_TEMPLATE = 0x0006
CMD_WAIT     = 0x0007   CMD_SETTINGS = 0x0008   CMD_CONNECT  = 0x2001
CMD_RESET    = 0x3002   CMD_CANCEL   = 0x3003   CMD_INFO     = 0x3004
CMD_STORAGE_TEMPLATE = 0x4002   CMD_COMMUNICATION = 0x6001   …
```

Dazu 223 Zeilen mit den Argument- und Ergebniskennungen. Die Rahmenschicht
(`hcp_tiny.c`), die Befehlsschicht (`bmlite_if.c`) und die HAL (`bmlite_hal.h`)
stehen daneben, mit **Andrey Perminov** als Urheber, ebenfalls Apache-2.0 — das
ist dann Fremdcode, aber freier.

**Eine dritte, unabhängige Umsetzung unter MIT:** MikroElektronika liefert für
sein BM-Lite-Board einen Treiber mit:
`github.com/MikroElektronika/mikrosdk_click_v2`,
`clicks/fingerprint4/lib_fingerprint4/include/fingerprint4.h`,
Kopf „Copyright (C) 2026 MikroElektronika d.o.o." mit dem vollen MIT-Text
(abgerufen 24.10.2026).

**Damit ist Punkt 3 für das BM-Lite erfüllt.** `parts.md` ist an dieser Stelle
zu berichtigen — siehe [Was in parts.md zu ändern ist](#was-in-partsmd-zu-ändern-ist).

### Was dabei nebenbei herauskam: das BM-Lite gibt Rohbilder heraus

`parts.md` führt es dem R503-M22 als Nachteil an, dass es „auf Befehl auch
Rohbilder" liefert, und stellt das BM-Lite als besser hin. **Das stimmt nicht.**
Im Befehlssatz des BM-Lite steht `CMD_IMAGE = 0x0005`, und die Kopfdatei
`bmlite_if.h` führt die Funktionen:

```
fpc_bep_result_t bep_image_get(HCP_comm_t *chain, uint8_t *data, uint32_t size);
fpc_bep_result_t bep_image_get_size(HCP_comm_t *chain, uint32_t *size);
fpc_bep_result_t bep_image_put(HCP_comm_t *chain, uint8_t *data, uint32_t size);
```

Quelle: `github.com/andpp/BM-Lite`, `BMLite_sdk/inc/bmlite_if.h`, abgerufen
24.10.2026. **Das BM-Lite kann Rohbilder herausgeben, genau wie das R503-M22.**
Der Vorwurf gegen das R503 war also kein Unterscheidungsmerkmal. Für
`docs/threat-model.md` heißt das: bei **jedem** dieser Module kann eine
veränderte Firmware Fingerabdruckbilder auslesen. Das ist eine Eigenschaft der
Gattung, nicht eines Teils.

## 2. FPC AllKey (FPC2530-Familie) — die erste Wahl

Das ist der eigentliche Fund. FPC hat die Baureihe nach dem BM-Lite
herausgebracht; der erste Durchgang konnte sie noch nicht kennen.

**Hauptquelle: FPC2530 Product specification, Dok. SPC27317, Fassung 8, vom
2026-01-28, „FPC AllKey – Biometric System family", 35 Seiten.**
Öffentlich abrufbar unter `fpc.com/media/iqtcuoah/spc27317-8-fpc2530-product-specification.pdf`
und gespiegelt bei DigiKey (`mm.digikey.com/…/spc27317-8-fpc2530-product-specification.pdf`).
Eine ältere Fassung 6 vom 2025-04-16 liegt bei Mouser. Alle Zahlen unten sind aus
Fassung 8 gelesen, abgerufen am 24.10.2026.

### Bestellnummern

Aus SPC27317-8:

| Ausführung | Bauform | Bestellnummer FPC | Sachnummer |
| --- | --- | --- | --- |
| FPC2532AP AllKey | LGA | FPC2532AP | 100026222 |
| **FPC2532AM AllKey** | **Flex-Modul** | FPC2532AM | **100026207** |
| FPC2534AP AllKey Pro | LGA | FPC2534AP | 100026223 |
| **FPC2534AM AllKey Pro** | **Flex-Modul** | FPC2534AM | **100026230** |

Der **Pro** (FPC2534) ist laut Herstellerangabe die Ausführung „für
Anwendungen, bei denen Fingerabdrücke zur Authentisierung dienen und Schutz vor
Manipulation Vorrang hat" — also genau unser Fall.

### Technische Daten

Alle aus **SPC27317-8, Abschn. 2.3 (Betriebsbedingungen) und 2.4 (Elektrische
Kennwerte)**, sofern nicht anders angegeben.

| Eigenschaft | Wert | Abschnitt |
| --- | --- | --- |
| **Sensorkörper (Ø × Dicke)** | **11,0 × 0,83 mm** | 2.3 |
| Aktive Fläche | 5,0 × 4,8 mm | 2.3 |
| Pixelmatrix | 100 × 96 Punkte | 2.3 |
| Auflösung | **508 dpi**, 256 Graustufen (8 bit) | 2.3 |
| Bauform LGA | 47 Pins, 8 × 8 Matrix, 1,0 mm Raster | 2.3 |
| Bauform Modul | Sensor auf Flex, **12-poliger ZIF-Anschluss** | 7.1 |
| Betriebstemperatur | −40 bis +125 °C | 2.3 |
| Lagerfähigkeit | 30 Jahre bei 25 °C erwartet | 2.3 |
| **Versorgung V_DD** | **3,0 / 3,3 / 3,6 V** (min/typ/max) | 2.4 |
| Strom, Biometrik (Enroll + Identify) | **17 mA typ, 21 mA max** | 2.4 |
| Strom, Befehle (aktiv) | 5,2 mA | 2.4 |
| Strom, Fingererkennung (wartend) | 14,3 / 22 / 78 µA | 2.4 |
| **Strom, Deep Sleep** | **14 µA** | 2.4 |
| Schnittstellen | **SPI, UART, USB, I²C** — per CONFIG-Pins gewählt | 6.4 |
| SPI-Takt | 10 MHz (Modus 0) | 2.4 |
| UART | Vorgabe 921 600 Baud (9 600 … 921 600) | 2.4 |
| Vorlagen | **30** | 3.5 (Befehl 0x0054) |
| Startzeit | 150 ms | 3.6 |
| Aufwachen aus Deep Sleep | 500 µs | 3.6 |
| Lötpads | 0,6 mm SMD, quadratisch | 6.12 |

**Widerspruch, offen:** SparkFun gibt für sein FPC2534-Board **45** verschlüsselte
Vorlagen an, die Herstellerbeschreibung für die Familie **30** (SPC27317-8,
Abschn. 3.7: „if the maximum number of fingers (30) are already enrolled, new
ones cannot be registered"). Möglich, dass der Pro mehr kann als die
Basisausführung; die Stelle in der Beschreibung unterscheidet nicht. **Vor der
Bestellung zu klären**, falls die Zahl der Vorlagen wichtig ist. Für unseren
Zweck sind beide Zahlen reichlich — ein Schlüsselanhänger braucht zwei bis zehn.

### Rechnet er selbst? Ja — und das Gehäuse schützt die Leitungen

Aus SPC27317-8, Abschn. 2.2 (Produktmerkmale):

- „On board authentication software"
- „Encrypted template storage"
- „Fingerprint templates are automatically created and stored in the internal
  flash memory. The device can be controlled from any host with basic commands
  for enrollment and verification" (Abschn. 2.2, Allgemeine Beschreibung)

**Und ein Punkt, den kein anderer Kandidat hat** — Abschn. 2.5 „Secure package
layout": Die internen Verbindungen zwischen Sensor, Flash und Mikrorechner
liegen **im Verguss des Gehäuses** und sind von außen nicht zugänglich:

> „Internal communication lines between the sensor, flash and MCU are all hidden
> inside the molded package body. To maintain integrity, signals cannot be
> accessed from the outside."

Das ist für `docs/threat-model.md` ein echter Gewinn: Beim BM-Lite und beim R503
ist „der Vergleich passiert im Modul" eine Software-Aussage. Hier ist das
Abhören der internen Leitungen **durch den Aufbau** versperrt.

### Gibt er Rohbilder heraus? Ja — aber eingeschränkt

Aus SPC27317-8, Abschn. 3.2 (Zustände) und 3.5 (Befehle):

- Befehl `0x0050 Capture` — „Arm the sensor to capture a single image on touch"
- Befehl `0x0053 Image Data` — „Request readout of fingerprint image collected
  in Capture mode"
- Zustand `0x0004 Capture`: „**Note that the image cannot be used for biometric
  operations; this state is intended for debugging purposes and evaluating image
  quality.**"
- Zustand `0x0010 Image Available`: „A captured fingerprint image is available
  for readout. **No biometrics can be performed on the image.** State is cleared
  upon readout."

**Also: ja, Rohbilder sind auslesbar.** Das ist bei allen drei Kandidaten so und
kein Unterscheidungsmerkmal. Der Unterschied zum R503: Das Aufnehmen eines
Bildes und das Vergleichen sind hier getrennte, einander ausschließende
Zustände — ein Bild aus dem Capture-Modus **kann nicht** zum Vergleich benutzt
werden. Wer Bilder abziehen will, muss den Sensor also aus dem Betriebszustand
holen; das ist nachweisbar und nicht nebenbei zu machen.

### Lizenzlage der Beschreibung — der entscheidende Punkt

**Die vollständige Beschreibung ist ohne Geheimhaltungsvertrag öffentlich
abrufbar** — bei FPC selbst und gespiegelt bei zwei Händlern. Sie enthält, was
man zum Anschluss braucht: die Befehlstabelle mit allen Kennungen, den
Zustandsautomaten, das Rahmenformat (Abschn. 3.4.1 Frame Header, 3.4.2 Command
Header), die Zeiten, die elektrischen Kennwerte, die Pinbelegung und das
Lötbild. **Kein Download-Formular, keine Unterschrift.**

**Eine Einschränkung, die genannt werden muss, weil sie auf jeder Seite steht:**

> „This document may not be copied in whole or in part, without written
> permission from Fingerprint Cards AB. This document may neither be reproduced,
> displayed, modified, or disclosed, nor transferred in any manner to any by
> Fingerprint Cards AB unauthorized person."

**Was das heißt und was nicht:** Es ist eine Beschränkung des **Dokuments**,
kein Geheimhaltungsvertrag und keine Beschränkung der Benutzung. Wir dürfen die
Beschreibung lesen und danach bauen; wir dürfen die PDF nicht in unser
Verzeichnis legen und nicht Seiten daraus abschreiben. Für ein MIT-Projekt ist
das tragbar — so steht es bei fast jedem Datenblatt, auch beim SE051, nach dem
dieses Projekt bereits baut. **Dieses Dokument hier zitiert knappe Stellen mit
Quellenangabe; eine Kopie des Dokuments gehört nicht ins Verzeichnis.**

**Dazu, und das ist der eigentliche Freibrief: eine offene Umsetzung unter MIT.**

| Fund | Lizenz | Stand |
| --- | --- | --- |
| `github.com/sparkfun/SparkFun_FPC2534_Arduino_Library` | **MIT** (GitHub-Lizenzangabe, `LICENSE` vorhanden) | letzter Push **04.12.2025** |

Beschreibung dort: „Arduino Library for the SparkFun FPC2534 Breakout Board …
based off the AllKey Biometric System family from Fingerprints Cards (FPC)".
Mitgelieferte Beispiele, abgerufen 24.10.2026:

```
examples/Example01_NavigationI2C    examples/Example02_EnrollI2C
examples/Example03_NavigationUART   examples/Example04_EnrollUART
```

**Damit gibt es eine lesbare, frei lizenzierte Umsetzung von Anmeldung und
Erkennung über I²C und über UART.** Für ein MIT-Projekt ist MIT die bequemste
Lage, die es gibt — bequemer als Apache-2.0 beim BM-Lite.

**Und die Befehlsschnittstelle selbst liegt unter Apache-2.0 vor, von FPC.** In
der SparkFun-Bibliothek liegt FPCs eigene Kopfdatei mit:

`SparkFun_FPC2534_Arduino_Library`, `src/sfTk/fpc_api.h`, abgerufen 24.10.2026:

```
/*
 * Copyright (c) 2024 Fingerprint Cards AB
 *
 * Licensed under the Apache License, Version 2.0 (the "License")
 * …
 */
/**
 * @file    fpc_api.h
 * @brief   FPC AllKey Pro API
 *
 * This is the Command Interface for the Fingerprint Sensor Module fpc2534.
 */
```

**Das ist dieselbe Lage wie beim BM-Lite, nur zwei Jahre neuer:** FPC
veröffentlicht die Befehlsschnittstelle seiner Sensoren als Quelltext unter
Apache-2.0. Punkt 3 ist damit nicht nur über die Händler-PDF erfüllt, sondern
über eine freie Lizenz auf genau der Datei, die man zum Bauen braucht.

Die Kopfdatei gehört zum Entwicklungskit (`FPC2534 AllKey Pro Development Kit
Getting Started`, Dok. INS27386, Fassung 1, auf `fpc.com`; Beispiele unter
`examples/secure_com/src/`) — SparkFun liefert sie in ihrem MIT-Verzeichnis mit,
unter dem Apache-Kopf, den FPC ihr gegeben hat.

### Beschaffung — Einzelstücke bei echten Händlern

**Alle Preise und Bestände sind Momentaufnahmen, abgerufen am 24.10.2026, über
den Suchindex gelesen und `ungeprueft`.** Die Händlerseiten von Mouser und
DigiKey ließen sich nicht direkt abrufen (beide weisen automatische Abrufe ab);
vor der Bestellung ist die Seite im Browser anzusehen.

| Teil | Händler | Preis Menge 1 | Bestand |
| --- | --- | --- | --- |
| FPC2532AP (LGA) | DigiKey | 21,24 USD | `ungeprueft` |
| FPC2534AP (LGA) | DigiKey | 23,85 USD | `ungeprueft` |
| **FPC2532AM (Flex-Modul)** | DigiKey | **26,95 USD** | `ungeprueft` |
| FPC2532AM (Flex-Modul) | Mouser | 27,28 USD (19,06 ab 100) | `ungeprueft` |
| **FPC2534AM (Flex-Modul, Pro)** | Mouser | **30,14 USD** | `ungeprueft` |
| FPC2532 Entwicklungskit | DigiKey 100026231 | 150 USD, **Lieferzeit 10 Wochen** | `ungeprueft` |
| FPC2532 Entwicklungskit | Mouser | 140,12 USD | 46 Stück, `ungeprueft` |
| FPC2534 Pro Entwicklungskit | Mouser | 164,15 USD | 4 Stück, `ungeprueft` |
| **SparkFun SEN-29854 „Fingerprint Sensor – FPC2534 (Qwiic)"** | **SparkFun** | **59,95 USD** | **lagernd**, `ungeprueft` |
| SEN-29854 | Antratek (EU) | 72,54 EUR | `ungeprueft` |
| SEN-29854 | RobotShop | 59,95 USD | `ungeprueft` |

**Punkt 2 ist erfüllt**, und zwar doppelt: das nackte Modul für rund 27–30 USD
und ein Fertigboard für 60 USD, beides in Menge 1.

Das SparkFun-Board ist zudem das richtige Teil für den Teststand: Es trägt den
**Pro** (FPC2534), kostet ein Viertel des FPC-Entwicklungskits, ist lagernd
statt zehn Wochen Lieferzeit — und bringt die MIT-Bibliothek mit, in der
I²C und UART bereits vorgeführt werden. In Europa gibt es das Board bei
Antratek.

### Teststand: passt der Sensor neben den Sicherheitschip?

**Ja, ohne Konflikt.** Beide hängen am I²C des nRF52840-DK, aber auf
verschiedenen Adressen:

| Teil | I²C-Adresse | Quelle |
| --- | --- | --- |
| SE051C2 (MIKROE-5392) | **0x48** werkseitig | `hardware/schematic.md`, nach SE050 DS Rev. 3.8 |
| FPC2534 (SEN-29854) | **0x24** als Vorgabe | `sfDevFPC2534I2C.h`, `kFPC2534DefaultAddress`, abgerufen 24.10.2026 |

Die Adressen in der Herstellerbeschreibung SPC27317-8 ließen sich im
ausgelesenen Text nicht finden — die 0x24 stammt aus der SparkFun-Bibliothek und
ist insoweit `ungeprueft`, aber sie ist der Wert, mit dem die mitgelieferten
Beispiele laufen. **Der Sensor kann also an denselben Bus wie der
Sicherheitschip**, und das Board führt zusätzlich UART und SPI heraus, falls
I²C zu langsam wird (SPI bis 10 MHz, UART bis 921 600 Baud).

### Und FIDO2 damit? Zum Teil — ein Befehl fehlt

Hier ist zu trennen, was der Sensor am FIDO2-Protokoll tun soll. Es sind zwei
verschiedene Dinge, und nur eines davon geht mit der heutigen Firmware.

**Was heute schon geht: der Finger als Freigabe.** Die Firmware hat genau einen
Engpass für die Freigabe am Gerät — `osk_ui_confirm()` in
`apps/firmware/include/osk_ui.h`, Zeile 35. Der CTAP-Kern fragt dort bei jedem
Vorgang selbst nach (so steht es in `src/classes/transport/nfc_ctap.h`). Ein
Sensor, der ja/nein liefert, wird an dieser einen Stelle angeschlossen; die
UV-Flagge im Authenticator-Data wird dann gesetzt wie heute nach der PIN
(belegt durch `test/ctap/test_pin.c`, Zeile 289: „Jetzt muss die UV-Flagge
gesetzt sein"). `authenticatorGetInfo` meldet bereits `uv: true` und
`pinUvAuthToken: true` (`osk_ctap_core.c`, Zeilen 1286 und 1292).
**Dafür ist am CTAP-Teil nichts zu ändern.**

**Was fehlt: das Anlernen über FIDO2.** Der Befehl
`authenticatorBioEnrollment` (0x09) ist nicht umgesetzt, und zwar
**absichtlich und ausdrücklich**. In `src/classes/ctap/osk_pin.c`, Zeile 744:

> „Was wir nicht koennen, darf auch nicht verlangt werden: wir haben weder
> credMgmt noch bioEnroll noch authenticatorConfig."

Die Berechtigung `OSK_PERM_BE` wird mit `CTAP2_ERR_UNAUTHORIZED_PERM`
abgewiesen. **Folge:** Ein Browser oder Windows kann auf diesem Schlüssel keine
Finger anlernen, löschen oder auflisten — so wie es bei einem YubiKey Bio geht.

**Das ist kein Hindernis, sondern eine Festlegung, wo angelernt wird:**
**am Gerät, über OLED und Taster**, nicht über den Rechner. Dafür ist die
Anzeige ohnehin da, und der AllKey bringt die Befehle mit (`0x0054 Enroll`
über mehrere Berührungen, mit Rückmeldung je Berührung — SPC27317-8, Abschn. 3.7).
Wer das Anlernen über den Browser will, muss `bioEnroll` nachbauen; das gehört
dem CTAP-Strang und ist mit dem Sensor nicht erledigt.

**Noch nicht geprüft:** ob `uv: true` in `getInfo` heute richtig gemeldet ist.
In CTAP 2.1 meint die Option `uv` die **im Gerät eingebaute** Prüfung — die PIN
wird getrennt über `clientPin` gemeldet. Solange nur die PIN da ist, könnte das
Feld zu viel versprechen. Mit einem Sensor wäre es dann richtig. **Gehört dem
CTAP-Strang; hier nur gemeldet, nicht entschieden.**

## 3. Bedingung 4 nachgerechnet: die Baugröße ist kein Hindernis mehr

Der Auftrag fragt, wie dick ein Sensor wirklich sein darf. **Die Antwort steht
schon im Projekt**, in `hardware/layout-notes.md`, und sie ist günstiger als
gedacht.

Der Aufbau heute (`hardware/layout-notes.md`, Abschn. „Bauform: 24 × 70 mm"):

| Lage | Dicke |
| --- | --- |
| Platine | 0,8 mm |
| Akku (Adafruit 1317) auf der einen Seite | 3,8 mm |
| Funkmodul (MDBT50Q) auf der anderen Seite | 2,05 mm |
| **Summe** | **≈ 6,7 mm plus Gehäuse, fertig etwa 8–9 mm** |

`layout-notes.md` schreibt dazu, wörtlich:

> „**Wichtig für Stufe 2:** Das BM-Lite muss **neben** die Zelle, nicht über
> sie. Mit 2,8 mm ist es dünner als der Akku (3,8 mm) — nebeneinander bestimmt
> also weiter der Akku die Höhe, und das Gehäuse bleibt gleich."

**Das heißt: schon mit 2,8 mm kostet der Sensor keine Bauhöhe.** Die
Gesamthöhe richtet sich nach dem Akku, nicht nach dem Sensor. Solange der Sensor
dünner als 3,8 mm bleibt und **neben** der Zelle liegt, ändert sich am Gehäuse
nichts. Die Grenze ist also nicht „so dünn wie möglich", sondern **3,8 mm** —
und selbst darüber wäre nur die Seitenaufteilung zu ändern, nicht die Bauform.

Mit dem AllKey wird es deutlich einfacher:

| | BM-Lite | **AllKey FPC2534AM** | Gewinn |
| --- | --- | --- | --- |
| Dicke | 2,8 mm | **0,83 mm** (Sensorkörper) | **−1,97 mm**, 70 % weniger |
| Grundfläche | 15 × 15 mm = 225 mm² | **Ø 11 mm ≈ 95 mm²** | **−58 %** |
| Abstand zum Akku (3,8 mm) | 1,0 mm Luft | **3,0 mm Luft** | — |

**Zwei Folgen, beide erfreulich:**

1. **Die Einschränkung in `layout-notes.md` fällt weg.** Dort steht: „Mit der
   schmalen Zelle (12 mm) passt das BM-Lite (15 × 15 mm) bequem; mit der
   19,75 mm breiten wird es eng." Mit Ø 11 mm statt 15 × 15 mm **passt der
   Sensor auch neben die breite Zelle** — die Wahl der Akkukapazität ist nicht
   mehr an die Sensorfrage gekoppelt.
2. **Die Gehäuseaussparung wird kleiner.** Statt eines 15-mm-Quadrats ein Loch
   von Ø 11 mm, und die aktive Fläche ist nur 5,0 × 4,8 mm. Das Blindstück aus
   Stufe 1 wird entsprechend kleiner.

**Was bleibt:** Der Flex-Anschluss (12-polig, ZIF) braucht einen Steckverbinder
oder eine Lötstelle; das Stiftfeld J5 (1 × 6, 1,27 mm) passt dafür nicht
unmittelbar. Das ist eine Layout-Frage für den Hardware-Strang, keine
Beschaffungsfrage — und für den Teststand irrelevant, weil das SparkFun-Board
Qwiic (I²C) herausführt.

### Der Lastschalter Q5 bleibt Pflicht

`hardware/layout-notes.md` begründet Q5 damit, dass der Sensor sonst „13 µA
Dauerstrom mit[bringe] — mehr als unser gesamtes Ruhebudget". Das gilt
unverändert: Der AllKey zieht im Deep Sleep **14 µA** (SPC27317-8, Abschn. 2.4),
also eher etwas mehr als das BM-Lite. **Die Entscheidung, Q5 auch ohne Sensor zu
bestücken, war richtig und wird durch diesen Fund bestätigt.**

Zum Energiebudget, zum Vergleich:

| | BM-Lite | AllKey FPC2534 |
| --- | --- | --- |
| Erkennung | 12 mA (`parts.md`) | **17 mA typ / 21 mA max** |
| Ruhe | 13 µA | **14 µA** |
| Wartend auf Finger | `ungeprueft` | 14,3 / 22 / 78 µA |

Der AllKey zieht beim Rechnen **mehr** als das BM-Lite. Das ist für den
Spitzenstrom zu prüfen: Das Energiebudget deckelt die Nebenverbraucher, und
`hardware.md` schreibt, über +4 dBm Sendeleistung „bleiben nur 5 mA". 21 mA
gleichzeitig mit Funk geht also nicht — **Erkennung und Funk müssen sich
abwechseln**, was sie ohnehin tun (erst Finger, dann Antwort). Gehört dem
Energiebudget-Strang; gemeldet, nicht gerechnet.

## 4. Was in den anderen Ecken zu finden war

### Smartcard-Welt: nichts einzeln kaufbar

Gesucht nach IDEX Biometrics, Zwipe und den Sensoren aus
Fingerabdruck-Bezahlkarten. **Ergebnis: Geschäft zwischen Firmen, kein
Einzelverkauf.** Die Funde sind Mengenabschlüsse — Zwipe bestellte 2020 bei IDEX
300 000 Sensoren (IDX3405) für seine Pay-ONE-Plattform; IDEX liefert die
Sensoren für IDEMIAs Biometriekarten. **Kein Händler, kein Datenblatt, keine
Einzelstücke** (`ungeprueft`, über den Suchindex gelesen). Dazu ein Warnzeichen:
Nach einem Bericht von Januar 2025 wollte Zwipe eine Dringlichkeitssitzung
darüber abhalten, ob es eine Forderung von IDEX überhaupt bezahlen und
weiterarbeiten kann. **Fällt an Punkt 2 und 3.** Ein „T-Shape"-Produkt von FPC
ließ sich nicht belegen — der Name kommt in keiner auffindbaren Quelle vor.

Dieser Weg ist nun ohnehin erledigt: **Der AllKey ist mit 0,83 mm so dünn wie
ein Kartensensor und einzeln zu kaufen.** Genau das, was dieser Umweg suchen
sollte.

### Laptop- und Telefonsensoren: scheitern an einer Hardwaregrenze

Das war der aussichtsreichste der vorgeschlagenen Umwege, und er führt zu einem
klaren Nein — aus einem Grund, der nichts mit Lizenzen zu tun hat.

Es gibt **viel** offene Arbeit an Sensoren, die selbst rechnen
(„Match-on-Chip"), getrieben von Linux-Leuten:

| Projekt | Sensor | Rechnet selbst |
| --- | --- | --- |
| `BlueApple233/OMS-fingerprint-linux` | OMS 33a7:2388 | ja — bis 16 Vorlagen im Chip |
| `ITx-prash/libfprint-elan-04f3-0c6c` | Elan MOC2 | ja |
| `WilkerDev1/libfprint-egismoc-rs` | Egis 1c7a:0582–05a5 | ja (mit Microsoft SDCP) |
| libfprint, eigener Treiber | Goodix MOC | ja (`ungeprueft`) |
| FPCs eigene Organisation `github.com/fingerprint-cards` | fpc1xxx, fpc16xx | **nein** — reine Bildsensoren, GPL-2.0 |

**Warum das alles nicht in Frage kommt: diese Sensoren sind USB-Geräte, und der
nRF52840 kann kein USB-Host sein.** Sein USB-Baustein ist ein reines
Gerät („device only"), kein Host und kein OTG (nRF52840 Product Specification,
Kapitel USBD; von Nordic im Entwicklerforum ausdrücklich bestätigt:
„According to specification, USB peripheral available on nRF52840 is device
only" — `ungeprueft`, Forumsantwort, nicht aus der Beschreibung selbst
abgeschrieben). Ein zusätzlicher Host-Chip wäre nötig.

**Und selbst dann:** Der USB-C-Anschluss des Geräts ist der Weg zum Rechner —
das Gerät **ist** dort das USB-Gerät. Ein zweiter USB-Anschluss nur für den
Sensor bedeutete einen weiteren Chip, mehr Platz, mehr Strom und mehr
Angriffsfläche in einem Schlüsselanhänger. **Dieser Weg ist zu.**

Nebenbei bemerkenswert und für `parts.md` eine Berichtigung: **FPC betreibt eine
eigene GitHub-Organisation** (`github.com/fingerprint-cards`) mit drei
Verzeichnissen — `libfprint` (LGPL-2.1), `optical_device_driver` für die
fpc16xx-Reihe (GPL-2.0) und `capacitive_device_driver` für die
fpc1xxx-Reihe (GPL-2.0), zuletzt 2020 bzw. 2023 geändert (abgerufen 24.10.2026).
Das sind Treiber für **Bildsensoren**, die nicht selbst rechnen — für uns
unbrauchbar (Punkt 1). Aber es belegt: **FPC veröffentlicht Quelltext unter
freien Lizenzen.** Das macht den Apache-Kopf auf `fpc_hcp_common.h` glaubwürdig.

### Andere offene Projekte: weiterhin niemand mit Sensor im Schlüssel

Gesucht nach Türschlössern, offenen Hardware-Geldbörsen und Zutrittssystemen.

**Türschlösser:** Es gibt viele Bastelprojekte, alle mit den optischen
UART-Modulen der R30x/R50x-Reihe (`frickelzeugs/FingerprintDoorbell` mit ESP32
und R503, `OpenMakerProjects/fingerprint-lock-system`,
`RalphBacon/Arduino-Fingerprint-Sensor-With-Lock`). Für ein Türschloss an der
Wand ist die Bauhöhe gleichgültig — für einen Schlüsselanhänger nicht. **Kein
Gewinn.**

**Hardware-Geldbörsen:** Keine offene Geldbörse mit Sensor und veröffentlichtem
Schaltplan gefunden. Keystone 3 Pro hat einen Sensor, aber die Hardware ist nur
teilweise offen; Trezor veröffentlicht `trezor/trezor-hardware`, hat aber keinen
Sensor; Coldcard hat Schaltpläne, aber einzelne Bauteile sind geschlossen
(alles `ungeprueft`, über den Suchindex gelesen).

**Der Befund des ersten Durchgangs bleibt also stehen: Kein offenes
FIDO-Projekt hat einen Fingerabdrucksensor** (Nitrokey 3, Solo2, OpenSK — alle
nur Taster). Wir wären die Ersten. **Das ist jetzt aber kein Hindernis mehr,
sondern nur noch eine Feststellung** — denn der Teil, der fehlte, war nicht das
Vorbild, sondern ein beschaffbares Teil mit offener Beschreibung.

### Suprema SFM Slim: an der Dicke gescheitert

Der Hersteller nennt es „the slimmest 13,5 mm FAP 20-certified Optical
Fingerprint Module", mit Vergleich im Modul (5 000 Vorlagen je Sekunde,
bis 22 000 Vorlagen). **13,5 mm** — das ist dasselbe Problem wie beim R503 und
für einen Schlüsselanhänger erledigt (`ungeprueft`, Herstellerseite über den
Suchindex). Next Biometrics: nichts Brauchbares auffindbar.

### Was aus den anderen genannten Herstellern wurde

Goodix, Egis, Novatek, Chipone, Silead, Synaptics: **unverändert nicht
beschaffbar.** Ihre Match-in-Sensor-Teile gehen an Geräteherstellers in Mengen,
mit Vertrieb über Distributoren und Geheimhaltung. Dass ihre Sensoren über
libfprint ansprechbar sind, hilft nicht — das sind Ausbauteile mit
USB-Anschluss (siehe oben). IDEX, Zwipe: siehe Smartcard-Welt. Next
Biometrics: nichts belegt.

**FPC ist der einzige der genannten Hersteller, der offen liefert.** Zweimal
sogar: Quelltext unter Apache-2.0 und MIT für die Befehlssätze, GPL-Treiber in
der eigenen GitHub-Organisation, und eine vollständige Produktbeschreibung ohne
Formular.

## Empfehlung

**Der Sensor, der alle vier Bedingungen erfüllt, ist der FPC2534AM
(AllKey Pro, Flex-Modul, Sachnummer 100026230).**

| Bedingung | erfüllt? | Beleg |
| --- | --- | --- |
| 1. rechnet selbst | **ja** | SPC27317-8, Abschn. 2.2: „On board authentication software", „Encrypted template storage"; dazu Abschn. 2.5: interne Leitungen im Verguss |
| 2. Kleinmenge beim Händler | **ja** | DigiKey/Mouser, Menge 1: 26,95–30,14 USD; SparkFun SEN-29854, 59,95 USD, lagernd |
| 3. Beschreibung ohne NDA | **ja** | SPC27317 Fassung 8, 2026-01-28, 35 Seiten, offen auf fpc.com und bei DigiKey; dazu MIT-Bibliothek von SparkFun |
| 4. klein genug | **ja** | Ø 11,0 × **0,83 mm** gegen 3,8 mm Akku, der die Höhe bestimmt |

**Damit sollte Thomas' Entscheidung geändert werden — aber nicht übereilt.**

Was unverändert gilt und gegen den Sensor spricht, ganz ohne Beschaffungsfrage:
**Bei FIDO2 ersetzt die Biometrie die PIN, sie ist kein zusätzlicher Faktor.**
Das Gerät wird durch den Sensor nicht sicherer, sondern bequemer. Dieses
Argument aus `parts.md` und `hardware.md` ist richtig und wird von diesem
Durchgang **nicht** berührt. Wer den Sensor aus diesem Grund weglässt, hat
weiterhin recht.

**Was sich geändert hat, ist nur das andere Argument** — „die Beschaffungslage
ist ungelöst". Die ist jetzt gelöst. Daraus folgt ein abgestufter Vorschlag:

1. **Stufe 1 bleibt ohne Sensor.** Die Entscheidung dafür steht auf zwei Beinen,
   und das tragende (FIDO2-Biometrie ersetzt die PIN) ist unverändert fest. Die
   Platine ist entworfen, der Teststand bestellt; jetzt umzuplanen würde Arbeit
   wegwerfen für eine Bequemlichkeit.
2. **Aber: J5 ist für das falsche Teil ausgelegt.** Das Stiftfeld 1 × 6 mit
   1,27 mm passt zum BM-Lite, nicht zum 12-poligen ZIF-Flex des AllKey. **Wenn
   die Platine noch nicht gefertigt ist, sollte das Feld jetzt auf den AllKey
   umgelegt werden** — sonst ist die Zusage „Stufe 2 ohne neue Platine"
   hinfällig. Das ist die einzige Sache, die wirklich drängt, und es ist eine
   Entscheidung für den Hardware-Strang.
3. **Den Teststand um ein Teil erweitern, wenn Stufe 2 ernst gemeint ist:**
   SparkFun SEN-29854, 59,95 USD, lagernd, mit MIT-Bibliothek. Damit lässt sich
   die Anbindung am nRF52840-DK ausprobieren, **bevor** eine Platine davon
   abhängt. Das ist kein Vorrat, sondern das Wegnehmen des letzten
   ungeprüften Punkts — denn **nichts davon ist an echter Hardware gelaufen.**
   Es passt ohne Umbau neben den Sicherheitschip (I²C 0x24 gegen 0x48), und
   der Finger kann über `osk_ui_confirm()` sofort als Freigabe dienen —
   **die FIDO2-Benutzerprüfung geht damit ohne Änderung am CTAP-Teil.** Nur das
   Anlernen über den Browser fehlt (`bioEnroll`); angelernt wird am Gerät.
4. **Das FPC-Entwicklungskit nicht bestellen** (140–164 USD, 10 Wochen
   Lieferzeit bei DigiKey). Das SparkFun-Board tut dasselbe für ein Drittel und
   ist lagernd.

**Kurz: Die Beschaffungssperre ist gefallen, das Sicherheitsargument steht. Also
nicht Stufe 1 umbauen — aber J5 auf den AllKey umlegen, solange das noch
kostenlos ist, und ein Board für 60 USD zum Teststand legen.**

## Was nicht geprüft ist

Ehrlich aufgezählt, weil es die Grenzen dieses Durchgangs sind:

- **Alle Händlerpreise und Bestände sind `ungeprueft`.** Mouser, DigiKey und
  Mikroe weisen automatische Abrufe ab; die Zahlen sind über den Suchindex
  gelesen, Abrufdatum 24.10.2026. Mehrere der Fundstellen waren laut Suchindex
  495 bis 556 Tage alt. **Vor der Bestellung die Seite im Browser ansehen.**
- **Das Produktdatenblatt SPC26627-3 des BM-Lite** (Fassung 3, 2023-10-25,
  „Public" gekennzeichnet) ließ sich nicht herunterladen — Mouser bricht den
  Abruf ab. Die BM-Lite-Zahlen in diesem Dokument stammen daher aus `parts.md`
  und sind dort schon als `ungeprueft` geführt.
- **Die Zahl der Vorlagen** widerspricht sich: 30 laut SPC27317-8, 45 laut
  SparkFun. Nicht aufgelöst.
- **Ob das BM-Lite noch gefertigt wird**, ist nicht belegt. FPC bewirbt AllKey
  als aktuelle Baureihe; ein Abkündigungsvermerk für das BM-Lite wurde **nicht**
  gefunden, aber auch keine Bestätigung der Weiterführung. Das Mikroe-Board
  (MIKROE-5465, Fingerprint 4 Click, 88 USD) ist auf der Herstellerseite als
  „no longer in stock" geführt — während die Maschinendaten derselben Seite
  „InStock" sagen. Widersprüchlich, `ungeprueft`.
- **Keine Anfrage an einen Hersteller gestellt**, keine E-Mail, kein Formular —
  wie beauftragt. Die Frage „kommt der BM-Lite-Befehlssatz ohne Unterschrift"
  hat sich dadurch erledigt, dass er offen im Netz liegt; eine Anfrage ist nicht
  mehr nötig.
- **Keine Lebenderkennung belegt**, bei keinem Kandidaten. Die Beschreibung des
  AllKey nennt ESD-, Kratz- und Stoßschutz durch die Beschichtung, aber **keine
  Erkennung von Attrappen.** Wer einen abgeformten Finger fürchtet, gewinnt
  durch keinen dieser Sensoren etwas.
- **Nichts ist an echter Hardware gelaufen.** Wie beim ganzen Projekt.

## Was in parts.md zu ändern ist

`docs/parts.md` gehört diesem Strang nicht. Zu ändern wäre dort im Abschnitt
„Fingerabdrucksensor":

1. **Die Überschrift.** „kein Kandidat erfüllt alles" ist überholt — der
   FPC2534AM erfüllt alles.
2. **Der Satz zum BM-Lite,** der Punkt 3 als „nur halb erfüllt" führt, und der
   Absatz „Vor dem Kauf zu klären (Frage an den Hersteller): Kommt der
   Befehlssatz ohne Unterschrift?". **Antwort: ja, er liegt offen** — Apache-2.0
   mit FPC als Urheber, zweifach unabhängig belegt, dazu eine MIT-Umsetzung von
   Mikroe. Der Satz „Das ist für ein MIT-Projekt ein echtes Problem und keine
   Formalie" ist nicht mehr richtig.
3. **Der Vergleich BM-Lite gegen R503 beim Rohbild.** Dem R503 wird vorgehalten,
   es liefere „auf Befehl auch Rohbilder" — **das BM-Lite tut das auch**
   (`CMD_IMAGE`, `bep_image_get`). Kein Unterscheidungsmerkmal.
4. **Eine Zeile für die AllKey-Familie** in der Übersichtstabelle, mit Verweis
   hierher.
5. **Die Tabellenzeile „Goodix, Egis, Silead, Chipone … nicht beschaffbar"**
   bleibt richtig, sollte aber den Grund nennen: ihre Teile sind als
   USB-Ausbauteile dokumentiert, und der nRF52840 kann kein USB-Host sein.
6. **Der Satz „Nicht bestellen: das BM-Lite-Entwicklungskit"** bleibt richtig —
   zu ergänzen wäre: das FPC-AllKey-Kit ebenfalls nicht, sondern das
   SparkFun-Board, falls Stufe 2 ausprobiert werden soll.

Für `hardware/layout-notes.md` (gehört ebenfalls nicht diesem Strang):

7. **J5 passt nicht zum AllKey.** 1 × 6 mit 1,27 mm gegen 12-poligen ZIF-Flex.
   Wenn die Zusage „Stufe 2 ohne neue Platine" gelten soll und der AllKey das
   Zielteil ist, muss das Feld umgelegt werden.
8. **Die Einschränkung „mit der 19,75 mm breiten Zelle wird es eng"** gilt für
   den AllKey nicht mehr: Ø 11 mm statt 15 × 15 mm.

Für den CTAP-Strang (`docs/ctap.md`, `osk_ctap_core.c`, `osk_pin.c`):

11. **`bioEnroll` fehlt und wird abgewiesen** (`osk_pin.c`, Zeile 744 ff.).
    Ohne diesen Befehl kann kein Browser Finger anlernen; angelernt wird am
    Gerät über OLED und Taster. Das ist eine Festlegung, die irgendwo stehen
    sollte — heute steht sie nur als Kommentar im Quelltext.
12. **`uv: true` in `getInfo` ist zu prüfen.** In CTAP 2.1 meint die Option die
    im Gerät eingebaute Prüfung; die PIN wird über `clientPin` gemeldet. Ohne
    Sensor könnte das Feld zu viel versprechen.

Für `docs/threat-model.md`:

9. **„Der Vergleich passiert im Modul" ist beim AllKey mehr als Software:**
   SPC27317-8 Abschn. 2.5 — die Leitungen zwischen Sensor, Flash und Rechner
   liegen im Verguss und sind von außen nicht erreichbar.
10. **Rohbilder sind bei jedem dieser Module auslesbar.** Beim AllKey nur in
    einem Zustand, in dem kein Vergleich möglich ist; das ist besser, aber
    keine Sperre.

## Quellen

**Herstellerbeschreibungen**

- **FPC2530 Product specification, Dok. SPC27317, Fassung 8, 2026-01-28,**
  „FPC AllKey – Biometric System family", 35 Seiten.
  `fpc.com/media/iqtcuoah/spc27317-8-fpc2530-product-specification.pdf`,
  gespiegelt bei DigiKey. Abgerufen und ausgelesen 24.10.2026. — Alle
  AllKey-Zahlen, Befehle, Zustände.
- FPC2530 Product specification, Fassung 6, 2025-04-16 — ältere Fassung bei
  Mouser, nicht ausgewertet.
- FPC2534 AllKey Pro Development Kit Getting Started, Dok. INS27386, Fassung 1,
  `fpc.com`. `ungeprueft` — nur über den Suchindex gelesen.
- BM-Lite Development Guide, Dok. 100021008 Rev. 7.2, 03.04.2020 — der
  „proprietary"-Vermerk, an dem der erste Durchgang scheiterte.
- BM-Lite Safetouch product specification, Dok. SPC26627, Fassung 3,
  2023-10-25, „Public" — **Abruf misslungen** (Mouser bricht ab).
- nRF52840 Product Specification, Kapitel USBD — USB nur als Gerät.
  Fassungsnummer `ungeprueft`.

**Quelltext mit freier Lizenz** (alle abgerufen 24.10.2026)

- `github.com/sparkfun/SparkFun_FPC2534_Arduino_Library` — **MIT**, letzter Push
  04.12.2025. Beispiele für Anmeldung und Navigation über I²C und UART.
  Darin `src/sfTk/fpc_api.h` — **„Copyright (c) 2024 Fingerprint Cards AB",
  Apache-2.0**, „FPC AllKey Pro API … Command Interface for the Fingerprint
  Sensor Module fpc2534". Die Befehlsschnittstelle vom Hersteller, frei
  lizenziert. Dazu `src/sfTk/sfDevFPC2534I2C.h` mit der I²C-Vorgabeadresse
  **0x24**.
- `github.com/andpp/BM-Lite`, Zweig `master` — Apache-2.0.
  `BMLite_sdk/inc/fpc_hcp_common.h` trägt **„Copyright (c) 2020 Fingerprint
  Cards AB"**; `hcp_tiny.c`, `bmlite_if.c`, `bmlite_hal.h` tragen
  „Copyright (c) 2020 Andrey Perminov". Enthält eine HAL für den
  **nRF52840-DK** — unsere Zielplattform.
- `github.com/embenix/esp32c3_bmlite` —
  `components/fpc_bmlite/include/fpc_hcp_common.h`, derselbe FPC-Apache-Kopf.
  Unabhängige Bestätigung.
- `github.com/MikroElektronika/mikrosdk_click_v2`,
  `clicks/fingerprint4/lib_fingerprint4/include/fingerprint4.h` — **MIT**,
  „Copyright (C) 2026 MikroElektronika d.o.o.".
- `github.com/fingerprint-cards` — FPCs eigene Organisation: `libfprint`
  (LGPL-2.1, 24.04.2023), `optical_device_driver` (GPL-2.0, 01.07.2020),
  `capacitive_device_driver` (GPL-2.0, 30.06.2020). Bildsensoren, nicht
  verwendbar, aber als Beleg für FPCs Umgang mit Lizenzen.

**Offene Treiber für Ausbauteile** (nicht verwendbar, USB)

- `github.com/BlueApple233/OMS-fingerprint-linux` — OMS 33a7:2388,
  Match-on-Chip, 16 Vorlagen.
- `github.com/ITx-prash/libfprint-elan-04f3-0c6c` — Elan MOC2.
- `github.com/WilkerDev1/libfprint-egismoc-rs` — Egis, mit Microsoft SDCP.

**Händler-Momentaufnahmen, alle `ungeprueft`, Abrufdatum 24.10.2026**

- DigiKey: FPC2532AP 21,24 USD, FPC2534AP 23,85 USD, FPC2532AM 26,95 USD;
  Entwicklungskit 100026231, 150 USD, 10 Wochen.
- Mouser: FPC2532AM 27,28 USD (Menge 1), FPC2534AM 30,14 USD; Kits 140,12 bzw.
  164,15 USD.
- SparkFun: **SEN-29854 „Fingerprint Sensor – FPC2534 (Qwiic)", 59,95 USD,
  lagernd**; Antratek 72,54 EUR; RobotShop 59,95 USD.
- Mikroe: MIKROE-5465 „Fingerprint 4 Click" (trägt BM-Lite 100018754), 88 USD,
  Seitentext „no longer in stock" bei gleichzeitigem „InStock" in den
  Maschinendaten — widersprüchlich.
- DigiKey, BM-Lite 100018754: 29,00 USD, 159 Stück — laut Suchindex etwa
  2 400 Tage alt, also **veraltet**.

**Firmenlage**

- Zwipe/IDEX: Mengenabschluss 300 000 Sensoren (IDX3405) 2020; Bericht vom
  Januar 2025 über eine Dringlichkeitssitzung bei Zwipe zur Zahlungsfähigkeit.
  Beides `ungeprueft`, über den Suchindex.
- FPC AllKey Ultra, November 2025 — Ausführung mit Sicherheitschip;
  AdvanIDe als Vertriebspartner (Juni 2026). `ungeprueft`. **Nicht weiter
  verfolgt**, weil die Beschreibung dazu nicht auffindbar war; für Stufe 2 der
  interessantere Weg, falls der Sensor den Sicherheitschip ersetzen soll.
