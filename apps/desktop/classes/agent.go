// Der ssh-Agent: ssh, git, scp und Ansible reden mit ihm, und er holt die
// Unterschrift vom Geraet.
//
// Das Protokoll ist das des OpenSSH-Agenten (draft-miller-ssh-agent). Jede
// Nachricht ist eine Laenge in vier Bytes und dahinter ein Typbyte mit Rumpf.
// Gebraucht werden genau zwei Fragen: "welche Schluessel hast du" und
// "unterschreibe das hier". Alles, was Schluessel in den Agenten legen oder
// aus ihm entfernen will, wird abgelehnt - der Tresor ist das Geraet, nicht
// dieser Prozess.
//
// Was der Agent NIE tut: einen privaten Schluessel behalten. Er kennt die
// oeffentlichen Teile; fuer jede einzelne Unterschrift holt er das Geheimnis
// neu vom Geraet, benutzt es und ueberschreibt es. Darum liegt bei einem
// Absturz nichts im Speicherabbild, und darum sieht man jede Unterschrift
// einzeln im Geraeteprotokoll.
package classes

import (
	"encoding/binary"
	"errors"
	"io"
	"net"
	"sync"
	"time"
)

// Die Nachrichtentypen des Agentenprotokolls.
const (
	agentFailure           = 5
	agentSuccess           = 6
	agentRequestIdentities = 11
	agentIdentitiesAnswer  = 12
	agentSignRequest       = 13
	agentSignResponse      = 14
	agentAddIdentity       = 17
	agentRemoveIdentity    = 18
	agentRemoveAll         = 19
	agentAddIDConstrained  = 25
	agentLock              = 22
	agentUnlock            = 23
	agentExtension         = 27
	agentExtensionFailure  = 28
)

// Grenzen des Agenten.
const (
	// agentMaxMessage ist die groesste Nachricht, die angenommen wird.
	// OpenSSH selbst begrenzt auf 256 kB; mehr braucht niemand, und eine
	// unbegrenzte Laengenangabe waere eine Einladung.
	agentMaxMessage = 256 * 1024
	// agentIdleTimeout beendet eine Verbindung, auf der nichts passiert.
	agentIdleTimeout = 10 * time.Minute
	// AgentName ist der Name, den der Agent dem Geraet nennt, wenn das
	// System nicht sagt, welches Programm fragt.
	AgentName = "ssh-agent"
)

// agentIdentity ist ein Schluessel, so wie der Agent ihn kennt: nur der
// oeffentliche Teil und die Nummer des Eintrags im Tresor.
type agentIdentity struct {
	blob    []byte
	comment string
	itemID  uint32
}

// Agent beantwortet die Fragen von ssh. Er haelt keine Geheimnisse.
type Agent struct {
	app *App

	mu         sync.Mutex
	identities []agentIdentity
	primed     bool
}

// NewAgent baut den Agenten auf dem Kern des Programms auf - dieselbe
// Verbindung, dieselben Pruefungen.
func NewAgent(app *App) *Agent {
	return &Agent{app: app}
}

// access gibt einen Zugang unter dem uebergebenen Namen.
func (a *Agent) access(askedBy string) (*Access, error) {
	client, err := a.app.Client()
	if err != nil {
		return nil, err
	}
	return NewAccess(client, askedBy), nil
}

// AgentAskedBy baut den Namen, der im Geraeteprotokoll steht. Nennt das
// System das fragende Programm, steht es dabei - und davor immer der Agent,
// damit im Protokoll steht, auf welchem Weg gefragt wurde. Darum liest sich
// eine Zeile als "ssh-agent: ssh.exe": der Weg, dann das Programm.
func AgentAskedBy(peer string) string {
	if peer == "" {
		return AgentName
	}
	return AgentName + ": " + peer
}

// Forget vergisst die bekannten Identitaeten. Nach dem Sperren muss der
// Agent neu fragen, sonst nennt er Schluessel eines zugesperrten Tresors.
func (a *Agent) Forget() {
	a.mu.Lock()
	defer a.mu.Unlock()
	a.identities = nil
	a.primed = false
}

