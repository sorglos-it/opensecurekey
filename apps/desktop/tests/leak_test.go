// Die Leak-Pruefung: geht wirklich nur der Vorsatz hinaus?
//
// DAS IST DER EINZIGE TEST DIESES STRANGS, DER NICHT ANDERS GEHT. Die
// Zusage der k-Anonymitaet lautet: das Kennwort verlaesst den Rechner
// nicht, hinaus gehen die ersten fuenf Zeichen des Hashwerts. Ein Blick in
// den Quelltext beweist das nicht - "schickt den Vorsatz" und "schickt den
// ganzen Hashwert" sind eine Zeile Unterschied, und beide sehen richtig
// aus.
//
// Darum steht hier ein eigener Lauscher, und er ist ABSICHTLICH kein
// HTTP-Server: er liest die rohen Byte aus der Leitung, so wie ein
// Mitleser sie saehe. Danach wird in diesen Byte gesucht - nach dem
// Kennwort, nach dem ganzen Hashwert und nach dem Teil, der hierbleiben
// soll. Was in den Byte nicht vorkommt, ist nicht hinausgegangen.
//
// Der Hashwert wird im Test SELBST gerechnet, mit crypto/sha1. Ihn von
// leak.go rechnen zu lassen hiesse, die Behauptung mit sich selbst zu
// pruefen.
package tests

import (
	"bufio"
	"crypto/sha1"
	"encoding/hex"
	"fmt"
	"net"
	"strings"
	"sync"
	"testing"
	"time"

	"osk/classes"
)

// mitleser ist ein Lauscher, der die rohen Byte jeder Anfrage behaelt.
type mitleser struct {
	mu       sync.Mutex
	anfragen []string
	antwort  string
	listener net.Listener
}

// starteMitleser stellt den Lauscher hin und gibt seine Adresse zurueck.
func starteMitleser(t *testing.T, antwort string) *mitleser {
	t.Helper()
	listener, err := net.Listen("tcp", "127.0.0.1:0")
	if err != nil {
		t.Fatalf("kein Port fuer den Mitleser: %v", err)
	}
	m := &mitleser{antwort: antwort, listener: listener}
	go m.serve()
	t.Cleanup(func() { listener.Close() })
	return m
}

func (m *mitleser) adresse() string {
	return "http://" + m.listener.Addr().String()
}

func (m *mitleser) serve() {
	for {
		conn, err := m.listener.Accept()
		if err != nil {
			return
		}
		go m.handle(conn)
	}
}

// handle liest die Anfrage als Byte, nicht als HTTP. Genau darum geht es:
// ein HTTP-Server haette die Anfrage schon zerlegt, und was nicht in seine
// Felder passt, saehe man nicht mehr.
func (m *mitleser) handle(conn net.Conn) {
	defer conn.Close()
	conn.SetDeadline(time.Now().Add(10 * time.Second))
	reader := bufio.NewReader(conn)
	var roh strings.Builder
	for {
		line, err := reader.ReadString('\n')
		roh.WriteString(line)
		if err != nil {
			break
		}
		if line == "\r\n" || line == "\n" {
			break
		}
	}
	// Alles, was noch anliegt, gehoert mit in die Messung: ein Rumpf
	// waere der Weg, auf dem ein Kennwort doch hinausgeht.
	conn.SetReadDeadline(time.Now().Add(200 * time.Millisecond))
	rest := make([]byte, 4096)
	if n, _ := reader.Read(rest); n > 0 {
		roh.Write(rest[:n])
	}
	m.mu.Lock()
	m.anfragen = append(m.anfragen, roh.String())
	m.mu.Unlock()
	conn.SetWriteDeadline(time.Now().Add(10 * time.Second))
	fmt.Fprintf(conn, "HTTP/1.1 200 OK\r\nContent-Length: %d\r\nConnection: close\r\n\r\n%s",
		len(m.antwort), m.antwort)
}

func (m *mitleser) gesehen() []string {
	m.mu.Lock()
	defer m.mu.Unlock()
	return append([]string(nil), m.anfragen...)
}

