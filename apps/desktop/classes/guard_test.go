// Pruefungen der ersten Stufe: Frage und Antwort, Bindung an die
// Programmdatei, Abziehen des Sticks. Alles gegen die Attrappe aus
// guard_device.go - kein echtes Geraet war beteiligt.
package classes

import (
	"bytes"
	"context"
	"crypto/ed25519"
	"crypto/rand"
	"crypto/sha256"
	"errors"
	"os"
	"path/filepath"
	"testing"
	"time"
)

// guardTestDevice baut Attrappe, Client und Guard fuer eine Pruefung.
func guardTestDevice(t *testing.T, opts GuardFakeOptions) (*GuardFakeDevice, *Guard, func()) {
	t.Helper()
	if len(opts.Seed) == 0 {
		opts.Seed = []byte("pruefsaat-dongle-1")
	}
	device, err := NewGuardFakeDevice(opts)
	if err != nil {
		t.Fatalf("Attrappe laesst sich nicht bauen: %v", err)
	}
	client, unplug := device.GuardFakePair("test")
	cfg := DefaultGuardConfig()
	cfg.DevicePublicKey = device.PublicKey()
	cfg.BinaryPath = guardTestBinary(t, "programm", []byte("so sieht das Programm aus"))
	guard, err := NewGuard(client, cfg)
	if err != nil {
		t.Fatalf("Guard laesst sich nicht bauen: %v", err)
	}
	return device, guard, unplug
}

// guardTestBinary legt eine Datei an, die als Programmdatei dient.
func guardTestBinary(t *testing.T, name string, content []byte) string {
	t.Helper()
	path := filepath.Join(t.TempDir(), name)
	if err := os.WriteFile(path, content, 0o600); err != nil {
		t.Fatalf("Pruefdatei: %v", err)
	}
	return path
}

func TestGuardPresentAcceptsRealSignature(t *testing.T) {
	_, guard, _ := guardTestDevice(t, GuardFakeOptions{})
	if err := guard.Present(); err != nil {
		t.Fatalf("der Stick steckt, trotzdem Fehler: %v", err)
	}
}

// Jede Frage ist neu: zwei Runden duerfen nie denselben Wert benutzen.
func TestGuardChallengeIsFreshEveryTime(t *testing.T) {
	_, guard, _ := guardTestDevice(t, GuardFakeOptions{})
	seen := map[string]bool{}
	for runde := 0; runde < 20; runde++ {
		answer, err := guard.Ask()
		if err != nil {
			t.Fatalf("Runde %d: %v", runde, err)
		}
		key := string(answer.Nonce)
		if seen[key] {
			t.Fatalf("Runde %d: dieselbe Frage zum zweiten Mal", runde)
		}
		seen[key] = true
	}
}

// Eine aufgezeichnete Antwort nuetzt nichts: zu einer neuen Frage passt sie
// nicht. Das ist der Kern von Frage und Antwort, und es wird hier gemessen,
// nicht angenommen.
func TestGuardRecordedAnswerDoesNotWork(t *testing.T) {
	device, guard, _ := guardTestDevice(t, GuardFakeOptions{})
	recorded, err := guard.Ask()
	if err != nil {
		t.Fatalf("erste Runde: %v", err)
	}
	if err := GuardVerify(device.PublicKey(), recorded); err != nil {
		t.Fatalf("die eigene Antwort muesste passen: %v", err)
	}
	// Dieselbe Unterschrift, aber eine neue Frage - so wuerde ein
	// Mitschnitt eingespielt.
	replay := recorded
	replay.Nonce = make([]byte, GuardNonceLen)
	if _, err := rand.Read(replay.Nonce); err != nil {
		t.Fatalf("Zufall: %v", err)
	}
	if err := GuardVerify(device.PublicKey(), replay); err == nil {
		t.Fatal("die aufgezeichnete Antwort wurde zu einer neuen Frage angenommen")
	}
}

// Ein anderer Stick wird erkannt, auch wenn er richtig unterschreibt.
func TestGuardOtherDeviceIsRejected(t *testing.T) {
	device, _, _ := guardTestDevice(t, GuardFakeOptions{})
	stranger, err := NewGuardFakeDevice(GuardFakeOptions{Seed: []byte("ein anderer stick")})
	if err != nil {
		t.Fatalf("zweite Attrappe: %v", err)
	}
	strangerClient := stranger.GuardFakeClient("test")
	cfg := DefaultGuardConfig()
	cfg.DevicePublicKey = device.PublicKey() // eingetragen ist der erste
	cfg.BinaryPath = guardTestBinary(t, "programm", []byte("egal"))
	guard, err := NewGuard(strangerClient, cfg)
	if err != nil {
		t.Fatalf("Guard: %v", err)
	}
	err = guard.Present()
	if err == nil {
		t.Fatal("ein fremder Stick wurde angenommen")
	}
	var problem *Problem
	if !errors.As(err, &problem) || problem.Key != "error.guard.wrongdevice" {
		t.Fatalf("falsche Ursache: %v", err)
	}
}

