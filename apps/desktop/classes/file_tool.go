// Das Dateiwerkzeug: eine Datei verschluesseln, eine Datei entschluesseln.
//
// Zwei Formate, beide oeffentlich beschrieben und beide hier nach der
// Beschreibung gebaut, nicht aus einem fremden Paket uebernommen:
//
//   - age (age-encryption.org/v1): X25519 als Empfaengerverfahren,
//     ChaCha20-Poly1305 abschnittsweise. Passt zu diesem Geraet, weil der
//     Chip genau dieses X25519 rechnet und das Format eine Stelle fuer
//     Schluessel in Hardware vorsieht.
//   - OpenPGP (RFC 9580): X25519 im Sitzungsschluesselpaket, AES-256-GCM
//     abschnittsweise. Damit ist der Stick fuer Programme erreichbar, die
//     nur OpenPGP kennen.
//
// ZWEI REGELN, die ueber allem stehen:
//
//  1. Der private Schluessel gibt es nie als Datei. Es gibt in diesem Werkzeug
//     keine Ausgabefunktion dafuer, keinen Zwischenspeicher auf der Platte und
//     kein kurzlebiges PEM. Entschluesselt wird NICHT, indem das Programm
//     einen Schluessel holt, sondern indem es dem Geraet einen fremden
//     oeffentlichen Punkt hinhaelt und das gemeinsame Geheimnis
//     zurueckbekommt - Befehl 16 `agree` aus shared/protocol.md, im Chip
//     osk_se050_ecdh(). Was das Programm sieht, ist je Datei ein anderer Wert
//     und nicht der Schluessel.
//
//  2. Abschnittsweise. Eine grosse Datei wird nie ganz in den Speicher
//     gelesen; beide Formate sind darauf gebaut, und darum ist der Weg hier
//     io.Reader nach io.Writer.
//
// Was NICHT geht, mit klarer Meldung: ein age-Schluessel als Datei
// (AGE-SECRET-KEY-1...). Wer so etwas hat, braucht dieses Geraet nicht, und
// das Werkzeug soll nicht der Weg sein, auf dem Schluessel doch auf die
// Platte kommen.
package classes

import (
	"crypto/ecdh"
	"crypto/rand"
	"io"
	"strings"
)

// FileChunkSize ist die Laenge eines Abschnitts im Klartext, in Byte. Beide
// Formate verlangen Zweierpotenzen: age schreibt 64 KiB fest vor, OpenPGP
// nennt die Potenz im Kopf der Nachricht. Darum steht die Zahl einmal hier
// und wird fuer OpenPGP in die Potenz zurueckgerechnet.
const FileChunkSize = 1 << 16

// FileKeyLen ist die Laenge eines oeffentlichen X25519-Punktes, in Byte.
const FileKeyLen = 32

// FileFormat ist das Dateiformat. Es wird immer genannt; eine stille Vorgabe
// gibt es nicht, denn die zwei Formate sehen am Anfang verschieden aus und
// wer sie verwechselt, merkt es erst beim Empfaenger.
type FileFormat string

const (
	FileFormatAge FileFormat = "age"
	FileFormatPGP FileFormat = "pgp"
)

// ValidFileFormat sagt, ob der Name eines der zwei Formate ist.
func ValidFileFormat(name string) bool {
	return FileFormat(name) == FileFormatAge || FileFormat(name) == FileFormatPGP
}

// FileRecipient ist, wer eine Datei oeffnen darf: ein oeffentlicher
// X25519-Punkt und nichts weiter. Woher er kommt - aus dem eigenen Chip oder
// von einem anderen Stick - steht nicht darin, denn fuer das Verschluesseln
// macht es keinen Unterschied.
type FileRecipient struct {
	Public []byte
}

// NewFileRecipient prueft den Punkt. Ein Punkt, den die Kurve nicht kennt,
// wird hier abgewiesen und nicht erst beim Rechnen - so steht die Ursache an
// der Stelle, an der sie noch zu verstehen ist.
func NewFileRecipient(public []byte) (FileRecipient, error) {
	if len(public) != FileKeyLen {
		return FileRecipient{}, Fail("error.file.recipient")
	}
	if _, err := ecdh.X25519().NewPublicKey(public); err != nil {
		return FileRecipient{}, Fail("error.file.recipient")
	}
	copied := make([]byte, FileKeyLen)
	copy(copied, public)
	return FileRecipient{Public: copied}, nil
}

