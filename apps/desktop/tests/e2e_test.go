// Prueflauf gegen die Attrappe: Oberflaeche und Kommandozeile laufen gegen
// die Attrappe aus tests/simulator, nicht gegen echte Hardware.
package tests

import (
	"bytes"
	"encoding/json"
	"io"
	"net"
	"net/http"
	"os"
	"os/exec"
	"path/filepath"
	"runtime"
	"strconv"
	"strings"
	"sync"
	"testing"
	"time"

	"osk/classes"
)

const simPin = "123456"

// simulatorBinary baut die Attrappe genau einmal. Sie wird als eigenes
// Programm gestartet, nicht ueber "go run": sonst bliebe beim Beenden das
// Kindprogramm stehen und der Prueflauf wartet ewig.
var (
	buildOnce sync.Once
	buildPath string
	buildErr  error
)

func simulatorBinary(t *testing.T) string {
	t.Helper()
	buildOnce.Do(func() {
		name := "simulator"
		if runtime.GOOS == "windows" {
			name += ".exe"
		}
		buildPath = filepath.Join(os.TempDir(), "osk-"+name)
		cmd := exec.Command("go", "build", "-o", buildPath, "./simulator")
		cmd.Env = append(os.Environ(), "CGO_ENABLED=0")
		out, err := cmd.CombinedOutput()
		if err != nil {
			buildErr = err
			t.Log(string(out))
		}
	})
	if buildErr != nil {
		t.Skipf("Attrappe laesst sich nicht bauen: %v", buildErr)
	}
	return buildPath
}

// startSimulator startet die Attrappe und gibt ihre Adresse zurueck.
func startSimulator(t *testing.T) string {
	t.Helper()
	port := freePort(t)
	cmd := exec.Command(simulatorBinary(t), "-port", strconv.Itoa(port))
	var output bytes.Buffer
	cmd.Stdout = &output
	cmd.Stderr = &output
	if err := cmd.Start(); err != nil {
		t.Skipf("Attrappe laesst sich nicht starten: %v", err)
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
		time.Sleep(100 * time.Millisecond)
	}
	t.Fatalf("Attrappe antwortet nicht auf %s: %s", address, output.String())
	return ""
}

func freePort(t *testing.T) int {
	t.Helper()
	listener, err := net.Listen("tcp", "127.0.0.1:0")
	if err != nil {
		t.Fatalf("kein freier Port: %v", err)
	}
	defer listener.Close()
	return listener.Addr().(*net.TCPAddr).Port
}

func newApp(t *testing.T, address string) *classes.App {
	t.Helper()
	cfg := classes.DefaultConfig()
	cfg.SimAddress = address
	cfg.Lang = "de"
	cfg.Port = freePort(t)
	app := classes.NewApp(os.DirFS(".."), cfg)
	t.Cleanup(app.Disconnect)
	return app
}

// Das Protokoll in ganzer Breite: hello, unlock, list, reveal, add, update,
// delete, log, pair, backup, restore, Firmware, attest, lock.
func TestProtocolAgainstSimulator(t *testing.T) {
	app := newApp(t, startSimulator(t))
	client, err := app.Client()
	if err != nil {
		t.Fatalf("verbinden: %v", app.Message(err))
	}
	info, err := client.Hello()
	if err != nil {
		t.Fatalf("hello: %v", app.Message(err))
	}
	if info.Protocol != classes.ProtocolVersion || info.Serial == "" {
		t.Fatalf("hello antwortet unerwartet: %+v", info)
	}
	// Vor dem Entsperren gibt die Attrappe nichts heraus.
	if _, err := client.List(0, ""); err == nil {
		t.Error("Liste ohne Entsperren erlaubt")
	}
	if err := client.Unlock([]byte("000000")); err == nil {
		t.Error("falsche PIN angenommen")
	}
	if err := client.Unlock([]byte(simPin)); err != nil {
		t.Fatalf("unlock: %v", app.Message(err))
	}
	items, err := client.List(0, "")
	if err != nil || len(items) == 0 {
		t.Fatalf("list: %v (%d Eintraege)", err, len(items))
	}
	secret, err := client.Reveal(items[0].ID)
	if err != nil || len(secret) == 0 {
		t.Fatalf("reveal: %v", err)
	}
	classes.Wipe(secret)

	id, err := client.Add(classes.Item{Type: classes.ItemNote, Name: "Prueftext",
		Site: "test.example"}, []byte("inhalt"))
	if err != nil || id == 0 {
		t.Fatalf("add: %v", err)
	}
	if err := client.Update(id, classes.Item{Type: classes.ItemNote,
		Name: "Prueftext 2"}, nil); err != nil {
		t.Fatalf("update: %v", err)
	}
	if err := client.Delete(id); err != nil {
		t.Fatalf("delete: %v", err)
	}
	if err := client.Delete(id); err == nil {
		t.Error("zweites Loeschen wurde angenommen")
	}
	entries, err := client.Log(50)
	if err != nil || len(entries) == 0 {
		t.Fatalf("log: %v (%d Zeilen)", err, len(entries))
	}
	// Das Geraet schreibt mit, welches Programm gefragt hat.
	var sawName bool
	for _, entry := range entries {
		if entry.Event == "reveal" && entry.AskedBy == classes.DefaultAppName {
			sawName = true
		}
	}
	if !sawName {
		t.Error("das Protokoll nennt den Namen des Programms nicht")
	}
	if _, err := client.Pair("host"); err != nil {
		t.Fatalf("pair: %v", err)
	}
	if _, err := client.Pair("unsinn"); err == nil {
		t.Error("unbekannte Kopplungsart angenommen")
	}
	blob, err := client.Backup()
	if err != nil || len(blob) == 0 {
		t.Fatalf("backup: %v", err)
	}
	if err := client.Restore(blob); err != nil {
		t.Fatalf("restore: %v", err)
	}
	if err := client.Firmware(bytes.Repeat([]byte{0x5a}, 5000), nil); err != nil {
		t.Fatalf("firmware: %v", err)
	}
	signature, err := client.Attest([]byte("aufgabe"))
	if err != nil || len(signature) == 0 {
		t.Fatalf("attest: %v", err)
	}
	if err := client.Lock(); err != nil {
		t.Fatalf("lock: %v", err)
	}
}

