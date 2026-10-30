// Salsa20, nach der Beschreibung von Daniel J. Bernstein (2007).
//
// WOZU: der KeePass-Behaelter verdeckt die Kennwoerter INNERHALB der schon
// entschluesselten XML-Datei noch ein zweites Mal, mit einem Schluesselstrom.
// Die Fassung 3 nimmt dafuer Salsa20, die Fassung 4 ChaCha20. ChaCha20 steht
// schon in chacha.go; Salsa20 ist sein Vorgaenger und fehlt. Also hier, nach
// der Beschreibung, wie alles in diesem Programm.
//
// Dass die Kennwoerter im Klartext-XML noch verdeckt sind, schuetzt uebrigens
// fast nichts - der Schluessel dazu steht im selben Dateikopf. Es haelt sie
// aus dem Arbeitsspeicher heraus, solange niemand sie ansieht. Gelesen werden
// muessen sie trotzdem, und darum steht diese Datei hier.
package classes

import "encoding/binary"

// Salsa20KeyLen ist die Schluessellaenge in Byte.
const Salsa20KeyLen = 32

// Salsa20NonceLen ist die Laenge des Einmalwertes in Byte.
const Salsa20NonceLen = 8

// salsa20BlockLen ist die Blocklaenge in Byte.
const salsa20BlockLen = 64

// salsa20Constants ist "expand 32-byte k", in vier Woertern. Derselbe Satz
// wie bei ChaCha20 - die beiden Verfahren teilen ihn.
var salsa20Constants = [4]uint32{0x61707865, 0x3320646e, 0x79622d32, 0x6b206574}

// Salsa20Stream ist ein laufender Schluesselstrom.
type Salsa20Stream struct {
	state   [16]uint32
	block   [salsa20BlockLen]byte
	used    int
	started bool
}

// NewSalsa20 baut den Strom.
func NewSalsa20(key, nonce []byte) (*Salsa20Stream, error) {
	if len(key) != Salsa20KeyLen {
		return nil, Fail("error.salsa20.key")
	}
	if len(nonce) != Salsa20NonceLen {
		return nil, Fail("error.salsa20.nonce")
	}
	s := &Salsa20Stream{used: salsa20BlockLen}
	s.state[0] = salsa20Constants[0]
	s.state[5] = salsa20Constants[1]
	s.state[10] = salsa20Constants[2]
	s.state[15] = salsa20Constants[3]
	for i := 0; i < 4; i++ {
		s.state[1+i] = binary.LittleEndian.Uint32(key[i*4:])
		s.state[11+i] = binary.LittleEndian.Uint32(key[16+i*4:])
	}
	s.state[6] = binary.LittleEndian.Uint32(nonce[0:])
	s.state[7] = binary.LittleEndian.Uint32(nonce[4:])
	// 8 und 9 sind der Zaehler und fangen bei null an.
	return s, nil
}

// XOR verknuepft data mit dem Strom - an der Stelle, an der der Strom
// gerade steht. Das ist wichtig: der Behaelter verdeckt die Kennwoerter in
// der Reihenfolge, in der sie in der Datei stehen, und wer eins
// ueberspringt, entschluesselt alle folgenden falsch.
func (s *Salsa20Stream) XOR(data []byte) {
	for i := range data {
		if s.used == salsa20BlockLen {
			s.next()
		}
		data[i] ^= s.block[s.used]
		s.used++
	}
}

func (s *Salsa20Stream) next() {
	var x [16]uint32
	copy(x[:], s.state[:])
	for round := 0; round < 10; round++ {
		// Die Spalten: quarterround(x0,x4,x8,x12) und die drei
		// gedrehten Spalten dazu.
		salsa20Quarter(&x, 4, 0, 8, 12)
		salsa20Quarter(&x, 9, 5, 13, 1)
		salsa20Quarter(&x, 14, 10, 2, 6)
		salsa20Quarter(&x, 3, 15, 7, 11)
		// Die Zeilen: quarterround(y0,y1,y2,y3) und die drei gedrehten
		// Zeilen dazu.
		salsa20Quarter(&x, 1, 0, 2, 3)
		salsa20Quarter(&x, 6, 5, 7, 4)
		salsa20Quarter(&x, 11, 10, 8, 9)
		salsa20Quarter(&x, 12, 15, 13, 14)
	}
	for i := 0; i < 16; i++ {
		binary.LittleEndian.PutUint32(s.block[i*4:], x[i]+s.state[i])
	}
	s.used = 0
	// Der Zaehler ist 64 Bit weit und steht in zwei Woertern.
	s.state[8]++
	if s.state[8] == 0 {
		s.state[9]++
	}
}

// salsa20Quarter ist die Viertelrunde. Die Reihenfolge der Zuweisungen und
// die Drehungen 7, 9, 13, 18 sind die der Beschreibung.
func salsa20Quarter(x *[16]uint32, a, b, c, d int) {
	x[a] ^= rotateLeft32(x[b]+x[d], 7)
	x[c] ^= rotateLeft32(x[a]+x[b], 9)
	x[d] ^= rotateLeft32(x[c]+x[a], 13)
	x[b] ^= rotateLeft32(x[d]+x[c], 18)
}
