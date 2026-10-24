// Die Gegenstelle der Erweiterung von Anfang bis Ende gegen die Attrappe.
//
// Geprueft wird der ganze Weg: Nachricht mit Laengenkopf hinein, Antwort mit
// Laengenkopf heraus, dazwischen das Protokoll zum Geraet. Und vor allem:
// was passiert, wenn die Erweiterung LUEGT.
//
// Das ist der Fall, auf den es ankommt. Die Erweiterung liest die Adresse
// aus der Adresszeile - aber wenn jemand die Erweiterung uebernimmt, nennt
// sie, was sie will. Dann muss hier Schluss sein.
package classes

import (
	"bytes"
	"encoding/binary"
	"encoding/json"
	"io"
	"testing"
)

// nativePair baut einen Wirt mit entsperrtem Tresor und zwei Eintraegen auf
// verschiedenen Adressen.
func nativePair(t *testing.T) (*nativeHost, *App, map[string]uint32) {
	t.Helper()
	app := unlockedApp(t)
	client, err := app.Client()
	if err != nil {
		t.Fatalf("verbinden: %v", app.Message(err))
	}
	access := NewAccess(client, "osk put")
	ids := map[string]uint32{}
	for name, site := range map[string]string{
		"bank":  "bank.example.org",
		"forum": "forum.example.org",
		"ohne":  "",
	} {
		id, addErr := access.Add(Item{Type: ItemLogin, Name: name, Site: site},
			[]byte("geheim-"+name))
		if addErr != nil {
			t.Fatalf("%s ablegen: %v", name, app.Message(addErr))
		}
		ids[name] = id
	}
	return &nativeHost{app: app, caller: nativeCaller("chrome-extension://abc/")}, app, ids
}

// roundTrip schickt eine Nachricht durch Kopf und Rumpf und liest die
// Antwort - also wirklich ueber den Kanal, nicht als Funktionsaufruf.
func roundTrip(t *testing.T, host *nativeHost, request nativeRequest) nativeReply {
	t.Helper()
	body, err := json.Marshal(request)
	if err != nil {
		t.Fatalf("Anfrage bauen: %v", err)
	}
	var header [4]byte
	binary.NativeEndian.PutUint32(header[:], uint32(len(body)))
	in := bytes.NewReader(append(header[:], body...))
	var out bytes.Buffer
	if err := host.serve(in, &out); err != nil && err != io.EOF {
		t.Fatalf("Kanal: %v", err)
	}
	if out.Len() < 4 {
		t.Fatalf("keine Antwort (%d Bytes)", out.Len())
	}
	data := out.Bytes()
	size := binary.NativeEndian.Uint32(data[:4])
	if int(size) != len(data)-4 {
		t.Fatalf("Laengenangabe %d, aber %d Bytes Rumpf", size, len(data)-4)
	}
	var reply nativeReply
	if err := json.Unmarshal(data[4:], &reply); err != nil {
		t.Fatalf("Antwort lesen: %v", err)
	}
	return reply
}

// Die Liste nennt nur die Eintraege der genannten Adresse - und verraet
// nicht, was sonst im Tresor liegt.
func TestNativeListOnlyMatchingSite(t *testing.T) {
	host, _, ids := nativePair(t)

	reply := roundTrip(t, host, nativeRequest{ID: 1, Was: "list",
		Origin: "https://bank.example.org"})
	if !reply.OK {
		t.Fatalf("list: %s", reply.Error)
	}
	if len(reply.Matches) != 1 {
		t.Fatalf("%d Treffer, erwartet 1: %+v", len(reply.Matches), reply.Matches)
	}
	if reply.Matches[0].ID != ids["bank"] {
		t.Errorf("Treffer %d, erwartet %d", reply.Matches[0].ID, ids["bank"])
	}
	if reply.Origin != "https://bank.example.org" {
		t.Errorf("Antwort nennt %q", reply.Origin)
	}

	// Eine Adresse, zu der nichts gehoert: leere Liste, kein Fehler - und
	// ausdruecklich NICHT die uebrigen Eintraege.
	reply = roundTrip(t, host, nativeRequest{ID: 2, Was: "list",
		Origin: "https://boese.tld"})
	if !reply.OK {
		t.Fatalf("list: %s", reply.Error)
	}
	if len(reply.Matches) != 0 {
		t.Errorf("%d Treffer auf einer fremden Adresse: %+v",
			len(reply.Matches), reply.Matches)
	}
}