// Die Kommandozeile arbeitet mit demselben Kern.
func TestCLIAgainstSimulator(t *testing.T) {
	app := newApp(t, startSimulator(t))

	var out, errOut bytes.Buffer
	if code := classes.RunCLI(app, []string{"unlock"},
		strings.NewReader(simPin+"\n"), &out, &errOut); code != 0 {
		t.Fatalf("unlock: %s", errOut.String())
	}
	out.Reset()
	if code := classes.RunCLI(app, []string{"list"}, nil, &out, &errOut); code != 0 {
		t.Fatalf("list: %s", errOut.String())
	}
	if !strings.Contains(out.String(), "Beispiel-Zugang") {
		t.Fatalf("list zeigt den Eintrag nicht: %q", out.String())
	}
	out.Reset()
	if code := classes.RunCLI(app, []string{"get", "Beispiel-Zugang"},
		nil, &out, &errOut); code != 0 {
		t.Fatalf("get: %s", errOut.String())
	}
	if strings.TrimSpace(out.String()) != "geheim-1" {
		t.Fatalf("get gibt %q", out.String())
	}
	out.Reset()
	errOut.Reset()
	if code := classes.RunCLI(app, []string{"get", "gibtesnicht"},
		nil, &out, &errOut); code == 0 {
		t.Error("unbekannter Name wurde nicht gemeldet")
	}
	if code := classes.RunCLI(app, []string{"unsinn"}, nil, &out, &errOut); code != 2 {
		t.Error("unbekannter Befehl gibt den falschen Ausstiegscode")
	}
}

