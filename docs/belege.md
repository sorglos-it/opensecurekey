# Die Belege, nachgerechnet

Sechs Straenge haben am 29./30.10.2026 gemeldet, ihre Umsetzung sei gegen
etwas Fremdes geprueft. Dieses Blatt ist die Gegenrechnung: geschrieben von
jemandem, der keine der sechs Umsetzungen gebaut hat. Gemessen am 30.10.2026.

**Der Unterschied, auf den alles ankommt.** Ein **Rundlauf** ist der eigene
Code, der sich selbst zustimmt — "verschluesselt und wieder entschluesselt"
geht auch mit einer falschen Umsetzung auf, solange beide Seiten denselben
Fehler machen. Ein **amtlicher Testwert** ist eine Zahl, die jemand anders
veroeffentlicht hat. Dazwischen liegen zwei weitere Stufen, und sie sind
nicht gleich viel wert:

| Stufe | Was es ist | Was es taugt |
|---|---|---|
| **1 amtlich** | Zahl aus der Beschreibung selbst oder aus der Pruefsammlung der Behoerde | Der starke Beleg |
| **2 fremdes Programm** | Ein anderes Programm liest oder rechnet das, was hier geschrieben wurde | Fast so stark; deckt auch ab, was die Beschreibung offen laesst |
| **3 nachgerechnet** | Dieselbe Rechnung, aber von Hand und mit anderem Werkzeug | Fuettert den Verdacht, beweist keine Lesart der Beschreibung |
| **4 Rundlauf** | Der eigene Code gegen sich selbst | Kein Beleg |

Jede Zeile unten sagt: welche Stufe, woher die Zahl kommt, **und ob ich sie
selbst zum Laufen gebracht habe**. Wo nichts gelaufen ist, steht das da — was
nicht gelaufen ist, ist nicht geprueft.

---

## Die Tafel

