# Wenn etwas nicht klappt

**Stand:** Nichts davon wurde an einem echten Stick erlebt. Die Meldungen stammen aus dem Programm. Allgemeine Tipps sind als solche markiert.

## Der Stick ist weg

Das ist der Abschnitt, auf den es ankommt. Lies ihn **jetzt,** nicht erst, wenn es passiert ist.

### Erst: Was kann der, der den Stick findet?

- **Nichts, ohne die PIN.**
- Die Fehlversuche zählt der Sicherheitschip. Den Zähler setzt kein Löschen zurück.
- Auch mit der PIN braucht jede Freigabe den Knopf am Stick.
- Es gibt **keine Fernsperre.** Davon steht nichts in der Technik-Doku.
- Allgemeiner Rat, nicht vom Stick: Bist du unsicher, ändere die Passwörter der wichtigsten Konten.

### Dann: Was hast du? Such deinen Fall.

#### Fall 1: Du hast einen Zweitstick (Weg „Ersatz möglich“)

1. Nimm den Zweitstick.
2. Er kann dasselbe wie der verlorene, **für alles, was als „darf gespiegelt werden“ markiert war.**
3. Dein Wallet-Satz war nicht darunter. Er geht nie auf einen Zweitstick.
4. Lege so bald wie möglich einen **neuen** Zweitstick an. Sonst hast du wieder nur einen.
5. Der öffentliche Teil von SSH-, Zertifikats- und PGP-Schlüsseln ist bisher **nicht** auf dem Zweitstick. Speichere ihn neu, sonst kann ein Programm den Schlüssel nicht auflisten.

#### Fall 2: Du hast eine Sicherung und das Hauptkennwort

1. Besorge einen neuen Stick.
2. Richte ihn ein. Siehe [installation.md](installation.md).
3. Lade die Sicherung. Dafür brauchst du die Datei **und** das Hauptkennwort.
4. Hast du das Hauptkennwort nicht mehr, aber Umschläge: **Drei von fünf** (oder die Zahl, die du gewählt hast) öffnen die Datei ohne Hauptkennwort.
5. Beachte: Das Programm warnt, dass das Laden den Inhalt des Geräts ersetzt.

**Offen:** Ob ein neuer Stick aus der Sicherung auch deine **Passkeys** wieder benutzen kann, steht in der Technik-Doku nicht fest. Verlass dich bei Passkeys auf den Zweitstick.

#### Fall 3: Du hast nichts davon, und du wähltest „nur dieser Stick“

- Für die damit angelegten Zugänge gibt es **keinen Ersatz.**
- Bei jedem Dienst musst du von vorn anfangen, über den Weg, den der Dienst für verlorene Zugänge anbietet.
- Der Stick selbst kann dir dabei nicht helfen.

#### Fall 4: Alles weg: beide Sticks und das Hauptkennwort

- **Wer Hauptkennwort und alle Sticks verliert, verliert alles.**
- Niemand kann den Inhalt zurückholen. Auch das Projekt nicht. Es gibt keine Hintertür, die man stehlen könnte.

### So sorgst du vor

Diese vier Dinge, am besten gleich bei der Einrichtung:

1. **Zweitstick** koppeln (nur bei Weg A).
2. **Sicherung** speichern, getrennt vom Hauptkennwort lagern.
3. **Notfallzettel:** Hauptkennwort auf Papier, an einem sicheren Ort. Dazu notierst du, wo die Sicherung liegt.
4. **Umschläge** für Menschen, denen du vertraust: ein Teil je Umschlag, drei von fünf öffnen.

Bewahre **nie** alles am selben Ort auf. Datei und Hauptkennwort zusammen öffnen alles.

Die Reihenfolge deiner Sicherheit:

- Zweitstick + Sicherung + Zettel: eine Panne ist kein Drama.
- Nur Zweitstick: Brand oder Diebstahl der Tasche nimmt beide mit.
- Nichts davon: Ein verlorener Stick ist ein Totalverlust.

## Der Stick wird nicht erkannt

Im Programm unter **Geräte** steht „Kein Gerät gefunden.“

Allgemeine Tipps (nicht am Stick erprobt):

1. Anderen USB-Anschluss versuchen.
2. Anderes Kabel oder direkt an den Rechner, ohne Verteiler.
3. Stick abziehen, einstecken, Programm neu starten.
4. Ist der Akku womöglich leer? Laden lassen.

Was sonst zählt:

- **macOS:** Ohne die besondere Bauweise (cgo) startet das Programm zwar, **findet aber keinen echten Stick** und sagt das auch.
- Möchtest du den Test-Stick benutzen, trage seine Adresse in den Einstellungen ein. Siehe [settings.md](settings.md).
- Die Kennung des Sticks (Hersteller- und Produktnummer) ist **vorläufig** und mit der Firmware noch nicht abgestimmt.

Meldungen dazu:

| Meldung | Bedeutung | Was tun |
| --- | --- | --- |
| Kein Gerät gefunden. | Es antwortet kein Stick. | Siehe oben. |
| Die Geräte dieses Rechners lassen sich nicht auflisten. | Das System verweigert die Liste. | Programm neu starten. Hilft das nicht, melde es mit dem Text in Klammern. |
| Das Gerät lässt sich nicht öffnen. | Mögliche Ursache: Ein anderes Programm benutzt den Stick, oder das System erlaubt es nicht. | Andere Programme schließen, die den Stick benutzen. |
| Vom Gerät lässt sich nichts lesen. / Zum Gerät lässt sich nichts schreiben. | Die Verbindung ist abgebrochen. | Stick neu einstecken. |
| Das Gerät hat nicht rechtzeitig geantwortet. / zu lange gebraucht. | Der Stick wartet vielleicht auf deinen Druck. | Schau auf den Bildschirm am Stick. |
| Die Verbindung zum Gerät ist gestört. | Wie oben. | Stick neu einstecken. |
| Dieses Betriebssystem wird noch nicht unterstützt. | Es gibt noch keinen Weg für dieses System. | Windows oder Linux nehmen. |
| Das Gerät spricht eine andere Protokollfassung. | Programm und Firmware passen nicht zusammen. | Das Programm oder die Firmware aktualisieren. |

## Die PIN ist gesperrt

- **Drei falsche PINs in Folge** sperren den Stick, bis du ihn **abziehst und wieder einsteckst.** Danach darfst du es erneut versuchen.
- Der Zähler liegt im Sicherheitschip. Neu aufspielen oder Löschen des Speichers setzt ihn **nicht** zurück.
- **Wie viele Fehlversuche es insgesamt gibt,** bevor die PIN für immer gesperrt ist, und was dann geschieht, steht in der Technik-Doku **nicht.** Rate deshalb nicht. Such erst die PIN.
- Für die Chipkarten-Seite (PIV, OpenPGP) gibt es zusätzlich einen PUK und eine Admin-PIN. Sie liegen im Tresor, jede mit eigenem Zähler.

| Meldung | Bedeutung | Was tun |
| --- | --- | --- |
| Das Gerät ist gesperrt. | Du hast noch nicht entsperrt. | **Entsperren.** Die Freigabe fragt der Stick an seinem eigenen Bildschirm. |
| Die Freigabe am Gerät fehlt. | Du hast nicht (oder mit „Nein“) bestätigt, oder es dauerte über 30 Sekunden. | Neu versuchen. Lang drücken, dann lang drücken. |

### Zurücksetzen

Im Notfall lässt sich der Anmelde-Teil (FIDO) zurücksetzen.

- **Das ist endgültig.** Das Hauptgeheimnis wird neu, **jede alte Anmeldung ist wertlos.** Du musst dich bei jedem Dienst neu anmelden.
- Zurücksetzen geht **nicht über NFC.**
- Löschen am Stick fragt zweimal.
- Ob das auch eine gesperrte PIN löst, ist in der Technik-Doku nicht beschrieben.

## Der Browser nimmt den Stick nicht an

**Stand:** Nie in einem Browser ausprobiert. Diese Liste kennt nur die **bekannten Grenzen.**

- **Alte Seiten, die nur U2F können,** nehmen den Stick womöglich nicht an. Er schickt kein Beglaubigungszertifikat mit.
- **Firmenseiten mit Pflicht-Zertifikat** lehnen ihn womöglich ab. Er beglaubigt sich selbst.
- Der Stick nennt sich **FIDO 2.0 und 2.1.** Befehle zur Verwaltung von Passkeys (Auflisten, Löschen) und zum Einlernen von Fingerabdrücken im Browser **gibt es nicht.**
- Der Stick meldet **keine eingebaute Nutzerprüfung** (`uv`), solange kein Sensor drin ist. Das ist richtig so. Die PIN wird getrennt gemeldet.
- Über **Bluetooth** gibt es **keine** Anmeldung im Browser. Browser unterstützen FIDO über Bluetooth nicht zuverlässig. Nimm USB oder NFC.
- Der **Unterschriftszähler** beginnt bei jedem Einschalten bei null. Eine Seite, die auf wachsende Zähler besteht, könnte den Stick für eine Kopie halten.
- Die **Kennung des Herstellers** (AAGUID) ist vorläufig.

Meldungen der Browser-Erweiterung:

| Meldung | Bedeutung | Was tun |
| --- | --- | --- |
| Dieser Eintrag gehört zu einer anderen Adresse. | Der Eintrag ist für eine andere Seite gespeichert. | **Das ist der Phishing-Schutz.** Prüfe die Adresse. Stimmt sie, ändere die Adresse am Eintrag. |
| Diese Seite ist nicht verschlüsselt. Hier geht kein Geheimnis hin. | Die Seite nutzt kein https. | Nicht ausfüllen. |
| Der Kanal zum Browser ist abgerissen. | Die Erweiterung hat die Verbindung verloren. | Browser neu starten. |
| Die Anmeldedatei ließ sich nicht schreiben. | Die Erweiterung konnte sich nicht eintragen. | Rechte im Ordner prüfen. |
| Das ist keine Kennung einer Erweiterung. / Diesen Browser kenne ich nicht. | Falsche Angabe beim Eintragen. | Kennung und Browsername prüfen. |

