// Pruefungen des Serverbetriebs: zwei Knoten, ein signierter Auftrag, ein
// Schluesselaustausch - und der Fall, dass der gelieferte Chip kein ECDH kann.
package classes

import (
	"bytes"
	"errors"
	"os"
	"strings"
	"testing"
	"time"
)

// nodeTestGuard baut einen Knoten mit eigener Attrappe.
func nodeTestGuard(t *testing.T, name string, opts GuardFakeOptions) (*GuardFakeDevice, *Guard) {
	t.Helper()
	opts.Seed = []byte("knoten-" + name)
	device, err := NewGuardFakeDevice(opts)
	if err != nil {
		t.Fatalf("Attrappe %s: %v", name, err)
	}
	cfg := DefaultGuardConfig()
	cfg.DevicePublicKey = device.PublicKey()
	cfg.BinaryPath = guardTestBinary(t, "knoten", []byte("knoten "+name))
	guard, err := NewGuard(device.GuardFakeClient(name), cfg)
	if err != nil {
		t.Fatalf("Guard %s: %v", name, err)
	}
	return device, guard
}

// Zwei Knoten schicken einander einen Auftrag und pruefen ihn. Genau das
// braucht NextGenCloud, und es laeuft hier ohne Schluesseldatei auf der
// Platte - der Schluessel liegt im Chip.
func TestTwoNodesSignAndVerifyAJob(t *testing.T) {
	deviceA, nodeA := nodeTestGuard(t, "a", GuardFakeOptions{})
	_, nodeB := nodeTestGuard(t, "b", GuardFakeOptions{})

	publicA, err := nodeA.NodePublic("auftraege", "ed25519")
	if err != nil {
		t.Fatalf("oeffentlicher Schluessel von A: %v", err)
	}
	job := []byte(`{"tat":"abbild-holen","was":"org.sorglos.terminal","wann":1700000000}`)
	signature, err := nodeA.NodeSign("auftraege", job)
	if err != nil {
		t.Fatalf("A unterschreibt: %v", err)
	}
	// B prueft - ohne eigenes Geraet, nur mit dem oeffentlichen Schluessel.
	if err := NodeVerify(publicA.PublicKey, "auftraege", job, signature); err != nil {
		t.Fatalf("B lehnt den Auftrag von A ab: %v", err)
	}

	// Ein veraenderter Auftrag faellt durch.
	bent := append([]byte{}, job...)
	bent[10] ^= 0x01
	if err := NodeVerify(publicA.PublicKey, "auftraege", bent, signature); err == nil {
		t.Fatal("ein veraenderter Auftrag wurde angenommen")
	}

	// Der Schluessel eines anderen Knotens passt nicht.
	publicB, err := nodeB.NodePublic("auftraege", "ed25519")
	if err != nil {
		t.Fatalf("oeffentlicher Schluessel von B: %v", err)
	}
	if bytes.Equal(publicA.PublicKey, publicB.PublicKey) {
		t.Fatal("zwei Knoten haben denselben Schluessel")
	}
	if err := NodeVerify(publicB.PublicKey, "auftraege", job, signature); err == nil {
		t.Fatal("der Schluessel des falschen Knotens passte")
	}

	// Und die Bezeichnung gehoert dazu: dieselbe Unterschrift unter einer
	// anderen Schluesselbezeichnung gilt nicht.
	if err := NodeVerify(publicA.PublicKey, "anderes", job, signature); err == nil {
		t.Fatal("die Unterschrift galt auch unter anderer Bezeichnung")
	}

	// Jede Unterschrift steht im Geraeteprotokoll.
	var signed int
	for _, line := range deviceA.Log() {
		if strings.HasPrefix(line.Event, "sign:") {
			signed++
		}
	}
	if signed != 1 {
		t.Fatalf("%d Unterschriften im Protokoll, erwartet 1", signed)
	}
}

