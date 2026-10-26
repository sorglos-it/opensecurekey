# Der Rechenkern und der Durchsatz

Der Stick soll einen verschlüsselten Datenträger von 8 bis 16 GB bekommen.
Der heutige Rechenkern schafft am USB rund **400 kB/s**. Damit braucht 1 GB
42 Minuten und 8 GB knapp sechs Stunden. Diese Datei beantwortet eine einzige
Frage: **Welche Hardware wäre nötig, damit das benutzbar wird — und lohnt der
Wechsel?**

Die kurze Antwort steht am Ende: Faktor **7,5**, nicht Faktor 60. Und die
Wand steht nicht dort, wo alle hinsehen.

Seit die drei Betriebsarten des Datenträgers entschieden sind, hängen vier
Dinge hintereinander — USB, Speicherschnittstelle, Verschlüsselung, Hashen.
Welche davon zuerst klemmt, steht in einer eigenen Tabelle weiter unten. Die
Antwort vorweg: **heute das USB, nach jedem Wechsel der Speicher.**

## Was gemessen ist und was nicht

Das Wichtigste vorweg, weil sonst jede Zahl unten mehr verspricht, als sie
hält:

- **Gemessen:** die 400 kB/s am nRF5340-DK. Das ist die einzige echte Messung
  in dieser Datei.
- **Datenblatt:** was ein Hersteller seinem Baustein zuschreibt. Sagt, was der
  Baustein *kann*, nicht was er *liefert*.
- **Forenmessung:** was ein Fremder an seiner Platine gemessen hat. Ein
  Anhaltspunkt, kein Beweis — andere Platine, andere Karte, andere Puffer.
- **`ungeprüft`:** gerechnet oder geschätzt, von niemandem nachgemessen.

Es gibt keine Platine, keinen nRF54LM20 auf dem Tisch und keinen Datenträger.
**Alles unten außer den 400 kB/s ist Papier.**

## Die Falle zuerst: der nRF5340 bringt nichts

Der naheliegende Gedanke wäre, in der Familie zu bleiben und auf den nRF5340
zu gehen. Das wäre vergeblich.

**Befund:** Der nRF5340 hat denselben USB-Baustein wie der nRF52840. Sein
Datenblatt führt „USB 2.0 full speed (12 Mbps)". Ein Nordic-Mitarbeiter
schreibt im DevZone, die USBD-Peripherie sei zwar USB-2.0-konform, hänge aber
an einer **USB-1.1-PHY**. Die 400 kB/s wurden an einem nRF5340-DK gemessen —
an genau diesem Baustein.

**Begründung, warum es 400 kB/s sind und nicht 1,5 MB/s:** Ein Bulk-Endpunkt
darf bei Full Speed 64 Byte je Mikrorahmen (125 µs) übertragen. Das sind
512 000 B/s theoretisch. Mit Dateisystem, Befehlsköpfen und Quittungen bleiben
rund 400 kB/s übrig. Die 1,5 MB/s, die man aus „12 Mbit/s" rechnet, hat ein
Bulk-Endpunkt nie gesehen.

**Kein nRF52 und kein nRF53 hat USB High Speed.** Innerhalb dieser beiden
Familien gibt es nichts zu holen.

## Die sechs Anforderungen

Jeder Kandidat wird gegen dieselbe Liste geprüft. Fällt einer davon weg, ist
es ein anderes Gerät:

1. USB 2.0 High Speed (480 Mbit/s) oder besser
2. Bluetooth LE
3. NFC, Typ-4-Tag nach ISO 14443
4. Vorzertifiziertes Funkmodul beschaffbar
5. Zephyr / nRF Connect SDK
6. Kryptobeschleuniger, genug Flash und RAM; Platz in 27 mm Breite

## Die Kandidaten

Die Annahme war: „BLE, NFC und USB High Speed in einem Chip ist selten."
Das stimmt — aber es ist **nicht unmöglich**. Nordics zwei neueste Familien
haben alle drei.

