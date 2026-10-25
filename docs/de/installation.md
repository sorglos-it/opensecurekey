# Einrichten

**Stand:** Den Stick gibt es noch nicht. Diese Schritte beschreiben den geplanten Ablauf. Das Programm `osk` läuft bisher nur gegen eine Test-Attrappe. Einen Einrichtungsassistenten, der die Schritte 3 bis 5 führt, gibt es noch nicht: Die Texte stehen, das Programm dazu nicht.

## Was du brauchst

- den Stick
- einen Rechner mit Windows oder Linux (macOS: nie ausprobiert)
- einen Browser
- Zeit, die Frage in Schritt 3 in Ruhe zu lesen

Du brauchst **keine Installation** und **keine Administrator-Rechte** für das Programm `osk`.

## Schritt 1: Das Programm holen

- `osk` ist **eine Datei** pro Betriebssystem.
- Es legt seine Einstellungen **neben sich** ab, nicht im System.
- Ein fertiges Programm zum Herunterladen gibt es noch nicht. Es ist kein Release gebaut. Wer es selbst bauen will, liest [development.md](development.md).

## Schritt 2: Stick einstecken, Programm starten

1. Stick in einen USB-Anschluss stecken.
2. `osk` starten.
3. Das Programm schreibt eine Adresse, zum Beispiel `http://127.0.0.1:8101/`.
4. Diese Adresse im Browser öffnen.
5. Unter **Geräte** muss dein Stick stehen. Steht dort „Kein Gerät gefunden“, hilft [troubleshooting.md](troubleshooting.md).

Die Adresse führt nur auf deinen eigenen Rechner. Von außen kommt niemand an die Seite.

## Schritt 3: Die eine Frage

Die Oberfläche fragt dich zuerst:

> **Soll es später einen Ersatz-Stick geben können?**

**Diese Antwort lässt sich nie mehr ändern.** Das ist kein Vorsicht-Hinweis, sondern Technik: Ein Geheimnis, das im Sicherheitschip entstanden ist, kommt nie wieder heraus. Es lässt sich also später nicht auf einen zweiten Stick kopieren.

Es gibt zwei Wege. Beide haben einen Preis.

### Weg A: „Ja, Ersatz möglich“

- **Das bekommst du:** Du kannst später einen zweiten Stick anlegen, der dasselbe kann. Verlierst du einen, nimmst du den anderen und kommst überall wieder hinein.
- **Das zahlst du:** Dein Hauptgeheimnis wird beim Einrichten **einmal außerhalb des Sicherheitschips** erzeugt. In diesem einen Moment könnte ein Schadprogramm auf deinem Rechner es mitlesen. Danach nie wieder.

### Weg B: „Nein, nur dieser Stick“

- **Das bekommst du:** Alles bleibt für immer im Sicherheitschip. Kein Geheimnis verlässt ihn, zu keinem Zeitpunkt.
- **Das zahlst du:** Es gibt **keinen Ersatz.** Geht der Stick verloren oder kaputt, kommst du in die damit angelegten Zugänge nicht mehr hinein. Bei jedem Dienst müsstest du von vorn anfangen.

### Was das Programm empfiehlt

- **Für den Alltag:** Ersatz möglich. Wer seinen einzigen Stick verliert, verliert sonst alle Zugänge.
- **Für eine Krypto-Geldbörse:** Dafür kannst du später ein **zweites Profil ohne Ersatz** anlegen.

Keiner der beiden Wege ist grundsätzlich der sicherere. Es hängt davon ab, was du mehr fürchtest: den einen Moment auf dem Rechner, oder den Verlust des einzigen Sticks.

### Umentscheiden

- Das geht nur so: **neues Profil anlegen** und dich bei **jedem Dienst neu anmelden.**
- Ein Stick fasst bis zu **vier Profile.** Jedes hat eine eigene PIN.

Bevor es weitergeht, musst du ankreuzen: „Ich habe verstanden, dass sich das nicht mehr ändern lässt.“

## Schritt 4: PIN

- Ohne PIN geht nichts. Wer den Stick findet, kommt ohne PIN an nichts.
- Die PIN hat mindestens **4 Zeichen.** Umlaute zählen je als ein Zeichen.
- Wie viele Fehlversuche es insgesamt gibt, bevor die PIN dauerhaft gesperrt ist, **steht noch nicht fest.** Drei falsche in Folge sperren zunächst nur bis zum Abziehen des Sticks. Mehr dazu in [troubleshooting.md](troubleshooting.md).
- Die Fehlversuche zählt der **Sicherheitschip.** Den Zähler kann niemand zurücksetzen, indem er den Speicher löscht.
- **Offen:** Wo genau du die PIN zum ersten Mal festlegst, ist noch nicht beschrieben.

## Schritt 5: Absichern, bevor du den Stick benutzt

Das ist der wichtigste Schritt. Tu ihn **gleich**, nicht „später“.

