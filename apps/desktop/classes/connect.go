// Geraete finden und verbinden. Zwei Arten: ein echtes HID-Geraet oder die
// Attrappe aus tests/, die dasselbe Protokoll ueber eine Verbindung auf
// 127.0.0.1 spricht. Oberflaeche und Kommandozeile sehen keinen Unterschied.
package classes

import (
	"io"
	"net"
	"strings"
	"sync"
	"time"
)

// SimPrefix kennzeichnet die Attrappe: sim:127.0.0.1:8201
const SimPrefix = "sim:"

// DeviceRef ist ein gefundenes Geraet, wie es die Oberflaeche anzeigt.
type DeviceRef struct {
	Path      string `json:"path"`
	Label     string `json:"label"`
	Simulated bool   `json:"simulated"`
}

// FindDevices sucht echte Geraete und nimmt die Attrappe dazu, wenn eine
// Adresse eingestellt ist. Findet die HID-Schicht nichts, ist das kein
// Fehler - dann gibt es eben kein Geraet.
func FindDevices(simAddress string) []DeviceRef {
	var out []DeviceRef
	if found, err := HidFind(); err == nil {
		for _, d := range found {
			out = append(out, DeviceRef{Path: d.Path, Label: d.Label()})
		}
	}
	if simAddress != "" {
		out = append(out, DeviceRef{
			Path:      SimPrefix + simAddress,
			Label:     simAddress,
			Simulated: true,
		})
	}
	return out
}

// Connect oeffnet das Geraet unter path. askedBy ist der Name, den das Geraet
// protokolliert - eine Behauptung dieses Programms, kein Beweis.
func Connect(path, askedBy string) (*Client, error) {
	if path == "" {
		return nil, Fail("error.device.none")
	}
	if strings.HasPrefix(path, SimPrefix) {
		transport, err := SimDial(strings.TrimPrefix(path, SimPrefix))
		if err != nil {
			return nil, err
		}
		return NewClient(transport, askedBy), nil
	}
	transport, err := HidOpen(path)
	if err != nil {
		return nil, err
	}
	return NewClient(transport, askedBy), nil
}

// ConnectFirst nimmt das erste gefundene Geraet - fuer die Kommandozeile,
// damit kein Pfad getippt werden muss.
func ConnectFirst(simAddress, askedBy string) (*Client, error) {
	devices := FindDevices(simAddress)
	if len(devices) == 0 {
		return nil, Fail("error.device.none")
	}
	return Connect(devices[0].Path, askedBy)
}

// simTransport spricht das Protokoll ueber eine Verbindung auf dem eigenen
// Rechner. Nur fuer die Attrappe, nie fuer echte Geheimnisse.
type simTransport struct {
	conn  net.Conn
	label string
	mu    sync.Mutex
}

// SimDial verbindet mit der Attrappe. Erlaubt ist allein die Rueckschleife -
// eine Attrappe auf einem anderen Rechner waere ein offener Kanal.
func SimDial(address string) (Transport, error) {
	host, port, err := net.SplitHostPort(address)
	if err != nil || !isLoopbackName(host) || port == "" {
		return nil, Fail("error.sim.address", address)
	}
	conn, err := net.DialTimeout("tcp", net.JoinHostPort(host, port), shortTimeout)
	if err != nil {
		return nil, Fail("error.sim.connect", address)
	}
	return &simTransport{conn: conn, label: address}, nil
}

func isLoopbackName(host string) bool {
	if host == "localhost" {
		return true
	}
	ip := net.ParseIP(host)
	return ip != nil && ip.IsLoopback()
}

func (s *simTransport) Describe() string { return s.label }

func (s *simTransport) WriteReport(report []byte) error {
	if len(report) != ReportSize {
		return Fail("error.frame.size")
	}
	s.mu.Lock()
	defer s.mu.Unlock()
	if err := s.conn.SetWriteDeadline(time.Now().Add(shortTimeout)); err != nil {
		return Fail("error.hid.write", err.Error())
	}
	if _, err := s.conn.Write(report); err != nil {
		return Fail("error.hid.write", err.Error())
	}
	return nil
}

func (s *simTransport) ReadReport(timeout time.Duration) ([]byte, error) {
	s.mu.Lock()
	defer s.mu.Unlock()
	if err := s.conn.SetReadDeadline(time.Now().Add(timeout)); err != nil {
		return nil, Fail("error.hid.read", err.Error())
	}
	report := make([]byte, ReportSize)
	if _, err := io.ReadFull(s.conn, report); err != nil {
		if netErr, ok := err.(net.Error); ok && netErr.Timeout() {
			return nil, Fail("error.hid.timeout")
		}
		return nil, Fail("error.hid.read", err.Error())
	}
	return report, nil
}

func (s *simTransport) Close() error {
	s.mu.Lock()
	defer s.mu.Unlock()
	return s.conn.Close()
}
