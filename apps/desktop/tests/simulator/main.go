// Geraete-Attrappe: spricht das Protokoll aus shared/protocol.md, damit
// Oberflaeche und Kommandozeile ohne Hardware laufen. Sie ist ein Pruefmittel
// und nichts anderes: der Tresor liegt im Arbeitsspeicher, es wird nichts
// verschluesselt, und die Freigabe am Geraet wird nur behauptet.
//
//	go run ./tests/simulator            lauscht auf 127.0.0.1:8201
//	go run ./tests/simulator -port 8202
package main

import (
	"flag"
	"fmt"
	"io"
	"log"
	"net"
	"os"
	"strconv"
	"strings"
	"sync"
	"time"

	"osk/classes"
)

// vault ist der Tresor der Attrappe.
type vault struct {
	mu       sync.Mutex
	unlocked bool
	nextID   uint32
	items    map[uint32]record
	log      []classes.CborMap
}

type record struct {
	meta   classes.Item
	secret []byte
}

const demoPin = "123456"

func newVault() *vault {
	v := &vault{nextID: 1, items: map[uint32]record{}}
	v.put(classes.Item{Type: classes.ItemLogin, Name: "Beispiel-Zugang",
		Site: "example.org"}, []byte("geheim-1"))
	v.put(classes.Item{Type: classes.ItemCert, Name: "Beispiel-Zertifikat",
		Site: "vpn.example.org"}, []byte("-----BEGIN CERTIFICATE-----"))
	v.put(classes.Item{Type: classes.ItemTotp, Name: "Beispiel-Einmalcode",
		Site: "mail.example.org"}, []byte("JBSWY3DPEHPK3PXP"))
	return v
}

func (v *vault) put(meta classes.Item, secret []byte) uint32 {
	id := v.nextID
	v.nextID++
	meta.ID = id
	meta.ChangedUTC = time.Now().Unix()
	v.items[id] = record{meta: meta, secret: secret}
	return id
}

func (v *vault) note(event, askedBy string, id uint32) {
	v.log = append(v.log, classes.CborMap{
		"ts": uint64(time.Now().Unix()),
		"e":  event,
		"a":  askedBy,
		"id": uint64(id),
	})
	if len(v.log) > 200 {
		v.log = v.log[len(v.log)-200:]
	}
}

func main() {
	port := flag.Int("port", 8201, "Port auf 127.0.0.1")
	flag.Parse()

	listener, err := net.Listen("tcp", "127.0.0.1:"+strconv.Itoa(*port))
	if err != nil {
		log.Println("Port belegt:", err)
		os.Exit(1)
	}
	store := newVault()
	fmt.Printf("Attrappe laeuft auf 127.0.0.1:%d (PIN %s)\n", *port, demoPin)
	for {
		conn, acceptErr := listener.Accept()
		if acceptErr != nil {
			return
		}
		go serve(conn, store)
	}
}

// connTransport macht aus einer Verbindung einen Transport, damit dieselbe
// Rahmenlogik wie im Programm benutzt wird.
type connTransport struct {
	conn net.Conn
}

func (c connTransport) WriteReport(report []byte) error {
	_, err := c.conn.Write(report)
	return err
}

func (c connTransport) ReadReport(timeout time.Duration) ([]byte, error) {
	if err := c.conn.SetReadDeadline(time.Now().Add(timeout)); err != nil {
		return nil, err
	}
	report := make([]byte, classes.ReportSize)
	if _, err := io.ReadFull(c.conn, report); err != nil {
		return nil, err
	}
	return report, nil
}

func (c connTransport) Close() error     { return c.conn.Close() }
func (c connTransport) Describe() string { return "simulator" }

func serve(conn net.Conn, store *vault) {
	defer conn.Close()
	transport := connTransport{conn: conn}
	for {
		payload, err := classes.JoinFrames(transport, 10*time.Minute)
		if err != nil {
			return
		}
		answer := handle(payload, store)
		out, err := classes.CborEncode(answer)
		if err != nil {
			return
		}
		reports, err := classes.SplitFrames(out)
		if err != nil {
			return
		}
		for _, report := range reports {
			if writeErr := transport.WriteReport(report); writeErr != nil {
				return
			}
		}
	}
}

