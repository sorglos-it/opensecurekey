# Wie der Stick innen arbeitet

**Stand:** Alles hier ist Entwurf und am Rechner geprüft. Nichts lief auf einem echten Stick.

## Zwei Chips, zwei Aufgaben

- **Der Funkchip (nRF52840)** redet mit dem Rechner und dem Telefon.
- **Der Sicherheitschip (SE051)** hält die Geheimnisse. Er gibt keinen Schlüssel heraus.

Warum zwei? Der Schutz des Funkchips wurde in der Vergangenheit schon mit Spannungsstößen ausgehebelt. Ein kaputter Funkchip verrät deshalb allein noch nichts.

Dazu kommen:

- **Speicher (64 MB):** der Tresor, verschlüsselt, und das Protokoll.
- **Bildschirm:** zeigt, was du freigibst.
- **Ein Knopf:** das ist deine Freigabe.
- **Akku:** für NFC und Bluetooth ohne Rechner, und damit die Uhr weiterläuft.

## Wege zum Rechner

- USB als Anmelde-Gerät (FIDO2)
- USB als Chipkarten-Leser (PIV, OpenPGP)
- USB als Tastatur (**geplant, noch nicht gebaut**)
- USB-Kanal für das Verwaltungsprogramm
- USB-Laufwerk, nur lesbar, mit den Programmen drauf (**geplant**)
- NFC
- Bluetooth, nur für das Verwaltungsprogramm

## Warum ein Akku?

Der Funkchip kann sich keinen Strom aus dem NFC-Feld holen. Ein anderer Stick kann das, weil dort ein einfacher Kartenchip antwortet. Hier antwortet die Firmware. Also braucht der Stick eigenen Strom.

- Der Akku lädt über USB.
- **Laufzeit:** Auf dem Papier etwa **7 Monate** bei zehn Benutzungen am Tag. Die Rechnung steckt voller Annahmen, zum Beispiel für die Selbstentladung. **Gemessen ist nichts.**
- Ob der Akku eine NFC-Sitzung durchhält, ist offen.

## Die zwei Wege der Einrichtung

Das ist die Technik hinter der einen Frage aus [installation.md](installation.md).

### „Nur dieser Stick“

- Für jeden Zugang entsteht im Sicherheitschip **ein eigenes Schlüsselpaar.** Der private Teil existiert **nirgends sonst.**
- Es gibt nichts zu stehlen und nichts zu entpacken.
- Der Preis: Jeder Zugang belegt Platz im Chip. Die Zahl der Dienste ist **endlich.** Wie viele es sind, hängt von der gelieferten Chip-Ausführung ab (46 oder 104 kB) und ist **nicht gemessen.** Das Programm soll rechtzeitig warnen.
- Es gibt kein zweites Gerät für diese Zugänge.

### „Ersatz möglich“ (gespiegelt)

- Ein Hauptgeheimnis wird beim Koppeln **außerhalb beider Chips** erzeugt und in beide geschrieben.
- Der Preis: Es liegt kurz im Speicher des Funkchips.
- Nach dem Schreiben kann die Firmware es **nur benutzen,** nicht lesen, nicht überschreiben, nicht herausholen. Das ist keine Disziplin, sondern so legt es der Chip fest.

### Warum nicht änderbar?

Ein Geheimnis, das im Chip entstanden ist, verlässt ihn nie, auch nicht verschlüsselt. Es lässt sich also nie nachträglich spiegeln. Ein gespiegeltes Hauptgeheimnis muss **vor** seiner Entstehung am richtigen Ort entstehen.

Keiner der Wege ist grundsätzlich sicherer. Ein verlorener einziger Stick zerstört mehr, als ein gestohlener meist anrichtet.

## Die PIN

- Der Stick bekommt die PIN nie im Klartext, nur einen Prüfwert daraus.
- Der **Sicherheitschip** prüft sie und zählt die Fehlversuche. Den Zähler setzt kein Löschen des Speichers zurück.
- Drei falsche PINs in Folge sperren, bis du den Stick abziehst.
- PIV und OpenPGP benutzen **dieselbe** PIN. Die Verwaltungs-PINs (PUK, Admin-PIN) liegen im Tresor, jede mit eigenem Zähler.
- Jeder Versuch mit einer Verwaltungs-PIN fragt am Bildschirm. Das ist richtig so.

## Der Tresor

- Jedes Geheimnis ist einzeln verschlüsselt (AES-256-GCM).
- **Stromausfall:** Jede Änderung wird in fester Reihenfolge geschrieben. Nach jedem Ausfall ist der alte **oder** der neue Zustand da, und alles ist lesbar. Das wurde an jeder einzelnen Schreibstelle getestet.
- Der Stick verteilt das Schreiben über den ganzen Speicher. Er nutzt sich so nicht an einer Stelle ab.
- **Suchen ohne Öffnen:** Manche Einträge tragen eine kurze Marke. Sie sagt, zu welchem Dienst der Eintrag gehört. So findet der Stick einen Passkey schnell. Die Marke ist selbst verschlüsselt.
- Die Suche muss alle Teile einer Liste durchgehen. **Wie lange das auf dem echten Chip dauert, ist nicht gemessen.**
- Eine Firmware-Aktualisierung verliert keine Daten.

## Die Sicherung

- Eine verschlüsselte Datei, aus der sich ein neuer Stick einrichten lässt.
- Ihr Schlüssel kommt aus dem **Hauptkennwort,** nicht aus dem Chip. Sonst hinge die Sicherung an dem Stick, der verloren ging.
- Auf dem Stick sind die Rechenwerte klein gehalten. Die Stärke muss deshalb vom Hauptkennwort kommen. Darum acht Wörter.
- Die Umschläge teilen den Schlüssel der Datei auf (Shamir).

