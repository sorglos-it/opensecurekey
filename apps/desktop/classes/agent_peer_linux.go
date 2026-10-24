//go:build linux

// Wer fragt da? Unter Linux sagt es der Kern.
//
// SO_PEERCRED nennt Prozessnummer, Benutzer und Gruppe der Gegenseite - vom
// Kern, nicht vom Programm. Aus der Prozessnummer wird der Programmname
// gelesen. Das ist mehr wert als eine Behauptung des Programms, aber es ist
// immer noch nur ein NAME: ein Programm kann sich umbenennen, und eine
// Prozessnummer kann zwischen Abfrage und Anzeige weiterverwendet werden.
// Die Oberflaeche sagt darum weiter, dass der Name eine Behauptung ist.
package classes

import (
	"net"
	"os"
	"strconv"
	"strings"
)

// peerOfUnixConn nennt das Programm am anderen Ende, oder leer.
func peerOfUnixConn(conn *net.UnixConn) string {
	raw, err := conn.SyscallConn()
	if err != nil {
		return ""
	}
	var pid int
	controlErr := raw.Control(func(fd uintptr) {
		cred, credErr := getPeerCred(fd)
		if credErr == nil {
			pid = int(cred.Pid)
		}
	})
	if controlErr != nil || pid <= 0 {
		return ""
	}
	return peerName(processName(pid))
}

// processName liest den Programmnamen zu einer Prozessnummer. Zuerst der
// Pfad, weil er den echten Dateinamen nennt; faellt er weg, der Kurzname aus
// comm (auf 15 Zeichen beschnitten, aber besser als nichts).
func processName(pid int) string {
	folder := "/proc/" + strconv.Itoa(pid)
	if link, err := os.Readlink(folder + "/exe"); err == nil && link != "" {
		return link
	}
	if data, err := os.ReadFile(folder + "/comm"); err == nil {
		return strings.TrimSpace(string(data))
	}
	return ""
}
