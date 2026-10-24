// osk-guard fuer Go: ein geschuetztes Programm in wenigen Zeilen.
//
//	guard, err := oskguard.Open("127.0.0.1:8300", schluessel)
//	if err != nil { ... }                       // kein Stick: nicht loslaufen
//	if err := guard.Present(""); err != nil { ... }
//
// Die Bibliothek haengt am lokalen Dienst des Programms `osk` und prueft die
// Unterschrift SELBST. Dem Dienst wird kein Ja geglaubt - sonst waere der
// Schutz die Aussage eines Programms ueber sich selbst.
//
// Nur Standardbibliothek, kein Fremdcode.
package oskguard

import (
	"bufio"
	"crypto/ed25519"
	"crypto/rand"
	"crypto/sha256"
	"encoding/hex"
	"errors"
	"io"
	"net"
	"os"
	"strconv"
	"strings"
	"time"
)

// Groessen des Verfahrens - dieselben wie im Geraet.
const (
	NonceLen  = 32
	HashLen   = 32
	KeyLen    = 32
	SigLen    = 64
	SecretLen = 32

	attestDomain = "osk-guard/attest/1"
	signDomain   = "osk-guard/sign/1"

	maxBinary = 512 << 20
	maxLine   = 4 << 20
)

// Error ist ein Fehler des Dongle-Schutzes. Code ist der Schluessel, zu dem
// in lang/<sprache>.json ein Satz steht - derselbe Schluessel in allen fuenf
// Bibliotheken.
type Error struct {
	Code string
}

func (e *Error) Error() string { return e.Code }

func fail(code string) error { return &Error{Code: code} }

// Code nennt den Fehlerschluessel, oder "" bei einem fremden Fehler.
func Code(err error) string {
	var own *Error
	if errors.As(err, &own) {
		return own.Code
	}
	return ""
}

// NoECDH sagt, ob der gelieferte Chip keinen Schluesselaustausch kann. Dann
// faellt der Serverbetrieb mit Schluessel im Chip weg - das Programm sollte
// den Anwendungsfall abschalten und es sagen, nicht abbrechen.
func NoECDH(err error) bool { return Code(err) == "error.guard.noecdh" }

// Guard ist die Verbindung zum lokalen Dienst.
type Guard struct {
	address   string
	devicePub []byte
	timeout   time.Duration
}

// Open prueft die Angaben und sieht nach, ob der Dienst antwortet.
// devicePublicKey sind die 32 Byte, die der Hersteller einmal mit
// `osk guard-key` aus seinem Stick gelesen und ins Programm eingetragen hat.
func Open(address string, devicePublicKey []byte) (*Guard, error) {
	if len(devicePublicKey) != KeyLen {
		return nil, fail("error.guard.key")
	}
	host, port, err := net.SplitHostPort(address)
	if err != nil || port == "" {
		return nil, fail("error.guard.request")
	}
	// Nur die Rueckschleife: der Stick steckt hier, nicht im Netz.
	if ip := net.ParseIP(host); host != "localhost" && (ip == nil || !ip.IsLoopback()) {
		return nil, fail("error.guard.request")
	}
	guard := &Guard{address: address, timeout: 30 * time.Second}
	guard.devicePub = make([]byte, KeyLen)
	copy(guard.devicePub, devicePublicKey)
	if _, err := guard.ask("ping"); err != nil {
		return nil, err
	}
	return guard, nil
}

// ask schickt eine Zeile und liest die Antwort. Je Anfrage eine Verbindung:
// das ist langsamer, aber ein Programm kann so nie die Antwort einer
// fremden Anfrage lesen.
func (g *Guard) ask(line string) (map[string]string, error) {
	conn, err := net.DialTimeout("tcp", g.address, 3*time.Second)
	if err != nil {
		return nil, fail("error.guard.nodevice")
	}
	defer conn.Close()
	if err := conn.SetDeadline(time.Now().Add(g.timeout)); err != nil {
		return nil, fail("error.guard.nodevice")
	}
	if _, err := conn.Write([]byte(line + "\n")); err != nil {
		return nil, fail("error.guard.nodevice")
	}
	reader := bufio.NewReaderSize(conn, 64*1024)
	answer, err := readLine(reader)
	if err != nil {
		return nil, fail("error.guard.nodevice")
	}
	return parseAnswer(answer)
}

func readLine(reader *bufio.Reader) (string, error) {
	var out strings.Builder
	for {
		part, more, err := reader.ReadLine()
		if err != nil {
			return "", err
		}
		if out.Len()+len(part) > maxLine {
			return "", io.ErrShortBuffer
		}
		out.Write(part)
		if !more {
			return out.String(), nil
		}
	}
}

func parseAnswer(line string) (map[string]string, error) {
	fields := strings.Fields(line)
	if len(fields) == 0 {
		return nil, fail("error.guard.answer")
	}
	switch fields[0] {
	case "OK":
	case "ERR":
		if len(fields) < 2 {
			return nil, fail("error.guard.internal")
		}
		return nil, fail(fields[1])
	default:
		return nil, fail("error.guard.answer")
	}
	out := map[string]string{}
	for _, field := range fields[1:] {
		if name, value, found := strings.Cut(field, "="); found {
			out[name] = value
		}
	}
	return out, nil
}

