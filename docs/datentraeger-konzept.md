# Der verschluesselte Datentraeger

Entwurf des Verfahrens fuer einen echten Datentraeger von 8 bis 16 GB am
Stick: verschluesselt, und die Daten ueber den Sicherheitschip auf
Unverfaelschtheit pruefbar.

**Stand: Papier. Es gibt keinen Baustein, keine Platine und keine Zeile
Code dazu.** Alle Zahlen sind gerechnet oder aus Herstellerunterlagen
abgeschrieben, keine ist an Hardware gemessen. Was gemessen ist, steht in
der Tabelle „Was geprueft ist" am Ende - und das ist wenig.

Dieses Papier beschreibt **nur das Verfahren**. Welcher Rechenkern und
welche Bauform (geloeteter Baustein oder Kartenschacht) genommen werden,
klaeren zwei andere Straenge. Der Entwurf ist so gebaut, dass er von
dieser Entscheidung so wenig wie moeglich abhaengt.

Nicht zu verwechseln mit `docs/laufwerk.md`. Das ist ein kleines,
**schreibgeschuetztes** Laufwerk mit den Verwaltungsprogrammen. Hier geht
es um einen Datentraeger, auf den der Nutzer schreiben soll - ein ganz
anderer Fall, siehe „Das Betriebssystem schreibt, wann es will".

## Was von der offenen Hardware-Entscheidung abhaengt

Zuerst das Wichtigste fuer die Leitsitzung: was hier festliegt und was
noch wackelt.

| Teil des Entwurfs | haengt an der Hardware? |
| --- | --- |
| Drei Ebenen: Stempel je Block + Baum + Zaehler | nein, gilt immer |
| Rueckdreh-Schutz ueber den monotonen Zaehler | nein, der Chip bleibt der SE051C2 |
| MAC statt Signatur ueber den Wurzelhash | nein |
| Schluesselhierarchie und Freigabe je Sitzung | nein |
| Zwei-Phasen-Festschreibung | nein |
| Blockgroesse 4 KiB | kaum - passt zu Flash-Seite und SD-Sektor |
| **Wahl ChaCha20-Poly1305 statt AES-256** | **ja**, siehe unten |
| Die Durchsatz-Rechnung | **ja** |
| „Karte ohne Stick ist wertlos" | **ja**, nur bei Kartenschacht ein Thema |
| Groesse des Baums (8 oder 16 GB) | ja, beide gerechnet |

Die Verfahrensentscheidung, die wirklich von der Hardware abhaengt, ist
die Wahl des Verschluesselungsverfahrens - und zwar nur, weil der heute
verbaute nRF52840 eine bestimmte Luecke hat.

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

Was zu tun ist, steht unten unter „Vorschlaege an Dateien, die ich nicht
anfasse".

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
auf diesem Rechenkern laut Treibertabelle vom Beschleuniger kommt.

**Also:**

| | |
| --- | --- |
| Verfahren | ChaCha20-Poly1305 |
| Schluessel | 256 Bit |
| Blockgroesse | 4096 B Klartext |
| Stempel | 16 B je Block, **nicht** im Datenblock |
| Nonce | 12 B, nie wiederholt (unten) |
| zusaetzliche Daten (AAD) | Blocknummer, Datentraegerkennung, Formatfassung |

Die 4 KiB sind kein Zufall: es ist die Flash-Seitengroesse, die das
Projekt schon nutzt (`apps/firmware/prj.conf`,
`CONFIG_NORDIC_QSPI_NOR_FLASH_LAYOUT_PAGE_SIZE=4096`) und ein Vielfaches
des 512-B-Sektors, den ein USB-Massenspeicher dem Rechner zeigt. Ein
Rechner, der 512 Byte schreibt, loest also Lesen - Aendern - Schreiben auf
4 KiB aus. Das ist der Preis; 512-B-Bloecke waeren die Alternative, und
sie waeren achtmal so viele Stempel und ein achtmal so grosser Baum.

### Die Nonce - der eine Fehler, der alles bricht

Eine Nonce zweimal mit demselben Schluessel bei verschiedenem Klartext
bricht Poly1305 vollstaendig: es gibt nicht eine Nachricht her, sondern
den Stempelschluessel. Genau dieselbe Falle, die `shared/vault-format.md`
fuer AES-GCM schon beschreibt (Zeile 126-127).

Der Tresor hat dafuer ein erprobtes Muster, und der Datentraeger uebernimmt
es im Aufbau, nicht in den Zahlen:

```
Nonce (12 B) = Schreibzaehler des Blocks (4 B)
             + Blocknummer              (4 B)
             + Zweck                    (4 B)
```

- Der **Schreibzaehler je Block** waechst bei jedem Schreiben dieses
  Blocks um eins. Zwei Schreibvorgaenge auf denselben Block bekommen also
  verschiedene Nonces.
- Die **Blocknummer** trennt die Bloecke voneinander.
- Der **Zweck** trennt Daten von Baumseiten und Wurzelsatz, wie beim
  Tresor (`OSK_NONCE_BLOB/INDEX/LOG/...`).

Der Schreibzaehler muss beim Lesen wieder zur Hand sein. Er steht darum
neben dem Stempel im Stempelfeld - das ist der Grund, warum ein Eintrag
dort mehr als 16 Byte braucht.

