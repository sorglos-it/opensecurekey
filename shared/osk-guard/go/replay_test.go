// Die eine Pruefung, um die es bei Stufe 1 geht: eine AUFGEZEICHNETE
// Antwort darf nicht wiederverwendbar sein.
//
// "Jede Frage ist neu" ist nicht dasselbe. Darum wird hier wirklich
// mitgeschnitten und wirklich noch einmal eingereicht - einmal ueber die
// Leitung (gegen einen Dienst, der die erste Antwort aufbewahrt und sie
// danach immer wieder gibt) und einmal gegen die eingecheckten Bytes aus
// tests/replay-vectors.json, die alle fuenf Sprachen pruefen.
//
// Der erste Fall muss ANGENOMMEN werden. Ohne ihn beweist kein abgewiesener
// Fall etwas: eine Bibliothek, die alles abweist, waere auch gruen.
package oskguard

import (
	"crypto/ed25519"
	"encoding/hex"
	"encoding/json"
	"io"
	"os"
	"path/filepath"
	"strings"
	"testing"
)

// recordingAgent unterschreibt die erste Frage ehrlich und gibt danach
// immer dieselbe Antwort - der Angreifer, der einmal mitgehoert hat.
func recordingAgent(private ed25519.PrivateKey, pub []byte) (func(string) string, *int) {
	var kept string
	rounds := 0
	return func(line string) string {
		fields := strings.Fields(line)
		if len(fields) == 0 {
			return "ERR error.guard.request"
		}
		if fields[0] == "ping" {
			return "OK v=1"
		}
		if fields[0] != "attest" || len(fields) != 3 {
			return "ERR error.guard.verb"
		}
		rounds++
		if kept != "" {
			return kept
		}
		nonce, err := hex.DecodeString(fields[1])
		if err != nil {
			return "ERR error.guard.request"
		}
		binHash, err := hex.DecodeString(fields[2])
		if err != nil {
			return "ERR error.guard.request"
		}
		signature := ed25519.Sign(private, message(attestDomain, nonce, binHash))
		kept = "OK sig=" + hex.EncodeToString(signature) +
			" pub=" + hex.EncodeToString(pub)
		return kept
	}, &rounds
}

func TestRecordedAnswerIsRefusedOnTheWire(t *testing.T) {
	public, private := testKeyPair(t, "echtes-geraet")
	answer, rounds := recordingAgent(private, public)
	agent := startFakeAgent(t, answer)
	guard, err := Open(agent.address(), public, nil)
	if err != nil {
		t.Fatalf("Open: %v", err)
	}
	binary := testBinary(t)

	// Erste Runde: ehrlich unterschrieben, muss angenommen werden.
	if err := guard.Present(binary); err != nil {
		t.Fatalf("die erste, echte Runde wurde abgewiesen: %v", err)
	}
	// Zweite Runde: dieselben Bytes, neue Frage. MUSS abgewiesen werden.
	err = guard.Present(binary)
	if err == nil {
		t.Fatal("eine aufgezeichnete Antwort wurde angenommen - " +
			"Stufe 1 waere damit eine Aufnahme wert")
	}
	if Code(err) != "error.guard.signature" {
		t.Fatalf("erwartet error.guard.signature, bekommen %s", Code(err))
	}
	// Und sie wurde wirklich zweimal gefragt, nicht aus dem Speicher
	// beantwortet.
	if *rounds != 2 {
		t.Fatalf("erwartet 2 Runden am Dienst, gezaehlt %d", *rounds)
	}
}

// Dieselbe Aufzeichnung, dreimal hintereinander: auch die dritte Runde
// darf nicht durchkommen, und der Grund bleibt derselbe.
func TestRecordedAnswerStaysRefused(t *testing.T) {
	public, private := testKeyPair(t, "echtes-geraet")
	answer, _ := recordingAgent(private, public)
	agent := startFakeAgent(t, answer)
	guard, err := Open(agent.address(), public, nil)
	if err != nil {
		t.Fatalf("Open: %v", err)
	}
	binary := testBinary(t)
	if err := guard.Present(binary); err != nil {
		t.Fatalf("erste Runde: %v", err)
	}
	for round := 2; round <= 4; round++ {
		err := guard.Present(binary)
		if Code(err) != "error.guard.signature" {
			t.Fatalf("Runde %d: erwartet error.guard.signature, bekommen %q",
				round, Code(err))
		}
	}
}

type replayCase struct {
	Name    string `json:"name"`
	Nonce   string `json:"nonce"`
	BinHash string `json:"binhash"`
	Pub     string `json:"pub"`
	Sig     string `json:"sig"`
	Valid   bool   `json:"valid"`
	Expect  string `json:"expect"`
	Why     string `json:"why"`
}

type replayFile struct {
	DevicePub string       `json:"device_pub"`
	Cases     []replayCase `json:"cases"`
}

func loadReplay(t *testing.T) replayFile {
	t.Helper()
	path := filepath.Join("..", "tests", "replay-vectors.json")
	handle, err := os.Open(path)
	if err != nil {
		t.Fatalf("%s: %v", path, err)
	}
	defer handle.Close()
	raw, err := io.ReadAll(handle)
	if err != nil {
		t.Fatalf("%s: %v", path, err)
	}
	var out replayFile
	if err := json.Unmarshal(raw, &out); err != nil {
		t.Fatalf("%s: %v", path, err)
	}
	if len(out.Cases) < 6 {
		t.Fatalf("%s hat nur %d Faelle - zu wenig, um etwas zu messen",
			path, len(out.Cases))
	}
	return out
}

// Die eingecheckten Bytes: dieselbe Datei pruefen alle fuenf Sprachen, und
// keine von ihnen braucht dafuer einen Unterschreiber.
func TestReplayVectors(t *testing.T) {
	file := loadReplay(t)
	devicePub, err := hex.DecodeString(file.DevicePub)
	if err != nil || len(devicePub) != KeyLen {
		t.Fatalf("device_pub unbrauchbar")
	}
	valid := 0
	for _, item := range file.Cases {
		nonce, _ := hex.DecodeString(item.Nonce)
		binHash, _ := hex.DecodeString(item.BinHash)
		signature, _ := hex.DecodeString(item.Sig)
		got := ed25519.Verify(ed25519.PublicKey(devicePub),
			message(attestDomain, nonce, binHash), signature)
		if got != item.Valid {
			t.Fatalf("%s: erwartet %v, bekommen %v (%s)",
				item.Name, item.Valid, got, item.Why)
		}
		if item.Valid {
			valid++
		}
	}
	if valid == 0 {
		t.Fatal("kein einziger gueltiger Fall - dann beweist kein " +
			"abgewiesener Fall etwas")
	}
	t.Logf("%d Faelle, davon %d gueltig", len(file.Cases), valid)
}
