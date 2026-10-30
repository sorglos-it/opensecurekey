# Die PIN am Gerät

Stand 30.10.2026. Gebaut und am PC geprüft. **Auf Hardware ist nichts davon
gelaufen** – es gibt noch kein Gerät.

## Warum die PIN nicht mehr über den Rechner geht

Bisher sagte die Anzeige „PIN am Rechner eingeben". Das geht nicht weiter:

Ab **FIPS 140-3 Stufe 3** ist ein vertrauenswürdiger Kanal Pflicht. Die PIN
darf nur über einen Weg laufen, der dem Rechner nicht gehört. Über USB läuft
sie damit nie wieder. Aus einer Bequemlichkeit wird eine Anforderung.

Dazu die zweite Hälfte: eine PIN, die der Rechner tippt, liegt dort auch im
Speicher. Genau diese Fehlerklasse – ein Geheimnis an der falschen Stelle –
hat 2024 den Marktführer getroffen (EUCLEAK).

## Die Frage, die Thomas gehört: wie viele Taster?

Heute ist **ein** Taster bedienbar (SW1, seitlich). SW2 ist zur Lötbrücke für
den Reset geworden. Ein zweiter Taster braucht Platz auf einer Platine, die
schon zu kurz ist.

Gerechnet, nicht geschätzt – sechsstellige PIN, alle Zahlen aus
`osk_pin_cost()` in `apps/firmware/src/classes/ui/osk_pin_entry.h`, nachgeprüft in
`test_pin.c`:

| Weg | Taster | Drücke schlimmster Fall | Drücke mittlerer Fall | Zeit schlimmster Fall | Zeit mittlerer Fall |
|---|---|---|---|---|---|
| **A** Ein Taster: kurz blättert, lang nimmt | 1 | 60 | 33 | **39,2 s** | **22,4 s** |
| **B** Ein Taster: kurz = 0, lang = 1, vier Bits je Stelle | 1 | 24 | 24 | **22,8 s** | **18,8 s** |
| **C** Zwei Taster: 1 blättert, 2 nimmt | 2 | 60 | 33 | **16,2 s** | **8,9 s** |

Woher die Zeiten kommen (die Annahmen stehen nur in `osk_pin_entry.h`, damit sie
nicht zweimal behauptet werden):

* kurzer Druck: 120 ms Taster unten + 150 ms loslassen
* **+ 350 ms Wartezeit**, wenn derselbe Taster auch den Doppeldruck kennt –
  ein kurzer Druck darf erst gemeldet werden, wenn kein zweiter mehr folgen
  kann
* langer Druck: 800 ms halten + 150 ms loslassen; gemeldet wird er, sobald
  die Zeit erreicht ist
* ein echter Finger ist **nicht gemessen** worden

Der größte einzelne Posten ist nicht das Blättern, sondern diese 350 ms: bei
einem Taster muss jeder kurze Druck sie abwarten, bei zwei Tastern nie, weil
der zweite Taster die Absicht schon verrät. Das ist der ganze Unterschied
zwischen A (22,4 s) und C (8,9 s).

### Der Preis je Weg

| | Platz und Bauteile | Verwechslungsgefahr |
|---|---|---|
| **A** | nichts Neues; läuft auf der Platine von heute | gering: blättern und nehmen sind kurz und lang, der Wert steht groß auf dem Glas |
| **A** | | zäh: im schlimmsten Fall 54 Mal blättern für sechs Ziffern |
| **B** | nichts Neues | **hoch**: der Nutzer muss im Kopf in Vierer-Bits zählen. Vier Drücke je Stelle sind wenige, aber ein falsches Bit fällt erst auf, wenn die Ziffer fertig ist. 10 bis 15 sind keine Ziffer – die Stelle fängt dann von vorn an |
| **C** | ein Taster mehr (EVQP7A01P, ~3 × 2,5 mm), ein Loch mehr im Gehäuse, eine Leitung mehr zum Rechenkern | mittel: zwei Taster an der Seite werden vertauscht, solange sie gleich aussehen |

**Nicht entschieden.** Die Tasterzahl ist eine Hardwarefrage. Der Code
braucht die Antwort nicht, um zu laufen (siehe unten); er braucht sie nur,
damit die richtige Variante freigeschaltet wird.

