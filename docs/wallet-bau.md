# Die Geldbörse: wie sie gebaut ist und was sie verweigert

Dieses Papier beschreibt den **Bau**. Was die Geldbörse können soll, steht
in `docs/wallet.md`; das ist die Beschreibung und wird hier nicht
geändert. Hier steht, wie es umgesetzt ist, was absichtlich fehlt, und —
der längste Abschnitt — **was das Gerät nicht unterschreibt und warum
nicht**.

Stand: 30.10.2026. Stufe 2 von 4: geschrieben / übersetzt / im Abbild /
am Gerät gelaufen. **Auf Hardware ist nichts davon gelaufen.** Jede Zahl
in diesem Papier kommt von einem Lauf am PC über die WSL.

---

## 1. Die Regel, die alles bestimmt

> **Was das Gerät nicht lesbar anzeigen kann, unterschreibt es nicht.**

Das ist der Grund, warum in diesem Gerät überhaupt eine Anzeige verbaut
ist. Der Rechner, der die Überweisung schickt, kann belogen worden sein.
Was auf der Anzeige steht, kommt aus den Bytes, die wirklich
unterschrieben werden — nicht aus dem, was der Rechner behauptet.

Daraus folgt eine Entscheidung, die vorher anders war:

### Die Blindunterschrift ist entfernt

Vorher gab es ein `allow_blind` durch alle Schichten: „verweigert,
**außer** der Nutzer schaltet es ein". Das ist jetzt weg — nicht als
Schalter, nicht als Sonderfall, nicht nach Freigabe.

Der Grund: in einem Gerät, dessen ganzer Zweck diese Regel ist, ist ein
Schalter, der sie abschaltet, kein Zugeständnis an den Nutzer. Er ist die
Stelle, auf die ein Angreifer ihn hinredet. Wer am Telefon überredet
wird, „kurz die erweiterte Freigabe einzuschalten", hat danach ein Gerät,
das alles unterschreibt.

Eine Frage, die man mit Ja beantworten kann, **ist** die
Blindunterschrift. Darum wird bei einer nicht lesbaren Ansicht nicht
einmal gefragt: `osk_wallet_confirm()` gibt `OSK_ERR_DENIED` zurück, ohne
die Anzeige zu bemühen. Geprüft in `test_confirm.c`
(`test_keine_blindunterschrift`): dreimal hintereinander, mit einem
Taster, der Ja sagen *würde*, und `mock_ui_calls()` bleibt 0.

Im Drahtformat bleibt ein Rest stehen: `GET_CONFIG` liefert im ersten
Byte weiterhin das Bit `0x01` (`OSK_APDU_FLAG_BLIND`), das einmal „blind
ist möglich" hieß. Es ist auf 0 festgenagelt. Das Byte bleibt, damit eine
Gegenstelle, die es liest, nicht ins Leere greift — und damit ein Rechner
**vorher** erfährt, dass er es nicht versuchen muss.

### Und wer sie trotzdem verlangt, bekommt einen Fehler

Das Feld auf 0 festzunageln **reicht nicht.** Eine gemeldete 0 sagt nur,
was das Gerät *kann* — sie sagt nichts darüber, was passiert, wenn ein
Rechner das Blindsignieren **verlangt**.

Würde eine solche Bitte still überlesen oder auf 0 zurechtgebogen, wäre
das schlimmer als gar keine Prüfung: der Rechner bekäme ein `0x9000` und
glaubte, er habe es eingeschaltet; der Nutzer glaubte, es sei aus. Beide
hätten recht in ihrer eigenen Vorstellung, und niemand merkte den
Unterschied — bis etwas unterschrieben ist. **Ein stiller Riegel ist
schlimmer als ein laut krachender.**

Genau dieser Fehler ist in diesem Projekt am **29.10.2026** schon einmal
gebaut worden: `osk_store_add()` machte eine Kennung 0 klammheimlich zu
1, statt einen Fehler zu geben. Folge wären zwei Einträge mit derselben
Kennung gewesen — und `osk_store_reveal` hätte unter einer Kennung das
Geheimnis des **falschen** Eintrags herausgegeben.

Darum, mit eigenem Statuswort:

