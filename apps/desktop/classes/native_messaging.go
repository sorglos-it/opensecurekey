// Die Gegenstelle der Browser-Erweiterung.
//
//	osk native <herkunft>
//
// So startet der Browser dieses Programm: als Kindprozess, mit der Kennung
// der Erweiterung als erstem Aufrufparameter. Gesprochen wird ueber die
// Standardein- und -ausgabe, nach den Regeln von WebExtensions Native
// Messaging: vier Bytes Laenge (Reihenfolge des Rechners), dahinter JSON.
//
// Warum dieser Weg und nicht ein Port auf 127.0.0.1: ein Port ist von jeder
// Webseite im selben Browser erreichbar. Diesen Kindprozess startet nur der
// Browser, und nur fuer die Erweiterungen, die in seiner Anmeldedatei
// stehen. Es gibt hier also keinen offenen Kanal, den eine fremde Seite
// anklopfen koennte.
//
// DER PHISHING-SCHUTZ STECKT HIER, NICHT IN DER ERWEITERUNG.
//
// Die Erweiterung nennt die Adresse der Seite, auf der sie steht. Welche
// Eintraege dazu passen und ob ein Geheimnis herausgeht, entscheidet dieses
// Programm - und es gibt ein Geheimnis NUR heraus, wenn die Adresse des
// Eintrags zur genannten Adresse passt. Waere die Entscheidung in der
// Erweiterung, haette eine uebernommene Erweiterung den ganzen Tresor; so
// hat sie nur das, was zur Seite gehoert, auf der der Anwender steht.
//
// Was hier NIE passiert: ausfuellen, ohne dass der Anwender es anstoesst,
// und ein Geheimnis zu einer Adresse herausgeben, die nicht im Eintrag
// steht.
package classes

import (
	"encoding/binary"
	"encoding/json"
	"fmt"
	"io"
	"strings"
)

// Grenzen des Kanals. Chrome nimmt vom Wirt hoechstens ein Megabyte an und
// schickt hoechstens 64 - mehr als ein Megabyte braucht hier nichts.
const (
	nativeMaxMessage = 1024 * 1024
	// NativeHostName ist der Name, unter dem die Erweiterung diesen Wirt
	// anspricht. Er steht auch in der Anmeldedatei des Browsers.
	NativeHostName = "li.stw.opensecurekey"
)

// nativeRequest ist eine Anfrage der Erweiterung.
type nativeRequest struct {
	// ID kommt zurueck, damit die Erweiterung Antwort und Frage zuordnen
	// kann.
	ID int `json:"id"`
	// Was: hello, state, unlock, list, reveal, lock
	Was string `json:"was"`
	// Origin ist die Adresse der Seite, auf der die Erweiterung steht -
	// aus der Adresszeile des Browsers, nicht aus dem Inhalt der Seite.
	Origin string `json:"origin,omitempty"`
	// Item ist die Nummer des gemeinten Eintrags.
	Item uint32 `json:"item,omitempty"`
	// Feld sagt, welcher Teil gebraucht wird: "user" oder "secret".
	Feld string `json:"feld,omitempty"`
	// PIN nur bei unlock. Sie steht nie in einem Protokoll.
	PIN string `json:"pin,omitempty"`
}

// nativeReply ist die Antwort.
type nativeReply struct {
	ID       int              `json:"id"`
	OK       bool             `json:"ok"`
	Error    string           `json:"error,omitempty"`
	Unlocked bool             `json:"unlocked,omitempty"`
	Origin   string           `json:"origin,omitempty"`
	Matches  []nativeMatch    `json:"matches,omitempty"`
	Value    string           `json:"value,omitempty"`
	Claim    string           `json:"claim,omitempty"`
	Device   *nativeDeviceRef `json:"device,omitempty"`
}

// nativeMatch ist ein Eintrag, der zur Adresse passt. Ein Geheimnis steht
// hier nie drin.
type nativeMatch struct {
	ID   uint32 `json:"id"`
	Name string `json:"name"`
	Site string `json:"site"`
	Type uint8  `json:"type"`
}