// Bindung an die Programmdatei: eine veraenderte Datei bekommt keine
// gueltige Antwort. Gemessen wird dabei beides - dass die Pruefsumme in die
// Frage eingeht UND dass die Antwort darum nicht mehr passt.
func TestGuardChangedBinaryGetsNoValidAnswer(t *testing.T) {
	device, err := NewGuardFakeDevice(GuardFakeOptions{Seed: []byte("bindung")})
	if err != nil {
		t.Fatalf("Attrappe: %v", err)
	}
	original := []byte("das ausgelieferte Programm")
	path := guardTestBinary(t, "programm", original)

	cfg := DefaultGuardConfig()
	cfg.DevicePublicKey = device.PublicKey()
	cfg.BinaryPath = path
	guard, err := NewGuard(device.GuardFakeClient("test"), cfg)
	if err != nil {
		t.Fatalf("Guard: %v", err)
	}
	wantHash := sha256.Sum256(original)
	if !bytes.Equal(guard.BinaryHash(), wantHash[:]) {
		t.Fatal("die Pruefsumme der Programmdatei stimmt nicht")
	}
	answer, err := guard.Ask()
	if err != nil {
		t.Fatalf("Runde: %v", err)
	}
	if !bytes.Equal(answer.BinHash, wantHash[:]) {
		t.Fatal("die Pruefsumme geht nicht in die Frage ein")
	}
	if err := GuardVerify(device.PublicKey(), answer); err != nil {
		t.Fatalf("die unveraenderte Datei muesste durchkommen: %v", err)
	}

	// Jetzt dieselbe Antwort, aber mit der Pruefsumme einer veraenderten
	// Datei: die Unterschrift gilt nicht mehr.
	patched := sha256.Sum256(append(original, byte('!')))
	forged := answer
	forged.BinHash = patched[:]
	if err := GuardVerify(device.PublicKey(), forged); err == nil {
		t.Fatal("eine veraenderte Programmdatei bekam eine gueltige Antwort")
	}

	// Und der ganze Weg: die Datei wirklich aendern, neu starten, und die
	// Antwort gegen die ALTE Pruefsumme pruefen - so wuerde ein geaendertes
	// Programm versuchen, mit der alten Bindung weiterzulaufen.
	if err := os.WriteFile(path, append(original, byte('!')), 0o600); err != nil {
		t.Fatalf("Datei aendern: %v", err)
	}
	guard2, err := NewGuard(device.GuardFakeClient("test"), cfg)
	if err != nil {
		t.Fatalf("Guard nach Aenderung: %v", err)
	}
	if bytes.Equal(guard2.BinaryHash(), wantHash[:]) {
		t.Fatal("die Pruefsumme hat sich nicht geaendert")
	}
	answer2, err := guard2.Ask()
	if err != nil {
		t.Fatalf("Runde nach Aenderung: %v", err)
	}
	withOldHash := answer2
	withOldHash.BinHash = wantHash[:]
	if err := GuardVerify(device.PublicKey(), withOldHash); err == nil {
		t.Fatal("die neue Antwort passte zur alten Pruefsumme")
	}
}

// Beim Abziehen bricht das geschuetzte Programm ab.
func TestGuardWatchStopsWhenDeviceIsPulled(t *testing.T) {
	_, guard, unplug := guardTestDevice(t, GuardFakeOptions{})
	guard.cfg.Interval = 5 * time.Millisecond

	lost := make(chan error, 1)
	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()
	go guard.Watch(ctx, func(err error) { lost <- err })

	// Erst laeuft es: ein paar Runden ohne Abbruch.
	time.Sleep(40 * time.Millisecond)
	select {
	case err := <-lost:
		t.Fatalf("abgebrochen, obwohl der Stick steckt: %v", err)
	default:
	}

	unplug()
	select {
	case err := <-lost:
		if err == nil {
			t.Fatal("Abbruch ohne Grund")
		}
	case <-time.After(2 * time.Second):
		t.Fatal("der Abzug wurde nicht gemerkt")
	}
}

// Ohne eingetragenen Schluessel startet der Schutz nicht - lieber gar nicht
// laufen als scheinbar schuetzen.
func TestGuardRefusesWithoutDeviceKey(t *testing.T) {
	device, err := NewGuardFakeDevice(GuardFakeOptions{Seed: []byte("ohne")})
	if err != nil {
		t.Fatalf("Attrappe: %v", err)
	}
	cases := []struct {
		name string
		cfg  GuardConfig
		want string
	}{
		{"kein Schluessel", GuardConfig{}, "error.guard.key"},
		{"zu kurzer Schluessel", GuardConfig{DevicePublicKey: make([]byte, 31)},
			"error.guard.key"},
		{"unbrauchbarer Herstellerschluessel", GuardConfig{
			DevicePublicKey: device.PublicKey(),
			IssuerPublicKey: make([]byte, 5),
		}, "error.guard.issuer"},
	}
	for _, one := range cases {
		t.Run(one.name, func(t *testing.T) {
			_, err := NewGuard(device.GuardFakeClient("test"), one.cfg)
			var problem *Problem
			if !errors.As(err, &problem) || problem.Key != one.want {
				t.Fatalf("erwartet %s, bekommen %v", one.want, err)
			}
		})
	}
}

