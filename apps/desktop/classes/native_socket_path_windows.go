//go:build windows

package classes

// defaultSocketPath nennt den Platz der lokalen Schnittstelle unter Windows.
// Eine eigene Pipe, nicht die des ssh-Agenten - wer ssh erreichen darf, soll
// nicht nebenbei den ganzen Tresor erreichen duerfen. Die Rechteliste ist
// dieselbe: nur der angemeldete Benutzer (siehe agent_listen_windows.go).
func defaultSocketPath() string {
	return `\\.\pipe\osk-local`
}
