// Einstellungen. Portabel: die Datei liegt neben dem Programm, nicht in der
// Registry und nicht im Benutzerordner. Fehlt sie, gelten die Vorgaben -
// das Programm laeuft auch von einem schreibgeschuetzten Datentraeger.
package classes

import (
	"encoding/json"
	"os"
	"path/filepath"
	"strings"
)

// Vorgaben.
const (
	DefaultPort     = 8101
	DefaultAppName  = "osk"
	configFileName  = "config_osk.json"
	configMaxBytes  = 64 * 1024
	ClipboardWipeMs = 20000 // Zwischenablage nach 20 s leeren
)

// Config sind die Einstellungen des Programms.
type Config struct {
	Lang       string `json:"lang"`       // leer: Sprache des Systems
	Port       int    `json:"port"`       // erster Port, der versucht wird
	SimAddress string `json:"simAddress"` // Attrappe aus tests/, leer: keine
	AppName    string `json:"appName"`    // Name, den das Geraet protokolliert
}

// DefaultConfig gibt die Vorgaben.
func DefaultConfig() Config {
	return Config{Port: DefaultPort, AppName: DefaultAppName}
}

// ConfigPath nennt die Datei neben dem Programm.
func ConfigPath() string {
	exe, err := os.Executable()
	if err != nil {
		return filepath.Join("config", configFileName)
	}
	return filepath.Join(filepath.Dir(exe), "config", configFileName)
}

// LoadConfig liest die Einstellungen. Ein Fehler ist nie toedlich: dann
// gelten die Vorgaben, und der Grund steht im Rueckgabewert.
func LoadConfig(path string) (Config, error) {
	cfg := DefaultConfig()
	info, err := os.Stat(path)
	if err != nil {
		return cfg, nil
	}
	if info.Size() > configMaxBytes {
		return cfg, Fail("error.config.size")
	}
	data, err := os.ReadFile(path)
	if err != nil {
		return cfg, Fail("error.config.read")
	}
	var fromFile Config
	if err := json.Unmarshal(data, &fromFile); err != nil {
		return cfg, Fail("error.config.syntax")
	}
	return cfg.merge(fromFile), nil
}

// merge nimmt nur brauchbare Werte aus der Datei.
func (c Config) merge(other Config) Config {
	out := c
	if validLangCode(other.Lang) {
		out.Lang = other.Lang
	}
	if other.Port >= 1024 && other.Port <= 65000 {
		out.Port = other.Port
	}
	if addr := strings.TrimSpace(other.SimAddress); addr != "" && len(addr) < 64 {
		out.SimAddress = addr
	}
	if name := strings.TrimSpace(other.AppName); name != "" && len(name) <= 64 && !hasControl(name) {
		out.AppName = name
	}
	return out
}

// Save schreibt die Einstellungen atomar: erst daneben, dann umbenennen.
func (c Config) Save(path string) error {
	data, err := json.MarshalIndent(c, "", "  ")
	if err != nil {
		return Fail("error.config.write")
	}
	if err := os.MkdirAll(filepath.Dir(path), 0o700); err != nil {
		return Fail("error.config.write")
	}
	temp := path + ".new"
	if err := os.WriteFile(temp, data, 0o600); err != nil {
		return Fail("error.config.write")
	}
	if err := os.Rename(temp, path); err != nil {
		os.Remove(temp)
		return Fail("error.config.write")
	}
	return nil
}

// SystemLang raet die Sprache aus den Umgebungsvariablen. Fuendig wird nur,
// wer eine der vorhandenen Sprachdateien trifft.
func SystemLang(available []string) string {
	for _, name := range []string{"OSK_LANG", "LC_ALL", "LC_MESSAGES", "LANG"} {
		value := strings.TrimSpace(os.Getenv(name))
		if len(value) < 2 {
			continue
		}
		code := strings.ToLower(value[:2])
		for _, have := range available {
			if have == code {
				return code
			}
		}
	}
	return FallbackLang
}
