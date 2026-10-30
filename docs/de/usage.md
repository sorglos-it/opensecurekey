# Benutzen

**Stand:** Nichts davon ist an einem echten Stick gelaufen. Bei jeder Funktion steht, wie weit sie geprüft ist.

## Die Regel für alles

- **Nichts kommt heraus, wenn du nicht am Stick drückst.**
- Der Bildschirm am Stick nennt, **was** du freigibst: den Dienst, den Empfänger, den Betrag.
- Was der Stick nicht lesbar zeigen kann, gibt er nicht frei.
- Ein Schadprogramm auf dem Rechner kann fragen. Es kann den Knopf nicht drücken, und es kann nicht ändern, was der Bildschirm sagt.

## Der Knopf

Der Stick hat **einen** Knopf und drei Arten zu drücken.

| Drücken | Dauer | Bedeutung |
| --- | --- | --- |
| kurz | unter 0,8 Sekunden | nächste Seite. Auf der Frageseite: **Nein.** |
| lang | ab 0,8 Sekunden | zur Frageseite. Dort: **Ja.** |
| doppelt | zweimal kurz, schnell hintereinander | alles noch einmal von Seite 1 lesen |

So läuft eine Freigabe:

1. **Lesen.** Jede Seite zeigt den Wortlaut. Mit kurzem Druck blätterst du.
2. **Entscheiden.** Lang drücken zeigt die Frageseite. Dort: lang = Ja, kurz = Nein.
3. **Löschen fragt zweimal.** Zwei lange Drücke auf zwei Frageseiten.

Ein langer Druck auf einer Leseseite gibt **nichts** frei. Er bringt nur die Frageseite. Niemand gibt etwas frei, was er nicht gesehen hat.

Nach 30 Sekunden ohne Druck gilt es als „Nein“.

Lange Namen und Adressen werden **nie abgeschnitten.** Sie laufen über mehrere Seiten. Eine abgeschnittene Empfängeradresse wäre schlimmer als gar kein Bildschirm.

**Stand:** Auf einem echten Bildschirm wurde noch nie etwas angezeigt. Ob 0,8 Sekunden sich in der Hand richtig anfühlen, weiß niemand.

## Bei einer Webseite anmelden (Passkey)

**Stand:** Nie in einem Browser ausprobiert.

1. Auf der Webseite „Mit Sicherheitsschlüssel anmelden“ oder „Passkey“ wählen.
2. Wenn der Rechner nach der PIN fragt: PIN eingeben.
3. Der Bildschirm am Stick nennt den Dienst.
4. Lesen. **Lang drücken** zur Frageseite, dort **lang drücken** für Ja.

Gut zu wissen:

- Ein Passkey gilt **nur für die echte Adresse** der Seite. Auf einer nachgemachten Seite funktioniert er nicht.
- Der Stick versteht Passkeys, die ältere U2F-Anmeldung und die Erweiterung, mit der Programme wie Bitwarden, BitLocker und LUKS einen Schlüssel ableiten (`hmac-secret`).
- **Alte Seiten, die nur U2F können,** lehnen den Stick möglicherweise ab. Er schickt keine Beglaubigung mit.
- Manche Firmenseiten verlangen ein Beglaubigungszertifikat. Das hat der Stick nicht.
- Über NFC braucht jede Freigabe ebenfalls den Knopf.

## Mit dem Telefon antippen (NFC)

**Nur mit USB-Strom.** Der Stick hat keinen Akku. NFC geht nur, solange er im USB-Anschluss steckt: Das Telefon kommt zum Stick, nicht umgekehrt. Den Stick lose ans Handy halten geht nicht mehr. Ob ein reines USB-Ladegerät ohne Rechner genügt, ist nicht geprüft.

**Stand:** Nie mit einem echten Telefon ausprobiert. Die Reichweite ist nicht gemessen.

- Halte den Stick **näher ans Lesegerät als eine Karte.**
- Das liegt an der Bauform, nicht an einem Fehler. Im kleinen Gehäuse ist die Antenne kleiner als bei einer Karte.
- Wie nah genau, wissen wir erst nach einer Messung am fertigen Gerät. Darum steht hier keine Zahl.
- Geht es nicht gleich: Verschiebe den Stick langsam ein Stück, statt fester zu drücken.
- Auch über NFC braucht jede Freigabe den Knopf am Stick.

