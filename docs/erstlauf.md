# Erstlauf: die Werksschlüssel des Sicherheitschips ersetzen

Was dieses Dokument beschreibt: den Ablauf, der beim ersten Start eines
Geräts die Werksschlüssel des SE051 austauscht, und die vier Entscheidungen,
die dabei zu treffen waren — jede mit ihrer Begründung und mit dem, was sie
**nicht** leistet.

Code: `apps/firmware/include/osk_commission.h` (Vertrag),
`apps/firmware/src/classes/se050/se050_commission.c` (Ablauf),
`apps/firmware/src/classes/se050/se050_commission_platform.c` (Platinenhaken,
noch ein Platzhalter), `apps/firmware/test/se050/test_commission.c`
(Prüfungen), `apps/firmware/src/main.c` (der Aufruf beim Start).

## Der Befund, der dazu geführt hat

Die Werks-Schlüssel des Platform-SCP-Kanals stehen im Klartext in NXP
AN12973 Tabelle 6 — ENC, MAC und DEK, je Variante und Fertigungslos (OEF
ID). Wer das freie Dokument liest, liest auf einem Gerät im Werkszustand den
ganzen I2C-Verkehr zum Chip mit und kann ihn stellen. Damit ist der ganze
Zweck des zweiten Chips hin, denn der besteht darin, dass die Schlüssel ihn
nie verlassen. Belegt in `docs/se050.md`, Abschnitt *The factory keys are
published*.

`osk_se050_rotate_scp_keys()` und `osk_se050_require_scp()` waren gebaut und
geprüft. **Gerufen hat sie niemand.** Es gab im ganzen Repo nur die
Deklaration, zwei Attrappen, `test_contract.c` und das CHANGELOG. Das
Werkzeug war da, der Ablauf fehlte — und ein so ausgeliefertes Gerät läuft
mit öffentlich bekannten Schlüsseln. Diese Lücke füllt der Erstlauf.

## Wo der Start des Geräts liegt

Gesucht und gefunden: `apps/firmware/src/main.c`. Das ist der Zephyr-Einstieg
und heute ein Durchlauf durch die sieben Teile — er ruft jeden einmal und
schreibt, was er gemeldet hat. Der Bootloader-Teil in `apps/firmware/boot/`
ist etwas anderes: er entscheidet, **welches Abbild** startet
(`osk_boot_decide`), nicht was die Firmware danach tut. Der Erstlauf gehört
in die Firmware, nicht in den Bootloader, denn er braucht den
SE050-Treiber, den Tresor und die Zufallsquellen — alles Dinge, die der
Bootloader nicht hat und nicht haben soll.

Der Erstlauf ist in `main()` der **erste** Schritt, vor `walk_se050()`. Jeder
andere Teil, der den Chip anfasst, täte es sonst über einen Kanal mit
bekannten Schlüsseln.

## Frage 1: Woran erkennt das Gerät, dass es sein erster Start ist?

**Nicht an einem Flag. An der Frage, welcher Schlüsselsatz den Kanal
öffnet.** Der Chip antwortet; der Zettel im Flash ist nur das Gedächtnis.

Ein Flag allein wäre fälschbar: wer es zurücksetzen kann, bekommt einen
zweiten „ersten Start" und damit einen Tausch gegen Schlüssel, die er selbst
wählt. Deshalb wird der Zettel bei jedem Lauf gegen den Chip geprüft, und
zwar in allen vier Kombinationen:

| Zettel | Chip antwortet auf | Ergebnis |
|---|---|---|
| keiner / `FACTORY` | Werkssatz | **erster Start** — es wird getauscht |
| keiner / `FACTORY` | Werkssatz nicht | `OSK_ERR_ROLLBACK` — nichts wird getauscht |
| `DONE` | eigenen Satz | fertig, kein einziger Befehl geht auf den Bus |
| `DONE` | Werkssatz | `OSK_ERR_ROLLBACK` — nichts wird getauscht |

Die zweite Zeile ist der gelöschte Flash: der Chip hält Schlüssel, die wir
nicht mehr kennen. Ein Tausch bräuchte den **gültigen** Satz als `current`,
und den haben wir nicht. Das Gerät ist nicht mehr zu öffnen; das ist das
ehrliche Ergebnis, und es wird gemeldet statt übergangen.

Die vierte Zeile ist der zurückgesetzte oder ausgetauschte Chip. Ein Erstlauf
wäre hier genau das, was ein Angreifer will, also findet er nicht statt.

