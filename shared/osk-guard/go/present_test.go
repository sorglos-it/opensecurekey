// Pruefungen der ersten Stufe in der Bibliothek selbst - gegen einen
// LUEGENDEN Dienst auf der Rueckschleife.
//
// Das ist der Angreifer, um den es bei Stufe 1 ueberhaupt geht: jedes
// Programm auf diesem Rechner kann einen Port aufmachen und "OK" antworten.
// Darum prueft die Bibliothek die Unterschrift selbst - und darum muss
// gemessen werden, dass sie es wirklich tut.
//
// Der Anlass fuer diese Datei: die Python-Fassung hat ihren eigenen Leser
// mutiert und dabei gefunden, dass die Pruefung "pub ist der eingetragene
// Schluessel" ohne Fall war. Sie aendert nicht das Ergebnis - abgewiesen
// wird so oder so - sondern den SATZ, den der Anwender liest: "Es steckt
// ein anderer Sicherheitsschluessel" gegen "Die Unterschrift stimmt nicht".
// Das ist ein Unterschied, der jemanden an die falsche Stelle schickt.
package oskguard

import (
	"bufio"
	"crypto/ed25519"
	"encoding/hex"
	"net"
	"os"
	"path/filepath"
	"strings"
	"testing"
	"time"
)

// fakeAgent ist ein Dienst, der antwortet, was ihm gesagt wird.
type fakeAgent struct {
	listener net.Listener
	answer   func(line string) string
}

func startFakeAgent(t *testing.T, answer func(string) string) *fakeAgent {
	t.Helper()
	listener, err := net.Listen("tcp", "127.0.0.1:0")
	if err != nil {
		t.Fatalf("Port: %v", err)
	}
	agent := &fakeAgent{listener: listener, answer: answer}
	go agent.serve()
	t.Cleanup(func() { listener.Close() })
	return agent
}

func (a *fakeAgent) serve() {
	for {
		conn, err := a.listener.Accept()
		if err != nil {
			return
		}
		go func() {
			defer conn.Close()
			reader := bufio.NewReaderSize(conn, 1<<20)
			for {
				conn.SetDeadline(time.Now().Add(10 * time.Second))
				line, err := reader.ReadString('\n')
				if err != nil {
					return
				}
				out := a.answer(strings.TrimRight(line, "\r\n"))
				if _, err := conn.Write([]byte(out + "\n")); err != nil {
					return
				}
			}
		}()
	}
}

func (a *fakeAgent) address() string { return a.listener.Addr().String() }

// testBinary legt eine Datei an, die als Programmdatei dient.
func testBinary(t *testing.T) string {
	t.Helper()
	path := filepath.Join(t.TempDir(), "programm")
	if err := os.WriteFile(path, []byte("ein Programm"), 0o600); err != nil {
		t.Fatalf("Pruefdatei: %v", err)
	}
	return path
}

// testKeyPair gibt ein wiederholbares Schluesselpaar.
func testKeyPair(t *testing.T, seedText string) (ed25519.PublicKey, ed25519.PrivateKey) {
	t.Helper()
	seed := make([]byte, ed25519.SeedSize)
	copy(seed, seedText)
	private := ed25519.NewKeyFromSeed(seed)
	return private.Public().(ed25519.PublicKey), private
}

// answerAttest baut die Antwort eines Dienstes, der mit key unterschreibt
// und pub als Geraeteschluessel ausgibt. Beide getrennt, damit sich der
// Fall "richtiger Schluessel genannt, falsch unterschrieben" bauen laesst.
func answerAttest(private ed25519.PrivateKey, pub []byte, bend bool) func(string) string {
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
		nonce, err := hex.DecodeString(fields[1])
		if err != nil {
			return "ERR error.guard.request"
		}
		binHash, err := hex.DecodeString(fields[2])
		if err != nil {
			return "ERR error.guard.request"
		}
		signature := ed25519.Sign(private, message(attestDomain, nonce, binHash))
		if bend {
			signature[0] ^= 0x01
		}
		return "OK sig=" + hex.EncodeToString(signature) +
			" pub=" + hex.EncodeToString(pub)
	}
}

// Der ehrliche Fall zuerst: richtig unterschrieben, richtiger Schluessel.
func TestPresentAcceptsTheRightDevice(t *testing.T) {
	public, private := testKeyPair(t, "echtes-geraet")
	agent := startFakeAgent(t, answerAttest(private, public, false))
	guard, err := Open(agent.address(), public, nil)
	if err != nil {
		t.Fatalf("Open: %v", err)
	}
	if err := guard.Present(testBinary(t)); err != nil {
		t.Fatalf("der richtige Stick wurde abgewiesen: %v", err)
	}
}