// Schluesselaustausch: beide Seiten kommen auf dasselbe Geheimnis.
func TestTwoNodesAgreeOnTheSameSecret(t *testing.T) {
	_, nodeA := nodeTestGuard(t, "a", GuardFakeOptions{})
	_, nodeB := nodeTestGuard(t, "b", GuardFakeOptions{})

	publicA, err := nodeA.NodePublic("austausch", "x25519")
	if err != nil {
		t.Fatalf("A: %v", err)
	}
	publicB, err := nodeB.NodePublic("austausch", "x25519")
	if err != nil {
		t.Fatalf("B: %v", err)
	}
	secretA, err := nodeA.NodeAgree("austausch", publicB.PublicKey)
	if err != nil {
		t.Fatalf("A rechnet: %v", err)
	}
	secretB, err := nodeB.NodeAgree("austausch", publicA.PublicKey)
	if err != nil {
		t.Fatalf("B rechnet: %v", err)
	}
	if !bytes.Equal(secretA, secretB) {
		t.Fatal("die beiden Seiten kamen auf verschiedene Geheimnisse")
	}
	if len(secretA) != NodeSecretLen {
		t.Fatalf("Laenge %d statt %d", len(secretA), NodeSecretLen)
	}
	// Mit einem dritten Schluessel kommt etwas anderes heraus.
	_, nodeC := nodeTestGuard(t, "c", GuardFakeOptions{})
	publicC, err := nodeC.NodePublic("austausch", "x25519")
	if err != nil {
		t.Fatalf("C: %v", err)
	}
	secretAC, err := nodeA.NodeAgree("austausch", publicC.PublicKey)
	if err != nil {
		t.Fatalf("A mit C: %v", err)
	}
	if bytes.Equal(secretA, secretAC) {
		t.Fatal("verschiedene Gegenseiten ergaben dasselbe Geheimnis")
	}
}

// Der wichtige Fall: ein geliefertes Stueck, das im Werk auf FIPS gestellt
// wurde, verweigert ECDH. Dann wird gesagt, was wegfaellt - und nichts
// hilfsweise in der Firmware gerechnet. Unterschreiben geht weiter.
func TestNodeAgreeSaysWhichUseCaseIsGoneWhenEcdhIsMissing(t *testing.T) {
	_, node := nodeTestGuard(t, "fips", GuardFakeOptions{NoECDH: true})
	_, partner := nodeTestGuard(t, "partner", GuardFakeOptions{})
	publicPartner, err := partner.NodePublic("austausch", "x25519")
	if err != nil {
		t.Fatalf("Gegenseite: %v", err)
	}

	_, err = node.NodeAgree("austausch", publicPartner.PublicKey)
	if err == nil {
		t.Fatal("ein Chip ohne ECDH hat gerechnet")
	}
	var problem *Problem
	if !errors.As(err, &problem) || problem.Key != "error.guard.noecdh" {
		t.Fatalf("erwartet error.guard.noecdh, bekommen %v", err)
	}
	if !GuardNoECDH(err) {
		t.Fatal("GuardNoECDH erkennt den eigenen Fall nicht")
	}

	// Der Satz dazu muss den Anwendungsfall nennen, nicht nach fehlender
	// PIN klingen. Gemessen am Text selbst, in allen fuenf Sprachen.
	for _, lang := range Languages(os.DirFS(appFS())) {
		text := LoadTexts(os.DirFS(appFS()), lang).Message(err)
		if text == "" || text == "error.guard.noecdh" {
			t.Fatalf("%s: kein Satz zum Fall", lang)
		}
		if text == LoadTexts(os.DirFS(appFS()), lang).Get("error.internal") {
			t.Fatalf("%s: nur der allgemeine Fehlertext", lang)
		}
		if strings.Contains(strings.ToLower(text), "pin") {
			t.Fatalf("%s: der Satz redet von der PIN: %q", lang, text)
		}
	}

	// Und das Entscheidende: Unterschreiben geht weiter. Nur der
	// Schluesselaustausch faellt weg, nicht der ganze Knoten.
	public, err := node.NodePublic("auftraege", "ed25519")
	if err != nil {
		t.Fatalf("Unterschreiben sollte weiter gehen: %v", err)
	}
	job := []byte("ein Auftrag")
	signature, err := node.NodeSign("auftraege", job)
	if err != nil {
		t.Fatalf("Unterschreiben sollte weiter gehen: %v", err)
	}
	if err := NodeVerify(public.PublicKey, "auftraege", job, signature); err != nil {
		t.Fatalf("die Unterschrift gilt nicht: %v", err)
	}

	// Auch nach dem oeffentlichen X25519-Teil zu fragen, scheitert sauber.
	if _, err := node.NodePublic("austausch", "x25519"); err == nil {
		t.Fatal("ein Chip ohne ECDH gab einen X25519-Schluessel heraus")
	}
}

