// Die Leak-Pruefung: ist dieses Kennwort in einer bekannten Sammlung
// gestohlener Zugangsdaten aufgetaucht?
//
// DIE GANZE SACHE HAENGT AN EINEM SATZ: das Kennwort verlaesst den Rechner
// NICHT. Hinaus gehen die ersten fuenf Zeichen des Hashwerts, also 20 Bit.
// Zurueck kommt die Liste aller Hashwerte, die mit diesen fuenf Zeichen
// anfangen - typisch einige hundert bis einige tausend. Darin wird LOKAL
// gesucht. Die Gegenseite erfaehrt damit, dass jemand ein Kennwort aus
// dieser Gruppe geprueft hat, und nicht welches. Das ist k-Anonymitaet, und
// das k ist die Groesse der Gruppe.
//
// Wer stattdessen den GANZEN Hashwert schickt, hat genau das getan, wovor
// dieses Geraet schuetzen soll: die Gegenseite kann den Hashwert in ihrer
// eigenen Sammlung nachsehen und weiss dann das Kennwort. Es sieht im
// Quelltext fast gleich aus, und darum ist der Nachweis hier KEIN Blick in
// den Quelltext: tests/leak_test.go stellt einen eigenen Server hin, laesst
// diesen Code wirklich fragen und liest mit, was in der Leitung steht - und
// prueft, dass weder das Kennwort noch der Rest des Hashwerts darin
// vorkommt. Ein Kommentar wie dieser ist eine Behauptung; der Test ist die
// Messung.
//
// UND: eine Abfrage nach draussen braucht die Zustimmung des Anwenders.
// Der Plan sagt "kein Server, keine Wolke, keine Telemetrie" - dieses
// Programm fragt niemanden, solange niemand es erlaubt hat. Es gibt zwei
// Wege zur Erlaubnis, jedes Mal oder dauerhaft eingeschaltet, und keinen
// dritten. Ohne sie geht nicht einmal die Anfrage mit den fuenf Zeichen
// hinaus: LeakAllow ist nicht eine Anzeige, sondern eine Bedingung.
package classes

import (
	"crypto/sha1"
	"encoding/hex"
	"io"
	"net/http"
	"strconv"
	"strings"
	"time"
)

// LeakPrefixLen ist die Zahl der Hexzeichen, die hinausgehen. Fuenf, wie es
// der Dienst vorsieht: 20 Bit, also eine Gruppe von durchschnittlich einem
// Millionstel der Sammlung.
//
// Diese Zahl steht EINMAL hier. Sie ist die Grenze zwischen "niemand
// erfaehrt mein Kennwort" und "die Gegenseite kann es nachsehen", und sie
// darf nicht an zwei Stellen stehen, wo sie auseinanderlaufen koennte.
const LeakPrefixLen = 5

// LeakHashLen ist die Laenge eines SHA-1-Hashwerts in Hexzeichen.
const LeakHashLen = 40

// LeakDefaultURL ist der Dienst, der diese Abfrage anbietet. Er wird nur
// gefragt, wenn der Anwender es erlaubt - siehe LeakConsent.
const LeakDefaultURL = "https://api.pwnedpasswords.com"

// LeakTimeout ist die Wartezeit. Eine Leak-Pruefung, die haengt, soll nicht
// die Oberflaeche aufhalten.
const LeakTimeout = 10 * time.Second

// LeakMaxAnswer ist die groesste Antwort, die gelesen wird. Die Antwort
// kommt von aussen und ist damit eine fremde Eingabe wie jede andere: ohne
// Obergrenze waere sie der Weg, diesem Programm den Speicher zu fuellen.
const LeakMaxAnswer = 8 << 20

// LeakConsent ist die Erlaubnis, nach draussen zu fragen.
//
// Allowed ist das Ja fuer DIESE Pruefung. Remember sagt nur, ob es in den
// Einstellungen bleiben soll - es ersetzt das Ja nicht. Ein voreingestelltes
// Ja gibt es nicht; die Nullform dieser Struktur fragt niemanden.
type LeakConsent struct {
	Allowed  bool
	Remember bool
}

// LeakResult ist die Antwort fuer ein Kennwort.
type LeakResult struct {
	// Count ist, wie oft das Kennwort in der Sammlung steht. 0: nicht
	// gefunden - was NICHT "sicher" heisst, nur "nicht in dieser Sammlung".
	Count uint64 `json:"count"`
	// GroupSize ist das k: wie viele Hashwerte die Antwort enthielt. Es
	// wird genannt, weil es der Wert ist, der die Zusage traegt - eine
	// Gruppe von drei waere keine Anonymitaet, und dann soll man es sehen.
	GroupSize int `json:"groupSize"`
	// Sent ist, was wirklich hinausgegangen ist. Es steht hier, damit die
	// Oberflaeche es anzeigen kann: nicht "wir pruefen sicher", sondern
	// die fuenf Zeichen selbst.
	Sent string `json:"sent"`
}

