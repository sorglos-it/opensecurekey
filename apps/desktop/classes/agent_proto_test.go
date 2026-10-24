// Der ssh-Agent von Anfang bis Ende gegen die Attrappe.
//
// Geprueft wird nicht der Agent allein, sondern der ganze Weg: Schluessel in
// den Tresor legen, den Agenten nach seinen Identitaeten fragen,
// unterschreiben lassen und die Unterschrift mit der Standardbibliothek
// nachpruefen. Dazwischen liegt wirklich das Protokoll - die Nachrichten
// gehen als Bytes ueber eine Verbindung, nicht als Funktionsaufruf.
package classes

import (
	"bytes"
	"crypto/ed25519"
	"net"
	"os"
	"os/exec"
	"path/filepath"
	"runtime"
	"strconv"
	"sync"
	"testing"
	"time"
)

const testPin = "123456"

var (
	simOnce sync.Once
	simPath string
	simErr  error
)

// simulatorBinary baut die Attrappe genau einmal. Als eigenes Programm, nicht
// ueber "go run": sonst bliebe beim Beenden das Kindprogramm stehen.
func simulatorBinary(t *testing.T) string {
	t.Helper()
	simOnce.Do(func() {
		name := "osk-agent-sim"
		if runtime.GOOS == "windows" {
			name += ".exe"
		}
		simPath = filepath.Join(t.TempDir(), name)
		build := exec.Command("go", "build", "-o", simPath, "../tests/simulator")
		build.Env = append(os.Environ(), "CGO_ENABLED=0")
		out, err := build.CombinedOutput()
		if err != nil {
			simErr = err
			t.Log(string(out))
		}
	})
	if simErr != nil {
		// Laut und mit Grund: eine Pruefung, die sich bei einem
		// Umgebungsfehler still abschaltet, ist die schlimmste Messung.
		t.Skipf("die Attrappe laesst sich nicht bauen, darum wurde NICHTS geprueft: %v", simErr)
	}
	return simPath
}

// startSimulator startet die Attrappe und gibt ihre Adresse zurueck.
func startSimulator(t *testing.T) string {
	t.Helper()
	port := freeTestPort(t)
	cmd := exec.Command(simulatorBinary(t), "-port", strconv.Itoa(port))
	var output bytes.Buffer
	cmd.Stdout, cmd.Stderr = &output, &output
	if err := cmd.Start(); err != nil {
		t.Skipf("die Attrappe laesst sich nicht starten, darum wurde NICHTS geprueft: %v", err)
	}
	address := "127.0.0.1:" + strconv.Itoa(port)
	t.Cleanup(func() {
		if cmd.Process != nil {
			cmd.Process.Kill()
		}
		cmd.Wait()
		if t.Failed() {
			t.Log("Attrappe:", output.String())
		}
	})
	deadline := time.Now().Add(20 * time.Second)
	for time.Now().Before(deadline) {
		conn, err := net.DialTimeout("tcp", address, time.Second)
		if err == nil {
			conn.Close()
			return address
		}
		time.Sleep(50 * time.Millisecond)
	}
	t.Fatalf("die Attrappe antwortet nicht auf %s: %s", address, output.String())
	return ""
}

func freeTestPort(t *testing.T) int {
	t.Helper()
	listener, err := net.Listen("tcp", "127.0.0.1:0")
	if err != nil {
		t.Fatalf("kein freier Port: %v", err)
	}
	defer listener.Close()
	return listener.Addr().(*net.TCPAddr).Port
}

// unlockedApp baut einen Kern, der mit der Attrappe verbunden und entsperrt
// ist. Jede Pruefung bekommt ihre eigene Attrappe - sonst sieht die zweite
// Pruefung die Eintraege der ersten.
func unlockedApp(t *testing.T) *App {
	t.Helper()
	cfg := DefaultConfig()
	cfg.SimAddress = startSimulator(t)
	cfg.Lang = "de"
	app := NewApp(os.DirFS(".."), cfg)
	t.Cleanup(app.Disconnect)
	client, err := app.Client()
	if err != nil {
		t.Fatalf("verbinden: %v", app.Message(err))
	}
	if err := client.Unlock([]byte(testPin)); err != nil {
		t.Fatalf("entsperren: %v", app.Message(err))
	}
	return app
}

// agentPair verbindet einen Agenten mit einer Verbindung im Arbeitsspeicher.
// Die Nachrichten gehen als Bytes hin und zurueck - genau wie bei ssh.
func agentPair(t *testing.T, app *App, peer string) (net.Conn, *Agent) {
	t.Helper()
	agent := NewAgent(app)
	ours, theirs := net.Pipe()
	go agent.Serve(theirs, peer)
	t.Cleanup(func() { ours.Close() })
	return ours, agent
}

