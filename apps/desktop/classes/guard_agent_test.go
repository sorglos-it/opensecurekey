// Pruefungen des lokalen Dienstes, an dem osk-guard haengt. Hier wird auch
// gemessen, dass die Sprachdateien der Bibliothek mit denen des Programms
// uebereinstimmen - zwei Kopien, die auseinanderlaufen, waeren schlimmer
// als eine.
package classes

import (
	"bufio"
	"crypto/rand"
	"encoding/hex"
	"encoding/json"
	"errors"
	"net"
	"os"
	"path/filepath"
	"strings"
	"testing"
	"time"
)

// agentTestLine schickt eine Zeile und liest die Antwort.
func agentTestLine(t *testing.T, address, line string) string {
	t.Helper()
	conn, err := net.DialTimeout("tcp", address, 3*time.Second)
	if err != nil {
		t.Fatalf("verbinden: %v", err)
	}
	defer conn.Close()
	if err := conn.SetDeadline(time.Now().Add(10 * time.Second)); err != nil {
		t.Fatalf("Frist: %v", err)
	}
	if _, err := conn.Write([]byte(line + "\n")); err != nil {
		t.Fatalf("senden: %v", err)
	}
	answer, err := bufio.NewReaderSize(conn, 1<<20).ReadString('\n')
	if err != nil {
		t.Fatalf("lesen: %v", err)
	}
	return strings.TrimRight(answer, "\r\n")
}

// agentTestValue liest einen Wert aus einer OK-Zeile.
func agentTestValue(t *testing.T, answer, key string) string {
	t.Helper()
	if !strings.HasPrefix(answer, "OK ") {
		t.Fatalf("keine OK-Zeile: %q", answer)
	}
	for _, field := range strings.Fields(answer)[1:] {
		name, value, found := strings.Cut(field, "=")
		if found && name == key {
			return value
		}
	}
	t.Fatalf("Feld %s fehlt in %q", key, answer)
	return ""
}

func agentTestSetup(t *testing.T, opts GuardFakeOptions) (*GuardFakeDevice, *GuardAgent) {
	t.Helper()
	device, guard, _ := guardTestDevice(t, opts)
	agent, err := NewGuardAgent(guard, 0)
	if err != nil {
		t.Fatalf("Dienst: %v", err)
	}
	t.Cleanup(func() { agent.Close() })
	return device, agent
}

// Der Dienst lauscht allein auf der Rueckschleife. Eine Adresse im Netz
// waere ein offener Kanal zum Stick.
func TestAgentListensOnLoopbackOnly(t *testing.T) {
	_, agent := agentTestSetup(t, GuardFakeOptions{})
	host, _, err := net.SplitHostPort(agent.Address())
	if err != nil {
		t.Fatalf("Adresse: %v", err)
	}
	ip := net.ParseIP(host)
	if ip == nil || !ip.IsLoopback() {
		t.Fatalf("der Dienst lauscht auf %s", host)
	}
}

func TestAgentPing(t *testing.T) {
	_, agent := agentTestSetup(t, GuardFakeOptions{})
	if got := agentTestLine(t, agent.Address(), "ping"); got != "OK v=1" {
		t.Fatalf("ping antwortet %q", got)
	}
}