```
#define OSK_SW_BLIND_REFUSED 0x6A81u   /* ISO 7816-4: nicht unterstützt */
```

Eigener Code und **nicht** `OSK_SW_DENIED` (`0x6985`), damit eine
Gegenstelle „der Nutzer hat nein gesagt" von „danach zu fragen ist in
diesem Gerät nicht vorgesehen" unterscheiden kann.

Abgewiesen wird die Bitte auf **jedem** Weg, auf dem sie ankommen kann:

| Weg | Antwort |
|---|---|
| `GET_CONFIG` mit `FLAG_BLIND` in P1 | `0x6A81` |
| `GET_CONFIG` mit `FLAG_BLIND` in P2 | `0x6A81` |
| `GET_CONFIG` mit einem Datenbyte, `FLAG_BLIND` gesetzt | `0x6A81` |
| `GET_CONFIG` mit anderen Daten oder anderem P1/P2 | `0x6A80` |

`GET_CONFIG` hat keine Eingabe. Wer trotzdem Daten schickt, will
Eigenschaften **setzen** — und die einzige, die es hier je gab, ist das
Blindsignieren. Die Daten einfach wegzuwerfen wäre der stille Riegel.

### Dasselbe Prinzip für P1 und P2 überall

Der stille Riegel ist eine Fehlerklasse, nicht ein Einzelfall. Überall,
wo ein Feld vom Rechner kommt, wird ein unbekannter Wert jetzt
**abgewiesen** statt auf einen bekannten zurechtgebogen:

- **Sign-Befehle, P2**: `0` heißt „das war das letzte Stück", `1` heißt
  „es kommt noch mehr". Ein dritter Wert wird nicht als `0` gelesen. Das
  ist der gefährlichste stille Riegel in dieser Schicht: „es kommt noch
  mehr" versehentlich als „jetzt unterschreiben" zu verstehen hieße, ein
  **halbes PSBT zu unterschreiben**. Ein angefangener Zusammenbau wird
  dabei verworfen.
- **`GET_PUBKEY`, P2**: `0` = nicht anzeigen, `1` = am Gerät anzeigen.
  Alles andere ist keine der beiden Bedeutungen.

Wie ernst der erste Punkt ist, zeigt die Mutationsprüfung: nimmt man die
P2-Prüfung heraus, antwortet das Gerät auf ein vollständiges PSBT mit
`P2 = 0x02` nicht mit einer Abweisung, sondern mit `0x9000` — es
**unterschreibt wirklich**.

---

## 2. Was angezeigt wird

Vier Dinge, und alle vier müssen da sein, sonst wird nicht
unterschrieben:

| Feld | Inhalt | fehlt es, dann |
|---|---|---|
| Empfänger | die Adresse, **ganz** | abgelehnt |
| Betrag | Satoshi bzw. Wei, als Dezimaltext | abgelehnt |
| Gebühr | dieselbe Einheit | abgelehnt |
| Kette / Art | Netz, Vertragsaufruf, Anzahl weiterer Empfänger | darf leer sein |

Beträge werden **nie** als Fließkommazahl gerechnet. Satoshi und Wei sind
ganze Zahlen und bleiben es bis in den Text (`osk_wallet_format_amount`,
`osk_wallet_format_amount256` — Ethereum rechnet in Wei, und das passt
nicht in 64 Bit).

### Die Adresse: der Angriff, der den üblichen Weg benutzt

Eine Bitcoin-Adresse hat bis zu 90 Zeichen, eine Ethereum-Adresse 42. Die
Anzeige ist **10,54 × 14,70 mm**. Das passt nicht.

Der übliche Weg ist, Anfang und Ende zu zeigen und die Mitte durch
`...` zu ersetzen. **Das ist auch der übliche Angriff.** Ein Angreifer
sucht sich so lange eine Adresse, bis Anfang und Ende passen — das ist
Rechenarbeit, aber keine schwere —, und die Mitte sieht niemand. Genau
das hat dieser Code vorher getan (`osk_wallet_shorten`), und nirgends
stand, dass es ein Angriff ist.

**Was jetzt gemacht wird — und was das löst:**

