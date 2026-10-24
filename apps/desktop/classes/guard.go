// Dongle-Schutz, Stufe 1: Frage und Antwort. Das Programm wuerfelt einen
// Wert, das Geraet unterschreibt ihn, das Programm prueft die Unterschrift
// gegen den eingetragenen oeffentlichen Schluessel des Geraets.
//
// Was diese Stufe wert ist, steht in docs/dongle.md - kurz: sie haelt das
// Mitnehmen einer Kopie auf und nichts darueber hinaus. Wer das Programm mit
// einem Disassembler aufmacht, schneidet die Abfrage heraus. Echter Schutz
// ist allein Stufe 2 (guard_seal.go): ein Schluessel, der den Stick nie
// verlaesst, und ohne den Teile des Programms gar nicht lesbar sind.
package classes

import (
	"context"
	"crypto/ed25519"
	"crypto/rand"
	"crypto/sha256"
	"io"
	"os"
	"time"
)

// Die Befehle, die der Dongle-Teil zum Protokoll aus shared/protocol.md
// hinzufuegt. 14 ist dort schon vergeben, 15 bis 18 schlaegt dieser Strang
// vor; die Begruendung je Befehl steht in docs/dongle.md.
const (
	CmdDerive  = 15 // Schluessel im Chip ableiten (osk_se050_hkdf)
	CmdAgree   = 16 // Schluesselaustausch (osk_se050_ecdh) - kann fehlen
	CmdSession = 17 // Freischaltung ohne Tastendruck je Vorgang
	CmdSign    = 18 // Auftrag unterschreiben, getrennt von attest
)

// Trennzeichenketten. Jede Unterschrift des Geraets nennt ihren Zweck, damit
// eine Antwort aus einem Zweck in einem anderen nichts wert ist. Das Geraet
// setzt sie selbst davor - deshalb kann `attest` keinen fremden Auftrag
// unterschreiben, auch wenn jemand den Auftrag als Frage hineingibt.
const (
	guardAttestDomain = "osk-guard/attest/1"
	guardSignDomain   = "osk-guard/sign/1"
	guardSealDomain   = "osk-guard/seal/1"

	// Der alte Aufruf Client.Attest() schickt nur eine Aufgabe beliebiger
	// Laenge und keine Pruefsumme. Er bekommt einen EIGENEN Zweck, und das
	// ist nicht Ordnungsliebe, sondern ein Loch, das sonst offen stuende:
	//
	// Beide Wege wuerden "Zweck 0x00 <Aufgabe> <Pruefsumme>" unterschreiben.
	// Eine Aufgabe von 64 Byte ist aber genau so lang wie Frage und
	// Pruefsumme zusammen - wer also den alten Weg mit
	// <Frage>||<Pruefsumme> aufruft, bekommt eine Unterschrift, die der
	// neue Weg fuer gueltig haelt. Damit haette ein geaendertes Programm
	// wieder gueltige Antworten bekommen, solange der echte Stick steckt:
	// die Bindung an die Programmdatei waere weg.
	//
	// Mit zwei verschiedenen Zwecken kann keine Unterschrift des einen
	// Weges im anderen gelten. Ein Fall prueft das.
	guardAttestRawDomain = "osk-guard/attest-raw/1"
)

// Groessen des Verfahrens.
const (
	GuardNonceLen = 32 // Laenge der Frage
	GuardHashLen  = sha256.Size
	GuardKeyLen   = ed25519.PublicKeySize
	GuardSigLen   = ed25519.SignatureSize

	guardMaxBinary = 512 << 20 // 512 MiB: darueber ist es kein Programm mehr
)