| Kandidat | USB | BLE | NFC | Zertifiziertes Modul | Zephyr | Urteil |
| --- | --- | --- | --- | --- | --- | --- |
| nRF52840 *(heute)* | Full Speed | ja | ja | MDBT50Q, lieferbar | ja | 400 kB/s |
| nRF5340 | Full Speed, gleiche PHY | ja | ja | ja | ja | **kein Gewinn** |
| **nRF54LM20A/B** | **HS 480 Mbit/s** | ja | ja (NFCT) | BL54LM20B — **Vorserie** | ja | **einziger Ein-Chip-Weg** |
| nRF54H20 | HS 480 Mbit/s | ja | ja | **keines mehr** | ja | **Weg zu** |
| ESP32-P4 (+ C6) | HS | nur über C6 | **kein NFC** | nein | P4 praktisch nicht | fällt weg |
| STM32F723 / U5A | HS, interne PHY | **nein** | **nein** | — | ja | nur als zweiter Chip |
| i.MX RT | HS | **nein** | **nein** | — | ja | nur als zweiter Chip |

### nRF54H20 — technisch passend, aber nicht zu bekommen

Der nRF54H20 hätte alles: USB High Speed 480 Mbit/s, Bluetooth LE, NFC-A-Tag,
2 MB Flash, 1 MB RAM. Es gab auch ein vorzertifiziertes Modul, das BL54H20 von
Ezurio, 13,5 × 10 × 1,8 mm.

**Befund: dieser Weg ist zu.** Ezurio hat die nRF54H20-Module auf
**End of Life** gesetzt und verweist ausdrücklich auf die nRF54LM20B-Reihe als
Ersatz. Nordic selbst liefert den nRF54H20 nur an ausgewählte Kunden; die
Produktseite sagt „available soon", Distributoren nennen 32 Wochen Lieferzeit
(alles `ungeprüft`, Momentaufnahme 26.10.2026). Anforderung 4 fällt weg.

### nRF54LM20A/B — der einzige Chip, der alles kann

| | nRF54LM20A/B |
| --- | --- |
| USB | High Speed (USBHS), Datenblatt |
| Bluetooth LE | ja, dazu 802.15.4 |
| NFC | NFC-A-Listen (NFCT) |
| Kern | Cortex-M33 128 MHz + RISC-V-Nebenkern 128 MHz |
| Speicher | 2 MB NVM, 512 KB RAM |
| Krypto | CRACEN, mit Schutz gegen Stromverbrauchsanalyse |
| Gehäuse | CSP98 3,67 × 3,85 mm (Serienfertigung) |
| Zephyr | im nRF Connect SDK; ein Zephyr-Brett nutzt den USBHS-Treiber |

Das Modul dazu ist das **BL54LM20B von Ezurio: 11 × 8 × 1,8 mm**,
vorzertifiziert mit eigener Antenne, NFC-A-Tag und High-Speed-USB. Es ist
**kleiner** als das heutige MDBT50Q (15,5 × 10,5 × 2,05 mm) und passt damit
ohne Weiteres in die 27 mm Breite.

**Der Haken ist nicht technisch:** Das BL54LM20B ist **Vorserie**. Beta-Muster
ab Oktober 2026, kein veröffentlichter Preis, Zulassungen teils noch „pending"
(`ungeprüft`, Herstellerseite, Momentaufnahme).

## Die Wand steht hinter dem USB

Hier wird es unangenehm. **Das USB ist nicht die einzige Engstelle, und nach
einem Wechsel ist es nicht einmal mehr die engste.**

