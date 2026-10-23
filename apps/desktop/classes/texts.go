package classes

import (
	"encoding/json"
	"errors"
	"fmt"
	"io/fs"
	"sort"
	"strings"
)

// Texts sind die sichtbaren Texte in einer Sprache.
type Texts map[string]string

// FallbackLang ist die Sprache, die einspringt, wenn ein Schluessel fehlt.
const FallbackLang = "en"

// LoadTexts laedt lang/<code>.json und legt Englisch darunter, damit kein
// Schluessel nackt oder leer bleibt.
func LoadTexts(fsys fs.FS, code string) Texts {
	out := Texts{}
	for _, c := range []string{FallbackLang, code} {
		if c == "" || !validLangCode(c) {
			continue
		}
		one, err := readLang(fsys, c)
		if err != nil {
			continue
		}
		for k, v := range one {
			out[k] = v
		}
	}
	return out
}

// Languages nennt die Sprachen, die in lang/ liegen - ohne Liste im Code.
func Languages(fsys fs.FS) []string {
	entries, err := fs.ReadDir(fsys, "lang")
	if err != nil {
		return []string{FallbackLang}
	}
	var out []string
	for _, e := range entries {
		name := e.Name()
		if e.IsDir() || !strings.HasSuffix(name, ".json") {
			continue
		}
		code := strings.TrimSuffix(name, ".json")
		if validLangCode(code) {
			out = append(out, code)
		}
	}
	sort.Strings(out)
	if len(out) == 0 {
		return []string{FallbackLang}
	}
	return out
}

// Get liefert den Text zum Schluessel, ersatzweise den Schluessel selbst.
func (t Texts) Get(key string) string {
	if s, ok := t[key]; ok && s != "" {
		return s
	}
	return key
}

// Message macht aus einem Fehler den Text fuer den Anwender.
func (t Texts) Message(err error) string {
	if err == nil {
		return ""
	}
	var p *Problem
	if !errors.As(err, &p) {
		return t.Get("error.internal")
	}
	format, ok := t[p.Key]
	if !ok {
		return t.Get("error.internal")
	}
	if len(p.Args) == 0 {
		return format
	}
	return fmt.Sprintf(format, p.Args...)
}

func readLang(fsys fs.FS, code string) (Texts, error) {
	data, err := fs.ReadFile(fsys, "lang/"+code+".json")
	if err != nil {
		return nil, err
	}
	var t Texts
	if err := json.Unmarshal(data, &t); err != nil {
		return nil, err
	}
	return t, nil
}

// validLangCode laesst nur zwei Kleinbuchstaben zu - ein Sprachname darf
// niemals aus einem Pfad bestehen.
func validLangCode(code string) bool {
	if len(code) != 2 {
		return false
	}
	for i := 0; i < len(code); i++ {
		if code[i] < 'a' || code[i] > 'z' {
			return false
		}
	}
	return true
}