| Baustein | Behauptet | Woher der Vergleichswert kommt | Stufe | Deckt ab | Deckt NICHT ab | Selbst gelaufen? |
|---|---|---|---|---|---|---|
| **ML-KEM-768**<br>`apps/firmware/src/classes/pqc/` | 34 amtliche NIST-Werte, 582 Faelle | **usnistgov/ACVP-Server**, `ML-KEM-keyGen-FIPS203` und `ML-KEM-encapDecap-FIPS203`, Satz ML-KEM-768. Ich habe die beiden Dateien selbst geladen (559 kB / 1,47 MB) und `tools/mkkat_pqc.py` neu laufen lassen: **jede Zahl in `kat_mlkem768.h` kommt genau so wieder heraus** | **1** | Schluesselerzeugung (25 Faelle, d/z hinein, SHA3-256 ueber ek und dk heraus), Kapseln (3), Entkapseln mit stiller Verwerfung (4) | Die Mischung mit X25519 (eigener Entwurf, dafuer gibt es keine amtlichen Zahlen); die 20 NIST-Faelle `encapsulationKeyCheck`/`decapsulationKeyCheck`, die genau die Eingangspruefung messen, die der Strang mit **eigenen** Faellen prueft; 22 der 25 Kapsel-Faelle; 6 der 10 Entkapsel-Faelle; nichts davon am Geraet | **Ja.** `make` in der WSL: `pqc: 582 Faelle, 0 Fehler`, GRUEN |
| **PKCS#10**<br>`applets/` | openssl 3.5.7 liest fuenf Faelle und bestaetigt die Unterschrift | **openssl 3.5.7** (WSL), ein Programm, das nichts von diesem Projekt weiss | **2** | Der Aufbau nach RFC 2986: Reihenfolge der Namensteile, die Erweiterungen am richtigen Platz, und dass die Unterschrift ueber **genau** den Teil geht, der unterschrieben sein muss — bei p256, p384, rsa2048, rsa3072, rsa4096 | **Wer unterschreibt.** Unterschrieben hat openssl, nicht dieses Projekt — der SE051C2 ist nicht da. Der Beleg zeigt, dass der Rahmen stimmt, nicht dass das Geraet ihn fuellen kann | **Ja, zweimal.** `make openssl` baut und meldet 5 von 5. Danach habe ich openssl selbst auf die fertigen Dateien gesetzt: `openssl req -in <datei> -verify` sagt bei allen fuenf `Certificate request self-signature verify OK` |
| **CXF**<br>`apps/desktop/` | Gegen die Beschreibung geprueft | `apps/desktop/tests/vektoren/cxf-fido-beispiel.json`. Ich habe die FIDO-Nachtragsliste vom 09.03.2026 geladen (`cxf-v1.0-ps-errata-20260309.html`, 840 kB) und das Beispiel aus Anhang A herausgeschnitten: **es ist Byte fuer Byte dieselbe Datei**, 29 465 Byte, auch als JSON gleich | **1 fuer die Daten, 2 fehlt** | Dass die Beispieldatei der Beschreibung vollstaendig gelesen wird: 14 Eintraege, 15 Zugangsdaten, alle vorkommenden Arten, die durchgetragenen Erweiterungen | **Ein fremdes Programm.** Es gibt keines: CXF ist seit 8/2025 Proposed Standard, die Verwaltungen liefern nicht aus. Was dieses Programm **schreibt**, hat also niemand sonst gelesen — nur die eigene Lesart der Beschreibung sagt, dass es stimmt. `docs/cxf.md`:347–351 sagt das selbst und klar | **Ja.** `go test ./tests/` laeuft gruen |
| **Geldboerse — BIP32**<br>`wallet/` | Vektoren 1–5 | **bip-0032.mediawiki** (bitcoin/bips). Alle **44** `xpub`/`xprv` aus `test_bip32.c` stehen wortwoertlich in der Beschreibung; dazu die Saaten. Die vier Ed25519-Zahlen stammen nicht aus BIP-32, sondern aus **SLIP-0010** ("Test vector 1 for ed25519") — auch das habe ich geladen und nachgesehen: alle vier stehen dort | **1** | Ableitung gehaertet und offen, fuehrende Nullen (Vektor 3/4), die Liste ungueltiger Schluessel (Vektor 5), Ed25519 nach SLIP-0010 | Vektor 5 prueft Abweisung, nicht Rechnung; nichts am Geraet | **Ja.** `make` in `test/wallet`: alle Prueflaeufe ok |
| **Geldboerse — Ethereum**<br>`wallet/` | „das EIP-712-Beispiel" | **Hier traegt der Beleg nicht.** Die alte Transaktion ist wirklich das Beispiel aus **EIP-155** (Bytes und Hash stehen dort, nachgesehen). Der EIP-712-Fall ist **kein Beispiel aus EIP-712**: Bereich = Byte 0…31, Nachricht = Byte 32…63, frei gewaehlt. Ich habe Keccak-256 nach der Beschreibung selbst gebaut (gegengeprueft an zwei bekannten Werten) und `71d794…0228` nachgerechnet — die Zahl **stimmt**, sie ist nur von niemandem veroeffentlicht. Dasselbe fuer EIP-1559 (`61b515…eeea`) | **1 fuer EIP-155, 3 fuer EIP-712 und EIP-1559** | Dass die Verkettung `0x19 0x01 ‖ Bereich ‖ Nachricht` richtig gehasht wird | **Alles, was EIP-712 eigentlich ausmacht**: `typeHash`, `encodeData`, der Bereichstrenner. Das Geraet rechnet davon nichts — es unterschreibt blind und sagt das auch. „Das EIP-712-Beispiel geprueft" ist darum zu viel gesagt | **Ja**, und selbst nachgerechnet |
| **Dateiwerkzeug — OpenPGP**<br>`apps/desktop/` | Strang J meldete: gpg 2.4.9 kann die Nachricht **nicht** oeffnen, der fremde Beleg **fehlt** | **Der Beleg fehlt nicht — er stand bereit.** Auf dieser Maschine liegt **`sq` 1.3.1** (sequoia-openpgp 2.0.0), eine fremde Umsetzung, die RFC 9580 **kennt**. Sie liest die Huelle richtig (PKESK Fassung 6, X25519, anonymer Empfaenger, 76 Byte; SEIPD Fassung 2, AES-256, GCM, Stueckgroesse 65536). Und mit bekanntem Sitzungsschluessel hat sie den Rumpf **entschluesselt**: 3000 Byte Klartext, `cmp` sagt Byte fuer Byte gleich | **2** | Alles, was geschrieben wird: Paketfassungen, AES-256-GCM, die Stueckelung, die Abschlussmarke, das Literal-Paket — bestaetigt von einem Programm, das nichts von hier weiss | Den Weg vom Empfaengerschluessel zum Sitzungsschluessel (die Schluesselentpackung) — dafuer braeuchte `sq` den privaten Schluessel als Datei, und den gibt es nach Entwurf nicht. Nichts am Geraet | **Ja.** Siehe unten, „Wie der fehlende Beleg beschafft wurde" |
| **Dateiwerkzeug — age**<br>`apps/desktop/` | wie oben | **Fehlt weiter.** `age` und `rage` sind auf dieser Maschine nicht da, weder unter Windows noch in der WSL; openssl kennt das Format nicht. Die inneren Rechnungen sind ueber **RFC 8439** (ChaCha20, Poly1305) und **RFC 3394** (AES Key Wrap) amtlich belegt, das Format selbst nicht | **1 fuer die Bausteine, kein Beleg fuer das Format** | ChaCha20-Poly1305 und AES Key Wrap gegen veroeffentlichte Zahlen | Dass die age-Huelle so aussieht, wie `age` sie erwartet. Das ist **Rundlauf** | **Nein.** Aber einen Schritt entfernt: Debian trixie hat `age` 1.2.1 im Vorrat. Ein `apt install age` wuerde den Beleg beschaffen — ich habe nichts installiert, das ist eine Entscheidung fuer die Leitsitzung |
| **Dongle**<br>`shared/osk-guard/` | Ed25519 in fuenf Sprachen; .NET laeuft nicht (kein dotnet), PHP ohne sodium nicht | Die Hashwerte in `tests/vectors.json` sind die bekannten SHA-2-Werte und stimmen gegen CPython `hashlib` (9 von 9, selbst nachgerechnet). Die **21 Unterschriften- und 24 Lizenzvektoren sind mit Go `crypto/ed25519` erzeugt** — eine fremde Umsetzung, aber **keine amtliche**. Amtliche RFC-8032-Vektoren kommen nirgends vor | **2**, fuer Go: **4** | C, Python, .NET und PHP haben jede eine eigene Pruefrechnung (bzw. libsodium bei PHP) — fuer sie ist der Go-Wert ein echter Quervergleich | **Go selbst.** `go/osk_guard.go` ruft `crypto/ed25519.Verify` — dasselbe Programm, das die Vektoren erzeugt hat. Fuer Go ist das ein Rundlauf und kein Beleg. Und: **kein einziger amtlicher Ed25519-Wert**, obwohl RFC 8032 Abschnitt 7.1 welche veroeffentlicht | **Vier von fuenf ja, siehe unten** |