// ask schickt eine Nachricht und liest die Antwort.
func ask(t *testing.T, conn net.Conn, request []byte) []byte {
	t.Helper()
	if err := conn.SetDeadline(time.Now().Add(30 * time.Second)); err != nil {
		t.Fatalf("Zeitlimit setzen: %v", err)
	}
	if err := writeAgentMessage(conn, request); err != nil {
		t.Fatalf("senden: %v", err)
	}
	answer, err := readAgentMessage(conn)
	if err != nil {
		t.Fatalf("lesen: %v", err)
	}
	return answer
}

// Der ganze Weg: Schluessel hineinlegen, Identitaet abfragen, unterschreiben
// lassen, Unterschrift nachpruefen.
func TestAgentSignsForSimulator(t *testing.T) {
	app := unlockedApp(t)
	client, err := app.Client()
	if err != nil {
		t.Fatalf("verbinden: %v", app.Message(err))
	}
	access := NewAccess(client, "osk put")
	id, err := access.Add(Item{Type: ItemSSHKey, Name: "Pruefschluessel",
		Site: "git.example.org"}, []byte(goldenEd25519Private))
	if err != nil {
		t.Fatalf("Schluessel ablegen: %v", app.Message(err))
	}
	if id == 0 {
		t.Fatal("die Attrappe hat keine Nummer vergeben")
	}

	conn, _ := agentPair(t, app, "ssh.exe")

	// 1. Welche Schluessel hast du?
	answer := ask(t, conn, []byte{agentRequestIdentities})
	if len(answer) == 0 || answer[0] != agentIdentitiesAnswer {
		t.Fatalf("auf die Frage nach den Identitaeten kam %v", answer)
	}
	reader := newWireReader(answer[1:])
	count, err := reader.Uint32()
	if err != nil {
		t.Fatalf("Anzahl lesen: %v", err)
	}
	if count != 1 {
		t.Fatalf("%d Identitaeten genannt, erwartet 1", count)
	}
	blob, err := reader.String()
	if err != nil {
		t.Fatalf("oeffentlichen Teil lesen: %v", err)
	}
	blob = append([]byte(nil), blob...)
	comment, err := reader.Name()
	if err != nil {
		t.Fatalf("Namen lesen: %v", err)
	}
	if comment != "Pruefschluessel" {
		t.Errorf("Name %q, erwartet den Namen aus dem Tresor", comment)
	}
	if !reader.Done() {
		t.Error("hinter der Antwort steht noch etwas")
	}
	// Der genannte oeffentliche Teil muss der des Schluessels sein.
	want, err := ParseSSHKey([]byte(goldenEd25519Private))
	if err != nil {
		t.Fatalf("Vergleichsschluessel: %v", err)
	}
	defer want.Wipe()
	if !bytes.Equal(blob, want.Blob) {
		t.Fatal("der Agent nennt einen anderen oeffentlichen Teil als den des Schluessels")
	}

	// 2. Unterschreibe das hier.
	data := []byte("was ssh zu unterschreiben gibt")
	request := &wireWriter{}
	request.Byte(agentSignRequest)
	request.String(blob)
	request.String(data)
	request.Uint32(0)
	answer = ask(t, conn, request.Bytes())
	if len(answer) == 0 || answer[0] != agentSignResponse {
		t.Fatalf("auf den Unterschriftswunsch kam %v", answer)
	}
	reader = newWireReader(answer[1:])
	signature, err := reader.String()
	if err != nil {
		t.Fatalf("Unterschrift lesen: %v", err)
	}
	if !reader.Done() {
		t.Error("hinter der Unterschrift steht noch etwas")
	}
	algorithm, raw := splitSignature(t, signature)
	if algorithm != algEd25519 {
		t.Fatalf("Verfahren %q", algorithm)
	}
	public := want.ed25519Key.Public().(ed25519.PublicKey)
	if !ed25519.Verify(public, data, raw) {
		t.Fatal("die Unterschrift des Agenten haelt nicht")
	}

	// 3. Im Geraeteprotokoll muss jeder Zugriff mit Namen stehen.
	lines, err := client.Log(50)
	if err != nil {
		t.Fatalf("Protokoll lesen: %v", err)
	}
	reveals := 0
	for _, line := range lines {
		if line.Event != "reveal" {
			continue
		}
		reveals++
		if line.AskedBy != AgentAskedBy("ssh.exe") {
			t.Errorf("im Protokoll steht %q, erwartet %q",
				line.AskedBy, AgentAskedBy("ssh.exe"))
		}
	}
	// Die Sorge hinter dieser Pruefung ist: Der Agent darf keinen privaten
	// Schluessel behalten. Frueher wurde das an der Zahl 2 gemessen - einmal
	// fuers Auflisten, einmal fuers Unterschreiben. Das Auflisten braucht
	// seit dem oeffentlichen Teil in der Liste keinen Zugriff mehr, also
	// waere 2 heute falsch und 1 richtig.
	//
	// Nur die Zahl zu senken wuerde die Sorge aber nicht mehr messen. Darum
	// wird jetzt die Eigenschaft selbst geprueft: JEDE Unterschrift kostet
	// genau einen Zugriff. Behielte der Agent den Schluessel, waere die
	// zweite umsonst.
	if reveals != 1 {
		t.Errorf("%d Zugriffe fuer eine Unterschrift, erwartet genau 1", reveals)
	}
}