// DER FALL, AUF DEN ES ANKOMMT: die Erweiterung fragt nach einem Eintrag,
// den sie auf dieser Adresse nie angeboten bekommen hat.
func TestNativeRevealRefusesWrongSite(t *testing.T) {
	host, _, ids := nativePair(t)

	// Zwischenpruefung: auf der RICHTIGEN Adresse kommt das Geheimnis.
	// Ohne sie wuesste niemand, ob die Ablehnung unten an der Adresse liegt
	// oder an irgendetwas anderem.
	good := roundTrip(t, host, nativeRequest{ID: 1, Was: "reveal",
		Origin: "https://bank.example.org", Item: ids["bank"], Feld: "secret"})
	if !good.OK || good.Value != "geheim-bank" {
		t.Fatalf("der Weg bis zur gemeinten Stelle ist nicht offen: %+v", good)
	}

	// Und jetzt dieselbe Nummer auf einer fremden Adresse.
	for _, origin := range []string{
		"https://boese.tld",
		"https://bank.example.org.boese.tld",
		"https://forum.example.org",
		"https://example.org",
		"https://www.bank.example.org",
	} {
		reply := roundTrip(t, host, nativeRequest{ID: 2, Was: "reveal",
			Origin: origin, Item: ids["bank"], Feld: "secret"})
		if reply.OK {
			t.Errorf("%s: das Geheimnis kam heraus", origin)
		}
		if reply.Value != "" {
			t.Errorf("%s: in der Antwort steht ein Wert", origin)
		}
	}
}

// Ein Eintrag ohne Adresse gehoert zu keiner Seite. Er darf nirgends
// ausgefuellt werden - sonst waere "Feld leer" der Hauptschluessel.
func TestNativeEntryWithoutSiteFillsNowhere(t *testing.T) {
	host, _, ids := nativePair(t)
	for _, origin := range []string{
		"https://bank.example.org",
		"https://example.org",
		"http://localhost",
	} {
		reply := roundTrip(t, host, nativeRequest{ID: 1, Was: "reveal",
			Origin: origin, Item: ids["ohne"], Feld: "secret"})
		if reply.OK {
			t.Errorf("%s: ein Eintrag ohne Adresse wurde ausgegeben", origin)
		}
	}
	// Und er steht in keiner Liste.
	reply := roundTrip(t, host, nativeRequest{ID: 2, Was: "list",
		Origin: "https://bank.example.org"})
	for _, match := range reply.Matches {
		if match.ID == ids["ohne"] {
			t.Error("ein Eintrag ohne Adresse stand in der Liste")
		}
	}
}

// Eine Adresse, die nicht gelesen werden kann, bekommt kein Geheimnis -
// und auch keine Liste.
func TestNativeRefusesBadOrigin(t *testing.T) {
	host, _, ids := nativePair(t)
	for _, origin := range []string{
		"",
		"bank.example.org",
		"http://bank.example.org",
		"https://bank.example.org@boese.tld",
		"https://bank.example.org/anmelden",
		"javascript:alert(1)",
	} {
		list := roundTrip(t, host, nativeRequest{ID: 1, Was: "list", Origin: origin})
		if list.OK {
			t.Errorf("list %q wurde beantwortet", origin)
		}
		reveal := roundTrip(t, host, nativeRequest{ID: 2, Was: "reveal",
			Origin: origin, Item: ids["bank"], Feld: "secret"})
		if reveal.OK || reveal.Value != "" {
			t.Errorf("reveal %q gab etwas heraus", origin)
		}
	}
}