---

## Was beim Dongle wirklich gelaufen ist

Die gemeldete Lage war falsch. Gemessen am 30.10.2026 auf dieser Maschine:

| Sprache | Gemeldet | Gemessen | Wie |
|---|---|---|---|
| **Go** | laeuft | **laeuft** | `tests/run.py`, voll und ohne-ecdh, je 7 Ergebniszeilen |
| **Python** | laeuft | **laeuft** | `python/test_guard.py`: 67 Pruefungen, OK |
| **PHP** | **laeuft nicht** (kein sodium) | **laeuft** | `tests/run.py` laedt sodium selbst nach (`-d extension_dir=…/ext -d extension=sodium`). Mit denselben Schaltern laeuft auch `php/test_guard.php` durch: 24 Lizenzen, 21 Unterschriften, alles gruen. Nur der **nackte** Aufruf scheitert, und genau der stand im Bericht |
| **.NET** | **laeuft nicht** (kein dotnet) | **laeuft** | `tests/dotnet-test.py`: 9 Hashwerte, 21 Unterschriften, 24 Lizenzen, alles gruen. Ohne SDK, ueber den Roslyn-Uebersetzer aus den Build-Tools gegen die .NET-8-Laufzeit — `tests/run.py` kann diesen Weg, `dotnet --version` kann er nicht, und davon kam die Meldung |
| **C** | laeuft | **halb** | `c/test_guard` baut und laeuft **in der WSL** gruen (9 Hashwerte, 21 Unterschriften, 24 Lizenzen, 6 aufgezeichnete Antworten). Auf Windows uebersprungen: kein C-Uebersetzer im Pfad. **Und `c/example.c` uebersetzt nirgends** — siehe Befund 1 |