Zu ändern ist dann **eine** Zeile: `OSK_PIN_SCHEME_DEFAULT` in `osk_pin_entry.h`.
Bei zwei Tastern kommt dazu, dass die Firmware beim Start `button2_down` in
`osk_ui_port_t` einhängt. Hängt es nicht, lehnt die Anzeige den Zwei-Taster-Weg
mit `OSK_ERR_UNSUPPORTED` ab, statt ihn still falsch zu bedienen.

## Wie die Eingabe von der Tasterzahl entkoppelt ist

Drei Schichten, und nur die erste weiß von Tastern:

1. `osk_pin_map()` – Rohereignis eines Tasters → **Handlung**
   (weiter, nehmen, Bit 0, Bit 1, zurück, abbrechen). Das ist eine Tabelle,
   rund 40 Zeilen.
2. `osk_pin_apply()` – Handlung → Stand der Eingabe. Kennt keinen Taster.
3. `osk_pin_ask()` – die Schleife: lesen, zeichnen, abgeben. Fragt nur
   `osk_pin_buttons()`, wie viele Drehbücher sie lesen muss.

Ein Wechsel von einem auf zwei Taster rührt Schicht 2 und 3 nicht an. Das ist
im Prüfsatz nachgewiesen: dieselbe Eingabe läuft dort einmal mit einem und
einmal mit zwei Tastern durch, ohne dass eine Zeile dazwischen anders ist.

## Die drei Auflagen

### 1. Die PIN bleibt im Rechenkern und nicht lange

* Die Ziffern liegen in genau einer Struktur (`osk_pin_entry_t`), nur im RAM.
* Sie gehen **einmal** an `osk_pin_sink_t.submit()` – das ist der
  Sicherheitschip, der langsam ableitet: PIN → KEK → DEK. Die Ableitung und
  der Vergleich mit gleicher Laufzeit gehören dem Chip (`se050/`), nicht
  diesem Teil.
* Danach wird überschrieben (`osk_pin_wipe()`), und zwar auf **jedem** Weg
  hinaus: richtig, falsch, abgebrochen, Zeit abgelaufen, Chip gesperrt.
* Geprüft: nach einer vollen Eingabe sind alle zwölf Bytes des Feldes null,
  und ein Abbruch mitten in der Eingabe lässt keine halbe PIN stehen.

### 2. Der Zähler liegt im Chip, und es gibt keinen Weg daran vorbei

* `submit()` ist die **einzige** Stelle, an der eine PIN geprüft wird. Einen
  Vergleich im Flash gibt es nicht – er hätte keinen Zähler.
* Ist kein Chip eingehängt, gibt es am Gerät **keine** PIN-Eingabe
  (`OSK_ERR_UNSUPPORTED`). Absichtlich kein Ersatzweg.
* Der Chip wird **vor** der ersten Ziffer gefragt, wie viele Versuche er noch
  hergibt. Sind es null, wird nichts eingegeben – eine PIN in ein gesperrtes
  Gerät zu tippen zeigt sie nur jemandem, der zuschaut.
* Ein **Abbruch erhöht den Zähler nicht**, denn der Chip hat die Ziffern nie
  gesehen. Das Bild sagt darum „abgebrochen" und nicht „falsch".
* Geprüft mit einem Chip als Attrappe, der mitzählt: falsche PIN → Zähler 1;
  Abbruch → Zähler 0, nichts angekommen; Zeitablauf → Zähler 0; gesperrt →
  `OSK_ERR_LOCKED` und keine einzige Ziffer abgegeben.

### 3. Was auf dem Glas steht, kann ein Mensch lesen

Aktive Fläche **10,54 × 14,70 mm** auf 48 × 64 Punkten → 0,220 mm je Punkt
in der Breite, 0,230 mm in der Höhe.

* große Schrift 6 × 10 Punkte = **1,32 × 2,30 mm** je Platz
* kleine Schrift 4 × 8 Punkte = **0,88 × 1,84 mm** je Platz

Das Bild der Eingabe, von oben nach unten:

| Zeile | Was | Nachgezählt |
|---|---|---|
| 0–9 | Titel „PIN", große Schrift, mittig | 3 Zeichen × 6 = 18 von 48 Punkten |
| 10 | Trennstrich | 48 Punkte |
| 14–23 | die Ziffer, die gerade gewählt wird, groß | 6 von 48 Punkten |
| 30–34 | **die Punktreihe: eine Marke je Stelle** | 3 Punkte breit + 1 Abstand; 6 Stellen = 23, 12 Stellen = 47 von 48 |
| 38–53 | der Hinweis, was der Taster tut, kleine Schrift, höchstens 3 Zeilen | 12 Zeichen je Zeile |
| 56–63 | Fußzeile: „1/6" links, verbleibende Versuche rechts | |