// handle beantwortet eine Anfrage. Jede Antwort traegt dieselbe Nummer und
// ein Ergebnis; unbekannte Befehle bekommen OSK_ERR_UNSUPPORTED.
func handle(payload []byte, store *vault) classes.CborMap {
	value, err := classes.CborDecode(payload)
	if err != nil {
		return reply(0, 9, nil) // OSK_ERR_FORMAT
	}
	request, ok := value.(classes.CborMap)
	if !ok {
		return reply(0, 9, nil)
	}
	seq, _ := classes.CborUint(request, "s")
	version, _ := classes.CborUint(request, "v")
	if version != classes.ProtocolVersion {
		return reply(seq, 10, nil) // OSK_ERR_UNSUPPORTED
	}
	cmd, _ := classes.CborUint(request, "c")
	data, _ := classes.CborSub(request, "d")
	if data == nil {
		data = classes.CborMap{}
	}
	askedBy, _ := classes.CborText(data, "a")

	store.mu.Lock()
	defer store.mu.Unlock()

	switch cmd {
	case classes.CmdHello:
		return reply(seq, 0, classes.CborMap{
			"fw":       "0.0.0.0-attrappe",
			"serial":   "SIM-0001",
			"proto":    uint64(classes.ProtocolVersion),
			"profiles": uint64(1),
			"unlocked": store.unlocked,
			"tr":       []any{"usb-hid", "sim"},
		})
	case classes.CmdUnlock:
		pin, _ := classes.CborBytes(data, "pin")
		if string(pin) != demoPin {
			store.note("unlock-denied", askedBy, 0)
			return reply(seq, 4, nil) // OSK_ERR_DENIED
		}
		store.unlocked = true
		store.note("unlock", askedBy, 0)
		return reply(seq, 0, classes.CborMap{"unlocked": true})
	case classes.CmdLock:
		store.unlocked = false
		store.note("lock", askedBy, 0)
		return reply(seq, 0, classes.CborMap{})
	}

	if !store.unlocked {
		return reply(seq, 5, nil) // OSK_ERR_LOCKED
	}

	switch cmd {
	case classes.CmdList:
		query, _ := classes.CborText(data, "q")
		return reply(seq, 0, classes.CborMap{"items": listItems(store, query)})
	case classes.CmdReveal:
		id, _ := classes.CborUint(data, "id")
		entry, found := store.items[uint32(id)]
		if !found {
			return reply(seq, 6, nil) // OSK_ERR_NOT_FOUND
		}
		store.note("reveal", askedBy, uint32(id))
		return reply(seq, 0, classes.CborMap{"sec": entry.secret})
	case classes.CmdAdd:
		meta, secret, metaErr := itemFrom(data)
		if metaErr != nil || len(secret) == 0 {
			return reply(seq, 1, nil) // OSK_ERR_ARG
		}
		id := store.put(meta, secret)
		store.note("add", askedBy, id)
		return reply(seq, 0, classes.CborMap{"id": uint64(id)})
	case classes.CmdUpdate:
		id, _ := classes.CborUint(data, "id")
		entry, found := store.items[uint32(id)]
		if !found {
			return reply(seq, 6, nil)
		}
		meta, secret, metaErr := itemFrom(data)
		if metaErr != nil {
			return reply(seq, 1, nil)
		}
		meta.ID = entry.meta.ID
		meta.ChangedUTC = time.Now().Unix()
		if len(secret) == 0 {
			secret = entry.secret
		}
		store.items[uint32(id)] = record{meta: meta, secret: secret}
		store.note("update", askedBy, uint32(id))
		return reply(seq, 0, classes.CborMap{"id": id})
	case classes.CmdDelete:
		id, _ := classes.CborUint(data, "id")
		if _, found := store.items[uint32(id)]; !found {
			return reply(seq, 6, nil)
		}
		delete(store.items, uint32(id))
		store.note("delete", askedBy, uint32(id))
		return reply(seq, 0, classes.CborMap{})
	case classes.CmdLog:
		limit, _ := classes.CborUint(data, "n")
		lines := store.log
		if limit > 0 && uint64(len(lines)) > limit {
			lines = lines[uint64(len(lines))-limit:]
		}
		out := make([]any, 0, len(lines))
		for _, line := range lines {
			out = append(out, line)
		}
		return reply(seq, 0, classes.CborMap{"log": out})
	case classes.CmdPair:
		kind, _ := classes.CborText(data, "k")
		if kind != "host" && kind != "device" {
			return reply(seq, 1, nil)
		}
		store.note("pair-"+kind, askedBy, 0)
		return reply(seq, 0, classes.CborMap{"num": "428 913", "done": false})
	case classes.CmdBackup:
		store.note("backup", askedBy, 0)
		// Nur ein Platzhalter: die Attrappe verschluesselt nichts.
		return reply(seq, 0, classes.CborMap{"blob": []byte("sim-backup")})
	case classes.CmdRestore:
		blob, _ := classes.CborBytes(data, "blob")
		if len(blob) == 0 {
			return reply(seq, 1, nil)
		}
		store.note("restore", askedBy, 0)
		return reply(seq, 0, classes.CborMap{})
	case classes.CmdFirmware:
		part, _ := classes.CborBytes(data, "part")
		if len(part) == 0 {
			return reply(seq, 1, nil)
		}
		last, _ := data["last"].(bool)
		if last {
			store.note("firmware", askedBy, 0)
		}
		return reply(seq, 0, classes.CborMap{"off": uint64(len(part))})
	case classes.CmdAttest:
		challenge, _ := classes.CborBytes(data, "ch")
		if len(challenge) == 0 {
			return reply(seq, 1, nil)
		}
		store.note("attest", askedBy, 0)
		// Keine echte Unterschrift - nur eine Antwort gleicher Laenge.
		signature := make([]byte, len(challenge))
		for i, b := range challenge {
			signature[i] = b ^ 0x5a
		}
		return reply(seq, 0, classes.CborMap{"sig": signature})
	}
	return reply(seq, 10, nil) // OSK_ERR_UNSUPPORTED
}

