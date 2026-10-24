// Geraete-Attrappe mit echter Rechnung, fuer die Dongle-Pruefungen.
//
// Die Attrappe in tests/simulator antwortet auf `attest` mit dem Fragewert
// XOR 0x5a - das reicht, um die Rahmen zu pruefen, aber nicht, um eine
// Unterschrift zu pruefen. Hier liegt darum eine zweite, die wirklich
// unterschreibt: echtes Ed25519, echtes X25519, echtes HKDF. Nur der
// Tastendruck ist behauptet, und das Grundgeheimnis liegt im
// Arbeitsspeicher statt in einem Chip.
//
// Sie ist ein Pruefmittel. Fuer echte Geheimnisse ist sie nicht gedacht und
// sagt das in jeder Beschreibung ("guard-fake") auch.
package classes

import (
	"crypto/ecdh"
	"crypto/ed25519"
	"crypto/hkdf"
	"crypto/rand"
	"crypto/sha256"
	"crypto/sha512"
	"errors"
	"io"
	"net"
	"strconv"
	"sync"
	"time"
)

// GuardFakeOptions stellt die Attrappe ein - vor allem die Faelle, die man
// sonst nur mit bestimmter Hardware zu sehen bekommt.
type GuardFakeOptions struct {
	// NoECDH stellt ein geliefertes Stueck nach, das im Werk auf FIPS
	// gestellt wurde und darum keinen Schluesselaustausch macht.
	NoECDH bool
	// RequireTouch verlangt je Unterschrift einen Tastendruck. Die Attrappe
	// kann keinen entgegennehmen: ohne Freischaltung antwortet sie dann
	// OSK_ERR_DENIED - genau wie ein Geraet, an dem niemand steht.
	RequireTouch bool
	// Seed macht die Schluessel wiederholbar. Leer: Zufall.
	Seed []byte
	// Pin ist die PIN der Attrappe. Leer: der Tresor ist offen.
	Pin string
}

// GuardFakeDevice ist die Attrappe. Sie beantwortet die Befehle, die der
// Dongle-Teil braucht: hello, unlock, list, reveal, attest, derive, agree,
// session, sign.
type GuardFakeDevice struct {
	mu       sync.Mutex
	opts     GuardFakeOptions
	root     []byte             // Grundgeheimnis - im Chip waere es unlesbar
	identity ed25519.PrivateKey // Geraeteschluessel fuer attest
	unlocked bool
	items    map[uint32]guardFakeItem
	nextID   uint32
	session  map[string]time.Time
	log      []LogEntry
	now      func() time.Time
}

type guardFakeItem struct {
	meta   Item
	secret []byte
}

// NewGuardFakeDevice baut eine Attrappe. Ohne Seed sind ihre Schluessel
// zufaellig und gelten nur fuer diesen Lauf.
func NewGuardFakeDevice(opts GuardFakeOptions) (*GuardFakeDevice, error) {
	seed := opts.Seed
	if len(seed) == 0 {
		seed = make([]byte, 32)
		if _, err := rand.Read(seed); err != nil {
			return nil, Fail("error.guard.random")
		}
	}
	root, err := hkdf.Key(sha512.New, seed, nil, "guard-fake/root/1", 32)
	if err != nil {
		return nil, Fail("error.guard.internal")
	}
	identitySeed, err := hkdf.Key(sha512.New, seed, nil, "guard-fake/identity/1",
		ed25519.SeedSize)
	if err != nil {
		return nil, Fail("error.guard.internal")
	}
	device := &GuardFakeDevice{
		opts:     opts,
		root:     root,
		identity: ed25519.NewKeyFromSeed(identitySeed),
		items:    map[uint32]guardFakeItem{},
		nextID:   1,
		session:  map[string]time.Time{},
		unlocked: opts.Pin == "",
		now:      time.Now,
	}
	return device, nil
}

