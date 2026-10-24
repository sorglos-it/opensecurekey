// Zaehlt, wie oft PKCS#11 das Geraet nach Freigabe fragt.
//
// Dieselbe Messung wie beim ssh-Agenten in freigaben_test.go, nur fuer den
// anderen Weg: Firefox, OpenSSL, Java und ssh -I laden ihre Objekte ueber
// PKCS#11. Fuer die oeffentlichen Groessen - Modulus, Kurve, Punkt - wurde
// dabei frueher jeder Schluessel einzeln per `reveal` geholt. Das kostete
// einen Tastendruck je Schluessel, der nichts schuetzte.
package tests

import (
	"testing"

	"osk/classes"
)

// pkcs11Token baut ein angemeldetes Token auf dem offenen Tresor.
func pkcs11Token(t *testing.T, app *classes.App) (*classes.Pkcs11Token, uint64) {
	t.Helper()
	token := classes.NewPkcs11Token(app)
	token.SetHost("pruefung")
	session, err := token.OpenSession(classes.CkfSerialSession)
	if err != nil {
		t.Fatalf("Sitzung oeffnen: %v", app.Message(err))
	}
	if err := token.Login(session, classes.CkuUser, []byte(simPin)); err != nil {
		t.Fatalf("anmelden: %v", app.Message(err))
	}
	return token, session
}

func TestPkcs11AuflistenFragtNichtNachFreigabe(t *testing.T) {
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
		if _, err := client.Add(classes.Item{
			Type: classes.ItemSSHKey,
			Name: "Pruefschluessel " + string(rune('A'+i)),
			Site: "git.example",
		}, []byte(pruefSchluessel)); err != nil {
			t.Fatalf("anlegen %d: %v", i, app.Message(err))
		}
	}

	token, session := pkcs11Token(t, app)
	vorher := zaehleHerausgaben(t, client)

	// Das ist, was ein fremdes Programm beim Laden tut: alle Objekte suchen.
	objekte, err := token.FindObjects(session, nil)
	if err != nil {
		t.Fatalf("FindObjects: %v", app.Message(err))
	}
	if err := token.FindObjectsFinal(session); err != nil {
		t.Fatalf("FindObjectsFinal: %v", app.Message(err))
	}
	// Je Schluessel ein privates und ein oeffentliches Objekt: ohne die
	// Untergrenze waere der Test auch gruen, wenn gar nichts gefunden wird
	// und darum auch nichts zu fragen war.
	if len(objekte) < 6 {
		t.Fatalf("PKCS#11 nennt %d Objekte, erwartet mindestens 6",
			len(objekte))
	}

	// Das ist die Messung, um die es geht.
	if nachher := zaehleHerausgaben(t, client); nachher != vorher {
		t.Errorf("das Auflisten hat %d Herausgaben gekostet, erwartet 0",
			nachher-vorher)
	}
	if fallbacks := token.RevealFallbacks(); fallbacks != 0 {
		t.Errorf("%d Rueckfaelle auf Reveal, erwartet 0 - das Geraet schickt "+
			"den oeffentlichen Teil mit", fallbacks)
	}

	// Gegenprobe: Unterschreiben fragt sehr wohl, und zwar genau einmal je
	// Unterschrift. Ohne diese Haelfte waere der Test auch dann gruen, wenn
	// ueberhaupt nichts mehr nach Freigabe fragte. Und zweimal gemessen,
	// weil ein Token, das den Schluessel behaelt, die zweite Unterschrift
	// umsonst bekaeme - genau das soll nicht sein.
	privat := erstesPrivatesObjekt(t, app, token, session)
	for runde := 1; runde <= 2; runde++ {
		vorSignatur := zaehleHerausgaben(t, client)
		if err := token.SignInit(session, privat, classes.CkmEDDSA); err != nil {
			t.Fatalf("SignInit: %v", app.Message(err))
		}
		if _, err := token.Sign(session, []byte("zu unterschreiben")); err != nil {
			t.Fatalf("Sign: %v", app.Message(err))
		}
		if nach := zaehleHerausgaben(t, client); nach-vorSignatur != 1 {
			t.Errorf("Unterschrift %d hat %d Herausgaben gekostet, erwartet genau 1",
				runde, nach-vorSignatur)
		}
	}
}

// erstesPrivatesObjekt sucht ein privates Schluesselobjekt zum Unterschreiben.
func erstesPrivatesObjekt(t *testing.T, app *classes.App,
	token *classes.Pkcs11Token, session uint64) uint64 {
	t.Helper()
	found, err := token.FindObjects(session, []classes.Pkcs11Attribute{
		{Type: classes.CkaClass, Value: classes.Pkcs11UlongValue(classes.CkoPrivateKey)},
		{Type: classes.CkaKeyType, Value: classes.Pkcs11UlongValue(classes.CkkECEdwards)},
	})
	if err != nil {
		t.Fatalf("privates Objekt suchen: %v", app.Message(err))
	}
	if err := token.FindObjectsFinal(session); err != nil {
		t.Fatalf("FindObjectsFinal: %v", app.Message(err))
	}
	if len(found) == 0 {
		t.Fatalf("kein privates Objekt gefunden")
	}
	return found[0]
}