// Identities nennt die Schluessel des Tresors.
//
// Hier steckt eine Luecke des Protokolls: `list` liefert Metadaten, aber
// nicht den oeffentlichen Teil eines Schluessels. ssh will die Liste der
// Identitaeten aber wissen, BEVOR unterschrieben wird. Darum holt der Agent
// beim ersten Mal jedes Geheimnis einmal, rechnet den oeffentlichen Teil
// daraus und ueberschreibt den privaten sofort - also ein Tastendruck je
// Schluessel, nur um den Namen zu erfahren. Traegt `list` eines Tages ein
// Feld mit dem oeffentlichen Teil, faellt dieser Schritt weg.
func (a *Agent) Identities(askedBy string) ([]agentIdentity, error) {
	a.mu.Lock()
	if a.primed {
		out := a.identities
		a.mu.Unlock()
		return out, nil
	}
	a.mu.Unlock()

	access, err := a.access(askedBy)
	if err != nil {
		return nil, err
	}
	items, err := access.ItemsOfType(ItemSSHKey)
	if err != nil {
		return nil, err
	}
	found := make([]agentIdentity, 0, len(items))
	for _, item := range items {
		secret, revealErr := access.Reveal(item.ID)
		if revealErr != nil {
			// Ein Schluessel, den das Geraet nicht herausgibt, fehlt eben
			// in der Liste. Die anderen bleiben brauchbar.
			continue
		}
		key, parseErr := ParseSSHKey(secret)
		Wipe(secret)
		if parseErr != nil {
			continue
		}
		found = append(found, agentIdentity{
			blob:    key.Blob,
			comment: item.Name,
			itemID:  item.ID,
		})
		key.Wipe()
	}
	a.mu.Lock()
	a.identities = found
	a.primed = true
	a.mu.Unlock()
	return found, nil
}

// Sign holt das Geheimnis, unterschreibt und ueberschreibt es wieder.
func (a *Agent) Sign(askedBy string, blob, data []byte, flags uint32) ([]byte, error) {
	identities, err := a.Identities(askedBy)
	if err != nil {
		return nil, err
	}
	var want *agentIdentity
	for i := range identities {
		if constantEqual(identities[i].blob, blob) {
			want = &identities[i]
			break
		}
	}
	if want == nil {
		return nil, Fail("error.agent.unknownkey")
	}
	access, err := a.access(askedBy)
	if err != nil {
		return nil, err
	}
	secret, err := access.Reveal(want.itemID)
	if err != nil {
		return nil, err
	}
	key, err := ParseSSHKey(secret)
	Wipe(secret)
	if err != nil {
		return nil, err
	}
	defer key.Wipe()
	// Gegenprobe: das Geraet muss denselben Schluessel geliefert haben, der
	// hier angeboten wurde. Sonst wuerde mit einem anderen Schluessel
	// unterschrieben, als ssh gemeint hat.
	if !constantEqual(key.Blob, blob) {
		return nil, Fail("error.agent.unknownkey")
	}
	return key.Sign(data, flags)
}

// constantEqual vergleicht ohne fruehen Abbruch. Bei oeffentlichen Teilen
// ist das nicht noetig, aber es kostet nichts und die Gewohnheit ist richtig.
func constantEqual(a, b []byte) bool {
	if len(a) != len(b) {
		return false
	}
	var diff byte
	for i := range a {
		diff |= a[i] ^ b[i]
	}
	return diff == 0
}

// Serve bedient eine Verbindung, bis sie endet. peer ist der Name, den das
// System fuer das fragende Programm nennt - leer, wenn es keinen nennt.
func (a *Agent) Serve(conn net.Conn, peer string) {
	defer conn.Close()
	askedBy := AgentAskedBy(peer)
	for {
		if err := conn.SetDeadline(time.Now().Add(agentIdleTimeout)); err != nil {
			return
		}
		request, err := readAgentMessage(conn)
		if err != nil {
			return
		}
		answer := a.handle(request, askedBy)
		if err := writeAgentMessage(conn, answer); err != nil {
			return
		}
	}
}