// Ohne Freischaltung verlangt das Geraet je Unterschrift einen Tastendruck.
// Steht niemand davor, bleibt der Server stehen - genau das ist der Grund,
// warum es die Freischaltung gibt.
func TestServerStandsStillWithoutGrant(t *testing.T) {
	_, node := nodeTestGuard(t, "server", GuardFakeOptions{RequireTouch: true})
	_, err := node.NodeSign("auftraege", []byte("ein Auftrag"))
	if err == nil {
		t.Fatal("ohne Tastendruck und ohne Freischaltung unterschrieben")
	}
	var problem *Problem
	if !errors.As(err, &problem) || problem.Key != "error.device.denied" {
		t.Fatalf("erwartet error.device.denied, bekommen %v", err)
	}
}

// Mit Freischaltung laeuft der Server - begrenzt auf die genannten
// Schluessel, zeitlich befristet, und jeder Zugriff steht im Protokoll.
func TestSessionGrantIsLimitedToNamedKeysAndTime(t *testing.T) {
	device, node := nodeTestGuard(t, "server", GuardFakeOptions{RequireTouch: true})
	now := time.Unix(1700000000, 0)
	device.SetNow(func() time.Time { return now })

	if err := node.SessionAllow(SessionGrant{
		Keys: []string{"auftraege"}, TTL: time.Hour}); err != nil {
		t.Fatalf("freischalten: %v", err)
	}
	state, err := node.SessionStatus()
	if err != nil {
		t.Fatalf("Stand: %v", err)
	}
	if !state.Active || len(state.Keys) != 1 || state.Keys[0] != "auftraege" {
		t.Fatalf("falscher Stand: %+v", state)
	}

	// Der freigeschaltete Schluessel unterschreibt ohne Tastendruck.
	if _, err := node.NodeSign("auftraege", []byte("Auftrag 1")); err != nil {
		t.Fatalf("mit Freischaltung: %v", err)
	}
	// Ein anderer Schluessel nicht - die Freischaltung gilt nicht fuer alles.
	if _, err := node.NodeSign("tresor-schluessel", []byte("Auftrag 2")); err == nil {
		t.Fatal("ein nicht freigeschalteter Schluessel hat unterschrieben")
	}

	// Nach Ablauf der Zeit ist Schluss, ohne dass jemand etwas tun muss.
	now = now.Add(2 * time.Hour)
	if _, err := node.NodeSign("auftraege", []byte("Auftrag 3")); err == nil {
		t.Fatal("die Freischaltung galt nach Ablauf weiter")
	}

	// Jeder Zugriff und jede Freischaltung steht im Geraeteprotokoll - das
	// ist die Zusage aus docs/dongle.md.
	var granted, signed, denied int
	for _, line := range device.Log() {
		switch {
		case strings.HasPrefix(line.Event, "session-on:"):
			granted++
		case strings.HasPrefix(line.Event, "sign:"):
			signed++
		case strings.HasPrefix(line.Event, "sign-denied:"):
			denied++
		}
	}
	if granted != 1 || signed != 1 || denied != 2 {
		t.Fatalf("Protokoll: %d freigeschaltet, %d unterschrieben, %d abgelehnt",
			granted, signed, denied)
	}
}

// Zuruecknehmen geht jederzeit und ohne Tastendruck - weniger Rechte darf
// ein Programm immer verlangen.
func TestSessionEndTakesTheGrantBack(t *testing.T) {
	_, node := nodeTestGuard(t, "server", GuardFakeOptions{RequireTouch: true})
	if err := node.SessionAllow(SessionGrant{
		Keys: []string{"auftraege"}, TTL: time.Hour}); err != nil {
		t.Fatalf("freischalten: %v", err)
	}
	if _, err := node.NodeSign("auftraege", []byte("Auftrag")); err != nil {
		t.Fatalf("mit Freischaltung: %v", err)
	}
	if err := node.SessionEnd(); err != nil {
		t.Fatalf("zuruecknehmen: %v", err)
	}
	if _, err := node.NodeSign("auftraege", []byte("Auftrag")); err == nil {
		t.Fatal("nach dem Zuruecknehmen weiter unterschrieben")
	}
	state, err := node.SessionStatus()
	if err != nil {
		t.Fatalf("Stand: %v", err)
	}
	if state.Active {
		t.Fatal("die Freischaltung gilt noch")
	}
}