// Nach einem Schluessel, der nicht im Tresor liegt, darf keine Unterschrift
// kommen - auch nicht, wenn er gueltig aussieht.
func TestAgentRefusesForeignKey(t *testing.T) {
	app := unlockedApp(t)
	// Der Tresor MUSS einen Schluessel enthalten. Ohne ihn faellt der Fall
	// schon an der leeren Liste und prueft den Vergleich nie - gemessen am
	// 24.10.2026: der Mutant "fremder Schluessel gilt als eigener"
	// ueberlebte, weil die Schleife nie lief.
	putTestKey(t, app, "Eigener Schluessel", goldenEd25519Private)
	conn, _ := agentPair(t, app, "")

	foreign, err := ParseSSHKey([]byte(goldenEcdsaP256Private))
	if err != nil {
		t.Fatalf("Vergleichsschluessel: %v", err)
	}
	defer foreign.Wipe()

	request := &wireWriter{}
	request.Byte(agentSignRequest)
	request.String(foreign.Blob)
	request.String([]byte("bitte unterschreiben"))
	request.Uint32(0)
	answer := ask(t, conn, request.Bytes())
	if len(answer) != 1 || answer[0] != agentFailure {
		t.Fatalf("auf einen fremden Schluessel kam %v, erwartet ein Nein", answer)
	}
}

// Der Tresor ist das Geraet. Wer Schluessel in den Agenten legen oder aus ihm
// loeschen will, muss ein Nein bekommen - ein stiller Erfolg waere die
// gefaehrlichere Antwort, weil das fremde Programm dann glaubt, sein
// Schluessel liege hier.
func TestAgentRefusesWrites(t *testing.T) {
	app := unlockedApp(t)
	conn, _ := agentPair(t, app, "")

	cases := map[string]byte{
		"Schluessel hinzufuegen":   agentAddIdentity,
		"Schluessel mit Bedingung": agentAddIDConstrained,
		"Schluessel loeschen":      agentRemoveIdentity,
		"alle Schluessel loeschen": agentRemoveAll,
		"Agenten sperren":          agentLock,
		"Agenten entsperren":       agentUnlock,
		"unbekannte Nachricht":     200,
	}
	for name, kind := range cases {
		answer := ask(t, conn, []byte{kind})
		if len(answer) != 1 || answer[0] != agentFailure {
			t.Errorf("%s: Antwort %v, erwartet ein Nein", name, answer)
		}
	}
	// Erweiterungen bekommen ihre eigene Absage - so sagt das Protokoll es.
	answer := ask(t, conn, append([]byte{agentExtension}, 0, 0, 0, 0))
	if len(answer) != 1 || answer[0] != agentExtensionFailure {
		t.Errorf("Erweiterung: Antwort %v, erwartet die eigene Absage", answer)
	}
}

