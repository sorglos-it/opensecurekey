// Wo der Agent lauscht - der gemeinsame Teil.
//
// Unter Linux und macOS ist das eine Datei im Dateisystem (ein Socket), unter
// Windows eine benannte Pipe. Beides erreicht nur, wer am selben Rechner als
// derselbe Benutzer angemeldet ist; ueber das Netz ist der Agent nie zu
// erreichen. Das ist keine Bequemlichkeit, sondern die Grenze: wer den Agenten
// erreicht, kann in seinem Namen unterschreiben lassen.
package classes

import (
	"errors"
	"net"
	"strings"
	"sync"
)

// AgentListener nimmt Verbindungen an und nennt dabei, welches Programm
// fragt - soweit das System das sagt.
type AgentListener interface {
	// Accept wartet auf die naechste Verbindung. Der zweite Rueckgabewert
	// ist der Name des fragenden Programms, wie das Betriebssystem ihn
	// nennt, oder leer.
	Accept() (net.Conn, string, error)
	Close() error
	// Address ist die Stelle, die in SSH_AUTH_SOCK oder IdentityAgent
	// gehoert.
	Address() string
}

// RunAgent bedient Verbindungen, bis der Lauscher geschlossen wird. Jede
// Verbindung laeuft fuer sich; eine abgebrochene Verbindung darf den Agenten
// nicht mitnehmen.
func RunAgent(agent *Agent, listener AgentListener) error {
	var wait sync.WaitGroup
	defer wait.Wait()
	for {
		conn, peer, err := listener.Accept()
		if err != nil {
			return err
		}
		wait.Add(1)
		go func(conn net.Conn, peer string) {
			defer wait.Done()
			agent.Serve(conn, peer)
		}(conn, peer)
	}
}

// AgentEnvName ist die Umgebungsvariable, mit der ein Platz von Hand
// vorgegeben werden kann.
const AgentEnvName = "OSK_AGENT"

// listenerClosed sagt, ob ein Fehler des Lauschers das GEWOLLTE Ende ist.
//
// Der Unterschied ist wichtiger, als er aussieht: wer ihn nicht macht,
// beendet das Programm bei jedem Fehler mit Ausstiegscode 0 - und ein
// Programm, das die Startzeile ausgibt und dann wortlos endet, sieht aus,
// als laufe es. Gemessen am 24.10.2026 an einem falsch geschriebenen
// Pipe-Namen.
func listenerClosed(err error) bool {
	if err == nil {
		return true
	}
	if errors.Is(err, net.ErrClosed) {
		return true
	}
	if problem, ok := err.(*Problem); ok {
		return problem.Key == "error.agent.closed"
	}
	return strings.Contains(err.Error(), "use of closed")
}

// peerName macht aus dem Pfad eines Programms den Namen, der ins
// Geraeteprotokoll kommt: nur der Dateiname, nie der ganze Pfad. Ein Pfad
// verraet Ordnernamen, und im Protokoll des Geraets ist dafuer kein Platz.
func peerName(path string) string {
	if path == "" {
		return ""
	}
	cut := 0
	for i := len(path) - 1; i >= 0; i-- {
		if path[i] == '/' || path[i] == '\\' {
			cut = i + 1
			break
		}
	}
	return CleanAskedBy(path[cut:])
}