## Ein Passwort holen

**Stand:** gegen die Test-Attrappe geprüft, nie gegen einen echten Stick.

### In der Oberfläche

1. `osk` starten und die Adresse im Browser öffnen.
2. Den Stick **entsperren** (PIN).
3. Den Eintrag suchen und öffnen.
4. **Zeigen** wählen.
5. Am Stick bestätigen.
6. Mit **Kopieren** legst du es in die Zwischenablage. Sie wird nach etwa **20 Sekunden** wieder geleert.

### Per Befehl

Der PIN wird nie als Parameter übergeben. Er kommt über die Standardeingabe, weil Parameter für andere Programme sichtbar sind.

```powershell
osk unlock
```

```powershell
osk list
```

```powershell
osk get "Mein Konto"
```

Passt ein Name auf mehrere Einträge, rät das Programm nicht. Es sagt, dass der Name nicht eindeutig ist.

### Im Browser ausfüllen lassen

Es gibt eine Browser-Erweiterung. Sie füllt ein Anmeldefeld **nur auf der gespeicherten Adresse.** Für eine andere Adresse verweigert sie das. Das ist der Schutz vor nachgemachten Seiten, kein Komfortfehler. Auf einer unverschlüsselten Seite (ohne https) gibt sie nichts heraus.

**Stand:** Ein Anmeldefeld wurde in einem echten Chromium ausgefüllt, mit der Attrappe. Mit einem Stick nie.

### Öffentliche Teile ohne Druck

Der **öffentliche** Teil eines SSH-Schlüssels, Zertifikats oder PGP-Schlüssels kommt **ohne** Druck am Stick. Das ist Absicht: Würdest du für jeden Namen drücken, drückst du bald, ohne hinzusehen. Dann wäre der eine Druck, der zählt, nichts mehr wert: der vor dem Unterschreiben.

## Einen Code für die Zwei-Faktor-Anmeldung (2FA)

**Stand: offen.**

- Der Stick kann den Startwert für Einmalcodes speichern, als Eintrag „Einmalcode (Zeit)“ oder „Einmalcode (Zähler)“.
- Der Stick hat keinen Akku. Der Rechner stellt die Uhr beim Einstecken (**geplant, noch nicht gebaut**). Geht die Uhr des Rechners falsch, stimmen auch die Codes nicht. Zeitbasierte Codes brauchen die Uhrzeit.
- **Wie der fertige sechsstellige Code in der Oberfläche oder am Stick angezeigt wird, ist in der Technik-Doku nicht beschrieben.** Dazu schreibe ich hier nichts.
- Der Befehl `osk codes` zeigt **nicht** diese Codes, sondern die Liste der Ausstiegszahlen für Skripte.
- KeePassXC und VeraCrypt arbeiten mit einem Yubico-Verfahren. Das fehlt noch: **Beide Programme gehen mit dem jetzigen Stand nicht.**

## Eine Datei oder Nachricht unterschreiben

**Stand:** Rechenkern geschrieben und am Rechner geprüft. Mit einem echten Chipkarten-Programm (GnuPG, OpenSC) nie ausprobiert.

Der Stick verhält sich wie eine **Chipkarte.** Dafür braucht Windows, Linux und macOS keinen Treiber.

- Er kann sich als **PIV-Karte** und als **OpenPGP-Karte** melden.
- Schlüssel liegen im Sicherheitschip. Sie verlassen ihn nie.
- **Jede Unterschrift braucht den Knopf.** Der Bildschirm nennt den Dienst.
- Bei PIV muss die PIN **unmittelbar vor jeder** Unterschrift am Platz „9C“ neu geprüft werden.
- Unterstützt sind elliptische Kurven (P-256 und P-384). **RSA bisher nicht** (der Rechenkern lehnt es ab).

Ablauf:

1. Das Programm (zum Beispiel GnuPG) bitten, mit dem Schlüssel auf der Karte zu unterschreiben.
2. PIN eingeben, wenn gefragt.
3. Am Stick den Namen lesen, **lang drücken**, **lang drücken.**