// nativeDeviceRef nennt das verbundene Geraet.
type nativeDeviceRef struct {
	Label     string `json:"label"`
	Serial    string `json:"serial"`
	Simulated bool   `json:"simulated"`
}

// nativeHost haelt den Zustand eines Gespraechs mit dem Browser.
type nativeHost struct {
	app *App
	// caller ist die Kennung der Erweiterung, wie der Browser sie uebergibt.
	// Sie kommt vom Browser, nicht von der Erweiterung - das ist der
	// Unterschied zu einer Behauptung, und sie steht so im Geraeteprotokoll.
	caller string
}

// cliNative bedient den Browser. Der Befehl laeuft, bis der Browser die
// Verbindung schliesst.
func cliNative(app *App, args []string, in io.Reader, out, errOut io.Writer) int {
	caller := ""
	if len(args) > 1 {
		caller = args[1]
	}
	host := &nativeHost{app: app, caller: nativeCaller(caller)}
	if err := host.serve(in, out); err != nil && err != io.EOF {
		// Der Browser liest stderr nicht; die Meldung ist fuer den
		// Anwender, der den Befehl von Hand ausprobiert.
		fmt.Fprintln(errOut, app.Message(Fail("error.native.channel")))
		return ExitError
	}
	return ExitOK
}

// nativeCaller macht aus dem Aufrufparameter des Browsers einen Namen fuer
// das Geraeteprotokoll. Chrome uebergibt "chrome-extension://<kennung>/",
// Firefox die Kennung der Erweiterung.
func nativeCaller(raw string) string {
	clean := CleanAskedBy(raw)
	if clean == DefaultAppName {
		return "browser"
	}
	return "browser: " + clean
}

// serve liest Nachrichten, bis der Kanal endet.
func (h *nativeHost) serve(in io.Reader, out io.Writer) error {
	for {
		request, err := readNativeMessage(in)
		if err != nil {
			return err
		}
		reply := h.handle(request)
		if err := writeNativeMessage(out, reply); err != nil {
			return err
		}
	}
}

// readNativeMessage liest eine Nachricht: vier Bytes Laenge, dann JSON.
//
// Die Reihenfolge der vier Bytes ist die des Rechners - so steht es in der
// Beschreibung von Native Messaging. Auf allen Rechnern, auf denen dieses
// Programm laeuft, ist das das kleine Ende; wer es dennoch auf einem
// grossen baut, bekommt hier die richtige Reihenfolge, weil
// binary.NativeEndian genau das liefert.
func readNativeMessage(reader io.Reader) (nativeRequest, error) {
	var header [4]byte
	if _, err := io.ReadFull(reader, header[:]); err != nil {
		return nativeRequest{}, err
	}
	size := binary.NativeEndian.Uint32(header[:])
	if size == 0 || size > nativeMaxMessage {
		return nativeRequest{}, Fail("error.native.size")
	}
	body := make([]byte, size)
	if _, err := io.ReadFull(reader, body); err != nil {
		return nativeRequest{}, err
	}
	var request nativeRequest
	if err := json.Unmarshal(body, &request); err != nil {
		Wipe(body)
		return nativeRequest{}, Fail("error.native.format")
	}
	Wipe(body)
	return request, nil
}

// writeNativeMessage schreibt eine Nachricht mit Laenge davor.
func writeNativeMessage(writer io.Writer, reply nativeReply) error {
	body, err := json.Marshal(reply)
	if err != nil {
		return Fail("error.native.format")
	}
	if len(body) > nativeMaxMessage {
		return Fail("error.native.size")
	}
	var header [4]byte
	binary.NativeEndian.PutUint32(header[:], uint32(len(body)))
	if _, err := writer.Write(header[:]); err != nil {
		return err
	}
	_, err = writer.Write(body)
	// Der Puffer hat ein Geheimnis getragen, wenn es eine Antwort auf
	// reveal war. Er wird ueberschrieben, bevor er dem Sammler zufaellt.
	Wipe(body)
	return err
}

