# Nachquantensicher: ML-KEM-768 zusammen mit X25519

Stand 30.10.2026. Stufe **2 von 4**: geschrieben / **übersetzt** / im Abbild /
am Gerät gelaufen. Am Gerät ist nichts davon gelaufen.

Was es gibt: `apps/firmware/src/classes/pqc/` (Keccak, Rechenwerk, ML-KEM-768,
die Mischung), `apps/firmware/include/osk_pqc.h` als einzige Tür nach außen,
`apps/firmware/test/pqc/` als Prüfsatz. Was es nicht gibt: den Befehl 20
`kemagree` in der Firmware, das gespeicherte Verfahren je Schlüsselkennung und
die Paarungszahl — siehe `shared/protocol.md`.

---

## Die Regel, die alles andere bestimmt: hybrid oder gar nicht

ML-KEM läuft **zusammen mit** X25519, nicht an seiner Stelle.

Der Grund ist nicht Vorsicht gegen junge Mathematik. Er ist dieser: **X25519
kommt aus dem Sicherheitschip, ML-KEM aus dem Rechenkern.** Der geheime
X25519-Punkt entsteht im SE051C2 und verlässt ihn nie. ML-KEM kann der
Sicherheitschip nicht, also muss es der nRF52840 rechnen — und dessen
Auslesesperre ist mit Spannungs-Störimpulsen schon überwunden worden. Ein
reiner ML-KEM-Pfad hieße: der Schlüssel liegt im Rechenkern. Das wäre ein
**Rückschritt** gegenüber heute.

Daraus folgt beides:

- Fällt der ML-KEM-Teil — schlechte Mathematik, oder ein Fehler in dieser
  selbstgebauten Umsetzung —, bleibt X25519 aus dem Chip. Nichts ist schlechter
  als heute.
- Fällt X25519 — ein Quantenrechner —, bleibt ML-KEM.

**Gebaut ist deshalb kein Weg, auf dem ML-KEM allein ein Geheimnis trägt.**
Kein Schalter, keine Betriebsart, kein Prüfpfad. Wie das durchgesetzt wird,
steht weiter unten unter „Die eine Tür".

---

## Welche Größe, und was sie kostet

**Gewählt: ML-KEM-768.** Drei Gründe, in dieser Reihenfolge:

1. **Der Export trifft ohnehin auf 768.** `age` hat seit 2025 eine
   nachquantensichere Empfängerzeile, und die heißt `MLKEM768-X25519`
   (Empfänger beginnen mit `age1pq1`). Der CXP-Export und das Dateiwerkzeug
   müssen damit zusammenpassen; eine andere Größe hieße, zweimal zu bauen.
   Siehe `docs/datentraeger-konzept.md`.
2. **NIST-Stufe 3.** Das entspricht grob AES-192. ML-KEM-512 ist Stufe 1 und
   steht am unteren Rand dessen, was man für Schlüssel wählt, die zwanzig Jahre
   halten sollen.
3. **1024 kostet, ohne hier zu nützen.** Der Schlüsseltext wächst von 1088 auf
   1568 Byte, der Rechenaufwand um die Hälfte — und die Schwachstelle dieses
   Geräts ist nicht die Stufe des Gitterverfahrens, sondern der Rechenkern, in
   dem es läuft.

Die fünf Zahlen (K = 3, η₁ = η₂ = 2, d_u = 10, d_v = 4) stehen **einmal** im
Repo, in `apps/firmware/include/osk_pqc.h`. Alle Längen leiten sich daraus ab;
keine ist ein zweites Mal geschrieben.

### Längen

| | Byte | davon |
| --- | --- | --- |
| öffentlicher Schlüssel `ek` | 1216 | 32 X25519 + 1184 ML-KEM |
| geheimer Schlüssel `dk` | 2432 | 32 X25519 **öffentlich** + 2400 ML-KEM |
| Schlüsseltext `ct` | 1120 | 32 X25519 + 1088 ML-KEM |
| gemeinsames Geheimnis | 32 | |

Im `dk` steht vom X25519-Anteil nur der **öffentliche** Punkt. Der geheime
bleibt im Sicherheitschip; er steht in keinem Puffer dieses Moduls.

### Flash: gemessen 4,9 kB, geschätzt waren 11 kB

Gemessen am 30.10.2026 mit `arm-none-eabi-gcc 14.2.1` für Cortex-M4, `-Os`,
`-ffunction-sections -fdata-sections`. Nachzumessen:

```bash
sh apps/firmware/test/pqc/budget.sh
```