1. **Es wird nicht gekürzt.** `osk_wallet_shorten` ist gelöscht und
   durch `osk_wallet_set_address` ersetzt, das die Adresse ganz
   übernimmt oder `OSK_ERR_FORMAT` gibt. Es existiert keine Funktion
   mehr, die eine Adresse kürzt. **Das ist die Lösung**: steht die ganze
   Adresse da, braucht der Angreifer eine vollständige Kollision, und
   die gibt es nicht.
2. **Gegliedert in Vierergruppen** (`osk_wallet_group_address`). Das Auge
   verliert in einer Reihe aus 42 ähnlichen Zeichen die Stelle; in
   Vierergruppen nicht. Der Inhalt wird dabei nicht angetastet, nur
   gegliedert — geprüft Zeichen für Zeichen in `test_address`.
3. **Eine Prüfzahl über alle Zeichen** (`osk_wallet_fingerprint`): acht
   Ziffern aus dem SHA-256 der ganzen Adresse, angezeigt als `#12345678`.
   Ein einzelnes vertauschtes Zeichen mitten in der Adresse ändert sie.

**Was die Prüfzahl nicht ist — das muss deutlich sein:** sie ist nicht
der Schutz. Acht Ziffern sind rund 27 Bit, und 27 Bit kann ein Angreifer
durchprobieren. Wer **nur** die Prüfzahl vergleicht und die Adresse nicht
liest, ist nicht geschützt. Die Prüfzahl ist der zweite, unabhängige
Vergleich für den, der die Adresse überfliegt — der Schutz ist, dass die
ganze Adresse dasteht.

**Was dabei ungelöst bleibt, offen gesagt:** ob ein Mensch 42 Zeichen auf
einem 10,54 × 14,70 mm großen Glas wirklich vergleicht, entscheidet
dieser Code nicht. Er kann nur dafür sorgen, dass alles da ist, was man
vergleichen *müsste*. Ein Nutzer, der durchklickt, ist durch keine
Technik in diesem Modul zu retten. Die Zeitgrenze steht deshalb bei 60
Sekunden und nicht bei fünf (`CONFIRM_TIMEOUT_MS`) — eine zu kurze Frist
wäre eine Einladung zum Durchklicken.

---

## 3. Was ABGELEHNT wird, statt ungefähr angezeigt zu werden

Der Kern dieses Strangs. Eine Anzeige, die „ungefähr" stimmt, ist
schlimmer als eine Ablehnung, **weil der Nutzer ihr glaubt**.

| Fall | Verhalten | geprüft in |
|---|---|---|
| Taproot-Eingang (BIP341) | `OSK_ERR_UNSUPPORTED`, mit Meldung, **vor** der Frage | `test_psbt.c: test_taproot_abgelehnt` |
| Mehr als 8 Ausgänge | abgelehnt beim Zerlegen | `test_psbt.c: test_zu_viele_ausgaenge` |
| Mehr als 8 Eingänge | abgelehnt beim Zerlegen | `OSK_PSBT_MAX_IN` |
| Vertragsaufruf mit Daten | `OSK_ERR_DENIED`, Grund `wallet.vertragsaufruf` | `test_eth.c` |
| EIP-712 über APDU | **immer** `OSK_ERR_DENIED` | `test_eth.c`, `test_apdu.c` |
| Nachricht mit Steuerzeichen | abgelehnt | `test_eth.c` |
| Pfad tiefer als 8 Stufen | `OSK_ERR_FULL` | `test_bip32.c` |
| Anderer Sighash als ALL | `OSK_ERR_UNSUPPORTED` | `wallet_psbt.c` |

### Taproot

Abgelehnt, nicht halb gebaut. Taproot ist ein eigenes Hashverfahren und
ein eigener Signaturtyp; eine halbe Umsetzung würde unterschreiben, und
niemand wüsste, was. Erkannt wird es am Skript (`OP_1` und 32 Byte,
`OSK_BTC_P2TR`), und zwar an zwei Stellen: in einer Vorab-Prüfung in
`osk_psbt_sign`, **bevor** der Nutzer gefragt wird, und noch einmal in
`sighash_for` als zweiter Riegel (auch hinter einem P2SH-Redeem-Skript).