// Das Wichtigste am Dienst: er reicht die Frage des Aufrufers DURCH. Die
// Bibliothek prueft die Unterschrift selbst und glaubt dem Dienst kein Ja -
// sonst waere der Schutz die Aussage eines Programms ueber sich selbst.
func TestAgentPassesTheCallersChallengeThrough(t *testing.T) {
	device, agent := agentTestSetup(t, GuardFakeOptions{})
	nonce := make([]byte, GuardNonceLen)
	if _, err := rand.Read(nonce); err != nil {
		t.Fatalf("Zufall: %v", err)
	}
	binHash := make([]byte, GuardHashLen)
	for i := range binHash {
		binHash[i] = byte(i)
	}
	answer := agentTestLine(t, agent.Address(), "attest "+
		hex.EncodeToString(nonce)+" "+hex.EncodeToString(binHash))
	signature, err := hex.DecodeString(agentTestValue(t, answer, "sig"))
	if err != nil {
		t.Fatalf("Unterschrift: %v", err)
	}
	public, err := hex.DecodeString(agentTestValue(t, answer, "pub"))
	if err != nil {
		t.Fatalf("Schluessel: %v", err)
	}
	// Genau so prueft die Bibliothek: gegen den eingetragenen Schluessel
	// und gegen die eigene Frage.
	if err := GuardVerify(device.PublicKey(), GuardAnswer{
		Nonce: nonce, BinHash: binHash,
		Signature: signature, PublicKey: public}); err != nil {
		t.Fatalf("die Antwort des Dienstes haelt der Pruefung nicht: %v", err)
	}
	// Und sie passt zu KEINER anderen Frage.
	other := make([]byte, GuardNonceLen)
	if _, err := rand.Read(other); err != nil {
		t.Fatalf("Zufall: %v", err)
	}
	if err := GuardVerify(device.PublicKey(), GuardAnswer{
		Nonce: other, BinHash: binHash,
		Signature: signature, PublicKey: public}); err == nil {
		t.Fatal("die Antwort passte auch zu einer anderen Frage")
	}
}

func TestAgentSealAndUnseal(t *testing.T) {
	_, agent := agentTestSetup(t, GuardFakeOptions{})
	label := hex.EncodeToString([]byte("datenmodell"))
	plain := []byte("die Tabelle, ohne die nichts geht")
	answer := agentTestLine(t, agent.Address(), "seal "+label+" - "+
		hex.EncodeToString(plain))
	blob := agentTestValue(t, answer, "blob")
	back := agentTestLine(t, agent.Address(), "unseal "+label+" - "+blob)
	got, err := hex.DecodeString(agentTestValue(t, back, "plain"))
	if err != nil {
		t.Fatalf("Klartext: %v", err)
	}
	if string(got) != string(plain) {
		t.Fatalf("heraus kam %q", got)
	}
	// Unter einer anderen Bezeichnung geht sie nicht auf.
	wrong := agentTestLine(t, agent.Address(), "unseal "+
		hex.EncodeToString([]byte("anders"))+" - "+blob)
	if !strings.HasPrefix(wrong, "ERR ") {
		t.Fatalf("unter falscher Bezeichnung aufgegangen: %q", wrong)
	}
}

func TestAgentLicense(t *testing.T) {
	issuerPublic, issuerPrivate := licenseTestIssuer(t, "hersteller-dienst")
	device, err := NewGuardFakeDevice(GuardFakeOptions{Seed: []byte("dienst-lizenz")})
	if err != nil {
		t.Fatalf("Attrappe: %v", err)
	}
	blob, err := LicenseSign(licenseTestSample(), issuerPrivate)
	if err != nil {
		t.Fatalf("unterschreiben: %v", err)
	}
	device.AddItem(Item{Type: ItemLicense, Name: "mein-programm"}, blob)
	cfg := DefaultGuardConfig()
	cfg.DevicePublicKey = device.PublicKey()
	cfg.IssuerPublicKey = issuerPublic
	cfg.BinaryPath = guardTestBinary(t, "programm", []byte("egal"))
	guard, err := NewGuard(device.GuardFakeClient("test"), cfg)
	if err != nil {
		t.Fatalf("Guard: %v", err)
	}
	agent, err := NewGuardAgent(guard, 0)
	if err != nil {
		t.Fatalf("Dienst: %v", err)
	}
	defer agent.Close()

	answer := agentTestLine(t, agent.Address(), "license "+
		hex.EncodeToString([]byte("mein-programm")))
	if got := agentTestValue(t, answer, "seats"); got != "5" {
		t.Fatalf("Plaetze: %q", got)
	}
	if got := agentTestValue(t, answer, "expires"); got != "1800000000" {
		t.Fatalf("Ablauf: %q", got)
	}
	tiers := agentTestValue(t, answer, "tiers")
	want := hex.EncodeToString([]byte("basis")) + "," +
		hex.EncodeToString([]byte("berichte"))
	if tiers != want {
		t.Fatalf("Ausbaustufen: %q", tiers)
	}
	// Ein Produkt, das nicht darauf liegt, wird genannt.
	missing := agentTestLine(t, agent.Address(), "license "+
		hex.EncodeToString([]byte("fremdes")))
	if missing != "ERR error.license.missing" {
		t.Fatalf("erwartet ERR error.license.missing, bekommen %q", missing)
	}
}