// PublicKey ist der oeffentliche Geraeteschluessel - der Wert, den ein
// geschuetztes Programm als DevicePublicKey eintraegt.
func (d *GuardFakeDevice) PublicKey() []byte {
	out := make([]byte, GuardKeyLen)
	copy(out, d.identity.Public().(ed25519.PublicKey))
	return out
}

// AddItem legt einen Eintrag in den Tresor der Attrappe - fuer Lizenzen in
// den Pruefungen.
func (d *GuardFakeDevice) AddItem(meta Item, secret []byte) uint32 {
	d.mu.Lock()
	defer d.mu.Unlock()
	id := d.nextID
	d.nextID++
	meta.ID = id
	meta.ChangedUTC = d.now().UTC().Unix()
	copied := make([]byte, len(secret))
	copy(copied, secret)
	d.items[id] = guardFakeItem{meta: meta, secret: copied}
	return id
}

// Log gibt das Protokoll der Attrappe zurueck. Jede Unterschrift steht
// darin - das ist die Zusage aus docs/dongle.md, und sie wird geprueft.
func (d *GuardFakeDevice) Log() []LogEntry {
	d.mu.Lock()
	defer d.mu.Unlock()
	out := make([]LogEntry, len(d.log))
	copy(out, d.log)
	return out
}

// SetNow setzt die Uhr der Attrappe - damit laesst sich das Ablaufen einer
// Freischaltung pruefen, ohne zu warten.
func (d *GuardFakeDevice) SetNow(now func() time.Time) {
	d.mu.Lock()
	defer d.mu.Unlock()
	if now != nil {
		d.now = now
	}
}

func (d *GuardFakeDevice) note(event string, id uint32) {
	d.log = append(d.log, LogEntry{TimeUTC: d.now().UTC().Unix(), Event: event, ItemID: id})
	if len(d.log) > 500 {
		d.log = d.log[len(d.log)-500:]
	}
}

// nodeSeed leitet den Knotenschluessel aus dem Grundgeheimnis ab. Im Chip
// macht das osk_se050_key_derive_transient; hier dieselbe Eigenschaft:
// gleiche Bezeichnung, gleicher Schluessel, und das Grundgeheimnis bleibt,
// wo es ist.
func (d *GuardFakeDevice) nodeSeed(label, curve string) ([]byte, error) {
	seed, err := hkdf.Key(sha512.New, d.root, nil,
		"guard-fake/node/1|"+curve+"|"+label, 32)
	if err != nil {
		return nil, Fail("error.guard.internal")
	}
	return seed, nil
}

func (d *GuardFakeDevice) nodeEd25519(label string) (ed25519.PrivateKey, error) {
	seed, err := d.nodeSeed(label, "ed25519")
	if err != nil {
		return nil, err
	}
	return ed25519.NewKeyFromSeed(seed), nil
}

func (d *GuardFakeDevice) nodeX25519(label string) (*ecdh.PrivateKey, error) {
	seed, err := d.nodeSeed(label, "x25519")
	if err != nil {
		return nil, err
	}
	return ecdh.X25519().NewPrivateKey(seed)
}

// sessionAllows sagt, ob fuer diesen Schluessel gerade eine Freischaltung
// gilt. Abgelaufene Eintraege werden dabei entfernt.
func (d *GuardFakeDevice) sessionAllows(label string) bool {
	until, found := d.session[label]
	if !found {
		return false
	}
	if d.now().After(until) {
		delete(d.session, label)
		return false
	}
	return true
}

