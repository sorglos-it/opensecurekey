// Das Protokoll aus shared/protocol.md, Seite des Rechners. Jede Anfrage ist
// eine CBOR-Abbildung mit v, c, s und d; jede Antwort traegt dasselbe s und
// ein r. Jede Anfrage nennt dem Geraet den Namen des fragenden Programms -
// das ist eine Behauptung, kein Beweis, und wird so auch angezeigt.
package classes

import (
	"strconv"
	"sync"
	"time"
)

// ProtocolVersion ist die Fassung, die dieses Programm spricht.
const ProtocolVersion = 1

// Die Befehlsnummern aus shared/protocol.md.
const (
	CmdHello    = 1
	CmdUnlock   = 2
	CmdLock     = 3
	CmdList     = 4
	CmdReveal   = 5
	CmdAdd      = 6
	CmdUpdate   = 7
	CmdDelete   = 8
	CmdLog      = 9
	CmdPair     = 10
	CmdBackup   = 11
	CmdRestore  = 12
	CmdFirmware = 13
	CmdAttest   = 14
)

// Ergebniswerte des Geraets, gleiche Reihenfolge wie osk_result_t in
// apps/firmware/include/osk_result.h.
const (
	DevOK = 0
)

var deviceErrorKeys = []string{
	"error.device.ok",
	"error.device.arg",
	"error.device.io",
	"error.device.timeout",
	"error.device.denied",
	"error.device.locked",
	"error.device.notfound",
	"error.device.exists",
	"error.device.full",
	"error.device.format",
	"error.device.unsupported",
	"error.device.internal",
}

// Client spricht mit genau einem Geraet. Alle Aufrufe sind gegeneinander
// gesperrt: zwei Anfragen gleichzeitig wuerden die Berichte vermischen.
type Client struct {
	mu        sync.Mutex
	transport Transport
	seq       uint64
	askedBy   string
}

// NewClient bindet einen Transport. askedBy ist der Name, den das Geraet
// protokolliert.
func NewClient(t Transport, askedBy string) *Client {
	if askedBy == "" {
		askedBy = "osk"
	}
	if len(askedBy) > 64 {
		askedBy = askedBy[:64]
	}
	return &Client{transport: t, askedBy: askedBy}
}

// Describe nennt das Geraet, so wie das System es meldet.
func (c *Client) Describe() string { return c.transport.Describe() }

// Close schliesst den Transport.
func (c *Client) Close() error {
	c.mu.Lock()
	defer c.mu.Unlock()
	return c.transport.Close()
}