// handle beantwortet eine Anfrage.
func (h *nativeHost) handle(request nativeRequest) nativeReply {
	reply := nativeReply{ID: request.ID}
	switch request.Was {
	case "hello":
		return h.replyHello(reply)
	case "state":
		return h.replyState(reply)
	case "unlock":
		return h.replyUnlock(reply, request)
	case "lock":
		return h.replyLock(reply)
	case "list":
		return h.replyList(reply, request)
	case "reveal":
		return h.replyReveal(reply, request)
	}
	reply.Error = h.app.Text("error.native.unknown")
	return reply
}

func (h *nativeHost) access() (*Access, error) {
	client, err := h.app.Client()
	if err != nil {
		return nil, err
	}
	return NewAccess(client, h.caller), nil
}

func (h *nativeHost) replyHello(reply nativeReply) nativeReply {
	reply.OK = true
	// Der Satz, der dem Anwender sagt, wie der Name im Geraeteprotokoll zu
	// lesen ist. Er geht mit, damit die Erweiterung ihn anzeigen kann,
	// ohne ihn selbst zu formulieren.
	reply.Claim = h.app.Text("claim.appname")
	client, err := h.app.Client()
	if err != nil {
		reply.Error = h.app.Message(err)
		reply.OK = false
		return reply
	}
	info, err := client.Hello()
	if err != nil {
		reply.Error = h.app.Message(err)
		reply.OK = false
		return reply
	}
	reply.Unlocked = info.Unlocked
	reply.Device = &nativeDeviceRef{
		Label:     client.Describe(),
		Serial:    info.Serial,
		Simulated: strings.HasPrefix(h.app.CurrentPath(), SimPrefix),
	}
	return reply
}

func (h *nativeHost) replyState(reply nativeReply) nativeReply {
	client, err := h.app.Client()
	if err != nil {
		reply.Error = h.app.Message(err)
		return reply
	}
	info, err := client.Hello()
	if err != nil {
		reply.Error = h.app.Message(err)
		return reply
	}
	reply.OK = true
	reply.Unlocked = info.Unlocked
	return reply
}

func (h *nativeHost) replyUnlock(reply nativeReply, request nativeRequest) nativeReply {
	client, err := h.app.Client()
	if err != nil {
		reply.Error = h.app.Message(err)
		return reply
	}
	pin := []byte(request.PIN)
	request.PIN = ""
	if err := client.Unlock(pin); err != nil {
		reply.Error = h.app.Message(err)
		return reply
	}
	reply.OK = true
	reply.Unlocked = true
	return reply
}

func (h *nativeHost) replyLock(reply nativeReply) nativeReply {
	client, err := h.app.Client()
	if err != nil {
		reply.Error = h.app.Message(err)
		return reply
	}
	if err := client.Lock(); err != nil {
		reply.Error = h.app.Message(err)
		return reply
	}
	reply.OK = true
	reply.Unlocked = false
	return reply
}

// replyList nennt die Eintraege, die zu der Adresse gehoeren. Passt keiner,
// ist die Antwort eine leere Liste - und ausdruecklich KEINE Liste der
// uebrigen Eintraege. Die Erweiterung erfaehrt nie, was sonst im Tresor
// liegt.
func (h *nativeHost) replyList(reply nativeReply, request nativeRequest) nativeReply {
	origin, err := CheckOrigin(request.Origin)
	if err != nil {
		reply.Error = h.app.Message(err)
		return reply
	}
	reply.Origin = origin.String()
	access, err := h.access()
	if err != nil {
		reply.Error = h.app.Message(err)
		return reply
	}
	items, err := access.List(0, "")
	if err != nil {
		reply.Error = h.app.Message(err)
		return reply
	}
	reply.OK = true
	reply.Matches = []nativeMatch{}
	for _, item := range items {
		if !origin.Matches(item.Site) {
			continue
		}
		reply.Matches = append(reply.Matches, nativeMatch{
			ID:   item.ID,
			Name: item.Name,
			Site: item.Site,
			Type: uint8(item.Type),
		})
	}
	return reply
}