### Was dieser Weg nicht leistet

Fehlt der Zettel **und** ist der Chip wirklich im Werkszustand, dann sieht
ein frisches Gerät genauso aus wie eines, bei dem jemand den Flash gelöscht
**und** den Chip auf Werkszustand zurückgesetzt hat. Das ist hingenommen, und
zwar aus einem nachrechenbaren Grund: ein Werkszustand des Chips bedeutet,
dass alle Nutzschlüssel darin weg sind. Der Angreifer gewinnt ein leeres
Gerät. Es gibt nichts mehr zu schützen.

## Frage 2: Woher kommen die neuen Schlüssel, und wo liegen sie?

**Sie entstehen im Gerät, aus zwei Quellen mit XOR verknüpft:** dem Zufall
aus dem Rechenkern (CryptoCell des nRF52840) und dem Zufall aus dem Chip
(`osk_se050_random`). Keine der beiden bestimmt den Schlüssel allein. Meldet
eine einen Fehler oder liefert sie nur Nullen, bricht der Lauf ab, **bevor
etwas aufgeschrieben ist** — mit einer halben Quelle wird nicht
weitergerechnet.

**Sie liegen danach im Zettel im Flash des nRF52840.** Das ist die einzige
Stelle im Gerät, an der ein dauerhaftes Geheimnis im Rechenkern liegt, dessen
Auslesesperre uns als gebrochen gilt. Das ist unvermeidlich: ohne diese
Schlüssel kann das Gerät seinen Chip nicht ansprechen, und wer sie erst vom
Chip holen müsste, bräuchte sie dafür schon. Der Zettel wird von der
Tresorschicht verpackt, nicht vom Erstlauf.

Was die SCP03-Schlüssel schützen, ist also **der Lauscher an den zwei
I2C-Leitungen** — nicht der Angreifer, der den Rechenkern ausliest. Gegen den
schützt etwas anderes: die Nutzschlüssel entstehen im Chip und verlassen ihn
nie. Wer den Flash ausliest, kann mit dem Chip reden; er bekommt die
Schlüssel darin trotzdem nicht heraus, und die PIN-Fehlversuche zählt der
Chip.

### Wird der Ort gelöscht, ist das Gerät hin

Ein Gerät, dessen SCP03-Schlüssel verloren sind, kann seinen Chip nicht mehr
ansprechen. Ein Ziegelstein. **Es gibt absichtlich keine Sicherung.** Eine
Kopie außerhalb des Geräts wäre genau das, was der ganze Aufwand verhindern
soll: ein Ort, an dem die Schlüssel ohne das Gerät liegen. Dagegen hilft
nur, den Zettel dort hinzulegen, wo das Update ihn nicht anfasst — dafür ist
`osk_boot_may_write()` schon da.

### Der Lauscher beim ersten Start: offen und nicht zu schließen

**Wer den ersten Start mitschneidet, kennt die neuen Schlüssel.** Der
`PUT KEY`-Befehl trägt sie, mit dem **Werks**-Key-DEK verpackt (GPCS
Amendment D 6.2.8) — und der steht in AN12973. Die zwei Zufallsquellen helfen
dagegen nichts; sie sind gegen eine kaputte Quelle, nicht gegen den Lauscher.

Das ist keine Lücke dieses Ablaufs, sondern eine des Verfahrens: es gibt
keinen zweiten Weg zum Chip, über den der erste Schlüssel kommen könnte.
Daraus folgt eine Anforderung an die Fertigung, nicht an den Code: **der
erste Start muss in sicherer Hand geschehen.** Wer das Gerät erst danach in
die Hand bekommt, kommt zu spät.

## Frage 3: Was passiert bei Stromverlust mitten im Tausch?

Der Angelpunkt: **die Schlüssel werden festgenagelt, bevor ein Befehl den
Chip erreicht.** Der Zettel steht auf `PENDING` und trägt beide neuen Sätze,
noch bevor das erste `PUT KEY` gebaut wird. Ab da gilt: jeder weitere Lauf
setzt **denselben** Tausch fort und erfindet keine neuen Schlüssel.

Damit kann ein Angreifer, der den Strom im richtigen Augenblick zieht, keinen
Satz seiner Wahl erzwingen — er bekommt denselben Satz noch einmal. Das ist
der ganze Zweck dieser Reihenfolge.

