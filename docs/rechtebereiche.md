# Rechtebereiche, Zugangsschluessel, Zugangsdaten, Zeitstempel

**Strang F, 29.10.2026.** Die Schicht liegt in
`apps/firmware/src/classes/authz/`, ihr Vertrag in
`apps/firmware/include/osk_authz.h`.

**Stufe: 2 von 4** — geschrieben / **uebersetzt** / im Abbild / am Geraet
gelaufen. Nichts davon ist auf Hardware gelaufen. Was der Prueflauf
gemessen hat und was nicht, steht in Abschnitt 6.

---

## 1. Warum die Schicht `authz` heisst

Sie antwortet auf zwei Fragen, und zwar vor jedem Vorgang:

1. **Wer fragt?** — Zugangsschluessel, identitaetsbasiert
2. **Darf der das?** — Rechtebereich und Faehigkeit

Beides zusammen heisst Autorisierung, und `authz` ist die uebliche kurze
Schreibweise. Die vorhandenen Ordner heissen `se050`, `ctap`, `applets`,
`store`, `transport`, `ui`, `wallet` — alle nach dem, was sie tun, nicht
nach einem Strang. `authz` passt dazu.

Der Zeitstempel liegt in derselben Schicht, obwohl er zunaechst nicht
dazuzugehoeren scheint. Grund: er beantwortet die dritte Frage derselben
Art — **wann und in welcher Reihenfolge** ist etwas geschehen, und auf
wessen Veranlassung. Die unterschriebene Erklaerung in `authz_tsa.c`
fuehrt die Identitaet aus Teil 2 mit; ohne sie waere sie eine Unterschrift
ohne Urheber. Eine eigene Schicht fuer eine Datei waere teurer als der
Gewinn.

### Was NICHT „domain" heisst, und warum das wichtig ist

Der Plan nennt sie Rechtebereiche, die Vorlage am Markt nennt sie Domains.
Im Code heissen sie **`realm`**.

**Das Wort `domain` ist in diesem Baum schon zweimal belegt, fuer etwas
voellig anderes:** `attest_domain` in
`src/classes/applets/guard/osk_guard_msg.c` und `domain[32]` in
`src/classes/wallet/wallet_eth.c` sind Trennzeichenketten einer Signatur
(Domain Separation) — sie sorgen dafuer, dass eine Unterschrift aus einem
Zusammenhang in einem anderen nicht gilt. Das hat mit Rechten nichts zu
tun.

Zwei Groessen, die gleich heissen und Verschiedenes messen, ist das
durchgehende Fehlermuster dieses Projekts (`docs/plan.md` Abschnitt 6 zaehlt
sechs Faelle auf). Hier waere es der siebte geworden, und zwar in der
Schicht, die ueber jede Freigabe entscheidet. Darum `realm`.

## 2. Die sechzehn Bereiche

Eine Maske `uint16_t`, Bits 0 bis 15. Jeder Gegenstand (Schluessel,
Zugangsdaten) traegt eine, jeder Zugangsschluessel traegt eine.

**Die Regel, die trennt:** gearbeitet werden darf, wo sich beide Masken
**ueberschneiden**.

```
Zugangsschluessel "Zertifikatsdienst"   Bereiche: 0000 0000 0000 0010
Schluessel der Geldboerse               Bereiche: 0000 0000 0010 0000
Ueberschneidung                                   0000 0000 0000 0000  -> nein
```

Beide duerfen `OSK_CAP_SIGN`. **Die Faehigkeit allein trennt also nicht** —
es trennt der Bereich. Genau das ist der Punkt aus dem Auftrag: der Dienst,
der Server-Zertifikate ausstellt, kommt nicht an die privaten Schluessel
der Geldboerse.

**Eine leere Maske heisst „nirgends", nicht „ueberall".** Ein Gegenstand,
dessen Bereich noch nicht gesetzt ist, ist fuer niemanden erreichbar — auch
nicht fuer den Verwalter. Die umgekehrte Lesart waere der stille
Rechteverlust: ein neu angelegter Schluessel stuende offen, bis jemand
daran denkt.

