// Zaehlt, wie oft das Geraet nach Freigabe gefragt wird.
//
// Der Punkt: Ein oeffentlicher Schluessel ist kein Geheimnis. Ihn einzeln
// abzuholen kostete frueher einen Tastendruck je Schluessel - und wer lernt,
// Tastendruecke wegzudruecken, drueckt auch den weg, der zaehlt. Darum ist
// die Zahl der Freigaben der Nachweis, nicht dass es noch laeuft.
package tests

import (
	"strings"
	"testing"

	"osk/classes"
)

// Derselbe Pruefschluessel wie in den Golden-Tests des Kerns. Er ist nur fuer
// Pruefungen da und steht offen im Quelltext.
const pruefSchluessel = `-----BEGIN OPENSSH PRIVATE KEY-----
b3BlbnNzaC1rZXktdjEAAAAABG5vbmUAAAAEbm9uZQAAAAAAAAABAAAAMwAAAAtzc2gtZW
QyNTUxOQAAACCVcf5ZPDwWWFLR8OcWk3CD/BnkwDo53NCLq2bAF5nnigAAAJDLIUqZyyFK
mQAAAAtzc2gtZWQyNTUxOQAAACCVcf5ZPDwWWFLR8OcWk3CD/BnkwDo53NCLq2bAF5nnig
AAAEDB07e/V0BFFynNnMZCvRF3qM91XxIiJp1bqPs1iKtMjJVx/lk8PBZYUtHw5xaTcIP8
GeTAOjnc0IurZsAXmeeKAAAACWVkLWdvbGRlbgECAwQ=
-----END OPENSSH PRIVATE KEY-----
`

// zaehleHerausgaben liest das Geraeteprotokoll und zaehlt, wie oft ein
// Geheimnis herausgegeben wurde.
func zaehleHerausgaben(t *testing.T, client *classes.Client) int {
	t.Helper()
	entries, err := client.Log(200)
	if err != nil {
		t.Fatalf("log: %v", err)
	}
	count := 0
	for _, entry := range entries {
		if strings.Contains(strings.ToLower(entry.Event), "reveal") {
			count++
		}
	}
	return count
}

func TestAuflistenFragtNichtNachFreigabe(t *testing.T) {
	app := newApp(t, startSimulator(t))
	client, err := app.Client()
	if err != nil {
		t.Fatalf("verbinden: %v", app.Message(err))
	}
	if err := client.Unlock([]byte(simPin)); err != nil {
		t.Fatalf("unlock: %v", app.Message(err))
	}

	// Drei Schluessel, damit die Zahl etwas aussagt: bei einem einzelnen
	// koennte eine 1 noch Zufall sein.
	for i := 0; i < 3; i++ {
		_, err := client.Add(classes.Item{
			Type: classes.ItemSSHKey,
			Name: "Pruefschluessel " + string(rune('A'+i)),
			Site: "git.example",
		}, []byte(pruefSchluessel))
		if err != nil {
			t.Fatalf("anlegen %d: %v", i, app.Message(err))
		}
	}

	vorher := zaehleHerausgaben(t, client)

	agent := classes.NewAgent(app)
	identitaeten, err := agent.Identities("pruefung")
	if err != nil {
		t.Fatalf("identities: %v", app.Message(err))
	}
	if len(identitaeten) < 3 {
		t.Fatalf("der Agent nennt %d Identitaeten, erwartet mindestens 3",
			len(identitaeten))
	}

	// Das ist die Messung, um die es geht.
	if nachher := zaehleHerausgaben(t, client); nachher != vorher {
		t.Errorf("das Auflisten hat %d Herausgaben gekostet, erwartet 0",
			nachher-vorher)
	}
	if fallbacks := agent.RevealFallbacks(); fallbacks != 0 {
		t.Errorf("%d Rueckfaelle auf Reveal, erwartet 0 - das Geraet schickt "+
			"den oeffentlichen Teil mit", fallbacks)
	}

	// Gegenprobe: Unterschreiben fragt sehr wohl, und zwar genau einmal.
	// Ohne diese Haelfte waere der Test auch dann gruen, wenn ueberhaupt
	// nichts mehr nach Freigabe fragte.
	vorSignatur := zaehleHerausgaben(t, client)
	if _, err := agent.Sign("pruefung", identitaeten[0].Blob(),
		[]byte("zu unterschreiben"), 0); err != nil {
		t.Fatalf("sign: %v", app.Message(err))
	}
	if nach := zaehleHerausgaben(t, client); nach-vorSignatur != 1 {
		t.Errorf("das Unterschreiben hat %d Herausgaben gekostet, erwartet genau 1",
			nach-vorSignatur)
	}
}
