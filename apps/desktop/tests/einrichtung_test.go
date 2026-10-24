// Der Einrichtungsassistent, durchgespielt ueber echtes HTTP.
//
// Geprueft wird die Reihenfolge und das, was sich nicht umgehen laesst:
//
//   - Solange das Geraet im Werkszustand ist, fuehrt JEDE Seite zum
//     Assistenten. Ein Lesezeichen auf die Uebersicht hilft nicht.
//   - Ohne Betriebsart, ohne Bestaetigung oder mit zu kurzer PIN richtet
//     sich nichts ein. Es gibt keine stille Vorgabe.
//   - Nach dem Einrichten ist die Betriebsart fest: ein zweiter Versuch
//     wird abgewiesen, und das Geraet hat genau einmal eingerichtet.
//   - Danach ist die Oberflaeche frei, und der Notfallzettel nennt die
//     Betriebsart.
//
// Kein echtes Geraet ist beteiligt: gespielt wird gegen die Attrappe mit
// -werkszustand. Dass es dieselbe Attrappe ist wie ueberall sonst, ist
// Absicht - zwei Attrappen wuerden auseinanderlaufen, und dann prueft jeder
// gegen eine andere Vorstellung vom Geraet.
package tests

import (
	"bytes"
	"html"
	"io"
	"net/http"
	"strings"
	"testing"
	"time"

	"osk/classes"
)

// oberflaeche startet Server und Geraet und gibt beides zurueck.
func oberflaeche(t *testing.T, address string) (*classes.App, string, string,
	*http.Client) {
	t.Helper()
	app := newApp(t, address)
	server, err := classes.NewServer(app)
	if err != nil {
		t.Fatalf("Server: %v", app.Message(err))
	}
	t.Cleanup(func() { server.Close() })
	go server.Serve()
	base := server.Address()
	return app, base, strings.TrimSuffix(base, "/"), &http.Client{
		Timeout: 30 * time.Second,
		// Keine Umleitung von selbst folgen: ob umgeleitet wird, ist hier
		// die Messung.
		CheckRedirect: func(*http.Request, []*http.Request) error {
			return http.ErrUseLastResponse
		},
	}
}

// hole ruft eine Seite ab und gibt Code und Ziel der Umleitung.
func hole(t *testing.T, client *http.Client, url string) (int, string, []byte) {
	t.Helper()
	answer, err := client.Get(url)
	if err != nil {
		t.Fatalf("%s: %v", url, err)
	}
	defer answer.Body.Close()
	body, _ := io.ReadAll(answer.Body)
	return answer.StatusCode, answer.Header.Get("Location"), body
}

// Im Werkszustand fuehrt jede Seite zum Assistenten.
func TestAssistentLaesstSichNichtUeberspringen(t *testing.T) {
	_, base, _, client := oberflaeche(t, startSimulator(t, "-werkszustand"))

	for _, path := range []string{"", "item", "certs", "devices", "backup",
		"firmware", "settings", "sheet"} {
		code, ziel, _ := hole(t, client, base+path)
		if code != http.StatusSeeOther || ziel != "/setup" {
			t.Errorf("%q antwortet %d nach %q, erwartet 303 nach /setup",
				path, code, ziel)
		}
	}
	// Der Assistent selbst wird natuerlich ausgeliefert - sonst gaebe es
	// einen Kreis, und niemand kaeme weiter.
	code, _, body := hole(t, client, base+"setup")
	if code != http.StatusOK {
		t.Fatalf("/setup antwortet %d", code)
	}
	// Die unumkehrbare Frage und beide Preise stehen auf der Seite.
	for _, key := range []string{"setup.question", "setup.final",
		"setup.mirrored.cost", "setup.only.cost", "setup.recommend",
		"setup.confirm"} {
		if !enthaelt(body, textOf(t, client, base, key)) {
			t.Errorf("der Text %q fehlt auf der Seite", key)
		}
	}
	// Keine stille Vorgabe: kein Auswahlfeld ist vorbelegt.
	if bytes.Contains(body, []byte("checked")) {
		t.Error("eine der Betriebsarten ist vorbelegt - es darf keine " +
			"Vorgabe geben")
	}
}

// textOf holt einen Text in der Sprache der Oberflaeche.
func textOf(t *testing.T, client *http.Client, base, key string) string {
	t.Helper()
	texts := apiCall(t, client, strings.TrimSuffix(base, "/"),
		http.MethodGet, base+"api/texts", "")
	data, ok := texts["data"].(map[string]any)
	if !ok {
		t.Fatalf("die Texte fehlen")
	}
	value, ok := data[key].(string)
	if !ok || value == "" {
		t.Fatalf("der Text %q fehlt", key)
	}
	return value
}

// enthaelt sagt, ob der Text auf der Seite steht. Die Vorlage maskiert
// Anfuehrungszeichen und Umlaute, darum wird gegen die maskierte Form
// gemessen - sonst waere ein Text mit Anfuehrungszeichen nie zu finden.
func enthaelt(body []byte, text string) bool {
	return bytes.Contains(body, []byte(text)) ||
		bytes.Contains(body, []byte(html.EscapeString(text)))
}