// --- Der Nachweis -------------------------------------------------------------

// Mehrere Kennwoerter, damit ein Treffer nicht an einem einzelnen Wert
// haengt. Das dritte ist absichtlich eines mit Umlaut: eine Zeichenkodierung,
// die unterwegs umgeschrieben wird, faellt sonst nicht auf.
var leakKennwoerter = []string{"Password", "hunter2", "Käsekuchen-42", "x", strings.Repeat("a", 300)}

func TestLeakSchicktNurDieErstenFuenfHashzeichen(t *testing.T) {
	for _, kennwort := range leakKennwoerter {
		// Der Hashwert wird hier gerechnet, nicht von leak.go geholt.
		sum := sha1.Sum([]byte(kennwort))
		ganz := strings.ToUpper(hex.EncodeToString(sum[:]))
		vorsatz := ganz[:classes.LeakPrefixLen]
		rest := ganz[classes.LeakPrefixLen:]

		// Die Antwort enthaelt den Rest NICHT - sonst stuende er durch
		// die Antwort in der Leitung und die Suche unten waere wertlos.
		m := starteMitleser(t, "0018A45C4D1DEF81644B54AB7F969B88D65:1\r\n")
		checker := &classes.LeakChecker{BaseURL: m.adresse()}
		if _, err := checker.Check([]byte(kennwort),
			classes.LeakConsent{Allowed: true}); err != nil {
			t.Fatalf("%q: %v", kennwort, err)
		}
		gesehen := m.gesehen()
		if len(gesehen) != 1 {
			t.Fatalf("%q: %d Anfragen, erwartet 1", kennwort, len(gesehen))
		}
		roh := gesehen[0]

		// 1. Der Vorsatz muss drinstehen, und zwar als Pfad.
		if !strings.Contains(roh, "/range/"+vorsatz) {
			t.Errorf("%q: der Vorsatz %s steht nicht im Pfad:\n%s",
				kennwort, vorsatz, roh)
		}
		// 2. Das Kennwort darf NICHT drinstehen - in keiner Schreibweise.
		for _, form := range []string{kennwort, strings.ToUpper(kennwort),
			strings.ToLower(kennwort)} {
			if len(form) >= 3 && strings.Contains(roh, form) {
				t.Errorf("%q: das Kennwort steht in der Leitung:\n%s", kennwort, roh)
			}
		}
		// 3. Der Teil des Hashwerts, der hierbleiben soll, darf NICHT
		//    drinstehen. Das ist der eigentliche Punkt: wer den ganzen
		//    Hashwert schickt, hat das Kennwort verraten, auch wenn das
		//    Kennwort selbst nicht in der Leitung steht.
		for _, form := range []string{rest, strings.ToLower(rest)} {
			if strings.Contains(roh, form) {
				t.Errorf("%q: der Rest des Hashwerts steht in der Leitung:\n%s",
					kennwort, roh)
			}
		}
		// 4. Auch kein Stueck davon. Schon acht Zeichen mehr machen die
		//    Gruppe so klein, dass die Zusage nichts mehr wert ist.
		for _, form := range []string{rest[:8], strings.ToLower(rest[:8])} {
			if strings.Contains(roh, form) {
				t.Errorf("%q: der Anfang des Rests steht in der Leitung:\n%s",
					kennwort, roh)
			}
		}
		// 5. Und kein Rumpf. Eine Anfrage mit Rumpf wuerde oben
		//    mitgelesen; hier wird geprueft, dass nach dem Kopf nichts
		//    mehr kam.
		if cut := strings.Index(roh, "\r\n\r\n"); cut >= 0 {
			if nachKopf := strings.TrimSpace(roh[cut+4:]); nachKopf != "" {
				t.Errorf("%q: nach dem Kopf steht noch etwas: %q", kennwort, nachKopf)
			}
		}
	}
}