Dazu gelaufen: `tests/vertrag.py` — 204 Faelle, 0 Fehler (alle fuenf Sprachen
gegen `contract.json`). Und `tests/mutanten.py` — 14 von 14 Kontrollen
verhalten sich wie verlangt: jede eingebaute Verletzung wird von mindestens
einem Pruefsatz rot gemeldet, die harmlose Umbenennung von keinem. Das ist
der Beleg, dass die Pruefsaetze nicht nur gruen sind, sondern auch rot werden
koennen.

---

## Wie der fehlende OpenPGP-Beleg beschafft wurde

Damit es jemand nachmachen kann. `sq` 1.3.1 liegt in der WSL.

1. Eine Probedatei schreiben lassen (`FileEncrypt`, Format PGP, 3000 Byte Muster).
2. `sq packet dump probe.pgp` — liest PKESK Fassung 6 / X25519 / anonymer
   Empfaenger / 76 Byte und SEIPD Fassung 2 / AES-256 / GCM / Stueckgroesse
   65536. gpg 2.4 sagt an derselben Stelle nur „unknown version".
3. Fuer den Inhalt: den Datenteil mit einem **bekannten** Sitzungsschluessel
   schreiben (`pgpDataPacket`), dann
   `sq decrypt --session-key 9:<hex> --output klar.bin body.pgp`.
   Ergebnis: 3000 Byte, `cmp` gegen den Klartext gleich.

Die beiden Messhilfen dafuer lagen im Arbeitsbaum und sind wieder weg; im
Pruefsatz steht dieser Aufruf **nicht**. Er sollte dort stehen — das ist
Befund 3.

---

## Befunde

Keiner davon ist hier behoben. Ich messe, ich repariere nicht.

**Befund 1 — `shared/osk-guard/c/example.c` uebersetzt nicht, nirgends.**
Zwei Stellen sind beim Bearbeiten verrutscht: in Zeile 452 steht das Ende
einer `if`-Bedingung und der Anfang einer **zweiten, aelteren** auf derselben
Zeile (`… !step_agree(guard, &s)) {    if (!step_seal(guard, &s) || …`), und in
Zeile 468 klebt `nap(200);` hinter einem `printf`, gefolgt von einem
**doppelten** Praesenz-Block. Dadurch hat `run()` eine offene Klammer zu viel,
`main` landet darin, und gcc bricht ab:
`example.c:494:1: error: expected declaration or statement at end of input`.
Das ist nicht plattformabhaengig. Gefunden hat es niemand, weil auf dem
Windows-Rechner kein C-Uebersetzer im Pfad liegt und `tests/run.py` C darum
ueberspringt — der Prueflauf `c/test_guard` ist von dem Fehler nicht
betroffen und bleibt gruen. **Eine der fuenf Sprachen hat also ein Beispiel,
das sich nicht einmal uebersetzen laesst.**

