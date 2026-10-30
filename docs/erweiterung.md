# Die Browser-Erweiterung

Die Erweiterung hat eine Aufgabe: **Passwoerter ausfuellen, und zwar nur auf
der richtigen Seite.** Alles andere in diesem Ordner dient dieser einen
Aufgabe oder steht ihr im Weg.

Sie spricht nicht mit dem Geraet. Sie spricht mit dem Programm `osk`
(`apps/desktop`), und `osk` spricht mit dem Geraet. Dieser Text beschreibt
die Erweiterung; die Gegenstelle steht in
`apps/desktop/classes/native_messaging.go`.

> **Stand:** geschrieben und geprueft, nie an Hardware gelaufen. Der
> Sicherheitschip ist bestellt und nicht geliefert. Was hier "geprueft"
> heisst, heisst: `node --test` laeuft durch. Es heisst nicht, dass jemand
> auf einer echten Bankseite ein echtes Passwort hat eintragen sehen.

## Die Teile

| Datei | Was sie tut |
| --- | --- |
| `manifest.json` | Rechte und Einstiegspunkte |
| `assets/background.js` | Der Hintergrunddienst. Die EINZIGE Stelle, die mit `osk` spricht, und die einzige, die die Adresse der Seite feststellt |
| `assets/rules.js` | Die Entscheidungen - ohne Verbindung, ohne Fenster, ohne Geraet, darum einzeln pruefbar |
| `assets/popup.js`, `views/popup.html` | Das Fenster. Es zeigt an und fragt nach; es entscheidet nichts |
| `assets/texts.js`, `lang/*.json` | Die Texte, fuenf Sprachen |
| `tests/` | Die Fallliste und die Attrappe von `osk` |

---

## 1. Was ist "dieselbe Adresse"?

**Die Regel, und sie hat keine Ausnahme: der Rechnername des Eintrags und
der Rechnername der Seite muessen Zeichen fuer Zeichen gleich sein.**

Verglichen wird der *Rechnername* - nicht die ganze Adresse, nicht die
Domaene darunter.

* **Nicht die ganze Adresse.** Pfad, Abfrage und Anker fallen weg. Ein
  Anwender meldet sich auf `/login`, auf `/anmelden` und auf
  `/konto?weiter=1` an - das ist dieselbe Tuer. Und die Abfrage kann lang
  und irrefuehrend sein: `boese.tld/?echt=bank.example.org`.
* **Nicht die Domaene darunter.** Ein Eintrag fuer `example.org` fuellt
  **nicht** auf `login.example.org` aus. Wer beides braucht, legt zwei
  Eintraege an.
* **Der Port zaehlt nicht.** Das Geheimnis gehoert zum Haus, nicht zu einer
  Tuer an ihm.

### Warum keine Unterdomaenen

Das ist die Entscheidung, die am meisten kostet, und sie ist Absicht.

Wer Unterdomaenen einschliesst, muss wissen, **wo eine Domaene endet**, und
das weiss niemand aus dem Namen allein. `bank.de` endet nach zwei Teilen,
`bank.co.uk` nach dreien, `kunde.beispiel-hoster.net` nach keinem davon -
und dort sitzen zwei verschiedene Kunden auf zwei Nachbarnamen. Wer diese
Grenze ziehen will, braucht eine gepflegte Liste aller oeffentlichen
Endungen. Eine Regel, deren Grenze von einer gepflegten Liste abhaengt, ist
genau dann falsch, wenn die Liste alt ist - und sie ist immer irgendwo alt.

Die billige Abkuerzung waere "endet auf `.example.org`". Die faellt sofort:
dann passt `example.org` auch auf `example.org.boese.tld`, und genau so
heissen Phishing-Seiten.

Zwei Eintraege statt einem sind unbequem. Ein Passwort beim Angreifer ist
teurer.

### Die Fallliste

`apps/web/tests/adressen.test.js`. Sie hat **zwei Haelften, und beide sind
Pflicht**:

* `PASST` - 18 Paare, die zusammenfinden muessen
* `PASST NICHT` - 29 Paare, die nie zusammenfinden duerfen

Eine Liste mit nur der ersten Haelfte ist gruen, wenn die Regel "immer ja"
lautet. Das ist der Fehler, den man nicht sieht.

**Zum Anschlagen gebracht am 30.10.2026**, beide Richtungen: mit einem
`oskMatches`, das immer `true` liefert, faellt die zweite Haelfte; mit
einem, das immer `false` liefert, die erste. Dazu steht eine dritte
Gegenprobe *im* Pruefsatz: sie misst nach, dass eine grosszuegigere Regel
("endet auf" statt "ist gleich") an mindestens einer Zeile scheitern wuerde.
Ohne sie koennte die Liste mit der Zeit zahnlos werden, ohne rot zu werden.

Beim Schreiben der Liste fiel ein Fall anders aus als erwartet: ein Eintrag
mit einem Zeilenumbruch **am Ende** passt, weil `osk` den Rand genauso
abschneidet (`TrimSpace`). Er steht jetzt in der ersten Haelfte. Ein Umbruch
**mitten** im Feld bleibt verboten. Die Erweiterung strenger zu machen als
`osk` waere kein Gewinn: `osk` listete den Eintrag, und die Erweiterung
wiese ihn wortlos ab - der Anwender saehe einen Knopf, der nichts tut.

