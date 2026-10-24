//go:build !windows

// Der Agent unter Linux und macOS: ein Socket im Dateisystem.
//
// Der Schutz ist der des Dateisystems. Darum gilt beides zusammen: der Ordner
// darueber gehoert nur dem Benutzer (0700), und die Datei selbst auch (0600).
// Der Ordner allein genuegt nicht - auf manchen Systemen achtet der Kern bei
// Sockets nicht auf die Rechte der Datei, dann traegt der Ordner. Und die
// Datei allein genuegt nicht, weil ein fremder Ordner umbenannt werden kann.
package classes

import (
	"net"
	"os"
	"path/filepath"
	"strconv"
	"syscall"
)

// setUmask setzt die Maske fuer neu angelegte Dateien und gibt die alte
// zurueck. Sie gilt fuer den ganzen Prozess, darum wird sie nur um das
// Anlegen des Sockets herum veraendert und sofort wieder zurueckgesetzt.
func setUmask(mask int) int { return syscall.Umask(mask) }

// unixAgent ist der Lauscher auf einem Socket.
type unixAgent struct {
	listener *net.UnixListener
	path     string
}

// DefaultAgentAddress nennt den Platz, an dem der Agent lauscht.
//
// Bevorzugt wird der Laufzeitordner des Benutzers: der liegt auf einem
// Dateisystem im Arbeitsspeicher und wird beim Abmelden geraeumt. Gibt es
// keinen, wird ein eigener Ordner unter dem Platz fuer Temporaeres angelegt -
// mit der Benutzernummer im Namen, damit zwei Benutzer sich nicht in die
// Quere kommen.
func DefaultAgentAddress() string {
	if custom := os.Getenv(AgentEnvName); custom != "" {
		return custom
	}
	if runtime := os.Getenv("XDG_RUNTIME_DIR"); runtime != "" {
		return filepath.Join(runtime, "osk", "agent.sock")
	}
	return filepath.Join(os.TempDir(), "osk-"+strconv.Itoa(os.Getuid()), "agent.sock")
}

// ListenAgent oeffnet den Socket.
func ListenAgent(address string) (AgentListener, error) {
	if address == "" {
		address = DefaultAgentAddress()
	}
	folder := filepath.Dir(address)
	if err := os.MkdirAll(folder, 0o700); err != nil {
		return nil, Fail("error.agent.listen", address)
	}
	// Ein vorhandener Ordner kann anderen Rechte tragen - also nachziehen.
	if err := os.Chmod(folder, 0o700); err != nil {
		return nil, Fail("error.agent.listen", address)
	}
	// Eine Datei von einem abgestuerzten Lauf blockiert sonst das Binden.
	// Entfernt wird nur, was wirklich ein Socket ist: eine echte Datei an
	// dieser Stelle wird nicht angefasst.
	if info, err := os.Lstat(address); err == nil {
		if info.Mode()&os.ModeSocket == 0 {
			return nil, Fail("error.agent.inuse", address)
		}
		os.Remove(address)
	}
	// Die Maske sorgt dafuer, dass der Socket schon beim Anlegen nur dem
	// Benutzer gehoert. Ein Chmod danach waere ein offenes Fenster.
	old := setUmask(0o177)
	listener, err := net.ListenUnix("unix", &net.UnixAddr{Name: address, Net: "unix"})
	setUmask(old)
	if err != nil {
		return nil, Fail("error.agent.listen", address)
	}
	if err := os.Chmod(address, 0o600); err != nil {
		listener.Close()
		return nil, Fail("error.agent.listen", address)
	}
	return &unixAgent{listener: listener, path: address}, nil
}

func (u *unixAgent) Accept() (net.Conn, string, error) {
	conn, err := u.listener.AcceptUnix()
	if err != nil {
		return nil, "", err
	}
	return conn, peerOfUnixConn(conn), nil
}

func (u *unixAgent) Close() error {
	err := u.listener.Close()
	os.Remove(u.path)
	return err
}

func (u *unixAgent) Address() string { return u.path }