// Ein fremder Stick, der tadellos unterschreibt: abgewiesen, und zwar mit
// dem Satz "es steckt ein anderer Schluessel" - nicht mit "die
// Unterschrift stimmt nicht".
func TestPresentNamesTheWrongDeviceAsSuch(t *testing.T) {
	ours, _ := testKeyPair(t, "unser-geraet")
	strangerPublic, strangerPrivate := testKeyPair(t, "fremdes-geraet")
	agent := startFakeAgent(t, answerAttest(strangerPrivate, strangerPublic, false))
	guard, err := Open(agent.address(), ours, nil)
	if err != nil {
		t.Fatalf("Open: %v", err)
	}
	err = guard.Present(testBinary(t))
	if err == nil {
		t.Fatal("ein fremder Stick wurde angenommen")
	}
	if Code(err) != "error.guard.wrongdevice" {
		t.Fatalf("erwartet error.guard.wrongdevice, bekommen %s", Code(err))
	}
}

// Ein Dienst, der den richtigen Schluessel NENNT, aber falsch
// unterschreibt: dann ist die Unterschrift der Grund, nicht das Geraet.
func TestPresentNamesABadSignatureAsSuch(t *testing.T) {
	public, private := testKeyPair(t, "echtes-geraet")
	agent := startFakeAgent(t, answerAttest(private, public, true))
	guard, err := Open(agent.address(), public, nil)
	if err != nil {
		t.Fatalf("Open: %v", err)
	}
	err = guard.Present(testBinary(t))
	if err == nil {
		t.Fatal("eine verbogene Unterschrift wurde angenommen")
	}
	if Code(err) != "error.guard.signature" {
		t.Fatalf("erwartet error.guard.signature, bekommen %s", Code(err))
	}
}

// Ein Dienst, der den richtigen Schluessel nennt, aber mit einem anderen
// unterschreibt - der naheliegende Versuch, die Pruefung zu ueberlisten.
func TestPresentCatchesAForeignSignatureUnderOurName(t *testing.T) {
	public, _ := testKeyPair(t, "echtes-geraet")
	_, strangerPrivate := testKeyPair(t, "fremdes-geraet")
	agent := startFakeAgent(t, answerAttest(strangerPrivate, public, false))
	guard, err := Open(agent.address(), public, nil)
	if err != nil {
		t.Fatalf("Open: %v", err)
	}
	if err := guard.Present(testBinary(t)); Code(err) != "error.guard.signature" {
		t.Fatalf("erwartet error.guard.signature, bekommen %v", err)
	}
}

// Die Pruefsumme der Programmdatei geht in die Frage ein: eine veraenderte
// Datei bekommt keine gueltige Antwort. Gemessen an einem Dienst, der die
// Pruefsumme stillschweigend austauscht - genau das muesste ein Angreifer
// tun, der eine geaenderte Datei laufen lassen will.
func TestPresentBindsTheProgramFile(t *testing.T) {
	public, private := testKeyPair(t, "echtes-geraet")
	agent := startFakeAgent(t, func(line string) string {
		fields := strings.Fields(line)
		if len(fields) > 0 && fields[0] == "ping" {
			return "OK v=1"
		}
		if len(fields) != 3 {
			return "ERR error.guard.request"
		}
		nonce, _ := hex.DecodeString(fields[1])
		// Unterschrieben wird eine ANDERE Pruefsumme als die gefragte.
		other := make([]byte, HashLen)
		for i := range other {
			other[i] = 0xAB
		}
		signature := ed25519.Sign(private, message(attestDomain, nonce, other))
		return "OK sig=" + hex.EncodeToString(signature) +
			" pub=" + hex.EncodeToString(public)
	})
	guard, err := Open(agent.address(), public, nil)
	if err != nil {
		t.Fatalf("Open: %v", err)
	}
	if err := guard.Present(testBinary(t)); Code(err) != "error.guard.signature" {
		t.Fatalf("eine fremde Pruefsumme kam durch: %v", err)
	}
}

// Jede Frage ist neu - gemessen am Dienst, der sie mitschreibt.
func TestPresentAsksSomethingNewEveryTime(t *testing.T) {
	public, private := testKeyPair(t, "echtes-geraet")
	seen := map[string]bool{}
	var twice string
	agent := startFakeAgent(t, func(line string) string {
		fields := strings.Fields(line)
		if len(fields) > 0 && fields[0] == "ping" {
			return "OK v=1"
		}
		if len(fields) != 3 {
			return "ERR error.guard.request"
		}
		if seen[fields[1]] {
			twice = fields[1]
		}
		seen[fields[1]] = true
		nonce, _ := hex.DecodeString(fields[1])
		binHash, _ := hex.DecodeString(fields[2])
		return "OK sig=" + hex.EncodeToString(
			ed25519.Sign(private, message(attestDomain, nonce, binHash))) +
			" pub=" + hex.EncodeToString(public)
	})
	guard, err := Open(agent.address(), public, nil)
	if err != nil {
		t.Fatalf("Open: %v", err)
	}
	binary := testBinary(t)
	for runde := 0; runde < 20; runde++ {
		if err := guard.Present(binary); err != nil {
			t.Fatalf("Runde %d: %v", runde, err)
		}
	}
	if twice != "" {
		t.Fatalf("dieselbe Frage zum zweiten Mal: %s", twice)
	}
	if len(seen) != 20 {
		t.Fatalf("%d verschiedene Fragen bei 20 Runden", len(seen))
	}
}

