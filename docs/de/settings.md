# Einstellungen

**Stand:** Die Einstellungen des Programms sind gegen die Test-Attrappe geprüft. Die Einstellungen im Stick selbst gibt es noch nicht auf einem echten Stick.

Es gibt wenige Einstellungen. Das ist Absicht.

## Im Programm `osk`

Öffne in der Oberfläche die Seite **Einstellungen.** Gespeichert wird in der Datei `config_osk.json` **neben dem Programm**, nicht im System.

| Einstellung | Bedeutung | Vorgabe | Erlaubte Werte |
| --- | --- | --- | --- |
| Sprache | Sprache der Oberfläche | `de` | `de`, `en`, `fr`, `it`, `es` |
| Name dieses Programms | Der Name, den das Programm dem Stick nennt. Er landet im Protokoll. | `osk` | Text bis 64 Zeichen |
| Port | Anschluss, auf dem die Oberfläche lauscht | `8101` | Zahl von 1024 bis 65000 |
| Adresse der Attrappe | Nur zum Testen: Adresse des Test-Sticks | leer | zum Beispiel `127.0.0.1:8201`, und nur auf diesem Rechner |

Gut zu wissen:

- **Der Name ist eine Behauptung.** Der Stick schreibt ihn ins Protokoll, aber er beweist nichts. Nur der Stick selbst kann etwas verweigern.
- Ist der Port belegt, sucht das Programm ab dem eingestellten Wert einen freien. Findet es keinen, sagt es das.
- **Adresse der Attrappe leer lassen**, solange du einen echten Stick benutzt. Die Attrappe ist ein Testwerkzeug. Leg dort nie echte Geheimnisse ab. Ihre PIN ist `123456`, und sie verschlüsselt nichts.
- Ist die Einstellungsdatei kaputt oder zu groß, startet das Programm trotzdem. Es nimmt die Vorgaben und sagt, warum.

Beispiel (`config_osk.json`):

```json
{
  "lang": "de",
  "port": 8101,
  "appName": "osk",
  "simAddress": ""
}
```

## Je Eintrag im Stick

Jeder Eintrag hat drei Schalter. Siehe [usage.md](usage.md), Abschnitt „Eintragsarten“.

- darf gespiegelt werden
- nur gekoppelte Rechner
- schreibgeschützt

## Im Stick selbst

Diese Dinge stellst du **am Stick** ein, nicht im Programm. **Stand:** noch nirgends erprobt.

| Einstellung | Vorgabe | Was sie ändert |
| --- | --- | --- |
| Profil und PIN | bis zu 4 Profile, je eigene PIN | Wer was sieht |
| NFC-Antipp-Funktion (Adresse öffnen oder einen Einmalcode tippen) | lässt sich abschalten | Ein Telefon öffnet beim Antippen eine Adresse |
| Blind unterschreiben | aus | Der Stick unterschreibt auch Dinge, die er nicht lesbar zeigen kann. Die Frage am Bildschirm sieht dann sichtbar anders aus. |
| Server-Freigabe | aus | Ein Server darf ohne Druck unterschreiben, für genannte Schlüssel, höchstens 24 Stunden |
| Bluetooth | nur für das Verwaltungsprogramm | Nicht für die Anmeldung im Browser. Koppeln nur mit Zahlenvergleich und Druck am Stick. |

Gut zu wissen:

- **Alles Riskante ist aus,** bis du es bewusst einschaltest.
- **NFC braucht immer den Knopf.** Auch dann, wenn NFC kurz dauert. Das schützt vor einem Angriff aus der Tasche, bei dem jemand die Funkverbindung weiterleitet.
- Die Funkadresse von Bluetooth wechselt, damit man dich nicht verfolgen kann.

## Sprache am Stick

Der Bildschirm am Stick kennt dieselben fünf Sprachen. Fehlt ein Text, nimmt er Englisch, nie einen leeren Platz.

## Sprache wechseln

Wechsle die Sprache im Programm unter **Einstellungen.** Sprachdateien liegen im Ordner `lang`, eine je Sprache.