Dazu zwei Bits statt eines Schrittzählers: Bit 0 für Satz 11, Bit 1 für
Satz 12. Nach einem Abbruch steht ohne Raten fest, welcher Befehl noch fehlt.
Geschrieben wird das Bit **nach** dem Befehl, nicht vorher: umgekehrt stünde
im Zettel ein Satz als getauscht, den der Chip nie bekam, und das Gerät
suchte seinen Kanal mit Schlüsseln, die nirgends gelten.

### Das engste Fenster, und wie es geschlossen wird

Bleibt eines: `PUT KEY` ist durchgegangen, der Strom fiel **vor** dem
Schreiben des Bits aus. Dann hat der Chip den Satz, der Zettel weiß es nicht,
und ein zweiter Versuch mit dem Werks-Key-DEK als `current` kann nicht mehr
gelingen.

Geschlossen wird das mit derselben Frage wie überall: **öffnet der eigene
Satz den Kanal?** Wenn ja, ist das Bit nachzutragen und der Befehl
übersprungen. Geraten wird nicht — antwortet der Chip nicht eindeutig, bleibt
das Bit, wie es war, der Tausch wird versucht und darf scheitern.

Für Satz 11 ist das Fenster damit zu (geprüft). Für Satz 12 bleibt es offen,
weil die Frage dort nicht gestellt werden kann: der Kanal läuft nicht auf dem
Rettungssatz. Das Ergebnis dieses Falls ist ein Gerät, das **ansprechbar
bleibt** — Satz 11 ist unberührt — und dessen Erstlauf nicht fertig wird und
das auch meldet. Auch das ist geprüft, und zwar als genau dieser Ausgang, nicht
als Erfolg.

### Die Reihenfolge: Satz 12 vor Satz 11

Zwei Gründe, und beide zählen:

1. Satz 12 ist der Rettungssatz, mit dem Satz 11 neu gesetzt werden kann
   (AN12973 2.3). Solange er NXP gehört, ist ein getauschter Satz 11 nichts
   wert.
2. Der Gerätekanal läuft auf einem der beiden Sätze; welcher es ist, ist am
   echten Chip nicht bestätigt. Wäre es Satz 11 und wir tauschten ihn zuerst,
   käme der zweite Befehl über einen Kanal, dessen Schlüssel gerade gewechselt
   wurde — und Satz 12 bliebe für immer der von NXP. Satz 12 zuerst kann
   diesen Fall nicht haben.

**Die Reihenfolge rettet keinen der beiden Zwischenstände.** Zwischen den
Befehlen ist immer noch ein Werkssatz im Chip, und mit dem kann ein Angreifer
den anderen umschreiben. Sicher ist erst das fertige Paar. Darum kommt
`require_scp()` danach und nicht dazwischen, und darum ist ein abgebrochener
Lauf ein Fehler und kein Teilerfolg.

## Frage 4: Kann der Tausch zweimal laufen?

**Er läuft genau zweimal — einmal je Satz — und kein drittes Mal.**

Die Schlüsselfassung in `PUT KEY` ist die Satznummer: 11 ist `0x0B`, 12 ist
`0x0C`. Keine erfundenen Kennungen, sondern dieselbe Zahl in einem anderen
Zahlensystem. Erlaubt ist `0x01` bis `0x7F` (GPCS 11.8.2.1); `0x00` wäre
„neuen Satz anlegen" statt ersetzen, und `0x80` hat das Bit, das „letzter
Befehl" bedeutet — der Treiber weist beides ab, und das war schon geprüft.

Ob zwei Sätze gebraucht werden, war keine Ermessensfrage: AN12973
Abschnitt 2.3 verlangt es ausdrücklich — *„it is required that key set 12
shall be changed to a customer defined and owned value before the SE051
product is deployed in production"*. Wer nur 11 ersetzt, lässt eine Hintertür
offen, deren Schlüssel bei NXP liegt. Das stand bereits in `osk_se050.h` und
in `docs/se050.md`; dort steht auch der Satz, der diesen Auftrag ausgelöst
hat: *„nothing in the driver enforces the second one"*. Jetzt erzwingt es der
Erstlauf.

Gegen einen dritten Lauf stehen drei Dinge, unabhängig voneinander:

- Der Zettel steht auf `DONE`, und dann geht kein Befehl auf den Bus.
- Der Chip nimmt die Werksschlüssel nicht mehr an — mit ihnen öffnet kein
  Kanal, und ein `PUT KEY`, das mit dem Werks-Key-DEK verpackt ist, wird
  abgelehnt.
- `osk_se050_rotate_scp_keys()` weist `current == new` ab (`OSK_ERR_ARG`).