// Ein Dienst, der einfach "OK" sagt, ohne Unterschrift, kommt nicht durch.
func TestPresentRefusesAnAnswerWithoutProof(t *testing.T) {
	public, _ := testKeyPair(t, "echtes-geraet")
	for name, answer := range map[string]string{
		"nur OK":            "OK",
		"ohne Unterschrift": "OK pub=" + hex.EncodeToString(public),
		"leere Felder":      "OK sig=- pub=-",
		"Unterschrift kurz": "OK sig=00 pub=" + hex.EncodeToString(public),
		"kein Hex":          "OK sig=zz pub=" + hex.EncodeToString(public),
	} {
		t.Run(name, func(t *testing.T) {
			agent := startFakeAgent(t, func(line string) string {
				if strings.HasPrefix(line, "ping") {
					return "OK v=1"
				}
				return answer
			})
			guard, err := Open(agent.address(), public, nil)
			if err != nil {
				t.Fatalf("Open: %v", err)
			}
			if err := guard.Present(testBinary(t)); err == nil {
				t.Fatalf("durchgelassen: %q", answer)
			}
		})
	}
}

// Der Dienst darf nur auf der Rueckschleife stehen. Eine Adresse im Netz
// waere ein offener Kanal zum Stick.
func TestOpenRefusesAddressesOffTheLoopback(t *testing.T) {
	public, _ := testKeyPair(t, "echtes-geraet")
	for _, address := range []string{
		"8.8.8.8:8300", "example.org:8300", "0.0.0.0:8300",
		"kaputt", "127.0.0.1:", "",
	} {
		if _, err := Open(address, public, nil); Code(err) != "error.guard.request" {
			t.Errorf("%q: erwartet error.guard.request, bekommen %v", address, err)
		}
	}
}

// Unbrauchbare Schluessel werden abgewiesen, bevor etwas verbunden wird.
func TestOpenChecksKeyLengths(t *testing.T) {
	public, _ := testKeyPair(t, "echtes-geraet")
	if _, err := Open("127.0.0.1:1", make([]byte, 31), nil); Code(err) != "error.guard.key" {
		t.Errorf("kurzer Geraeteschluessel: %v", err)
	}
	if _, err := Open("127.0.0.1:1", nil, nil); Code(err) != "error.guard.key" {
		t.Errorf("fehlender Geraeteschluessel: %v", err)
	}
	if _, err := Open("127.0.0.1:1", public, make([]byte, 5)); Code(err) != "error.guard.issuer" {
		t.Errorf("kurzer Herstellerschluessel: %v", err)
	}
}

// Ohne eingetragenen Herstellerschluessel wird keine Lizenz geprueft,
// sondern abgelehnt - es gibt keinen Weg zu ungeprueften Werten.
func TestLicenseRefusesWithoutIssuerKey(t *testing.T) {
	public, private := testKeyPair(t, "echtes-geraet")
	agent := startFakeAgent(t, answerAttest(private, public, false))
	guard, err := Open(agent.address(), public, nil)
	if err != nil {
		t.Fatalf("Open: %v", err)
	}
	if _, err := guard.License("irgendwas"); Code(err) != "error.guard.issuer" {
		t.Fatalf("erwartet error.guard.issuer, bekommen %v", err)
	}
}

// Ein Dienst, der eine erfundene Lizenz ausgibt, kommt nicht durch - das
// ist der Fall, um den der Umbau von `license` gemacht wurde.
func TestLicenseFromALyingServiceIsRejected(t *testing.T) {
	devicePublic, devicePrivate := testKeyPair(t, "echtes-geraet")
	issuerPublic, _ := testKeyPair(t, "echter-hersteller")
	_, forgerPrivate := testKeyPair(t, "faelscher")

	// Der Dienst unterschreibt eine Lizenz mit dem falschen Schluessel.
	load := []byte{0xa1, 0x61, 'p', 0x65, 'f', 'a', 'l', 's', 'c'}
	blob := append([]byte(licenseMagic), licenseVersion)
	blob = append(blob, byte(len(load)>>8), byte(len(load)))
	blob = append(blob, load...)
	signed := append(append([]byte(licenseMagic), 0), load...)
	blob = append(blob, ed25519.Sign(forgerPrivate, signed)...)

	agent := startFakeAgent(t, func(line string) string {
		if strings.HasPrefix(line, "ping") {
			return "OK v=1"
		}
		if strings.HasPrefix(line, "attest") {
			return answerAttest(devicePrivate, devicePublic, false)(line)
		}
		return "OK lic=" + hex.EncodeToString(blob)
	})
	guard, err := Open(agent.address(), devicePublic, issuerPublic)
	if err != nil {
		t.Fatalf("Open: %v", err)
	}
	if _, err := guard.License("falsc"); Code(err) != "error.license.signature" {
		t.Fatalf("eine erfundene Lizenz kam durch: %v", err)
	}
}