4 Byte reichen fuer 4 294 967 295 Schreibvorgaenge **je Block**. Laeuft
einer ueber, muss der Datentraegerschluessel gewechselt werden (neue
Epoche, alles neu verschluesseln). Das ist ein offener Punkt, nicht
geloest - siehe unten.

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
USB-Anschlusses, die beschleunigte beim Fuenfzigfachen. Der Engpass ist
und bleibt der USB-Anschluss.

Mit einer Einschraenkung, und die ist der Grund, warum der Baum weiter
unten so sparsam gebaut ist: der Spielraum ist **Faktor 2**, nicht
Faktor 20. Faellt je ausgeliefertem Nutzdatenblock mehr als etwa das
Doppelte an Krypto-Arbeit an, wird die Krypta doch zum Engpass - wenn sie
in Software laeuft. Der Entwurf bleibt darum bei zwei zusaetzlichen
Hashvorgaengen je Leseoperation im schlechtesten Fall.

**Ungeprueft, und das ist wichtig:** die beiden Zeiten stammen aus
Nordics Veroeffentlichung, die Quelle wurde beim Schreiben dieses Papiers
**nicht gefunden** - sie kommt aus dem Auftrag. Eine unabhaengige
Messreihe am nRF52840-CryptoCell gibt es in `eprint.iacr.org/2021/058`;
sie misst mit anderer Eingabelaenge (64 B) und kommt zu dem Schluss, dass
gerade beim **Hashen** Hardware und Software naeher beieinander liegen als
bei anderen Operationen. Der Faktor 29,7 aus den obigen Zahlen ist also
mit Vorsicht zu nehmen. Die Aussage „USB ist der Engpass" haengt davon
nicht ab: sie gilt schon mit der Software-Zahl.

Auch die 400-500 kB/s sind nicht an diesem Geraet gemessen, sondern ein
Erfahrungswert fuer USB Full-Speed aus einem Parallelstrang.
**Ungeprueft.**

## Die Unverfaelschtheit - der Kern

Thomas' Wunsch: die Daten sollen ueber den Sicherheitschip pruefbar sein.
Der Vorschlag aus dem Auftrag war ein Merkle-Baum mit Wurzelhash im Chip.
Der Entwurf uebernimmt den Baum, aber in abgewandelter Form, und der
Wurzelhash wandert **nicht** in den Chip.

### Drei Ebenen, drei verschiedene Angriffe

| Ebene | Was sie erkennt | Womit |
| --- | --- | --- |
| 1. Stempel je Block | ein veraenderter Block | Poly1305-Stempel, 16 B |
| 2. Baum ueber die Stempel | ein **vertauschter** oder **einzeln zurueckgedrehter** Block | SHA-256 |
| 3. Zaehler im Chip | ein **vollstaendig zurueckgespielter** aelterer Stand | monotoner Zaehler im SE051C2 |

Warum alle drei gebraucht werden:

- Der Stempel allein genuegt nicht. Er bindet den Block an Schluessel,
  Blocknummer und Schreibzaehler - aber wer eine **alte, echte** Fassung
  von Block 4711 samt ihrem alten Stempel und Schreibzaehler
  zurueckschreibt, bekommt einen Block, der sich sauber entschluesselt.
  Der Stempel merkt nichts.
- Der Baum allein genuegt auch nicht. Er bindet alle Bloecke zu einem
  Gesamtstand - aber wer den **ganzen** Datentraeger auf einen aelteren,
  echten Stand zurueckspielt, Baum und Wurzel eingeschlossen, hat einen in
  sich stimmigen Stand. Genau das ist der Rueckdreh, und genau diese
  Luecke steht beim Tresor noch offen (`shared/vault-format.md:502-506`).
- Der Zaehler allein genuegt nicht, weil er nur eine Zahl ist. Er muss an
  einen Stand gebunden werden, und das tut der Wurzelhash.

### Der Baum - Aufbau und Groesse

Das Besondere: die **Blaetter sind nicht die Daten, sondern die Stempel**.
Das ist der Punkt, an dem ChaCha20-Poly1305 sich auszahlt.

```
Stempelfeld   je Block ein Eintrag von 32 B:
                16 B Poly1305-Stempel
                 4 B Schreibzaehler des Blocks
                12 B Reserve (Formatfassung, Zustandsbits)
              -> 128 Eintraege je 4-KiB-Seite

Ebene 1..n    je Seite der Ebene darunter ein SHA-256 (32 B)
              -> 128 Hashes je 4-KiB-Seite

Wurzelhash    SHA-256 ueber die oberste Seite, 32 B
```