### Software, die eine Schlüsseldatei verlangt

**Das ist der Punkt, an dem die meisten scheitern. Lies ihn, bevor du suchst.**

**Das Zertifikat gibt der Stick heraus.** Ein Zertifikat ist öffentlich — es weiterzugeben kostet nichts. Für alles, was nur die Bescheinigung braucht, genügt das.

**Den privaten Schlüssel gibt es nicht als Datei. Nie.** Es gibt keine Ausfuhr, keinen Haken dafür, kein Kennwort, das sie freischaltet. Such nicht danach.

Das ist **kein Mangel, sondern der Zweck des Geräts:** Was als Datei vorliegt, ist kopierbar. Ein Schlüssel, den man kopieren kann, ist kein Hardware-Schlüssel mehr.

**Die Folge, offen gesagt:** Software, die zwingend eine Schlüsseldatei verlangt — ein `.pem`, in dem `PRIVATE KEY` steht, oder ein `.pfx` mit Kennwort — **kann dieses Gerät grundsätzlich nicht bedienen.** Nicht mit einem Umweg, nicht mit einer Einstellung.

**Der Ausweg heißt PKCS#11.** Das ist eine festgelegte Schnittstelle für Schlüssel, die in einem Gerät bleiben. Programme, die sie kennen, nehmen statt eines Dateipfads eine **Schlüssel-Adresse** entgegen, die so aussieht:

```
pkcs11:object=mein-schluessel
```

Das können unter anderem `openssl`, der Webserver nginx, Java, viele VPN-Programme und die üblichen Signierwerkzeuge. Wer PKCS#11 kann, braucht keine Datei.

**Wer es nicht kann, bleibt außen vor.** Dann hilft nur: ein Programm nehmen, das PKCS#11 spricht, oder für diesen einen Zweck bei einer Schlüsseldatei auf der Festplatte bleiben — mit allem, was das bedeutet.

Die Bibliothek dafür heißt `osk-pkcs11` (als `.dll` und `.so`). **Stand:** Mit der Attrappe hat `openssl` damit eine Unterschrift als „Verified OK“ geprüft. Mit einem echten Stick nie.

### Per SSH und Git

**Stand:** Mit der Attrappe geprüft. `git push` über den Agenten ging unter Windows und Linux.

1. Den Agenten starten:

```powershell
osk agent
```

2. Die Schlüssel im Tresor anzeigen:

```powershell
osk keys
```

3. Jede Unterschrift bestätigst du am Stick.

Für Programme, die PKCS#11 sprechen, gibt es eine Bibliothek (`osk-pkcs11`, als .dll und .so). Damit hat `openssl` eine Unterschrift als „Verified OK“ geprüft, mit der Attrappe.

### Ein Geheimnis an ein Programm geben

```powershell
osk run NAME=eintrag -- programm
```

Das Geheimnis geht nur an das gestartete Programm. Die Shell daneben sieht es nicht.

## Am Rechner anmelden

**Das geht noch nicht.**

- Anmelden an Windows, Linux oder dem Mac mit dem Stick: nicht gebaut.
- Festplatte entsperren und der Stick als Tastatur (der Stick tippt dein Passwort): nicht gebaut.
- Diese Teile brauchen einmal Administrator-Rechte zum Einrichten. Das ist Sache des Betriebssystems.

Gebaut und mit der Attrappe geprüft sind nur: ssh-Agent, Befehlszeile, PKCS#11, Browser-Erweiterung und eine lokale Schnittstelle für eigene Programme.

## Krypto-Geldbörse (Bitcoin und Ethereum)

**Stand:** Am Rechner geprüft. **Es wurde nie eine echte Zahlung geschickt.**

- Eine Zahlung unterschreibt der Stick nur, wenn **Empfänger, Betrag und Gebühr** auf dem Bildschirm lesbar waren.
- Fehlt eines davon, sagt der Stick **Nein**, ohne den Chip zu fragen.
- **Nicht lesbar** sind unter anderem Verträge, Token-Beträge und Typed Data. Solche Dinge unterschreibt der Stick nur, wenn du am Stick „blind unterschreiben“ **eingeschaltet** hast.
- Es wird nie eine halbe Zahlung unterschrieben. Geht ein Teil nicht, scheitert alles.
- Nicht unterstützt: Taproot-Eingänge.
- Dein Wallet-Satz wird so angelegt, dass er nicht auf einen Zweitstick kopiert wird und nur an gekoppelte Rechner geht.