## Ein Zweitstick

- Beide Sticks erzeugen einen kurzlebigen Schlüssel und tauschen nur den öffentlichen Teil.
- Daraus entsteht die sechsstellige Zahl. Wer dazwischen sitzt, erzeugt **zwei verschiedene** Zahlen. Das wurde getestet.
- Ohne bestätigte Zahl verlässt kein Byte den Stick.

## Was den Stick vor wem schützt

| Wer | Was er bekommt |
| --- | --- |
| findet den Stick | nichts ohne die PIN |
| öffnet den Stick und prüft die Platine | Die Leitung zwischen den Chips ist verschlüsselt, **sobald die Werksschlüssel ersetzt sind** (siehe unten). Der Speicher ist verschlüsselt. |
| hat ein Schadprogramm auf dem Rechner | Jede Freigabe braucht den Knopf, und der Bildschirm nennt den Dienst. Das Programm kann nicht drücken. |
| betreibt eine nachgemachte Webseite | Passkeys sind an die echte Adresse gebunden. Die Erweiterung füllt nur auf der gespeicherten Adresse. |
| leitet NFC zwischen Telefon und Stick um | braucht trotzdem den Druck am Stick |
| kopiert einen Server | Der Schlüssel des Servers bleibt im Chip. **Nur wenn der gelieferte Chip das kann.** |

## Was der Stick nicht kann

Hier steht es ohne Beschönigung.

### Er schützt nicht vor

- **Jemandem, der dich zwingt, den Knopf zu drücken.** Das kann kein Gerät erkennen.
- **Veränderter Firmware.** Der Sicherheitschip schützt vor Angriffen auf den Chip, nicht vor einer Firmware, die dich anlügt. Dagegen hilft nur der unterschriebene Start. Er muss da sein, **bevor** der Leseschutz eingeschaltet wird.
- **Einem Mitlesen der PIN.** Du tippst sie am Rechner, also kann ein Schadprogramm sie lesen. Benutzen kann es sie nicht, weil jede Freigabe den Knopf braucht.
- **Dem Verlust von allem.** Siehe unten.

### Fingerabdruck

- Die erste Ausbaustufe hat **keinen Fingerabdrucksensor.** Du gibst frei mit PIN und Knopf.
- Der Platz für einen Sensor ist auf der Platine vorgesehen.
- Bei FIDO2 **ersetzt** ein Fingerabdruck die PIN. Er ist kein zusätzlicher Schutz.
- **Eine Lebenderkennung ist bei keinem geprüften Sensor belegt.** Wer einen abgeformten Finger fürchtet, gewinnt durch keinen dieser Sensoren etwas.
- Fingerabdruck-Daten wären nie im Tresor und könnten nie auf einen Zweitstick gehen.
- Einen Finger einlernen kann kein Browser und keine Webseite. Das geschähe am Stick selbst.

### Verlust

**Wer Hauptkennwort und alle Sticks verliert, verliert alles.**

Verlierst du Stick, Zweitstick, Sicherungsdatei und Umschläge zugleich, ist der Inhalt weg. Niemand kann helfen. Es gibt keine Hintertür, die man stehlen könnte.

### Weiteres, das man wissen sollte

- **Kein Test von außen.** Niemand außerhalb des Projekts hat die Verschlüsselung angesehen.
- **Kein FIDO-Prüftest** wurde gelaufen.
- **Der Chip kommt mit öffentlich bekannten Werksschlüsseln.** Bei der ersten Inbetriebnahme **müssen** sie ersetzt werden. Bis dahin ist die Leitung zwischen den Chips nicht geschützt. Das ist beschrieben und eingeplant, aber **nie an einem Chip ausgeführt.**
- **Wer den Speicher direkt umschreiben kann,** kann nichts lesen. Er kann aber einen älteren Zustand zurückspielen. Dagegen fehlt noch ein Zähler im Chip.
- Der **Unterschriftszähler** der Anmeldung beginnt bei jedem Einschalten bei null. Manche Seiten könnten das für einen kopierten Schlüssel halten.
- Der Stick schickt eine **Selbstbeglaubigung,** kein Zertifikat des Herstellers. Das ist gut für den Datenschutz, aber manche Firmenseiten verlangen mehr.
- Ob der gelieferte Chip **Schlüsselaustausch** (ECDH) erlaubt, hängt von seiner Werkseinstellung ab. Erlaubt er es nicht, **fällt der Serverbetrieb mit Schlüssel im Chip weg.** Das Unterschreiben geht weiter. Das Programm sagt es dann ehrlich, statt wie ein PIN-Fehler zu wirken.
- **Mac:** nie ausprobiert.
- **Speicherplatz in der Firmware** ist knapp. Ob alle Funktionen hineinpassen, ist offen.

## Programm und Stick reden miteinander

- Das Programm `osk` spricht mit dem Stick über einen eigenen USB-Kanal.
- Jede Antwort des Sticks wird als feindlich behandelt: strenge Formate, harte Grenzen für Größe und Tiefe, und die Reihenfolgenummer muss passen.
- Die lokale Oberfläche lauscht nur auf deinem Rechner. Jede Anfrage muss von der eigenen Adresse kommen. Eine fremde Seite im selben Browser kommt nicht heran.
- Jede Anfrage nennt den Namen des Programms. Der Stick schreibt ihn ins Protokoll. Er bleibt ein Name, kein Beweis.