// GuardConfig ist, was ein geschuetztes Programm einstellen muss.
type GuardConfig struct {
	// DevicePublicKey ist der oeffentliche Schluessel des Geraets, 32 Byte.
	// Den traegt der Hersteller des geschuetzten Programms ein; er liest ihn
	// einmal mit `osk guard-key` aus seinem eigenen Stick.
	DevicePublicKey []byte

	// IssuerPublicKey prueft Lizenzen, 32 Byte. Leer: keine Lizenzpruefung.
	IssuerPublicKey []byte

	// BinaryPath ist die Programmdatei, deren Pruefsumme in die Frage
	// eingeht. Leer: die laufende Datei.
	BinaryPath string

	// NoBinaryBinding schaltet die Bindung an die Programmdatei AUS.
	//
	// Die Frage steht hier bewusst verneint, damit der leere Wert der
	// sichere ist: wer GuardConfig selbst zusammensetzt und dieses Feld
	// vergisst, bekommt die Bindung - nicht umgekehrt. Aus ist nur fuer
	// Faelle gedacht, in denen die Programmdatei zur Laufzeit nicht
	// lesbar ist (manche Paketformate) oder in denen ein Dienst fuer
	// fremde Programme antwortet und die Pruefsumme von dort mitkommt.
	NoBinaryBinding bool

	// Interval ist der Abstand zwischen zwei Fragen in Watch.
	Interval time.Duration
}

// DefaultGuardConfig gibt die Vorgaben: Bindung an die Programmdatei an,
// alle zwei Sekunden eine neue Frage.
func DefaultGuardConfig() GuardConfig {
	return GuardConfig{Interval: 2 * time.Second}
}

// Guard ist der Dongle-Schutz auf einer bestehenden Verbindung.
type Guard struct {
	client  *Client
	cfg     GuardConfig
	binHash []byte
}

// NewGuard prueft die Einstellungen und berechnet die Pruefsumme der
// Programmdatei einmal beim Start. Ein Fehler hier heisst: der Schutz ist
// nicht einsatzbereit - und dann laeuft das Programm nicht los.
func NewGuard(client *Client, cfg GuardConfig) (*Guard, error) {
	if client == nil {
		return nil, Fail("error.guard.nodevice")
	}
	if len(cfg.DevicePublicKey) != GuardKeyLen {
		return nil, Fail("error.guard.key")
	}
	if len(cfg.IssuerPublicKey) != 0 && len(cfg.IssuerPublicKey) != GuardKeyLen {
		return nil, Fail("error.guard.issuer")
	}
	if cfg.Interval <= 0 {
		cfg.Interval = DefaultGuardConfig().Interval
	}
	g := &Guard{client: client, cfg: cfg}
	if cfg.NoBinaryBinding {
		g.binHash = make([]byte, GuardHashLen)
	} else {
		hash, err := guardBinaryHash(cfg.BinaryPath)
		if err != nil {
			return nil, err
		}
		g.binHash = hash
	}
	return g, nil
}

// BinaryHash nennt die Pruefsumme, die in jede Frage eingeht. Sie steht hier,
// damit der Hersteller sie beim Ausliefern vergleichen kann.
func (g *Guard) BinaryHash() []byte {
	out := make([]byte, len(g.binHash))
	copy(out, g.binHash)
	return out
}

// guardBinaryHash liest die Programmdatei stueckweise - eine Datei wird
// nicht in den Arbeitsspeicher geholt, nur um sie zu zaehlen.
func guardBinaryHash(path string) ([]byte, error) {
	if path == "" {
		exe, err := os.Executable()
		if err != nil {
			return nil, Fail("error.guard.binary")
		}
		path = exe
	}
	file, err := os.Open(path)
	if err != nil {
		return nil, Fail("error.guard.binary")
	}
	defer file.Close()
	info, err := file.Stat()
	if err != nil || info.IsDir() || info.Size() > guardMaxBinary {
		return nil, Fail("error.guard.binary")
	}
	digest := sha256.New()
	if _, err := io.Copy(digest, io.LimitReader(file, guardMaxBinary)); err != nil {
		return nil, Fail("error.guard.binary")
	}
	return digest.Sum(nil), nil
}