// Nur die zwei vereinbarten Felder, und eine Nummer, die es nicht gibt,
// bekommt kein Geheimnis.
func TestNativeFieldAndItemChecks(t *testing.T) {
	host, _, ids := nativePair(t)

	for _, feld := range []string{"", "pin", "alles", "SECRET"} {
		reply := roundTrip(t, host, nativeRequest{ID: 1, Was: "reveal",
			Origin: "https://bank.example.org", Item: ids["bank"], Feld: feld})
		if reply.OK {
			t.Errorf("Feld %q wurde beantwortet", feld)
		}
	}
	// Der Name ist kein Geheimnis und kommt ohne Freigabe.
	name := roundTrip(t, host, nativeRequest{ID: 2, Was: "reveal",
		Origin: "https://bank.example.org", Item: ids["bank"], Feld: "user"})
	if !name.OK || name.Value != "bank" {
		t.Errorf("Name: %+v", name)
	}
	// Eine Nummer, die es nicht gibt.
	missing := roundTrip(t, host, nativeRequest{ID: 3, Was: "reveal",
		Origin: "https://bank.example.org", Item: 9999, Feld: "secret"})
	if missing.OK || missing.Value != "" {
		t.Errorf("fremde Nummer: %+v", missing)
	}
	// Ein Wunsch, den es nicht gibt.
	unknown := roundTrip(t, host, nativeRequest{ID: 4, Was: "zaubern"})
	if unknown.OK {
		t.Error("ein unbekannter Wunsch wurde beantwortet")
	}
}

// Bei gesperrtem Tresor kommt nichts heraus, und die Erweiterung erfaehrt,
// dass sie entsperren lassen muss.
func TestNativeLockedState(t *testing.T) {
	host, app, ids := nativePair(t)
	client, err := app.Client()
	if err != nil {
		t.Fatalf("verbinden: %v", app.Message(err))
	}
	if err := client.Lock(); err != nil {
		t.Fatalf("sperren: %v", app.Message(err))
	}
	state := roundTrip(t, host, nativeRequest{ID: 1, Was: "state"})
	if !state.OK || state.Unlocked {
		t.Errorf("state nach dem Sperren: %+v", state)
	}
	reveal := roundTrip(t, host, nativeRequest{ID: 2, Was: "reveal",
		Origin: "https://bank.example.org", Item: ids["bank"], Feld: "secret"})
	if reveal.OK || reveal.Value != "" {
		t.Errorf("gesperrt kam etwas heraus: %+v", reveal)
	}
	// Mit richtiger PIN wieder offen.
	unlock := roundTrip(t, host, nativeRequest{ID: 3, Was: "unlock", PIN: testPin})
	if !unlock.OK || !unlock.Unlocked {
		t.Errorf("unlock: %+v", unlock)
	}
	// Mit falscher PIN nicht.
	if err := client.Lock(); err != nil {
		t.Fatalf("sperren: %v", app.Message(err))
	}
	wrong := roundTrip(t, host, nativeRequest{ID: 4, Was: "unlock", PIN: "000000"})
	if wrong.OK {
		t.Error("die Anmeldung gelang mit falscher PIN")
	}
}

// Der Name im Geraeteprotokoll kommt vom BROWSER, nicht von der
// Erweiterung: er steht im Aufrufparameter, den der Browser setzt.
func TestNativeCallerInDeviceLog(t *testing.T) {
	host, app, ids := nativePair(t)
	reply := roundTrip(t, host, nativeRequest{ID: 1, Was: "reveal",
		Origin: "https://bank.example.org", Item: ids["bank"], Feld: "secret"})
	if !reply.OK {
		t.Fatalf("reveal: %s", reply.Error)
	}
	client, err := app.Client()
	if err != nil {
		t.Fatalf("verbinden: %v", app.Message(err))
	}
	lines, err := client.Log(50)
	if err != nil {
		t.Fatalf("Protokoll: %v", app.Message(err))
	}
	found := false
	for _, line := range lines {
		if line.Event == "reveal" {
			found = true
			if line.AskedBy != "browser: chrome-extension://abc/" {
				t.Errorf("im Protokoll steht %q", line.AskedBy)
			}
		}
	}
	if !found {
		t.Error("im Protokoll steht kein Zugriff")
	}
}