## 3. Mehrere Zugangsschluessel — und warum identitaetsbasiert

**FIPS 140-3 verlangt ab Stufe 3 identitaetsbasierte Authentifizierung,
nicht rollenbasierte** (ISO/IEC 19790 Abschnitt 7.4.3). Der Unterschied
liegt nicht darin, wie streng geprueft wird, sondern darin, **was hinterher
im Protokoll steht**: bei rollenbasierter Pruefung „ein Verwalter", bei
identitaetsbasierter, *welcher*.

Das ist eine Bauaufgabe und keine Formalie, und zwar eine, die sich nicht
nachtraeglich einziehen laesst: jeder Vorgang muss eine Identitaet
mitfuehren, und das sind alle Aufrufwege. Heute hat das Geraet eine PIN —
und wer sie kennt, ist nicht von jedem anderen zu unterscheiden, der sie
kennt.

**Anmeldung, zwei Schritte, gegenseitig:**

```
1. begin(id, host_challenge)        -> device_challenge
2. Rechner: HMAC(K, "osk-authz-host-1" || dev_chal || host_chal)
3. authenticate(session, proof)     -> HMAC(K, "osk-authz-dev-1" || ...)
```

Drei Entscheidungen, jede mit einem Grund:

| | Grund |
| --- | --- |
| **Beide Herausforderungen** gehen in den Beweis | die des Geraets verhindert, dass ein abgehoerter Beweis noch einmal gilt; die des Rechners verhindert, dass ein untergeschobenes Geraet den Rechner auf eine Herausforderung festnagelt, zu der es den Beweis schon kennt. Eine von beiden allein reicht nicht |
| **Zwei verschiedene Trennzeichenketten** | sonst geht der Beweis des Rechners als Beweis des Geraets durch, und ein untergeschobenes Geraet braeuchte das Geheimnis nicht |
| **Das Geheimnis liegt im Chip** | `osk_se050_hmac()`. Es verlaesst ihn nie; die Schicht im Rechenkern sieht es nicht |

**Die Weitergabe-Schranke.** Jeder Zugangsschluessel hat zwei Masken:
`caps` (was er darf) und `delegated` (was er weitergeben darf, wenn er
andere anlegt). Ohne die zweite legt sich jeder, der verwalten darf, einen
zweiten Schluessel mit mehr Rechten an und die Rechtetrennung ist eine
Hoeflichkeit. Mit ihr schrumpft die Rechtemenge ueber jede Kette hinweg und
waechst nie — auch nicht ueber den Umweg, dem Neuen das Recht
weiterzugeben, was man selbst nicht weitergeben darf.

## 4. Zugangsdaten im Chip (Teil 3)

**Wofuer:** `docs/plan.md`, Abschnitt „Dongle und Server (NGC)" —
„Knotenschluessel im Chip statt auf der Platte". Ein Server soll seinen
Schluessel nicht im Dateisystem liegen lassen.

**Was den Chip verlaesst, ohne Beschoenigung:**

| | verlaesst den Chip |
| --- | --- |
| Langzeitgeheimnis (HMAC-Schluessel) | **nie** |
| Sitzungsschluessel (enc, mac, je 16 Byte) | **ja — das ist ihr Zweck** |

Der Gewinn liegt nicht darin, dass nichts herauskommt. Er liegt darin, dass
das **Langzeitgeheimnis** nicht herauskommt: ein gestohlener Server gibt
nichts her, was ueber seine laufende Sitzung hinausreicht. Wer die
Sitzungsschluessel abhoert, hat sie bis zum Ende der Sitzung.

**Ableitung:** SP 800-108 Zaehlermodus, PRF HMAC-SHA256 aus dem Chip.
Zwei Blocke mit verschiedenem Zaehler fuer die zwei Schluessel; beide aus
einem Block zu schneiden waere kuerzer und falsch, weil dann der eine aus
dem anderen zu errechnen ist.