| Datei | Flash (Byte) | fester RAM |
| --- | --- | --- |
| `pqc_keccak.o` | 1251 | 0 |
| `pqc_poly.o` | 1532 | 0 |
| `pqc_mlkem.o` | 1676 | 0 |
| `pqc_hybrid.o` | 484 | 0 |
| **zusammen** | **4943** | **0** |

**Die Schätzung im Plan hält, mit Luft.** 4,9 kB sind 45 % der geschätzten
11 kB. Was in dieser Zahl *nicht* steckt: der Befehl 20 in der Firmware (nicht
gebaut), der X25519-Anteil (der liegt im Chip, nicht im Flash), und was der
Binder im Gesamtabbild noch dazulegt. Die Zahl ist der Modulanteil, und der ist
vollständig.

Fester RAM: **0 Byte**. Keine `.data`, keine `.bss`, kein dynamischer Speicher.
Alles läuft auf dem Stapel oder in Puffern des Aufrufers.

### RAM: gemessen 11,2 kB Stapel, im tiefsten Weg

Aus `-fstack-usage`, summiert entlang des tiefsten Wegs. Der tiefste ist das
Entkapseln, weil es den Schlüsseltext zur Prüfung noch einmal verschlüsselt:

```
osk_pqc_decaps          80
osk_mlkem_decaps      4232   (Schlüsseltext zum Vergleichen, 1088 Byte)
kpke_encrypt          6328   (drei Vektoren à 1536 Byte)
osk_poly_sample_ntt    240
osk_keccak_squeeze      24
permute                344
                    ------
                     11248
```

Entschlüsseln und Verschlüsseln ruft `osk_mlkem_decaps` **nacheinander**, nicht
ineinander — sonst wären es 4 kB mehr. Die anderen Wege sind flacher:
Schlüssel erzeugen 6,0 kB, Kapseln 7,2 kB.

Bei 256 kB RAM im nRF52840 ist das tragbar, aber es ist kein Betrag, den man
nebenbei auf einem Arbeitsstapel unterbringt: **der Aufrufer braucht einen
Stapel von mindestens 12 kB**, und das muss beim Einbau in die Firmware
eingestellt werden. Das ist der erste offene Punkt unten.

### Zeit: nicht gemessen, wo es zählt

Am PC (Ryzen Threadripper 3990X, `gcc -O2`): Schlüssel erzeugen 0,218 ms,
kapseln 0,196 ms, entkapseln 0,241 ms.

**Das sagt über das Gerät nichts Belastbares.** Der nRF52840 läuft mit 64 MHz
gegen knapp 4 GHz, und bei dieser Art Ganzzahlrechnung kommt der Unterschied in
der Rechenwerksbreite noch dazu. Grob zwei Größenordnungen heißt: 40 bis 80 ms
fürs Entkapseln. **Die Schätzung von 55 ms liegt darin — bestätigt ist sie
nicht.** Das braucht das Gerät.

Falls es zu langsam wird, ist der Hebel bekannt und liegt an einer Stelle:
diese Umsetzung rechnet modulo q mit `%` auf eine Konstante (der Übersetzer
macht daraus eine Multiplikation mit Schieben, nicht eine Division — die
Laufzeit hängt also nicht am Wert). Eine Montgomery-Reduktion wäre zwei bis
drei Mal schneller und sitzt in genau drei Funktionen in `pqc_poly.c`.

---

## Die Zusammenführung beider Geheimnisse

Hier entscheidet sich, ob „hybrid" wirklich hybrid ist.

```
ss = SHA3-256( Kennung ‖ ss_x ‖ ss_m ‖ ct_x ‖ ct_m ‖ pk_x ‖ ek_m )
```

mit `ss_x` dem rohen X25519-Ergebnis aus dem Chip, `ss_m` dem Geheimnis aus
ML-KEM, den beiden Schlüsseltexten und den beiden öffentlichen Schlüsseln. Die
Kennung ist `openSecureKey hybrid-kem v1 ML-KEM-768+X25519`.

**Warum ein Angreifer, der einen Anteil bricht, nichts gewinnt.** Beide Anteile
gehen durch *einen* Hash, nicht getrennt. Wer `ss_x` kennt und `ss_m` nicht
(oder umgekehrt), hat 256 unbekannte Bit im Vorbild von SHA3-256 stehen. Es gibt
keinen Rechenweg, der das Ergebnis aus einem Anteil allein annähert — anders als
bei einem XOR oder einer Addition der beiden fertigen Schlüssel, wo sich der
bekannte Anteil herausrechnen lässt. Das Ergebnis bleibt also sicher, solange
**einer** von beiden hielt.