// fileRecipientHRP ist die Vorsilbe, unter der age-Empfaenger geschrieben
// werden: "age1..." in der Schreibweise bech32.
const fileRecipientHRP = "age"

// String schreibt den Empfaenger so, wie age ihn schreibt. Damit kann ein
// Empfaenger weitergegeben werden, ohne dass dieses Programm ein eigenes
// Format erfindet.
func (r FileRecipient) String() string {
	return bech32Encode(fileRecipientHRP, r.Public)
}

// ParseFileRecipient liest einen Empfaenger aus seiner Schreibweise.
func ParseFileRecipient(text string) (FileRecipient, error) {
	text = strings.TrimSpace(text)
	if strings.HasPrefix(strings.ToUpper(text), "AGE-SECRET-KEY-") {
		// Ausdrueckliche Meldung statt "ungueltig": wer das hier
		// einsetzt, hat die Absicht missverstanden, nicht sich vertippt.
		return FileRecipient{}, Fail("error.file.secretkey")
	}
	hrp, data, err := bech32Decode(text)
	if err != nil || hrp != fileRecipientHRP {
		return FileRecipient{}, Fail("error.file.recipient")
	}
	return NewFileRecipient(data)
}

// FileIdentity ist, wer eine Datei oeffnen kann. Absichtlich eine
// Schnittstelle mit genau zwei Fragen und keiner, die Schluesselmaterial
// herausgibt:
//
//	Public() - der oeffentliche Teil, er darf heraus
//	Agree()  - das gemeinsame Geheimnis zu einem fremden Punkt
//
// Es gibt hier kein Private(). Das ist kein Versehen: eine solche Frage
// waere der Weg, auf dem Schluesselmaterial die Grenze verlaesst, und dann
// waere der ganze Stick sinnlos.
type FileIdentity interface {
	Public() []byte
	Agree(peerPublic []byte) ([]byte, error)
}

// FileDeviceIdentity ist eine Kennung im Chip, angesprochen ueber ihre
// Bezeichnung. Das Rechnen macht das Geraet.
type FileDeviceIdentity struct {
	guard  *Guard
	label  string
	public []byte
}

// NewFileDeviceIdentity holt den oeffentlichen Teil beim Geraet - einmal, und
// nur den oeffentlichen. Antwortet der Chip, dass er keinen
// Schluesselaustausch kann (ein im Werk auf FIPS gestelltes Stueck), kommt
// error.guard.noecdh durch, und das Programm kann sagen, was wegfaellt,
// statt bei der ersten Datei zu scheitern.
func NewFileDeviceIdentity(guard *Guard, label string) (*FileDeviceIdentity, error) {
	if guard == nil {
		return nil, Fail("error.guard.nodevice")
	}
	key, err := guard.NodePublic(label, "x25519")
	if err != nil {
		return nil, err
	}
	if len(key.PublicKey) != FileKeyLen {
		return nil, Fail("error.guard.answer")
	}
	return &FileDeviceIdentity{guard: guard, label: label, public: key.PublicKey}, nil
}

// fileNodeGuard baut den Zugang zu den Befehlen 16 und 18 auf einer
// bestehenden Verbindung.
//
// Absichtlich NICHT ueber NewGuard(): der verlangt den eingetragenen
// oeffentlichen Schluessel des Geraets, weil er die Frage-und-Antwort des
// Dongle-Schutzes prueft. Das Dateiwerkzeug prueft sie nicht - es benutzt
// nur NodePublic() und NodeAgree(), und die rechnen beide im Chip. Einen
// Schluessel zu verlangen, mit dem hier nichts geprueft wird, waere eine
// Zusage, die nicht gehalten wird.
func fileNodeGuard(client *Client) *Guard {
	return &Guard{client: client, cfg: DefaultGuardConfig()}
}