// Der Aufrufparameter des Browsers muss wirklich bis ins Geraeteprotokoll
// durchgereicht werden.
//
// TestNativeCallerInDeviceLog baut den Wirt selbst und prueft darum nur den
// Wirt. Diese Pruefung geht durch `osk native`, also ueber die Stelle, an
// der der Parameter uebernommen wird - gemessen am 24.10.2026: ein Mutant,
// der ihn dort wegwirft, ueberlebte die andere Pruefung, weil sie die
// Stelle gar nicht beruehrt.
func TestNativeCliPassesCallerThrough(t *testing.T) {
	app := unlockedApp(t)
	client, err := app.Client()
	if err != nil {
		t.Fatalf("verbinden: %v", app.Message(err))
	}
	id, err := NewAccess(client, "osk put").Add(
		Item{Type: ItemLogin, Name: "Durchreiche", Site: "example.org"},
		[]byte("geheim"))
	if err != nil {
		t.Fatalf("Eintrag ablegen: %v", app.Message(err))
	}

	body, err := json.Marshal(nativeRequest{ID: 1, Was: "reveal",
		Origin: "https://example.org", Item: id, Feld: "secret"})
	if err != nil {
		t.Fatalf("Anfrage: %v", err)
	}
	var header [4]byte
	binary.NativeEndian.PutUint32(header[:], uint32(len(body)))
	var out, errOut bytes.Buffer

	code := cliNative(app, []string{"native", "chrome-extension://xyz/"},
		bytes.NewReader(append(header[:], body...)), &out, &errOut)
	if code != ExitOK {
		t.Fatalf("osk native gab %d: %s", code, errOut.String())
	}
	// Zwischenpruefung: der Zugriff muss wirklich gelungen sein. Sonst
	// stuende unten einfach nichts im Protokoll, und die Pruefung waere
	// gruen, ohne etwas gemessen zu haben.
	var reply nativeReply
	if out.Len() < 4 {
		t.Fatalf("keine Antwort (%d Bytes)", out.Len())
	}
	if err := json.Unmarshal(out.Bytes()[4:], &reply); err != nil {
		t.Fatalf("Antwort: %v", err)
	}
	if !reply.OK || reply.Value != "geheim" {
		t.Fatalf("der Weg bis zur gemeinten Stelle ist nicht offen: %+v", reply)
	}

	lines, err := client.Log(50)
	if err != nil {
		t.Fatalf("Protokoll: %v", app.Message(err))
	}
	found := false
	for _, line := range lines {
		if line.Event != "reveal" || line.ItemID != id {
			continue
		}
		found = true
		if line.AskedBy != "browser: chrome-extension://xyz/" {
			t.Errorf("im Protokoll steht %q, erwartet den Namen aus dem "+
				"Aufrufparameter des Browsers", line.AskedBy)
		}
	}
	if !found {
		t.Error("im Protokoll steht kein Zugriff auf den Eintrag")
	}
}

// Die Namen, die der Browser uebergibt, werden zu lesbaren Namen - und ein
// leerer Aufrufparameter wird nicht zu einem erfundenen Namen.
func TestNativeCallerNames(t *testing.T) {
	cases := map[string]string{
		"chrome-extension://abcdefghijklmnop/": "browser: chrome-extension://abcdefghijklmnop/",
		"osk@sorglos.org":                      "browser: osk@sorglos.org",
		"":                                     "browser",
		"   ":                                  "browser",
		"\x00\x01":                             "browser",
	}
	for raw, want := range cases {
		if got := nativeCaller(raw); got != want {
			t.Errorf("%q ergab %q, erwartet %q", raw, got, want)
		}
	}
}

// Eine Laengenangabe ohne Rumpf dahinter darf nicht Gigabyte anfordern.
func TestNativeMessageLimits(t *testing.T) {
	big := make([]byte, 4)
	binary.NativeEndian.PutUint32(big, nativeMaxMessage+1)
	cases := map[string][]byte{
		"Laenge null":   {0, 0, 0, 0},
		"zu gross":      big,
		"abgeschnitten": {8, 0, 0, 0, '{', '}'},
		"kein JSON":     append([]byte{3, 0, 0, 0}, 'x', 'y', 'z'),
	}
	for name, data := range cases {
		if _, err := readNativeMessage(bytes.NewReader(data)); err == nil {
			t.Errorf("%s: wurde angenommen", name)
		}
	}
}