// Call schickt einen Befehl und wartet auf die Antwort.
func (c *Client) Call(cmd uint64, data CborMap, timeout time.Duration) (CborMap, error) {
	c.mu.Lock()
	defer c.mu.Unlock()

	if data == nil {
		data = CborMap{}
	}
	data["a"] = c.askedBy
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

func parseAnswer(payload []byte, seq uint64) (CborMap, error) {
	value, err := CborDecode(payload)
	if err != nil {
		return nil, err
	}
	top, ok := value.(CborMap)
	if !ok {
		return nil, Fail("error.protocol.answer")
	}
	if version, found := CborUint(top, "v"); !found || version != ProtocolVersion {
		return nil, Fail("error.protocol.version")
	}
	if got, found := CborUint(top, "s"); !found || got != seq {
		return nil, Fail("error.protocol.sequence")
	}
	result, found := CborUint(top, "r")
	if !found {
		return nil, Fail("error.protocol.answer")
	}
	data, _ := CborSub(top, "d")
	if data == nil {
		data = CborMap{}
	}
	if result != DevOK {
		return nil, deviceError(result)
	}
	return data, nil
}

// deviceError macht aus dem Ergebniswert des Geraets einen Text, den der
// Anwender versteht. Unbekannte Werte werden genannt, nicht verschwiegen.
func deviceError(result uint64) error {
	if result < uint64(len(deviceErrorKeys)) {
		return Fail(deviceErrorKeys[result])
	}
	return Fail("error.device.unknown", strconv.FormatUint(result, 10))
}

// DeviceInfo ist die Antwort auf hello.
type DeviceInfo struct {
	Firmware   string   `json:"firmware"`
	Serial     string   `json:"serial"`
	Protocol   uint64   `json:"protocol"`
	Transports []string `json:"transports"`
	Unlocked   bool     `json:"unlocked"`
	Profiles   uint64   `json:"profiles"`
}

// Hello fragt Fassung, Seriennummer und eingeschaltete Zugaenge ab.
func (c *Client) Hello() (DeviceInfo, error) {
	data, err := c.Call(CmdHello, nil, shortTimeout)
	if err != nil {
		return DeviceInfo{}, err
	}
	info := DeviceInfo{}
	info.Firmware, _ = CborText(data, "fw")
	info.Serial, _ = CborText(data, "serial")
	info.Protocol, _ = CborUint(data, "proto")
	info.Profiles, _ = CborUint(data, "profiles")
	if unlocked, ok := data["unlocked"].(bool); ok {
		info.Unlocked = unlocked
	}
	if list, ok := CborList(data, "tr"); ok {
		for _, entry := range list {
			if name, isText := entry.(string); isText {
				info.Transports = append(info.Transports, name)
			}
		}
	}
	return info, nil
}

// Unlock oeffnet den Tresor. Der Puffer mit der PIN wird hier ueberschrieben,
// sobald er geschickt ist; der Aufrufer braucht ihn danach nicht mehr.
func (c *Client) Unlock(pin []byte) error {
	defer Wipe(pin)
	data := CborMap{}
	if len(pin) > 0 {
		data["pin"] = pin
	}
	_, err := c.Call(CmdUnlock, data, readTimeout)
	return err
}

// Lock schliesst den Tresor.
func (c *Client) Lock() error {
	_, err := c.Call(CmdLock, nil, shortTimeout)
	return err
}

// List holt die Angaben aller Eintraege eines Profils - niemals Geheimnisse.
func (c *Client) List(profile uint8, query string) ([]Item, error) {
	data := CborMap{"p": uint64(profile)}
	if query != "" {
		if len(query) > MaxSiteLen {
			return nil, Fail("error.item.site")
		}
		data["q"] = query
	}
	answer, err := c.Call(CmdList, data, readTimeout)
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

func itemFromCbor(record CborMap) (Item, error) {
	id, ok := CborUint(record, "id")
	if !ok || id > 0xffffffff {
		return Item{}, Fail("error.protocol.answer")
	}
	typeNumber, ok := CborUint(record, "t")
	if !ok || typeNumber > 0xff {
		return Item{}, Fail("error.protocol.answer")
	}
	item := Item{ID: uint32(id), Type: ItemType(typeNumber)}
	if profile, found := CborUint(record, "p"); found && profile <= 0xff {
		item.Profile = uint8(profile)
	}
	item.Name, _ = CborText(record, "n")
	item.Site, _ = CborText(record, "s")
	if changed, found := CborUint(record, "ch"); found && changed <= 1<<62 {
		item.ChangedUTC = int64(changed)
	}
	if mirrored, found := record["m"].(bool); found {
		item.Mirrored = mirrored
	}
	if pairedOnly, found := record["ph"].(bool); found {
		item.PairedHostOnly = pairedOnly
	}
	if pub, found := record["pub"].([]byte); found && len(pub) > 0 {
		item.Pub = append([]byte(nil), pub...)
	}
	if !item.Type.Valid() {
		return Item{}, Fail("error.item.type")
	}
	return item, nil
}

func itemToCbor(it Item) CborMap {
	return CborMap{
		"t":  uint64(it.Type),
		"p":  uint64(it.Profile),
		"n":  it.Name,
		"s":  it.Site,
		"m":  it.Mirrored,
		"ph": it.PairedHostOnly,
	}
}

// Reveal holt ein Geheimnis. Der Aufrufer muss den Puffer mit Wipe loeschen,
// sobald er ihn nicht mehr braucht.
func (c *Client) Reveal(id uint32) ([]byte, error) {
	answer, err := c.Call(CmdReveal, CborMap{"id": uint64(id)}, readTimeout)
	if err != nil {
		return nil, err
	}
	secret, ok := CborBytes(answer, "sec")
	if !ok {
		if text, isText := CborText(answer, "sec"); isText {
			return []byte(text), nil
		}
		return nil, Fail("error.protocol.answer")
	}
	return secret, nil
}

// Add legt einen Eintrag an. secret wird nach dem Senden ueberschrieben.
func (c *Client) Add(it Item, secret []byte) (uint32, error) {
	defer Wipe(secret)
	if err := CheckItem(it); err != nil {
		return 0, err
	}
	if len(secret) > MaxSecretLen {
		return 0, Fail("error.item.secret")
	}
	data := itemToCbor(it)
	data["sec"] = secret
	answer, err := c.Call(CmdAdd, data, readTimeout)
	if err != nil {
		return 0, err
	}
	id, ok := CborUint(answer, "id")
	if !ok || id > 0xffffffff {
		return 0, Fail("error.protocol.answer")
	}
	return uint32(id), nil
}

// Update aendert einen Eintrag. Ein leeres secret laesst das Geheimnis stehen.
func (c *Client) Update(id uint32, it Item, secret []byte) error {
	defer Wipe(secret)
	if err := CheckItem(it); err != nil {
		return err
	}
	if len(secret) > MaxSecretLen {
		return Fail("error.item.secret")
	}
	data := itemToCbor(it)
	data["id"] = uint64(id)
	if len(secret) > 0 {
		data["sec"] = secret
	}
	_, err := c.Call(CmdUpdate, data, readTimeout)
	return err
}

// Delete loescht einen Eintrag.
func (c *Client) Delete(id uint32) error {
	_, err := c.Call(CmdDelete, CborMap{"id": uint64(id)}, readTimeout)
	return err
}

// LogEntry ist eine Zeile des Geraeteprotokolls.
type LogEntry struct {
	TimeUTC int64  `json:"time"`
	Event   string `json:"event"`
	AskedBy string `json:"askedBy"`
	ItemID  uint32 `json:"itemId"`
}

// Log liest das Geraeteprotokoll.
func (c *Client) Log(limit uint64) ([]LogEntry, error) {
	if limit == 0 || limit > CborMaxItems {
		limit = 100
	}
	answer, err := c.Call(CmdLog, CborMap{"n": limit}, readTimeout)
	if err != nil {
		return nil, err
	}
	raw, ok := CborList(answer, "log")
	if !ok {
		return nil, Fail("error.protocol.answer")
	}
	out := make([]LogEntry, 0, len(raw))
	for _, entry := range raw {
		record, isMap := entry.(CborMap)
		if !isMap {
			return nil, Fail("error.protocol.answer")
		}
		line := LogEntry{}
		if when, found := CborUint(record, "ts"); found && when <= 1<<62 {
			line.TimeUTC = int64(when)
		}
		line.Event, _ = CborText(record, "e")
		line.AskedBy, _ = CborText(record, "a")
		if id, found := CborUint(record, "id"); found && id <= 0xffffffff {
			line.ItemID = uint32(id)
		}
		out = append(out, line)
	}
	return out, nil
}

// PairResult ist die Antwort auf pair: die Zahl, die auf beiden Seiten gleich
// sein muss.
type PairResult struct {
	Number string `json:"number"`
	Done   bool   `json:"done"`
}

// Pair koppelt diesen Rechner oder ein zweites Geraet.
func (c *Client) Pair(kind string) (PairResult, error) {
	if kind != "host" && kind != "device" {
		return PairResult{}, Fail("error.pair.kind")
	}
	answer, err := c.Call(CmdPair, CborMap{"k": kind}, readTimeout)
	if err != nil {
		return PairResult{}, err
	}
	out := PairResult{}
	out.Number, _ = CborText(answer, "num")
	if done, ok := answer["done"].(bool); ok {
		out.Done = done
	}
	return out, nil
}

// Backup holt die verschluesselte Sicherung. Der Rechner kann sie nicht lesen.
func (c *Client) Backup() ([]byte, error) {
	answer, err := c.Call(CmdBackup, nil, readTimeout)
	if err != nil {
		return nil, err
	}
	blob, ok := CborBytes(answer, "blob")
	if !ok || len(blob) == 0 {
		return nil, Fail("error.protocol.answer")
	}
	return blob, nil
}

// Restore spielt eine Sicherung zurueck.
func (c *Client) Restore(blob []byte) error {
	if len(blob) == 0 || len(blob) > CborMaxText {
		return Fail("error.backup.size")
	}
	_, err := c.Call(CmdRestore, CborMap{"blob": blob}, readTimeout)
	return err
}

// FirmwareChunk ist die Groesse eines Stuecks beim Update. Es muss in eine
// Nachricht passen und laesst Platz fuer den Kopf.
const FirmwareChunk = 2048

// Firmware schickt ein unterschriebenes Abbild stueckweise. fortschritt wird
// nach jedem Stueck mit der Zahl der gesendeten Bytes aufgerufen.
func (c *Client) Firmware(image []byte, progress func(sent, total int)) error {
	if len(image) == 0 {
		return Fail("error.firmware.empty")
	}
	total := len(image)
	for offset := 0; offset < total; offset += FirmwareChunk {
		end := offset + FirmwareChunk
		if end > total {
			end = total
		}
		data := CborMap{
			"off":   uint64(offset),
			"total": uint64(total),
			"part":  image[offset:end],
			"last":  end == total,
		}
		if _, err := c.Call(CmdFirmware, data, readTimeout); err != nil {
			return err
		}
		if progress != nil {
			progress(end, total)
		}
	}
	return nil
}

// Attest beantwortet eine Aufgabe - damit kann ein anderes Programm pruefen,
// dass das Geraet steckt.
func (c *Client) Attest(challenge []byte) ([]byte, error) {
	if len(challenge) == 0 || len(challenge) > 256 {
		return nil, Fail("error.attest.challenge")
	}
	answer, err := c.Call(CmdAttest, CborMap{"ch": challenge}, readTimeout)
	if err != nil {
		return nil, err
	}
	signature, ok := CborBytes(answer, "sig")
	if !ok || len(signature) == 0 {
		return nil, Fail("error.protocol.answer")
	}
	return signature, nil
}