func (i *FileDeviceIdentity) Public() []byte { return i.public }

// Agree laesst das Geraet rechnen. Jeder Aufruf steht im Geraeteprotokoll,
// und ohne Freischaltung verlangt das Geraet einen Tastendruck.
func (i *FileDeviceIdentity) Agree(peerPublic []byte) ([]byte, error) {
	return i.guard.NodeAgree(i.label, peerPublic)
}

// Recipient nennt die Kennung als Empfaenger - so kann der Besitzer eine
// Datei fuer sich selbst verschluesseln.
func (i *FileDeviceIdentity) Recipient() (FileRecipient, error) {
	return NewFileRecipient(i.public)
}

// FileEncrypt schreibt `in` verschluesselt nach `out`. Mehrere Empfaenger
// sind erlaubt; jeder von ihnen kann die Datei allein oeffnen.
func FileEncrypt(format FileFormat, recipients []FileRecipient, in io.Reader, out io.Writer) error {
	if len(recipients) == 0 {
		return Fail("error.file.norecipient")
	}
	switch format {
	case FileFormatAge:
		return ageEncrypt(recipients, in, out)
	case FileFormatPGP:
		return pgpEncrypt(recipients, in, out)
	}
	return Fail("error.file.format")
}

// FileDecrypt liest `in` und schreibt den Klartext nach `out`. Entschluesselt
// wird nur, was unverfaelscht ist: jeder Abschnitt traegt ein Siegel, und ein
// abgeschnittener Strom ist ein Fehler, keine kuerzere Datei.
func FileDecrypt(format FileFormat, identity FileIdentity, in io.Reader, out io.Writer) error {
	if identity == nil {
		return Fail("error.file.noidentity")
	}
	switch format {
	case FileFormatAge:
		return ageDecrypt(identity, in, out)
	case FileFormatPGP:
		return pgpDecrypt(identity, in, out)
	}
	return Fail("error.file.format")
}

// fileEphemeral wuerfelt das fluechtige Schluesselpaar des Absenders. Es lebt
// nur fuer diese eine Datei und wird nicht aufbewahrt.
func fileEphemeral() (*ecdh.PrivateKey, error) {
	key, err := ecdh.X25519().GenerateKey(rand.Reader)
	if err != nil {
		return nil, Fail("error.file.random")
	}
	return key, nil
}

// ecdhPublic liest einen fremden Punkt. Die Pruefung macht crypto/ecdh - sie
// weist auch die Punkte kleiner Ordnung ab, aus denen ein gemeinsames
// Geheimnis aus lauter Nullen wuerde.
func ecdhPublic(public []byte) (*ecdh.PublicKey, error) {
	key, err := ecdh.X25519().NewPublicKey(public)
	if err != nil {
		return nil, Fail("error.file.recipient")
	}
	return key, nil
}

// fileRandom holt Zufall fuer Salz und Nonce.
func fileRandom(n int) ([]byte, error) {
	buf := make([]byte, n)
	if _, err := rand.Read(buf); err != nil {
		return nil, Fail("error.file.random")
	}
	return buf, nil
}

// --- bech32, nach BIP-173 ------------------------------------------------
//
// age schreibt Empfaenger in dieser Form. Sie steht hier selbst gerechnet,
// weil sie kurz ist und weil ein Paket dafuer gegen die Projektvorgabe
// stuende. Die Pruefsumme ist der Zweck der Uebung: ein verlesener oder
// vertippter Empfaenger faellt auf, bevor eine Datei fuer ihn
// verschluesselt wird.

const bech32Alphabet = "qpzry9x8gf2tvdw0s3jn54khce6mua7l"

// bech32Encode schreibt Daten als "<hrp>1<daten><pruefsumme>".
func bech32Encode(hrp string, data []byte) string {
	values := bech32ToFive(data)
	checksum := bech32Checksum(hrp, values)
	var b strings.Builder
	b.WriteString(hrp)
	b.WriteByte('1')
	for _, v := range append(values, checksum...) {
		b.WriteByte(bech32Alphabet[v])
	}
	return b.String()
}

