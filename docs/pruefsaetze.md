# Die Pruefsaetze der Firmware

Diese Datei sagt, welche Pruefsaetze es gibt, was jeder prueft, wie man ihn
ruft und was er dafuer braucht. Gemessen am 30.10.2026.

## Der eine Befehl

```powershell
powershell -ExecutionPolicy Bypass -File apps\firmware\test\alle.ps1
```

Das laesst **jeden** Satz laufen, in **beiden** Richtungen der RSA-Option,
und endet mit einem Ausstiegscode ungleich 0, sobald irgendetwas rot ist.
Die letzte Zeile nennt Gesamtzahl und Gesamtfehler.

In der WSL oder unter Linux geht es direkt:

```bash
cd apps/firmware/test && ./alle.sh
```

oder `make`, `make rsa`, `make norsa` im selben Ordner.

Nur einzelne Saetze, nur eine Richtung:

```bash
./alle.sh ctap ui          # nur diese zwei, beide Richtungen
./alle.sh RSA=0            # alle Saetze, nur die Fassung OHNE RSA
```

**Warum es diese Datei gibt.** Es gibt je Ordner eine Baubeschreibung, und
wer seine eigene laufen laesst, sieht die des Nachbarn nicht. Am
29./30.10.2026 hat ein Strang drei Dateien zum CTAP-Kern gelegt und nur in
*seinem* Makefile nachgetragen; die Saetze `ctap` und `transport` waren
danach 19 Stunden rot, und niemandem fiel es auf, weil niemand alles messen
konnte. `alle.sh` ist der eine Befehl, der es kann.

**Die Saetze werden gefunden, nicht aufgezaehlt.** Ein neuer Ordner unter
`apps/firmware/test/` mit einem `Makefile` oder einer `CMakeLists.txt` wird
ab dem ersten Lauf mitgemessen. Genau das Nachtragen ist letztes Mal
vergessen worden.

**Warum die Logik in bash steht und nicht in PowerShell.** `make`, `gcc` und
`cmake` gibt es nur in der WSL - also muss die Arbeit dort geschehen.
`alle.ps1` ist deshalb ein duenner Mantel, der nur `alle.sh` aufruft. Zwei
Baubeschreibungen, die sich widersprechen, haben den Prueflauf des
SE050-Treibers am 28.10.2026 schon einmal still gebrochen.

**Die WSL erreicht keine Netzlaufwerke.** Liegt der Arbeitsbaum auf `Z:`,
bricht `alle.ps1` mit einer Meldung ab. Es wird ein Arbeitsbaum unter
`D:\claude\working` gebraucht.

## Was ein gruener Lauf heisst

Uebersetzt und **am PC** ausgefuehrt - Stufe 2 von 4 (geschrieben /
uebersetzt / im Abbild / am Geraet gelaufen). Am Geraet ist nichts davon
gelaufen; der SE051C2 ist bestellt und nicht geliefert. Jeder Satz laeuft
gegen Attrappen im RAM, nicht gegen Hardware.

`tools/check.py` sagt ueber die Firmware **nichts**: es ueberspringt
`firmware/build`, `firmware/tests` und `firmware/format`.

## Die Saetze

12 Saetze. Die folgende Tabelle hat 11; `wallet` steht darunter einzeln,
weil daran zur Messzeit noch gearbeitet wurde.
Alle Zahlen vom 30.10.2026, gcc 14.2.0 (Debian) in der WSL, mit
AddressSanitizer und UndefinedBehaviorSanitizer, wo die Baubeschreibung sie
einschaltet.