// replyReveal gibt genau ein Geheimnis heraus - und nur, wenn die Adresse
// des Eintrags zur genannten Adresse passt.
//
// Die Pruefung wird hier NOCH EINMAL gemacht, obwohl replyList schon
// gefiltert hat. Das ist kein Versehen: die Erweiterung koennte eine Nummer
// schicken, die sie nie aus einer Liste bekommen hat.
func (h *nativeHost) replyReveal(reply nativeReply, request nativeRequest) nativeReply {
	origin, err := CheckOrigin(request.Origin)
	if err != nil {
		reply.Error = h.app.Message(err)
		return reply
	}
	if request.Feld != "user" && request.Feld != "secret" {
		reply.Error = h.app.Text("error.native.field")
		return reply
	}
	access, err := h.access()
	if err != nil {
		reply.Error = h.app.Message(err)
		return reply
	}
	items, err := access.List(0, "")
	if err != nil {
		reply.Error = h.app.Message(err)
		return reply
	}
	var found *Item
	for index := range items {
		if items[index].ID == request.Item {
			found = &items[index]
			break
		}
	}
	if found == nil {
		reply.Error = h.app.Text("error.device.notfound")
		return reply
	}
	if !origin.Matches(found.Site) {
		// Der eigentliche Riegel. Die Meldung sagt, WARUM - sonst sucht der
		// Anwender den Fehler bei sich.
		reply.Error = h.app.Text("error.native.origin")
		return reply
	}
	// Der Benutzername braucht keine Freigabe am Geraet - er ist kein
	// Geheimnis.
	//
	// Nur: ES GIBT KEIN FELD DAFUER. shared/vault-format.md kennt je
	// Eintrag einen Namen, eine Adresse und EIN Geheimnis. Also wird hier
	// der Name des Eintrags als Benutzername angeboten - das ist, was ein
	// Tresor ohne eigenes Feld hergeben kann, und die Erweiterung zeigt
	// ihn vorher an, damit niemand ueberrascht wird. Braucht ein Eintrag
	// einen anderen Benutzernamen als seinen Namen, fehlt dem Format ein
	// Feld; das ist im Bericht dieses Strangs genannt.
	if request.Feld == "user" {
		reply.OK = true
		reply.Origin = origin.String()
		reply.Value = found.Name
		return reply
	}
	secret, err := access.Reveal(found.ID)
	if err != nil {
		reply.Error = h.app.Message(err)
		return reply
	}
	reply.OK = true
	reply.Origin = origin.String()
	reply.Value = string(secret)
	Wipe(secret)
	return reply
}

// nativeManifestJSON baut die Anmeldedatei, die der Browser braucht. Sie
// nennt den Pfad des Startskripts und die Erweiterungen, die es ansprechen
// duerfen - mehr darf niemand. Genau diese Liste ist der Riegel: ohne sie
// koennte jede Erweiterung im Browser den Wirt starten.
func nativeManifestJSON(browser, path string, extensions []string) ([]byte, error) {
	if path == "" || len(extensions) == 0 {
		return nil, Fail("error.native.manifest")
	}
	record := map[string]any{
		"name":        NativeHostName,
		"description": "OpenSecureKey",
		"path":        path,
		"type":        "stdio",
	}
	switch browser {
	case "chrome":
		origins := make([]string, 0, len(extensions))
		for _, id := range extensions {
			origins = append(origins, "chrome-extension://"+id+"/")
		}
		record["allowed_origins"] = origins
	case "firefox":
		record["allowed_extensions"] = append([]string(nil), extensions...)
	default:
		return nil, Fail("error.native.browser", browser)
	}
	return json.MarshalIndent(record, "", "  ")
}
