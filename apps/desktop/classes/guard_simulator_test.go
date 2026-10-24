// Der Dongle-Schutz gegen die GETEILTE Attrappe aus tests/simulator -
// ueber einen Port, mit echtem Rahmen, als eigenes Programm gestartet.
//
// Warum das hier steht und nicht bei den anderen Faellen: die uebrigen
// Pruefungen laufen gegen guard_device.go im Arbeitsspeicher. Das misst die
// Rechnung, aber nicht den Weg - und tests/simulator ist die Attrappe, die
// Oberflaeche und Kommandozeile benutzen. Antwortete die dort anders, waere
// jeder Nachweis nur fuer meinen eigenen Nachbau gueltig.
//
// Gemessen wird also: dasselbe Protokoll, derselbe Transport, dasselbe
// Programm, das auch die anderen Straenge benutzen.
package classes

import (
	"bytes"
	"crypto/ed25519"
	"encoding/hex"
	"net"
	"os"
	"os/exec"
	"path/filepath"
	"runtime"
	"strconv"
	"strings"
	"sync"
	"testing"
	"time"
)

// simulatorPin ist die PIN der geteilten Attrappe (tests/simulator).
const simulatorPin = "123456"

// startSharedSimulator baut tests/simulator, startet sie auf einem freien
// Port und liest die Schluessel aus ihrer Ausgabe.
func startSharedSimulator(t *testing.T, extra ...string) (address string,
	deviceKey, issuerKey []byte) {
	t.Helper()

	name := "osk-sim-pruefung"
	if runtime.GOOS == "windows" {
		name += ".exe"
	}
	binary := filepath.Join(t.TempDir(), name)
	build := exec.Command("go", "build", "-o", binary, "./tests/simulator")
	build.Dir = ".."
	build.Env = append(os.Environ(), "CGO_ENABLED=0")
	if out, err := build.CombinedOutput(); err != nil {
		// Kein Ueberspringen: ohne diese Attrappe ist der Nachweis nicht
		// zu fuehren, und ein stiller Sprung waere die schlechtere Lage.
		t.Fatalf("tests/simulator laesst sich nicht bauen: %v\n%s", err, out)
	}

	port := freeLoopbackPort(t)
	args := append([]string{"-port", strconv.Itoa(port)}, extra...)
	run := exec.Command(binary, args...)
	output := &safeBuffer{}
	run.Stdout = output
	run.Stderr = output
	if err := run.Start(); err != nil {
		t.Fatalf("Attrappe laesst sich nicht starten: %v", err)
	}
	t.Cleanup(func() {
		if run.Process != nil {
			run.Process.Kill()
		}
		run.Wait()
		if t.Failed() {
			t.Log("Ausgabe der Attrappe:\n" + output.String())
		}
	})

	address = "127.0.0.1:" + strconv.Itoa(port)
	deadline := time.Now().Add(30 * time.Second)
	for time.Now().Before(deadline) {
		conn, err := net.DialTimeout("tcp", address, time.Second)
		if err == nil {
			conn.Close()
			break
		}
		time.Sleep(50 * time.Millisecond)
	}

	// Die Schluessel stehen in der Ausgabe. Stehen sie nicht da, ist die
	// Attrappe eine andere als die, gegen die hier geprueft werden soll.
	for time.Now().Before(deadline) {
		deviceKey = keyFromOutput(output.String(), "Geraeteschluessel ")
		issuerKey = keyFromOutput(output.String(), "Herstellerschluessel ")
		if len(deviceKey) == GuardKeyLen && len(issuerKey) == GuardKeyLen {
			return address, deviceKey, issuerKey
		}
		time.Sleep(50 * time.Millisecond)
	}
	t.Fatalf("die Attrappe nennt keine Schluessel:\n%s", output.String())
	return "", nil, nil
}

// keyFromOutput liest den Hexwert hinter einem Schlagwort.
func keyFromOutput(text, label string) []byte {
	for _, line := range strings.Split(text, "\n") {
		line = strings.TrimSpace(line)
		if !strings.HasPrefix(line, label) {
			continue
		}
		raw, err := hex.DecodeString(strings.TrimPrefix(line, label))
		if err != nil {
			return nil
		}
		return raw
	}
	return nil
}

// safeBuffer ist ein Puffer, in den ein anderer Prozess schreiben darf,
// waehrend der Fall ihn liest.
type safeBuffer struct {
	mu  sync.Mutex
	buf bytes.Buffer
}

func (b *safeBuffer) Write(p []byte) (int, error) {
	b.mu.Lock()
	defer b.mu.Unlock()
	return b.buf.Write(p)
}

func (b *safeBuffer) String() string {
	b.mu.Lock()
	defer b.mu.Unlock()
	return b.buf.String()
}