| Satz | prueft | Bau | Faelle RSA=1 | Faelle RSA=0 | Fehler | Code |
|---|---|---|---|---|---|---|
| `applets` | Kartenanwendungen (PIV, OpenPGP, OTP, RSA-Karte, Attestierung, CSR, statische Kennwoerter, Seriennummer, APDU, CCID) | Makefile | 1818 | 1542 | 0 | 0 |
| `authz` | Rechteschicht (Berechtigungen, Nachweise, DER, Schluessel, Zeitstempel) | Makefile | 255 | 255 | 0 | 0 |
| `boot` | Firmware-Update (Zustand, Signatur, Rueckdreh-Schutz, Abbruch, Notfallweg, Tresor bleibt unberuehrt) | CMake | 793 | 793 | 0 | 0 |
| `ctap` | CTAP-Kern (CBOR, HID, PIN, U2F, Nachweisverwaltung, Signaturzaehler, Einstellungen) | Makefile | 1425 | 1425 | 0 | 0 |
| `guard` | Dongle- und Server-Befehle 14 bis 18 (Nachrichten, Knoten, Faelschungen) | Makefile | 539 | 539 | 0 | 0 |
| `msc` | Laufwerk (SCSI-Leseweg, Schreibschutz) | Makefile | 223 | 223 | 0 | 0 |
| `pqc` | PQC-Modul (ML-KEM-768, Keccak, Polynome, Hybridverfahren) | Makefile | 582 | 582 | 0 | 0 |
| `se050` | SE050-Treiber (T=1, TLV, CRC, SCP03, Zaehler, RSA, Inbetriebnahme, Fehlerwege) | Makefile | 1192 | 1192 | 0 | 0 |
| `store` | Tresor (Kern, Seiten, Protokollkette, Sicherung, Shamir, Umzug, Verschleiss, Loeschen, Datentraeger und seine drei Betriebsarten) | CMake | 88826 | 88826 | 0 | 0 |
| `transport` | Funkteil und USB (NFC, ISO-DEP, NDEF, BLE, USB-HID, Tastatur, YubiKey-Rahmen) | Makefile | 1367 | 1367 | 0 | 0 |
| `ui` | Anzeige (Bild, Schrift, Text, fuenf Sprachen, Taster, PIN am Geraet, Bestaetigung) - beide Schirmgroessen | Makefile | 9314 | 9314 | 0 | 0 |
| | **Gesamt** | | **106334** | **106058** | **0** | **0** |

Insgesamt **212 392 Faelle, 0 Fehler, Ausstiegscode 0.**

### `wallet` - gemessen, aber nicht aus diesem Lauf

| Satz | prueft | Bau | Faelle RSA=1 | Faelle RSA=0 | Fehler | Code |
|---|---|---|---|---|---|---|
| `wallet` | Geldboerse (BIP32, BIP39, Bitcoin, Ethereum, PSBT, RLP, Bestaetigung, APDU, Wortliste) | Makefile | 1323 | 1323 | 0 | 0 |

`wallet` steht hier getrennt, weil daran gleichzeitig gearbeitet wurde.
Gemessen wurde nicht der Arbeitsstand jenes Strangs, sondern der Stand, der
beim Einfuehren von `main` in diesem Arbeitsbaum lag - eine Momentaufnahme,
die mit dem naechsten Commit der Geldboerse ueberholt ist. `alle.sh` findet
den Satz von allein und misst ihn ab sofort mit; er braucht nichts
Besonderes.

Mit `wallet` sind es **213 715 Faelle, 0 Fehler** in beiden Richtungen.

Nur `applets` zaehlt in den beiden Richtungen verschieden (1818 gegen 1542):
dort haengen 276 Faelle an der RSA-Karte. `ctap` und `transport` zaehlen
gleich, uebersetzen aber verschiedenen Code - `CONFIG_OSK_RSA` steht in
`src/classes/ctap/osk_ctap_core.c`.

## Welche Saetze die RSA-Option ueberhaupt betrifft

`CONFIG_OSK_RSA` kommt in genau zwei Quelldateien vor:

- `src/classes/applets/osk_rsa_card.c` (und ihre Kopfdatei)
- `src/classes/ctap/osk_ctap_core.c` (und `osk_ctap_internal.h`)

Gebunden werden die von `applets`, `ctap` und `transport` - und genau diese
drei kennen die Option `RSA=` schon. **In den anderen acht Saetzen wurde
nichts nachgetragen**, weil die Option ihren Ordner nicht betrifft: ein
`-DCONFIG_OSK_RSA=1` wuerde dort keine einzige Zeile anders uebersetzen und
nur so aussehen, als waere etwas geprueft. `alle.sh` laesst sie trotzdem in
beiden Richtungen laufen - der Beleg, dass die Option sie nicht beruehrt,
sind die gleichen Zahlen in der Tabelle.