// Der halbe Weg richtet nichts ein: keine Betriebsart, keine Bestaetigung,
// zu kurze PIN - jedes Mal eine Absage.
func TestAssistentWeistHalbeAngabenAb(t *testing.T) {
	app, base, origin, client := oberflaeche(t,
		startSimulator(t, "-werkszustand"))

	faelle := []struct {
		name string
		body string
	}{
		// Die drei wichtigsten Zeilen hier: eine vergessene, eine leere
		// und eine auf null gesetzte Angabe duerfen NIE zu einer
		// Betriebsart werden. Darum faengt die Zaehlung bei 1 an - waere
		// eine der Arten die Null, haette ein Versaeumnis still gewaehlt.
		{"ohne Betriebsart", `{"pin":"123456","confirm":true}`},
		{"Betriebsart null statt Zahl", `{"mode":null,"pin":"123456","confirm":true}`},
		{"Betriebsart 0", `{"mode":0,"pin":"123456","confirm":true}`},
		{"dritte Betriebsart", `{"mode":3,"pin":"123456","confirm":true}`},
		{"ohne Bestaetigung", `{"mode":1,"pin":"123456","confirm":false}`},
		{"PIN zu kurz", `{"mode":1,"pin":"12345","confirm":true}`},
	}
	for _, fall := range faelle {
		t.Run(fall.name, func(t *testing.T) {
			answer := apiCall(t, client, origin, http.MethodPost,
				base+"api/setup", fall.body)
			if ok, _ := answer["ok"].(bool); ok {
				t.Errorf("angenommen, erwartet abgewiesen")
			}
			if text, _ := answer["error"].(string); text == "" {
				t.Errorf("kein Grund genannt")
			}
		})
	}
	// Und das Geraet ist unveraendert: noch im Werkszustand, ohne
	// Betriebsart. Gemessen am Geraet, nicht am Programm - das Programm
	// koennte sich irren.
	info := helloVon(t, app)
	if !info.Factory || info.ModeOf(0) != classes.ModeNoneSet {
		t.Errorf("factory=%v mode=%d, erwartet Werkszustand ohne Betriebsart",
			info.Factory, info.ModeOf(0))
	}
}

// helloVon fragt das Geraet nach seinem Zustand.
func helloVon(t *testing.T, app *classes.App) classes.DeviceInfo {
	t.Helper()
	client, err := app.Client()
	if err != nil {
		t.Fatalf("verbinden: %v", app.Message(err))
	}
	info, err := client.Hello()
	if err != nil {
		t.Fatalf("hello: %v", app.Message(err))
	}
	return info
}

// Der ganze Weg: Betriebsart, PIN, Sicherung, Zettel - und danach ist die
// Wahl fest.
func TestAssistentGanzerWeg(t *testing.T) {
	app, base, origin, client := oberflaeche(t,
		startSimulator(t, "-werkszustand"))

	// Schritt 1 und 2: Betriebsart und PIN gehen zusammen zum Geraet.
	answer := apiCall(t, client, origin, http.MethodPost, base+"api/setup",
		`{"mode":2,"pin":"123456","confirm":true}`)
	if ok, _ := answer["ok"].(bool); !ok {
		t.Fatalf("einrichten abgewiesen: %v", answer["error"])
	}
	// Am Geraet nachgesehen, nicht beim Programm nachgefragt.
	if art := helloVon(t, app).ModeOf(0); art != classes.ModeOnly {
		t.Fatalf("Betriebsart %d, erwartet %d", art, classes.ModeOnly)
	}

	// Die Wahl ist endgueltig: ein zweiter Versuch wird abgewiesen, und das
	// Geraet hat genau einmal eingerichtet.
	zweiter := apiCall(t, client, origin, http.MethodPost, base+"api/setup",
		`{"mode":1,"pin":"654321","confirm":true}`)
	if ok, _ := zweiter["ok"].(bool); ok {
		t.Error("die Betriebsart liess sich ein zweites Mal setzen")
	}
	// Auch die Betriebsart selbst ist unveraendert - eine Absage, die
	// trotzdem etwas umgestellt haette, waere das Schlimmste von beidem.
	if art := helloVon(t, app).ModeOf(0); art != classes.ModeOnly {
		t.Errorf("Betriebsart nach dem zweiten Versuch %d, erwartet %d",
			art, classes.ModeOnly)
	}

	// Jetzt ist die Oberflaeche frei - keine Umleitung mehr.
	code, ziel, _ := hole(t, client, base)
	if code != http.StatusOK {
		t.Errorf("die Uebersicht antwortet %d nach %q, erwartet 200",
			code, ziel)
	}

	// Schritt 3: die Sicherung - OHNE ein zweites Entsperren. Der Anwender
	// hat die PIN eben selbst gesetzt; ihn sofort noch einmal danach zu
	// fragen, waere die Art von Rueckfrage, die man wegdrueckt. Im Browser
	// war genau das der Fehler: der Stick war eingerichtet, aber zu, und
	// die Sicherung scheiterte mit "Das Geraet ist gesperrt".
	sicherung := apiCall(t, client, origin, http.MethodPost, base+"api/backup", "{}")
	if ok, _ := sicherung["ok"].(bool); !ok {
		t.Fatalf("Sicherung abgewiesen: %v", sicherung["error"])
	}
	blob, _ := sicherung["data"].(map[string]any)["blob"].(string)
	if blob == "" {
		t.Error("die Sicherung ist leer")
	}

	// Schritt 4: der Notfallzettel. Er nennt die Betriebsart und warnt bei
	// "nur dieser Stick", dass die Sicherung den Chip nicht zurueckbringt.
	code, _, body := hole(t, client, base+"sheet")
	if code != http.StatusOK {
		t.Fatalf("/sheet antwortet %d", code)
	}
	for _, key := range []string{"sheet.only.warn", "sheet.file.title",
		"sheet.pass.title", "sheet.shares.title", "sheet.steps.title",
		"sheet.nopin", "setup.only.title"} {
		if !enthaelt(body, textOf(t, client, base, key)) {
			t.Errorf("der Text %q fehlt auf dem Zettel", key)
		}
	}
	// Und das Wichtigste: die PIN steht nicht darauf.
	if bytes.Contains(body, []byte("123456")) {
		t.Error("die PIN steht auf dem Notfallzettel")
	}
}