// freeLoopbackPort nimmt einen freien Port. Er wird gleich wieder
// freigegeben - zwischen hier und dem Start kann ihn theoretisch ein
// anderer greifen, darum wartet der Aufrufer auf die Verbindung statt
// anzunehmen, dass sie steht.
func freeLoopbackPort(t *testing.T) int {
	t.Helper()
	listener, err := net.Listen("tcp", "127.0.0.1:0")
	if err != nil {
		t.Fatalf("kein freier Port: %v", err)
	}
	defer listener.Close()
	return listener.Addr().(*net.TCPAddr).Port
}

// guardAgainstSharedSimulator baut einen Guard auf der geteilten Attrappe
// und oeffnet ihren Tresor.
func guardAgainstSharedSimulator(t *testing.T, extra ...string) (*Guard, []byte) {
	t.Helper()
	address, deviceKey, issuerKey := startSharedSimulator(t, extra...)
	client, err := Connect(SimPrefix+address, "dongle-pruefung")
	if err != nil {
		t.Fatalf("verbinden: %v", err)
	}
	t.Cleanup(func() { client.Close() })
	if err := client.Unlock([]byte(simulatorPin)); err != nil {
		t.Fatalf("Tresor oeffnen: %v", err)
	}
	cfg := DefaultGuardConfig()
	cfg.DevicePublicKey = deviceKey
	cfg.IssuerPublicKey = issuerKey
	cfg.BinaryPath = guardTestBinary(t, "programm", []byte("ein Programm"))
	guard, err := NewGuard(client, cfg)
	if err != nil {
		t.Fatalf("Guard: %v", err)
	}
	return guard, deviceKey
}

// Der ganze Weg gegen die geteilte Attrappe: Praesenz, Verschliessen,
// Lizenz, Auftrag, Schluesselaustausch.
func TestDongleAgainstSharedSimulator(t *testing.T) {
	guard, deviceKey := guardAgainstSharedSimulator(t)

	// 1. Praesenz, und die Antwort haelt der Pruefung.
	answer, err := guard.Ask()
	if err != nil {
		t.Fatalf("fragen: %v", err)
	}
	if err := GuardVerify(deviceKey, answer); err != nil {
		t.Fatalf("die Attrappe unterschreibt nicht pruefbar: %v", err)
	}
	if err := guard.Present(); err != nil {
		t.Fatalf("Praesenz: %v", err)
	}

	// 2. Und sie unterschreibt wirklich - eine erfundene Antwort gleicher
	// Laenge wuerde hier auffallen. Das ist der Grund fuer diesen Fall:
	// frueher antwortete die Attrappe mit der Frage XOR 0x5a.
	if len(answer.Signature) != GuardSigLen {
		t.Fatalf("Unterschrift hat %d Byte", len(answer.Signature))
	}
	if bytes.Equal(answer.Signature[:GuardNonceLen], answer.Nonce) {
		t.Fatal("die Antwort ist die Frage selbst")
	}
	xored := make([]byte, GuardNonceLen)
	for i := range xored {
		xored[i] = answer.Nonce[i] ^ 0x5a
	}
	if bytes.Equal(answer.Signature[:GuardNonceLen], xored) {
		t.Fatal("die Antwort ist die Frage XOR 0x5a - die Attrappe " +
			"unterschreibt nicht")
	}

	// 3. Verschliessen und wieder aufmachen.
	plain := []byte("die Tabelle, ohne die das Programm nichts kann")
	blob, err := guard.Seal("datenmodell", []byte("v1"), plain)
	if err != nil {
		t.Fatalf("verschliessen: %v", err)
	}
	back, err := guard.Unseal("datenmodell", []byte("v1"), blob)
	if err != nil {
		t.Fatalf("oeffnen: %v", err)
	}
	if !bytes.Equal(back, plain) {
		t.Fatal("heraus kam etwas anderes")
	}
	if _, err := guard.Unseal("datenmodell", []byte("v2"), blob); err == nil {
		t.Fatal("mit falschem Zusatz aufgegangen")
	}

	// 4. Die Lizenz aus dem Tresor der Attrappe.
	license, err := guard.License("beispiel-programm")
	if err != nil {
		t.Fatalf("Lizenz: %v", err)
	}
	if license.Seats != 3 || !license.HasTier("berichte") {
		t.Fatalf("falsche Lizenz: %+v", license)
	}
	if license.Expired(time.Now()) {
		t.Fatal("die Beispiel-Lizenz ist abgelaufen")
	}

	// 5. Ein Auftrag, unterschrieben vom Knotenschluessel.
	public, err := guard.NodePublic("auftraege", "ed25519")
	if err != nil {
		t.Fatalf("Knotenschluessel: %v", err)
	}
	job := []byte(`{"tat":"abbild-holen"}`)
	signature, err := guard.NodeSign("auftraege", job)
	if err != nil {
		t.Fatalf("unterschreiben: %v", err)
	}
	if err := NodeVerify(public.PublicKey, "auftraege", job, signature); err != nil {
		t.Fatalf("die Unterschrift gilt nicht: %v", err)
	}
	bent := append([]byte{}, job...)
	bent[3] ^= 0x01
	if err := NodeVerify(public.PublicKey, "auftraege", bent, signature); err == nil {
		t.Fatal("ein veraenderter Auftrag galt")
	}

	// 6. Schluesselaustausch gegen eine Gegenseite im Arbeitsspeicher.
	peer, peerGuard := nodeTestGuard(t, "gegenseite-sim", GuardFakeOptions{})
	_ = peer
	peerPublic, err := peerGuard.NodePublic("auftraege", "x25519")
	if err != nil {
		t.Fatalf("Gegenseite: %v", err)
	}
	secret, err := guard.NodeAgree("auftraege", peerPublic.PublicKey)
	if err != nil {
		t.Fatalf("Schluesselaustausch: %v", err)
	}
	if len(secret) != NodeSecretLen {
		t.Fatalf("Geheimnis hat %d Byte", len(secret))
	}
}