`se050` hat eine eigene RSA-Datei (`se050_rsa.c`, `test_rsa.c`), die aber
nicht an `CONFIG_OSK_RSA` haengt: der Chiptreiber kann RSA immer.

## Zusatzziele - laufen bei `alle.sh` NICHT mit

Ein uebersprungener Beleg sieht aus wie ein erbrachter, darum stehen sie
hier einzeln. Keiner davon ist Teil des Gesamtlaufs.

| Aufruf | was es tut | braucht |
|---|---|---|
| `make -C applets fuzz` | Fuzzlauf ueber APDU- und CCID-Parser, `FUZZ_SECS` lang | Zeit |
| `make -C ctap fuzz` | Fuzzlauf ueber CBOR und HID | Zeit |
| `make -C guard fuzz` | Fuzzlauf ueber den Nachrichtenrahmen | Zeit |
| `make -C msc fuzz` | Fuzzlauf ueber SCSI | Zeit |
| `make -C transport fuzz` | Fuzzlauf ueber ISO-DEP, NDEF, YubiKey-HID | Zeit |
| `make -C applets openssl` | Beleg mit einem **fremden** Programm: openssl liest und bestaetigt die von der Firmware gebaute Zertifikatsanfrage | `openssl`, `python3` |
| `make -C pqc budget` | rechnet Flash- und RAM-Bedarf des PQC-Moduls nach | - |
| `guard/mutanten.py` | setzt Mutanten in den Waechter und prueft, dass der Satz anschlaegt | `python3` |
| `make -C transport yk` | YubiKey-Teil einzeln | - |

`ui` braucht `python3` auch im normalen Lauf: das Ziel `generated`
vergleicht die erzeugten Schrift- und Texttabellen mit `generate.py` und den
fuenf Sprachdateien.

## Gegenprobe: bindet jeder Satz alle Quellen seines Ordners?

Genau hier lag der 19-Stunden-Fehler: eine fehlende Datei in einer
Baubeschreibung faellt erst beim Binden auf, und nur, wenn jemand den Satz
ueberhaupt baut. Abgeglichen wurde je Satz, welche `.c`-Dateien die
Baubeschreibung bindet und welche der Quellordner wirklich hat.

**Ergebnis: kein Satz laesst eine Quelldatei liegen, die er binden muesste.**
Jede Auslassung ist gewollt:

| Satz | Quellordner | nicht gebunden | Grund |
|---|---|---|---|
| `applets` | `src/classes/applets/` | - | bindet alle 14 |
| `authz` | `src/classes/authz/` | - | bindet alle 5 |
| `boot` | `boot/` | - | bindet alle 6 |
| `ctap` | `src/classes/ctap/` | - | bindet alle 11 |
| `guard` | `src/classes/applets/guard/` | - | bindet alle 4 |
| `msc` | `src/classes/transport/` | alles ausser `usb_msc.c` | der Rest gehoert zu `transport` und `applets` |
| `pqc` | `src/classes/pqc/` | - | bindet alle 4 |
| `se050` | `src/classes/se050/` | `se050_crypto_psa.c`, `se050_crypto_cng.c` | andere Wege: PSA braucht das nRF Connect SDK, CNG ist der Windows-Weg; hier laeuft `se050_crypto_openssl.c` |
| `se050` | | `se050_commission_platform.c` | die drei Platinenhaken setzt `test_platform.c` im Pruefsatz selbst |
| `store` | `src/classes/store/` | `osk_crypto_psa.c`, `osk_flash_qspi.c` | Hardware-Wege; ersetzt durch `fake_crypto.c` und `fake_flash.c` |
| `transport` | `src/classes/transport/` | `usb_ccid.c`, `usb_msc.c` | `usb_ccid.c` prueft `applets`, `usb_msc.c` prueft `msc` |
| `ui` | `src/classes/ui/`, `src/views/` | `osk_ui_port_nrf.c` | Hardware-Weg; ersetzt durch `fake_panel.c` |