Die Vorab-Prüfung steht vor der Frage und nicht danach: den Nutzer etwas
freigeben zu lassen, das anschließend scheitert, wäre eine Freigabe für
nichts. Und es gibt eine Meldung — `wallet.taproot-nicht-unterstuetzt` —,
denn eine Ablehnung ohne Erklärung ist am Gerät nicht zu verstehen.

Der Test enthält eine **Gegenprobe**: dasselbe PSBT ohne den Taproot-
Tausch wird unterschrieben. Ohne sie wüsste niemand, ob die Ablehnung am
Taproot hängt oder an einem Tippfehler in der Testdatei.

### EIP-712: der Fall, der echte Verträglichkeit kostet

Der APDU-Befehl für typisierte Nachrichten übergibt **zwei Hashes** —
den Bereichstrenner und den Hash der Struktur. Aus zwei Hashes kann
niemand zurückrechnen, was dort stand. Das Gerät könnte also nur
„unterschreibe etwas" anzeigen, und das ist die Blindunterschrift mit
einem höflicheren Namen.

Also: **immer abgelehnt.** Der Preis ist echt und soll nicht
schöngeredet werden: **MetaMask kann über dieses Gerät keine typisierte
Nachricht unterschreiben lassen.** Wer das braucht, braucht ein anderes
Gerät oder einen Befehl, der die Struktur selbst überträgt statt ihren
Hash. Letzteres wäre baubar, ist aber nicht gebaut.

Das Hashen nach EIP-712 ist trotzdem umgesetzt und gegen das Beispiel aus
der Beschreibung geprüft (`osk_eth_typed_hash`) — es fehlt nicht die
Rechnung, es fehlt die Möglichkeit, dem Nutzer zu zeigen, was er freigibt.

### Zu viele Ausgänge

Acht sind die Grenze (`OSK_BTC_MAX_OUT`). Neun teilweise anzuzeigen wäre
die gefährlichste Anzeige von allen: der Nutzer sähe acht richtige Zeilen
und wüsste nicht, dass eine fehlt. Der Test geht die Zahlen 1 bis 10
durch und verlangt, dass es **bis** zur Grenze geht und **darüber** nicht
— ohne die erste Hälfte würde die zweite nichts beweisen.

---

## 4. Die Ledger-verträgliche APDU-Schicht

Nach der öffentlich beschriebenen Form selbst geschrieben; kein fremder
Code gelesen oder nachgebildet.

Umgesetzt sind fünf Ethereum-Befehle (öffentlichen Schlüssel holen,
Transaktion, Nachricht nach EIP-191, typisierte Nachricht, Fassung
abfragen) und **ein eigener** Bitcoin-Befehl `0xF0`, der ein ganzes PSBT
annimmt und unterschrieben zurückgibt.

Der alte Bitcoin-Weg mit `GET_TRUSTED_INPUT` ist **nicht** umgesetzt: er
verlangt, dass das Gerät fremde Transaktionen in einem eigenen Format
zwischenspeichert und mit einem geräteeigenen Schlüssel beglaubigt. Viel
Code für einen Weg, den PSBT ersetzt. Folge: Electrum und Sparrow können
PSBT über Datei oder Zwischenablage — das geht also, **nur nicht als
angestecktes Ledger-Gerät**.

### Diese Schicht wird gefuzzt

Sie ist die Stelle, die ein Angreifer **unmittelbar** erreicht: jeder
Rechner, an dem der Stick steckt, kann APDUs schicken, ohne dass ein
Mensch zustimmt. Die Freigabe am Taster kommt erst, wenn diese Schicht
die Bytes schon gelesen, einen Pfad gebaut und ein PSBT zusammengesetzt
hat. Alles davor passiert auf Zuruf eines Fremden.

`test/wallet/fuzz_apdu.c`, gebaut nach dem Muster von
`test/msc/fuzz_scsi.c`, mit ASan und UBSan. Sechs Zusicherungen; die
beiden, auf die es ankommt:

- **Ohne Freigabe entsteht nie eine Unterschrift.** Der Taster sagt im
  ganzen Lauf nein; danach muss `mock_se050_imports()` null sein.