func TestAgentNodeSignAndAgree(t *testing.T) {
	device, agent := agentTestSetup(t, GuardFakeOptions{})
	label := hex.EncodeToString([]byte("auftraege"))
	answer := agentTestLine(t, agent.Address(), "nodepub "+label+" "+
		hex.EncodeToString([]byte("ed25519")))
	public, err := hex.DecodeString(agentTestValue(t, answer, "pub"))
	if err != nil {
		t.Fatalf("Schluessel: %v", err)
	}
	job := []byte(`{"tat":"abbild-holen"}`)
	signed := agentTestLine(t, agent.Address(), "nodesign "+label+" "+
		hex.EncodeToString(job))
	signature, err := hex.DecodeString(agentTestValue(t, signed, "sig"))
	if err != nil {
		t.Fatalf("Unterschrift: %v", err)
	}
	if err := NodeVerify(public, "auftraege", job, signature); err != nil {
		t.Fatalf("die Unterschrift des Dienstes gilt nicht: %v", err)
	}
	_ = device

	// Schluesselaustausch mit einer Gegenseite.
	peer, peerGuard := nodeTestGuard(t, "gegenseite", GuardFakeOptions{})
	_ = peer
	peerPublic, err := peerGuard.NodePublic("auftraege", "x25519")
	if err != nil {
		t.Fatalf("Gegenseite: %v", err)
	}
	agreed := agentTestLine(t, agent.Address(), "nodeagree "+label+" "+
		hex.EncodeToString(peerPublic.PublicKey))
	secret, err := hex.DecodeString(agentTestValue(t, agreed, "sec"))
	if err != nil {
		t.Fatalf("Geheimnis: %v", err)
	}
	if len(secret) != NodeSecretLen {
		t.Fatalf("Laenge %d", len(secret))
	}
}

// Der Fall ohne ECDH kommt als eigener Schluessel heraus, nicht als
// allgemeiner Fehler - sonst koennte eine Bibliothek nicht sagen, welcher
// Anwendungsfall wegfaellt.
func TestAgentReportsMissingEcdhByItsOwnName(t *testing.T) {
	_, agent := agentTestSetup(t, GuardFakeOptions{NoECDH: true})
	_, peerGuard := nodeTestGuard(t, "gegenseite-fips", GuardFakeOptions{})
	peerPublic, err := peerGuard.NodePublic("auftraege", "x25519")
	if err != nil {
		t.Fatalf("Gegenseite: %v", err)
	}
	answer := agentTestLine(t, agent.Address(), "nodeagree "+
		hex.EncodeToString([]byte("auftraege"))+" "+
		hex.EncodeToString(peerPublic.PublicKey))
	if answer != "ERR error.guard.noecdh" {
		t.Fatalf("erwartet ERR error.guard.noecdh, bekommen %q", answer)
	}
}

// Kaputte Zeilen werden abgewiesen, nicht geraten.
func TestAgentRejectsBrokenLines(t *testing.T) {
	_, agent := agentTestSetup(t, GuardFakeOptions{})
	cases := map[string]string{
		"unbekannter Befehl":    "gibtsnicht",
		"attest ohne Werte":     "attest",
		"attest mit zu wenig":   "attest " + strings.Repeat("00", GuardNonceLen),
		"attest kein Hex":       "attest zz " + strings.Repeat("00", GuardHashLen),
		"attest falsche Laenge": "attest 00 " + strings.Repeat("00", GuardHashLen),
		"seal ohne Inhalt":      "seal " + hex.EncodeToString([]byte("x")) + " - -",
		"nodepub ohne Kurve":    "nodepub " + hex.EncodeToString([]byte("x")),
		"Steuerzeichen im Namen": "nodesign " + hex.EncodeToString([]byte("a\nb")) +
			" " + hex.EncodeToString([]byte("job")),
	}
	for name, line := range cases {
		t.Run(name, func(t *testing.T) {
			answer := agentTestLine(t, agent.Address(), line)
			if !strings.HasPrefix(answer, "ERR ") {
				t.Fatalf("angenommen: %q -> %q", line, answer)
			}
		})
	}
}