**Befund 2 — „das EIP-712-Beispiel" ist keines.** `apps/firmware/test/wallet/test_eth.c`:206–214
rechnet mit einem frei gewaehlten Bereich und einer frei gewaehlten Nachricht.
Die Zahl stimmt (ich habe sie unabhaengig nachgerechnet), aber sie ist nicht
veroeffentlicht, und der Fall prueft nur die Verkettung `0x1901`, nicht die
Typ-Hashes, die EIP-712 ausmachen. Im Bericht als „das EIP-712-Beispiel"
gefuehrt zu werden, ist eine Stufe zu hoch. Der Kommentar in der Datei selbst
ist dagegen ehrlich („unabhaengig in Python nachgerechnet").

**Befund 3 — der OpenPGP-Beleg laeuft nicht von selbst mit.** Er ist jetzt
erbracht (oben), aber `apps/desktop/tests/dateiwerkzeug_test.go`:500 ruft nur
`gpg`, und dieses gpg kann RFC 9580 nicht. Ein Aufruf von `sq` daneben — mit
`t.Skip`, wenn `sq` fehlt — wuerde den starken Beleg bei jedem Prueflauf
mitnehmen. Ich habe ihn nicht ergaenzt; `apps/desktop` gehoert gerade anderen.

**Befund 4 — 20 amtliche NIST-Faelle liegen ungenutzt.** Die
`encapDecap`-Datei enthaelt neben den benutzten Gruppen auch
`encapsulationKeyCheck` (10 Faelle) und `decapsulationKeyCheck` (10 Faelle).
Genau die messen die Eingangspruefung — „nicht reduzierter oeffentlicher
Schluessel", „geheimer Schluessel passt nicht zu seinem Hash" —, die
`test_mlkem.c` heute mit **eigenen** Faellen prueft. `tools/mkkat_pqc.py`
nimmt sie nicht mit. Dazu: 22 von 25 Kapsel-Faellen und 6 von 10
Entkapsel-Faellen bleiben liegen (`[:3]`, `[:2]`). 32 von 70 verfuegbaren
amtlichen Faellen sind in Gebrauch.

**Befund 5 — der Dongle hat keinen einzigen amtlichen Ed25519-Wert.** Alle 21
Unterschriften- und 24 Lizenzvektoren sind mit Go `crypto/ed25519` erzeugt.
Fuer C, Python, .NET und PHP ist das ein echter Quervergleich. Fuer **Go** ist
es ein Rundlauf: `go/osk_guard.go`:257 prueft mit genau derselben Bibliothek,
die die Zahlen gemacht hat. RFC 8032 Abschnitt 7.1 veroeffentlicht Vektoren;
sie kommen im Repo nicht vor.

**Befund 6 — der age-Beleg fehlt und ist billig zu haben.** Debian trixie hat
`age` 1.2.1. Ohne ihn ist das age-Format ein Rundlauf.

---

## Was keiner dieser Belege abdeckt

Alles hier ist **am PC** gelaufen. Kein Beleg auf diesem Blatt hat ein Geraet
gesehen. Das Projekt steht auf Stufe 2 von 4 (geschrieben / uebersetzt / im
Abbild / am Geraet gelaufen), und die Belege aendern daran nichts — sie sagen,
dass die **Rechnungen** stimmen, nicht dass die Hardware sie ausfuehrt.
Besonders: beim PKCS#10 unterschreibt openssl an der Stelle des SE051C2, und
beim Dateiwerkzeug antwortet eine Attrappe an seiner Stelle, deren fuenf
offene Annahmen in `docs/dateiwerkzeug.md` stehen.

---

## Was wo gemessen wurde

| Beleg | Befehl | Wo |
|---|---|---|
| ML-KEM-768 | `python tools/mkkat_pqc.py <acvp> neu.h` und Vergleich; `make` | Windows / WSL |
| PKCS#10 | `make openssl`; dann `openssl req -in <datei> -verify` von Hand | WSL, openssl 3.5.7 |
| CXF | `go test ./tests/`; Beispiel aus der Nachtragsliste byteweise verglichen | Windows, Go |
| Geldboerse | `make` in `apps/firmware/test/wallet`; Werte gegen bip-0032, slip-0010, eip-155 | WSL / Windows |
| OpenPGP | `sq packet dump`, `sq decrypt --session-key` | WSL, sq 1.3.1 |
| Dongle | `tests/run.py`, `tests/vertrag.py`, `tests/dotnet-test.py`, `python/test_guard.py`, `php/test_guard.php` (mit sodium), `make test` in `c/` | Windows / WSL |