- **Der Lauf muss das Unterschreiben wirklich erreichen.** Zufällige
  Bytes ergeben nie ein gültiges PSBT — ein Lauf aus lauter Zufall würde
  die erste Zusicherung niemals auf die Probe stellen und wäre damit
  keine Prüfung. Darum wird regelmäßig das echte PSBT aus
  `psbt_vectors.h` ordentlich zerteilt durchgeschickt, und am Ende wird
  gezählt, wie oft die Freigabe wirklich verlangt wurde. Nie = rot.

Dazu eine siebte: **wer das Blindsignieren verlangt, bekommt nie ein
`0x9000`.** Der Lauf baut die Bitte gezielt (aus zufälligen Bytes
entsteht sie kaum), zählt mit, wie oft sie wirklich vorkam, und gilt als
rot, wenn das nie passiert ist.

Diese Zusicherung gilt nur für Rahmen, die *sonst* in Ordnung sind. Lügt
das Längenbyte, wird die APDU vorher als kaputt abgewiesen — das ist
richtig und auch nicht still, der Rechner bekommt einen Fehler. Ich
hatte das zuerst nicht unterschieden, und der Fuzzer hat mir prompt eine
Abweisung mit `0x6700` als Verstoß gemeldet, die keiner war.

Zwei Dinge, die diesen Fuzzer von einem beruhigenden unterscheiden:

- Jede APDU wird in einen **passgenauen** Puffer umkopiert, bevor sie
  hineingeht. Liegt sie in einem großen Puffer, landet ein Zerleger, der
  dem Längenbyte blind glaubt, immer noch *innerhalb* des Puffers, und
  niemand merkt es.
- Es gibt eine Sorte **abgeschnittener Befehl**: das Längenbyte kündigt
  viel an, dahinter kommt fast nichts. Aus zufälligen Bytes entsteht das
  so gut wie nie.

---

## 5. Prüfen: was gelaufen ist und was es wert ist

### Zahlen

| | vorher | nachher |
|---|---|---|
| Fälle | 1099 | **1323** |
| Fehler | 0 | **0** |

Je Baustein: bip32 270, psbt 206, apdu 160, eth 132, confirm 126, bip39
115, ec 69, btc 67, codec 63, rlp 61, bn 28, hash 26.

Fuzzläufe (15 s je Lauf): `fuzz_psbt` 1043 Durchgänge (66 unterschrieben),
`fuzz_rlp` 2,9 Mio, `fuzz_apdu` 5642 APDUs (288 mal bis zur Freigabe,
**769 mal wurde das Blindsignieren verlangt** und 769 mal abgewiesen).
Alle grün, kein Absturz.

### Amtliche Testwerte — der Beleg, der nicht von uns kommt

- **BIP32**: die Testvektoren 1 bis 5 aus der Beschreibung.
- **BIP39**: die Vektoren mit Passwort `TREZOR`.
- **EIP-155**: das Beispiel aus der Beschreibung selbst.
- **EIP-712**: das vollständige Beispiel.
- **EIP-55**: die Prüfsummen-Beispiele.
- **Bech32 / Bech32m**: die gültigen und ungültigen Zeichenketten aus
  BIP173 und BIP350.

### Der fremde Beleg: openssl

**Eine selbstgebaute Signatur, die nur der eigene Code nachrechnet,
beweist nichts.** Sie beweist, dass beide Seiten dieselbe Vorstellung
haben — nicht, dass die Vorstellung stimmt. Genau daran hat sich dieses
Projekt am 29.10.2026 verletzt.

Für Ableitung und Hashen gibt es amtliche Vektoren (siehe oben). Für die
**Signatur selbst** gibt es keine: sie hängt am Zufallswert *k*. Also
rechnet openssl nach — ein Programm, das nichts von diesem Projekt weiß.

`make fremdbeleg` (in `apps/firmware/test/wallet`), gelaufen mit
**OpenSSL 3.5.7**:

```
ok   1. oeffentlicher Schluessel: openssl rechnet denselben Punkt aus
ok   2. Signatur: openssl bestaetigt sie
ok   3. Gegenprobe: openssl verwirft den verbogenen Hash
```

- **1** belegt die Punktmultiplikation auf der Kurve: openssl bekommt nur
  den privaten Schlüssel und rechnet den öffentlichen selbst aus — er
  stimmt Byte für Byte mit unserem überein.
- **2** belegt ECDSA: openssl prüft unsere DER-Signatur gegen Schlüssel
  und Hash.