// Handle beantwortet eine Anfrage. Die Rueckgabe ist die vollstaendige
// Antwort samt v, s und r.
func (d *GuardFakeDevice) Handle(payload []byte) CborMap {
	value, err := CborDecode(payload)
	if err != nil {
		return guardFakeReply(0, 9, nil) // OSK_ERR_FORMAT
	}
	request, ok := value.(CborMap)
	if !ok {
		return guardFakeReply(0, 9, nil)
	}
	seq, _ := CborUint(request, "s")
	if version, _ := CborUint(request, "v"); version != ProtocolVersion {
		return guardFakeReply(seq, 10, nil) // OSK_ERR_UNSUPPORTED
	}
	cmd, _ := CborUint(request, "c")
	data, _ := CborSub(request, "d")
	if data == nil {
		data = CborMap{}
	}

	d.mu.Lock()
	defer d.mu.Unlock()

	switch cmd {
	case CmdHello:
		return guardFakeReply(seq, 0, CborMap{
			"fw":       "0.0.0.0-guard-fake",
			"serial":   "GUARD-FAKE-0001",
			"proto":    uint64(ProtocolVersion),
			"profiles": uint64(1),
			"unlocked": d.unlocked,
			"tr":       []any{"guard-fake"},
		})
	case CmdUnlock:
		pin, _ := CborBytes(data, "pin")
		if string(pin) != d.opts.Pin {
			d.note("unlock-denied", 0)
			return guardFakeReply(seq, 4, nil) // OSK_ERR_DENIED
		}
		d.unlocked = true
		d.note("unlock", 0)
		return guardFakeReply(seq, 0, CborMap{"unlocked": true})
	case CmdLock:
		d.unlocked = false
		d.note("lock", 0)
		return guardFakeReply(seq, 0, nil)
	case CmdAttest:
		return d.handleAttest(seq, data)
	}

	if !d.unlocked {
		return guardFakeReply(seq, 5, nil) // OSK_ERR_LOCKED
	}

	switch cmd {
	case CmdList:
		return guardFakeReply(seq, 0, CborMap{"items": d.listItems()})
	case CmdReveal:
		id, _ := CborUint(data, "id")
		entry, found := d.items[uint32(id)]
		if !found {
			return guardFakeReply(seq, 6, nil) // OSK_ERR_NOT_FOUND
		}
		d.note("reveal", uint32(id))
		return guardFakeReply(seq, 0, CborMap{"sec": entry.secret})
	case CmdLog:
		return guardFakeReply(seq, 0, CborMap{"log": d.logAsCbor()})
	case CmdDerive:
		return d.handleDerive(seq, data)
	case CmdAgree:
		return d.handleAgree(seq, data)
	case CmdSign:
		return d.handleSign(seq, data)
	case CmdSession:
		return d.handleSession(seq, data)
	}
	return guardFakeReply(seq, 10, nil) // OSK_ERR_UNSUPPORTED
}

// handleAttest unterschreibt die Frage. Der Zweck wird hier davorgesetzt,
// nicht vom Rechner mitgeschickt - sonst koennte jemand einen Auftrag als
// Frage hineingeben und bekaeme eine gueltige Auftragsunterschrift.
func (d *GuardFakeDevice) handleAttest(seq uint64, data CborMap) CborMap {
	nonce, _ := CborBytes(data, "ch")
	binHash, _ := CborBytes(data, "bh")
	if len(nonce) != GuardNonceLen || len(binHash) != GuardHashLen {
		return guardFakeReply(seq, 1, nil) // OSK_ERR_ARG
	}
	signature := ed25519.Sign(d.identity, GuardMessage(guardAttestDomain, nonce, binHash))
	d.note("attest", 0)
	return guardFakeReply(seq, 0, CborMap{"sig": signature, "pub": d.PublicKey()})
}

func (d *GuardFakeDevice) handleDerive(seq uint64, data CborMap) CborMap {
	info, _ := CborBytes(data, "info")
	length, _ := CborUint(data, "n")
	if len(info) == 0 || len(info) > 256 || length == 0 || length > 64 {
		return guardFakeReply(seq, 1, nil)
	}
	key, err := hkdf.Key(sha256.New, d.root, nil, string(info), int(length))
	if err != nil {
		return guardFakeReply(seq, 11, nil) // OSK_ERR_INTERNAL
	}
	d.note("derive", 0)
	return guardFakeReply(seq, 0, CborMap{"key": key})
}