// Praesenz muss OHNE PIN gehen: ein geschuetztes Programm soll nachsehen
// koennen, ob der Stick steckt, ohne den Tresor zu oeffnen.
func TestPresenceNeedsNoPinOnTheSharedSimulator(t *testing.T) {
	address, deviceKey, _ := startSharedSimulator(t)
	client, err := Connect(SimPrefix+address, "dongle-pruefung")
	if err != nil {
		t.Fatalf("verbinden: %v", err)
	}
	defer client.Close()

	cfg := DefaultGuardConfig()
	cfg.DevicePublicKey = deviceKey
	cfg.BinaryPath = guardTestBinary(t, "programm", []byte("ein Programm"))
	guard, err := NewGuard(client, cfg)
	if err != nil {
		t.Fatalf("Guard: %v", err)
	}
	// Kein Unlock vorher.
	if err := guard.Present(); err != nil {
		t.Fatalf("Praesenz verlangt eine PIN: %v", err)
	}
	// Der Tresor bleibt dabei zu - die Lizenz kommt nicht heraus.
	if _, err := guard.License("beispiel-programm"); err == nil {
		t.Fatal("die Lizenz kam ohne PIN heraus")
	}
}

// Ein Stueck, das kein ECDH kann, meldet das auch ueber den Draht - und
// unterschreibt weiter.
func TestSharedSimulatorWithoutEcdh(t *testing.T) {
	guard, _ := guardAgainstSharedSimulator(t, "-no-ecdh")
	_, peerGuard := nodeTestGuard(t, "gegenseite-fips-sim", GuardFakeOptions{})
	peerPublic, err := peerGuard.NodePublic("auftraege", "x25519")
	if err != nil {
		t.Fatalf("Gegenseite: %v", err)
	}
	_, err = guard.NodeAgree("auftraege", peerPublic.PublicKey)
	if !GuardNoECDH(err) {
		t.Fatalf("erwartet error.guard.noecdh, bekommen %v", err)
	}
	// Unterschreiben geht weiter - nur der Austausch faellt weg.
	public, err := guard.NodePublic("auftraege", "ed25519")
	if err != nil {
		t.Fatalf("Knotenschluessel: %v", err)
	}
	job := []byte("ein Auftrag")
	signature, err := guard.NodeSign("auftraege", job)
	if err != nil {
		t.Fatalf("unterschreiben: %v", err)
	}
	if err := NodeVerify(public.PublicKey, "auftraege", job, signature); err != nil {
		t.Fatalf("die Unterschrift gilt nicht: %v", err)
	}
}

// Die geteilte Attrappe und die im Arbeitsspeicher muessen bei gleicher
// Saat dasselbe unterschreiben - sonst sind es zwei Rechnungen, und die
// Pruefungen im Arbeitsspeicher sagen nichts ueber den Draht.
func TestBothSimulatorsAgree(t *testing.T) {
	_, deviceKey, _ := startSharedSimulator(t)
	// Dieselbe Saat wie in tests/simulator (demoSeed).
	inMemory, err := NewGuardFakeDevice(GuardFakeOptions{
		Seed: []byte("osk-simulator-1")})
	if err != nil {
		t.Fatalf("Attrappe: %v", err)
	}
	if !bytes.Equal(deviceKey, inMemory.PublicKey()) {
		t.Fatalf("die beiden Attrappen haben verschiedene Schluessel:\n"+
			"  Draht:           %x\n  Arbeitsspeicher: %x\n"+
			"Dann gilt keine Messung der einen fuer die andere.",
			deviceKey, inMemory.PublicKey())
	}
	// Und sie unterschreiben dieselbe Frage gleich.
	nonce := make([]byte, GuardNonceLen)
	binHash := make([]byte, GuardHashLen)
	for i := range nonce {
		nonce[i] = byte(i)
		binHash[i] = byte(255 - i)
	}
	want := ed25519.Sign(inMemory.identity,
		GuardMessage(guardAttestDomain, nonce, binHash))
	if len(want) != GuardSigLen {
		t.Fatalf("Unterschrift hat %d Byte", len(want))
	}
	if !ed25519.Verify(ed25519.PublicKey(deviceKey),
		GuardMessage(guardAttestDomain, nonce, binHash), want) {
		t.Fatal("die Unterschrift der einen gilt nicht fuer die andere")
	}
}