// value holt einen Hexwert aus der Antwort und prueft seine Laenge.
func value(answer map[string]string, key string, want int) ([]byte, error) {
	text, found := answer[key]
	if !found || text == "-" {
		return nil, fail("error.guard.answer")
	}
	raw, err := hex.DecodeString(text)
	if err != nil {
		return nil, fail("error.guard.answer")
	}
	if want > 0 && len(raw) != want {
		return nil, fail("error.guard.answer")
	}
	return raw, nil
}

// BinaryHash ist die Pruefsumme einer Programmdatei. Leerer Pfad: die
// laufende Datei.
func BinaryHash(path string) ([]byte, error) {
	if path == "" {
		exe, err := os.Executable()
		if err != nil {
			return nil, fail("error.guard.binary")
		}
		path = exe
	}
	file, err := os.Open(path)
	if err != nil {
		return nil, fail("error.guard.binary")
	}
	defer file.Close()
	info, err := file.Stat()
	if err != nil || info.IsDir() || info.Size() > maxBinary {
		return nil, fail("error.guard.binary")
	}
	digest := sha256.New()
	if _, err := io.Copy(digest, io.LimitReader(file, maxBinary)); err != nil {
		return nil, fail("error.guard.binary")
	}
	return digest.Sum(nil), nil
}

// message baut die Bytefolge, die das Geraet unterschreibt.
func message(domain string, nonce, binHash []byte) []byte {
	out := make([]byte, 0, len(domain)+1+len(nonce)+len(binHash))
	out = append(out, domain...)
	out = append(out, 0)
	out = append(out, nonce...)
	return append(out, binHash...)
}

// Present ist eine Runde Frage und Antwort: der Zufallswert kommt von hier,
// die Unterschrift wird hier geprueft. Kommt kein Fehler, steckte in diesem
// Augenblick der richtige Stick, und die Programmdatei ist unveraendert.
//
// binaryPath leer heisst: die laufende Datei. Was diese Stufe wert ist,
// steht in docs/dongle.md - sie haelt das Mitnehmen einer Kopie auf und
// nichts darueber hinaus. Echter Schutz ist Seal/Unseal.
func (g *Guard) Present(binaryPath string) error {
	binHash, err := BinaryHash(binaryPath)
	if err != nil {
		return err
	}
	nonce := make([]byte, NonceLen)
	if _, err := rand.Read(nonce); err != nil {
		return fail("error.guard.random")
	}
	answer, err := g.ask("attest " + hex.EncodeToString(nonce) + " " +
		hex.EncodeToString(binHash))
	if err != nil {
		return err
	}
	public, err := value(answer, "pub", KeyLen)
	if err != nil {
		return err
	}
	signature, err := value(answer, "sig", SigLen)
	if err != nil {
		return err
	}
	if !equal(public, g.devicePub) {
		return fail("error.guard.wrongdevice")
	}
	if !ed25519.Verify(ed25519.PublicKey(g.devicePub),
		message(attestDomain, nonce, binHash), signature) {
		return fail("error.guard.signature")
	}
	return nil
}

func equal(a, b []byte) bool {
	if len(a) != len(b) {
		return false
	}
	var diff byte
	for i := range a {
		diff |= a[i] ^ b[i]
	}
	return diff == 0
}

// Watch fragt in festem Abstand weiter und ruft lost auf, sobald eine Runde
// scheitert - beim Abziehen also beim naechsten Durchgang. Danach kehrt
// Watch zurueck. Ein geschuetztes Programm beendet sich in lost.
func (g *Guard) Watch(every time.Duration, binaryPath string, lost func(error)) {
	if every <= 0 {
		every = 2 * time.Second
	}
	for {
		time.Sleep(every)
		if err := g.Present(binaryPath); err != nil {
			if lost != nil {
				lost(err)
			}
			return
		}
	}
}

// Seal verschliesst Daten mit einem Schluessel, der den Stick nie verlaesst.
// extra darf leer sein; ist er gesetzt, muss er beim Oeffnen gleich sein.
func (g *Guard) Seal(label string, extra, plain []byte) ([]byte, error) {
	if len(plain) == 0 {
		return nil, fail("error.guard.size")
	}
	answer, err := g.ask("seal " + hexText(label) + " " + hexOrDash(extra) +
		" " + hex.EncodeToString(plain))
	if err != nil {
		return nil, err
	}
	return value(answer, "blob", 0)
}

// Unseal macht wieder auf. Ohne Stick gibt es den Schluessel nicht - dann
// kommt ein Fehler und keine halben Daten.
func (g *Guard) Unseal(label string, extra, blob []byte) ([]byte, error) {
	if len(blob) == 0 {
		return nil, fail("error.guard.size")
	}
	answer, err := g.ask("unseal " + hexText(label) + " " + hexOrDash(extra) +
		" " + hex.EncodeToString(blob))
	if err != nil {
		return nil, err
	}
	return value(answer, "plain", 0)
}