**Das Passwort wird geprueft, nicht nur in die Ableitung gegeben.** Gaebe
es nur die Ableitung, kaeme bei falschem Passwort ein falscher
Sitzungsschluessel heraus — und der Versuch waere unbegrenzt wiederholbar,
kein Fehlversuchszaehler griffe, das Passwort waere in Ruhe zu erraten.
Darum liegt neben den Zugangsdaten ein auf 16 Byte gekuerzter
`HMAC(secret, "osk-authz-pw-verify-1" || password)`.

> **Abwaegung, die der Master kennen muss:** Die Vorlage am Markt leitet mit
> **AES-CMAC** nach SCP03 ab. Hier steht **HMAC-SHA256**, weil der Vertrag
> `osk_se050.h` `osk_se050_hmac()` fuehrt, aber kein CMAC, und diese Schicht
> kein eigenes Krypto-Grundverfahren mitbringen soll. **Folge: fremde
> Programme, die das Protokoll der Vorlage sprechen, passen nicht.** Fuer
> NGC und `osk-guard` ist das ohne Belang — das ist unsere eigene Software
> auf beiden Seiten. Wer Fremdvertraeglichkeit will, braucht CMAC im
> Vertrag.

## 5. Zeitstempel (Teil 4)

**Der Befund steht in `docs/zeitstempel.md` und wurde vor der Umsetzung
geschrieben.** Kurz:

Das Geraet hat keinen Akku, seine Uhr wird beim Einstecken vom Rechner
gestellt, und eine falsche Rechneruhr setzt eine falsche Geraetezeit
(`docs/architecture.md`). Ein Zeitstempel aus dieser Uhr beweist genau so
viel wie der Rechner, gegen den er beweisen soll. **Darum ist das Geraet
Antragsteller, nie Zeitstempelstelle.** Es gibt keine Funktion, die aus
der Geraetezeit eine Zeitangabe macht.

Drei Beweise, die einzeln etwas sagen:

| | beweist | haengt ab von |
| --- | --- | --- |
| Token einer fremden Stelle | Zeitpunkt | Vertrauen in die Stelle, Netz |
| Gegenzeichnung des Geraets | dieses Geraet hat es gesehen | Geraeteschluessel im Chip |
| monotoner Chipzaehler | **lueckenlose Reihenfolge** | nichts aussen |

**Der Chipzaehler ist der staerkste der drei**, weil er ohne jede fremde
Stelle gilt. Er beweist keine Uhrzeit — „vorher" ja, „am Dienstag" nein.

**Die Geraetezeit steht ausdruecklich NICHT in der unterschriebenen
Erklaerung.** Sie beginnt bei jedem Einstecken bei null; unterschrieben
wuerde sie wie eine Zeitangabe aussehen, ohne eine zu sein. Sie wird
gemeldet, damit ein Protokoll sie daneben legen kann, und sie heisst im
Vertrag `device_ms`, nicht `time`.

**Was die Firmware nicht prueft:** die Signatur des Tokens. Dafuer
braeuchte sie CMS (RFC 5652), eine X.509-Kettenpruefung und einen
Vertrauensspeicher — zusammen deutlich mehr Flash als der Rest dieser
Schicht, und die Flash-Rechnung ist schon offener Punkt 2 im Plan. Geprueft
wird die **Form**: `status`, Aufbau, Laengen, Inhaltstyp, der Hash im Token
und die Nonce. Die Funktion heisst darum `osk_tsa_response_parse()` und
nicht `_verify()`.

**Arbeitsregel eingehalten:** die technischen Anforderungen von RFC 3161
sind gebaut. **Keine Zeitstempelstelle ist beantragt** — das ist der
bezahlte Teil, und der wird geschoben.

## 6. Wie geprueft wurde, und was der Lauf gemessen hat