// Ein Geraet, das eine Unterschrift falscher Laenge schickt, wird
// abgewiesen - und zwar mit der Ursache "Antwort passt nicht", nicht mit
// "Unterschrift stimmt nicht". Die beiden sagen verschiedene Dinge.
func TestGuardVerifyChecksLengths(t *testing.T) {
	device, err := NewGuardFakeDevice(GuardFakeOptions{Seed: []byte("laengen")})
	if err != nil {
		t.Fatalf("Attrappe: %v", err)
	}
	good := GuardAnswer{
		Nonce:     make([]byte, GuardNonceLen),
		BinHash:   make([]byte, GuardHashLen),
		Signature: make([]byte, GuardSigLen),
		PublicKey: device.PublicKey(),
	}
	cases := map[string]func(a *GuardAnswer){
		"Frage zu kurz":        func(a *GuardAnswer) { a.Nonce = a.Nonce[:8] },
		"Pruefsumme zu kurz":   func(a *GuardAnswer) { a.BinHash = a.BinHash[:8] },
		"Unterschrift zu kurz": func(a *GuardAnswer) { a.Signature = a.Signature[:8] },
	}
	for name, bend := range cases {
		t.Run(name, func(t *testing.T) {
			answer := good
			bend(&answer)
			err := GuardVerify(device.PublicKey(), answer)
			var problem *Problem
			if !errors.As(err, &problem) || problem.Key != "error.guard.answer" {
				t.Fatalf("erwartet error.guard.answer, bekommen %v", err)
			}
		})
	}
}

// Das Geraet setzt den Zweck selbst davor. Darum kann eine Antwort auf eine
// Praesenzfrage nie als Auftragsunterschrift durchgehen - und umgekehrt.
// Ohne diese Trennung koennte jemand einen Auftrag als Frage hineingeben.
func TestGuardDomainsCannotBeMixedUp(t *testing.T) {
	device, err := NewGuardFakeDevice(GuardFakeOptions{Seed: []byte("zwecke")})
	if err != nil {
		t.Fatalf("Attrappe: %v", err)
	}
	nonce := make([]byte, GuardNonceLen)
	binHash := make([]byte, GuardHashLen)
	attest := GuardMessage(guardAttestDomain, nonce, binHash)
	sign := NodeSigned("knoten-1", binHash)
	if bytes.Equal(attest, sign) {
		t.Fatal("beide Zwecke bilden dieselbe Bytefolge")
	}
	// Eine Praesenz-Antwort gegen den Auftrags-Zweck gehalten: faellt.
	public := ed25519.PublicKey(device.PublicKey())
	signature := ed25519.Sign(device.identity, attest)
	if ed25519.Verify(public, sign, signature) {
		t.Fatal("eine Praesenz-Antwort gilt als Auftragsunterschrift")
	}
}

// Der leere Wert von GuardConfig muss der sichere sein: wer das Feld
// vergisst, bekommt die Bindung an die Programmdatei - nicht umgekehrt.
// Vorher hiess das Feld BindBinary, und ein vergessenes Feld hiess: keine
// Bindung, und niemand haette es gemerkt.
func TestBinaryBindingIsOnByDefault(t *testing.T) {
	device, err := NewGuardFakeDevice(GuardFakeOptions{Seed: []byte("vorgabe")})
	if err != nil {
		t.Fatalf("Attrappe: %v", err)
	}
	content := []byte("das ausgelieferte Programm")
	want := sha256.Sum256(content)

	// Eine Einstellung, die NUR den Schluessel setzt - alles andere leer.
	guard, err := NewGuard(device.GuardFakeClient("test"), GuardConfig{
		DevicePublicKey: device.PublicKey(),
		BinaryPath:      guardTestBinary(t, "programm", content),
	})
	if err != nil {
		t.Fatalf("Guard: %v", err)
	}
	if !bytes.Equal(guard.BinaryHash(), want[:]) {
		t.Fatal("ohne Zutun wurde die Programmdatei nicht gebunden")
	}

	// Und ausschalten geht nur ausdruecklich.
	loose, err := NewGuard(device.GuardFakeClient("test"), GuardConfig{
		DevicePublicKey: device.PublicKey(),
		NoBinaryBinding: true,
	})
	if err != nil {
		t.Fatalf("Guard ohne Bindung: %v", err)
	}
	if !bytes.Equal(loose.BinaryHash(), make([]byte, GuardHashLen)) {
		t.Fatal("ausgeschaltet und trotzdem gebunden")
	}
}
