// Ein Geraet im Werkszustand, nur fuer diese Pruefungen.
//
// Warum nicht die Attrappe aus tests/simulator: die hat schon ein Profil und
// Beispieleintraege, sie stellt ein Geraet im Betrieb dar. Fuer den
// Assistenten braucht es das Gegenteil - ein Geraet, das noch nichts haelt,
// keine Betriebsart kennt und auf `setup` wartet. Die Attrappe wird von
// mehreren Straengen benutzt und bleibt darum unangetastet.
//
// Hier steht absichtlich nichts Kryptografisches: dieses Geraet rechnet
// nicht, es antwortet. Geprueft wird der Ablauf des Assistenten, nicht die
// Rechnung des Geraets.
package tests

import (
	"io"
	"net"
	"sync"
	"testing"
	"time"

	"osk/classes"
)

// frischesGeraet ist der Zustand des Geraets im Werkszustand.
type frischesGeraet struct {
	mu       sync.Mutex
	mode     string
	pin      string
	unlocked bool
	// setupCount zaehlt, wie oft `setup` angenommen wurde. Mehr als einmal
	// waere ein Fehler: die Betriebsart ist endgueltig.
	setupCount int
}

// starteFrischesGeraet lauscht auf 127.0.0.1 und gibt die Adresse zurueck.
func starteFrischesGeraet(t *testing.T) (string, *frischesGeraet) {
	t.Helper()
	listener, err := net.Listen("tcp", "127.0.0.1:0")
	if err != nil {
		t.Fatalf("lauschen: %v", err)
	}
	t.Cleanup(func() { listener.Close() })
	device := &frischesGeraet{}
	go func() {
		for {
			conn, acceptErr := listener.Accept()
			if acceptErr != nil {
				return
			}
			go device.bediene(conn)
		}
	}()
	return listener.Addr().String(), device
}

// geraeteTransport macht aus einer Verbindung einen Transport, damit
// dieselbe Rahmenlogik wie im Programm benutzt wird.
type geraeteTransport struct {
	conn net.Conn
}

func (g geraeteTransport) WriteReport(report []byte) error {
	_, err := g.conn.Write(report)
	return err
}

func (g geraeteTransport) ReadReport(timeout time.Duration) ([]byte, error) {
	if err := g.conn.SetReadDeadline(time.Now().Add(timeout)); err != nil {
		return nil, err
	}
	report := make([]byte, classes.ReportSize)
	if _, err := io.ReadFull(g.conn, report); err != nil {
		return nil, err
	}
	return report, nil
}

func (g geraeteTransport) Close() error     { return g.conn.Close() }
func (g geraeteTransport) Describe() string { return "werkszustand" }

func (d *frischesGeraet) bediene(conn net.Conn) {
	defer conn.Close()
	transport := geraeteTransport{conn: conn}
	for {
		payload, err := classes.JoinFrames(transport, time.Minute)
		if err != nil {
			return
		}
		out, err := classes.CborEncode(d.antworte(payload))
		if err != nil {
			return
		}
		reports, err := classes.SplitFrames(out)
		if err != nil {
			return
		}
		for _, report := range reports {
			if writeErr := transport.WriteReport(report); writeErr != nil {
				return
			}
		}
	}
}