// Bei "Ersatz moeglich" darf die Warnung des anderen Weges NICHT auf dem
// Zettel stehen. Ohne diese Gegenprobe waere der Test oben auch gruen, wenn
// die Warnung immer gedruckt wuerde.
func TestZettelWarntNurBeiNurDiesemStick(t *testing.T) {
	_, base, origin, client := oberflaeche(t,
		startSimulator(t, "-werkszustand"))
	apiCall(t, client, origin, http.MethodPost, base+"api/setup",
		`{"mode":1,"pin":"123456","confirm":true}`)

	code, _, body := hole(t, client, base+"sheet")
	if code != http.StatusOK {
		t.Fatalf("/sheet antwortet %d", code)
	}
	warnung := textOf(t, client, base, "sheet.only.warn")
	if enthaelt(body, warnung) {
		t.Error("die Warnung fuer \"nur dieser Stick\" steht auf dem Zettel " +
			"eines Profils mit Ersatz")
	}
	if !enthaelt(body, textOf(t, client, base, "setup.mirrored.title")) {
		t.Error("der Zettel nennt die Betriebsart nicht")
	}
}

// Ohne Geraet bleibt die Oberflaeche bedienbar: es gibt nichts
// einzurichten, und der Assistent darf niemanden einsperren.
func TestOhneGeraetKeineUmleitung(t *testing.T) {
	_, base, _, client := oberflaeche(t, "")
	code, ziel, _ := hole(t, client, base)
	if code != http.StatusOK {
		t.Errorf("die Uebersicht antwortet %d nach %q, erwartet 200",
			code, ziel)
	}
}

// Die Gegenprobe zum Werkszustand: die Attrappe OHNE den Schalter verhaelt
// sich wie bisher. Sie ist eingerichtet, fuehrt niemanden in den Assistenten
// und nimmt kein `setup` mehr an. Ohne diese Pruefung waere die Ergaenzung
// an der Attrappe nicht nachweisbar additiv.
func TestEingerichteteAttrappeBleibtWieSieWar(t *testing.T) {
	app, base, origin, client := oberflaeche(t, startSimulator(t))

	info := helloVon(t, app)
	if info.Factory {
		t.Error("die gewoehnliche Attrappe meldet Werkszustand")
	}
	if art := info.ModeOf(0); art != classes.ModeMirrored {
		t.Errorf("Betriebsart %d, erwartet %d", art, classes.ModeMirrored)
	}

	// Keine Umleitung: die Oberflaeche ist sofort bedienbar.
	code, ziel, _ := hole(t, client, base)
	if code != http.StatusOK {
		t.Errorf("die Uebersicht antwortet %d nach %q, erwartet 200", code, ziel)
	}

	// Und ein `setup` auf ein eingerichtetes Geraet wird abgewiesen.
	answer := apiCall(t, client, origin, http.MethodPost, base+"api/setup",
		`{"mode":2,"pin":"123456","confirm":true}`)
	if ok, _ := answer["ok"].(bool); ok {
		t.Error("die eingerichtete Attrappe nahm ein zweites setup an")
	}
	if art := helloVon(t, app).ModeOf(0); art != classes.ModeMirrored {
		t.Errorf("Betriebsart nach dem Versuch %d, erwartet %d",
			art, classes.ModeMirrored)
	}
}