**`python tools/check.py` ist kein Beleg fuer diese Schicht.** Der Lauf
meldet `firmware/tests` als **uebersprungen** (kein cmake, kein make unter
Windows) — er fuehrt die C-Pruefungen also nicht aus. Was er fuer diese
Schicht wirklich gemessen hat, ist genau eine Sache, und die ist nicht
wenig: `firmware/symbole` hat 93 Dateien und 605 globale Namen gelesen und
**keine Kollision** gefunden. Die Schicht traegt sich also ins Abbild ein,
ohne einen anderen Teil zu verdraengen — genau der Fehler, der in diesem
Projekt bei `osk_wipe()` das Binden gebrochen hat.

**Der Weg, auf dem die Pruefungen wirklich laufen**, ist
`apps/firmware/test/authz/build.ps1` mit den Visual-Studio-Build-Tools,
nach dem Vorbild von `test/se050/build.ps1`:

```
authz: 255 Faelle, 0 Fehler
GRUEN
```

Uebersetzt mit `/W4 /WX` — jede Warnung ist ein Fehler. Keine
Fremdbibliothek: SHA-256 und HMAC kommen aus
`src/classes/ctap/osk_crypto.c`, also aus eigenem Code. Daneben liegt ein
`Makefile` fuer gcc mit Address- und UB-Sanitizer; **dieser Weg ist hier
nicht gelaufen**, weil auf diesem Rechner kein gcc ist. Beide Dateilisten
stehen in derselben Reihenfolge, damit ein Vergleich mit dem Auge moeglich
ist — das war der Fehler, an dem `test/se050` gestolpert ist.

### Der Lauf ist gegengeprueft worden

Ein gruener Lauf sagt fuer sich genommen nichts: er koennte gruen sein,
weil nichts geprueft wird. Darum sind **zwei absichtliche Fehler** in die
Schicht gesetzt und der Lauf wiederholt worden:

| absichtlicher Fehler | Ergebnis |
| --- | --- |
| Die Bereichs-Ueberschneidung in `osk_authz_check()` entfernt | **ROT**, 3 Fehler |
| `authz_equal_ct()` meldet immer „gleich" | **ROT**, 23 Fehler |

Danach beide zurueckgenommen, Lauf wieder gruen bei 255 Faellen. Erst damit
ist belegt, dass der Lauf die beiden Dinge wirklich misst, auf die es
ankommt.

### Die Rechtetrennung ist dort geprueft, wo sie zaehlt

`test_realm.c` geht ausdruecklich die **verbotenen** Wege:

- Zertifikatsdienst versucht `SIGN` im Bereich der Geldboerse → abgewiesen
- und umgekehrt genauso
- Faehigkeit vorhanden, Bereich nicht → abgewiesen
- mehrere verlangte Rechte, eines fehlt → abgewiesen (nicht „eines genuegt")
- begonnene, aber nicht bewiesene Sitzung → abgewiesen
- leere Bereichsmaske → abgewiesen, auch fuer den Verwalter

`test_keys.c` geht die Wege, auf denen eine Trennung von innen aufgeht:
Aufstieg in einem Schritt, Aufstieg ueber eine Zwischenstation, Zugang in
einem fremden Bereich einrichten, abgehoerten Beweis wiederverwenden,
sich selbst entfernen.

`test_cred.c` prueft ausserdem, dass ein Unberechtigter mit dem **richtigen
Passwort** die Fehlversuche fremder Zugangsdaten **nicht** hochzaehlt —
sonst waere eine Betriebsstoerung auf Zuruf moeglich.

`test_der.c` und `test_tsa.c` bearbeiten den Parser: abgeschnittene
Eingaben in jeder Laenge, unbestimmte Laenge, nicht minimal kodierte
Laengen und Zahlen, jedes einzelne Byte der Antwort verbogen. Der wichtigste
Fall dort ist `ein echter Zeitstempel ueber FREMDE Daten gilt nicht` — der
eigentliche Angriff auf einen Zeitstempel, und er kommt ohne jede
Faelschung aus.

### Was der Lauf NICHT gemessen hat

1. **Ob ein echter SE051 dieselben HMACs und Zaehlerstaende liefert.** Hier
   antwortet `test/authz/fake_se050.c`. Das ist Sache von Strang A und am
   Geraet zu messen.
2. **Ob eine echte Zeitstempelstelle Antworten dieser Form schickt.** Die
   Feldreihenfolge stammt aus RFC 3161 Abschnitt 2.4.2 und RFC 5652
   Abschnitt 5, nicht aus einer gemessenen Antwort. *(Beim ersten Anlauf
   fehlte im Prueffall die `[0]`-Huelle um `eContent` — der Leser hat es
   gemeldet, und das ist ein Hinweis darauf, wie leicht sich hier eine
   falsche Form einschleicht.)*
3. **Nichts im Abbild und nichts am Geraet.** Der ARM-Bau braucht das nRF
   Connect SDK; es ist auf diesem Rechner nicht da.
4. **`clang-format` ist nicht installiert** — `check.py` meldet
   `firmware/format` als uebersprungen. Der Stil ist nach `.clang-format`
   von Hand geschrieben, aber nicht nachgemessen.
5. **Der Stapelbedarf.** Die Puffer dieser Schicht sind klein (groesster:
   `uint8_t scratch[512]` im Prueffall, in der Schicht selbst unter
   128 Byte), aber gemessen ist nichts. Der offene Punkt 4a des Plans
   betrifft den SE050-Treiber; diese Schicht ruft ihn und erbt den Bedarf.
6. **Der Sanitizer-Lauf** (gcc-Weg) — kein gcc auf diesem Rechner.

## 7. Was offen blieb

1. **Die Tabellen liegen im RAM.** Zugangsschluessel und Zugangsdaten sind
   nach dem Abziehen weg und muessen beim Einstecken neu eingetragen
   werden. Wo sie dauerhaft liegen, ist eine Entscheidung, die den
   Tresor-Strang beruehrt — `docs/dongle-firmware.md` hat denselben
   offenen Punkt fuer seine Label-Tabelle, und es ist derselbe. **Hier
   nicht entschieden, weil es sonst zwei Antworten gaebe.**
2. **`osk_sha256()` liegt in `ctap/`, gehoert aber nach `include/`.** Diese
   Schicht braucht es und musste in den Ordner eines anderen Strangs
   greifen. Verschoben wurde nichts — das waere ein Eingriff in Strang B.
   **Dem Master gemeldet.** Eine zweite SHA-256 zu schreiben waere der
   Fehler, der bei `osk_wipe()` schon einmal das Binden gebrochen hat.
3. **Die Zahl der Bereiche steht an einer Stelle**
   (`OSK_AUTHZ_REALM_COUNT`), die Breite der Maske an einer zweiten
   (`uint16_t`). Beide muessen zusammenpassen, und nichts im Uebersetzer
   erzwingt das; ein Prueffall vergleicht sie
   (`sizeof(osk_realm_mask_t) == 2`). Das ist eine Kruecke, kein
   `_Static_assert` — letzteres waere besser und steht als Kleinigkeit
   offen.
4. **Der Passwort-Pruefwert ist ein Woerterbuch-Ziel** fuer jemanden, der
   den Speicher des Geraets lesen kann. Eine langsame Ableitung am Rechner
   davor waere die Gegenmassnahme; die gehoert zum PIN-Weg
   (PIN → langsame Ableitung → KEK → DEK, `docs/plan.md`) und damit nicht
   hierher. **Nicht gebaut, bewusst.**
5. **Tastendruck.** `needs_touch` steht im Vertrag und wird gespeichert,
   aber von dieser Schicht **nicht erzwungen** — der Taster gehoert der
   UI-Schicht, und ein Aufruf dorthin waere eine Abhaengigkeit, die diese
   Schicht nicht haben soll. Wer sie auswertet, ist offen.
6. **Kein Anschluss an das Geraeteprotokoll.** `osk_log_add()` gehoert dem
   Tresor-Strang. Die Schicht liefert mit
   `osk_authz_session_identity()` genau das, was ein Protokolleintrag
   braucht — eingetragen wird dort, nicht hier.