// LeakRange holt eine Gruppe von Hashwerten. Als Schnittstelle, damit die
// Pruefungen einen eigenen Dienst hinstellen koennen - aber NICHT, um den
// Nachweis zu ersetzen: was diese Schnittstelle bekommt, ist schon nur der
// Vorsatz, und ein Test dagegen wuerde nichts beweisen. Der Nachweis liegt
// eine Stufe tiefer, am echten Server. Siehe den Kopf dieser Datei.
type LeakRange interface {
	Range(prefix string) (io.ReadCloser, error)
}

// LeakChecker fragt den Dienst.
type LeakChecker struct {
	// BaseURL ist die Adresse ohne Pfad. Leer: LeakDefaultURL.
	BaseURL string
	// Source ersetzt den Weg nach draussen. Leer: HTTP.
	Source LeakRange
	// Client ist der HTTP-Weg. Leer: ein eigener mit LeakTimeout.
	Client *http.Client
}

// Check prueft ein Kennwort.
//
// Die Reihenfolge ist die Zusage: ERST die Erlaubnis, DANN der Hashwert,
// dann die fuenf Zeichen hinaus. Der Puffer mit dem Kennwort wird
// ueberschrieben, bevor diese Funktion zurueckkommt - der Aufrufer braucht
// ihn danach nicht mehr.
func (c *LeakChecker) Check(password []byte, consent LeakConsent) (LeakResult, error) {
	defer Wipe(password)
	if !consent.Allowed {
		return LeakResult{}, Fail("error.leak.noconsent")
	}
	if len(password) == 0 {
		return LeakResult{}, Fail("error.leak.empty")
	}

	// SHA-1 ist hier nicht die Wahl dieses Programms, sondern die des
	// Dienstes: seine Sammlung ist in SHA-1 abgelegt. Das ist in Ordnung,
	// denn es wird nichts geschuetzt - der Hashwert ist das Suchwort, und
	// seine Schwaeche gegen Kollisionen spielt dafuer keine Rolle. Zu
	// schuetzen hat ihn das Abschneiden auf fuenf Zeichen, nicht die
	// Hashfunktion.
	sum := sha1.Sum(password)
	full := strings.ToUpper(hex.EncodeToString(sum[:]))

	// HIER ist die Trennung, um die es geht. Was hinausgeht, ist prefix.
	// Was hierbleibt, ist rest. Unter diesen zwei Zeilen darf keine stehen,
	// die full weitergibt.
	prefix := full[:LeakPrefixLen]
	rest := full[LeakPrefixLen:]

	body, err := c.fetch(prefix)
	if err != nil {
		return LeakResult{}, err
	}
	defer body.Close()

	count, group, err := leakSearch(body, rest)
	if err != nil {
		return LeakResult{}, err
	}
	return LeakResult{Count: count, GroupSize: group, Sent: prefix}, nil
}

// fetch holt die Gruppe.
func (c *LeakChecker) fetch(prefix string) (io.ReadCloser, error) {
	if c.Source != nil {
		return c.Source.Range(prefix)
	}
	base := strings.TrimSuffix(strings.TrimSpace(c.BaseURL), "/")
	if base == "" {
		base = LeakDefaultURL
	}
	// Die Adresse wird aus dem Vorsatz gebaut, und der besteht aus fuenf
	// Hexzeichen - er kann keinen Pfad und keine Abfrage enthalten. Eine
	// Pruefung trotzdem, weil die Zusage dieser Datei an genau dieser Zeile
	// haengt: wer hier spaeter etwas anderes einsetzt, soll anstossen.
	if len(prefix) != LeakPrefixLen || !leakIsHex(prefix) {
		return nil, Fail("error.leak.internal")
	}
	request, err := http.NewRequest(http.MethodGet, base+"/range/"+prefix, nil)
	if err != nil {
		return nil, Fail("error.leak.request")
	}
	// Auffuellen auf eine gleiche Laenge. Ohne das verraet die GROESSE der
	// Antwort noch etwas ueber die Gruppe, auch wenn der Inhalt es nicht
	// tut - und wer mitliest, sieht die Laenge immer.
	request.Header.Set("Add-Padding", "true")
	request.Header.Set("User-Agent", "osk")
	client := c.Client
	if client == nil {
		client = &http.Client{Timeout: LeakTimeout}
	}
	answer, err := client.Do(request)
	if err != nil {
		return nil, Fail("error.leak.offline")
	}
	if answer.StatusCode != http.StatusOK {
		answer.Body.Close()
		return nil, Fail("error.leak.answer", strconv.Itoa(answer.StatusCode))
	}
	return answer.Body, nil
}