Zwölf Stellen brauchen 47 der 48 Punkte – darum ist `OSK_PIN_MAX_LEN` zwölf
und nicht mehr. Geprüft wird das nicht durch Hinsehen, sondern Punkt für
Punkt: für jede Länge von 6 bis 12 und jede Stelle darin zählt `test_pin.c`
die gesetzten Punkte und sieht nach, dass die Reihe ganz in der Fläche liegt
und die Marke genau an der Stelle hohl ist, an der der Nutzer steht.

**Was nicht auf dem Glas steht:** die schon genommenen Ziffern. Das Bild
bekommt sie nicht einmal übergeben – nur, an welcher Stelle der Nutzer ist.

**Was doch auf dem Glas steht, und wofür ich eine Entscheidung brauche:** die
Ziffer, die gerade gewählt wird. Ohne sie kann niemand ein Rad durchblättern –
blind sechs Mal bis zur richtigen Zahl zu zählen ist keine Bedienung. Sie
steht auf dem vertrauenswürdigen Display und nicht auf dem Rechner, so machen
es Trezor und Ledger auch. Wer das nicht will, baut mit
`-DOSK_PIN_HIDE_VALUE`; dann steht dort ein Balken statt der Ziffer und die
Eingabe ist blind. Standard ist: Ziffer sichtbar.

## Was andere Teile von der Anzeige bekommen

Zwei Auflagen, die Nachbarn gemeldet haben, sind eingebaut:

**Betriebsmodus vor dem Schreiben** (für den Datenträger):
`osk_view_mode(fb, mode_key, ziel, geheim)` zeigt den Modus groß, darunter
wörtlich das Ziel, und in der Fußzeile die Aussage „Geschützt" oder
„Ungeschützt". Beides ist in fünf Sprachen da und passt ungekürzt in die
Breite – das ist nachgemessen, denn eine Fußzeile wird nicht umbrochen. Die
Reihenfolge – erst nennen, dann schreiben – muss der Aufrufer einhalten; das
Bild kann sie nicht erzwingen.

**Lesbar oder ablehnen** (für die Geldbörse): `osk_view_value_ok(wert)` sagt
nein, wenn die Anzeige einen Betrag oder Empfänger nicht Zeichen für Zeichen
zeigen kann – bei Steuerzeichen, bei mehr als 96 Zeichen und bei jedem
Zeichen, das die Schrift nicht kennt. Die Schrift macht aus Unbekanntem ein
Fragezeichen, und „0,5 ETH an 0x71C7?6?EC7" sieht aus wie eine Adresse, ist
aber eine andere. Wer nein bekommt, **lehnt den Vorgang ab** statt etwas
Halbes zu zeigen. Eine ETH-Adresse, eine IBAN, „Jürgen Müller" und ein
Eurozeichen gehen durch; chinesische und kyrillische Zeichen nicht.

## Prüfstand

`apps/firmware/test/ui/`, über die WSL, `make` und `make wide`:

* vorher: **4074 Prüfungen, 0 Fehler**
* nachher: **4658 Prüfungen, 0 Fehler** (48 × 64, das Zielteil)
* zweite Größe: **4656 Prüfungen, 0 Fehler** (`make wide`, 128 × 64)

Jede neue Prüfung ist einmal zum Anschlagen gebracht worden, bevor sie grün
wurde. Kein Chip, kein Taster, kein Display war beteiligt – alles vier sind
Attrappen.

## Was offen ist

1. **Wie viele Taster?** Die Rechnung oben liegt vor; die Entscheidung gehört
   Thomas. Danach: eine Zeile in `osk_pin_entry.h`, und bei zwei Tastern ein Griff
   mehr beim Start der Firmware.
2. **Soll die gewählte Ziffer sichtbar sein?** Standard ja, Begründung oben.
3. **Der Chip fehlt.** `osk_pin_sink_t` ist der Vertrag; die langsame
   Ableitung, der laufzeitgleiche Vergleich und der Fehlversuchszähler werden
   im Sicherheitschip gebaut (`se050/`, anderer Strang). Bis dahin gibt es am
   Gerät keine PIN – mit Absicht.
4. **Die Länge** ist auf sechs Stellen festgelegt (`OSK_UI_PIN_LEN`), erlaubt
   sind 6 bis 12. Was der Chip wirklich verlangt, entscheidet der Chip.
