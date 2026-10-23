package classes

import (
	"encoding/json"
	"os"
	"path/filepath"
	"regexp"
	"sort"
	"strings"
	"testing"
)

// appFS ist das Teilprogramm selbst - die Tests liegen eine Ebene darunter.
func appFS() string { return ".." }

func loadFile(t *testing.T, code string) Texts {
	t.Helper()
	data, err := os.ReadFile(filepath.Join(appFS(), "lang", code+".json"))
	if err != nil {
		t.Fatalf("lang/%s.json: %v", code, err)
	}
	var texts Texts
	if err := json.Unmarshal(data, &texts); err != nil {
		t.Fatalf("lang/%s.json: %v", code, err)
	}
	return texts
}

// Die fuenf Sprachen muessen vorhanden sein - Hausregel #ml.
func TestFiveLanguages(t *testing.T) {
	found := Languages(os.DirFS(appFS()))
	want := []string{"de", "en", "es", "fr", "it"}
	sort.Strings(found)
	if strings.Join(found, ",") != strings.Join(want, ",") {
		t.Fatalf("Sprachen: %v, erwartet %v", found, want)
	}
}

// Alle Dateien haben dieselben Schluessel; fehlende sind ein Fehler.
func TestSameKeys(t *testing.T) {
	master := loadFile(t, FallbackLang)
	for _, code := range Languages(os.DirFS(appFS())) {
		texts := loadFile(t, code)
		for key := range master {
			if value, ok := texts[key]; !ok || strings.TrimSpace(value) == "" {
				t.Errorf("%s: Schluessel fehlt oder ist leer: %s", code, key)
			}
		}
		for key := range texts {
			if _, ok := master[key]; !ok {
				t.Errorf("%s: Schluessel kennt %s nicht: %s",
					code, FallbackLang, key)
			}
		}
	}
}

// Platzhalter muessen in jeder Sprache gleich oft vorkommen - sonst bricht
// die Meldung oder verliert eine Angabe.
func TestSamePlaceholders(t *testing.T) {
	master := loadFile(t, FallbackLang)
	placeholder := regexp.MustCompile(`%[a-zA-Z]`)
	for _, code := range Languages(os.DirFS(appFS())) {
		texts := loadFile(t, code)
		for key, value := range master {
			want := placeholder.FindAllString(value, -1)
			got := placeholder.FindAllString(texts[key], -1)
			if len(want) != len(got) {
				t.Errorf("%s: %s hat %d Platzhalter, %s hat %d",
					code, key, len(got), FallbackLang, len(want))
			}
		}
	}
}

// Kein sichtbarer Text im Code: jeder Schluessel, den Seiten oder Javascript
// benutzen, muss in den Sprachdateien stehen.
func TestKeysUsedExist(t *testing.T) {
	master := loadFile(t, FallbackLang)
	pattern := regexp.MustCompile(
		`(?:Texts\.Get |\.Text\(|\bt\(|Fail\()"([a-z][a-z0-9.]+)"`)
	roots := []string{"templates", "views", "assets", "classes", "."}
	count := 0
	for _, root := range roots {
		dir := filepath.Join(appFS(), root)
		entries, err := os.ReadDir(dir)
		if err != nil {
			t.Fatalf("%s: %v", dir, err)
		}
		for _, entry := range entries {
			if entry.IsDir() {
				continue
			}
			data, readErr := os.ReadFile(filepath.Join(dir, entry.Name()))
			if readErr != nil {
				t.Fatalf("%s: %v", entry.Name(), readErr)
			}
			for _, match := range pattern.FindAllStringSubmatch(string(data), -1) {
				count++
				if _, ok := master[match[1]]; !ok {
					t.Errorf("%s benutzt den Schluessel %s, den es nicht gibt",
						entry.Name(), match[1])
				}
			}
		}
	}
	// Findet die Suche nichts, ist der Test selbst kaputt.
	if count < 100 {
		t.Fatalf("nur %d Schluessel im Code gefunden", count)
	}
}

// Fehlt ein Schluessel in der gewaehlten Sprache, springt Englisch ein.
func TestFallbackToEnglish(t *testing.T) {
	texts := LoadTexts(os.DirFS(appFS()), "de")
	if texts.Get("app.name") == "" {
		t.Fatal("Texte nicht geladen")
	}
	if texts.Get("nicht.vorhanden") != "nicht.vorhanden" {
		t.Error("unbekannter Schluessel darf nicht leer antworten")
	}
	english := loadFile(t, "en")
	german := loadFile(t, "de")
	for key, value := range english {
		if german[key] == "" && texts.Get(key) != value {
			t.Errorf("%s fiel nicht auf Englisch zurueck", key)
		}
	}
}

// Eine Sprache darf nie ein Pfad sein.
func TestLangCodeIsChecked(t *testing.T) {
	for _, bad := range []string{"", "x", "../en", "EN", "en-GB"} {
		if validLangCode(bad) {
			t.Errorf("%q wurde als Sprache angenommen", bad)
		}
	}
}