- **3** ist die Gegenprobe. Ohne sie wüsste niemand, ob openssl
  überhaupt hinsieht; eine Prüfung, die immer Ja sagt, ist keine.

Was openssl **nicht** belegt: die Schlüsselableitung (dafür die
BIP32-Vektoren) und das Bilden der Transaktionshashes (dafür EIP-155 und
EIP-712). Fehlt openssl auf einem Rechner, endet das Skript mit 2 und
sagt, dass der Beleg nicht erbracht ist — es wird nicht stillschweigend
grün.

### Jede neue Prüfung wurde zum Anschlagen gebracht

Eine Prüfung, die man nicht hat anschlagen sehen, ist keine. Der Code
wurde absichtlich kaputtgemacht; alle **zwölf** Mutationen wurden
gefunden:

| Mutation | gefunden von |
|---|---|
| Adresse wieder kürzen | `test_psbt.c` (keine Punkte / ganze Adresse) |
| Prüfzahl ignoriert die Mitte | `test_confirm.c` (Prüfzahlen verschieden) |
| Blindunterschrift wieder zulassen | `test_eth.c` (nicht gefragt) |
| Vertragsaufruf durchlassen | `test_eth.c` (Grund genannt) |
| Nein des Nutzers ignorieren | `fuzz_apdu` (Taster sagte nein, Gerät sagte 0x9000) |
| Daten trotz Fehlerstatus | `fuzz_apdu` (8 Byte trotz 0x6e00) |
| Erfundenes Statuswort | `fuzz_apdu` (unbekanntes Statuswort 0x1234) |
| Längenbyte ungeprüft | `fuzz_apdu` → **ASan**: heap-buffer-overflow |
| Blind-Bitte still überlesen | `test_apdu.c` + `fuzz_apdu` (mit `0x9000` beantwortet) |
| Blind-Bitte in P1/P2 auf 0 zurechtgebogen | `test_apdu.c` + `fuzz_apdu` |
| Mit `DENIED` statt eigenem Code abgewiesen | `fuzz_apdu` (`0x6985` statt `0x6a81`) |
| P2 still zurechtgebogen | `test_apdu.c` (`0x9000` statt Abweisung) |

Zur Zeile „P2 still zurechtgebogen" gehört eine zweite Lehre, denn sie
hat mich beinahe getäuscht: meine erste Fassung dieser Prüfung schickte
**Unsinnsbytes** mit `P2 = 0x02`. Sie schlug nicht an — und zwar zu
Recht, denn ein kaputtes PSBT wird mit „falsche Daten" abgewiesen, ob P2
geprüft wird oder nicht. Die Prüfung war **leer**, und ich hätte sie für
bestanden gehalten. Erst mit einem **echten, gültigen** PSBT ist der
Unterschied eindeutig: wird P2 geprüft, kommt eine Abweisung; wird es
zurechtgebogen, wird wirklich unterschrieben. Jede dieser Prüfungen hat
darum eine **Gegenprobe**, die zeigt, dass der Weg ohne den Fehler
funktioniert.

Zur Zeile „Längenbyte ungeprüft" eine Warnung für den nächsten, der
dieses Werkzeug benutzt: die Mutation schien zuerst *nicht* anzuschlagen. Sie schlug an —
nur schreibt ASan kein „FEHLER", sondern bricht mit einem eigenen Bericht
ab. Das Mess-Skript sah auf die falsche Zeile. **Das Prüfwerkzeug maß
etwas anderes als gemeint**, nicht der Fuzzer versagte.

### Was „grün" hier heißt

Stufe 2 von 4: **übersetzt und am PC gelaufen.** Nicht am Gerät. Kein
SE051C2 war beteiligt — der Chip ist bestellt und nicht geliefert.

---

## 6. Die Attrappe für den Sicherheitschip — und was an ihr offen bleibt

Der Chip signiert; er ist nicht da. Die Attrappe in `test/wallet/mocks.c`
ist gegen den **Wortlaut** von `apps/firmware/include/osk_se050.h`
gebaut, nicht gegen eine Erwartung. Sie rechnet echtes ECDSA (mit dem
Bignum- und Kurvencode dieses Projekts, der seinerseits von openssl
belegt ist, siehe oben) und hält den Vertrag ein: der Hash kommt fertig
herein, 32 Byte, DER geht heraus.