// Ohne Zustimmung geht NICHTS hinaus. Gemessen wird das nicht am
// Rueckgabewert, sondern am Lauscher: er darf keine einzige Verbindung
// gesehen haben.
func TestLeakOhneZustimmungGehtNichtsHinaus(t *testing.T) {
	m := starteMitleser(t, "ABC:1\r\n")
	checker := &classes.LeakChecker{BaseURL: m.adresse()}
	for _, zustimmung := range []classes.LeakConsent{
		{},
		{Allowed: false},
		{Allowed: false, Remember: true},
	} {
		if _, err := checker.Check([]byte("Password"), zustimmung); err == nil {
			t.Errorf("%+v: angenommen, erwartet war eine Absage", zustimmung)
		}
	}
	// Und auch der Weg ueber den ganzen Tresor fragt nicht.
	if _, err := classes.LeakScan(nil, checker, 0, classes.LeakConsent{}); err == nil {
		t.Error("LeakScan ohne Zustimmung angenommen")
	}
	if gesehen := m.gesehen(); len(gesehen) != 0 {
		t.Fatalf("ohne Zustimmung sind %d Anfragen hinausgegangen:\n%v",
			len(gesehen), gesehen)
	}
}

// Gefunden und nicht gefunden - beides muss stimmen, und "nicht gefunden"
// darf nicht daran liegen, dass gar nicht gesucht wurde.
func TestLeakFindetUndFindetNicht(t *testing.T) {
	kennwort := []byte("Password")
	sum := sha1.Sum(kennwort)
	ganz := strings.ToUpper(hex.EncodeToString(sum[:]))
	rest := ganz[classes.LeakPrefixLen:]

	// Die Antwort enthaelt den Rest mit der Zahl 1234.
	mitTreffer := starteMitleser(t,
		"0000000000000000000000000000000000A:7\r\n"+rest+":1234\r\n"+
			"FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF:9\r\n")
	checker := &classes.LeakChecker{BaseURL: mitTreffer.adresse()}
	ergebnis, err := checker.Check(append([]byte(nil), kennwort...),
		classes.LeakConsent{Allowed: true})
	if err != nil {
		t.Fatalf("mit Treffer: %v", err)
	}
	if ergebnis.Count != 1234 {
		t.Errorf("Zahl %d, erwartet 1234", ergebnis.Count)
	}
	if ergebnis.GroupSize != 3 {
		t.Errorf("Gruppe %d, erwartet 3", ergebnis.GroupSize)
	}
	if ergebnis.Sent != ganz[:classes.LeakPrefixLen] {
		t.Errorf("gesendet %q, erwartet %q", ergebnis.Sent,
			ganz[:classes.LeakPrefixLen])
	}

	// Dieselbe Gruppe ohne den Rest: nicht gefunden.
	ohne := starteMitleser(t,
		"0000000000000000000000000000000000A:7\r\n"+
			"FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF:9\r\n")
	checker2 := &classes.LeakChecker{BaseURL: ohne.adresse()}
	ergebnis2, err := checker2.Check(append([]byte(nil), kennwort...),
		classes.LeakConsent{Allowed: true})
	if err != nil {
		t.Fatalf("ohne Treffer: %v", err)
	}
	if ergebnis2.Count != 0 {
		t.Errorf("Zahl %d, erwartet 0", ergebnis2.Count)
	}

	// Kleinschreibung in der Antwort muss genauso gefunden werden - dass
	// der Dienst gross schreibt, ist nirgends zugesagt.
	klein := starteMitleser(t, strings.ToLower(rest)+":5\r\n")
	checker3 := &classes.LeakChecker{BaseURL: klein.adresse()}
	ergebnis3, err := checker3.Check(append([]byte(nil), kennwort...),
		classes.LeakConsent{Allowed: true})
	if err != nil {
		t.Fatalf("klein geschrieben: %v", err)
	}
	if ergebnis3.Count != 5 {
		t.Errorf("klein geschrieben: Zahl %d, erwartet 5", ergebnis3.Count)
	}
}