## Am Ende wird nachgemessen

Bevor der Kanal verpflichtend gemacht wird, prüft der Erstlauf, dass die
Werksschlüssel **wirklich** weg sind: mit beiden Werkssätzen darf kein Kanal
mehr aufgehen. Tut er es doch, wird `require_scp()` **nicht** gerufen und der
Lauf meldet Fehler.

Das ist nicht ein zweites „sieht gut aus". Ein Baustein, der auf jedes
`PUT KEY` mit `0x9000` antwortet und nichts tauscht, hätte sonst ein Gerät mit
`SCP_REQUIRED` **und** den Schlüsseln aus AN12973 hinterlassen — das
schlechteste von beidem. Genau dieser Fall ist geprüft.

## Was am echten Chip offen ist

Am Gerät ist nichts davon gelaufen; es gibt keinen Chip. Der Prüflauf läuft
gegen einen Nachbau am PC. Was dabei **Annahme** bleibt, und zwar in der
Reihenfolge, in der es weh täte:

1. **Ob der SE051 `PUT KEY` über diesen Kanal überhaupt annimmt.** Das ist die
   größte offene Stelle, und sie ist älter als dieser Erstlauf: der Treiber
   baut den Applet-AESKey-Kanal auf (AN12413 4.5.3), Platform SCP liegt eine
   Ebene darunter beim Kartenverwalter, und AN12413 beschreibt nur
   Applet-Befehle. Vielleicht braucht es eine eigene Sitzung mit
   Schlüsselfassung 11 statt 00 und ohne ausgewähltes Applet. Der Inhalt des
   Befehls ist beleggenau; **die Ebene ist es nicht.** Am Gerät ist das eine
   Messung: Befehl schicken und sehen, ob `0x9000` kommt. Steht auch in
   `docs/se050.md` und an der Aufrufstelle im Code.
2. **Ob sich mit Satz 12 ein Kanal aufbauen lässt.** Der Nachbau baut
   Sitzungen nur gegen Satz 11 auf — das ist seine Annahme, nicht eine
   Messung. Davon hängt ab, ob das letzte Abbruchfenster (Frage 3) zu ist oder
   offen bleibt. Heute ist es offen, und der Prüflauf hält genau das fest.
3. **Ob der Chip nach einem `PUT KEY` die alte Sitzung stehen lässt.** Der
   Nachbau tut es, mit der Begründung, dass die Sitzungsschlüssel schon
   abgeleitet sind. Wenn der echte Chip die Sitzung stattdessen fallen lässt,
   braucht der zweite Befehl einen neuen Kanalaufbau — der Ablauf überlebt
   das, denn ein gescheiterter Befehl lässt den Zettel auf `PENDING`, aber es
   kostet einen Start.
4. **Ob der Chip ein `PUT KEY` mit falschem Key-DEK wirklich ablehnt.** Der
   Nachbau packt aus und prüft den Prüfwert nach, weil der Chip es so tun
   müsste (Amendment D 7.2.2). Wie er es wirklich meldet — welches Statuswort
   — ist nicht belegt. Für den Ablauf zählt nur, dass es nicht `0x9000` ist.
5. **Was `SetPlatformSCPRequest` mit `SCP_REQUIRED` danach ablehnt.** Der
   Nachbau merkt sich nur, dass es angekommen ist; er verlangt den Kanal
   ohnehin immer. Die Wirkung ist nicht nachgestellt.

Die Werkssätze im Prüflauf sind **erfundene** Werte. Die echten stehen in
AN12973 Tabelle 6 und hängen am Fertigungslos; sie gehören nicht in dieses
Repo, und die Platinenschicht liefert, was für das verbaute Teil gilt.

## Was noch nicht gebaut ist

Der Platinenhaken `osk_commission_platform_backend()` ist ein ehrlicher
Platzhalter und meldet `OSK_ERR_UNSUPPORTED`
(`se050_commission_platform.c` sagt, welche vier Dinge fehlen: der
Flash-Bereich für den Zettel, die zwei Zufallsquellen und die Werkssätze nach
OEF ID). Der Start ruft ihn trotzdem, und das Ergebnis steht in der Ausgabe —
ein Gerät, auf dem der Erstlauf nicht gelaufen ist, läuft mit öffentlich
bekannten Schlüsseln, und das soll man sehen.

Stufe nach `docs/se050.md`: **geschrieben und übersetzt, am PC geprüft.**
Nicht am Gerät gelaufen.