// bech32Decode liest die Form und prueft die Pruefsumme. Gemischte Gross-
// und Kleinschreibung ist nach BIP-173 ungueltig und wird abgewiesen.
func bech32Decode(text string) (string, []byte, error) {
	if text != strings.ToLower(text) && text != strings.ToUpper(text) {
		return "", nil, Fail("error.file.recipient")
	}
	text = strings.ToLower(text)
	cut := strings.LastIndexByte(text, '1')
	if cut < 1 || cut+7 > len(text) {
		return "", nil, Fail("error.file.recipient")
	}
	hrp := text[:cut]
	values := make([]byte, 0, len(text)-cut-1)
	for i := cut + 1; i < len(text); i++ {
		index := strings.IndexByte(bech32Alphabet, text[i])
		if index < 0 {
			return "", nil, Fail("error.file.recipient")
		}
		values = append(values, byte(index))
	}
	if !bech32Verify(hrp, values) {
		return "", nil, Fail("error.file.recipient")
	}
	data, err := bech32FromFive(values[:len(values)-6])
	if err != nil {
		return "", nil, err
	}
	return hrp, data, nil
}

// bech32ToFive zerlegt Bytes in Gruppen von fuenf Bit.
func bech32ToFive(data []byte) []byte {
	out := make([]byte, 0, len(data)*8/5+1)
	var acc uint32
	var bits uint
	for _, b := range data {
		acc = acc<<8 | uint32(b)
		bits += 8
		for bits >= 5 {
			bits -= 5
			out = append(out, byte(acc>>bits)&31)
		}
	}
	if bits > 0 {
		out = append(out, byte(acc<<(5-bits))&31)
	}
	return out
}

// bech32FromFive setzt die Gruppen wieder zu Bytes zusammen. Uebrige Bits
// muessen Null sein - sonst waere dieselben Daten auf zwei Weisen
// schreibbar, und genau das soll nicht sein.
func bech32FromFive(values []byte) ([]byte, error) {
	out := make([]byte, 0, len(values)*5/8)
	var acc uint32
	var bits uint
	for _, v := range values {
		if v > 31 {
			return nil, Fail("error.file.recipient")
		}
		acc = acc<<5 | uint32(v)
		bits += 5
		for bits >= 8 {
			bits -= 8
			out = append(out, byte(acc>>bits))
		}
	}
	if bits >= 5 || byte(acc<<(8-bits))&0xff != 0 {
		return nil, Fail("error.file.recipient")
	}
	return out, nil
}

var bech32Generator = [5]uint32{0x3b6a57b2, 0x26508e6d, 0x1ea119fa, 0x3d4233dd, 0x2a1462b3}

func bech32Polymod(values []byte) uint32 {
	checksum := uint32(1)
	for _, v := range values {
		top := checksum >> 25
		checksum = (checksum&0x1ffffff)<<5 ^ uint32(v)
		for i := 0; i < 5; i++ {
			if top>>uint(i)&1 == 1 {
				checksum ^= bech32Generator[i]
			}
		}
	}
	return checksum
}

// bech32Expand macht aus der Vorsilbe die Werte, die in die Pruefsumme
// eingehen - so gehoert die Vorsilbe mit zur Pruefung.
func bech32Expand(hrp string) []byte {
	out := make([]byte, 0, len(hrp)*2+1)
	for i := 0; i < len(hrp); i++ {
		out = append(out, hrp[i]>>5)
	}
	out = append(out, 0)
	for i := 0; i < len(hrp); i++ {
		out = append(out, hrp[i]&31)
	}
	return out
}

func bech32Checksum(hrp string, values []byte) []byte {
	input := append(bech32Expand(hrp), values...)
	input = append(input, 0, 0, 0, 0, 0, 0)
	mod := bech32Polymod(input) ^ 1
	out := make([]byte, 6)
	for i := 0; i < 6; i++ {
		out[i] = byte(mod>>uint(5*(5-i))) & 31
	}
	return out
}

func bech32Verify(hrp string, values []byte) bool {
	return bech32Polymod(append(bech32Expand(hrp), values...)) == 1
}