// antworte beantwortet eine Anfrage. Unbekannte Befehle bekommen
// OSK_ERR_UNSUPPORTED, so wie ein echtes Geraet.
func (d *frischesGeraet) antworte(payload []byte) classes.CborMap {
	value, err := classes.CborDecode(payload)
	if err != nil {
		return antwort(0, 9, nil) // OSK_ERR_FORMAT
	}
	request, ok := value.(classes.CborMap)
	if !ok {
		return antwort(0, 9, nil)
	}
	seq, _ := classes.CborUint(request, "s")
	cmd, _ := classes.CborUint(request, "c")
	data, _ := classes.CborSub(request, "d")
	if data == nil {
		data = classes.CborMap{}
	}

	d.mu.Lock()
	defer d.mu.Unlock()

	switch cmd {
	case classes.CmdHello:
		out := classes.CborMap{
			"fw":       "0.0.0.0-werkszustand",
			"serial":   "SIM-WERK-1",
			"proto":    uint64(classes.ProtocolVersion),
			"profiles": uint64(0),
			"unlocked": d.unlocked,
			"fresh":    d.mode == "",
			"tr":       []any{"usb-hid", "sim"},
		}
		if d.mode != "" {
			out["mode"] = d.mode
		}
		return antwort(seq, 0, out)
	case classes.CmdSetup:
		mode, _ := classes.CborText(data, "mode")
		pin, _ := classes.CborBytes(data, "pin")
		if !classes.ValidMode(mode) || len(pin) < classes.MinPinLen {
			return antwort(seq, 1, nil) // OSK_ERR_ARG
		}
		if d.mode != "" {
			// Die Betriebsart ist endgueltig - das Geraet entscheidet das,
			// nicht das Programm.
			return antwort(seq, 4, nil) // OSK_ERR_DENIED
		}
		d.mode = mode
		d.pin = string(pin)
		d.setupCount++
		return antwort(seq, 0, classes.CborMap{})
	case classes.CmdUnlock:
		pin, _ := classes.CborBytes(data, "pin")
		if d.mode == "" || string(pin) != d.pin {
			return antwort(seq, 4, nil)
		}
		d.unlocked = true
		return antwort(seq, 0, classes.CborMap{"unlocked": true})
	case classes.CmdLock:
		d.unlocked = false
		return antwort(seq, 0, classes.CborMap{})
	}

	if !d.unlocked {
		return antwort(seq, 5, nil) // OSK_ERR_LOCKED
	}
	switch cmd {
	case classes.CmdList:
		return antwort(seq, 0, classes.CborMap{"items": []any{}})
	case classes.CmdBackup:
		// Ein frisch eingerichtetes Geraet hat noch keine Eintraege, aber
		// die Sicherung gibt es trotzdem: sie traegt das Hauptgeheimnis,
		// und genau darum wird sie im Assistenten vor dem ersten Eintrag
		// gespeichert.
		return antwort(seq, 0, classes.CborMap{
			"blob": []byte("oskbak-werkszustand"),
		})
	case classes.CmdLog:
		return antwort(seq, 0, classes.CborMap{"log": []any{}})
	}
	return antwort(seq, 10, nil) // OSK_ERR_UNSUPPORTED
}

func antwort(seq, result uint64, data classes.CborMap) classes.CborMap {
	if data == nil {
		data = classes.CborMap{}
	}
	return classes.CborMap{
		"v": uint64(classes.ProtocolVersion),
		"s": seq,
		"r": result,
		"d": data,
	}
}

// Betriebsart und Werkszustand stehen nach hello fest.
func (d *frischesGeraet) betriebsart() string {
	d.mu.Lock()
	defer d.mu.Unlock()
	return d.mode
}

func (d *frischesGeraet) zahlDerEinrichtungen() int {
	d.mu.Lock()
	defer d.mu.Unlock()
	return d.setupCount
}

// Ein Geraet im Werkszustand meldet sich als frisch und ohne Betriebsart.
func TestWerkszustandMeldetSichFrisch(t *testing.T) {
	address, _ := starteFrischesGeraet(t)
	app := newApp(t, address)
	client, err := app.Client()
	if err != nil {
		t.Fatalf("verbinden: %v", app.Message(err))
	}
	info, err := client.Hello()
	if err != nil {
		t.Fatalf("hello: %v", app.Message(err))
	}
	if !info.Fresh || info.Mode != "" {
		t.Errorf("fresh=%v mode=%q, erwartet frisch und ohne Betriebsart",
			info.Fresh, info.Mode)
	}
}