// GuardMessage baut die Frage, die unterschrieben wird: Zweck, Zufallswert,
// Pruefsumme der Programmdatei. Sie steht hier als eine Funktion, weil beide
// Seiten genau dasselbe bilden muessen - Geraet und Pruefer.
func GuardMessage(domain string, nonce, binHash []byte) []byte {
	out := make([]byte, 0, len(domain)+1+len(nonce)+len(binHash))
	out = append(out, domain...)
	out = append(out, 0)
	out = append(out, nonce...)
	out = append(out, binHash...)
	return out
}

// GuardAnswer ist eine Runde Frage und Antwort, so wie sie auch ueber den
// lokalen Dienst geht.
type GuardAnswer struct {
	Nonce     []byte `json:"nonce"`
	BinHash   []byte `json:"binHash"`
	Signature []byte `json:"signature"`
	PublicKey []byte `json:"publicKey"`
}

// Ask stellt eine neue Frage und holt die Antwort - ohne sie zu pruefen.
// Das Pruefen macht GuardVerify, und zwar beim Aufrufer: eine Antwort, die
// jemand anders schon geprueft hat, ist keine Pruefung.
func (g *Guard) Ask() (GuardAnswer, error) {
	nonce := make([]byte, GuardNonceLen)
	if _, err := rand.Read(nonce); err != nil {
		return GuardAnswer{}, Fail("error.guard.random")
	}
	return g.AskWith(nonce, g.binHash)
}

// GuardVerify prueft eine Antwort gegen den eingetragenen Schluessel. Sie
// steht als freie Funktion da, damit auch ein Programm sie benutzen kann,
// das die Antwort ueber den lokalen Dienst bekommt und selbst nachrechnet.
func GuardVerify(devicePublicKey []byte, answer GuardAnswer) error {
	if len(devicePublicKey) != GuardKeyLen {
		return Fail("error.guard.key")
	}
	if len(answer.PublicKey) != GuardKeyLen ||
		!constantEqual(devicePublicKey, answer.PublicKey) {
		return Fail("error.guard.wrongdevice")
	}
	if len(answer.Nonce) != GuardNonceLen || len(answer.BinHash) != GuardHashLen {
		return Fail("error.guard.answer")
	}
	if len(answer.Signature) != GuardSigLen {
		return Fail("error.guard.answer")
	}
	message := GuardMessage(guardAttestDomain, answer.Nonce, answer.BinHash)
	if !ed25519.Verify(ed25519.PublicKey(devicePublicKey), message, answer.Signature) {
		return Fail("error.guard.signature")
	}
	return nil
}

// constantEqual steht in agent.go - derselbe Vergleich, von einem anderen
// Strang zuerst geschrieben. Beim Zusammenfuehren standen beide Fassungen
// da, wortgleich im Verhalten; meine ist weg, weil zwei gleiche Funktionen
// in einem Paket nicht uebersetzen und die dortige mehr Aufrufer hat.

// Present ist eine vollstaendige Runde: fragen und pruefen. Kommt hier kein
// Fehler, steckte das richtige Geraet in diesem Augenblick - und die
// Programmdatei ist dieselbe wie beim Start.
func (g *Guard) Present() error {
	answer, err := g.Ask()
	if err != nil {
		return err
	}
	return GuardVerify(g.cfg.DevicePublicKey, answer)
}

// Watch fragt in festem Abstand weiter und ruft lost auf, sobald eine Runde
// scheitert - beim Abziehen des Sticks also beim naechsten Durchgang.
// Danach kehrt Watch zurueck; es versucht es nicht noch einmal. Ein
// geschuetztes Programm beendet sich in lost.
func (g *Guard) Watch(ctx context.Context, lost func(error)) {
	ticker := time.NewTicker(g.cfg.Interval)
	defer ticker.Stop()
	for {
		select {
		case <-ctx.Done():
			return
		case <-ticker.C:
			if err := g.Present(); err != nil {
				if lost != nil {
					lost(err)
				}
				return
			}
		}
	}
}