// Eine Nachricht mit einem Rest dahinter ist eine Nachricht, die etwas
// anderes meint als wir lesen. Sie muss abgewiesen werden.
func TestAgentRefusesTrailingBytes(t *testing.T) {
	app := unlockedApp(t)
	putTestKey(t, app, "Eigener Schluessel", goldenEd25519Private)
	conn, _ := agentPair(t, app, "")

	answer := ask(t, conn, []byte{agentRequestIdentities, 0x00})
	if len(answer) != 1 || answer[0] != agentFailure {
		t.Errorf("Frage mit Anhang: Antwort %v, erwartet ein Nein", answer)
	}

	// Der Schluessel ist BEKANNT: so kann den Wunsch nur noch der Rest
	// dahinter zu Fall bringen. Mit einem unbekannten Schluessel maess der
	// Fall die Unbekanntheit - gemessen am 24.10.2026, der Mutant
	// "Rest wird uebergangen" ueberlebte genau daran.
	known, err := ParseSSHKey([]byte(goldenEd25519Private))
	if err != nil {
		t.Fatalf("Vergleichsschluessel: %v", err)
	}
	defer known.Wipe()

	// Zwischenpruefung: derselbe Wunsch OHNE Rest muss gelingen. Sonst
	// liegt die Wand wieder vorher, und der Fall beweist nichts.
	good := &wireWriter{}
	good.Byte(agentSignRequest)
	good.String(known.Blob)
	good.String([]byte("daten"))
	good.Uint32(0)
	answer = ask(t, conn, good.Bytes())
	if len(answer) == 0 || answer[0] != agentSignResponse {
		t.Fatalf("der Weg bis zur gemeinten Stelle ist nicht offen: %v", answer)
	}

	request := &wireWriter{}
	request.Byte(agentSignRequest)
	request.String(known.Blob)
	request.String([]byte("daten"))
	request.Uint32(0)
	request.Byte(0x00) // der Rest
	answer = ask(t, conn, request.Bytes())
	if len(answer) != 1 || answer[0] != agentFailure {
		t.Errorf("Wunsch mit Anhang: Antwort %v, erwartet ein Nein", answer)
	}
}

// putTestKey legt einen Schluessel in den Tresor der Attrappe.
func putTestKey(t *testing.T, app *App, name, private string) uint32 {
	t.Helper()
	client, err := app.Client()
	if err != nil {
		t.Fatalf("verbinden: %v", app.Message(err))
	}
	id, err := NewAccess(client, "osk put").Add(
		Item{Type: ItemSSHKey, Name: name}, []byte(private))
	if err != nil {
		t.Fatalf("Schluessel ablegen: %v", app.Message(err))
	}
	return id
}

// Ist der Tresor zu, darf der Agent keine Identitaeten nennen. Sonst
// verraet er die Namen der Schluessel eines gesperrten Geraets.
func TestAgentSaysNothingWhileLocked(t *testing.T) {
	cfg := DefaultConfig()
	cfg.SimAddress = startSimulator(t)
	cfg.Lang = "de"
	app := NewApp(os.DirFS(".."), cfg)
	t.Cleanup(app.Disconnect)

	conn, _ := agentPair(t, app, "")
	answer := ask(t, conn, []byte{agentRequestIdentities})
	if len(answer) != 1 || answer[0] != agentFailure {
		t.Fatalf("bei gesperrtem Tresor kam %v, erwartet ein Nein", answer)
	}
}

// Forget muss wirken: nach dem Sperren darf die alte Liste nicht weiter
// genannt werden.
func TestAgentForgetsIdentities(t *testing.T) {
	app := unlockedApp(t)
	client, err := app.Client()
	if err != nil {
		t.Fatalf("verbinden: %v", app.Message(err))
	}
	access := NewAccess(client, "osk put")
	if _, err := access.Add(Item{Type: ItemSSHKey, Name: "Pruefschluessel"},
		[]byte(goldenEd25519Private)); err != nil {
		t.Fatalf("Schluessel ablegen: %v", app.Message(err))
	}
	agent := NewAgent(app)
	first, err := agent.Identities("test")
	if err != nil {
		t.Fatalf("Identitaeten: %v", app.Message(err))
	}
	if len(first) != 1 {
		t.Fatalf("%d Identitaeten, erwartet 1", len(first))
	}
	if err := client.Lock(); err != nil {
		t.Fatalf("sperren: %v", app.Message(err))
	}
	agent.Forget()
	if _, err := agent.Identities("test"); err == nil {
		t.Error("nach dem Sperren wurden Identitaeten genannt")
	}
}

// Eine Laengenangabe ohne Rumpf dahinter darf nicht Gigabyte anfordern.
func TestAgentMessageLimits(t *testing.T) {
	cases := map[string][]byte{
		"Laenge null":   {0, 0, 0, 0},
		"zu gross":      {0xff, 0xff, 0xff, 0xff},
		"abgeschnitten": {0, 0, 0, 8, 1, 2},
	}
	for name, data := range cases {
		if _, err := readAgentMessage(bytes.NewReader(data)); err == nil {
			t.Errorf("%s: wurde angenommen", name)
		}
	}
}