## Meldungen bei Einträgen und Sicherung

| Meldung | Bedeutung | Was tun |
| --- | --- | --- |
| Es gibt schon einen Eintrag mit diesem Namen. | Doppelter Name. | Anderen Namen wählen. |
| Auf dem Gerät ist kein Platz mehr. | Der Speicher des Profils ist voll (768 Einträge). | Alte Einträge löschen. |
| Das Gerät kennt diesen Eintrag nicht. | Eintrag gelöscht oder falsch benannt. | Liste neu laden. |
| Der Name passt auf mehrere Einträge. | Nicht eindeutig. | Genauer benennen. |
| Dieses Gerät kann das noch nicht. | Die Funktion fehlt in dieser Firmware. | Firmware aktualisieren, falls es eine neuere gibt. |
| Die Sicherungsdatei ist leer oder zu groß. | Falsche Datei. | Andere Datei wählen. |
| Die Daten auf dem Gerät haben nicht die erwartete Form. | Die Daten sind beschädigt, oder sie stammen aus einer **neueren** Firmware. Eine neuere Fassung wird nie falsch gelesen, sondern gar nicht geöffnet. | Firmware aktualisieren. Hilft das nicht, Sicherung laden. |
| Das Firmware-Abbild ist leer. | Keine Datei gewählt. | Datei wählen. |
| Die Einstellungsdatei lässt sich nicht lesen. / ist zu groß. / enthält einen Fehler. | Die Datei `config_osk.json` ist beschädigt. | Löschen. Das Programm nimmt dann die Vorgaben. |

## Meldungen beim ssh-Agenten und den Schnittstellen

| Meldung | Bedeutung | Was tun |
| --- | --- | --- |
| Dieser Platz ist schon belegt. | Ein anderer Agent oder Dienst läuft schon. | Den anderen beenden. |
| Der Schlüssel ist mit einem Passwort verschlüsselt. | Der SSH-Schlüssel im Tresor ist noch geschützt. | Entschlüsselt in den Tresor legen. |
| Dieser Schlüssel ist zu kurz und damit nicht mehr sicher. | Schwacher Schlüssel. | Neuen Schlüssel anlegen. |
| Diese Schlüsselart kann ich nicht. | Nicht unterstützt. | Andere Art wählen. |
| Nach diesem Schlüssel wurde gefragt, aber er liegt nicht im Tresor. | Das Programm verlangt einen Schlüssel, den du nicht hast. | Schlüssel in den Tresor legen. |
| Der Zugriff wurde nicht freigegeben. / Es kam keine Antwort auf die Frage nach der Freigabe. | Du hast abgelehnt oder nichts gedrückt. | Neu versuchen. |

Die Ausstiegszahlen für Skripte zeigt:

```powershell
osk codes
```

## Meldungen beim Dongle-Schutz

| Meldung | Bedeutung | Was tun |
| --- | --- | --- |
| Kein Sicherheitsschlüssel gefunden. Stecken Sie ihn ein. | Der Stick steckt nicht. | Einstecken. |
| Es steckt ein anderer Sicherheitsschlüssel als der eingetragene. | Das Programm gehört zu einem anderen Stick. | Den richtigen Stick einstecken. |
| Die Datei lässt sich nicht öffnen: falscher Schlüssel oder verändert. | Der Stick passt nicht zur verschlossenen Datei, oder die Datei wurde verändert. | Den Stick nehmen, der sie verschlossen hat. |
| Die Lizenz ist abgelaufen. | Die Lizenz war echt und ist vorbei. | Neue Lizenz holen. |
| Auf diesem Sicherheitsschlüssel liegt keine Lizenz. | Keine Lizenz für das Produkt. | Lizenz auf den Stick legen. |
| Dieser Chip kann keinen Schlüsselaustausch. | Der Chip wurde ab Werk so eingestellt. Serverbetrieb mit Schlüssel im Chip entfällt. Unterschreiben geht weiter. | Das lässt sich nicht ändern. |

## Das geht noch nicht

Damit du nicht lange suchst:

- **KeePassXC und VeraCrypt** mit dem Stick
- **Anmelden am Rechner,** Festplatte entsperren, der Stick als Tastatur
- ein fertig gedrucktes Notfallblatt vom Programm
- **macOS**

## Wenn du es nicht findest

Melde es im Projekt auf GitHub (sorglos-it/opensecurekey) und gib dabei an: Betriebssystem, Version von `osk`, die genaue Meldung, und was du vorher getan hast. Passwörter und PINs gehören nie in eine Meldung.