**Befund 1: Der nRF54LM20 hat kein SDMMC.** Seine schnellste serielle
Schnittstelle ist eine HS-SPI bis **32 MHz**; die sechs übrigen SPI laufen bis
8 MHz. Quad-SPI gibt es nur als *Software*-Peripherie auf dem RISC-V-Nebenkern
(`ungeprüft` — aus Datenblatt-Auszügen und der Herstellerseite, nicht im
Volltext nachgelesen). 32 MHz mit einer Datenleitung sind **4 MB/s
theoretisch**; mit Befehlen, Adressen und Wartezyklen bleiben realistisch rund
**3 MB/s** (`ungeprüft`, gerechnet).

Das betrifft auch den vorhandenen 64-MB-NOR-Flash: er hängt heute am echten
QSPI des nRF52840. Am nRF54LM20 müsste er über SPI oder über die
Software-Variante laufen — **langsamer als heute**. Das ist eine offene Frage
für die Platine, nicht für diese Datei.

**Befund 2: Auch echtes 4-Bit-SDMMC rettet es nicht.** Eine Forenmessung am
ESP32-P4 — ein Chip *mit* SDMMC — nennt am SD-Bus **4,1 MB/s beim Schreiben
und 2,5 MB/s beim Lesen** (4 KB Puffer), während dieselbe Maschine am USB
27 bis 29 MB/s schafft. Der Bus ist siebenmal schneller als das, was er
transportieren soll.

**Befund 3: Die Verschlüsselung ist nie gemessen worden.** Jedes Byte des
Datenträgers geht durch AES-GCM. Wie schnell CRACEN oder CryptoCell das am
Gerät tun, weiß niemand. Liegt es unter 3 MB/s, ist es die neue Wand.

**Zusammengefasst: nicht der Bus, der Speicher ist die Wand.** Ein Wechsel auf
USB High Speed verschiebt die Engstelle von 0,4 MB/s auf rund 3 MB/s — und
dort hört es auf, egal wie schnell das USB ist.

## Was der Wechsel wirklich bringt

Gerechnet mit 400 kB/s heute und 3 MB/s nach dem Wechsel. Beide
Spalten `ungeprüft` bis auf den Ausgangswert.

| Menge | heute (0,4 MB/s) | nRF54LM20 (~3 MB/s) | mit schnellem Speicher (25 MB/s) |
| --- | ---: | ---: | ---: |
| 1 GB | 42 min | **5,6 min** | 40 s |
| 8 GB | 5,6 h | **44 min** | 5,3 min |
| 16 GB | 11,1 h | **1,5 h** | 11 min |
| **Faktor** | 1 | **7,5×** | 60× |

Die dritte Spalte gibt es nicht zu kaufen: 25 MB/s bräuchten einen Rechenkern
mit echtem 4-Bit-SDMMC, und keiner davon hat BLE und NFC.

**Das ist die ehrliche Antwort auf die Gegenfrage.** Aus 42 Minuten werden
nicht 12, sondern knapp 6. Aus 5,6 Stunden werden 44 Minuten. Das ist kein
Rundungsfehler — **aus unbenutzbar wird benutzbar** —, aber es ist auch kein
USB-Stick. Ein gewöhnlicher Stick schreibt 8 GB in zwei bis vier Minuten.

## Vier Engstellen, und welche zuerst klemmt

Seit den drei Betriebsarten hängen vier Dinge hintereinander, und jedes Byte
muss durch alle vier: **USB → Speicherschnittstelle → Verschlüsselung →
Hashen**. Die langsamste bestimmt alles; die anderen drei sind dann egal.

| | USB | Speicher | AES-GCM | Hashen | **klemmt zuerst** |
| --- | ---: | ---: | ---: | ---: | --- |
| heute (nRF52840, microSD über SPI 1-Bit) | **0,4** | ~3 | ? | ~25,8 | **das USB** |
| nRF54LM20B (USB HS, weiter SPI 1-Bit) | ~38 | **~3** | ? | ~25,8 | **der Speicher** |
| STM32 + Funkmodul (USB HS, SDMMC 4-Bit) | ~38 | **~4** | ? | ~25,8 | **der Speicher** |