// handleAgree ist die Stelle, an der ein FIPS-Stueck aussteigt. Es wird
// nichts hilfsweise gerechnet: OSK_ERR_UNSUPPORTED, und der Rechner macht
// daraus die Aussage, welcher Anwendungsfall wegfaellt.
func (d *GuardFakeDevice) handleAgree(seq uint64, data CborMap) CborMap {
	if d.opts.NoECDH {
		d.note("agree-unsupported", 0)
		return guardFakeReply(seq, 10, nil) // OSK_ERR_UNSUPPORTED
	}
	label, _ := CborText(data, "k")
	peer, _ := CborBytes(data, "pub")
	if label == "" || len(label) > NodeKeyLabelMax || len(peer) != NodeSecretLen {
		return guardFakeReply(seq, 1, nil)
	}
	if !d.sessionAllows(label) && d.opts.RequireTouch {
		return guardFakeReply(seq, 4, nil) // OSK_ERR_DENIED
	}
	private, err := d.nodeX25519(label)
	if err != nil {
		return guardFakeReply(seq, 11, nil)
	}
	peerKey, err := ecdh.X25519().NewPublicKey(peer)
	if err != nil {
		return guardFakeReply(seq, 1, nil)
	}
	secret, err := private.ECDH(peerKey)
	if err != nil {
		return guardFakeReply(seq, 1, nil)
	}
	d.note("agree:"+label, 0)
	return guardFakeReply(seq, 0, CborMap{"sec": secret})
}

func (d *GuardFakeDevice) handleSign(seq uint64, data CborMap) CborMap {
	label, _ := CborText(data, "k")
	if label == "" || len(label) > NodeKeyLabelMax {
		return guardFakeReply(seq, 1, nil)
	}
	if wantPublic, _ := data["pub"].(bool); wantPublic {
		curve, _ := CborText(data, "cv")
		switch curve {
		case "ed25519":
			private, err := d.nodeEd25519(label)
			if err != nil {
				return guardFakeReply(seq, 11, nil)
			}
			return guardFakeReply(seq, 0,
				CborMap{"pub": []byte(private.Public().(ed25519.PublicKey))})
		case "x25519":
			if d.opts.NoECDH {
				return guardFakeReply(seq, 10, nil)
			}
			private, err := d.nodeX25519(label)
			if err != nil {
				return guardFakeReply(seq, 11, nil)
			}
			return guardFakeReply(seq, 0, CborMap{"pub": private.PublicKey().Bytes()})
		}
		return guardFakeReply(seq, 1, nil)
	}
	jobHash, _ := CborBytes(data, "h")
	if len(jobHash) != GuardHashLen {
		return guardFakeReply(seq, 1, nil)
	}
	if !d.sessionAllows(label) && d.opts.RequireTouch {
		// Kein Mensch am Geraet und keine Freischaltung: genau der Fall, in
		// dem ein Server stillstuende.
		d.note("sign-denied:"+label, 0)
		return guardFakeReply(seq, 4, nil) // OSK_ERR_DENIED
	}
	private, err := d.nodeEd25519(label)
	if err != nil {
		return guardFakeReply(seq, 11, nil)
	}
	signature := ed25519.Sign(private, NodeSigned(label, jobHash))
	d.note("sign:"+label, 0)
	return guardFakeReply(seq, 0, CborMap{"sig": signature})
}