// leakSearch durchsucht die Antwort. LOKAL - das ist die zweite Haelfte der
// k-Anonymitaet: die Gegenseite hat nicht gesagt, welcher Hashwert gemeint
// war, sie hat alle geschickt.
//
// Die Antwort ist eine fremde Eingabe: Zeilen in beliebiger Form, Zahlen
// beliebiger Groesse, Zeichen, die nicht hexadezimal sind. Nichts davon
// darf einen Fehler ergeben, der mehr ist als "unbrauchbare Antwort".
func leakSearch(body io.Reader, rest string) (uint64, int, error) {
	data, err := io.ReadAll(io.LimitReader(body, LeakMaxAnswer+1))
	if err != nil {
		return 0, 0, Fail("error.leak.offline")
	}
	if len(data) > LeakMaxAnswer {
		return 0, 0, Fail("error.leak.toobig")
	}
	var found uint64
	group := 0
	for _, line := range strings.Split(string(data), "\n") {
		line = strings.TrimSpace(line)
		if line == "" {
			continue
		}
		suffix := line
		countText := ""
		if colon := strings.IndexByte(line, ':'); colon >= 0 {
			suffix = line[:colon]
			countText = line[colon+1:]
		}
		if len(suffix) != LeakHashLen-LeakPrefixLen || !leakIsHex(suffix) {
			// Keine Zeile dieser Gruppe. Das Auffuellen des Dienstes
			// schickt absichtlich Zeilen mit, die nichts bedeuten -
			// sie werden nicht mitgezaehlt und nicht als Fehler
			// genommen.
			continue
		}
		group++
		// Vergleich ohne Ruecksicht auf Gross- und Kleinschreibung: der
		// Dienst schreibt gross, aber das ist nirgends zugesagt.
		if !strings.EqualFold(suffix, rest) {
			continue
		}
		number, err := strconv.ParseUint(strings.TrimSpace(countText), 10, 64)
		if err != nil {
			// Gefunden, aber die Zahl ist unlesbar. Dann ist die
			// Antwort "gefunden" und nicht "null" - eine kaputte Zahl
			// darf nicht zu "nicht betroffen" werden.
			number = 1
		}
		found = number
	}
	if group == 0 {
		return 0, 0, Fail("error.leak.answer", "0")
	}
	return found, group, nil
}

func leakIsHex(text string) bool {
	for i := 0; i < len(text); i++ {
		c := text[i]
		switch {
		case c >= '0' && c <= '9':
		case c >= 'A' && c <= 'F':
		case c >= 'a' && c <= 'f':
		default:
			return false
		}
	}
	return len(text) > 0
}

// --- Viele Eintraege auf einmal -----------------------------------------------

// LeakFinding ist das Ergebnis fuer einen Eintrag des Tresors.
type LeakFinding struct {
	ID    uint32 `json:"id"`
	Name  string `json:"name"`
	Count uint64 `json:"count"`
	// ReasonKey steht, wenn dieser Eintrag nicht geprueft werden konnte -
	// weil das Geraet ihn nicht herausgibt, etwa. Auch hier wird nichts
	// stillschweigend uebersprungen: ein Tresor, in dem die Haelfte
	// ungeprueft blieb, darf nicht wie ein geprueften aussehen.
	ReasonKey string `json:"reason,omitempty"`
}

// LeakScan prueft die Kennwoerter eines Profils.
//
// Jeder Eintrag kostet ein `reveal`, also einen Tastendruck - das ist nicht
// zu umgehen und soll es nicht sein. Wer das nicht will, prueft einzeln.
// Geprueft werden nur Arten, deren Geheimnis ein Kennwort IST: ein
// SSH-Schluessel in dieser Sammlung zu suchen waere sinnlos.
func LeakScan(client *Client, checker *LeakChecker, profile uint8,
	consent LeakConsent) ([]LeakFinding, error) {
	if !consent.Allowed {
		return nil, Fail("error.leak.noconsent")
	}
	if client == nil || checker == nil {
		return nil, Fail("error.guard.nodevice")
	}
	items, err := client.List(profile, "")
	if err != nil {
		return nil, err
	}
	var out []LeakFinding
	for _, meta := range items {
		if meta.Type != ItemLogin {
			continue
		}
		finding := LeakFinding{ID: meta.ID, Name: meta.Name}
		if !meta.Mirrored {
			finding.ReasonKey = "cxf.left.chipbound"
			out = append(out, finding)
			continue
		}
		secret, err := client.Reveal(meta.ID)
		if err != nil {
			finding.ReasonKey = cxfRefusalKey(err)
			out = append(out, finding)
			continue
		}
		result, err := checker.Check(secret, consent)
		if err != nil {
			finding.ReasonKey = cxfRefusalKey(err)
			out = append(out, finding)
			continue
		}
		finding.Count = result.Count
		out = append(out, finding)
	}
	return out, nil
}