Alles in MB/s. `?` heißt: nie gemessen, siehe unten.

**Befund: Nach einem Wechsel auf USB High Speed ist das USB nicht mehr die
Engstelle — der Speicher ist es.** Und zwar in *jedem* Vorschlag, den diese
Datei macht. Der nRF52840 kann kein 4-Bit-SDIO und kein eMMC, eine microSD
läuft dort nur über SPI mit einer Datenleitung; der nRF54LM20 hat ebenfalls
kein SDMMC. Und selbst der Weg mit echtem 4-Bit-SDMMC bringt in der einzigen
Forenmessung, die es dazu gibt, nur 4,1 MB/s — ein Drittel mehr, nicht das
Zehnfache. (Dieser Wert ist eine fremde Messung an einer fremden Platine; ein
guter Kartenleser schafft an einer guten Karte deutlich mehr. Er ist der
einzige, den es gibt, darum steht er hier — `ungeprüft` bleibt er trotzdem.)

**Ein Rechenkern mit USB High Speed, der am SPI-Speicher verhungert, hat
nichts gewonnen — außer Faktor 7,5, und keinen Schritt mehr.**

## Was das Hashen kostet

Jeder geschriebene Block muss gehasht und in den Merkle-Baum eingerechnet
werden, jeder gelesene gegen den Baum geprüft. Das ist zusätzliche Rechenlast,
und in Betriebsart 3 ist es die *einzige*.

**Der Ausgangswert** (Herstellerangabe, von der Mastersitzung übergeben, hier
nicht nachgemessen): SHA-256 mit Beschleuniger schafft 4123 Byte in 0,1596 ms.

Das sind **25,8 MB/s** — 0,039 µs je Byte.

**Befund: Das Hashen ist nicht die Engstelle.** 25,8 MB/s liegen achtmal über
den ~3 MB/s des Speichers. Es wird erst dann zur Wand, wenn der Speicher mehr
als 25 MB/s liefert — und so einen Speicherweg hat keiner der Kandidaten.

**Was der Baum obendrauf kostet.** Ein Merkle-Baum über 16 GB mit Blöcken zu
4 KiB hat 4 194 304 Blätter, also **22 Ebenen**. Je Block kommen zu den
4096 Byte Nutzdaten 22 Knoten zu 64 Byte dazu:

| | Byte | Zeit |
| --- | ---: | ---: |
| Block selbst | 4 096 | 159 µs |
| 22 Knoten im Pfad | 1 408 | 55 µs |
| **je 4-KiB-Block** | **5 504** | **214 µs** |

Das sind **19,2 MB/s** statt 25,8 — der Baum kostet rund ein Drittel. Immer
noch sechsmal schneller als der Speicher. (`ungeprüft`, gerechnet; der
Aufwand je Aufruf des Beschleunigers ist dabei **nicht** eingerechnet, und bei
22 kurzen Aufrufen zu 64 Byte kann der spürbar werden. Wer es misst, misst
genau das.)

**Betriebsart 3 ist nicht billiger — sie ist nur halb so teuer.** Ohne
Verschlüsselung bleibt das Hashen vollständig stehen. Und weil beide Arten
weit über dem Speicher liegen, ist der Unterschied **am Gerät gar nicht zu
sehen**: alle drei Betriebsarten laufen mit rund 3 MB/s, weil der Speicher
sie ausbremst.

**Es sei denn, AES-GCM ist langsamer als 3 MB/s.** Dann — und nur dann —
wird Betriebsart 3 spürbar schneller als 1 und 2, und die Verschlüsselung ist
die Wand statt des Speichers. Das ist die eine Zahl, die niemand hat.

## Was eine fremde Karte beim Einstecken kostet

Die Frage ist berechtigt und die Antwort ist eindeutig.

**Vollständige Prüfung — geht nicht.** Den ganzen Baum beim Einstecken
nachzurechnen heißt, die ganze Karte zu lesen. Nicht mit 26 MB/s, wie man beim
Blick auf den Hash-Wert denkt, sondern mit den ~3 MB/s des Speichers:

| Karte | ganze Karte nachrechnen |
| --- | ---: |
| 1 GB | 5,6 min |
| 8 GB | 44 min |
| 16 GB | **1,5 h** |

Nicht zehn Minuten — **anderthalb Stunden**. Der Hash war nie das Problem,
das Lesen ist es. Unzumutbar ist zu freundlich formuliert.

**Pfadprüfung je Block — geht.** Einmal beim Einstecken die Wurzel aus dem
Sicherheitschip holen, danach je gelesenem Block nur dessen Pfad prüfen. Das
sind die 214 µs von oben, also 19,2 MB/s — der Speicher bleibt die Engstelle,
das Einstecken kostet **nichts außer dem einen Zugriff auf den Sicherheitschip**.

**Befund für die Konzeptsitzung: Nur die Pfadprüfung trägt. Die vollständige
Prüfung ist bei 16 GB keine Option, auch nicht als „einmal am Anfang".**

Zwei Dinge, die dabei auffallen und dort hingehören, nicht hierher:

1. **Die Knoten des Pfades müssen gelesen werden.** 22 Zugriffe zu 32 Byte,
   verstreut über die Karte — und wahlfreie kurze Zugriffe sind auf einer
   SD-Karte das Langsamste, was es gibt (die ESP32-P4-Messung nennt
   0,14 MB/s). Das macht die Pfadprüfung kaputt, wenn man nichts tut.
   **Abhilfe:** die oberen Ebenen des Baums im RAM halten. Die obersten
   12 Ebenen sind 4 095 Knoten zu 32 Byte = **128 KB** — das passt in die
   512 KB des nRF54LM20, aber nur knapp in die 256 KB des nRF52840. Dann
   bleiben 10 Zugriffe je Block statt 22.
2. **Beim Schreiben wandert die Wurzel in den Sicherheitschip.** Wenn das bei
   jedem Block geschieht, bestimmt nicht der Speicher den Durchsatz, sondern
   der SE051 — und seine Schreibzyklen sind endlich. Das ist eine
   Entwurfsfrage der Konzeptsitzung, keine der Hardware.

## Der andere Weg: zwei Chips

### ESP32-P4 mit C6 als Funkbegleiter

Die Aufteilung aus dem oltd-Projekt. Sie fällt hier aus zwei Gründen durch:

- **Es gibt kein NFC.** Weder der P4 noch der C6 haben eine NFC-Peripherie.
  Man bräuchte einen **dritten** Chip samt eigener Antenne — auf einer
  Platine von 27 × 59,55 mm, 0,8 mm dick, beidseitig bestückt.
- **Die Firmware wäre neu.** Der P4 ist ein ESP-IDF-Baustein; Zephyr trägt
  ihn praktisch nicht. Von 12 000 Zeilen bliebe fast nichts.

Dazu: kein vorzertifiziertes P4+C6-Modul, also Funkprüfung der eigenen
Platine. Drei von sechs Anforderungen fallen weg.

### STM32 (oder i.MX RT) mit dem heutigen Funkmodul daneben

Das ist der einzige Weg zu **echter** Geschwindigkeit, der heute kaufbar ist:
ein STM32F723 oder U5A — USB High Speed mit interner PHY, dazu SDMMC mit vier
Datenleitungen — und daneben das bereits gewählte MDBT50Q für BLE und NFC.

| | Was es kostet |
| --- | --- |
| Platz | zweiter Baustein plus Beschaltung, grob 50–80 mm² (`ungeprüft`) |
| Strom | zwei Kerne laufen, der Funkteil kann nicht mehr schlafen, solange der Hauptkern ihn abfragt |
| Firmware | der Stapel wird geteilt: FIDO, Smartcard und Tresor auf dem einen Chip, BLE und NFC auf dem anderen, dazwischen ein eigenes Protokoll — das ist neue, fehleranfällige Arbeit, die es heute nicht gibt |
| Zulassung | bleibt beim Funkmodul, **kein** neuer Funkmesslauf |