// License ist eine gepruefte Lizenz aus dem Tresor.
type License struct {
	Product    string
	Holder     string
	Number     string
	Tiers      []string
	Seats      uint32
	IssuedUTC  int64
	ExpiresUTC int64
}

// Expired sagt, ob die Lizenz jetzt abgelaufen ist.
func (l License) Expired() bool {
	return l.ExpiresUTC != 0 && time.Now().UTC().Unix() > l.ExpiresUTC
}

// HasTier sagt, ob eine Ausbaustufe enthalten ist.
func (l License) HasTier(name string) bool {
	for _, have := range l.Tiers {
		if strings.EqualFold(have, name) {
			return true
		}
	}
	return false
}

// License holt die Lizenz fuer ein Produkt und prueft ihre Unterschrift.
func (g *Guard) License(product string) (License, error) {
	answer, err := g.ask("license " + hexText(product))
	if err != nil {
		return License{}, err
	}
	out := License{}
	name, err := value(answer, "product", 0)
	if err != nil {
		return License{}, err
	}
	out.Product = string(name)
	out.Holder = optional(answer["holder"])
	out.Number = optional(answer["number"])
	if list := answer["tiers"]; list != "" && list != "-" {
		for _, part := range strings.Split(list, ",") {
			raw, decodeErr := hex.DecodeString(part)
			if decodeErr != nil {
				return License{}, fail("error.guard.answer")
			}
			out.Tiers = append(out.Tiers, string(raw))
		}
	}
	seats, err := strconv.ParseUint(answer["seats"], 10, 32)
	if err != nil {
		return License{}, fail("error.guard.answer")
	}
	out.Seats = uint32(seats)
	if out.IssuedUTC, err = strconv.ParseInt(answer["issued"], 10, 64); err != nil {
		return License{}, fail("error.guard.answer")
	}
	if out.ExpiresUTC, err = strconv.ParseInt(answer["expires"], 10, 64); err != nil {
		return License{}, fail("error.guard.answer")
	}
	return out, nil
}

// NodePublic holt den oeffentlichen Teil eines Knotenschluessels. curve ist
// "ed25519" zum Unterschreiben oder "x25519" zum Schluesselaustausch.
func (g *Guard) NodePublic(label, curve string) ([]byte, error) {
	answer, err := g.ask("nodepub " + hexText(label) + " " + hexText(curve))
	if err != nil {
		return nil, err
	}
	return value(answer, "pub", KeyLen)
}

// NodeSign laesst den Knoten einen Auftrag unterschreiben.
func (g *Guard) NodeSign(label string, job []byte) ([]byte, error) {
	if len(job) == 0 {
		return nil, fail("error.guard.size")
	}
	answer, err := g.ask("nodesign " + hexText(label) + " " +
		hex.EncodeToString(job))
	if err != nil {
		return nil, err
	}
	return value(answer, "sig", SigLen)
}

// NodeVerify prueft die Unterschrift eines anderen Knotens. Das braucht
// keinen Stick - nur den oeffentlichen Schluessel der Gegenseite.
func NodeVerify(publicKey []byte, label string, job, signature []byte) error {
	if len(publicKey) != KeyLen {
		return fail("error.guard.key")
	}
	if len(signature) != SigLen {
		return fail("error.guard.signature")
	}
	if label == "" || len(job) == 0 {
		return fail("error.guard.size")
	}
	jobHash := sha256.Sum256(job)
	signed := make([]byte, 0, len(signDomain)+2+len(label)+HashLen)
	signed = append(signed, signDomain...)
	signed = append(signed, 0)
	signed = append(signed, label...)
	signed = append(signed, 0)
	signed = append(signed, jobHash[:]...)
	if !ed25519.Verify(ed25519.PublicKey(publicKey), signed, signature) {
		return fail("error.guard.signature")
	}
	return nil
}

// NodeAgree handelt ein gemeinsames Geheimnis mit der Gegenseite aus. Kann
// der gelieferte Chip kein ECDH, kommt error.guard.noecdh - siehe NoECDH.
func (g *Guard) NodeAgree(label string, peerPublic []byte) ([]byte, error) {
	if len(peerPublic) != SecretLen {
		return nil, fail("error.guard.peerkey")
	}
	answer, err := g.ask("nodeagree " + hexText(label) + " " +
		hex.EncodeToString(peerPublic))
	if err != nil {
		return nil, err
	}
	return value(answer, "sec", SecretLen)
}

func hexText(text string) string { return hex.EncodeToString([]byte(text)) }

func hexOrDash(raw []byte) string {
	if len(raw) == 0 {
		return "-"
	}
	return hex.EncodeToString(raw)
}

func optional(text string) string {
	if text == "" || text == "-" {
		return ""
	}
	raw, err := hex.DecodeString(text)
	if err != nil {
		return ""
	}
	return string(raw)
}