func (d *GuardFakeDevice) handleSession(seq uint64, data CborMap) CborMap {
	if ask, _ := data["ask"].(bool); ask {
		var keys []any
		var left uint64
		for label, until := range d.session {
			if d.now().After(until) {
				continue
			}
			keys = append(keys, label)
			if seconds := uint64(until.Sub(d.now()) / time.Second); seconds > left {
				left = seconds
			}
		}
		return guardFakeReply(seq, 0, CborMap{"on": len(keys) > 0, "k": keys, "ttl": left})
	}
	on, _ := data["on"].(bool)
	if !on {
		d.session = map[string]time.Time{}
		d.note("session-off", 0)
		return guardFakeReply(seq, 0, nil)
	}
	list, _ := CborList(data, "k")
	ttl, _ := CborUint(data, "ttl")
	if len(list) == 0 || ttl == 0 || ttl > uint64(SessionMaxTTL/time.Second) {
		return guardFakeReply(seq, 1, nil)
	}
	until := d.now().Add(time.Duration(ttl) * time.Second)
	for _, entry := range list {
		label, isText := entry.(string)
		if !isText || label == "" || len(label) > NodeKeyLabelMax {
			return guardFakeReply(seq, 1, nil)
		}
		d.session[label] = until
		d.note("session-on:"+label, 0)
	}
	return guardFakeReply(seq, 0, CborMap{"on": true, "ttl": ttl})
}

func (d *GuardFakeDevice) listItems() []any {
	out := make([]any, 0, len(d.items))
	for id := uint32(1); id < d.nextID; id++ {
		entry, found := d.items[id]
		if !found {
			continue
		}
		out = append(out, CborMap{
			"id": uint64(entry.meta.ID),
			"t":  uint64(entry.meta.Type),
			"p":  uint64(entry.meta.Profile),
			"n":  entry.meta.Name,
			"s":  entry.meta.Site,
			"ch": uint64(entry.meta.ChangedUTC),
			"m":  entry.meta.Mirrored,
			"ph": entry.meta.PairedHostOnly,
		})
	}
	return out
}

func (d *GuardFakeDevice) logAsCbor() []any {
	out := make([]any, 0, len(d.log))
	for _, line := range d.log {
		out = append(out, CborMap{
			"ts": uint64(line.TimeUTC),
			"e":  line.Event,
			"a":  line.AskedBy,
			"id": uint64(line.ItemID),
		})
	}
	return out
}

func guardFakeReply(seq, result uint64, data CborMap) CborMap {
	if data == nil {
		data = CborMap{}
	}
	return CborMap{
		"v": uint64(ProtocolVersion),
		"s": seq,
		"r": result,
		"d": data,
	}
}

// --- Verbindung ohne Netz -------------------------------------------------

// guardFakeTransport haengt die Attrappe ohne Umweg an einen Client: die
// Berichte wandern durch den Arbeitsspeicher. So brauchen die Pruefungen
// keinen Port und koennen nicht an einem besetzten scheitern.
type guardFakeTransport struct {
	mu      sync.Mutex
	device  *GuardFakeDevice
	request []byte
	replies [][]byte
	closed  bool
}

// GuardFakeClient gibt einen Client, der mit dieser Attrappe spricht.
func (d *GuardFakeDevice) GuardFakeClient(askedBy string) *Client {
	return NewClient(&guardFakeTransport{device: d}, askedBy)
}

func (t *guardFakeTransport) Describe() string { return "guard-fake" }

func (t *guardFakeTransport) Close() error {
	t.mu.Lock()
	defer t.mu.Unlock()
	t.closed = true
	t.request = nil
	t.replies = nil
	return nil
}

func (t *guardFakeTransport) WriteReport(report []byte) error {
	t.mu.Lock()
	defer t.mu.Unlock()
	if t.closed {
		return Fail("error.guard.nodevice")
	}
	if len(report) != ReportSize {
		return Fail("error.frame.size")
	}
	part, last, err := parseReport(report, len(t.request) == 0)
	if err != nil {
		return err
	}
	t.request = append(t.request, part...)
	if !last {
		return nil
	}
	payload := t.request
	t.request = nil
	out, err := CborEncode(t.device.Handle(payload))
	if err != nil {
		return err
	}
	reports, err := SplitFrames(out)
	if err != nil {
		return err
	}
	t.replies = append(t.replies, reports...)
	return nil
}