// Die Antwort kommt von aussen und ist damit eine fremde Eingabe wie jede
// andere. Keine davon darf mehr ergeben als einen Fehler - und keine darf
// aus einem betroffenen Kennwort ein unbetroffenes machen.
func TestLeakAntwortIstFremdeEingabe(t *testing.T) {
	kennwort := []byte("Password")
	sum := sha1.Sum(kennwort)
	rest := strings.ToUpper(hex.EncodeToString(sum[:]))[classes.LeakPrefixLen:]

	faelle := []struct {
		name    string
		antwort string
		// treffer ist die erwartete Zahl, -1 heisst: ein Fehler ist
		// richtig.
		treffer int64
	}{
		{"leere Antwort", "", -1},
		{"nur Leerzeilen", "\r\n\r\n\r\n", -1},
		{"kein Hexadezimal", "ZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZ:1\r\n", -1},
		{"zu kurze Zeilen", "ABC:1\r\nDEF:2\r\n", -1},
		{"Zeile ohne Doppelpunkt", rest + "\r\n", 1},
		{"Zahl unlesbar", rest + ":keine-zahl\r\n", 1},
		{"Zahl zu gross", rest + ":99999999999999999999999999\r\n", 1},
		{"Zahl negativ", rest + ":-5\r\n", 1},
		{"Auffuellzeilen dazwischen", "kurz\r\n" + rest + ":3\r\nnoch-kurz\r\n", 3},
		{"ohne Zeilenumbruch am Ende", rest + ":8", 8},
		{"Zeilenumbruch nur mit \\n", rest + ":9\n", 9},
	}
	for _, fall := range faelle {
		m := starteMitleser(t, fall.antwort)
		checker := &classes.LeakChecker{BaseURL: m.adresse()}
		ergebnis, err := checker.Check(append([]byte(nil), kennwort...),
			classes.LeakConsent{Allowed: true})
		if fall.treffer < 0 {
			if err == nil {
				t.Errorf("%s: angenommen (Zahl %d), erwartet war ein Fehler",
					fall.name, ergebnis.Count)
			}
			continue
		}
		if err != nil {
			t.Errorf("%s: %v", fall.name, err)
			continue
		}
		if int64(ergebnis.Count) != fall.treffer {
			t.Errorf("%s: Zahl %d, erwartet %d", fall.name, ergebnis.Count,
				fall.treffer)
		}
	}
}

// Ein Dienst, der nicht antwortet oder mit einem Fehler antwortet, ergibt
// eine Meldung - und niemals "nicht betroffen". Das ist der Unterschied,
// auf den es ankommt: ein stiller Fehlschlag sähe wie ein gutes Ergebnis
// aus.
func TestLeakFehlschlagIstNichtUnbetroffen(t *testing.T) {
	// Ein Port, auf dem niemand lauscht.
	listener, err := net.Listen("tcp", "127.0.0.1:0")
	if err != nil {
		t.Fatalf("kein Port: %v", err)
	}
	adresse := "http://" + listener.Addr().String()
	listener.Close()
	checker := &classes.LeakChecker{BaseURL: adresse}
	ergebnis, err := checker.Check([]byte("Password"), classes.LeakConsent{Allowed: true})
	if err == nil {
		t.Fatalf("ein toter Dienst ergab ein Ergebnis: %+v", ergebnis)
	}
}

