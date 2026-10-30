# Der verschluesselte Datentraeger

Entwurf des Verfahrens fuer einen Datentraeger von 8 bis 16 GB am Stick -
als microSD im Schacht oder als geloeteter Baustein: verschluesselt,
gesiegelt, und in drei Betriebsarten auch weitergebbar.

**Stand: Papier. Es gibt keinen Baustein, keine Platine und keine Zeile
Code dazu.** Alle Zahlen sind gerechnet oder aus Herstellerunterlagen
abgeschrieben, keine ist an Hardware gemessen. Was gemessen ist, steht in
der Tabelle „Was geprueft ist" am Ende - und das ist wenig.

Welcher Rechenkern genommen wird, klaert ein anderer Strang. Der Entwurf
ist so gebaut, dass diese Entscheidung ihn nicht umwirft; wo etwas daran
haengt, steht es ausdruecklich dabei.

## Die wichtigste Grenze, zuerst

**Die Karte braucht immer einen openSecureKey.** Wer sie in einen
gewoehnlichen Kartenleser steckt, kann nichts damit anfangen - auch nicht
in der Betriebsart „nur gesiegelt". Auf der Karte liegt unser eigenes
Format, kein FAT. Das ist eine Entscheidung und ein Produktmerkmal, aber
es ist kein universelles Austauschmedium, und niemand darf das glauben.

**Und die zweite, die in allen Betriebsarten gilt: eine Signatur beweist
Unverfaelschtheit, Urheberschaft nur bei vorher bekanntem oeffentlichem
Schluessel.** Dazu ein eigener Abschnitt weiter unten, weil es die
Aussage ist, auf die sich am leichtesten jemand falsch verlaesst.

Nicht zu verwechseln mit `docs/laufwerk.md`. Das ist ein kleines,
schreibgeschuetztes Laufwerk mit den Verwaltungsprogrammen, aus einem
festen Abbild. Hier geht es um einen veraenderlichen Datentraeger.

## Was von der offenen Hardware-Entscheidung abhaengt

| Teil des Entwurfs | haengt an der Hardware? |
| --- | --- |
| Drei Ebenen: Stempel je Block + Baum + Bindung | nein, gilt immer |
| Drei Betriebsarten und ihre Empfaengerzeilen | nein |
| Empfaengermodell nach `age`-Vorbild | nein |
| Signatur Ed25519, MAC HMAC-SHA256 | nein, der Chip bleibt der SE051C2 |
| Schluesselhierarchie, Freigabe je Sitzung | nein |
| Zwei-Phasen-Festschreibung, Festschreibe-Ausloeser | nein |
| FAT32 nach aussen, 4-KiB-Zuordnungseinheit | nein |
| Blockgroesse 4 KiB | kaum - passt zu Flash-Seite und SD-Sektor |
| **Wahl ChaCha20-Poly1305 statt AES-256** | **ja**, siehe unten |
| **Alle Zeitangaben** (Pruefdauer, Durchsatz) | **ja** |
| Groesse des Baums (8 oder 16 GB) | ja, beide gerechnet |
| X25519 und Ed25519 im SE051C2 | ja, zu bestaetigen |

## Die drei Betriebsarten

Waehlbar beim Beschreiben oder einmalig voreingestellt. Das Vorbild sind
die Eintragsmarken des Tresors (`shared/vault-format.md:229-234`:
„mirrored", „paired hosts only", schreibgeschuetzt) - eine Marke im
Datensatz, nicht ein zweites Format und keine zweite Oberflaeche. Hier
ist es eine Marke im Kopf des Datentraegers, und sie steht in der
signierten Wurzel, damit sie nicht nachtraeglich umgebogen werden kann.

| | Name | Wer kann lesen | Wer kann pruefen | Geheim? |
| --- | --- | --- | --- | --- |
| 1 | **nur dieser Stick** | nur der Stick, der beschrieben hat | derselbe | ja |
| 2 | **jeder unserer Sticks, mit Passwort** | jeder openSecureKey mit dem Kartenwort | jeder openSecureKey | ja |
| 3 | **jeder unserer Sticks, nur gesiegelt** | jeder openSecureKey | jeder openSecureKey | **nein** |

Alle drei brauchen einen openSecureKey. Alle drei haben Baum und
Signatur. Der Unterschied liegt **nur** darin, wie - und ob - der
Datenschluessel eingewickelt ist.

### Die Anzeige muss den Modus nennen, bevor geschrieben wird

Nicht danach, nicht in den Einstellungen allein. Wer glaubt,
verschluesselt zu haben, und hat nur gesiegelt, gibt Geheimnisse weg -
das ist der schlimmste Fehler, den dieses Geraet machen kann, und er
entsteht aus einer Verwechslung, nicht aus einem Angriff. Darum:

