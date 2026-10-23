package classes

import "fmt"

// Problem ist ein Fehler, dessen Text aus der Sprachdatei kommt.
// Key zeigt auf den Schluessel, Args fuellen die Platzhalter.
type Problem struct {
	Key  string
	Args []any
}

// Fail baut ein Problem mit Schluessel und Platzhaltern.
func Fail(key string, args ...any) *Problem {
	return &Problem{Key: key, Args: args}
}

func (p *Problem) Error() string {
	if len(p.Args) == 0 {
		return p.Key
	}
	return fmt.Sprintf("%s %v", p.Key, p.Args)
}
