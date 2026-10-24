// Die Schnittstelle der Oberflaeche. Antworten sind immer JSON; Fehler
// kommen als Text in der Sprache des Anwenders, nie als Fachmeldung.
// Geheimnisse stehen nur im Rumpf einer Antwort - niemals in einer Adresse,
// niemals in einem Protokoll.
package classes

import (
	"encoding/base64"
	"encoding/json"
	"io"
	"net/http"
	"strconv"
	"strings"
)

type apiReply struct {
	OK    bool   `json:"ok"`
	Error string `json:"error,omitempty"`
	Data  any    `json:"data,omitempty"`
}

func (s *Server) routeAPI(mux *http.ServeMux) {
	post := func(path string, fn func(*http.Request) (any, error)) {
		mux.HandleFunc(path, func(w http.ResponseWriter, r *http.Request) {
			if r.Method != http.MethodPost {
				s.reply(w, nil, Fail("error.method"))
				return
			}
			data, err := fn(r)
			s.reply(w, data, err)
		})
	}
	get := func(path string, fn func(*http.Request) (any, error)) {
		mux.HandleFunc(path, func(w http.ResponseWriter, r *http.Request) {
			if r.Method != http.MethodGet {
				s.reply(w, nil, Fail("error.method"))
				return
			}
			data, err := fn(r)
			s.reply(w, data, err)
		})
	}

	get("/api/texts", s.apiTexts)
	get("/api/types", s.apiTypes)
	get("/api/state", s.apiState)
	get("/api/devices", s.apiDevices)
	get("/api/items", s.apiItems)
	get("/api/log", s.apiLog)
	post("/api/use", s.apiUse)
	post("/api/setup", s.apiSetup)
	post("/api/unlock", s.apiUnlock)
	post("/api/lock", s.apiLock)
	post("/api/reveal", s.apiReveal)
	post("/api/save", s.apiSave)
	post("/api/delete", s.apiDelete)
	post("/api/pair", s.apiPair)
	post("/api/backup", s.apiBackup)
	post("/api/restore", s.apiRestore)
	post("/api/firmware", s.apiFirmware)
	post("/api/attest", s.apiAttest)
	post("/api/settings", s.apiSettings)
}

func (s *Server) reply(w http.ResponseWriter, data any, err error) {
	w.Header().Set("Content-Type", "application/json; charset=utf-8")
	out := apiReply{OK: err == nil, Data: data}
	status := http.StatusOK
	if err != nil {
		out.Error = s.app.Message(err)
		out.Data = nil
		status = http.StatusBadRequest
	}
	w.WriteHeader(status)
	json.NewEncoder(w).Encode(out)
}

// readJSON liest einen begrenzten Rumpf. Grosse Rumpfe gibt es nur fuer
// Firmware und Sicherung.
func readJSON(r *http.Request, target any, limit int64) error {
	body := http.MaxBytesReader(nil, r.Body, limit)
	data, err := io.ReadAll(body)
	if err != nil {
		return Fail("error.request.size")
	}
	if len(data) == 0 {
		return nil
	}
	if err := json.Unmarshal(data, target); err != nil {
		return Fail("error.request.format")
	}
	return nil
}

// apiTexts gibt die Texte der eingestellten Sprache. Die Oberflaeche holt
// sie beim Start - so steht kein Text im Javascript.
func (s *Server) apiTexts(r *http.Request) (any, error) {
	return s.app.Texts, nil
}

// apiTypes nennt die Eintragstypen mit ihrer Bezeichnung in der gewaehlten
// Sprache - damit in der Oberflaeche keine nackte Zahl steht.
func (s *Server) apiTypes(r *http.Request) (any, error) {
	out := make(map[string]string, len(ItemTypes()))
	for _, item := range ItemTypes() {
		out[strconv.Itoa(int(item))] = s.app.Text(item.Key())
	}
	return out, nil
}

func (s *Server) apiState(r *http.Request) (any, error) {
	type state struct {
		Device    string     `json:"device"`
		AppName   string     `json:"appName"`
		Version   string     `json:"version"`
		Connected bool       `json:"connected"`
		Info      DeviceInfo `json:"info"`
	}
	cfg := s.app.Config()
	out := state{AppName: cfg.AppName, Version: Version}
	client, err := s.app.Client()
	if err != nil {
		return out, nil // kein Geraet ist kein Fehler, nur ein Zustand
	}
	out.Connected = true
	out.Device = client.Describe()
	info, err := client.Hello()
	if err != nil {
		return out, err
	}
	out.Info = info
	return out, nil
}

func (s *Server) apiDevices(r *http.Request) (any, error) {
	return s.app.Devices(), nil
}

func (s *Server) apiUse(r *http.Request) (any, error) {
	var body struct{ Path string }
	if err := readJSON(r, &body, maxBodyBytes); err != nil {
		return nil, err
	}
	if err := s.app.Use(body.Path); err != nil {
		return nil, err
	}
	return map[string]string{"path": body.Path}, nil
}