func (t *guardFakeTransport) ReadReport(timeout time.Duration) ([]byte, error) {
	t.mu.Lock()
	defer t.mu.Unlock()
	if t.closed {
		return nil, Fail("error.guard.nodevice")
	}
	if len(t.replies) == 0 {
		return nil, Fail("error.hid.timeout")
	}
	report := t.replies[0]
	t.replies = t.replies[1:]
	return report, nil
}

// Unplug stellt das Abziehen nach: danach antwortet die Verbindung nicht
// mehr, und ein geschuetztes Programm muss abbrechen.
func (t *guardFakeTransport) Unplug() { t.Close() }

// GuardFakePair gibt Client und den Hebel zum Abziehen.
func (d *GuardFakeDevice) GuardFakePair(askedBy string) (*Client, func()) {
	transport := &guardFakeTransport{device: d}
	return NewClient(transport, askedBy), transport.Unplug
}

// --- Verbindung ueber die Rueckschleife ------------------------------------

// GuardFakeListener laesst die Attrappe auf 127.0.0.1 lauschen, damit die
// Beispiele in C, PHP, Python und .NET gegen etwas laufen koennen. Nur die
// Rueckschleife; eine Attrappe im Netz waere ein offener Kanal.
type GuardFakeListener struct {
	listener net.Listener
	device   *GuardFakeDevice
	wait     sync.WaitGroup
}

// GuardFakeListen oeffnet den Port. Port 0 nimmt einen freien.
func (d *GuardFakeDevice) GuardFakeListen(port int) (*GuardFakeListener, error) {
	listener, err := net.Listen("tcp", "127.0.0.1:"+strconv.Itoa(port))
	if err != nil {
		return nil, Fail("error.guard.listen", err.Error())
	}
	out := &GuardFakeListener{listener: listener, device: d}
	out.wait.Add(1)
	go out.serve()
	return out, nil
}

// Address nennt Rechner und Port, so wie ihn SimDial erwartet.
func (l *GuardFakeListener) Address() string { return l.listener.Addr().String() }

// Close haelt an und wartet, bis keine Verbindung mehr bedient wird.
func (l *GuardFakeListener) Close() error {
	err := l.listener.Close()
	l.wait.Wait()
	return err
}

func (l *GuardFakeListener) serve() {
	defer l.wait.Done()
	for {
		conn, err := l.listener.Accept()
		if err != nil {
			return
		}
		l.wait.Add(1)
		go func() {
			defer l.wait.Done()
			l.handle(conn)
		}()
	}
}

func (l *GuardFakeListener) handle(conn net.Conn) {
	defer conn.Close()
	for {
		payload, err := guardFakeReadFrames(conn)
		if err != nil {
			return
		}
		out, err := CborEncode(l.device.Handle(payload))
		if err != nil {
			return
		}
		reports, err := SplitFrames(out)
		if err != nil {
			return
		}
		for _, report := range reports {
			if _, writeErr := conn.Write(report); writeErr != nil {
				return
			}
		}
	}
}

// guardFakeReadFrames liest eine ganze Nachricht von der Verbindung.
func guardFakeReadFrames(conn net.Conn) ([]byte, error) {
	var payload []byte
	for count := 0; count < maxReports; count++ {
		if err := conn.SetReadDeadline(time.Now().Add(10 * time.Minute)); err != nil {
			return nil, err
		}
		report := make([]byte, ReportSize)
		if _, err := io.ReadFull(conn, report); err != nil {
			return nil, err
		}
		part, last, err := parseReport(report, count == 0)
		if err != nil {
			return nil, err
		}
		payload = append(payload, part...)
		if last {
			return payload, nil
		}
	}
	return nil, errors.New("too many reports")
}