// Mehrere Anfragen auf einer Verbindung, der Reihe nach.
func TestAgentServesSeveralRequestsOnOneConnection(t *testing.T) {
	_, agent := agentTestSetup(t, GuardFakeOptions{})
	conn, err := net.DialTimeout("tcp", agent.Address(), 3*time.Second)
	if err != nil {
		t.Fatalf("verbinden: %v", err)
	}
	defer conn.Close()
	if err := conn.SetDeadline(time.Now().Add(10 * time.Second)); err != nil {
		t.Fatalf("Frist: %v", err)
	}
	reader := bufio.NewReaderSize(conn, 1<<20)
	for runde := 0; runde < 5; runde++ {
		if _, err := conn.Write([]byte("ping\n")); err != nil {
			t.Fatalf("Runde %d senden: %v", runde, err)
		}
		line, err := reader.ReadString('\n')
		if err != nil {
			t.Fatalf("Runde %d lesen: %v", runde, err)
		}
		if strings.TrimRight(line, "\r\n") != "OK v=1" {
			t.Fatalf("Runde %d: %q", runde, line)
		}
	}
}

// Beide Kopien der Sprachdateien muessen gleich sein: die des Programms und
// die, die osk-guard fuer C, PHP, Python und .NET mitbringt. Weicht eine
// ab, sieht ein Anwender je Sprache einen anderen Satz zur selben Ursache.
func TestGuardLanguageFilesMatchTheProgram(t *testing.T) {
	folder := filepath.Join(appFS(), "..", "..", "shared", "osk-guard", "lang")
	entries, err := os.ReadDir(folder)
	if err != nil {
		// Fehlt der Ordner, ist die Bibliothek nicht da - das ist ein
		// Fehler und kein Grund, die Pruefung zu ueberspringen.
		t.Fatalf("%s: %v", folder, err)
	}
	var checked int
	for _, entry := range entries {
		if entry.IsDir() || !strings.HasSuffix(entry.Name(), ".json") {
			continue
		}
		code := strings.TrimSuffix(entry.Name(), ".json")
		data, readErr := os.ReadFile(filepath.Join(folder, entry.Name()))
		if readErr != nil {
			t.Fatalf("%s: %v", entry.Name(), readErr)
		}
		var guardTexts map[string]string
		if err := json.Unmarshal(data, &guardTexts); err != nil {
			t.Fatalf("%s: %v", entry.Name(), err)
		}
		if len(guardTexts) == 0 {
			t.Fatalf("%s ist leer", entry.Name())
		}
		app := loadFile(t, code)
		for key, text := range guardTexts {
			if app[key] != text {
				t.Errorf("%s: %s lautet in der Bibliothek %q, im Programm %q",
					code, key, text, app[key])
			}
			checked++
		}
	}
	if checked < 5*30 {
		t.Fatalf("nur %d Texte verglichen - zu wenige Sprachen oder Schluessel", checked)
	}
}

// Jeder Fehlerschluessel, den der Dienst hinausgibt, muss einen Satz haben.
// Sonst steht in einer Bibliothek eine Kennung, zu der es keinen Text gibt.
func TestAgentErrorKeysHaveTexts(t *testing.T) {
	english := loadFile(t, "en")
	for _, key := range []string{
		"error.guard.request", "error.guard.verb", "error.guard.size",
		"error.guard.internal", "error.guard.noecdh", "error.guard.label",
		"error.guard.sealbroken", "error.license.missing",
	} {
		if english[key] == "" {
			t.Errorf("%s hat keinen Satz", key)
		}
	}
	// Und die Gegenprobe, dass guardAgentErr wirklich den Schluessel nimmt.
	if got := guardAgentErr(Fail("error.guard.noecdh")); got != "ERR error.guard.noecdh" {
		t.Fatalf("guardAgentErr liefert %q", got)
	}
	if got := guardAgentErr(errors.New("etwas ganz anderes")); got != "ERR error.guard.internal" {
		t.Fatalf("ein fremder Fehler wird zu %q", got)
	}
}