// Die Anmeldedatei nennt genau die erlaubte Erweiterung - und nichts sonst.
func TestNativeManifest(t *testing.T) {
	chrome, err := nativeManifestJSON("chrome", "/pfad/osk-native.sh",
		[]string{"abcdefghijklmnopabcdefghijklmnop"})
	if err != nil {
		t.Fatalf("chrome: %v", err)
	}
	var record map[string]any
	if err := json.Unmarshal(chrome, &record); err != nil {
		t.Fatalf("chrome ist kein JSON: %v", err)
	}
	if record["name"] != NativeHostName || record["type"] != "stdio" {
		t.Errorf("chrome: %+v", record)
	}
	origins, ok := record["allowed_origins"].([]any)
	if !ok || len(origins) != 1 ||
		origins[0] != "chrome-extension://abcdefghijklmnopabcdefghijklmnop/" {
		t.Errorf("chrome: allowed_origins %+v", record["allowed_origins"])
	}

	firefox, err := nativeManifestJSON("firefox", "/pfad/osk-native.sh",
		[]string{"osk@sorglos.org"})
	if err != nil {
		t.Fatalf("firefox: %v", err)
	}
	if err := json.Unmarshal(firefox, &record); err != nil {
		t.Fatalf("firefox ist kein JSON: %v", err)
	}
	if extensions, ok := record["allowed_extensions"].([]any); !ok ||
		len(extensions) != 1 || extensions[0] != "osk@sorglos.org" {
		t.Errorf("firefox: allowed_extensions %+v", record["allowed_extensions"])
	}

	// Ohne Erweiterung und ohne Pfad gibt es keine Anmeldedatei: eine
	// Anmeldedatei ohne Liste erlaubt nichts oder alles, und beides ist
	// falsch.
	if _, err := nativeManifestJSON("chrome", "/pfad", nil); err == nil {
		t.Error("eine Anmeldedatei ohne Erweiterung wurde gebaut")
	}
	if _, err := nativeManifestJSON("chrome", "", []string{"x"}); err == nil {
		t.Error("eine Anmeldedatei ohne Pfad wurde gebaut")
	}
	if _, err := nativeManifestJSON("safari", "/pfad", []string{"x"}); err == nil {
		t.Error("ein unbekannter Browser wurde beantwortet")
	}
}

// Die Kennung einer Erweiterung wird geprueft. Eine falsche in der
// Anmeldedatei heisst: jede Erweiterung darf, oder keine.
func TestNativeExtensionIDs(t *testing.T) {
	good := map[string]string{
		"chrome":  "abcdefghijklmnopabcdefghijklmnop",
		"firefox": "osk@sorglos.org",
	}
	for browser, id := range good {
		if !validExtensionID(browser, id) {
			t.Errorf("%s: %q wurde abgewiesen", browser, id)
		}
	}
	if !validExtensionID("firefox", "{12345678-1234-1234-1234-123456789abc}") {
		t.Error("firefox: eine Kennung in Klammern wurde abgewiesen")
	}
	bad := map[string][]string{
		"chrome": {
			"", "zu kurz", "abcdefghijklmnopabcdefghijklmno",
			"abcdefghijklmnopabcdefghijklmnoq", // q liegt hinter p
			"ABCDEFGHIJKLMNOPABCDEFGHIJKLMNOP",
			"abcdefghijklmnopabcdefghijklmno/",
		},
		"firefox": {
			"", "ohne-at", "@start", "ende@", "mit leerzeichen@x.org",
			"mit\"anfuehrung@x.org", "{zu-kurz}",
		},
	}
	for browser, list := range bad {
		for _, id := range list {
			if validExtensionID(browser, id) {
				t.Errorf("%s: %q wurde angenommen", browser, id)
			}
		}
	}
}