**Welche ihrer Annahmen am echten Chip offen bleiben** — eine Attrappe
prüft die eigene Seite des Vertrags, nie die andere:

1. **Der Zufallswert *k*.** Die Attrappe bildet ihn aus HMAC-SHA512 über
   Schlüssel und Hash, also deterministisch. Der echte SE051C2 nimmt
   seinen eigenen. Zwei Läufe am Gerät liefern verschiedene, gültige
   Signaturen. Jeder Test, der eine Signatur Byte für Byte vergleicht,
   wird am Gerät scheitern — und das ist richtig so, nicht ein Fehler des
   Geräts.
2. **Der monotone Zähler wird von der Geldbörse gar nicht benutzt.**
   `osk_se050.h` beschreibt `counter_create/read/raise`; die Geldbörse
   ruft keinen davon. Das ist genau die Form des Unglücks vom 29.10.2026:
   damals führte eine Attrappe einen Zähler anders als die echte
   Gegenseite, alles war grün, und am Gerät wäre nicht signiert worden.
   Hier ist es milder — der Zähler fehlt auf beiden Seiten —, aber wenn
   eine Richtlinie am echten Chip einen Zähler verlangt, schlägt das
   erst am Gerät auf. **Offen.**
3. **Schlüsselrichtlinien.** Die Attrappe nimmt jeden Import an.
   `osk_key_policy_t` kann am echten Chip einen Import ablehnen, den die
   Attrappe durchlässt.
4. **Der Weg des Schlüssels.** Die Geldbörse leitet mit BIP32 ab und
   **importiert** den fertigen Schlüssel in den Chip. Das heißt: er
   existiert kurz im RAM des nRF52840. Das widerspricht dem Satz
   „Schlüssel entstehen im Sicherheitschip und verlassen ihn nie" — für
   BIP32 lässt sich das nicht anders bauen, solange der Chip keine
   BIP32-Ableitung kann. Nach jedem Vorgang wird gelöscht
   (`osk_bip32_wipe`, `osk_wipe`), aber der Satz stimmt für die
   Geldbörse nicht, und das sollte in `docs/wallet.md` nicht anders
   stehen. **Zu entscheiden, nicht von mir.**
5. **Fehlerbilder und Zeiten.** Was der echte Chip bei voller
   Schlüsselliste, bei einem Abbruch mitten im Vorgang oder bei einer
   gestörten Leitung tut, weiß die Attrappe nicht.

---

## 7. Was die ANZEIGE leisten muss — der Vertrag

Die Anzeige selbst gehört dem UI-Strang; `wallet_confirm` fragt nur über
`osk_ui_confirm()`. Was dort ankommt und was damit passieren muss:

### Was geliefert wird

`osk_ask_req_t` mit `kind = OSK_ASK_SIGN`:

- **`subject`** — bei einer Adresse: die **ganze** Adresse in
  Vierergruppen, dahinter ` #` und acht Ziffern Prüfzahl. Bis zu
  **122 Zeichen** (90 Adresse + 22 Leerzeichen + 10 für Prüfzahl). Bei
  einer Nachricht (EIP-191): der Text wörtlich, bis 95 Zeichen.
- **`detail`** — `"<Betrag> + <Gebühr>"`, dahinter bei Bedarf
  `" / <Kette oder Art>"` und `" +<Zahl weiterer Empfänger>"`.
- **`asked_by`** — welches Programm fragt.
- **`timeout_ms`** — 60000.

### Was die Anzeige erfüllen muss

1. **`subject` darf nicht stillschweigend abgeschnitten werden.** Das
   ist die wichtigste Forderung dieses ganzen Papiers. Passt der Text
   nicht auf das Glas, muss die Anzeige **blättern** (Taster = weiter)
   oder laufen lassen. Ein abgeschnittener `subject` stellt genau den
   Angriff wieder her, den dieses Modul gerade geschlossen hat — und
   zwar unsichtbar, denn `wallet_confirm` erfährt nichts davon.
2. **Kann sie nicht blättern, muss sie `OSK_ERR_DENIED` geben**, nicht
   einen gekürzten Text anzeigen.