// Eine Freischaltung ohne Schluessel oder ohne Frist gibt es nicht. Sie
// muss ausdruecklich und eng gesetzt werden.
func TestSessionGrantNeedsKeysAndTime(t *testing.T) {
	_, node := nodeTestGuard(t, "server", GuardFakeOptions{RequireTouch: true})
	cases := []struct {
		name  string
		grant SessionGrant
		want  string
	}{
		{"ohne Schluessel", SessionGrant{TTL: time.Hour}, "error.guard.sessionkeys"},
		{"ohne Frist", SessionGrant{Keys: []string{"a"}}, "error.guard.sessionttl"},
		{"zu lange Frist", SessionGrant{Keys: []string{"a"},
			TTL: SessionMaxTTL + time.Second}, "error.guard.sessionttl"},
		{"kaputte Bezeichnung", SessionGrant{Keys: []string{"a\nb"},
			TTL: time.Hour}, "error.guard.label"},
	}
	for _, one := range cases {
		t.Run(one.name, func(t *testing.T) {
			err := node.SessionAllow(one.grant)
			var problem *Problem
			if !errors.As(err, &problem) || problem.Key != one.want {
				t.Fatalf("erwartet %s, bekommen %v", one.want, err)
			}
		})
	}
}

func TestNodeRejectsBadArguments(t *testing.T) {
	_, node := nodeTestGuard(t, "a", GuardFakeOptions{})
	if _, err := node.NodePublic("gut", "kurve-die-es-nicht-gibt"); err == nil {
		t.Fatal("unbekanntes Schluesselverfahren angenommen")
	}
	if _, err := node.NodeSign("", []byte("x")); err == nil {
		t.Fatal("leere Bezeichnung angenommen")
	}
	if _, err := node.NodeSign("gut", nil); err == nil {
		t.Fatal("leerer Auftrag angenommen")
	}
	if _, err := node.NodeSign("gut", make([]byte, NodeJobMax+1)); err == nil {
		t.Fatal("zu grosser Auftrag angenommen")
	}
	if _, err := node.NodeAgree("gut", make([]byte, 31)); err == nil {
		t.Fatal("zu kurzer Fremdschluessel angenommen")
	}
	// Ein Punkt, der nicht auf der Kurve liegt: alles null ist der
	// bekannte Fall mit kleiner Ordnung.
	if _, err := node.NodeAgree("gut", make([]byte, NodeSecretLen)); err == nil {
		t.Fatal("ein unbrauchbarer Fremdschluessel wurde angenommen")
	}
	if err := NodeVerify(make([]byte, 3), "gut", []byte("x"), make([]byte, GuardSigLen)); err == nil {
		t.Fatal("unbrauchbarer oeffentlicher Schluessel angenommen")
	}
	if err := NodeVerify(make([]byte, GuardKeyLen), "gut", []byte("x"), nil); err == nil {
		t.Fatal("fehlende Unterschrift angenommen")
	}
}

// Derselbe Knoten, derselbe Name, derselbe Schluessel - sonst waere die
// Kennung eines Knotens nach jedem Neustart eine andere.
func TestNodeKeyIsStable(t *testing.T) {
	_, node := nodeTestGuard(t, "a", GuardFakeOptions{})
	first, err := node.NodePublic("auftraege", "ed25519")
	if err != nil {
		t.Fatalf("erst: %v", err)
	}
	again, err := node.NodePublic("auftraege", "ed25519")
	if err != nil {
		t.Fatalf("nochmal: %v", err)
	}
	if !bytes.Equal(first.PublicKey, again.PublicKey) {
		t.Fatal("derselbe Name ergab zwei Schluessel")
	}
	other, err := node.NodePublic("anderer-zweck", "ed25519")
	if err != nil {
		t.Fatalf("anderer: %v", err)
	}
	if bytes.Equal(first.PublicKey, other.PublicKey) {
		t.Fatal("zwei Namen ergaben denselben Schluessel")
	}
}