---

## 2. Namen, die gleich aussehen

Das kyrillische **а** (U+0430) sieht aus wie das lateinische **a**. `bank.de`
mit einem kyrillischen a ist ein anderer Rechner, aber auf dem Bildschirm
steht derselbe Name. Das ist kein Randfall - es ist der haeufigste Angriff
auf diesen Mechanismus.

Zwei Dinge halten das auf, und es braucht beide:

**1. Verglichen wird die ASCII-Form.** Der Browser wandelt einen Namen mit
fremden Zeichen in Punycode: aus dem kyrillischen `bank.example` wird
`xn--bnk-6cd.example`. Das ist nicht `bank.example`, also passt kein
Eintrag, also wird nichts ausgefuellt. Der Schutz steht schon durch den
Zeichen-fuer-Zeichen-Vergleich.

**2. Gezeigt wird dem Anwender ebenfalls die ASCII-Form, nie die huebsche.**
Eine Oberflaeche, die `bank.example` anzeigt, waehrend der Rechner
`xn--bnk-6cd.example` heisst, luegt den Anwender an - freundlich, aber sie
luegt. Im Fenster steht ueberall das, was verglichen wird.

Dazu ein Hinweis in der Kopfzeile (`popup.foreign`), sobald ein Teil des
Namens mit `xn--` beginnt. Er **sperrt nicht**: ein Name mit fremden Zeichen
kann vollkommen richtig sein - ein deutscher Umlaut, ein griechischer Laden
-, und wer ihn hinterlegt hat, soll ihn benutzen koennen. Er sagt nur: schau
genau hin.

---

## 3. Was passiert, wenn es nicht passt

**Nichts wird gefuellt. Es gibt keinen Weg, das zu uebergehen.**

Es gibt in dieser Erweiterung **keine Warnung mit einem Knopf daneben, der
sie wegklickt**. Eine Warnung, die man wegklicken kann, waehrend das Feld
schon gefuellt ist, ist keine Warnung, sondern eine Formalie. Darum:

* Der Hinweis auf fremde Zeichen steht in der Kopfzeile und hat **keinen
  Schliessknopf**. Er bleibt stehen, solange der Reiter so heisst.
* Passt die Adresse nicht, **erscheint der Eintrag gar nicht erst** in der
  Liste - es gibt keinen Knopf, den man druecken koennte.
* Die Pruefung, dass das Feld leer bleibt, steht als eigene Zeile im
  Pruefsatz (`passt die Adresse nicht, bleibt das Feld leer`). Geprueft wird
  dort nicht die Meldung, sondern das Feld.

### Die Riegel, der Reihe nach

| # | Wo | Was |
| --- | --- | --- |
| 1 | `background.js`, `activeOrigin` | Die Adresse kommt aus der Adresszeile. `http` nur auf dem eigenen Rechner, kein Benutzername in der Adresse |
| 2 | `osk`, `replyList` | `osk` listet nur Eintraege, deren Adresse passt |
| 3 | `background.js`, nach der Liste | Die Erweiterung **misst die Liste von `osk` nach** (`oskMatches`). Ein Riegel, der sich auf die Zusage der Gegenseite verlaesst, ist keiner |
| 4 | `background.js`, beim Ausfuellen | Ausgefuellt wird nur eine Nummer, die fuer **genau diese** Adresse schon in der Liste stand |
| 5 | `osk`, `replyReveal` | Auch wer die Nummer kennt, bekommt nichts, wenn die Adresse nicht passt |
| 6 | `rules.js`, `oskFillForm`, in der Seite | Die Adresse wird noch einmal aus dem Dokument gelesen. Hat sich der Reiter inzwischen bewegt, landet nichts |

Riegel 6 schliesst ein kleines Fenster zwischen dem Lesen der Adresse und
dem Eintragen. Es ist klein - und Phishing lebt von kleinen Fenstern.

---

## 4. Wer sagt, auf welcher Seite der Anwender ist

**`chrome.tabs`, also die Oberflaeche des Browsers. Nie die Seite.**

Die Quelle ist vertrauenswuerdig, weil sie ausserhalb dessen liegt, was eine
Seite beeinflussen kann: `tab.url` ist das, was in der Adresszeile steht,
und das setzt der Browser, nicht das Dokument. Eine Seite kann ihren eigenen
Inhalt beliebig beschriften - `document.domain`, ein `<meta>`, ein
`postMessage`, ein Eintrag in `history` - und koennte so behaupten, sie sei
die Bank. Nichts davon aendert `tab.url`.

Daraus folgt, was die Erweiterung **nicht** hat:

* **Kein Inhaltsskript**, das dauerhaft auf Seiten laeuft. Das Skript wird
  erst auf Knopfdruck in genau diesen einen Reiter gegeben
  (`chrome.scripting.executeScript`, `allFrames: false`). Darum genuegt das
  Recht `activeTab`; die Erweiterung braucht kein Recht auf fremde Seiten.
