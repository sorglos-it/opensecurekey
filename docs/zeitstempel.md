# Zeitstempel: was ein RFC-3161-Zeitstempel an diesem Geraet beweist

**Befund vom 29.10.2026, Strang F. Geschrieben VOR der Umsetzung**, weil die
Antwort entscheidet, was gebaut wird. Stufe: Befund aus Unterlagen, keine
Messung am Geraet.

---

## 1. Die Frage

Der Plan nennt unter "Signaturen" den **RFC-3161-Zeitstempel** (`docs/plan.md`,
Abschnitt "Verschluesseln, signieren, Zertifikate"). RFC 3161 beschreibt zwei
Rollen:

- die **Time-Stamping Authority (TSA)**, die einen Zeitstempel **ausstellt**
  (RFC 3161 Abschnitt 2.1),
- den **Requester**, der einen `TimeStampReq` schickt und einen
  `TimeStampResp` zurueckbekommt (Abschnitt 2.4.1 und 2.4.2).

Die Frage ist, welche dieser Rollen dieses Geraet einnehmen kann.

## 2. Die Bedingung, die alles entscheidet

**Das Geraet hat keinen Akku** (entschieden am 25.10.2026, `docs/plan.md`
Abschnitt "Kein Akku"). Daraus folgt in den eigenen Unterlagen dreimal
dasselbe, aus drei verschiedenen Richtungen:

| Belegstelle | Aussage |
| --- | --- |
| `docs/architecture.md`, Abschnitt zu den Gruenden fuer den Akkuausbau | "the clock is set from the computer when the stick is plugged in" und "a wrong computer clock sets a wrong device clock" |
| `docs/dongle-firmware.md`, "Second open point" | die Geraeteuhr zaehlt ab dem Einstecken von null; wer UTC liefert, ist **nicht entschieden** |
| `docs/stromaufnahme.md`, "Warum die Uhr keine Dauerversorgung braucht" | Zeitstempel bei Signaturen holen die Zeit von einem **Zeitstempel-Server** |

Die dritte Zeile ist bemerkenswert: **die Antwort stand schon im Repo**, in
einem Dokument ueber Stromaufnahme. Sie war nur nirgends als Entscheidung
ueber RFC 3161 aufgeschrieben.

## 3. Der Befund

**Ein Zeitstempel, den dieses Geraet aus seiner eigenen Uhr bildet, beweist
nicht mehr als die Uhr des Rechners, an dem es steckt.** Und die Uhr des
Rechners ist genau die Groesse, gegen die ein Zeitstempel schuetzen soll: wer
eine Signatur auf ein falsches Datum legen will, stellt vorher seine
Rechneruhr.

Die Kette ist kurz und hat genau eine schwache Stelle:

```
Rechneruhr  --(beim Einstecken)-->  Geraeteuhr  -->  Zeitangabe in der Signatur
    ^
    hier greift der Angreifer an, und danach ist jede Stufe korrekt
```

Das ist kein Fehler in der Umsetzung, den man beheben koennte. Es ist eine
Eigenschaft der Hardware.

**Daraus folgt: dieses Geraet kann keine TSA sein.** RFC 3161 Abschnitt 2.1
verlangt von einer TSA ausdruecklich, dass sie "a trustworthy source of time"
benutzt. Eine Uhr, die der Antragsteller selbst stellt, ist die
unvertrauenswuerdigste denkbare Quelle - der Antragsteller ist der Angreifer.

> **Befund und Begruendung getrennt:** Der Befund ist, dass das Geraet keine
> TSA sein kann. Er faellt nicht, wenn jemand spaeter doch eine Uhr einbaut -
> dann faellt nur die Begruendung, und der Befund muss neu geprueft werden.

## 4. Was statt dessen gebaut wird - drei Teile, die einzeln etwas beweisen

Die Rolle, die dem Geraet bleibt, ist die des **Requesters**, und sie ist
nicht wenig wert. Dazu kommen zwei eigene Beweise, die ohne Uhr auskommen.

### 4.1 Requester: die Zeit kommt von fremder Stelle (RFC 3161 eingehalten)

Das Geraet baut einen `TimeStampReq` ueber den Hash dessen, was es
unterschrieben hat, und nimmt den `TimeStampResp` einer fremden TSA an. Die
**Zeitangabe kommt aus dem Token der TSA**, nie aus der Geraeteuhr.

Was das beweist: dass die Daten zu dem Zeitpunkt existierten, den die TSA
bescheinigt - genau so weit, wie der Pruefer dieser TSA traut. Das ist die
uebliche und richtige Vertrauenskette; nichts daran ist durch die fehlende
Geraeteuhr geschwaecht.

