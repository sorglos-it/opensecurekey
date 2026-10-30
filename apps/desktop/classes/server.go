// Die Oberflaeche ist eine Webseite, die nur auf diesem Rechner erreichbar
// ist: gelauscht wird allein auf 127.0.0.1, der Port wird ab 8101 gesucht.
// Kein Token - stattdessen wird jede Anfrage auf Herkunft und Ziel geprueft,
// damit keine fremde Seite im Browser mitreden kann.
package classes

import (
	"html/template"
	"io/fs"
	"net"
	"net/http"
	"strconv"
	"strings"
	"time"
)

// Grenzen des Servers.
const (
	maxPortTries   = 100
	maxBodyBytes   = 256 * 1024
	maxUploadBytes = 8 * 1024 * 1024
	serverTimeout  = 120 * time.Second
)

// Server ist die Oberflaeche.
type Server struct {
	app       *App
	listener  net.Listener
	port      int
	templates *template.Template
	pages     []page
}

// page ist eine Seite der Oberflaeche: Adresse, Vorlage, Titelschluessel.
type page struct {
	Path  string
	View  string
	Title string
}

var uiPages = []page{
	{"/", "overview", "page.overview"},
	{"/item", "item", "page.item"},
	{"/certs", "certs", "page.certs"},
	{"/devices", "devices", "page.devices"},
	{"/backup", "backup", "page.backup"},
	{"/exchange", "exchange", "page.exchange"},
	{"/firmware", "firmware", "page.firmware"},
	{"/settings", "settings", "page.settings"},
}

// setupPages sind die Seiten des Assistenten. Sie stehen nicht in der
// Navigation: der Assistent ist ein Ablauf, kein Ort, den man nebenbei
// besucht. Der Notfallzettel bleibt danach erreichbar - wer ihn verliert,
// soll ihn neu drucken koennen.
var setupPages = []page{
	{"/setup", "setup", "page.setup"},
	{"/sheet", "sheet", "page.sheet"},
}

// NewServer bindet den ersten freien Port ab dem eingestellten.
func NewServer(app *App) (*Server, error) {
	templates, err := template.New("osk").ParseFS(app.Files,
		"templates/*.html", "views/*.html")
	if err != nil {
		return nil, Fail("error.server.templates")
	}
	start := app.Config().Port
	if start < 1024 {
		start = DefaultPort
	}
	for port := start; port < start+maxPortTries; port++ {
		listener, listenErr := net.Listen("tcp", "127.0.0.1:"+strconv.Itoa(port))
		if listenErr != nil {
			continue
		}
		return &Server{
			app:       app,
			listener:  listener,
			port:      port,
			templates: templates,
			pages:     uiPages,
		}, nil
	}
	return nil, Fail("error.server.port", strconv.Itoa(start))
}

// Port nennt den gebundenen Port.
func (s *Server) Port() int { return s.port }

// Address ist die Adresse, die der Anwender im Browser oeffnet.
func (s *Server) Address() string {
	return "http://127.0.0.1:" + strconv.Itoa(s.port) + "/"
}

// Serve bedient Anfragen, bis der Lauscher geschlossen wird.
func (s *Server) Serve() error {
	mux := http.NewServeMux()
	for _, p := range append(append([]page{}, s.pages...), setupPages...) {
		current := p
		mux.HandleFunc(current.Path, func(w http.ResponseWriter, r *http.Request) {
			if r.URL.Path != current.Path {
				s.notFound(w)
				return
			}
			// Der Assistent laesst sich nicht ueberspringen: solange das
			// Geraet im Werkszustand ist, fuehrt jede andere Seite dorthin
			// zurueck. Ein Lesezeichen auf die Uebersicht hilft nicht
			// weiter, und das ist Absicht - die erste Frage muss gestellt
			// werden, bevor ein Geheimnis entsteht.
			if current.Path != "/setup" && s.setupPending() {
				http.Redirect(w, r, "/setup", http.StatusSeeOther)
				return
			}
			s.renderPage(w, current)
		})
	}
	assets, err := fs.Sub(s.app.Files, "assets")
	if err != nil {
		return Fail("error.server.templates")
	}
	mux.Handle("/assets/", http.StripPrefix("/assets/",
		http.FileServer(http.FS(assets))))
	s.routeAPI(mux)

	server := &http.Server{
		Handler:           s.guard(mux),
		ReadHeaderTimeout: 10 * time.Second,
		ReadTimeout:       serverTimeout,
		WriteTimeout:      serverTimeout,
	}
	return server.Serve(s.listener)
}

// Close gibt den Port frei.
func (s *Server) Close() error { return s.listener.Close() }