* **Keine Schnittstelle fuer Seiten.** Eine Seite kann die Erweiterung nicht
  ansprechen. Es gibt nichts anzuklopfen.
* **Keinen Port auf `127.0.0.1`.** Ein Port waere von jeder Seite im selben
  Browser erreichbar. `osk` wird als Kindprozess vom Browser gestartet, und
  nur fuer die Erweiterungen, die in seiner Anmeldedatei stehen.

Das Fenster bekommt die Adresse **nur zum Anzeigen**. Es kann keine Adresse
nennen, und jede Antwort des Dienstes bezieht sich auf den Reiter, der
gerade vorn ist.

---

## 5. Welche Software fragt gerade

Das Geraet zeigt immer, welche Software fragt. Fuer die Erweiterung heisst
das: **eine Abfrage aus einem eingebetteten Rahmen darf nicht als die
Hauptseite auftreten.**

* Das Skript laeuft nur im **Hauptdokument** (`allFrames: false`), und
  `oskFillForm` prueft das selbst noch einmal (`window.top !== window.self`
  → `frame`). Wer eines Tages `allFrames: true` schreibt, faellt im
  Pruefsatz auf und nicht beim Anwender.
* Steht das Anmeldefeld in einem **fremden Rahmen** - der haeufige Fall bei
  Anmeldungen ueber einen Dritten und bei Bezahlseiten -, dann wird dort
  **nicht** gefuellt, und der Anwender erfaehrt **wessen** Rahmen es ist:
  *"Das Anmeldefeld gehoert nicht zu dieser Seite, sondern zu
  anmeldung.fremd.example."* Still "kein Feld gefunden" zu melden waere
  falsch - der Anwender *sieht* ein Anmeldefeld.
* Das Fenster zeigt ausserdem, unter welchem Namen das Geraet den Zugriff
  protokolliert, mit dem Satz von `osk`: dieser Name ist eine **Behauptung**
  des Programms, kein Beweis.

---

## Die Attrappe von `osk`

`apps/web/tests/osk-attrappe.js`. Sie ist **gegen den Wortlaut** der
vorhandenen Schnittstelle gebaut (gelesen am 30.10.2026), nicht gegen die
Erwartung dieses Strangs, und sie **prueft die andere Seite des Vertrags**:
sie nimmt eine Nachricht nur an, wenn sie dem Wortlaut entspricht, und
schreibt jede Abweichung in `host.refused`. Dass diese Liste leer bleibt,
ist das Ergebnis des Pruefsatzes - nicht seine Annahme. Eine Zeile prueft
ausserdem, dass `refused` sich ueberhaupt fuellen *kann*.

**Was die Attrappe annimmt und nicht beweisen kann** (ausfuehrlich im Kopf
der Datei):

1. Die Laenge steht in der Reihenfolge des Rechners. Auf einem Rechner mit
   grossem Ende waere es die andere - nicht nachgemessen.
2. Was der Browser als Aufrufparameter uebergibt, steht nur als Annahme.
3. Hier stehen die Fehler**schluessel**, nicht die Saetze. Dass der Text von
   `osk` in der Sprache des Anwenders ankommt, ist nicht geprueft.
4. Die Attrappe antwortet sofort. Ein echtes `osk` wartet auf eine Freigabe
   am Geraet; die Frist der Erweiterung (120 Sekunden) ist nicht gemessen.
5. Das Geraet in `hello` ist erfunden.
6. Der Tresor liegt im Arbeitsspeicher: 768 Eintraege je Profil, die
   Reihenfolge der Liste und ein voller Tresor sind nicht nachgebildet.

## Offen

* **Ein Name mit Umlaut kann nicht hinterlegt werden.** Die Erweiterung
  bringt `münchen.example` und `xn--mnchen-3ya.example` auf dieselbe Form;
  `cleanHost` in `osk` laesst im Feld eines Eintrags aber nur ASCII durch
  und weist `münchen.example` ab. Wer so einen Namen eintraegt, bekommt
  einen Eintrag, der nie passt. Gehoert nach `apps/desktop` und ist diesem
  Strang nicht erlaubt - gemeldet.
* **Kein eigenes Feld fuer den Benutzernamen.** `osk` gibt den *Namen* des
  Eintrags als Benutzernamen heraus (`replyReveal`), weil das Tresorformat
  kein weiteres Feld kennt. Wer einen anderen Benutzernamen braucht als den
  Namen seines Eintrags, kann ihn nicht hinterlegen.
* **Nichts davon ist in einem Browser gelaufen.** Alle Zahlen hier stammen
  aus `node --test`.

## Pruefen

```powershell
node --test apps\web\assets\rules.test.js apps\web\tests\adressen.test.js apps\web\tests\attrappe.test.js
```

```powershell
python tools\check.py
```

Stand 30.10.2026: 40 Tests gruen (vorher 13), 5 Sprachen mit 26 Schluesseln
(vorher 23), 8 JavaScript-Dateien (vorher 5).
