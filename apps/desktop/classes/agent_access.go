// Ein Zugang im Namen eines bestimmten Programms.
//
// shared/protocol.md verlangt: "Every reveal is written to the device log with
// the name of the asking program. That name is a claim by the program, not
// proof - it is shown as such." Die Oberflaeche und die Kommandozeile sind
// EIN Programm und nennen darum einen festen Namen (Config.AppName). Die
// Anbindungen sind es nicht: hinter dem ssh-Agenten steckt ssh oder git,
// hinter PKCS#11 steckt Firefox oder openssl, hinter der lokalen
// Schnittstelle steckt ein fremdes Programm. Jeder dieser Zugriffe muss dem
// Geraet SEINEN Namen nennen, nicht unseren.
//
// Darum dieser Zugang: eine einzige Verbindung zum Geraet, aber je Anfrage
// ein eigener Name. Zwei Verbindungen waeren der naheliegende Weg und der
// falsche - ein HID-Geraet zweimal zu oeffnen heisst, dass sich zwei
// Nachrichtenstroeme im selben Kanal vermischen.
package classes

import (
	"strings"
	"time"
)

// MaxAskedByLen ist die Laenge, auf die ein behaupteter Name gekuerzt wird.
// Sie ist dieselbe wie in NewClient - das Geraet soll nie mehr sehen muessen.
const MaxAskedByLen = 64

// Access ist ein Zugriffsweg mit eigenem Namen. Er haelt keine eigene
// Verbindung: er benutzt die des Programms und haengt nur seinen Namen an.
type Access struct {
	client *Client
	name   string
}

// NewAccess baut einen Zugang. name ist der Name, den das fragende Programm
// fuer sich behauptet; er wird geputzt, aber nicht geprueft - pruefen kann man
// ihn nicht, und genau das sagt die Oberflaeche dem Anwender auch.
func NewAccess(client *Client, name string) *Access {
	return &Access{client: client, name: CleanAskedBy(name)}
}

// Name nennt den behaupteten Namen, so wie er zum Geraet geht.
func (a *Access) Name() string { return a.name }

// CleanAskedBy macht aus einem behaupteten Namen etwas, das gefahrlos in ein
// Protokoll geschrieben und angezeigt werden kann: keine Steuerzeichen, keine
// unbegrenzte Laenge, nie leer.
func CleanAskedBy(name string) string {
	clean := make([]rune, 0, len(name))
	for _, r := range strings.TrimSpace(name) {
		if r < 0x20 || r == 0x7f {
			continue
		}
		clean = append(clean, r)
		if len(clean) >= MaxAskedByLen {
			break
		}
	}
	if len(clean) == 0 {
		return DefaultAppName
	}
	return string(clean)
}

// CallAs ist Client.Call, nennt dem Geraet aber den uebergebenen Namen statt
// den der Verbindung. Der Rumpf ist derselbe wie in Call; mit einer Zeile in
// client.go (Call ruft callAs mit c.askedBy) waere die Wiederholung weg -
// diese Datei darf client.go aber nicht aendern, und die Doppelung ist im
// Bericht des Strangs genannt.
func (c *Client) CallAs(askedBy string, cmd uint64, data CborMap,
	timeout time.Duration) (CborMap, error) {
	c.mu.Lock()
	defer c.mu.Unlock()

	if data == nil {
		data = CborMap{}
	}
	data["a"] = CleanAskedBy(askedBy)
	c.seq++
	seq := c.seq
	request := CborMap{
		"v": uint64(ProtocolVersion),
		"c": cmd,
		"s": seq,
		"d": data,
	}
	payload, err := CborEncode(request)
	if err != nil {
		return nil, err
	}
	reports, err := SplitFrames(payload)
	if err != nil {
		return nil, err
	}
	for _, report := range reports {
		if err := c.transport.WriteReport(report); err != nil {
			return nil, err
		}
	}
	answer, err := JoinFrames(c.transport, timeout)
	if err != nil {
		return nil, err
	}
	return parseAnswer(answer, seq)
}

// List holt die Angaben der Eintraege - niemals Geheimnisse.
func (a *Access) List(profile uint8, query string) ([]Item, error) {
	data := CborMap{"p": uint64(profile)}
	if query != "" {
		if len(query) > MaxSiteLen {
			return nil, Fail("error.item.site")
		}
		data["q"] = query
	}
	answer, err := a.client.CallAs(a.name, CmdList, data, readTimeout)
	if err != nil {
		return nil, err
	}
	raw, ok := CborList(answer, "items")
	if !ok {
		return nil, Fail("error.protocol.answer")
	}
	out := make([]Item, 0, len(raw))
	for _, entry := range raw {
		record, isMap := entry.(CborMap)
		if !isMap {
			return nil, Fail("error.protocol.answer")
		}
		item, itemErr := itemFromCbor(record)
		if itemErr != nil {
			return nil, itemErr
		}
		out = append(out, item)
	}
	return out, nil
}

// Reveal holt ein Geheimnis im Namen dieses Zugangs. Der Aufrufer muss den
// Puffer mit Wipe loeschen, sobald er ihn nicht mehr braucht.
func (a *Access) Reveal(id uint32) ([]byte, error) {
	answer, err := a.client.CallAs(a.name, CmdReveal, CborMap{"id": uint64(id)}, readTimeout)
	if err != nil {
		return nil, err
	}
	if secret, ok := CborBytes(answer, "sec"); ok {
		return secret, nil
	}
	if text, isText := CborText(answer, "sec"); isText {
		return []byte(text), nil
	}
	return nil, Fail("error.protocol.answer")
}

// Add legt einen Eintrag an. secret wird nach dem Senden ueberschrieben.
func (a *Access) Add(it Item, secret []byte) (uint32, error) {
	defer Wipe(secret)
	if err := CheckItem(it); err != nil {
		return 0, err
	}
	if len(secret) == 0 || len(secret) > MaxSecretLen {
		return 0, Fail("error.item.secret")
	}
	data := itemToCbor(it)
	data["sec"] = secret
	answer, err := a.client.CallAs(a.name, CmdAdd, data, readTimeout)
	if err != nil {
		return 0, err
	}
	id, ok := CborUint(answer, "id")
	if !ok || id > 0xffffffff {
		return 0, Fail("error.protocol.answer")
	}
	return uint32(id), nil
}

// ItemsOfType holt die Eintraege eines Typs. Mehr braucht keine Anbindung:
// der ssh-Agent will Schluessel, PKCS#11 will Schluessel und Zertifikate,
// die Erweiterung will Zugaenge.
func (a *Access) ItemsOfType(want ...ItemType) ([]Item, error) {
	all, err := a.List(0, "")
	if err != nil {
		return nil, err
	}
	var out []Item
	for _, item := range all {
		for _, t := range want {
			if item.Type == t {
				out = append(out, item)
				break
			}
		}
	}
	return out, nil
}