**Warum die Schlüsseltexte mit hineingehen.** Ohne diese Bindung sind Angriffe
bekannt, bei denen ein gültiges Geheimnis in einen anderen Zusammenhang
getragen wird, obwohl beide Verfahren halten: der Angreifer tauscht unterwegs
einen der beiden Anteile aus und bekommt zwei Seiten, die dasselbe Geheimnis für
verschiedene Gespräche halten. Mit dem ganzen Gespräch im Hash ergibt jeder
Austausch auf den beiden Seiten verschiedene Ergebnisse.

**Warum die Kennung vorn steht.** Dieselben zwei Anteile für eine andere Aufgabe
ergeben mit einer anderen Kennung ein anderes Geheimnis. Ohne sie passt ein
Geheimnis aus einem Zusammenhang in den anderen.

### Die eine Tür

`apps/firmware/include/osk_pqc.h` ist der einzige öffentliche Kopf des Moduls,
und er hat drei Funktionen: `osk_pqc_keypair`, `osk_pqc_encaps`,
`osk_pqc_decaps`. Alle drei verlangen den X25519-Anteil. Ist er lauter Nullen —
was bei X25519 heißt, dass die Gegenseite einen Punkt kleiner Ordnung geschickt
hat, also genau der Angriff, der den Chip-Anteil wegräumen will —, brechen sie
mit `OSK_ERR_ARG` ab.

Der blanke Mechanismus liegt in `src/classes/pqc/pqc_mlkem.h`, **nicht** in
`include/`, und dieser Kopf besteht auf `OSK_PQC_INTERNAL`. Das ist keine Sperre
gegen Böswillige, sondern ein Stolperdraht gegen die bequeme Abkürzung: „nur für
den Test" ist der Weg, auf dem solche Pfade entstehen und dann bleiben. Einbinden
darf ihn zweierlei: die Mischung selbst, und der Prüfsatz — der muss an die
blanken Werte heran, weil sonst nicht gegen die amtlichen Testwerte geprüft
werden kann.

Geprüft wird das auch: der Prüffall *„ohne den Anteil aus dem Chip entsteht kein
Geheimnis"* ruft alle drei Funktionen mit einem genullten X25519-Anteil und
erwartet jedes Mal eine Abweisung. Der Fall *„beide Anteile gehen in das
Ergebnis ein"* verbiegt jedes einzelne der 32 Byte beider Anteile und prüft, dass
sich das Ergebnis jedes Mal ändert. Fällt einer der beiden Anteile später aus der
Mischung heraus, fällt dieser Prüffall durch.

---

## Was geprüft ist — und was dieser Beleg wert ist

**Eine selbstgebaute Umsetzung, die nur gegen sich selbst geprüft ist, beweist
nichts.** Ein Rundlauf „gekapselt und wieder entkapselt" geht auch mit einer
falschen Umsetzung auf, solange beide Seiten denselben Fehler machen. Darum
steht hier, woher die Werte kommen.

### Die amtlichen Testwerte sind da

FIPS 203 hat Testwerte, und das NIST veröffentlicht sie im Repo
`usnistgov/ACVP-Server` unter `gen-val/json-files/`. Geholt am 30.10.2026:

- `ML-KEM-keyGen-FIPS203/internalProjection.json`
- `ML-KEM-encapDecap-FIPS203/internalProjection.json`

Daraus macht `tools/mkkat_pqc.py` den Kopf
`apps/firmware/test/pqc/kat_mlkem768.h`. Die Rohdateien liegen nicht im Repo —
zusammen 2,6 MB —, das Erzeugen ist mit dem Werkzeug jederzeit nachvollziehbar.

| geprüft gegen amtliche Werte | Fälle |
| --- | --- |
| Schlüssel erzeugen (`d`, `z` → `ek`, `dk`) | 25 |
| Kapseln (`ek`, `m` → `ct`, `K`) | 3 |
| Entkapseln, gültiger Schlüsseltext | 2 |
| Entkapseln, **verfälschter** Schlüsseltext → Ersatzgeheimnis | 2 |

Bei der Schlüsselerzeugung steht der SHA3-256 über dem amtlichen Ergebnis statt
des Ergebnisses selbst — ein `dk` hat 2400 Byte, und Hash gegen Hash ist genauso
scharf. Wo das amtliche Ergebnis Eingabe ist, steht es vollständig da.