// guard prueft Ziel und Herkunft jeder Anfrage. Ohne diese Pruefung koennte
// eine beliebige Webseite im selben Browser unseren Server ansprechen.
func (s *Server) guard(next http.Handler) http.Handler {
	allowed := map[string]bool{
		"127.0.0.1:" + strconv.Itoa(s.port): true,
		"localhost:" + strconv.Itoa(s.port): true,
		"[::1]:" + strconv.Itoa(s.port):     true,
	}
	origins := map[string]bool{}
	for host := range allowed {
		origins["http://"+host] = true
	}
	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		// Ziel: schuetzt gegen Namen, die auf 127.0.0.1 zeigen.
		if !allowed[strings.ToLower(r.Host)] {
			http.Error(w, "forbidden", http.StatusForbidden)
			return
		}
		origin := r.Header.Get("Origin")
		switch {
		case origin != "" && !origins[strings.ToLower(origin)]:
			http.Error(w, "forbidden", http.StatusForbidden)
			return
		case origin == "" && r.Method != http.MethodGet && r.Method != http.MethodHead:
			// Aendernde Anfragen ohne Herkunft kommen nicht aus unserer Seite.
			http.Error(w, "forbidden", http.StatusForbidden)
			return
		}
		if site := r.Header.Get("Sec-Fetch-Site"); site != "" &&
			site != "same-origin" && site != "none" {
			http.Error(w, "forbidden", http.StatusForbidden)
			return
		}
		header := w.Header()
		header.Set("Content-Security-Policy",
			"default-src 'none'; style-src 'self'; script-src 'self'; "+
				"img-src 'self' data:; connect-src 'self'; form-action 'none'; "+
				"base-uri 'none'; frame-ancestors 'none'")
		header.Set("X-Content-Type-Options", "nosniff")
		header.Set("Referrer-Policy", "no-referrer")
		header.Set("Cache-Control", "no-store")
		next.ServeHTTP(w, r)
	})
}

// setupPending sagt, ob der Assistent noch laufen muss. Ohne Geraet ist er
// es nicht: dann gibt es nichts einzurichten, und die Oberflaeche bleibt
// bedienbar. Antwortet das Geraet nicht oder sagt es nichts zum
// Werkszustand, gilt es als eingerichtet.
func (s *Server) setupPending() bool {
	client, err := s.app.Client()
	if err != nil {
		return false
	}
	info, err := client.Hello()
	if err != nil {
		return false
	}
	// Profil 0 ist das, mit dem die Oberflaeche arbeitet.
	return info.Factory && info.ModeOf(0) == ModeNoneSet
}

// pageData ist das, was eine Seite zu sehen bekommt.
type pageData struct {
	Title     string
	Active    string
	Lang      string
	Version   string
	Texts     Texts
	Pages     []page
	ItemTypes []ItemType
	Languages []string
	Config    Config
	WipeMs    int
	// Info ist der Zustand des Geraets, soweit hello ihn nennt. Der
	// Notfallzettel braucht Seriennummer und Betriebsart gedruckt, und ein
	// Zettel mit leeren Feldern waere wertlos.
	Info DeviceInfo
	// ModeOnly ist wahr, wenn das Profil als "nur dieser Stick" laeuft.
	// Dann bringt die Sicherung nicht zurueck, was im Sicherheitschip
	// liegt, und der Zettel muss das sagen.
	ModeOnly bool
	// ModeName ist der Textschluessel der Betriebsart, leer bei einem
	// Profil ohne. So steht auch hier kein Text im Code.
	ModeName string
	// Today ist der Tag, der auf dem Zettel steht.
	Today string
}

func (s *Server) renderPage(w http.ResponseWriter, p page) {
	data := pageData{
		Title:     s.app.Text(p.Title),
		Active:    p.Path,
		Lang:      s.app.Lang,
		Version:   Version,
		Texts:     s.app.Texts,
		Pages:     s.pages,
		ItemTypes: ItemTypes(),
		Languages: Languages(s.app.Files),
		Config:    s.app.Config(),
		WipeMs:    ClipboardWipeMs,
		Today:     time.Now().UTC().Format("2006-01-02"),
	}
	if client, err := s.app.Client(); err == nil {
		if info, infoErr := client.Hello(); infoErr == nil {
			mode := info.ModeOf(0)
			data.Info = info
			data.ModeOnly = mode == ModeOnly
			data.ModeName = ModeKey(mode)
		}
	}
	w.Header().Set("Content-Type", "text/html; charset=utf-8")
	if err := s.templates.ExecuteTemplate(w, p.View+".html", data); err != nil {
		http.Error(w, s.app.Text("error.internal"), http.StatusInternalServerError)
	}
}

func (s *Server) notFound(w http.ResponseWriter) {
	http.Error(w, s.app.Text("error.notfound"), http.StatusNotFound)
}

// T gibt den Text zum Schluessel - in den Vorlagen als {{T .Texts "key"}}.
func T(texts Texts, key string) string { return texts.Get(key) }
