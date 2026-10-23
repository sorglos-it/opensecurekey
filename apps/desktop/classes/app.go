// Der gemeinsame Kern: Oberflaeche und Kommandozeile benutzen beide diesen
// Teil. Er haelt Einstellungen, Texte und die Verbindung zum Geraet.
package classes

import (
	"io/fs"
	"sync"
)

// App ist der Kern. Die Verbindung wird erst aufgebaut, wenn sie gebraucht
// wird, und wieder geschlossen, sobald der Anwender sperrt.
type App struct {
	Files fs.FS
	Texts Texts
	Lang  string

	mu     sync.Mutex
	cfg    Config
	client *Client
	path   string
}

// NewApp baut den Kern. files ist das eingebettete Dateisystem mit lang/,
// views/, templates/ und assets/.
func NewApp(files fs.FS, cfg Config) *App {
	lang := cfg.Lang
	if lang == "" {
		lang = SystemLang(Languages(files))
	}
	return &App{
		Files: files,
		Texts: LoadTexts(files, lang),
		Lang:  lang,
		cfg:   cfg,
	}
}

// Config gibt die Einstellungen zurueck.
func (a *App) Config() Config {
	a.mu.Lock()
	defer a.mu.Unlock()
	return a.cfg
}

// SetConfig uebernimmt neue Einstellungen und schliesst die Verbindung, weil
// sie zu einem anderen Geraet gehoeren koennte.
func (a *App) SetConfig(cfg Config) {
	a.mu.Lock()
	old := a.client
	a.client = nil
	a.path = ""
	a.cfg = cfg
	if cfg.Lang != "" && cfg.Lang != a.Lang {
		a.Lang = cfg.Lang
		a.Texts = LoadTexts(a.Files, cfg.Lang)
	}
	a.mu.Unlock()
	if old != nil {
		old.Close()
	}
}

// Devices nennt die erreichbaren Geraete samt Attrappe.
func (a *App) Devices() []DeviceRef {
	return FindDevices(a.Config().SimAddress)
}

// Use verbindet mit einem bestimmten Geraet.
func (a *App) Use(path string) error {
	cfg := a.Config()
	client, err := Connect(path, cfg.AppName)
	if err != nil {
		return err
	}
	a.mu.Lock()
	old := a.client
	a.client = client
	a.path = path
	a.mu.Unlock()
	if old != nil {
		old.Close()
	}
	return nil
}

// Client gibt die Verbindung und baut sie beim ersten Mal auf.
func (a *App) Client() (*Client, error) {
	a.mu.Lock()
	if a.client != nil {
		client := a.client
		a.mu.Unlock()
		return client, nil
	}
	cfg := a.cfg
	a.mu.Unlock()

	client, err := ConnectFirst(cfg.SimAddress, cfg.AppName)
	if err != nil {
		return nil, err
	}
	a.mu.Lock()
	if a.client == nil {
		a.client = client
		a.path = client.Describe()
		a.mu.Unlock()
		return client, nil
	}
	// In der Zwischenzeit hat ein anderer Aufruf verbunden.
	existing := a.client
	a.mu.Unlock()
	client.Close()
	return existing, nil
}

// CurrentPath nennt das verbundene Geraet, leer wenn keines verbunden ist.
func (a *App) CurrentPath() string {
	a.mu.Lock()
	defer a.mu.Unlock()
	return a.path
}

// Disconnect schliesst die Verbindung. Nach dem Sperren bleibt nichts offen.
func (a *App) Disconnect() {
	a.mu.Lock()
	client := a.client
	a.client = nil
	a.path = ""
	a.mu.Unlock()
	if client != nil {
		client.Close()
	}
}

// Text ist die Kurzform fuer einen uebersetzten Text.
func (a *App) Text(key string) string { return a.Texts.Get(key) }

// Message macht aus einem Fehler den Text fuer den Anwender.
func (a *App) Message(err error) string { return a.Texts.Message(err) }