1. **Zweitstick anlegen** (nur bei Weg A). Siehe unten.
2. **Sicherung speichern.** Siehe unten.
3. **Notfallzettel schreiben.** Siehe unten.

Dass der Einrichtungsassistent dich zu einem Zweitstick **oder** einem gedruckten Notfallzettel zwingt, ist geplant. Gebaut ist es noch nicht. Mach beides.

### Zweitstick koppeln

1. Beide Sticks anstecken.
2. In der Oberfläche **Zweitgerät koppeln** wählen.
3. **Beide Sticks zeigen dieselbe sechsstellige Zahl.** Vergleiche sie.
4. Sind die Zahlen **gleich:** an beiden Sticks drücken.
5. Sind sie **verschieden:** nicht drücken. Dann sitzt jemand dazwischen. Es wird dann nichts übertragen.

Was übertragen wird:

- Nur Einträge mit der Markierung **„darf gespiegelt werden“.**

Was **nicht** übertragen wird:

- alles andere
- ein Krypto-Wallet-Satz: Er wird so angelegt, dass er nicht auf einen zweiten Stick kopiert wird

Gekoppelt wird nur über USB am Rechner. Beide Sticks stecken gleichzeitig. Eine Übertragung von Stick zu Stick über Funk gibt es nicht, weil der Stick keinen Akku hat.

Der Rechner, an dem beide Sticks stecken, sieht nur verschlüsselte Daten.

**Bekannte Lücke:** Der öffentliche Teil von SSH-Schlüsseln, Zertifikaten und PGP-Schlüsseln geht bisher **nicht** mit auf den Zweitstick und steht nicht in der Sicherung. Der Schlüssel funktioniert dort, aber ein Programm kann ihn erst auflisten, wenn der öffentliche Teil neu gespeichert wurde.

### Sicherung speichern

1. In der Oberfläche **Sicherung** öffnen, **Sicherung speichern.**
2. Du wählst ein **Hauptkennwort.** Es hat mindestens **zwölf Zeichen.** Nicht Ziffern, sondern Zeichen.
3. Nimm den Vorschlag des Sticks: **acht Wörter.** Das ist schwerer zu erraten als fast alles, was Menschen sich ausdenken, und leichter auf Papier zu schreiben.
4. Die Datei ist verschlüsselt. Der Rechner kann sie nicht lesen.
5. Lege die Datei auf eine Platte oder ein NAS.

**Wichtig:**

- Bewahre **Datei und Hauptkennwort getrennt** auf. Beide zusammen öffnen alles.
- Mit der Datei und dem Hauptkennwort lässt sich ein **ganz neuer Stick** einrichten. Die Datei hängt nicht an dem Stick, der sie geschrieben hat.
- Das Programm warnt beim Laden: „Das Laden ersetzt den Inhalt des Geräts.“ Lade also nicht aus Versehen.

### Notfallzettel

Ein Notfallzettel ist ein Blatt Papier, das du **an einem sicheren Ort** aufbewahrst. Er enthält dein **Hauptkennwort.**

Wenn du willst, dass auch andere im Notfall herankommen, geht es mit **Umschlägen:**

- Der Stick teilt den Schlüssel der Sicherung in mehrere Teile, zum Beispiel fünf.
- Jeder Teil ist **eine Zeile Text.** Drucke sie aus, je Person ein verschlossener Umschlag.
- **Drei von fünf** zusammen öffnen die Sicherung, ohne Hauptkennwort.
- **Ein einzelner Umschlag verrät nichts.** Nicht einmal einen Buchstaben.
- Ein falsch abgetippter Teil fällt auf. Es kommt kein falsches Ergebnis heraus.

Zahl der Teile und wie viele nötig sind, legst du fest.

**Offen:** Ein vom Programm erzeugtes, fertig gedrucktes „Notfallblatt“ ist als Funktion nicht beschrieben. Schreib dein Hauptkennwort und den Fundort der Sicherung selbst auf.

## Update

### Firmware (die Software im Stick)

1. In der Oberfläche **Firmware** öffnen, **Firmware aufspielen.**
2. Den Stick **während des Schreibens nicht abziehen.**
3. Der Stick nimmt **nur unterschriebene Abbilder** an.

Dein Tresor bleibt dabei erhalten. Eine ältere Fassung des Tresors wird gelesen und beim Benutzen neu geschrieben. Ein Tresor von einer **neueren** Firmware wird gar nicht erst geöffnet, statt falsch gelesen zu werden.

**Stand:** Das Schreiben auf einen Stick ist nie erprobt. Das Werkzeug dazu (`tools/flash.py`) ist ungeprüft.

### Das Programm `osk`

Es gibt noch keine Update-Funktion. Du ersetzt die Datei durch eine neuere.

## Entfernen

- **Programm:** die Datei `osk` löschen. Die Einstellungen liegen daneben, nicht im System.
- **Stick leeren:** Siehe [troubleshooting.md](troubleshooting.md), Abschnitt „Zurücksetzen“. **Das ist endgültig.**
