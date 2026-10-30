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
- **Kein Akku:** Der Stick läuft nur aus USB. Die Uhr für Einmalcodes stellt der Rechner beim Einstecken.

## Wege zum Rechner

- USB als Anmelde-Gerät (FIDO2)
- USB als Chipkarten-Leser (PIV, OpenPGP)
- USB als Tastatur (**geplant, noch nicht gebaut**)
- USB-Kanal für das Verwaltungsprogramm
- USB-Laufwerk, nur lesbar, mit den Programmen drauf (**geplant**)
- NFC (nur, solange USB den Stick versorgt)
- Bluetooth, nur für das Verwaltungsprogramm (nur, solange USB den Stick versorgt)

## Warum kein Akku?

Der Stick hat bewusst keinen Akku. Er läuft nur, solange er im USB-Anschluss steckt.

- Die Uhr für zeitbasierte Einmalcodes braucht keine Dauerversorgung. Der Rechner stellt sie beim Einstecken, und der Code entsteht erst im Moment des Benutzens. **Entwurf, noch nicht gebaut.**
- Den Zeitstempel einer Signatur holt der Stick von einem Zeitstempel-Server, nicht von seiner eigenen Uhr.
- Ein Akku hätte nur NFC und Bluetooth ohne Rechner gebracht. Dagegen standen mehr Länge (wie viel, steht in `hardware/layout-notes.md`) und ein Verbrauchsteil mit wenigen Jahren Lebensdauer, in einem Stick, der Jahrzehnte halten soll.

**Was du dafür aufgibst:**

- Der Funkchip kann sich keinen Strom aus dem NFC-Feld holen. NFC und Bluetooth gibt es deshalb nur, solange der Stick am Rechner steckt.
- Den Stick lose ans Handy halten geht nicht mehr.
- Bluetooth zu deiner eigenen App geht nur noch, solange der Stick am Rechner steckt. Mit dem Stick am Schlüsselbund geht es nicht mehr.
- Ein Zweitstick wird nur über USB am Rechner gekoppelt. Beide Sticks stecken dazu am selben Rechner.

Das ist bewusst so entschieden. Die Technik dahinter steht in `docs/architecture.md`.

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
- **Es gibt den privaten Schlüssel nicht als Datei.** Programme, die zwingend eine Schlüsseldatei verlangen, können mit dem Stick nicht arbeiten. Siehe [usage.md](usage.md), Abschnitt „Software, die eine Schlüsseldatei verlangt“.
- **Mac:** nie ausprobiert.
- **Speicherplatz in der Firmware** ist knapp. Ob alle Funktionen hineinpassen, ist offen.

## Das Laufwerk mit den Programmen braucht seine Zeit

**Stand:** gerechnet, nicht am Gerät gemessen. Das Laufwerk ist geplant und nicht gebaut.

Der Stick zeigt beim Einstecken ein kleines, nur lesbares Laufwerk mit den Verwaltungsprogrammen. Wer es öffnet, wartet.

**Hier stehen die drei Zahlen dazu. Alle anderen Stellen im Handbuch verweisen hierher.**

| Was | Zahl |
| --- | ---: |
| Was der Funkchip am USB durchbringt | rund **400 kB in der Sekunde** |
| Wie groß die Programme sind | **29,10 MB** (gemessen am 23.10.2026 am fertigen Abbild) |
| Wie lange das Lesen also dauert | rund **73 Sekunden** |

„Einstecken und starten" heißt in Wahrheit: einstecken, **über eine Minute warten,** starten. Wer das nicht weiß, hält den Stick für kaputt und zieht ihn ab.

Warum so langsam? Der Funkchip hängt an einem USB-Anschluss nach dem alten Standard 1.1. Daraus lassen sich auf dem Papier 1,5 MB in der Sekunde rechnen, aber der Stick muss jedes Stück erst aus seinem Speicher holen und beantworten. Was übrig bleibt, sind die 400 kB. Die 400 kB sind an einem Entwicklungsbrett **gemessen**, die beiden anderen Zahlen sind daraus gerechnet.

Die Programme wachsen. Wächst die Datei, wächst die Wartezeit im gleichen Verhältnis.

## Der verschlüsselte Datenträger

**Stand: Papier.** Es gibt keinen Baustein, keine Platine und keine Zeile Programm dazu. Dieser Abschnitt steht hier, damit niemand später etwas Falsches glaubt — nicht, weil es das gäbe.

Geplant ist eine Speicherkarte am Stick, 8 bis 16 GB, für eigene Dateien.

