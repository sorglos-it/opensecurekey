//go:build !windows

package classes

import (
	"os"
	"path/filepath"
	"strconv"
)

// defaultSocketPath nennt den Platz der lokalen Schnittstelle unter Linux
// und macOS. Ein ANDERER Platz als der des ssh-Agenten, mit denselben
// Rechten: wer ssh erreichen darf, soll nicht nebenbei den ganzen Tresor
// erreichen duerfen.
func defaultSocketPath() string {
	if runtime := os.Getenv("XDG_RUNTIME_DIR"); runtime != "" {
		return filepath.Join(runtime, "osk", "local.sock")
	}
	return filepath.Join(os.TempDir(), "osk-"+strconv.Itoa(os.Getuid()), "local.sock")
}