## Ein offener Punkt: store/build_and_run.ps1 ist zurueck

Der `store`-Satz hat **zwei** Baubeschreibungen: `CMakeLists.txt` und
`build_and_run.ps1`. Sie stimmen nicht ueberein. In der PowerShell-Fassung
fehlen sieben Dateien:

`volume_core.c`, `volume_recv.c`, `volume_tree.c`, `fake_card.c`,
`fake_vcrypto.c`, `test_volume.c`, `test_volume_modes.c`

Wer `build_and_run.ps1` laufen laesst, misst den **ganzen Datentraeger
nicht** und sieht trotzdem gruen. `CMakeLists.txt` ist die vollstaendige
Fassung, und `alle.sh` nimmt sie. Die PowerShell-Fassung ist hier nicht
angefasst worden, weil sie den Uebersetzer der Visual Studio Build Tools
braucht und ein Nachtrag, den niemand hat laufen lassen, nichts wert ist.
Entweder wird sie nachgezogen und einmal gebaut, oder sie wird geloescht -
`alle.ps1` deckt ihren Zweck ab.

## Dass der Weg wirklich rot wird

Eine Pruefung, die man nicht zum Anschlagen gebracht hat, ist keine. Der
Gesamtlauf wurde absichtlich dreimal gebrochen und jedes Mal wieder
zurueckgedreht:

| Bruch | wo | was `alle.sh` meldete |
|---|---|---|
| Eintrag `osk_sigcount.c` aus der Baubeschreibung entfernt (genau der Fehler vom 30.10.2026) | `transport/Makefile` | undefined reference to osk_sigcount_next, Code 2, ROT in beiden Richtungen, Ausstiegscode 1 |
| eine Pruefung fehlschlagen lassen (`T_TRUE(..., 1 == 2)`) | `msc/test_msc.c` | 224 Faelle, **1 Fehler**, Code 2, ROT in beiden Richtungen, Ausstiegscode 1 |
| Quelldatei `boot_state.c` aus der CMake-Liste entfernt | `boot/CMakeLists.txt` | undefined reference to osk_boot_state_store, Code 2, ROT, Ausstiegscode 1 |

Auch der PowerShell-Mantel ist in beiden Richtungen geprueft: `alle.ps1`
gibt bei gruen 0 und bei rot 1 weiter (nachgestellt mit demselben Bruch in
`msc/test_msc.c`). Und die Netzlaufwerk-Sperre schlaegt an: eine Kopie von
`alle.ps1` auf `Z:` bricht mit der Meldung "liegt auf einem Netzlaufwerk"
und Ausstiegscode 2 ab, statt die WSL erfolglos uebersetzen zu lassen.

Der dritte Bruch war noetig, weil CMake-Saetze in `alle.sh` einen anderen
Weg nehmen als Makefile-Saetze - ein Weg, den man nicht zum Anschlagen
gebracht hat, ist keiner.

Dabei ist noch etwas aufgefallen: der erste Entwurf von `alle.sh` las nur
Zeilen der Form `name: 12 Faelle, 0 Fehler`. `boot`, `store` und `ui` melden
aber `793 Pruefungen, 0 Fehler` - mit der Zahl am Zeilenanfang. Diese drei
Saetze meldeten deshalb **"0 Faelle, Code 0, gruen"**, obwohl sie 793, 88826
und 9314 Faelle wirklich gelaufen waren. Seitdem liest `alle.sh` beide
Woerter, und **0 Faelle gilt als ROT**: ein Satz, der keine Fallzahl
gemeldet hat, ist entweder nicht gelaufen oder wird nicht verstanden, und in
beiden Faellen ist nichts bewiesen.

## Der Ausstiegscode entscheidet

In diesem Projekt kommen eine Ausgabe, die "ok" sagt, und ein Ausstiegscode
ungleich 0 zusammen vor. `alle.sh` nennt darum je Satz und je Richtung die
drei Zahlen - Faelle, Fehler, Code - und urteilt nur gruen, wenn **Code 0**
*und* **0 Fehler** *und* **mehr als 0 Faelle** zusammentreffen.