**Befund:** Technisch der beste Datenträger, wirtschaftlich der teuerste Weg.
Er kauft Geschwindigkeit mit der Komplexität, die dieses Gerät gerade nicht
haben soll — ein Sicherheitsschlüssel mit zwei Rechnern und einer Brücke
dazwischen hat eine größere Angriffsfläche als einer mit einem.

## Was ein Wechsel auf nRF54LM20 kostet

**Firmware — das meiste bleibt.** Gleicher Hersteller, gleiches SDK, gleiche
Schnittstellen. Was angefasst werden muss (`ungeprüft`, nicht ausprobiert):

| Teil | Was passiert |
| --- | --- |
| Anwendungscode, FIDO, Smartcard, Tresor, Laufwerk | bleibt |
| Brettdateien, Pinbelegung, Devicetree | neu |
| USB-Treiber | von USBD auf USBHS umstellen |
| Flash-Treiber | QSPI → SPI oder Software-QSPI |
| Krypto | CryptoCell CC310 → CRACEN |
| Partitionierung, MCUboot | neu gerechnet (2 MB statt 1 MB) |

**Platine — der Funkteil wird kleiner, nicht größer.** Neues Fußbild
(11 × 8 mm statt 15,5 × 10,5 mm), neue Pinbelegung, der Speicher muss neu
angebunden werden.

**Zulassung — fällt nicht.** Das Modul bringt seine Zertifikate mit, wie heute
auch. Das war die ausdrückliche Entscheidung im Entwurf und sie bleibt
eingehalten.

**Wie lange, bis der heutige Stand wieder steht:** nicht seriös zu beziffern.
Der Code ist nie auf Hardware gelaufen; der „heutige Stand" ist ein
Übersetzungsstand, und den erreicht man nach einem Brettwechsel in Tagen, nicht
Wochen. Die eigentliche Arbeit — das erste Mal am echten Gerät — steht in
beiden Fällen noch aus.

## Empfehlung

**Jetzt nicht wechseln. Wenn, dann auf nRF54LM20B — nie auf nRF5340 oder
nRF54H20.**

Drei Gründe, in dieser Reihenfolge:

1. **Der Gewinn ist Faktor 7,5, nicht Faktor 60.** 8 GB in 44 Minuten statt in
   5,6 Stunden. Das ist viel, aber es ist nicht das, was jemand von einem
   USB-Stick erwartet. Wer 8 GB mitnehmen will, nimmt weiter einen zweiten
   Stick.
2. **Die entscheidende Zahl fehlt noch.** Bevor irgendetwas gewechselt wird,
   muss der Durchsatz des gewählten Speicherbausteins samt AES-GCM gemessen
   sein. Liegt er unter 1 MB/s, ist der ganze Wechsel umsonst, und das lässt
   sich heute nicht ausschließen. Das **Hashen** ist dabei entlastet: mit
   25,8 MB/s (19,2 MB/s mit Merkle-Pfad) liegt es weit über allem anderen und
   ist in keinem der Vorschläge die Engstelle.
3. **Das Modul gibt es noch nicht.** BL54LM20B ist Vorserie. Eine Platine auf
   ein Beta-Muster zu zeichnen, dessen Preis und Zulassung offen sind, ist für
   dieses Projekt die unsicherere Variante.

**Und die unbequeme Möglichkeit:** Bei 400 kB/s bleiben und das Laufwerk
ehrlich als das führen, was es ist — eine **Dokumentenablage** mit den drei
portablen Programmen und ein paar Schlüsseldateien, nicht ein Datenträger. Das
kostet nichts, bricht nichts und hält jede Zusage ein, die heute gemacht ist.

## Was Thomas entscheiden muss

Nicht meine Entscheidung, darum hier als Frage:

1. **Ist der große Datenträger eine Zusage oder ein Wunsch?** Ist er Zusage,
   führt an nRF54LM20B kein Weg vorbei und man wartet auf die Serie. Ist er
   Wunsch, bleibt alles wie es ist.
2. **Reichen 44 Minuten für 8 GB?** Wenn nein, hilft nur der Zwei-Chip-Weg mit
   STM32 — und der kostet die Einfachheit des Geräts.
3. **Wer misst AES-GCM und den Speicher am Gerät?** Ohne diese Messung ist
   jede Entscheidung hier geraten. Es ist dieselbe Messung, die auch
   entscheidet, ob die drei Betriebsarten sich überhaupt unterschiedlich
   schnell anfühlen.
4. **Soll der Speicherweg neu bedacht werden?** Solange der Datenträger über
   SPI mit einer Datenleitung hängt, ist bei rund 3 MB/s Schluss — mit und
   ohne neuen Rechenkern. Das ist eine Frage an den Platinen-Strang, nicht an
   diesen hier.

## Quellen

| Art | Was | Link |
| --- | --- | --- |
| Datenblatt | nRF5340: USB 2.0 full speed (12 Mbps) | https://www.nordicsemi.com/-/media/Software-and-other-downloads/Product-Briefs/nRF5340-SoC-PB.pdf |
| Forum | Nordic DevZone: USBD hängt an USB-1.1-PHY; 64 Byte je Mikrorahmen | https://devzone.nordicsemi.com/f/nordic-q-a/102857/how-to-make-the-speed-of-usbd-on-nrf5340-reach-to-12mbps |
| Herstellerseite | nRF54LM20A: High-speed USB, NFC, 2 MB NVM, 512 KB RAM, HS-SPI, Gehäuse | https://www.nordicsemi.com/Products/nRF54LM20A |
| Datenblatt | nRF54LM20A/B v1.0: HS-SPI bis 32 MHz, sechs SPI bis 8 MHz, NFCT, CRACEN | https://www.mouser.com/pdfdocs/Nordic_nRF54LM20A_nRF54LM20B_DS.pdf |
| Herstellerseite | Ezurio BL54LM20B: 11 × 8 × 1,8 mm, vorzertifiziert, NFC, HS-USB, Vorserie | https://www.ezurio.com/product/bl54lm20b-series-bluetooth-le-802-15-4-nfc-edge-ai-npu |
| Herstellerseite | Ezurio BL54H20: End of Life, Verweis auf BL54LM20B | https://www.ezurio.com/product/bl54h20-series-multi-core-bluetooth-le-80215-4-nfc |
| Forum | ESP32-P4: SD-Bus 4,13 MB/s schreiben, 2,47 MB/s lesen (4 KB Puffer) | https://github.com/espressif/arduino-esp32/issues/12027 |
| Herstellerseite | ESP32-P4 USB-CDC rund 220–230 Mbit/s bei 8 KB Puffer | https://docs.espressif.com/projects/esp-faq/en/latest/software-framework/peripherals/usb.html |
| Datenblatt | STM32F7x3: USB High Speed ohne externe ULPI-PHY (AN4879) | https://www.st.com/resource/en/application_note/an4879-introduction-to-usb-hardware-and-pcb-guidelines-using-stm32-mcus-stmicroelectronics.pdf |
| Doku | Zephyr: Brett mit Nordic-USBHS-PHY | https://docs.zephyrproject.org/latest/boards/seeed/xiao_nrf54lm20a/doc/index.html |
| Herstellerangabe | SHA-256 mit Beschleuniger: 4123 Byte in 0,1596 ms (von der Mastersitzung übergeben, hier nicht nachgemessen) | — |

Stand 26.10.2026. Preise, Lagerstände und Lieferzeiten sind Momentaufnahmen
und hier bewusst nicht aufgeführt — sie ändern sich schneller als diese Datei.