// apiSetup richtet ein Geraet im Werkszustand ein. Hier wird die
// Bestaetigung noch einmal verlangt, nicht nur im Browser: wer die Anfrage
// von Hand stellt, soll die unumkehrbare Wahl nicht nebenbei treffen
// koennen.
func (s *Server) apiSetup(r *http.Request) (any, error) {
	var body struct {
		Mode    string `json:"mode"`
		Pin     string `json:"pin"`
		Confirm bool   `json:"confirm"`
	}
	if err := readJSON(r, &body, maxBodyBytes); err != nil {
		return nil, err
	}
	pin := []byte(body.Pin)
	body.Pin = ""
	if !ValidMode(body.Mode) {
		Wipe(pin)
		return nil, Fail("error.setup.mode")
	}
	if !body.Confirm {
		Wipe(pin)
		return nil, Fail("error.setup.confirm")
	}
	client, err := s.app.Client()
	if err != nil {
		Wipe(pin)
		return nil, err
	}
	info, err := client.Hello()
	if err != nil {
		Wipe(pin)
		return nil, err
	}
	// Ein Geraet, das schon eingerichtet ist, wird nicht noch einmal
	// eingerichtet - die Betriebsart ist endgueltig.
	if !info.Fresh || info.Mode != "" {
		Wipe(pin)
		return nil, Fail("error.setup.notfresh")
	}
	// Die PIN wird gleich zweimal gebraucht: einmal zum Einrichten und
	// einmal, um den gerade eingerichteten Stick zu oeffnen. Setup und
	// Unlock loeschen ihren Puffer jeweils selbst.
	fuerUnlock := append([]byte(nil), pin...)
	if err := client.Setup(body.Mode, pin); err != nil {
		Wipe(fuerUnlock)
		return nil, err
	}
	// Direkt danach aufmachen: der Anwender hat die PIN eben selbst
	// gesetzt, und der naechste Schritt des Assistenten ist die Sicherung -
	// die braucht einen offenen Tresor. Scheitert das Oeffnen, ist das
	// Einrichten trotzdem geschehen und endgueltig; darum wird es gemeldet
	// und nicht verschwiegen.
	out := map[string]any{"mode": body.Mode, "unlocked": true}
	if err := client.Unlock(fuerUnlock); err != nil {
		out["unlocked"] = false
		out["warning"] = s.app.Message(err)
	}
	return out, nil
}

func (s *Server) apiUnlock(r *http.Request) (any, error) {
	var body struct{ Pin string }
	if err := readJSON(r, &body, maxBodyBytes); err != nil {
		return nil, err
	}
	pin := []byte(body.Pin)
	body.Pin = ""
	client, err := s.app.Client()
	if err != nil {
		Wipe(pin)
		return nil, err
	}
	// Unlock loescht den Puffer selbst, sobald er geschickt ist.
	if err := client.Unlock(pin); err != nil {
		return nil, err
	}
	return map[string]bool{"unlocked": true}, nil
}

func (s *Server) apiLock(r *http.Request) (any, error) {
	client, err := s.app.Client()
	if err != nil {
		return nil, err
	}
	if err := client.Lock(); err != nil {
		return nil, err
	}
	// Nach dem Sperren bleibt keine offene Verbindung stehen.
	s.app.Disconnect()
	return map[string]bool{"locked": true}, nil
}

func (s *Server) apiItems(r *http.Request) (any, error) {
	client, err := s.app.Client()
	if err != nil {
		return nil, err
	}
	profile, err := profileParam(r.URL.Query().Get("profile"))
	if err != nil {
		return nil, err
	}
	query := strings.TrimSpace(r.URL.Query().Get("q"))
	if len(query) > MaxSiteLen {
		return nil, Fail("error.item.site")
	}
	items, err := client.List(profile, query)
	if err != nil {
		return nil, err
	}
	if items == nil {
		items = []Item{}
	}
	return items, nil
}

func profileParam(text string) (uint8, error) {
	if text == "" {
		return 0, nil
	}
	n, err := strconv.ParseUint(text, 10, 8)
	if err != nil {
		return 0, Fail("error.item.profile")
	}
	return uint8(n), nil
}

func (s *Server) apiReveal(r *http.Request) (any, error) {
	var body struct{ ID uint32 }
	if err := readJSON(r, &body, maxBodyBytes); err != nil {
		return nil, err
	}
	if body.ID == 0 {
		return nil, Fail("error.item.id")
	}
	client, err := s.app.Client()
	if err != nil {
		return nil, err
	}
	secret, err := client.Reveal(body.ID)
	if err != nil {
		return nil, err
	}
	// Das Geheimnis geht einmal durch diese Antwort und wird hier sofort
	// wieder ueberschrieben.
	defer Wipe(secret)
	return map[string]string{"secret": string(secret)}, nil
}