Verzweigung also 128. Gerechnet (beide Groessen in Zweierpotenzen; die
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
als eigene SHA-256 ueber die Daten bildet (32 B je Block, kein
Schreibzaehler darin): das Stempelfeld waere genauso gross, aber **jede
Leseoperation muesste den 4-KiB-Datenblock zusaetzlich hashen**, obwohl
das Entschluesseln ihn gerade schon geprueft hat. Die Ersparnis liegt also
nicht im Platz, sondern in der Rechenzeit - ein Hashvorgang je
Leseoperation weniger, und bei Faktor 2 Spielraum (oben) ist das viel.

**Wo der Baum liegt: auf dem Datentraeger selbst.** Nicht im 64-MB-Flash
des Sticks. Dort ist kein Platz - `docs/laufwerk.md` rechnet 6,82 MiB frei
vor, und schon der kleine Baum braucht 64,50 MiB. Auch der Wurzelsatz
liegt auf dem Datentraeger (unten); im Chip steht nur der Zaehler.

### Was eine Leseoperation kostet

Im Stick-RAM wird die oberste Baumebene gehalten (4 bis 12 KiB - der
nRF52840 hat 256 KiB). Alles darunter muss gelesen werden.

| Fall | zusaetzliche Lesevorgaenge | zusaetzliche Hashvorgaenge | Verstaerkung |
| --- | ---: | ---: | ---: |
| ein einzelner Block, zufaellig | 2 Seiten zu 4 KiB | 2 zu 4 KiB | **3,0×** |
| 128 Bloecke in Folge (512 KiB) | 2 Seiten | 2 | **1,016×** |
| 1 MiB in Folge (256 Bloecke) | 3 Seiten | 3 | **1,012×** |

Begruendung: eine Stempelseite deckt 128 Datenbloecke ab, also 512 KiB
Nutzdaten; eine Ebene-1-Seite deckt 128 Stempelseiten ab, also 64 MiB.
Beim Lesen in Folge - und so liest ein Rechner Dateien - wird dieselbe
Stempelseite 128 Mal genutzt. Der Faktor 3 ist der schlechteste Fall:
verstreuter Zugriff auf einzelne Bloecke.

Gegen den Engpass gerechnet: 3 × 4 KiB Hashen je 4 KiB ausgelieferter
Daten. Mit beschleunigtem SHA-256 (25,8 MB/s) bleiben rechnerisch
8,6 MB/s, das Zwanzigfache des USB-Anschlusses. Mit Software-SHA-256
(870 kB/s) bleiben 290 kB/s - **unter** den 400 kB/s des Anschlusses. Das
ist der Grund, warum der Befund von oben wichtig ist: SHA-256 **ist**
beschleunigt, und der Baum traegt. Waere es nicht so, muesste die
Blockgroesse hoch oder der Baum flacher.

Ein hohes Zwischenlager im RAM wuerde den schlechtesten Fall entschaerfen,
aber das vollstaendige Halten der Ebene 1 kostet 1 MiB (16 GiB) - mehr als
der nRF52840 an RAM hat. Darum nur die Spitze.

Nicht genommen, aber mit Preis genannt: **16-KiB-Bloecke**. Das waere eine
Ebene weniger und ein Viertel des Baums - aber jeder 512-B-Schreibbefehl
des Rechners loest Lesen-Aendern-Schreiben auf 16 KiB aus, also 32-fache
Schreibverstaerkung im schlechtesten Fall. Bei 4 KiB sind es achtfach.
Fuer einen Datentraeger, auf dem ein Betriebssystem Verzeichniseintraege
fortschreibt, ist das der falsche Tausch.

### Der Rueckdreh - Wurzelhash und Zaehler zusammenbinden

Der Sicherheitschip hat monotone Zaehler. NXP AN12413 (SE05x APDU) und das
SE050-Datenblatt beschreiben sie:

- „can only be incremented and not be decremented"
- Laenge 1 bis 8 Byte, fluechtig oder dauerhaft
- Operationen Set, Get, Increment; `WriteCounter` **ohne** Wert erhoeht um
  eins, **mit** Wert setzt

Der Entwurf nutzt **einen dauerhaften Zaehler von 4 Byte**.

**Befund, der den Entwurf vereinfacht:** der Chip-Treiber im Projekt kann
heute **keine Binaerobjekte** -
`apps/firmware/src/classes/se050/se050_cmd.h:125-128` fuehrt
`TYPE_EC_KEY_PAIR`, `TYPE_AES_KEY`, `TYPE_USERID`, `TYPE_HMAC_KEY`, aber
**kein** `TYPE_BINARY` und **kein** `TYPE_COUNTER`. Ein 32-Byte-Wurzelhash
liesse sich also gar nicht einfach im Chip ablegen.

**Das ist kein Problem, sondern der bessere Weg.** Der Wurzelhash muss
nicht geheim sein und nicht im Chip liegen - er muss **unveraenderbar an
einen Zaehlerstand gebunden** sein. Das leistet ein MAC:

```
Wurzelsatz (auf dem Datentraeger, zweimal: Satz A und Satz B)
  magic, Formatfassung
  Zielzaehlerstand  N
  Wurzelhash        32 B
  Datentraegerkennung, Blockzahl, Epoche
  MAC               32 B = HMAC-SHA256(K_wurzel, alles davor)
                           K_wurzel verlaesst den Chip nie

Im Chip
  Zaehler  OSK_VOLUME_COUNTER_ID   dauerhaft, 4 B
  K_wurzel als HMACKey-Objekt      POLICY_OBJ_ALLOW_SIGN
```

Pruefung beim Oeffnen:

1. Zaehlerstand `C` aus dem Chip lesen.
2. Beide Wurzelsaetze lesen, MAC je Satz vom Chip pruefen lassen.
3. Gueltig ist der Satz mit `N == C`. Gibt es keinen, ist der
   Datentraeger nicht zu oeffnen (Sonderfall `N == C+1`: siehe
   „Stromausfall").
4. Oberste Baumseite lesen, hashen, gegen den Wurzelhash halten.

Ein Rueckdreh schreibt einen alten Wurzelsatz zurueck. Dessen `N` ist
kleiner als `C`, und `C` kann nicht fallen. Der Rueckdreh faellt auf.

**Das nutzt nur Operationen, die es fast schon gibt.** HMAC kann der
Treiber heute (`apps/firmware/include/osk_se050.h:69` →
`se050.c:663`, `HMAC_SHA256 0x19`). Fuer den Zaehler ist die Schnittstelle
schon vorgezeichnet: `apps/firmware/boot/osk_boot.h:78-83` hat
`counter_read`/`counter_raise` samt Kommentar „Monotoner Zaehler im
SE050", und `apps/firmware/boot/boot_se050.c:9-24` nennt die zwei
fehlenden Funktionen `osk_se050_counter_read` und
`osk_se050_counter_raise` mitsamt dem Objekttyp beim Namen. Gebaut wird
dieser Pfad heute nur mit `-DOSK_HAVE_SE050_COUNTER`, sonst liefert er
`OSK_ERR_UNSUPPORTED` (`:52-60`); die beiden Funktionen sind nirgends
definiert. Der Datentraeger braucht also **dieselben zwei Funktionen** und
eine zweite Objektkennung neben `OSK_BOOT_COUNTER_ID 0x4F534B01`
(`boot_se050.c:35`).

Nicht verwechseln mit zwei Zaehlern, die es schon gibt und die nichts
damit zu tun haben: dem SCP03-Befehlszaehler (`se050_scp03.h:61-63`) und
dem PIN-Fehlversuchszaehler (`se050.c:860`).

### Wer unterschreibt was: MAC, nicht Signatur

Im heissen Pfad **HMAC-SHA256** mit einem Schluessel, der den Chip nie
verlaesst. Begruendung:

- Pruefer und Stempler sind **dasselbe Geraet**. Eine Signatur leistet
  genau eine Sache mehr als ein MAC: sie ist von einem Dritten pruefbar,
  der den Schluessel nicht hat. Hier gibt es keinen solchen Dritten.
- Ein MAC ist billiger und schmaler: 32 B gegen 64-72 B, eine symmetrische
  Operation gegen eine ECDSA-Signatur.
- Wer `K_wurzel` hat, kann faelschen - aber `K_wurzel` steckt im Chip und
  kommt nicht heraus, genauso wie der Datentraegerschluessel. Der MAC ist
  also nicht schwaecher als die Verschluesselung daneben.

**Eine ECDSA-Signatur kommt dazu, aber nur selten und fuer einen anderen
Zweck:** wenn ein Dritter pruefen soll. Zwei Faelle, in denen das zaehlt -
das Verwaltungsprogramm am PC, das dem Nutzer belegen will, dass der Stand
echt ist, und ein Nachweis nach einem Verdacht. Dafuer bietet sich
`osk_se050_sign` (`osk_se050.h:48` → `se050.c:471`, `SIG_ECDSA_SHA_256`)
mit einem Schluessel, dessen oeffentlicher Teil bekannt gemacht wird. Das
ist eine Operation je Festschreibung im Ruhezustand, nicht je Block, und
kostet darum nichts, was man merkt. **Gestaltungsvorschlag, nicht
Pflichtteil.**

### Loest das auch die offene Stelle beim Tresor?

**Ja, und das ist ausdruecklich gesagt.** `shared/vault-format.md:502-506`
nennt den Rueckdreh als offene Grenze und als Behebung genau das, was hier
entworfen ist: „Fixing this needs a monotonic counter in the SE050 that is
compared when opening."

Der Wurzelsatz des Datentraegers und der Kopfsatz des Tresors haben
dieselbe Form des Problems und koennen dieselbe Loesung nutzen - ein
eigener Zaehler je Bereich, derselbe Code, dieselben zwei Chip-Funktionen.
Beim Tresor tritt an die Stelle des Wurzelhashs die laufende Nummer des
Kopfsatzes plus ein Hash ueber Kopf und Index; heute ist der Kopf nur mit
`crc32` abgesichert (`osk_vault_format.h:233`), also gegen Verwechslung,
nicht gegen Absicht.

Der Vorschlag dazu steht unten; `shared/vault-format.md` habe ich nicht
angefasst.

## Die Schluesselhierarchie

```
Grundgeheimnis im Chip                      verlaesst den Chip nie
   |
   | osk_se050_hkdf, im Chip gerechnet
   v
K_volume    "osk/volume/<kennung>"          32 B, je Datentraeger
   |
   +-- HKDF "osk/volume/data"  -> K_daten   ChaCha20-Poly1305, Datenbloecke
   +-- HKDF "osk/volume/tree"  -> K_baum    ChaCha20-Poly1305, Baumseiten
   +-- HKDF "osk/volume/root"  -> K_wurzel  HMACKey im Chip, bleibt dort
```

- `K_volume` entsteht **im Chip** aus dem Grundgeheimnis, mit der Kennung
  des Datentraegers als `info`. HKDF im Chip kann der Treiber heute
  (`osk_se050.h:57` → `se050.c:600`); dass der SE051C2 MAC und HKDF hat,
  ist in den Unterlagen des Kauf-Strangs festgehalten. **Zu pruefen:**
  HMACKey-Objekte brauchen das Applet-Merkmal `CONFIG_HMAC` (0x0010) -
  beim SE051C2 gegenpruefen, sonst gibt es `K_wurzel` nicht.
- `K_daten` und `K_baum` liegen **waehrend einer Sitzung im RAM** des
  Rechenkerns. Sie muessen dort liegen: bei 400 kB/s und 4-KiB-Bloecken
  sind das etwa 100 Operationen je Sekunde, und jede ueber den I2C zum
  Chip zu schicken ist aussichtslos.
- `K_wurzel` bleibt im Chip. Es ist der einzige Schluessel, der nie
  herauskommt, und genau er traegt den Rueckdreh-Schutz.

**Beim Sperren** - PIN-Sperre, Zeitablauf, Abziehen - wird der
Datentraeger festgeschrieben (unten), dann werden `K_daten` und `K_baum`
im RAM ueberschrieben (`osk_crypto_wipe`, wie im Tresor). Danach liefert
der Datentraeger auf jeden Lesebefehl `NOT READY`. Ohne PIN gibt es keinen
neuen Schluessel: `K_volume` kommt nur nach erfolgreicher PIN-Pruefung aus
dem Chip, und der Fehlversuchszaehler im Chip ist die eigentliche Schranke
- dieselbe Begruendung, aus der die Geraete-PIN schon heute ohne langsames
Verfahren abgeleitet wird.

### Wenn die Karte entnehmbar ist: ohne den Stick wertlos

Bei der Variante Kartenschacht gilt - und das ist nicht nur ein Wunsch,
sondern folgt aus dem Aufbau:

1. **Auf der Karte steht nur Chiffrat.** Datenbloecke, Baumseiten,
   Wurzelsatz. Kein Schluessel, kein Schluesselteil, keine
   schluesselverwandte Groesse.
2. **`K_volume` ist nicht auf der Karte ableitbar.** Es kommt aus dem
   Grundgeheimnis im Chip, und das verlaesst den Chip nicht. Es gibt
   keinen Weg von der Karte zum Schluessel - auch nicht mit der PIN. Die
   PIN oeffnet den Chip; ohne Chip oeffnet sie nichts.
3. Eine Karte in einem **fremden** Stick ist darum Rauschen. Nicht, weil
   der fremde Stick sich weigert, sondern weil er den Schluessel nicht
   herleiten kann.

Und die Gegenrichtung, die genauso wichtig ist: eine **fremde Karte in
diesem** Stick muss auffallen. Dafuer steht die Datentraegerkennung in den
zusaetzlichen Daten jedes Blocks (AAD) und im Wurzelsatz, und der
Wurzelsatz traegt den Zaehlerstand aus **diesem** Chip. Eine
untergeschobene Karte scheitert am MAC des Wurzelsatzes.

**Haengt an der Hardware-Entscheidung.** Bei einem geloeteten Baustein ist
der ganze Abschnitt gegenstandslos - dort ist die Verschluesselung ein
Schutz gegen das Ausloeten und Auslesen des Flash, nicht gegen den Verlust
einer Karte.

Nicht geloest: eine entnommene Karte laesst sich **kopieren**, und die
Kopie spaeter in denselben Stick zuruecklegen. Das ist ein Rueckdreh - und
genau der faellt am Zaehler auf. Der Zaehler ist damit nicht nur fuer den
Angreifer am Rechner da, sondern auch fuer den verlegten Kartenstapel.

## Stromausfall mitten im Schreiben

Der Tresor ist dafuer gebaut: Kopfsatz als Festschreibepunkt, Daten
zuerst, Verweis zuletzt (`shared/vault-format.md:325-348`). Ein
Merkle-Baum mit einer Wurzel, die an einen Zaehler gebunden ist, ist das
**nicht von selbst** - er hat zwei Dinge, die nicht in einem Zug
geschrieben werden koennen: den Wurzelsatz auf dem Datentraeger und den
Zaehler im Chip.

### Die Reihenfolge

```
1. Datenbloecke schreiben        (verschluesselt, mit Stempel)
2. Stempelseiten schreiben
3. Baumebenen von unten nach oben schreiben
4. Wurzelsatz in den *inaktiven* Satz schreiben, mit Zielstand N+1
5. Zaehler im Chip auf N+1 erhoehen            <- Festschreibepunkt
```

Nach Schritt 5 ist der neue Stand gueltig und der alte Satz wird beim
naechsten Mal ueberschrieben. Vorher gilt der alte Stand.

### Was ein Abbruch je Schritt bedeutet

| Abbruch | Zaehler | Was gilt | Was verloren ist |
| --- | --- | --- | --- |
| in 1 bis 3 | `N` | Satz mit `N` | alles seit der letzten Festschreibung |
| in 4 (Satz halb) | `N` | Satz mit `N`, der halbe faellt am MAC | dasselbe |
| **zwischen 4 und 5** | `N` | Sonderfall, unten | nichts, wenn der Satz stimmt |
| in 5 | `N` oder `N+1` | was der Chip sagt | nichts oder alles seit Schritt 1 |

**Das Fenster zwischen 4 und 5 ist unvermeidlich** und muss benannt
werden: der Satz mit `N+1` liegt schon auf dem Datentraeger, der Zaehler
steht noch auf `N`. Aufloesung:

> Ein Wurzelsatz mit Zielstand `C+1` bei Zaehlerstand `C` gilt als
> **vorbereitet**. Beim Oeffnen wird er angenommen - MAC geprueft, oberste
> Baumseite gegen den Wurzelhash gehalten - und dann wird die
> Zaehlererhoehung **nachgezogen**. Stimmt er nicht, bleibt der Satz mit
> `C` gueltig und der vorbereitete wird verworfen.

Das ist sicher, weil ein Angreifer einen Satz mit `C+1` nicht bauen kann:
dazu braeuchte er `K_wurzel` aus dem Chip. Und es ist nicht
missbrauchbar, weil `C+2` nie angenommen wird - der Sprung ist auf genau
eins begrenzt.

Was in Schritt 1 bis 3 verloren geht, ist **die Arbeit seit der letzten
Festschreibung, nicht der Datentraeger**. Der alte Stand ist vollstaendig
und stimmig; die halb geschriebenen Bloecke gehoeren zu keinem gueltigen
Baum und werden beim naechsten Schreiben ueberschrieben. Das ist genau die
Zusage, die der Tresor auch macht: ein Zustand oder der andere, nichts
dazwischen.

Was das fuer den Nutzer heisst, und das ist unangenehm: ein Abziehen
mitten im Kopieren kann **die ganze Sitzung** kosten, nicht nur die letzte
Datei - je nachdem, wann zuletzt festgeschrieben wurde. Darum muss oft
genug festgeschrieben werden (siehe naechster Abschnitt), und darum muss
die Anzeige am Stick sagen, wann es sicher ist abzuziehen.

### Der Zaehler als Verbrauchsgut

Jede Festschreibung ist ein Schreibvorgang im Chip. Das SE050-Datenblatt
(Rev. 3.8) nennt **100 Mio** Schreibzyklen global mit Verschleissausgleich
und mindestens 20 Mio, bei 25 Jahren Datenhaltung; NXP bestaetigt, dass
die Angabe global und nicht zellenbezogen gemeint ist.

Gerechnet: 50 Festschreibungen am Tag ueber 25 Jahre sind 456 250 - weit
unter der Mindestangabe. Selbst 1000 am Tag waeren 9,1 Mio, noch darunter.
Der Zaehler ist also **kein** knappes Gut, solange nicht je Schreibbefehl
des Betriebssystems festgeschrieben wird. Genau das waere der Fehler: ein
Betriebssystem schreibt beim Kopieren einer grossen Datei tausende Male je
Sekunde. Deshalb wird nach Ereignis festgeschrieben, nicht nach Vorgang -
bei `SYNCHRONIZE CACHE`, bei Auswurf, nach einer Ruhezeit, bei Sperre.
**Ungeprueft:** 4 Byte Zaehlerbreite reichen rechnerisch 4,29 Mrd Mal und
sind damit nicht die Schranke; die Schreibfestigkeit ist es.

## Das Betriebssystem schreibt, wann es will

Das ist der unangenehmste Abschnitt, und er aendert das Bedrohungsmodell.

### Befund

Ein USB-Massenspeicher bekommt Schreibbefehle ohne jede Freigabe. Steckt
der Datentraeger offen, schreibt Windows Papierkorb-Ordner, Indexdateien
und Zeitstempel hinein, ohne dass der Nutzer etwas tut. Schadsoftware auf
demselben Rechner kann dasselbe. Es gibt im Protokoll keinen Weg, einen
einzelnen 512-Byte-Schreibbefehl dem Nutzer vorzulegen und auf einen
Tastendruck zu warten - der Rechner wuerde in einen Zeitablauf laufen und
das Geraet fuer defekt halten.

Jede andere Funktion dieses Geraets ist anders gebaut: PIN und Tastendruck
sind Pflicht, und `docs/laufwerk.md` geht beim Programm-Laufwerk sogar so
weit, **jeden** Schreibbefehl abzuweisen.

### Was das bedeutet

Die Zusage, die dieser Entwurf geben kann, ist:

> **Was auf dem Datentraeger steht, hat dieser Stick geschrieben, und es
> ist der neueste Stand, den er geschrieben hat.**

Die Zusage, die er **nicht** geben kann:

> ~~Was auf dem Datentraeger steht, hat der Nutzer so gewollt.~~

Der Baum und der Zaehler schuetzen gegen einen Angreifer, der **am
Datentraeger** arbeitet: ihn ausliest, veraendert, kopiert, zurueckspielt
- auch mit dem Baustein in der Hand. Sie schuetzen **nicht** gegen einen
Angreifer, der **am freigegebenen Stick** arbeitet, denn dessen
Schreibbefehle sind echt und werden korrekt gestempelt. Das muss im
Bedrohungsmodell so stehen und darf nicht verwischt werden.

### Ist Freigabe je Sitzung der richtige Weg? Ja

Begruendung:

- **Je Vorgang ist nicht moeglich.** Siehe oben; das Protokoll gibt es
  nicht her.
- **Gar keine Freigabe waere schlechter.** Dann steht der Datentraeger
  offen, sobald der Stick steckt, und jeder Rechner schreibt hinein.
- **Je Sitzung ist das, was der Nutzer ueberhaupt entscheiden kann.** Er
  weiss, ob er diesem Rechner jetzt Daten geben will. Welchen der
  4 Millionen Bloecke das Betriebssystem dafuer anfasst, weiss er nicht
  und kann er nicht wissen.

Dazu gehoeren vier Dinge, sonst ist die Freigabe wertlos:

1. **Standardmaessig gar nicht da.** Nicht nur schreibgeschuetzt - beim
   Einstecken existiert der Datentraeger fuer den Rechner nicht. Erst PIN
   und Tastendruck am Geraet lassen ihn erscheinen.
2. **Zwei Stufen trennen.** Nur Lesen ist eine andere Freigabe als Lesen
   und Schreiben. Wer Daten mitnehmen will, braucht nicht zu erlauben,
   dass der Rechner schreibt. Schreibfreigabe ist der zweite, eigene
   Tastendruck, und sie wird im Schreibschutzbit von `MODE SENSE` gefuehrt,
   damit das Betriebssystem es vorher weiss - dieselbe Begruendung wie in
   `docs/laufwerk.md`.
3. **Sichtbar, solange sie gilt.** Die Anzeige sagt waehrend der ganzen
   Sitzung, dass der Datentraeger offen ist, und bei Schreibfreigabe
   deutlicher. Eine Freigabe, die man vergisst, ist keine.
4. **Von selbst zu Ende.** Nach einer Ruhezeit ohne Zugriff wird
   festgeschrieben und gesperrt. Das deckt den Fall „Stick steckt noch im
   fremden Rechner" ab, der in der Praxis haeufiger ist als jeder
   Angreifer.

Was zusaetzlich geht und billig ist: **mitschreiben, was geschrieben
wurde**. Die Zahl der geschriebenen Bloecke je Sitzung steht ohnehin fest,
weil jede Festschreibung sie kennt. Dem Nutzer nach der Sitzung „4,2 MB
geschrieben" zu zeigen, ist kein Schutz, aber es macht den Unterschied
sichtbar zwischen „ich habe eine Datei kopiert" und „der Rechner hat
hinter meinem Ruecken gearbeitet". Der Protokollring des Tresors
(`osk_vault_format.h:412`) ist dafuer das Vorbild.

## Vorschlaege an Dateien, die ich nicht anfasse

`hardware/`, `shared/vault-format.md` und `docs/rechenkern-durchsatz.md`
sind unberuehrt. Was dort geaendert werden sollte:

1. **`shared/vault-format.md`, „Known limits", Rueckdreh (Zeile 502-506).**
   Der Punkt kann von „Open" auf „Entwurf liegt vor" gehen und auf diesen
   Abschnitt verweisen. Loesung und Code sind dieselben: ein dauerhafter
   Zaehler im Chip, eigene Objektkennung, Vergleich beim Oeffnen, plus ein
   MAC ueber den Kopfsatz statt des `crc32`.
2. **`shared/vault-format.md`, dort wo AES-GCM steht.** Der Befund von oben
   gehoert dazu: auf dem nRF52840 kommt AES-256-GCM nicht vom
   Beschleuniger. Das ist fuer den Tresor nicht schlimm (kleine Haeppchen),
   aber es steht derzeit nirgends, und wer die Stellen zu Laufzeit und
   Passkey-Anmeldung liest (Zeile 204 und 215), koennte das Gegenteil
   annehmen.
3. **`docs/vault.md`** entsprechend, falls dort Durchsatz behauptet wird.
4. **Nebenbefund, der nicht zu diesem Papier gehoert, aber gefunden
   wurde:** in `apps/firmware/prj.conf:34-59` fehlt
   `CONFIG_PSA_WANT_ALG_ARGON2*`, obwohl `osk_crypto_argon2id`
   (`osk_crypto_psa.c:178-203`) darauf baut und ohne Treiber
   `OSK_ERR_UNSUPPORTED` liefert (`:210-222`). Das trifft die
   Sicherungsdatei, nicht den Datentraeger. Gemeldet, nicht geaendert.
5. **`apps/firmware/include/osk_se050.h`** braucht
   `osk_se050_counter_read` und `osk_se050_counter_raise` - heute als
   fehlend vermerkt in `boot/boot_se050.c:9-24`. Der Datentraeger und der
   Tresor brauchen beide dieselben zwei Funktionen; das ist eine Baustelle,
   nicht drei.

## Was geprueft ist, und wie

Nichts ist an Hardware gemessen. **Es gibt kein Brett, keinen
Datentraeger und keinen Code zu diesem Entwurf.**

| Angabe | Quelle | Stand |
| --- | --- | --- |
| cc3xx nur 128-Bit-AES auf cc310 | nRF Connect SDK, Treiberseite, woertlich zitiert | Herstellerdoku |
| kein Hardware-GCM auf cc310 | ebenda | Herstellerdoku |
| XTS in keiner Treibertabelle | ebenda, Tabellen Cipher und AEAD | Herstellerdoku |
| ChaCha20-Poly1305 bei cc3xx gelistet | ebenda | Herstellerdoku |
| „128 und 256 Bit" im Infocenter | Nordic Infocenter, CryptoCell-310-Seite | Herstellerdoku, **widerspricht** der Treiberseite |
| SHA-256 beschleunigt | nRF52840 PS, CryptoCell | Herstellerdoku |
| Tresor nutzt AES-256-GCM | `osk_crypto.h:20`, `osk_crypto_psa.c:61-64,81` | im Code gelesen |
| kein Treiberschalter im Repo | Suche ueber alle `*.conf` | im Repo gemessen |
| **Tresor rechnet in Software** | Folgerung aus den drei Zeilen darueber | **ungeprueft**, abgeleitet |
| 25,8 MB/s und 870 kB/s fuer SHA-256 | Nordic-Vergleichswert, aus dem Auftrag | **ungeprueft**, Quelle nicht gefunden |
| Gegenprobe zum Hashen | `eprint.iacr.org/2021/058`, andere Eingabelaenge | fremde Messung |
| 400-500 kB/s am USB | Erfahrungswert Full-Speed, Parallelstrang | **ungeprueft** |
| Zaehler nur aufwaerts, 1-8 B | NXP AN12413, SE050-Datenblatt | Herstellerdoku |
| 100 Mio / min. 20 Mio Schreibzyklen | SE050-Datenblatt Rev. 3.8 | Herstellerdoku |
| kein `TYPE_COUNTER`/`TYPE_BINARY` im Treiber | `se050_cmd.h:125-128` | im Code gelesen |
| HMAC, ECDSA, HKDF vorhanden | `osk_se050.h:48,57,69` | im Code gelesen |
| Zaehlerfunktionen fehlen | `boot/boot_se050.c:9-24`, `osk_boot.h:78-83` | im Code gelesen |
| Baumgroessen, Ebenen, Anteile | hier gerechnet, Zweierpotenzen | gerechnet |
| 3,0× / 1,016× / 1,012× Verstaerkung | hier gerechnet aus Verzweigung 128 | gerechnet |
| 6,82 MiB frei im Stick-Flash | `docs/laufwerk.md` | dort gerechnet |

## Offene Punkte

1. **Der Schreibzaehler je Block kann ueberlaufen.** 4 Byte reichen fuer
   4,29 Mrd Schreibvorgaenge auf **einen** Block. Ein Betriebssystem, das
   denselben Verzeichnisblock immer wieder fortschreibt, kommt dem naeher
   als es klingt. Was dann passieren muss - neue Epoche, Datentraeger neu
   verschluesseln - ist nicht entworfen. Der Baustein selbst gibt
   vermutlich vorher auf (Flash-Verschleiss), aber darauf darf man sich
   nicht verlassen.
2. **Das Festschreiben nach `SYNCHRONIZE CACHE` ist nicht gemessen.** Wie
   oft ein Windows, ein Linux und ein macOS diesen Befehl beim Kopieren
   schicken, entscheidet, wie viele Chip-Schreibvorgaenge anfallen und wie
   viel ein Abziehen kostet. Das laesst sich nur am Rechner messen, und es
   gibt keinen.
3. **Der schlechteste Fall beim Lesen traegt nur mit Hardware-SHA-256.**
   290 kB/s rechnerisch in Software gegen 400 kB/s Anschluss. Faellt der
   Befund „SHA-256 ist beschleunigt" bei der Messung anders aus, muss der
   Baum flacher oder die Blockgroesse groesser werden.
4. **`CONFIG_HMAC` beim SE051C2 ist nicht bestaetigt.** Ohne dieses
   Applet-Merkmal gibt es keine HMACKey-Objekte und damit kein `K_wurzel`.
   Dann muesste der Wurzelsatz anders gestempelt werden, etwa mit einem
   transienten abgeleiteten Schluessel.
5. **Der Rechenkern ist offen.** Faellt die Entscheidung auf einen Kern mit
   AES-256 im Beschleuniger und mehr RAM, ist die Wahl
   ChaCha20-Poly1305 neu zu pruefen - XTS bleibt trotzdem fraglich, weil es
   keinen Stempel liefert und die Treiberlage offen ist. Alles andere in
   diesem Papier bleibt gueltig.
6. **Der Baum deckt den Wurzelsatz nicht ab, der Wurzelsatz den Baum.** Das
   ist beabsichtigt, aber es heisst: der Wurzelsatz ist die einzige Stelle,
   deren Zerstoerung den ganzen Datentraeger unlesbar macht. Zwei Saetze
   (A und B) sind dagegen die Vorsorge; ob zwei genug sind, ist nicht
   begruendet, sondern uebernommen.
7. **Nichts davon ist umgesetzt.** Es gibt keinen Code, und die beiden
   Chip-Funktionen, auf denen alles steht, fehlen bis heute.