Die vier Entkapsel-Fälle sind der schärfste Teil. Bei den zwei verfälschten ist
der amtliche Sollwert **nicht** ein Fehler, sondern das Ersatzgeheimnis aus `z`
(stille Verwerfung). Dass dieser Wert stimmt, hängt an `z`, am ganzen
Schlüsseltext und an der Reihenfolge, in der beides in SHAKE256 geht — eine
Umsetzung, die dort irgendwo daneben liegt, fällt durch.

### Die Gegenprobe für Keccak

ML-KEM ist zu einem guten Teil Keccak. Die SHA3- und SHAKE-Werte im Prüfkopf
kommen aus `hashlib` (CPython) — auch eine fremde Umsetzung und damit eine echte
Gegenprobe, nur keine amtliche. Die Längen sind um die Blockgrößen herum gewählt
(135, 136, 137 bei einer Rate von 136): dort sitzen die Fehler im Abschluss.

### Die Gegenprobe für die NTT

Die Zahlentransformation wird gegen eine Multiplikation Zeile für Zeile geprüft,
so wie man sie in der Schule rechnet — ein völlig anderer Rechenweg zum selben
Ergebnis. Ein Fehler in der Tabelle, in den Drehweiten oder im Faktor am Ende
fällt dort auf, während ein Hin-und-zurück-Lauf ihn verschluckt, weil er sich
selbst aufhebt.

### Der Lauf

```powershell
wsl -e make -C apps/firmware/test/pqc
```

**582 Fälle, 0 Fehler** (30.10.2026, gcc 14.2.0, mit Address- und
Undefined-Behaviour-Sanitizer). Das heißt: **übersetzt und am PC ausgeführt.**
Es heißt nicht: am Gerät gelaufen.

### Was dieser Beleg nicht deckt

- **Kein Gerät.** Weder Flash-Zahl im echten Abbild noch Laufzeit noch
  Zusammenspiel mit dem SE051C2.
- **X25519 ist hier nicht geprüft.** Im Prüfsatz stehen an seiner Stelle feste
  32 Byte. Ob der SE051C2 das **rohe** ECDH-Ergebnis überhaupt herausgibt — und
  nicht nur ein schon abgeleitetes —, ist eine offene Frage am Gerät. Dieselbe
  Frage steht im Plan beim Dateiwerkzeug. Gibt der Chip nur Abgeleitetes heraus,
  muss die Mischung an dieser Stelle anders geschnitten werden.
- **Zeitkonstanz ist begründet, nicht gemessen.** Alle Rechenschritte hängen an
  Längen statt an Werten, der Vergleich der Schlüsseltexte sieht immer alle 1088
  Byte an, die Auswahl zwischen echtem und Ersatzgeheimnis läuft ohne `if`, und
  modulo q ist eine Multiplikation mit Schieben. Gemessen ist das nicht. Genau
  diese Fehlerklasse hat 2024 den Marktführer getroffen: EUCLEAK — jeder
  YubiKey 5 vor Firmware 5.7 war klonbar, weil eine Inversion nicht zeitkonstant
  war. Eine Messung am Gerät gehört in den Prüfstand.
- **Kein Fuzzing.** Der Schlüsseltext und der fremde öffentliche Schlüssel
  kommen vom Draht. Der Prüfsatz verbiegt sie an 30 Stellen; ein Fuzzer wäre
  mehr.
- **Keine fremde Begutachtung.** Selbstgebaute Gitterkryptographie, geprüft
  gegen amtliche Werte: das heißt „rechnet dasselbe wie die Prüfstelle des
  NIST". Es heißt nicht „widersteht einem Seitenkanalangriff".

---

## Offene Punkte

1. **Stapel einstellen.** Der Aufrufer braucht mindestens 12 kB. Wer das
   Modul in die Firmware einbaut, muss das im Kconfig setzen — sonst läuft der
   Stapel über, und das sieht aus wie ein Fehler irgendwo anders.
2. **Befehl 20 `kemagree` bauen**, mit dem gespeicherten Verfahren je
   Schlüsselkennung und der Paarungszahl über das Gespräch. Ohne das dritte
   Stück ist die Aushandlung von außen beeinflussbar — siehe
   `shared/protocol.md`.
3. **Am Gerät messen:** Laufzeit, Flash im Abbild, und ob der SE051C2 das rohe
   ECDH-Ergebnis herausgibt.
4. **Zeitkonstanz messen**, nicht nur begründen.
5. **CXP und `age`**: die Empfängerzeile `MLKEM768-X25519` benutzt eine andere
   Mischung als die hier (X-Wing). Für das Dateiformat muss deren Mischung
   gebaut werden, nicht diese — die Größe passt, die Zusammenführung nicht. Das
   ist eine eigene Arbeit und bewusst nicht hier mitgebaut.