func (s *Server) apiSave(r *http.Request) (any, error) {
	var body struct {
		ID             uint32 `json:"id"`
		Type           uint8  `json:"type"`
		Profile        uint8  `json:"profile"`
		Name           string `json:"name"`
		Site           string `json:"site"`
		Mirrored       bool   `json:"mirrored"`
		PairedHostOnly bool   `json:"pairedHostOnly"`
		Secret         string `json:"secret"`
	}
	if err := readJSON(r, &body, maxBodyBytes); err != nil {
		return nil, err
	}
	item := Item{
		ID:             body.ID,
		Type:           ItemType(body.Type),
		Profile:        body.Profile,
		Name:           strings.TrimSpace(body.Name),
		Site:           strings.TrimSpace(body.Site),
		Mirrored:       body.Mirrored,
		PairedHostOnly: body.PairedHostOnly,
	}
	if err := CheckItem(item); err != nil {
		return nil, err
	}
	secret := []byte(body.Secret)
	body.Secret = ""
	if len(secret) > MaxSecretLen {
		Wipe(secret)
		return nil, Fail("error.item.secret")
	}
	client, err := s.app.Client()
	if err != nil {
		Wipe(secret)
		return nil, err
	}
	if item.ID == 0 {
		if len(secret) == 0 {
			return nil, Fail("error.item.secret")
		}
		id, addErr := client.Add(item, secret)
		if addErr != nil {
			return nil, addErr
		}
		return map[string]uint32{"id": id}, nil
	}
	if err := client.Update(item.ID, item, secret); err != nil {
		return nil, err
	}
	return map[string]uint32{"id": item.ID}, nil
}

func (s *Server) apiDelete(r *http.Request) (any, error) {
	var body struct{ ID uint32 }
	if err := readJSON(r, &body, maxBodyBytes); err != nil {
		return nil, err
	}
	if body.ID == 0 {
		return nil, Fail("error.item.id")
	}
	client, err := s.app.Client()
	if err != nil {
		return nil, err
	}
	if err := client.Delete(body.ID); err != nil {
		return nil, err
	}
	return map[string]bool{"deleted": true}, nil
}

func (s *Server) apiLog(r *http.Request) (any, error) {
	client, err := s.app.Client()
	if err != nil {
		return nil, err
	}
	limit := uint64(100)
	if text := r.URL.Query().Get("n"); text != "" {
		n, parseErr := strconv.ParseUint(text, 10, 16)
		if parseErr != nil {
			return nil, Fail("error.request.format")
		}
		limit = n
	}
	entries, err := client.Log(limit)
	if err != nil {
		return nil, err
	}
	if entries == nil {
		entries = []LogEntry{}
	}
	return entries, nil
}

func (s *Server) apiPair(r *http.Request) (any, error) {
	var body struct{ Kind string }
	if err := readJSON(r, &body, maxBodyBytes); err != nil {
		return nil, err
	}
	client, err := s.app.Client()
	if err != nil {
		return nil, err
	}
	return client.Pair(body.Kind)
}

func (s *Server) apiBackup(r *http.Request) (any, error) {
	client, err := s.app.Client()
	if err != nil {
		return nil, err
	}
	blob, err := client.Backup()
	if err != nil {
		return nil, err
	}
	// Die Sicherung ist verschluesselt; der Rechner kann sie nicht lesen.
	return map[string]string{"blob": base64.StdEncoding.EncodeToString(blob)}, nil
}

func (s *Server) apiRestore(r *http.Request) (any, error) {
	var body struct{ Blob string }
	if err := readJSON(r, &body, maxUploadBytes); err != nil {
		return nil, err
	}
	blob, err := base64.StdEncoding.DecodeString(body.Blob)
	if err != nil {
		return nil, Fail("error.request.format")
	}
	client, err := s.app.Client()
	if err != nil {
		return nil, err
	}
	if err := client.Restore(blob); err != nil {
		return nil, err
	}
	return map[string]bool{"restored": true}, nil
}

func (s *Server) apiFirmware(r *http.Request) (any, error) {
	var body struct{ Image string }
	if err := readJSON(r, &body, maxUploadBytes); err != nil {
		return nil, err
	}
	image, err := base64.StdEncoding.DecodeString(body.Image)
	if err != nil {
		return nil, Fail("error.request.format")
	}
	client, err := s.app.Client()
	if err != nil {
		return nil, err
	}
	if err := client.Firmware(image, nil); err != nil {
		return nil, err
	}
	return map[string]int{"sent": len(image)}, nil
}

func (s *Server) apiAttest(r *http.Request) (any, error) {
	var body struct{ Challenge string }
	if err := readJSON(r, &body, maxBodyBytes); err != nil {
		return nil, err
	}
	challenge, err := base64.StdEncoding.DecodeString(body.Challenge)
	if err != nil {
		return nil, Fail("error.request.format")
	}
	client, err := s.app.Client()
	if err != nil {
		return nil, err
	}
	signature, err := client.Attest(challenge)
	if err != nil {
		return nil, err
	}
	return map[string]string{
		"signature": base64.StdEncoding.EncodeToString(signature),
	}, nil
}

func (s *Server) apiSettings(r *http.Request) (any, error) {
	current := s.app.Config()
	var body Config
	if err := readJSON(r, &body, maxBodyBytes); err != nil {
		return nil, err
	}
	merged := current.merge(body)
	if err := merged.Save(ConfigPath()); err != nil {
		return nil, err
	}
	s.app.SetConfig(merged)
	return merged, nil
}