// Die Oberflaeche: Seiten, Schnittstelle, Herkunftspruefung.
func TestServerAgainstSimulator(t *testing.T) {
	app := newApp(t, startSimulator(t))
	server, err := classes.NewServer(app)
	if err != nil {
		t.Fatalf("Server: %v", app.Message(err))
	}
	defer server.Close()
	go server.Serve()

	base := server.Address()
	origin := strings.TrimSuffix(base, "/")
	client := &http.Client{Timeout: 30 * time.Second}

	// Nur auf 127.0.0.1 - von aussen ist der Port nicht gebunden.
	if _, err := net.DialTimeout("tcp",
		hostOf(t)+":"+strconv.Itoa(server.Port()), 2*time.Second); err == nil {
		t.Error("der Server ist auch von aussen erreichbar")
	}

	for _, path := range []string{"", "item", "certs", "devices", "backup",
		"firmware", "settings"} {
		answer, getErr := client.Get(base + path)
		if getErr != nil {
			t.Fatalf("%s: %v", path, getErr)
		}
		body, _ := io.ReadAll(answer.Body)
		answer.Body.Close()
		if answer.StatusCode != http.StatusOK {
			t.Fatalf("%s antwortet %d", path, answer.StatusCode)
		}
		if !bytes.Contains(body, []byte("<html lang=\"de\"")) {
			t.Errorf("%s ohne Sprachangabe", path)
		}
	}

	// Fremde Herkunft wird abgewiesen - das ersetzt das Token.
	request, _ := http.NewRequest(http.MethodPost, base+"api/unlock",
		strings.NewReader(`{"pin":"`+simPin+`"}`))
	request.Header.Set("Origin", "http://evil.example")
	answer, err := client.Do(request)
	if err != nil {
		t.Fatalf("Anfrage: %v", err)
	}
	answer.Body.Close()
	if answer.StatusCode != http.StatusForbidden {
		t.Fatalf("fremde Herkunft antwortet %d", answer.StatusCode)
	}

	// Aendernde Anfrage ohne Herkunft ebenfalls.
	answer, err = client.Post(base+"api/lock", "application/json",
		strings.NewReader("{}"))
	if err != nil {
		t.Fatalf("Anfrage: %v", err)
	}
	answer.Body.Close()
	if answer.StatusCode != http.StatusForbidden {
		t.Fatalf("Anfrage ohne Herkunft antwortet %d", answer.StatusCode)
	}

	// Falscher Zielname (zeigt auch auf 127.0.0.1) wird abgewiesen.
	request, _ = http.NewRequest(http.MethodGet, base+"api/state", nil)
	request.Host = "fremd.example"
	answer, err = client.Do(request)
	if err != nil {
		t.Fatalf("Anfrage: %v", err)
	}
	answer.Body.Close()
	if answer.StatusCode != http.StatusForbidden {
		t.Fatalf("fremder Zielname antwortet %d", answer.StatusCode)
	}

	texts := apiCall(t, client, origin, http.MethodGet, base+"api/texts", "")
	if texts["data"].(map[string]any)["app.name"] == nil {
		t.Error("die Texte fehlen")
	}
	apiCall(t, client, origin, http.MethodPost, base+"api/unlock",
		`{"pin":"`+simPin+`"}`)
	items := apiCall(t, client, origin, http.MethodGet, base+"api/items", "")
	list, ok := items["data"].([]any)
	if !ok || len(list) == 0 {
		t.Fatalf("keine Eintraege: %v", items)
	}
	first := list[0].(map[string]any)
	id := int(first["id"].(float64))
	reveal := apiCall(t, client, origin, http.MethodPost, base+"api/reveal",
		`{"id":`+strconv.Itoa(id)+`}`)
	if reveal["data"].(map[string]any)["secret"] == "" {
		t.Error("das Geheimnis fehlt")
	}
	saved := apiCall(t, client, origin, http.MethodPost, base+"api/save",
		`{"id":0,"type":14,"name":"Aus der Oberflaeche","secret":"x"}`)
	newID := int(saved["data"].(map[string]any)["id"].(float64))
	if newID == 0 {
		t.Fatal("neuer Eintrag ohne Kennung")
	}
	apiCall(t, client, origin, http.MethodPost, base+"api/delete",
		`{"id":`+strconv.Itoa(newID)+`}`)

	// Ein Eintrag ohne Namen wird abgelehnt, und zwar mit Text.
	bad := apiCall(t, client, origin, http.MethodPost, base+"api/save",
		`{"id":0,"type":14,"name":"","secret":"x"}`)
	if bad["ok"].(bool) || bad["error"].(string) == "" {
		t.Errorf("Eintrag ohne Namen angenommen: %v", bad)
	}
}

// apiCall schickt eine Anfrage mit richtiger Herkunft und liest die Antwort.
func apiCall(t *testing.T, client *http.Client, origin, method, url,
	body string) map[string]any {
	t.Helper()
	var reader io.Reader
	if body != "" {
		reader = strings.NewReader(body)
	}
	request, err := http.NewRequest(method, url, reader)
	if err != nil {
		t.Fatalf("%s: %v", url, err)
	}
	request.Header.Set("Origin", origin)
	if body != "" {
		request.Header.Set("Content-Type", "application/json")
	}
	answer, err := client.Do(request)
	if err != nil {
		t.Fatalf("%s: %v", url, err)
	}
	defer answer.Body.Close()
	var out map[string]any
	if err := json.NewDecoder(answer.Body).Decode(&out); err != nil {
		t.Fatalf("%s: Antwort unlesbar: %v", url, err)
	}
	return out
}

// hostOf nennt eine Adresse dieses Rechners, die nicht die Rueckschleife ist.
func hostOf(t *testing.T) string {
	t.Helper()
	addresses, err := net.InterfaceAddrs()
	if err != nil {
		t.Skip("keine Netzadresse zu finden")
	}
	for _, address := range addresses {
		if ipNet, ok := address.(*net.IPNet); ok && !ipNet.IP.IsLoopback() &&
			ipNet.IP.To4() != nil {
			return ipNet.IP.String()
		}
	}
	t.Skip("keine Adresse ausserhalb der Rueckschleife")
	return ""
}