- Beim Freigeben zum Schreiben nennt die Anzeige den Modus im Klartext
  („**OFFEN - nur gesiegelt**" bei 3), und der Tastendruck bestaetigt
  genau diesen Text.
- Modus 3 bekommt eine eigene, deutlich andere Darstellung - nicht
  dieselbe Zeile mit einem anderen Wort darin.
- Der Modus einer eingesteckten Karte steht an, solange sie steckt.
- Ein Moduswechsel heisst: Karte neu beschreiben. Es gibt kein
  „nachtraeglich verschluesseln" - das waere ein halber Zustand, und
  halbe Zustaende sind in diesem Projekt verboten.

### Modus 3 ist nur sinnvoll, wenn er versiegelt bleibt

Ein USB-Massenspeicher bekommt Schreibbefehle ohne Freigabe (eigener
Abschnitt unten). Fuer Modus 3 ist das besonders heikel: der Stick wuerde
unterschreiben, was das Betriebssystem geschrieben hat. Eine Karte, die
„Software mit nachweisbarer Herkunft" tragen soll, darf das nicht.

**Darum ist Modus 3 ein Vorgang in zwei Schritten:** beschreiben, dann
auf Tastendruck **versiegeln**. Danach ist die Karte schreibgeschuetzt -
der Stick weist jeden Schreibbefehl ab, mit derselben Antwort, die
`docs/laufwerk.md` fuer das Programm-Laufwerk beschreibt (CSW `FAILED`,
`DATA PROTECT` / `WRITE PROTECTED`, plus Schreibschutzbit in
`MODE SENSE`). Entsiegeln ist ein eigener, ausdruecklicher Vorgang und
macht die alte Signatur ungueltig.

Das ist dieselbe Logik wie beim Programm-Laufwerk, nur mit einem
Schalter: dort ist der Schreibschutz fest, hier faellt er mit dem
Versiegeln.

## Das Empfaengermodell

Ein Datenschluessel verschluesselt den Inhalt. Er wird **mehrfach
eingewickelt**, einmal je Empfaenger. Genau das Modell von `age`.

```
K_daten    256 Bit, zufaellig, je Datentraeger

Empfaengerzeilen im Kopf (eine oder mehrere):
  geraet    <kennung>   Chipschluessel dieses Sticks         Modus 1,2
  wort      <salz> <n>  aus dem Kartenwort abgeleitet        Modus 2
  stick     <ephem>     X25519 an einen anderen Stick        Modus 2
  (keine)                                                    Modus 3
```

In Modus 3 gibt es keine Empfaengerzeile und keinen Datenschluessel: die
Bloecke liegen im Klartext. Baum und Signatur sind dieselben.

### Koennen wir `age` einfach uebernehmen?

**Das Empfaengermodell ja, das Dateiformat nein.** Und der Grund ist
hart, nicht Geschmack.

Was fuer `age` spricht und uebernommen wird:

- Die Empfaengerzeilen sind genau das gesuchte Mittel: ein Dateischluessel,
  mehrfach eingewickelt, je Empfaenger eine Zeile. Erweiterbar, ohne das
  Format zu aendern.
- `age` hat einen **Passwort-Empfaenger** (scrypt) und
  **oeffentliche-Schluessel-Empfaenger** (X25519), also beides, was
  Modus 2 braucht.
- Es hat seit 2025 **nachquantensichere Empfaenger** nativ:
  MLKEM768-X25519 nach X-Wing, Empfaenger beginnen mit `age1pq1`. Die
  C2SP-Fassung raet ausdruecklich davon ab, dieselbe Datei an einen
  solchen **und** an nicht quantensichere Empfaenger zu verschluesseln -
  eine Regel, die wir uebernehmen koennen, ohne sie zu erfinden.
- Es nutzt **ChaCha20-Poly1305** und HKDF-SHA-256 - genau das, was auf
  diesem Rechenkern beschleunigt ist (Befund unten). Das ist kein Zufall:
  `age` ist fuer kleine, einfache Umsetzungen gebaut.

Was dagegen spricht und das Dateiformat ausschliesst:

1. **Der `age`-Datenteil laesst sich nicht an der Stelle neu
   schreiben.** Er ist in Abschnitten von bis zu 64 KiB verschluesselt,
   und die Nonce jedes Abschnitts ergibt sich aus seiner **Position**
   (fortlaufender Zaehler). Einen Abschnitt an derselben Stelle neu zu
   verschluesseln heisst, dieselbe Nonce mit demselben Schluessel bei
   anderem Klartext zu benutzen - der eine Fehler, der Poly1305
   vollstaendig bricht. Ein Datentraeger wird aber genau so benutzt:
   derselbe Block, immer wieder.
2. **64 KiB sind als Blockgroesse zu grob.** Ein Rechner schreibt 512
   Byte. Bei 64-KiB-Abschnitten kostet das Lesen-Aendern-Schreiben von
   64 KiB, also 128-fache Schreibverstaerkung.
3. **`age` kennt keine Signaturen.** Bewusst nicht. Die Echtheit, die
   dieses Projekt will, ist in `age` gar nicht vorgesehen und muss
   daneben gebaut werden - und damit ist das Format ohnehin nicht mehr
   `age`.
4. Der Dateischluessel ist bei `age` 128 Bit. Wir wollen 256.

**Also:** der Kopf des Datentraegers uebernimmt Aufbau und Denkweise der
`age`-Empfaengerzeilen, einschliesslich der Ableitungen (HKDF-SHA-256,
Einwickeln mit ChaCha20-Poly1305) und der Regel zu den
nachquantensicheren Empfaengern. Der Datenteil ist eigen: 4-KiB-Bloecke,
Nonce aus Schreibzaehler und Blocknummer (unten), weil er an der Stelle
neu geschrieben werden muss.

**Das Ergebnis ist keine `age`-Datei und darf nicht so heissen.** Wer sie
mit `age` zu oeffnen versucht, scheitert, und das ist richtig so.

**Wo `age` dagegen wirklich passt: die Sicherungsdatei.** `osk_backup`
schreibt einmal und liest einmal, ohne Zufallszugriff - dort gibt es
keinen der vier Gruende. Das ist ein Vorschlag an einen anderen Strang,
nicht Teil dieses Entwurfs.

### Modus 2 neben Modus 1: die Kette ist so stark wie ihr schwaechstes Glied

Liegt eine Wort-Empfaengerzeile neben der Geraetezeile, ist der
Datentraeger **so sicher wie das Kartenwort**, nicht so sicher wie der
Chip. Ein Angreifer greift die schwaechste Zeile an. `age` zieht daraus
eine harte Folgerung und erlaubt den scrypt-Empfaenger nur als
**einzigen** Empfaenger einer Datei.

Wir uebernehmen die Folgerung abgeschwaecht, weil Modus 2 beides braucht
(der Absender will seine Karte selbst auch noch lesen):

- Die Anzeige nennt beim Beschreiben in Modus 2 die Zahl der Empfaenger.
- Ein **vom Nutzer erdachtes** Passwort ist nur mit ausdruecklicher
  Warnung zulaessig (naechster Abschnitt).
- Ein nachquantensicherer Empfaenger und ein gewoehnlicher X25519-
  Empfaenger gehen nicht zusammen - `age`-Regel, uebernommen.

## Das Passwort - und warum wir das Problem umgehen statt loesen

### Befund: Argon2id auf dem Stick ist zu schwach

`shared/vault-format.md` und `osk_backup.h:50-55`:

| | Speicher | Durchgaenge |
| --- | ---: | ---: |
| auf dem Geraet | 64 KiB | 64 |
| am Rechner | 64 MiB | 3 |
| Obergrenze Geraet | 128 KiB | - |

Der nRF52840 hat 256 KiB RAM; mehr geht nicht. 64 KiB sind gegen
Durchprobieren mit Grafikkarten oder Spezialbausteinen **kein**
Hindernis - die Speicherhaerte, die Argon2id ausmacht, ist bei 64 KiB
praktisch weg. Ein Passwort-Empfaenger, der mit diesen Werten abgeleitet
wird, ist so stark wie das Passwort selbst, nicht staerker.

Rechnet dagegen der Rechner mit 64 MiB und 3 Durchgaengen, ist die
Ableitung ordentlich - aber dann liegen Passwort und abgeleiteter
Einwickelschluessel im Arbeitsspeicher des Rechners, also genau dort, wo
dieses Geraet sonst nichts liegen laesst.

### Entscheidung: nicht stretchen, sondern Entropie erzeugen

Beide Wege haben einen Preis, und beide sind vermeidbar. **Der
Regelfall in Modus 2 ist kein erdachtes Passwort, sondern ein vom Stick
erzeugtes Kartenwort:** zehn Woerter aus der Liste von 256, die das
Projekt schon hat (`shared/vault-format.md:454`, `osk_wordlist.c`).

- 256 Woerter sind 8 Bit je Wort, zehn Woerter sind **80 Bit**.
- Bei 80 Bit echter Entropie ist es **gleichgueltig**, wie langsam die
  Ableitung ist. Durchprobieren ist aussichtslos, auch gegen die
  schwache Geraeteableitung.
- Damit darf der **Stick** ableiten - mit 64 KiB und 64 Durchgaengen -
  und es muss **nichts** im Arbeitsspeicher eines Rechners liegen.
  Weder Wort noch Schluessel.
- Das Kartenwort wird dem Empfaenger auf einem anderen Weg mitgeteilt
  (gesprochen, auf Papier). Es steht nirgends auf der Karte.

Der Preis: zehn Woerter muss man weitergeben und der Empfaenger
eintippen. Dafuer gibt es Taster und Anzeige, und es ist dieselbe
Bedienung, die das Projekt fuer die Sicherungsdatei schon vorsieht.

**Das erdachte Passwort bleibt moeglich, aber als zweite Wahl** - fuer
den Fall, dass sich jemand etwas merken will. Dann gilt:

- Der **Rechner** leitet ab, mit den hohen Werten, weil die
  Geraeteableitung dafuer zu schwach ist.
- Die Anzeige sagt beim Beschreiben, dass Passwort und Schluessel dabei
  im Arbeitsspeicher des Rechners liegen und dass der Schutz nur so gut
  ist wie das Passwort.
- Der abgeleitete Schluessel wickelt nur `K_daten` ein und wird danach
  im Rechner ueberschrieben; er geht nie wieder zum Rechner zurueck, und
  Klartextdaten sieht der Rechner ohnehin nur ueber das Laufwerk.

**Was ausdruecklich nicht funktioniert und darum nicht vorgeschlagen
wird:** ein Geheimnis im Chip („Pfeffer") in die Ableitung
hineinzurechnen. Das wuerde Durchprobieren ohne Stick unmoeglich machen -
aber dann koennte nur *dieser* Stick lesen, und genau das soll Modus 2
nicht. Pfeffer und Weitergabe schliessen sich aus.

**Was auch nicht hilft, aber oft vorgeschlagen wird:** die schwache
Geraeteableitung hundertfach hintereinander zu rechnen. Das macht sie
hundertmal langsamer, aber nicht speicherhart - gegen
Spezialbausteine gewinnt man dadurch nichts. Wer nur 64 KiB hat, hat
Zeitharte, keine Speicherharte. Fuer 80 Bit Entropie braucht man
keines von beidem; fuer ein erdachtes Passwort reicht keines von beidem.

## Echtheit ist nicht Urheberschaft

**Das ist die wichtigste Grenze dieses Entwurfs.**

Eine Signatur ueber den Wurzelhash beweist: **die Daten sind unveraendert
seit dem Signieren**, und sie wurden mit einem bestimmten privaten
Schluessel signiert.

Sie beweist **nicht**, von wem sie kommen. Dass eine Karte von Thomas
kommt, folgt aus der Signatur nur, wenn der Empfaenger **vorher** dessen
oeffentlichen Schluessel kennt - aus einem Treffen, einer Visitenkarte,
einer Veroeffentlichung, einem anderen Kanal.

Was ein Geraetezertifikat leistet und was nicht:

| Behauptung | durch die Signatur gedeckt? |
| --- | --- |
| „Diese Daten sind unveraendert." | **ja** |
| „Sie wurden von einem openSecureKey signiert." | ja, mit Geraetezertifikat |
| „Sie wurden von *diesem* Geraet signiert." | ja, Geraetekennung |
| „Dieses Geraet gehoert Thomas." | **nein** - das weiss nur, wer es weiss |
| „Die Daten stammen von Thomas." | **nein** |

Aus einem Geraetezertifikat folgt genau: „von irgendeinem
openSecureKey". Das ist weniger, als es klingt - es ist der Beweis, dass
die Karte in unserer Geraetefamilie entstanden ist, nicht, wer sie
beschrieben hat.

Darum, und das ist Teil des Entwurfs, nicht der Bedienung:

- Die Anzeige beim Einstecken nennt den **Absenderschluessel** als
  Fingerabdruck und sagt, ob er bekannt ist: „**unbekannter
  Absender**" oder der vom Nutzer vergebene Name.
- Ein Absenderschluessel wird **einmal** bestaetigt und dann im Tresor
  gemerkt, mit Namen. Danach erkennt der Stick ihn wieder. Das ist
  dieselbe Mechanik wie bei gepaarten Rechnern
  (`vault-format.md:234` „paired hosts only").
- Die Oberflaeche sagt nie „echt", wenn sie nur „unveraendert" meint.
  „**Unveraendert, Absender unbekannt**" ist die Wahrheit beim ersten
  Mal, und sie ist unbequem - deswegen muss sie dastehen.

## Die Verschluesselung

### Befund: der Beschleuniger kann kein AES-256

Das Datenblatt des nRF52840 (PS v1.1) listet fuer die CryptoCell CC310
AES in ECB, CBC, CMAC, CTR und CCM - mit **128 Bit**. AES-256 nennt es
nicht. ChaCha20/Poly1305 steht dort dagegen ausdruecklich mit 128 **und**
256 Bit.

Die Treiberseiten des nRF Connect SDK sind deutlicher. Woertlich:

> The Arm CryptoCell cc3xx driver is limited to AES key sizes of 128 bits
> on devices with Arm CryptoCell cc310.

und, fuer GCM eigens:

> The Arm CryptoCell cc3xx driver does not provide hardware support for
> GCM on devices with Arm CryptoCell cc310.

Dazu drei Befunde aus denselben Tabellen:

- **XTS steht in keiner der beiden Treibertabellen.** Nicht bei `cc3xx`,
  nicht bei `nrf_oberon`. Es gibt also gar keinen PSA-Treiber dafuer.
- ChaCha20 und ChaCha20-Poly1305 stehen bei `cc3xx` als unterstuetzt.
- SHA-256 ist beschleunigt (CC310: „SHA-1 und SHA-2 bis 256 Bit").

Ein Widerspruch gehoert dazu: die allgemeine CryptoCell-310-Seite im
Nordic-Infocenter nennt „Supported key size: 128 and 256 bits". Sie
beschreibt die Baureihe allgemein. Wo sich allgemeine Seite und
Treiberseite widersprechen, gilt die Treiberseite - sie beschreibt den
Code, der tatsaechlich laeuft. **Ungeprueft:** niemand hat das an einem
nRF52840 nachgemessen; es gibt kein Brett.

### Befund zum vorhandenen Tresor - meldepflichtig

Der Tresor verschluesselt mit AES-256-GCM:

- `apps/firmware/src/classes/store/osk_crypto.h:20-24` - `OSK_KEY_LEN 32`,
  `OSK_NONCE_LEN 12`, `OSK_TAG_LEN 16`
- `apps/firmware/src/classes/store/osk_crypto_psa.c:61-64` -
  `PSA_KEY_TYPE_AES`, `psa_set_key_bits(&attr, 256)`
- dieselbe Datei `:81`, `:85`, `:103` - `PSA_ALG_GCM`

Damit trifft er **beide** Grenzen von oben: 256 Bit und GCM.

In der Konfiguration steht nichts, was gegensteuert:

- `apps/firmware/prj.conf:32` `CONFIG_PSA_CRYPTO=y`
- `:37` `CONFIG_PSA_WANT_ALG_GCM=y`, `:53` `CONFIG_PSA_WANT_KEY_TYPE_AES=y`
- **Im ganzen Repo gibt es keine Zeile, die den Treiber waehlt**: kein
  `CONFIG_*CC3XX*`, kein `*OBERON*`, kein `CONFIG_MBEDTLS_*`, keinen
  Schalter fuer die Schluessellaenge. Die Wahl trifft die SDK-Vorgabe fuer
  das Brett.

**Befund:** Der Tresor rechnet seine Verschluesselung auf dem nRF52840
aller Wahrscheinlichkeit nach in Software, im Treiber `nrf_oberon` - nicht
im Beschleuniger. Abgeleitet aus Treibertabelle und Konfiguration, **am
Geraet ungeprueft**.

**Begruendung, warum das trotzdem kein Alarm ist:** der Tresor ist
28,2 MiB gross und wird in kleinen Haeppchen gelesen (ein 8-KiB-Platz je
Geheimnis). Dort kostet Software-Krypto Millisekunden, die niemand merkt.
Beim Datentraeger ist es anders - dort geht es um Megabyte am Stueck, und
genau darum ist die Frage hier entscheidend und dort nicht.

Zum Vergleich nicht vergessen: der nRF52840 **hat** eine CryptoCell CC310
(die `cc3xx`-Familie). Sie beschleunigt nur AES-256 und GCM nicht.

### Entscheidung: nicht AES-256-XTS, sondern ChaCha20-Poly1305 je Block

AES-256-XTS ist der Standard fuer Blockgeraete; BitLocker und LUKS nehmen
es. Trotzdem **nein**, aus vier Gruenden, und keiner davon ist Geschmack:

1. **Kein Treiber bietet XTS.** Es waere eine Eigenimplementierung ueber
   AES-ECB. Eigener Krypto-Code ist in diesem Projekt die letzte Wahl.
2. **Kein Beschleuniger.** AES-256 laeuft hier in Software (Befund oben).
   XTS braucht zwei AES-Durchgaenge je Block (Tweak und Daten), also etwa
   doppelte Kosten gegenueber CTR.
3. **XTS erkennt nichts.** Es ist reine Verschluesselung ohne
   Echtheitsstempel. Wer ein mit XTS verschluesseltes Blockgeraet
   veraendert, bekommt beim Lesen Mull - aber das Geraet merkt es nicht,
   es liefert den Mull aus. Unverfaelschtheit muesste dann vollstaendig
   aus dem Baum kommen, und der Baum muesste die Daten selbst hashen.
4. **ChaCha20-Poly1305 liefert den Stempel gratis mit.** Ein
   16-Byte-Stempel je Block, und der ist beim Entschluesseln ohnehin
   geprueft. Damit wird der Baum deutlich billiger (siehe „Was eine
   Leseoperation kostet").

Dazu kommt: ChaCha20-Poly1305 ist das **einzige** 256-Bit-Verfahren, das
auf diesem Rechenkern laut Treibertabelle vom Beschleuniger kommt - und
es ist dasselbe Verfahren, das `age` nutzt.

**Also:**

| | |
| --- | --- |
| Verfahren | ChaCha20-Poly1305 |
| Schluessel | 256 Bit |
| Blockgroesse | 4096 B Klartext |
| Stempel | 16 B je Block, **nicht** im Datenblock |
| Nonce | 12 B, nie wiederholt (unten) |
| zusaetzliche Daten (AAD) | Blocknummer, Datentraegerkennung, Modus, Formatfassung |

In Modus 3 entfaellt die Verschluesselung; der Stempel wird dort durch
den SHA-256 des Blocks ersetzt (siehe „Der Baum").

Die 4 KiB sind kein Zufall: es ist die Flash-Seitengroesse, die das
Projekt schon nutzt (`apps/firmware/prj.conf`,
`CONFIG_NORDIC_QSPI_NOR_FLASH_LAYOUT_PAGE_SIZE=4096`), ein Vielfaches des
512-B-Sektors, den ein USB-Massenspeicher dem Rechner zeigt, und genau
die Zuordnungseinheit, die wir dem Rechner als FAT32 anbieten (unten).
Damit faellt Lesen-Aendern-Schreiben fuer Nutzdaten weitgehend weg.

### Die Nonce - der eine Fehler, der alles bricht

Eine Nonce zweimal mit demselben Schluessel bei verschiedenem Klartext
bricht Poly1305 vollstaendig: es gibt nicht eine Nachricht her, sondern
den Stempelschluessel. Genau dieselbe Falle, die `shared/vault-format.md`
fuer AES-GCM schon beschreibt (Zeile 126-127), und genau der Grund, warum
der `age`-Datenteil hier nicht geht.

```
Nonce (12 B) = Schreibzaehler des Blocks (4 B)
             + Blocknummer              (4 B)
             + Zweck                    (4 B)
```

- Der **Schreibzaehler je Block** waechst bei jedem Schreiben dieses
  Blocks um eins. Zwei Schreibvorgaenge auf denselben Block bekommen also
  verschiedene Nonces. Das ist der Unterschied zu `age`, wo die Nonce aus
  der Position allein folgt.
- Die **Blocknummer** trennt die Bloecke voneinander.
- Der **Zweck** trennt Daten von Baumseiten und Wurzelsatz, wie beim
  Tresor (`OSK_NONCE_BLOB/INDEX/LOG/...`).

Der Schreibzaehler muss beim Lesen wieder zur Hand sein. Er steht darum
neben dem Stempel im Stempelfeld - das ist der Grund, warum ein Eintrag
dort mehr als 16 Byte braucht.

4 Byte reichen fuer 4 294 967 295 Schreibvorgaenge **je Block**. Laeuft
einer ueber, muss der Datentraeger neu verschluesselt werden. Offener
Punkt, nicht geloest - siehe unten.

## Durchsatz: die Verschluesselung ist nicht der Engpass

Nordics Vergleichswerte fuer SHA-256, 4123 Byte:

| | Zeit | Durchsatz |
| --- | ---: | ---: |
| Beschleuniger | 0,1596 ms | **25,8 MB/s** |
| Software | 4,7413 ms | **870 kB/s** |

Gerechnet: 4123 B / 0,0001596 s = 25 833 333 B/s. Und
4123 B / 0,0047413 s = 869 804 B/s.

Dagegen der USB-Anschluss des Sticks: USB Full-Speed, in der Praxis
**400 bis 500 kB/s** fuer Massenspeicher.

**Aussage: die Verschluesselung ist kein Engpass.** Schon die
**Software**-Variante von SHA-256 liegt beim etwa Doppelten des
USB-Anschlusses, die beschleunigte beim Fuenfzigfachen.

Mit einer Einschraenkung, und die ist der Grund, warum der Baum weiter
unten so sparsam gebaut ist: der Spielraum ist **Faktor 2**, nicht
Faktor 20. Faellt je ausgeliefertem Nutzdatenblock mehr als etwa das
Doppelte an Krypto-Arbeit an, wird die Krypta doch zum Engpass - wenn sie
in Software laeuft.

**Ungeprueft, und das ist wichtig:** die beiden Zeiten stammen aus
Nordics Veroeffentlichung, die Quelle wurde beim Schreiben dieses Papiers
**nicht gefunden** - sie kommt aus dem Auftrag. Eine unabhaengige
Messreihe am nRF52840-CryptoCell gibt es in `eprint.iacr.org/2021/058`;
sie misst mit anderer Eingabelaenge (64 B) und kommt zu dem Schluss, dass
gerade beim **Hashen** Hardware und Software naeher beieinander liegen als
bei anderen Operationen. Der Faktor 29,7 aus den obigen Zahlen ist also
mit Vorsicht zu nehmen. Die Aussage „USB ist der Engpass" haengt davon
nicht ab: sie gilt schon mit der Software-Zahl.

Auch die 400-500 kB/s sind nicht an diesem Geraet gemessen.
**Ungeprueft.** Welche Stelle der Kette - USB, Kartenschnittstelle,
Verschluesselung, Hashen - zuerst klemmt, rechnet ein anderer Strang
(`docs/rechenkern-durchsatz.md`). Dieser Entwurf haengt nicht daran:
er wird nur schneller oder langsamer, nicht anders.

## Die Unverfaelschtheit - der Kern

### Drei Ebenen, drei verschiedene Angriffe

| Ebene | Was sie erkennt | Womit |
| --- | --- | --- |
| 1. Stempel je Block | ein veraenderter Block | Poly1305-Stempel (Modus 1,2) oder SHA-256 (Modus 3) |
| 2. Baum ueber die Stempel | ein **vertauschter** oder **einzeln zurueckgedrehter** Block | SHA-256 |
| 3. Bindung der Wurzel | ein **vollstaendig zurueckgespielter** aelterer Stand | Zaehler im Chip (im Geraet) bzw. Fassung und Zeit (weitergegeben) |

Warum alle drei gebraucht werden:

- Der Stempel allein genuegt nicht. Er bindet den Block an Schluessel,
  Blocknummer und Schreibzaehler - aber wer eine **alte, echte** Fassung
  von Block 4711 samt ihrem alten Stempel und Schreibzaehler
  zurueckschreibt, bekommt einen Block, der sich sauber entschluesselt.
- Der Baum allein genuegt auch nicht. Er bindet alle Bloecke zu einem
  Gesamtstand - aber wer den **ganzen** Datentraeger auf einen aelteren,
  echten Stand zurueckspielt, Baum und Wurzel eingeschlossen, hat einen in
  sich stimmigen Stand. Genau das ist der Rueckdreh.
- Die Bindung allein genuegt nicht, weil sie nur eine Zahl ist. Sie muss
  an einen Stand gebunden werden, und das tut der Wurzelhash.

### Der Baum - Aufbau und Groesse

Das Besondere: die **Blaetter sind nicht die Daten, sondern die Stempel**.
Das ist der Punkt, an dem ChaCha20-Poly1305 sich auszahlt.

```
Stempelfeld   je Block ein Eintrag von 32 B:
                16 B Poly1305-Stempel (Modus 1,2)
                     oder die ersten 16 B des SHA-256 (Modus 3)
                 4 B Schreibzaehler des Blocks
                12 B Reserve (Formatfassung, Zustandsbits)
              -> 128 Eintraege je 4-KiB-Seite

Ebene 1..n    je Seite der Ebene darunter ein SHA-256 (32 B)
              -> 128 Hashes je 4-KiB-Seite

Wurzelhash    SHA-256 ueber die oberste Seite, 32 B
```

In Modus 3 gibt es keinen AEAD-Stempel, also tritt ein Hash an seine
Stelle. 16 Byte davon genuegen: der Baum darueber bindet sie, ein
Angreifer muss also eine Kollision **gegen einen festen Zielwert**
finden, und 128 Bit sind dafuer ausreichend. **Begruendung, nicht
Messung** - wer das anders sieht, nimmt 32 Byte und verdoppelt das
Stempelfeld auf 1,6 %.

Verzweigung 128. Gerechnet (beide Groessen in Zweierpotenzen; die
Aufschrift „8 GB" auf einer Karte meint dezimal etwa 7 % weniger, die
Zahlen hier sind damit eine obere Schranke):

| | 8 GiB | 16 GiB |
| --- | ---: | ---: |
| Datenbloecke zu 4 KiB | 2 097 152 | 4 194 304 |
| Stempelfeld (32 B je Block) | 64 MiB (16 384 Seiten) | 128 MiB (32 768 Seiten) |
| Ebene 1 | 512 KiB (128 Seiten) | 1 MiB (256 Seiten) |
| Ebene 2 | 4 KiB (1 Seite) | 8 KiB (2 Seiten) |
| Ebene 3 | - | 4 KiB (1 Seite) |
| **Baum gesamt** | **64,50 MiB** | **129,01 MiB** |
| **Anteil am Datentraeger** | **0,79 %** | **0,79 %** |
| Ebenen ueber dem Stempelfeld | 2 | 3 |

Zur Gegenprobe, wenn man die Abkuerzung **nicht** nimmt und die Blaetter
als eigene SHA-256 ueber die Daten bildet: das Stempelfeld waere genauso
gross, aber **jede Leseoperation muesste den 4-KiB-Datenblock zusaetzlich
hashen**, obwohl das Entschluesseln ihn gerade schon geprueft hat. Die
Ersparnis liegt also nicht im Platz, sondern in der Rechenzeit.

**Wo der Baum liegt: auf dem Datentraeger selbst.** Nicht im 64-MB-Flash
des Sticks. Dort ist kein Platz - `docs/laufwerk.md` rechnet 6,82 MiB frei
vor, und schon der kleine Baum braucht 64,50 MiB.

### Der eigentliche Grund fuer den Baum: nicht pruefen muessen

**Eine fremde Karte vollstaendig zu pruefen, dauert Minuten. Das ist der
Normalfall in Modus 2 und 3, und es ist unzumutbar.** Der Baum ist genau
das Mittel, es nicht tun zu muessen.

Gerechnet mit dem beschleunigten SHA-256 (25,8 MB/s), **nur das Hashen**;
Lesen von der Karte und Entschluesseln kommen hinzu und haengen an der
Hardware:

| | 8 GiB | 16 GiB |
| --- | ---: | ---: |
| **alles hashen** | 332 s = **5,5 min** | 665 s = **11,1 min** |
| Baum dazu (0,79 %) | +2,6 s | +5,2 s |
| **beim Einstecken: nur die Wurzel** | ~0,2 ms | ~0,2 ms |
| **je gelesener Block: der Pfad** | 0,32 ms | 0,32 ms |

Der Pfad sind zwei Seitenhashes zu 4 KiB (Stempelseite und
Ebene-1-Seite; die Spitze liegt im RAM): 8192 B / 25,8 MB/s = 0,32 ms.
**Das ist zwei Millionen Mal weniger Arbeit als die vollstaendige
Pruefung** (665 s / 0,317 ms = 2 097 152 - die Verzweigung zahlt sich
genau so aus, wie sie soll).

In Software statt im Beschleuniger werden daraus 11,5 ms je Pfad und
5,5 Stunden fuer alles. Das ist der zweite Grund, warum der Befund zum
Beschleuniger wichtig ist.

### Was das heisst - und was die Anzeige sagen darf

**Beim Einstecken wird nicht die Karte geprueft, sondern die Wurzel.**
Signatur des Wurzelsatzes, Fassung, Absenderschluessel, und dass die
oberste Baumseite zum Wurzelhash passt. Das kostet Millisekunden. Was
dabei **nicht** geprueft wird: dass die 4 Millionen Bloecke zu dieser
Wurzel passen.

Ein veraenderter Block in einer Datei, die der Empfaenger nie oeffnet,
bleibt unentdeckt. Das ist vertretbar - er bekommt ihn ja auch nie zu
sehen, und sobald er ihn liest, faellt es auf, **bevor** ein Byte
hinausgeht. Aber es muss ehrlich dastehen:

| Was die Anzeige sagen darf | Was sie nicht sagen darf |
| --- | --- |
| „Siegel gueltig, Absender *Name*" | ~~„Karte geprueft"~~ |
| „Fassung 7, 14.03.2027" | ~~„Daten echt"~~ |
| „Daten werden beim Lesen geprueft" | ~~„unverfaelscht"~~ |
| „Block 4711 **nicht** gueltig - Lesen abgebrochen" | |

**Die Pruefung ist nicht abwaehlbar.** Weil immer ein openSecureKey
dazwischensteht, kann der Stick sie erzwingen: ein Block, dessen Pfad
nicht stimmt, wird nicht ausgeliefert - der Lesebefehl scheitert mit
`MEDIUM ERROR`, und die Anzeige nennt den Block. Das ist staerker als ein
Pruefprogramm, das man weglassen kann.

**Daneben eine ausdrueckliche vollstaendige Pruefung**, die der Nutzer
anstoesst - im Verwaltungsprogramm oder am Geraet. Sie **nennt ihre Dauer
vorher** („etwa 11 Minuten"), laeuft mit Fortschrittsanzeige, ist
abbrechbar, und ihr Ergebnis wird mit Datum und Fassungsnummer im Tresor
gemerkt, damit man es nicht zweimal machen muss. Sinnvoll vor dem
Weitergeben und bei Verdacht; im Alltag unnoetig.

### Die Wurzel binden - zwei Faelle, zwei Garantien

**Hier trennen sich die Betriebsarten, und sie duerfen nicht vermischt
werden.** Wer sie vermischt, glaubt eine Garantie zu haben, die er nicht
hat.

```
Wurzelsatz (auf dem Datentraeger, zweimal: Satz A und Satz B)
  magic, Formatfassung
  Betriebsart (1, 2 oder 3)
  Fassungsnummer     F, nur aufwaerts
  Zeitstempel        wann signiert
  Datentraegerkennung, Blockzahl
  Wurzelhash         32 B
  Absenderkennung    oeffentlicher Schluessel oder sein Fingerabdruck
  --- und dann, je Fall ---
  Zielzaehlerstand   N      nur Fall A
  MAC                32 B   nur Fall A: HMAC-SHA256(K_wurzel, alles davor)
  Signatur           64 B   Fall B: Ed25519 ueber alles davor
```

#### Fall A - der Datentraeger bleibt im Geraet (Modus 1)

Der Sicherheitschip hat monotone Zaehler. NXP AN12413 (SE05x APDU) und
das SE050-Datenblatt: „can only be incremented and not be decremented",
Laenge 1 bis 8 Byte, fluechtig oder dauerhaft; Operationen Set, Get,
Increment; `WriteCounter` ohne Wert erhoeht um eins.

Ein dauerhafter Zaehler von 4 Byte, und der Wurzelsatz traegt den
Zielstand. Pruefung beim Oeffnen: Zaehlerstand `C` lesen, gueltig ist der
Satz mit `N == C`. Ein Rueckdreh schreibt einen alten Satz zurueck,
dessen `N` ist kleiner, und `C` kann nicht fallen. **Der Rueckdreh faellt
auf, vollstaendig und ohne Zutun.**

**Befund, der den Entwurf vereinfacht:** der Chip-Treiber kann heute
**keine Binaerobjekte** (`se050_cmd.h:125-128`: `TYPE_EC_KEY_PAIR`,
`TYPE_AES_KEY`, `TYPE_USERID`, `TYPE_HMAC_KEY` - **kein** `TYPE_BINARY`,
**kein** `TYPE_COUNTER`). Der Wurzelhash muss aber gar nicht in den Chip:
er muss nur unveraenderbar an einen Zaehlerstand gebunden sein, und das
leistet ein MAC mit einem Schluessel, der den Chip nie verlaesst.

#### Fall B - die Karte wird weitergegeben (Modus 2 und 3)

**Der Zaehler traegt hier nicht.** Eine Karte, die weitergegeben wird,
verlaesst den Zaehler; ein Empfaenger hat den Chip des Absenders nicht.
An seine Stelle treten **Fassungsnummer und Zeitstempel in der
signierten Wurzel**, mit einer Ed25519-Signatur des Absenders.

Was das leistet:

- Wer eine Karte **schon einmal gesehen hat**, erkennt einen Rueckdreh.
  Dafuer merkt der empfangende Stick in seinem Tresor eine kleine Zeile
  je Karte: Kartenkennung, hoechste gesehene Fassung, Zeitstempel,
  Absender. Diese Tabelle liegt im Tresor und ist durch dessen eigene
  Bindung geschuetzt - **der Zaehler wirkt also mittelbar weiter**, nur
  nicht auf der Karte, sondern beim Empfaenger. Eine Karte, deren Fassung
  kleiner ist als die gemerkte, wird als **„aelterer Stand als zuletzt
  gesehen"** angezeigt und standardmaessig nicht geoeffnet.
- Wer eine Karte **das erste Mal** sieht, erkennt ihn **nicht**. Er sieht
  Fassung 3 und kann nicht wissen, ob es Fassung 7 gibt. Die Signatur
  ueber Fassung 3 ist echt; sie war nur einmal aktuell.

**Das ist die Grenze von Fall B, und sie ist nicht wegzuentwerfen.** Ein
weitergegebener Datentraeger kann aus sich selbst nicht beweisen, dass er
der neueste ist - er hat niemanden zu fragen. Was dagegen hilft, liegt
ausserhalb des Geraets:

- Der Absender nennt die Fassung auf dem Weg, auf dem er auch das
  Kartenwort nennt. „Fassung 7" auf Papier ist die ganze Loesung.
- Der Zeitstempel macht einen alten Stand sichtbar, wenn der Empfaenger
  weiss, wann er sie bekommen hat. Er beweist nichts - die Uhr des
  Absenders ist seine eigene - aber eine Karte von vor zwei Jahren faellt
  auf.
- Eine kurze Gueltigkeit („nicht nach diesem Datum oeffnen") ist moeglich
  und bei Dokumenten sinnvoll. Preis: der Stick braucht eine Uhr, der er
  traut, und die hat er nach dem Wegfall der Zelle nicht mehr durchgaengig.
  **Offener Punkt.**

| | Fall A (im Geraet) | Fall B (weitergegeben) |
| --- | --- | --- |
| veraenderter Block | erkannt | erkannt |
| vertauschte Bloecke | erkannt | erkannt |
| ganzer Stand zurueckgedreht | **erkannt** | erkannt **nur**, wenn schon gesehen |
| Karte das erste Mal | - | Rueckdreh **nicht** erkennbar |
| gebunden an | Zaehler im Chip | Fassung, Zeit, Signatur |

### Wer unterschreibt was: MAC im Geraet, Signatur zum Weitergeben

Das ist keine Geschmacksfrage, sondern folgt aus der Frage, wer prueft.

| | wer prueft | Mittel | warum |
| --- | --- | --- | --- |
| Fall A | derselbe Stick | **HMAC-SHA256**, Schluessel im Chip | Pruefer und Stempler sind dasselbe Geraet; eine Signatur leistet nur eines mehr - Pruefbarkeit durch Dritte - und das braucht hier niemand. Billiger, 32 B statt 64 B. |
| Fall B | ein **anderer** Stick | **Ed25519**, oeffentlicher Teil bekannt | Ein MAC braucht einen geteilten Schluessel. Den gibt es nicht und darf es nicht geben: er waere auf jedem Stick und damit kein Geheimnis. |

Ed25519 hat der SE051C2 (`se050_cmd.h:99-102`, `SIG_ED25519PURE`); der
Treiber kann signieren (`osk_se050.h:48` → `se050.c:471`). Fuer den
X25519-Empfaenger braucht es Curve25519-ECDH - der Treiber hat ECDH
(`se050.c:548`), die Kurve ist beim SE051C2 **zu bestaetigen**.

Beides ist eine Operation **je Festschreibung**, nicht je Block. Die
Kosten fallen also nicht ins Gewicht.

## Die Schluesselhierarchie

```
Grundgeheimnis im Chip                      verlaesst den Chip nie
   |
   | osk_se050_hkdf, im Chip gerechnet
   +-- K_geraet   "osk/volume/device"       wickelt K_daten ein (Modus 1,2)
   +-- K_wurzel   "osk/volume/root"         HMACKey im Chip (Fall A)
   +-- Ed25519-Schluesselpaar               signiert (Fall B), bleibt im Chip
   +-- X25519-Schluesselpaar                Empfaenger-Schluessel dieses Sticks

K_daten   256 Bit, zufaellig je Datentraeger, NICHT abgeleitet
   |
   +-- HKDF "osk/volume/data"  -> K_bloecke   Datenbloecke
   +-- HKDF "osk/volume/tree"  -> K_baum      Baumseiten (Modus 1,2)
```

`K_daten` ist **gewuerfelt, nicht abgeleitet** - genau deshalb kann er
mehrfach eingewickelt werden, und genau das ist der Unterschied zum
Tresor, wo alles aus dem Chip faellt.

- `K_bloecke` und `K_baum` liegen **waehrend einer Sitzung im RAM** des
  Rechenkerns. Sie muessen dort liegen: bei 400 kB/s und 4-KiB-Bloecken
  sind das etwa 100 Operationen je Sekunde, und jede ueber den I2C zum
  Chip zu schicken ist aussichtslos.
- Der Ed25519-Schluessel und `K_wurzel` bleiben im Chip.

**Beim Sperren** - PIN-Sperre, Zeitablauf, Abziehen - wird
festgeschrieben, dann werden `K_daten`, `K_bloecke` und `K_baum` im RAM
ueberschrieben (`osk_crypto_wipe`, wie im Tresor). Danach liefert der
Datentraeger auf jeden Lesebefehl `NOT READY`. Ohne PIN gibt es keinen
neuen Schluessel; der Fehlversuchszaehler im Chip ist die eigentliche
Schranke - dieselbe Begruendung, aus der die Geraete-PIN ohne langsames
Verfahren abgeleitet wird.

### Modus 1: die Karte ohne den Stick ist wertlos

Das folgt aus dem Aufbau, es ist nicht nur gewuenscht:

1. **Auf der Karte steht nur Chiffrat** - Bloecke, Baumseiten,
   Wurzelsatz und die eingewickelten Schluessel. Kein Schluessel im
   Klartext.
2. **`K_daten` ist nur mit `K_geraet` eingewickelt**, und `K_geraet` kommt
   aus dem Grundgeheimnis im Chip, das den Chip nicht verlaesst. Es gibt
   keinen Weg von der Karte zum Schluessel - auch nicht mit der PIN. Die
   PIN oeffnet den Chip; ohne Chip oeffnet sie nichts.
3. Eine Karte in einem fremden Stick ist Rauschen. Nicht, weil der
   fremde Stick sich weigert, sondern weil er nichts herleiten kann.

Und die Gegenrichtung: eine **fremde Karte in diesem** Stick muss
auffallen. Dafuer steht die Datentraegerkennung in den zusaetzlichen
Daten jedes Blocks und im Wurzelsatz, und der Wurzelsatz traegt den
Zaehlerstand aus diesem Chip.

Nicht geloest ohne Fall A: eine entnommene Karte laesst sich **kopieren**
und spaeter zuruecklegen. Das ist ein Rueckdreh - in Modus 1 faellt er am
Zaehler auf, in Modus 2 und 3 nur, wenn die Karte schon gesehen wurde.

**Haengt an der Bauform.** Bei einem geloeteten Baustein ist Modus 1 der
einzige sinnvolle, und die Verschluesselung schuetzt dort gegen Ausloeten
und Auslesen, nicht gegen den Verlust einer Karte.

## Was der Rechner sieht: FAT32 ueber unserem Format

Auf der Karte liegt unser Format. Was der Stick dem Rechner als Laufwerk
**zeigt**, muss ein gewoehnliches Dateisystem sein, sonst oeffnet Windows
es nicht. Der Stick uebersetzt.

**Entscheidung: FAT32 mit 4-KiB-Zuordnungseinheit.** Begruendung:

- FAT32 reicht bis 2 TiB, 16 GB sind kein Problem. exFAT waere moderner,
  ist aber deutlich mehr Code auf einer Flaeche, die jeder fremde Rechner
  anfasst - und das Projekt hat mit FAT16 im Programm-Laufwerk schon
  einen erprobten, kleinen Erzeuger (`tools/mkimage.py`).
  Preis: einzelne Dateien bis 4 GiB. Fuer 8-16 GB vertretbar.
- Die Zuordnungseinheit **ist** unser Block: 4 KiB. Damit faellt die
  Uebersetzung weg - der logische 512-B-Sektor `L` liegt in Block
  `L / 8`, Versatz `(L mod 8) * 512`. **Keine Zuordnungstabelle, kein
  Verschleissausgleich in der Firmware**, der Baustein macht das selbst.

| | 8 GiB | 16 GiB |
| --- | ---: | ---: |
| Zuordnungseinheiten | 2 097 152 | 4 194 304 |
| FAT, zwei Abschriften | 2 × 8 MiB | 2 × 16 MiB |
| RAM im Stick | ~16 KiB | ~16 KiB |

Der RAM-Bedarf ist der eigentlich gute Teil und aendert sich mit der
Groesse **nicht**: ein Datenblock (4 KiB) fuer Lesen-Aendern-Schreiben,
eine Stempelseite, eine Ebene-1-Seite, die Baumspitze. Zusammen rund
16 KiB von 256 KiB.

**Der Stick muss FAT nicht verstehen.** Er sieht Bloecke, nicht Dateien.
Damit ist auch die Frage „Signatur ueber Bloecke oder ueber Dateien"
entschieden, und zwar fuer alle drei Modi: **ueber Bloecke.** Begruendung:

- Eine Signatur ueber Bloecke erfasst **auch das Dateisystem selbst** -
  wer Dateien entfernt oder umbenennt, aendert FAT-Sektoren, und die
  liegen in Bloecken. Ueber Dateien erfasst sie das nicht.
- Eine Signatur ueber Dateien waere nur dann besser, wenn man eine
  einzelne Datei herauskopieren und weiter pruefen will. Dafuer braeuchte
  es einen fremden Pruefer - und den gibt es nicht, weil die Karte nur in
  unseren Sticks laeuft.
- Der Stick muesste FAT lesen und schreiben koennen, um Dateien zu
  erkennen. Das ist viel Code an einer Stelle, an der ein fremder Rechner
  die Eingaben bestimmt. Die Bloecke zu hashen braucht davon nichts.

Was dadurch wegfaellt: wer eine Datei von der Karte wegkopiert, hat
danach eine gewoehnliche Datei ohne Nachweis. **Der Nachweis klebt am
Datentraeger, nicht an der Datei.** Das gehoert gesagt, weil man es
anders erwarten koennte.

## Wann wird neu gerechnet und signiert?

Das Betriebssystem schreibt in Bloecken, wann es will, und sagt nicht,
wann es fertig ist. Nach jedem Block neu zu signieren ist zu teuer (eine
Chip-Operation und ein Zaehlerschritt je 4 KiB); erst beim Abziehen ist
zu spaet, weil Abziehen oft gar nicht angekuendigt wird.

**Fuenf Ausloeser, und der vierte und fuenfte greifen immer:**

| | Ausloeser | woher |
| --- | --- | --- |
| 1 | `SYNCHRONIZE CACHE (10/16)` | der eigentliche Zweck dieses Befehls |
| 2 | `START STOP UNIT` mit Auswurf, `PREVENT ALLOW MEDIUM REMOVAL` frei | der Rechner meldet das Auswerfen |
| 3 | Tastendruck „sichern" am Geraet | der Nutzer |
| 4 | **Ruhe**: kein SCSI-Befehl fuer eine Wartezeit | immer da |
| 5 | **Obergrenze**: nach K veraenderten Bloecken | immer da |

Zu 4 und 5: Die Ruhezeit (Vorschlag 750 ms, **nicht gemessen**) und die
Obergrenze (Vorschlag 1024 Bloecke = 4 MiB) sind die Zusicherung, dass
**nie** nichts passiert. Wie oft Windows, Linux und macOS den Befehl aus
Zeile 1 tatsaechlich schicken, ist unbekannt und laesst sich nur am
Rechner messen - und es gibt keinen. Darum darf der Entwurf sich nicht
darauf stuetzen.

Was die Obergrenze kostet, lohnt die Rechnung: 4 MiB sind bei 400 kB/s
etwa 10 Sekunden Schreiben. Das ist gleichzeitig die Obergenze fuer das,
was ein Stromausfall kostet - und bei 1000 Festschreibungen am Tag ueber
25 Jahre sind es 9,1 Mio Zaehlerschritte, noch unter der
Mindestschreibfestigkeit des Chips (unten).

**Und wenn er es nicht erkennt?** Dann greift die Ruhezeit, spaetestens
die Obergrenze. Der Preis ist Arbeit, die verloren geht - nie ein kaputter
Datentraeger (naechster Abschnitt). Die Anzeige unterscheidet darum
**„gesichert"** von **„ungesichert, noch N Bloecke offen"**, und sie sagt
beim Abziehen, ob es sicher ist.

Modus 3 hat das Problem nicht: dort wird einmal beschrieben und dann auf
Tastendruck versiegelt.

## Stromausfall mitten im Schreiben

Der Tresor ist dafuer gebaut: Kopfsatz als Festschreibepunkt, Daten
zuerst, Verweis zuletzt (`shared/vault-format.md:325-348`). Ein
Merkle-Baum mit gebundener Wurzel ist das **nicht von selbst** - er hat
zwei Dinge, die nicht in einem Zug geschrieben werden koennen: den
Wurzelsatz auf dem Datentraeger und den Zaehler im Chip.

### Die Reihenfolge

```
1. Datenbloecke schreiben        (verschluesselt, mit Stempel)
2. Stempelseiten schreiben
3. Baumebenen von unten nach oben schreiben
4. Wurzelsatz in den *inaktiven* Satz schreiben (Fassung F+1, Stand N+1)
5. Fall A: Zaehler im Chip auf N+1 erhoehen     <- Festschreibepunkt
   Fall B: der signierte Satz in 4 ist selbst der Festschreibepunkt
```

### Was ein Abbruch je Schritt bedeutet

| Abbruch | Was gilt | Was verloren ist |
| --- | --- | --- |
| in 1 bis 3 | der alte Satz | alles seit der letzten Festschreibung |
| in 4 (Satz halb) | der alte Satz, der halbe faellt am MAC bzw. an der Signatur | dasselbe |
| **zwischen 4 und 5** (nur Fall A) | Sonderfall, unten | nichts, wenn der Satz stimmt |
| in 5 | was der Chip sagt | nichts oder alles seit Schritt 1 |

**In Fall B gibt es das Fenster nicht**, weil es keinen zweiten Ort gibt:
der signierte Satz ist entweder ganz da und gueltig oder nicht. Gueltig
ist der mit der **hoeheren Fassungsnummer**, wenn seine Signatur stimmt.
Das ist der eine Punkt, an dem Fall B einfacher ist als Fall A.

**Das Fenster in Fall A ist unvermeidlich** und muss benannt werden:

> Ein Wurzelsatz mit Zielstand `C+1` bei Zaehlerstand `C` gilt als
> **vorbereitet**. Beim Oeffnen wird er angenommen - MAC geprueft,
> oberste Baumseite gegen den Wurzelhash gehalten - und die
> Zaehlererhoehung **nachgezogen**. Stimmt er nicht, bleibt der Satz mit
> `C` gueltig und der vorbereitete wird verworfen.

Das ist sicher, weil ein Angreifer einen Satz mit `C+1` nicht bauen kann:
dazu braeuchte er `K_wurzel` aus dem Chip. Und nicht missbrauchbar, weil
`C+2` nie angenommen wird - der Sprung ist auf genau eins begrenzt.

Was in Schritt 1 bis 3 verloren geht, ist **die Arbeit seit der letzten
Festschreibung, nicht der Datentraeger**. Der alte Stand ist vollstaendig
und stimmig; die halb geschriebenen Bloecke gehoeren zu keinem gueltigen
Baum und werden ueberschrieben. Dieselbe Zusage, die der Tresor macht: ein
Zustand oder der andere, nichts dazwischen.

Was das fuer den Nutzer heisst, und das ist unangenehm: ein Abziehen
mitten im Kopieren kann bis zu **4 MiB oder eine Ruhezeit** an Arbeit
kosten, nicht nur die letzte Datei. Darum die Obergrenze aus dem Abschnitt
davor, und darum muss die Anzeige sagen, wann es sicher ist abzuziehen.

### Der Zaehler als Verbrauchsgut

Jede Festschreibung in Fall A ist ein Schreibvorgang im Chip. Das
SE050-Datenblatt (Rev. 3.8) nennt **100 Mio** Schreibzyklen global mit
Verschleissausgleich und mindestens 20 Mio, bei 25 Jahren Datenhaltung;
NXP bestaetigt, dass die Angabe global und nicht zellenbezogen ist.

50 Festschreibungen am Tag ueber 25 Jahre sind 456 250 - weit unter der
Mindestangabe. 1000 am Tag waeren 9,1 Mio, noch darunter. Der Zaehler ist
also **kein** knappes Gut, solange nicht je Schreibbefehl festgeschrieben
wird. **Ungeprueft:** 4 Byte Breite reichen rechnerisch 4,29 Mrd Mal und
sind damit nicht die Schranke; die Schreibfestigkeit ist es.

In Fall B faellt das weg: dort wird nur signiert, nichts im Chip
geschrieben.

## Das Betriebssystem schreibt, wann es will

Der neue Zweck macht das wichtiger, nicht unwichtiger.

### Befund

Ein USB-Massenspeicher bekommt Schreibbefehle ohne jede Freigabe. Steckt
der Datentraeger offen, schreibt Windows Papierkorb-Ordner, Indexdateien
und Zeitstempel hinein, ohne dass der Nutzer etwas tut. Schadsoftware auf
demselben Rechner kann dasselbe. Es gibt im Protokoll keinen Weg, einen
einzelnen 512-Byte-Schreibbefehl dem Nutzer vorzulegen und auf einen
Tastendruck zu warten - der Rechner wuerde in einen Zeitablauf laufen.

Jede andere Funktion dieses Geraets ist anders gebaut: PIN und Tastendruck
sind Pflicht, und `docs/laufwerk.md` weist beim Programm-Laufwerk **jeden**
Schreibbefehl ab.

### Was das bedeutet

Die Zusage, die dieser Entwurf geben kann:

> **Was auf dem Datentraeger steht, hat ein openSecureKey mit diesem
> Absenderschluessel geschrieben, und es ist die hoechste Fassung, die
> dieser Stick davon gesehen hat.**

Die Zusagen, die er **nicht** geben kann:

> ~~Was auf dem Datentraeger steht, hat der Nutzer so gewollt.~~
> ~~Es ist die neueste Fassung, die es gibt.~~ (nur Fall B)
> ~~Es kommt von Thomas.~~ (nur mit vorher bekanntem Schluessel)

Baum und Bindung schuetzen gegen einen Angreifer, der **am
Datentraeger** arbeitet: ihn ausliest, veraendert, kopiert,
zurueckspielt - auch mit dem Baustein in der Hand. Sie schuetzen
**nicht** gegen einen Angreifer, der **am freigegebenen Stick** arbeitet,
denn dessen Schreibbefehle sind echt und werden korrekt gestempelt und
signiert. Das muss im Bedrohungsmodell so stehen.

Fuer Modus 3 ist das der Grund fuer das Versiegeln: eine Karte, die
„Software mit nachweisbarer Herkunft" tragen soll, darf nicht offen
stehen, waehrend ein Rechner hineinschreibt.

### Ist Freigabe je Sitzung der richtige Weg? Ja

- **Je Vorgang ist nicht moeglich.** Das Protokoll gibt es nicht her.
- **Gar keine Freigabe waere schlechter.** Dann steht der Datentraeger
  offen, sobald der Stick steckt.
- **Je Sitzung ist das, was der Nutzer ueberhaupt entscheiden kann.** Er
  weiss, ob er diesem Rechner jetzt Daten geben will. Welchen der
  4 Millionen Bloecke das Betriebssystem dafuer anfasst, weiss er nicht.

Dazu gehoeren fuenf Dinge, sonst ist die Freigabe wertlos:

1. **Standardmaessig gar nicht da.** Beim Einstecken existiert der
   Datentraeger fuer den Rechner nicht. Erst PIN und Tastendruck lassen
   ihn erscheinen. (Modus 3 braucht zum Lesen keine PIN - die Daten sind
   nicht geheim - aber die Freigabe bleibt.)
2. **Zwei Stufen trennen.** Nur Lesen ist eine andere Freigabe als Lesen
   und Schreiben, mit eigenem Tastendruck, gefuehrt im Schreibschutzbit
   von `MODE SENSE`, damit das Betriebssystem es vorher weiss.
3. **Den Modus nennen, bevor geschrieben wird.** Siehe oben.
4. **Sichtbar, solange sie gilt**, und bei Schreibfreigabe deutlicher.
   Eine Freigabe, die man vergisst, ist keine.
5. **Von selbst zu Ende.** Nach einer Ruhezeit festschreiben und sperren.
   Das deckt „Stick steckt noch im fremden Rechner" ab - in der Praxis
   haeufiger als jeder Angreifer.

Dazu billig und wirksam: **mitschreiben, was geschrieben wurde.** Die Zahl
der geaenderten Bloecke je Sitzung kennt jede Festschreibung ohnehin. Dem
Nutzer „4,2 MB geschrieben" zu zeigen, ist kein Schutz, macht aber den
Unterschied sichtbar zwischen „ich habe eine Datei kopiert" und „der
Rechner hat hinter meinem Ruecken gearbeitet". Der Protokollring des
Tresors (`osk_vault_format.h:412`) ist das Vorbild.

## Vorschlaege an Dateien, die ich nicht anfasse

`hardware/`, `shared/vault-format.md` und `docs/rechenkern-durchsatz.md`
sind unberuehrt. Was dort geaendert werden sollte:

1. **`shared/vault-format.md`, „Known limits", Rueckdreh (Zeile 502-506).**
   Von „Open" auf „Entwurf liegt vor", mit Verweis hierher. Loesung und
   Code sind dieselben wie in Fall A: ein dauerhafter Zaehler im Chip,
   eigene Objektkennung, Vergleich beim Oeffnen, plus ein MAC ueber den
   Kopfsatz statt des `crc32` (`osk_vault_format.h:233`).
2. **`shared/vault-format.md`, dort wo AES-GCM steht.** Der Befund von
   oben gehoert dazu: auf dem nRF52840 kommt AES-256-GCM nicht vom
   Beschleuniger. Fuer den Tresor nicht schlimm, aber es steht nirgends,
   und wer die Stellen zu Laufzeit und Passkey-Anmeldung liest (Zeile 204
   und 215), koennte das Gegenteil annehmen.
3. **Die Sicherungsdatei koennte wirklich `age` werden.** `osk_backup`
   schreibt einmal und liest einmal; keiner der vier Gruende, die das
   `age`-Format fuer die Karte ausschliessen, trifft dort zu. Dafuer
   bekaeme sie die Empfaenger (Passwort, anderer Stick,
   nachquantensicher) und ein Format, das Fremdwerkzeuge lesen. Eigener
   Strang, nicht dieser.
4. **Nebenbefund:** in `apps/firmware/prj.conf:34-59` fehlt
   `CONFIG_PSA_WANT_ALG_ARGON2*`, obwohl `osk_crypto_argon2id`
   (`osk_crypto_psa.c:178-203`) darauf baut und sonst
   `OSK_ERR_UNSUPPORTED` liefert (`:210-222`). Trifft die
   Sicherungsdatei. Gemeldet, nicht geaendert.
5. **`apps/firmware/include/osk_se050.h`** braucht
   `osk_se050_counter_read` und `osk_se050_counter_raise` - heute als
   fehlend vermerkt in `boot/boot_se050.c:9-24`, Schnittstelle in
   `boot/osk_boot.h:78-83`, Kennung `OSK_BOOT_COUNTER_ID 0x4F534B01`
   (`:35`), gebaut nur mit `-DOSK_HAVE_SE050_COUNTER`. Datentraeger und
   Tresor brauchen dieselben zwei Funktionen - eine Baustelle, nicht drei.
   Nicht verwechseln mit dem SCP03-Befehlszaehler (`se050_scp03.h:61-63`)
   und dem PIN-Fehlversuchszaehler (`se050.c:860`).
6. **`docs/threat-model.md`** muss die drei Modi und ihre
   unterschiedlichen Garantien aufnehmen, besonders „Echtheit ist nicht
   Urheberschaft" und die Rueckdreh-Grenze in Fall B.

## Was geprueft ist, und wie

Nichts ist an Hardware gemessen. **Es gibt kein Brett, keine Karte und
keinen Code zu diesem Entwurf.**

| Angabe | Quelle | Stand |
| --- | --- | --- |
| cc3xx nur 128-Bit-AES auf cc310 | nRF Connect SDK, Treiberseite, woertlich zitiert | Herstellerdoku |
| kein Hardware-GCM auf cc310 | ebenda | Herstellerdoku |
| XTS in keiner Treibertabelle | ebenda, Tabellen Cipher und AEAD | Herstellerdoku |
| ChaCha20-Poly1305 bei cc3xx gelistet | ebenda | Herstellerdoku |
| „128 und 256 Bit" im Infocenter | Nordic Infocenter, CryptoCell-310 | Herstellerdoku, **widerspricht** der Treiberseite |
| SHA-256 beschleunigt | nRF52840 PS, CryptoCell | Herstellerdoku |
| Tresor nutzt AES-256-GCM | `osk_crypto.h:20`, `osk_crypto_psa.c:61-64,81` | im Code gelesen |
| kein Treiberschalter im Repo | Suche ueber alle `*.conf` | im Repo gemessen |
| **Tresor rechnet in Software** | Folgerung aus den drei Zeilen darueber | **ungeprueft**, abgeleitet |
| `age`: Empfaengerzeilen, X25519, scrypt | C2SP `age.md`, c2sp.org/age | fremde Spezifikation |
| `age`: Abschnitte bis 64 KiB, Nonce aus der Position | C2SP `age.md` und Vortragsunterlagen | fremde Spezifikation, Zweitquelle |
| `age`: MLKEM768-X25519, `age1pq1`, Mischverbot | C2SP `age.md`, age-Veroeffentlichungen | fremde Spezifikation |
| `age` kennt keine Signaturen | C2SP `age.md`, keine vorhanden | fremde Spezifikation |
| Argon2id 64 KiB/64 auf dem Geraet | `osk_backup.h:50-55`, `vault-format.md` | im Code gelesen |
| Wortliste 256 Woerter, 8 Bit je Wort | `vault-format.md:454`, `osk_wordlist.c` | im Code gelesen |
| 10 Woerter = 80 Bit | gerechnet | gerechnet |
| Zaehler nur aufwaerts, 1-8 B | NXP AN12413, SE050-Datenblatt | Herstellerdoku |
| 100 Mio / min. 20 Mio Schreibzyklen | SE050-Datenblatt Rev. 3.8 | Herstellerdoku |
| kein `TYPE_COUNTER`/`TYPE_BINARY` im Treiber | `se050_cmd.h:125-128` | im Code gelesen |
| HMAC, ECDSA, Ed25519, HKDF, ECDH vorhanden | `osk_se050.h:48,52,57,69`, `se050_cmd.h:99-102` | im Code gelesen |
| X25519 (Curve25519-ECDH) am SE051C2 | - | **zu bestaetigen** |
| `CONFIG_HMAC` (0x0010) am SE051C2 | - | **zu bestaetigen** |
| Zaehlerfunktionen fehlen | `boot/boot_se050.c:9-24`, `osk_boot.h:78-83` | im Code gelesen |
| Eintragsmarken als Vorbild | `vault-format.md:229-234` | im Code gelesen |
| Baumgroessen, Ebenen, Anteile | gerechnet, Zweierpotenzen | gerechnet |
| 5,5 min / 11,1 min vollstaendig | gerechnet aus 25,8 MB/s | gerechnet auf **ungeprueftem** Wert |
| 0,32 ms je Pfad, Verhaeltnis 2 097 152 | gerechnet | gerechnet auf **ungeprueftem** Wert |
| FAT32-Zahlen, 16 KiB RAM | gerechnet | gerechnet |
| 750 ms Ruhezeit, 1024 Bloecke Obergrenze | Vorschlag | **nicht gemessen**, frei gewaehlt |
| 400-500 kB/s am USB | Erfahrungswert Full-Speed | **ungeprueft** |
| 6,82 MiB frei im Stick-Flash | `docs/laufwerk.md` | dort gerechnet |

## Offene Punkte

1. **Fall B kann den Rueckdreh beim ersten Mal nicht erkennen.** Das ist
   keine Luecke im Entwurf, sondern eine Eigenschaft eines
   weitergegebenen Datentraegers: er hat niemanden zu fragen. Behoben wird
   es nur ausserhalb des Geraets (Fassungsnummer auf dem Weg mitteilen).
   **Muss in der Oberflaeche stehen, nicht nur hier.**
2. **Eine Uhr, der der Stick traut, gibt es nicht mehr.** Der Zeitstempel
   in der Wurzel ist die Uhr des Absenders. Eine Gueltigkeitsgrenze
   („nicht nach diesem Datum") braucht mehr, und nach dem Wegfall der
   Zelle ist offen, woher.
3. **Der Schreibzaehler je Block kann ueberlaufen.** 4 Byte reichen fuer
   4,29 Mrd Schreibvorgaenge auf **einen** Block. Ein Betriebssystem, das
   denselben FAT-Sektor immer wieder fortschreibt, kommt dem naeher als es
   klingt. Was dann passieren muss - neu verschluesseln - ist nicht
   entworfen.
4. **Wie oft `SYNCHRONIZE CACHE` wirklich kommt, ist unbekannt.** Es
   entscheidet, wie viel ein Abziehen kostet. Nur am Rechner messbar.
   Ruhezeit und Obergrenze sind frei gewaehlt und nicht begruendet, nur
   gerechnet.
5. **Der schlechteste Fall beim Lesen traegt nur mit Hardware-SHA-256.**
   In Software werden aus 0,32 ms je Pfad 11,5 ms, und die vollstaendige
   Pruefung dauert 5,5 Stunden statt 11 Minuten. Faellt der Befund
   „SHA-256 ist beschleunigt" bei der Messung anders aus, muss der Baum
   flacher oder die Blockgroesse groesser werden.
6. **`CONFIG_HMAC` und Curve25519 beim SE051C2 sind nicht bestaetigt.**
   Ohne `CONFIG_HMAC` gibt es `K_wurzel` nicht (Fall A braucht dann einen
   transienten abgeleiteten Schluessel); ohne Curve25519-ECDH gibt es den
   X25519-Empfaenger nicht, und Modus 2 bleibt auf das Kartenwort
   beschraenkt.
7. **Die 16 Byte je Blatt in Modus 3 sind begruendet, nicht geprueft.**
   Wer 128 Bit gegen einen festen Zielwert zu wenig findet, nimmt 32 Byte
   und zahlt 1,6 % statt 0,79 %.
8. **Nachquantensicher ist vorgesehen, aber nicht entworfen.** Die
   `age`-Empfaengerzeile MLKEM768-X25519 passt ins Modell; ob der SE051C2
   oder der Rechenkern ML-KEM-768 rechnen kann und was es an RAM kostet,
   ist hier nicht untersucht.
9. **Die zwei Ebenen kosten Code an der gefaehrlichsten Stelle.** FAT32
   nach aussen, eigenes Format innen - der Uebersetzer sieht Eingaben, die
   ein fremder Rechner bestimmt. Er ist einfach gehalten (kein
   FAT-Verstaendnis, feste Abbildung), aber er ist neu und ungeprueft.
10. **Nichts davon ist umgesetzt.** Es gibt keinen Code, und die beiden
    Chip-Funktionen, auf denen Fall A steht, fehlen bis heute.

## Was davon gebaut ist (Stand 30.10.2026)

Der Entwurf oben ist unveraendert; hier steht nur, was daraus Code geworden
ist und was dabei anders kam. **Nichts ist an Hardware gelaufen** - es gibt
weder Karte noch Chip noch Brett. Gelaufen ist es gegen Attrappen am PC.

| | Datei |
| --- | --- |
| Vertrag: drei Arten, Baum, Pfadkosten, Auflage an die Anzeige | `apps/firmware/src/classes/store/osk_volume.h` |
| Kartenschnittstelle | `apps/firmware/src/classes/store/osk_volume_card.h` |
| ChaCha20-Poly1305 und Ed25519-Pruefung, Vertrag | `apps/firmware/src/classes/store/osk_volume_crypto.h` |
| Oeffnen, Lesen, Schreiben, Festschreiben, Versiegeln | `volume_core.c` |
| Empfaengerzeilen (geraet, wort, stick) | `volume_recv.c` |
| Baumgroesse, Pfadkosten, Modustexte | `volume_tree.c` |
| Krypto auf dem Geraet (PSA) | angehaengt an `osk_crypto_psa.c` |
| Attrappen Karte und Krypto | `apps/firmware/test/store/fake_card.c`, `fake_vcrypto.c` |
| Pruefsaetze | `test_volume.c`, `test_volume_modes.c` |

**Wo der Code des Laufwerks liegt:** `apps/firmware/src/classes/transport/`
(`usb_msc.c`, `usb_msc.h`) - nicht unter `classes/msc/`. Das ist der
SCSI- und Bulk-Only-Weg des **schreibgeschuetzten Programm-Laufwerks**; er
weist jeden Schreibbefehl ab. Der Datentraeger braucht dort eine zweite,
schreibbare Einheit. Die Schnittstelle dazu ist `osk_volume.h`; gebaut ist
die Anbindung nicht.

### Gemessen statt geschaetzt

Alles aus den Pruefsaetzen, nicht aus dem Kopf:

| | Wert | woher |
| --- | --- | --- |
| Baum 8 GiB | 16 513 Seiten = 64,50 MiB = 0,79 % | `test_volume.c`, aus `osk_vol_layout` |
| Baum 16 GiB | 33 027 Seiten = 129,01 MiB = 0,79 % | ebenda |
| Ebenen 8 / 16 GiB | 2 / 3 | ebenda |
| **Pfadpruefung** | **3 Lesevorgaenge** von der Karte, **8 KiB gehasht** | gezaehlt an der Karten-Attrappe |
| davon Nutzdaten | 1 Block = 4 KiB | - |
| vollstaendige Pruefung 16 GiB | 671 s = 11,2 min bei 25,8 MB/s | gerechnet, Hashleistung **ungeprueft** |
| dieselbe in Software | 5,5 Stunden bei 870 kB/s | ebenda |

**Was das auf dem Durchsatz obendrauf legt, ehrlich gerechnet.** Je
ausgelieferten 4-KiB-Block liest der Stick 12 KiB von der Karte (Block,
Stempelseite, Ebene-1-Seite) und hasht 8 KiB:

- USB ist die Wand: 4 KiB bei 400 kB/s sind **10,2 ms**.
- Die 8 KiB Aufschlag von der Karte sind bei 3 MB/s **2,7 ms** - also
  **27 % des Zeitbudgets**, nicht Faktor drei. Es passt, aber nicht
  bequem.
- Das Hashen von 8 KiB kostet 0,32 ms im Beschleuniger (3 %) und 9,4 ms in
  Software (**92 % des Budgets**). Faellt der Befund zum Beschleuniger
  anders aus, ist der Baum und nicht USB der Engpass.
- Beim Lesen hintereinanderliegender Bloecke faellt der Aufschlag fast
  weg: 128 Bloecke teilen sich eine Stempelseite. **Diese Zwischenablage
  ist NICHT gebaut** - gemessen sind die 3 Zugriffe je Block ohne sie.

### Was anders kam als im Entwurf - meldepflichtig

1. **Stempelfeld und Baumseiten liegen im Klartext.** Der Entwurf gibt
   `K_baum` fuer die Baumseiten vor (Modus 1 und 2). Gebaut ist es nicht,
   und der Grund ist das Format selbst: ein AEAD-Stempel je Baumseite hat
   darin keinen Platz, und ohne Stempel ist die Verschluesselung einer
   Seite, die der Baum ohnehin bindet, nur Verdeckung. Was dadurch
   sichtbar bleibt: **wie oft welcher Block beschrieben wurde** - ein
   Nutzungsmuster, keine Nutzdaten. `K_baum` wird darum nicht abgeleitet.
   Wer das anders entscheidet, braucht ein eigenes Stempelfeld fuer
   Baumseiten.
2. **Festgeschrieben wird der GANZE Baum, nicht der Pfad.** Bei 16 GiB
   sind das 129 MiB Hashen je Festschreibung, rund 5 Sekunden im
   Beschleuniger - unvereinbar mit der Obergrenze von 1024 Bloecken. Das
   Nachziehen nur der beruehrten Seiten ist dieselbe Rechnung von unten
   nach oben und fehlt.
3. **Formatieren geht einmal ueber die ganze Karte.** Jeder Block braucht
   einen Stempel, sonst ist er unlesbar - und ein Betriebssystem liest
   Sektoren, in die es nie geschrieben hat. Bei 16 GiB sind das etwa
   1,5 Stunden. Die Anzeige muss diese Dauer vorher nennen, so wie die
   vollstaendige Pruefung ihre nennt. Im Entwurf steht das nicht.
4. **Der Empfaenger "anderer Stick" (X25519) ist nicht gebaut** und
   meldet `OSK_ERR_UNSUPPORTED`. Grund: offener Punkt 6 - Curve25519 am
   SE051C2 ist nicht bestaetigt. Eine Attrappe dafuer wuerde die eigene
   Seite des Vertrags pruefen und nicht die des Chips. Modus 2 laeuft
   darum heute auf Chipschluessel und Kartenwort.
5. **Die Signatur eines fremden Sticks wird auf dem Rechenkern geprueft,
   nicht im Chip.** `osk_se050.h` hat dafuer keine Funktion, und das ist
   richtig: eine Rechnung auf oeffentlichen Daten gehoert nicht ueber den
   I2C. Ob PSA auf dem nRF52840 `PSA_ALG_PURE_EDDSA` zum Pruefen anbietet,
   ist **zu bestaetigen**.
6. **Die Tabelle der bekannten Absender fehlt.** Sie liegt im Tresor und
   gehoert dem store-Strang. Solange sie fehlt, antwortet
   `osk_volume_sender()` fuer jeden fremden Stick **"unbekannt"** - das
   ist die Wahrheit und keine Luecke, aber es heisst, dass Modus 2 und 3
   heute nie "bekannter Absender" anzeigen koennen.

### Was die Anzeige leisten muss

Der Vertrag steht in `osk_volume.h` bei `osk_vol_ui_confirm_fn`. Ohne diese
Rueckmeldung gibt es keinen Datentraeger: `osk_volume_init()` weist eine
Einrichtung ohne Anzeige mit `OSK_ERR_ARG` ab, und ohne Zustimmung weist
jeder Schreibbefehl mit `OSK_ERR_DENIED` ab. Verlangt wird:

1. **Den Modus im Klartext nennen, bevor geschrieben wird.** Bei Modus 3
   faengt der Text mit **OFFEN** an - `osk_vol_mode_text()` liefert
   "OFFEN - nur gesiegelt, nicht geheim", und der Pruefsatz haelt fest,
   dass das erste Wort dort steht.
2. **Modus 3 bekommt eine deutlich andere Darstellung**, nicht dieselbe
   Zeile mit einem anderen Wort darin. Die Rueckmeldung bekommt dafuer
   `secret = false` ausdruecklich mitgegeben, damit die Anzeige nicht aus
   der Nummer schliessen muss.
3. **Der Tastendruck bestaetigt diesen Text**, nicht "weiter".
4. **In Modus 2 die Zahl der Empfaenger nennen** - sie steht im Aufruf.
5. **Ein Zeitablauf ist ein Fehler, keine Zustimmung** (`OSK_ERR_TIMEOUT`).
6. **Nur die erlaubten Aussagen verwenden.** `osk_vol_claim_text()` gibt
   sie vor. "dieser Block geprueft" gibt es, **"Karte geprueft" nicht** -
   und zwar nicht, weil es unhoeflich waere, sondern weil es falsch ist.
7. **Echtheit und Urheberschaft getrennt anzeigen.**
   `osk_volume_root_unchanged()` und `osk_volume_sender()` sind zwei
   Funktionen, und die Anzeige muss zwei Zeilen daraus machen:
   "unveraendert" und daneben "Absender unbekannt".