**Die Karte braucht immer einen openSecureKey.** In einem gewöhnlichen Kartenleser ist mit ihr nichts zu machen. Auf ihr liegt ein eigenes Format, kein Windows-Format. Sie ist **kein** Datenträger, den man jemandem ohne Stick in die Hand drückt.

### Drei Betriebsarten — und eine davon ist nicht geheim

Das ist die Stelle, an der man sich am leichtesten selbst schadet. Darum die drei Arten nebeneinander:

| Art | Name | Wer kann lesen | Inhalt geheim? |
| --- | --- | --- | --- |
| 1 | **nur dieser Stick** | nur der Stick, der geschrieben hat | **ja** |
| 2 | **jeder unserer Sticks, mit Kartenwort** | jeder openSecureKey, der das Kartenwort hat | **ja** |
| 3 | **jeder unserer Sticks, nur gesiegelt** | **jeder** openSecureKey, ohne Kennwort | **nein** |

**Art 3 verschlüsselt nichts.** Sie siegelt nur: man erkennt, ob jemand den Inhalt verändert hat. Lesen kann ihn jeder, der irgendeinen openSecureKey hat.

**Wer glaubt verschlüsselt zu haben und hat nur gesiegelt, gibt seine Geheimnisse weg.** Das ist der schlimmste Fehler, den dieses Gerät machen kann, und er entsteht nicht durch einen Angriff, sondern durch eine Verwechslung.

Deshalb ist geplant:

- Der Bildschirm nennt die Betriebsart im Klartext, **bevor** geschrieben wird, und dein Druck bestätigt genau diesen Text. Bei Art 3 steht dort „**OFFEN — nur gesiegelt**".
- Art 3 sieht am Bildschirm deutlich anders aus, nicht dieselbe Zeile mit einem anderen Wort darin.
- Solange die Karte steckt, steht die Betriebsart an.
- **Umstellen heißt neu beschreiben.** Es gibt kein „nachträglich verschlüsseln".

### Echtheit ist nicht Urheberschaft

Die Unterschrift auf der Karte beweist: **der Inhalt ist unverändert,** seit er unterschrieben wurde.

Sie beweist **nicht,** von wem er kommt.

Dass eine Karte von einer bestimmten Person stammt, folgt aus der Unterschrift **nur dann,** wenn du den öffentlichen Schlüssel dieser Person **vorher** kennst — von einem Treffen, einer Visitenkarte, einem anderen Weg. Sonst weißt du nur: hier hat irgendein openSecureKey unterschrieben.

| Behauptung | durch die Unterschrift gedeckt? |
| --- | --- |
| „Dieser Inhalt ist unverändert." | **ja** |
| „Ein openSecureKey hat unterschrieben." | ja |
| „*Dieses* Gerät hat unterschrieben." | ja |
| „Dieses Gerät gehört einer bestimmten Person." | **nein** |
| „Der Inhalt stammt von dieser Person." | **nein** |

Geplant ist deshalb: Beim Einstecken nennt der Bildschirm den Absenderschlüssel als kurze Zeichenfolge und sagt dazu „**unbekannter Absender**" oder den Namen, den du diesem Absender einmal gegeben hast.

### Geprüft wird beim Lesen, nicht beim Einstecken

Eine 16-GB-Karte **vollständig** zu prüfen heißt, sie ganz zu lesen. Das dauert **rund anderthalb Stunden** (gerechnet mit der Geschwindigkeit der Karte selbst, nicht mit der des USB-Anschlusses aus dem Abschnitt oben — das sind zwei verschiedene Wege). Niemand wartet das beim Einstecken ab, und darum tut der Stick es nicht.

Stattdessen prüft er **jeden Block in dem Moment, in dem er ihn liest.**

Das heißt im Guten: Keine Datei, die du öffnest, kommt ungeprüft bei dir an.

Und im Schlechten: **Ein veränderter Block in einer Datei, die niemand öffnet, fällt nicht auf.**

Darum sagt der Stick nie „Karte geprüft". Das wäre eine Lüge. Er sagt „dieser Block geprüft", und das ist die Wahrheit.

## Programm und Stick reden miteinander

- Das Programm `osk` spricht mit dem Stick über einen eigenen USB-Kanal.
- Jede Antwort des Sticks wird als feindlich behandelt: strenge Formate, harte Grenzen für Größe und Tiefe, und die Reihenfolgenummer muss passen.
- Die lokale Oberfläche lauscht nur auf deinem Rechner. Jede Anfrage muss von der eigenen Adresse kommen. Eine fremde Seite im selben Browser kommt nicht heran.
- Jede Anfrage nennt den Namen des Programms. Der Stick schreibt ihn ins Protokoll. Er bleibt ein Name, kein Beweis.