3. **Die Vierergruppen bleiben erhalten.** Umbrechen bitte an den
   Leerzeichen, nie mitten in einer Gruppe.
4. **Die Prüfzahl steht am Stück.** `#12345678` darf nicht über zwei
   Seiten gehen — dann vergleicht sie niemand.
5. **Zeichengenauigkeit.** Die Schrift muss `0`/`O`, `1`/`l`/`I` und
   `5`/`S` unterscheidbar zeigen. Bech32 lässt `1`, `b`, `i` und `o`
   absichtlich weg, Base58 und Hex nicht.
6. **Kein Durchklicken.** Die Freigabe soll nicht derselbe Tastendruck
   sein, der die letzte Seite umblättert.

### Was noch fehlt und nicht von mir kommen kann

**Die Meldungen sind nicht übersetzt.** `apps/firmware/lang/*.json` hat
überhaupt keinen `wallet`-Abschnitt. Alle Sprachschlüssel, die diese
Geldbörse an `osk_ui_status()` gibt, stehen in keiner Sprachdatei — am
Gerät erschiene die rohe Zeichenkette statt eines Satzes. Das untergräbt
die Forderung „mit einer Meldung, die sagt warum" unmittelbar.

(Das ist ein Zustand von vorher, nicht von diesem Strang; `check.py`
merkt es nicht, weil es nur Gleichheit zwischen den fünf Sprachen prüft
und keine Sprache diese Schlüssel hat.)

Gebraucht werden in **de, en, fr, it, es**:

| Schlüssel | wann |
|---|---|
| `wallet.nichts-lesbar` | nichts anzuzeigen |
| `wallet.adresse-nicht-anzeigbar` | Adresse passt nicht |
| `wallet.adresse-zu-lang` | über 90 Zeichen |
| `wallet.taproot-nicht-unterstuetzt` | Taproot-Eingang |
| `wallet.vertragsaufruf` | Vertragsaufruf mit Daten |
| `wallet.eip712-inhalt-unbekannt` | typisierte Nachricht |
| `wallet.nachricht-nicht-lesbar` | Steuerzeichen im Text |
| `wallet.betrag-nicht-darstellbar` | Betrag zu groß |
| `wallet.gebuehr-nicht-darstellbar` | Gebühr zu groß |
| `wallet.ausgang-ohne-adresse` | Ausgang ohne lesbare Adresse |

---

## 8. Offene Punkte

1. **Am Gerät ist nichts gelaufen.** Stufe 2 von 4. Der SE051C2 ist
   bestellt und nicht geliefert.
2. **Die Meldungen sind nicht übersetzt** (Abschnitt 7). Bis dahin zeigt
   das Gerät rohe Schlüssel.
3. **Die Anzeige kann nicht blättern.** `osk_ui_confirm()` nimmt einen
   `subject` und zeigt ihn einmal. Ohne Blättern passt eine 42-Zeichen-
   Adresse nicht auf 10,54 × 14,70 mm — der Vertrag aus Abschnitt 7 ist
   gestellt, aber nicht erfüllt. **Das ist der wichtigste offene Punkt:**
   bis er erfüllt ist, hängt die Lösung des Adressangriffs an einer
   Anzeige, die es noch nicht gibt.
4. **MetaMask kann kein EIP-712** über dieses Gerät (Abschnitt 3). Ein
   Befehl, der die Struktur statt ihres Hashes überträgt, wäre baubar.
   Wer in `apps/desktop` auf das Eigenschaftsbit baut: es ist immer 0,
   und eine Bitte darum beantwortet das Gerät mit `0x6A81`.
5. **Der Schlüssel liegt kurz im RAM** (Abschnitt 6, Punkt 4). Das
   widerspricht einer Zusage in `docs/wallet.md`, die ich nicht ändere.
6. **Der monotone Zähler wird nicht benutzt** (Abschnitt 6, Punkt 2).
7. **Solana und Cardano sind gestrichen**, Taproot abgelehnt. Nur
   Bitcoin und Ethereum.
8. **`GET_TRUSTED_INPUT` fehlt**: Electrum und Sparrow über PSBT-Datei,
   nicht als angestecktes Ledger-Gerät.