// readAgentMessage liest eine Nachricht: Laenge, dann Rumpf.
func readAgentMessage(reader io.Reader) ([]byte, error) {
	var header [4]byte
	if _, err := io.ReadFull(reader, header[:]); err != nil {
		return nil, err
	}
	size := binary.BigEndian.Uint32(header[:])
	if size == 0 || size > agentMaxMessage {
		return nil, errors.New("agent: Nachrichtenlaenge unbrauchbar")
	}
	body := make([]byte, size)
	if _, err := io.ReadFull(reader, body); err != nil {
		return nil, err
	}
	return body, nil
}

// writeAgentMessage schreibt eine Nachricht mit Laenge davor.
func writeAgentMessage(writer io.Writer, body []byte) error {
	out := make([]byte, 4+len(body))
	binary.BigEndian.PutUint32(out, uint32(len(body)))
	copy(out[4:], body)
	_, err := writer.Write(out)
	return err
}

// handle beantwortet eine Nachricht. Fehler werden nie im Wortlaut nach
// aussen gegeben: ssh kann mit nichts anderem als "Fehlschlag" umgehen, und
// der Grund gehoert ins Protokoll des Programms, nicht auf die Leitung.
func (a *Agent) handle(request []byte, askedBy string) []byte {
	if len(request) == 0 {
		return []byte{agentFailure}
	}
	switch request[0] {
	case agentRequestIdentities:
		if len(request) != 1 {
			return []byte{agentFailure}
		}
		return a.answerIdentities(askedBy)
	case agentSignRequest:
		return a.answerSign(request[1:], askedBy)
	case agentAddIdentity, agentAddIDConstrained, agentRemoveIdentity, agentRemoveAll:
		// Der Tresor ist das Geraet. Ein Programm, das hier Schluessel
		// hinzufuegen oder loeschen will, bekommt ein klares Nein statt
		// eines stillen Erfolgs - das Geraet entscheidet, nie das Programm.
		return []byte{agentFailure}
	case agentLock, agentUnlock:
		// Gesperrt und entsperrt wird am Geraet, nicht hier.
		return []byte{agentFailure}
	case agentExtension:
		return []byte{agentExtensionFailure}
	}
	return []byte{agentFailure}
}

// answerIdentities nennt die Schluessel: Anzahl, dann je Schluessel der
// oeffentliche Teil und der Name des Eintrags.
func (a *Agent) answerIdentities(askedBy string) []byte {
	identities, err := a.Identities(askedBy)
	if err != nil {
		return []byte{agentFailure}
	}
	out := &wireWriter{}
	out.Byte(agentIdentitiesAnswer)
	out.Uint32(uint32(len(identities)))
	for _, id := range identities {
		out.String(id.blob)
		out.Name(id.comment)
	}
	return out.Bytes()
}

// answerSign liest den Wunsch und gibt die Unterschrift zurueck.
func (a *Agent) answerSign(body []byte, askedBy string) []byte {
	reader := newWireReader(body)
	blob, err := reader.String()
	if err != nil {
		return []byte{agentFailure}
	}
	data, err := reader.String()
	if err != nil {
		return []byte{agentFailure}
	}
	flags, err := reader.Uint32()
	if err != nil {
		return []byte{agentFailure}
	}
	if !reader.Done() {
		// Ein Rest heisst: die Gegenseite meint etwas anderes als wir lesen.
		return []byte{agentFailure}
	}
	signature, err := a.Sign(askedBy, blob, data, flags)
	if err != nil {
		return []byte{agentFailure}
	}
	out := &wireWriter{}
	out.Byte(agentSignResponse)
	out.String(signature)
	return out.Bytes()
}