## Programm nur mit Stick (Dongle) und Server

**Stand:** Nur mit der Attrappe. Die Stick-Seite dazu gibt es noch nicht.

- **Stufe 1** (der Stick sagt „ich bin da“) hält jemanden auf, der ein Programm nur **kopiert.** Wer ein Programm zerlegen kann, entfernt die Prüfung.
- **Stufe 2** (Daten sind mit einem Schlüssel aus dem Chip verschlossen) ist grundsätzlich stärker: Ohne den Stick sind die Daten nicht lesbar.
- Ein Server kann mit dem Stick unterschreiben, statt mit einer Schlüsseldatei. **Ohne Freigabe** steht ein Server still, weil niemand um 3 Uhr nachts drückt. Die Freigabe ist **aus**, nennt Schlüssel, läuft nach **höchstens 24 Stunden** ab und wird protokolliert. **Mit** Freigabe kann ein Schadprogramm auf diesem Server unterschreiben.

Mehr dazu: [how-it-works.md](how-it-works.md).

## Das Protokoll des Sticks

Der Stick führt ein eigenes verschlüsseltes Protokoll. Es zeigt, **welches Programm wann** welches Geheimnis geholt hat. Es steht im Stick, nicht auf dem Rechner. Es fasst die letzten paar hundert Ereignisse.

```powershell
osk log
```

Der Programmname im Protokoll ist eine **Behauptung** des Programms, kein Beweis.

Geheimnisse stehen nie im Protokoll.

## Mehrere Personen an einem Stick

Ein Stick hat bis zu **vier Profile**, jedes mit eigener PIN. Die PIN des einen öffnet kein anderes. Ein gesperrtes Profil zeigt nichts, nicht einmal die Namen der Einträge. Platz: **768 Einträge je Profil**, **3072 im Stick.**

**Eine Grenze, die nicht am Stick liegt:** Die Anmeldung mit Passkeys (FIDO) kennt **keine Profile.** Diese Technik geht davon aus, dass ein Stick einem Menschen gehört. Der Stick kann daran nichts ändern.

Was das im Alltag bedeutet:

- Wechselst du das Profil, musst du dich **bei den Diensten neu anmelden.** Die Passkeys des anderen Profils stehen dort nicht zur Verfügung.
- Ein Profil ist also kein zweites Gesicht für denselben Dienst, sondern ein getrennter Satz Zugänge.
- Wer zwischen zwei Profilen oft hin und her will, fährt mit **zwei Sticks** besser.

**Stand:** Nie in einem Browser ausprobiert.

## Eintragsarten

Zugang, Einmalcode (Zeit), Einmalcode (Zähler), SSH-Schlüssel, Zertifikat, PGP-Schlüssel, API-Schlüssel, Wiederherstellungscodes, Wallet-Satz, Lizenzschlüssel, Bankkarte, Ausweis, WLAN, Notiz, Datei.

Jeder Eintrag trägt drei Markierungen, die du selbst setzt:

| Markierung | Wirkung |
| --- | --- |
| darf gespiegelt werden | geht auf deinen Zweitstick, und nur dorthin |
| nur gekoppelte Rechner | wird nur an einen gekoppelten Rechner herausgegeben |
| schreibgeschützt | lässt sich nicht ändern oder löschen, auch nicht von dir, bis du es aufhebst |

## Eine Speicherkarte am Stick

**Das gibt es noch nicht.** Geplant ist eine Speicherkarte für eigene Dateien.

Zwei Dinge dazu solltest du kennen, **bevor** es sie gibt, weil beide Geheimnisse kosten können:

- Von **drei Betriebsarten** verschlüsselt eine **nicht.** Sie siegelt nur, und jeder openSecureKey kann lesen.
- Eine Unterschrift beweist, dass der Inhalt **unverändert** ist — nicht, **von wem** er kommt.

Beides steht ausführlich in [how-it-works.md](how-it-works.md), Abschnitt „Der verschlüsselte Datenträger“.