// Der Tresor am Stueck: was nicht geprueft werden konnte, wird GENANNT und
// nicht uebersprungen. Ein Bericht, in dem die Haelfte fehlt, sieht sonst
// aus wie ein sauberer Tresor.
func TestLeakScanNenntDasUngeprueefte(t *testing.T) {
	app := newApp(t, startSimulator(t))
	client, err := app.Client()
	if err != nil {
		t.Fatalf("verbinden: %v", app.Message(err))
	}
	if err := client.Unlock([]byte(simPin)); err != nil {
		t.Fatalf("unlock: %v", app.Message(err))
	}
	// Ein Zugang, der herauskommt, und einer, der chipgebunden ist.
	if _, err := client.Add(classes.Item{Type: classes.ItemLogin,
		Name: "Leak gespiegelt", Site: "a.example", Mirrored: true},
		[]byte("Password")); err != nil {
		t.Fatalf("anlegen: %v", app.Message(err))
	}
	if _, err := client.Add(classes.Item{Type: classes.ItemLogin,
		Name: "Leak chipgebunden", Site: "b.example", Mirrored: false},
		[]byte("Password")); err != nil {
		t.Fatalf("anlegen: %v", app.Message(err))
	}

	sum := sha1.Sum([]byte("Password"))
	rest := strings.ToUpper(hex.EncodeToString(sum[:]))[classes.LeakPrefixLen:]
	m := starteMitleser(t, rest+":77\r\n")
	checker := &classes.LeakChecker{BaseURL: m.adresse()}

	findings, err := classes.LeakScan(client, checker, 0,
		classes.LeakConsent{Allowed: true})
	if err != nil {
		t.Fatalf("LeakScan: %v", app.Message(err))
	}
	gespiegelt := false
	chipgebunden := false
	for _, finding := range findings {
		switch finding.Name {
		case "Leak gespiegelt":
			gespiegelt = true
			if finding.Count != 77 {
				t.Errorf("gespiegelt: Zahl %d, erwartet 77", finding.Count)
			}
			if finding.ReasonKey != "" {
				t.Errorf("gespiegelt: Grund %q, erwartet keinen", finding.ReasonKey)
			}
		case "Leak chipgebunden":
			chipgebunden = true
			if finding.ReasonKey == "" {
				t.Error("chipgebunden: kein Grund genannt - der Eintrag waere " +
					"stillschweigend als geprueft durchgegangen")
			}
			if finding.Count != 0 {
				t.Errorf("chipgebunden: Zahl %d, erwartet 0", finding.Count)
			}
		}
	}
	if !gespiegelt || !chipgebunden {
		t.Fatalf("nicht beide Eintraege im Bericht: gespiegelt=%v chipgebunden=%v",
			gespiegelt, chipgebunden)
	}
}

// Und der Vollstaendigkeit halber: dass die Zahl der Zeichen, die hinausgeht,
// an EINER Stelle steht und nicht nebenbei fuenf ist.
func TestLeakVorsatzLaengeIstDieZahlAusDerQuelle(t *testing.T) {
	m := starteMitleser(t, "0018A45C4D1DEF81644B54AB7F969B88D65:1\r\n")
	checker := &classes.LeakChecker{BaseURL: m.adresse()}
	ergebnis, err := checker.Check([]byte("Password"), classes.LeakConsent{Allowed: true})
	if err != nil {
		t.Fatalf("leak: %v", err)
	}
	if len(ergebnis.Sent) != classes.LeakPrefixLen {
		t.Fatalf("gesendet %d Zeichen, LeakPrefixLen ist %d",
			len(ergebnis.Sent), classes.LeakPrefixLen)
	}
	gesehen := m.gesehen()
	if len(gesehen) != 1 {
		t.Fatalf("%d Anfragen", len(gesehen))
	}
	// Die Zeile der Anfrage sieht so aus: GET /range/XXXXX HTTP/1.1
	zeile := strings.SplitN(gesehen[0], "\r\n", 2)[0]
	felder := strings.Fields(zeile)
	if len(felder) != 3 || felder[0] != "GET" {
		t.Fatalf("unerwartete Anfragezeile: %q", zeile)
	}
	pfad := strings.TrimPrefix(felder[1], "/range/")
	if len(pfad) != classes.LeakPrefixLen {
		t.Fatalf("der Pfad traegt %d Zeichen, erwartet %d: %q",
			len(pfad), classes.LeakPrefixLen, felder[1])
	}
	// Und kein Fragezeichen: ein Wert in der Abfrage waere eine Angabe,
	// die in jedem Protokoll eines Vermittlers landet.
	if strings.ContainsAny(felder[1], "?&=") {
		t.Fatalf("die Adresse traegt eine Abfrage: %q", felder[1])
	}
}