func listItems(store *vault, query string) []any {
	out := make([]any, 0, len(store.items))
	for id := uint32(1); id < store.nextID; id++ {
		entry, found := store.items[id]
		if !found {
			continue
		}
		if query != "" && !contains(entry.meta.Name, query) &&
			!contains(entry.meta.Site, query) {
			continue
		}
		record := classes.CborMap{
			"id": uint64(entry.meta.ID),
			"t":  uint64(entry.meta.Type),
			"p":  uint64(entry.meta.Profile),
			"n":  entry.meta.Name,
			"s":  entry.meta.Site,
			"ch": uint64(entry.meta.ChangedUTC),
			"m":  entry.meta.Mirrored,
			"ph": entry.meta.PairedHostOnly,
		}
		// Der oeffentliche Teil geht ohne Freigabe mit - das echte Geraet
		// macht es genauso (osk_store_public, shared/protocol.md Befehl 4).
		// Er ist kein Geheimnis, und ihn einzeln abzuholen haette einen
		// Tastendruck je Schluessel gekostet.
		if pub := publicPartOf(entry); len(pub) > 0 {
			record["pub"] = pub
		}
		out = append(out, record)
	}
	return out
}

// publicPartOf liefert den oeffentlichen Teil eines Eintrags, soweit sein Typ
// einen hat. Die Attrappe rechnet ihn aus dem gespeicherten Geheimnis - das
// echte Geraet holt ihn aus dem Sicherheitschip oder aus dem Tresor, ohne das
// Geheimnis anzufassen. Fuer den Aufrufer ist das derselbe Wert.
func publicPartOf(entry record) []byte {
	switch entry.meta.Type {
	case classes.ItemSSHKey:
		key, err := classes.ParseSSHKey(entry.secret)
		if err != nil {
			return nil
		}
		defer key.Wipe()
		return append([]byte(nil), key.Blob...)
	case classes.ItemCert, classes.ItemPGPKey:
		// Zertifikate und PGP-Schluessel liegen hier als Ganzes; der
		// oeffentliche Teil ist das, was auch ohne Freigabe herausgeht.
		return append([]byte(nil), entry.secret...)
	}
	return nil
}

// contains sucht ohne Ruecksicht auf Gross- und Kleinschreibung.
func contains(text, part string) bool {
	return strings.Contains(strings.ToLower(text), strings.ToLower(part))
}

func itemFrom(data classes.CborMap) (classes.Item, []byte, error) {
	typeNumber, _ := classes.CborUint(data, "t")
	profile, _ := classes.CborUint(data, "p")
	name, _ := classes.CborText(data, "n")
	site, _ := classes.CborText(data, "s")
	mirrored, _ := data["m"].(bool)
	pairedOnly, _ := data["ph"].(bool)
	secret, _ := classes.CborBytes(data, "sec")
	item := classes.Item{
		Type:           classes.ItemType(typeNumber),
		Profile:        uint8(profile),
		Name:           name,
		Site:           site,
		Mirrored:       mirrored,
		PairedHostOnly: pairedOnly,
	}
	if err := classes.CheckItem(item); err != nil {
		return classes.Item{}, nil, err
	}
	return item, secret, nil
}

func reply(seq, result uint64, data classes.CborMap) classes.CborMap {
	if data == nil {
		data = classes.CborMap{}
	}
	return classes.CborMap{
		"v": uint64(classes.ProtocolVersion),
		"s": seq,
		"r": result,
		"d": data,
	}
}