**Arbeitsregel von Thomas eingehalten:** die technischen Anforderungen von
RFC 3161 werden gebaut (Aufbau des `TimeStampReq`, Nonce,
`messageImprint`, Pruefung der Antwortfelder). **Keine Zeitstempelstelle wird
beantragt** - das waere der bezahlte Prüflauf, und der wird geschoben.

### 4.2 Gegenzeichnen: das Geraet bindet das fremde Token an sich

Das Geraet unterschreibt eine eigene Erklaerung ueber:

```
Hash der Daten  +  Hash des TSA-Tokens  +  eigener Zaehlerstand  +  wer es anstiess
```

Was das beweist: dass **dieses** Geraet dieses fremde Token zu diesen Daten
gesehen hat. Ohne die Gegenzeichnung liegen zwei Papiere nebeneinander, die
nichts verbindet; mit ihr ist es eine Aussage.

### 4.3 Reihenfolge ohne Uhrzeit: der monotone Chipzaehler

Strang A liefert monotone Zaehler im Sicherheitschip
(`osk_se050_counter_raise()`, nur vorwaerts, kein Weg zurueck,
`include/osk_se050.h`). Jede gegengezeichnete Erklaerung traegt einen
Zaehlerstand.

**Was das beweist, und es ist mehr als es klingt:** eine **vollstaendige,
lueckenlose Reihenfolge** aller Signaturen dieses Geraets, die sich nicht
umstellen und nicht zurueckdrehen laesst - auch dann nicht, wenn der
Angreifer den gesamten Flash kontrolliert. Wer zwei Erklaerungen desselben
Geraets hat, weiss zweifelsfrei, welche zuerst entstand. Eine Luecke in der
Nummernfolge zeigt eine unterdrueckte Erklaerung.

**Was es nicht beweist:** keine Uhrzeit. "Vorher" ja, "am Dienstag" nein.

### 4.4 Die drei im Vergleich

| | beweist | haengt ab von |
| --- | --- | --- |
| TSA-Token (4.1) | Zeitpunkt | Vertrauen in die TSA, Netz zur Zeit der Signatur |
| Gegenzeichnung (4.2) | dieses Geraet hat es gesehen | Geraeteschluessel im Chip |
| Chipzaehler (4.3) | Reihenfolge, lueckenlos | nichts aussen - staerkster der drei |

**Der Chipzaehler ist der einzige der drei, der ohne jede fremde Stelle und
ohne Netz etwas beweist.** Darum wird er immer gefuehrt, auch wenn keine TSA
erreichbar ist.

## 5. Was ausdruecklich NICHT gebaut wird

- **Keine Funktion, die aus der Geraeteuhr einen Zeitstempel macht.** Es gibt
  kein `authz_tsa_make_token()`. Haette das Geraet so eine Funktion, wuerde
  irgendwann jemand ihr Ergebnis fuer einen Beweis halten.
- **Keine erfundene Uhr.** Die Geraetezeit wird in den Erklaerungen
  gefuehrt, aber sie ist als `Geraetezeit seit dem Einstecken` benannt und
  nicht als UTC. Wer sie liest, soll nicht glauben koennen, es sei eine
  Weltzeit.
- **Keine Pruefung der TSA-Signatur in der Firmware** (`offen`, siehe unten).

## 6. Offen

1. **Wer prueft die Signatur des TSA-Tokens?** Dazu gehoeren CMS
   (RFC 5652), eine X.509-Kettenpruefung und ein Vertrauensspeicher - zusammen
   deutlich mehr Flash als der Rest dieser Schicht, und die Flash-Rechnung ist
   schon offener Punkt 2 im Plan. **Heute: das Geraet prueft die Form der
   Antwort (Felder, Laengen, Nonce, `status`), nicht die Signatur.** Die
   Signaturpruefung macht das Verwaltungsprogramm am Rechner. Das ist eine
   bewusste Grenze und gehoert ins Handbuch, nicht in eine stille Annahme.
2. **Woher kommt die Nonce-Gegenprobe nach einem Stromausfall?** Die Nonce
   liegt im RAM; wird der Stick zwischen Anfrage und Antwort gezogen, ist sie
   weg und die Antwort nicht mehr zuzuordnen. Dann gilt die Anfrage als
   verloren und muss wiederholt werden. Gebaut, aber nicht am Geraet geprueft.
3. **Wer liefert UTC beim Einstecken?** Unveraendert offen, wie in
   `docs/dongle-firmware.md` festgehalten. Diese Schicht wartet nicht darauf:
   keiner ihrer drei Beweise braucht eine richtige Geraeteuhr.
