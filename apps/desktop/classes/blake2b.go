// BLAKE2b nach RFC 7693.
//
// WARUM diese Datei existiert: der KeePass-Behaelter in der Fassung 4
// schluesselt mit Argon2, und Argon2 rechnet mit BLAKE2b. Beides fehlt in
// der Standardbibliothek von Go, und apps/desktop/go.mod hat keine
// Abhaengigkeiten. Also nach der Beschreibung gebaut - wie bech32 im
// Dateiwerkzeug und aus demselben Grund.
//
// Gerechnet wird GEGEN DIE TESTWERTE DER BESCHREIBUNG, nicht gegen die
// eigene Erwartung: RFC 7693 nennt in Anhang A einen Wert fuer "abc", und
// die Testdatei prueft ihn. Eine selbstgebaute Hashfunktion, die nur gegen
// sich selbst geprueft ist, ist nicht geprueft - das ist derselbe Fehler
// wie die Attrappe, die ihren Zaehler anders fuehrt als das Geraet.
package classes

import (
	"encoding/binary"
	"hash"
)

// Blake2bBlockLen ist die Blocklaenge in Byte.
const Blake2bBlockLen = 128

// Blake2bMaxLen ist die groesste Ausgabelaenge in Byte.
const Blake2bMaxLen = 64

// Blake2bMaxKeyLen ist die groesste Schluessellaenge in Byte.
const Blake2bMaxKeyLen = 64

// blake2bIV ist der Anfangszustand - dieselben acht Zahlen wie bei SHA-512.
var blake2bIV = [8]uint64{
	0x6a09e667f3bcc908, 0xbb67ae8584caa73b, 0x3c6ef372fe94f82b, 0xa54ff53a5f1d36f1,
	0x510e527fade682d1, 0x9b05688c2b3e6c1f, 0x1f83d9abfb41bd6b, 0x5be0cd19137e2179,
}

// blake2bSigma ist die Reihenfolge, in der die Woerter der Nachricht in die
// zehn Runden gehen. Die Runden 11 und 12 wiederholen die ersten zwei.
var blake2bSigma = [10][16]byte{
	{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15},
	{14, 10, 4, 8, 9, 15, 13, 6, 1, 12, 0, 2, 11, 7, 5, 3},
	{11, 8, 12, 0, 5, 2, 15, 13, 10, 14, 3, 6, 7, 1, 9, 4},
	{7, 9, 3, 1, 13, 12, 11, 14, 2, 6, 5, 10, 4, 0, 15, 8},
	{9, 0, 5, 7, 2, 4, 10, 15, 14, 1, 11, 12, 6, 8, 3, 13},
	{2, 12, 6, 10, 0, 11, 8, 3, 4, 13, 7, 5, 15, 14, 1, 9},
	{12, 5, 1, 15, 14, 13, 4, 10, 0, 7, 6, 3, 9, 2, 8, 11},
	{13, 11, 7, 14, 12, 1, 3, 9, 5, 0, 15, 4, 8, 6, 2, 10},
	{6, 15, 14, 9, 11, 3, 0, 8, 12, 2, 13, 7, 1, 4, 10, 5},
	{10, 2, 8, 4, 7, 6, 1, 5, 15, 11, 9, 14, 3, 12, 13, 0},
}

// blake2b ist der laufende Zustand.
type blake2b struct {
	h [8]uint64
	// start ist der Anfangszustand samt Parameterblock. Er wird behalten,
	// damit Reset nicht denselben Aufbau ein zweites Mal rechnen muss -
	// eine zweite Stelle, an der die Parameter stehen, waere eine zweite
	// Wahrheit.
	start  [8]uint64
	key    []byte
	block  [Blake2bBlockLen]byte
	filled int
	// counted ist die Zahl der Byte, die schon in h eingegangen sind.
	counted uint64
	size    int
}

// NewBlake2b baut eine Hashfunktion mit der gewuenschten Ausgabelaenge und
// einem Schluessel. Schluessel leer: ohne.
func NewBlake2b(size int, key []byte) (hash.Hash, error) {
	state, err := newBlake2b(size, key)
	if err != nil {
		return nil, err
	}
	return state, nil
}

func newBlake2b(size int, key []byte) (*blake2b, error) {
	if size <= 0 || size > Blake2bMaxLen {
		return nil, Fail("error.blake2b.size")
	}
	if len(key) > Blake2bMaxKeyLen {
		return nil, Fail("error.blake2b.key")
	}
	state := &blake2b{h: blake2bIV, size: size}
	// Der Parameterblock geht in das erste Wort: Ausgabelaenge,
	// Schluessellaenge, Faecher und Tiefe. Die uebrigen Felder sind null,
	// weil dieses Programm weder Baumhashen noch Salz benutzt.
	state.h[0] ^= uint64(size) | uint64(len(key))<<8 | 1<<16 | 1<<24
	state.start = state.h
	if len(key) > 0 {
		state.key = append([]byte(nil), key...)
	}
	state.applyKey()
	return state, nil
}

// applyKey stellt den Schluesselblock voran, so wie die Beschreibung es
// verlangt: ein ganzer Block, mit Nullen aufgefuellt.
func (s *blake2b) applyKey() {
	if len(s.key) == 0 {
		return
	}
	var padded [Blake2bBlockLen]byte
	copy(padded[:], s.key)
	s.Write(padded[:])
	Wipe(padded[:])
}

func (s *blake2b) Size() int      { return s.size }
func (s *blake2b) BlockSize() int { return Blake2bBlockLen }

func (s *blake2b) Reset() {
	s.h = s.start
	s.filled = 0
	s.counted = 0
	s.applyKey()
}

// Write nimmt Nachricht auf. Ein voller Block wird NICHT sofort gerechnet,
// sondern erst, wenn klar ist, dass noch etwas kommt: der letzte Block wird
// anders gerechnet, und ob ein Block der letzte ist, weiss man erst
// hinterher.
func (s *blake2b) Write(data []byte) (int, error) {
	total := len(data)
	for len(data) > 0 {
		if s.filled == Blake2bBlockLen {
			s.counted += Blake2bBlockLen
			s.compress(s.block[:], s.counted, false)
			s.filled = 0
		}
		n := copy(s.block[s.filled:], data)
		s.filled += n
		data = data[n:]
	}
	return total, nil
}

// Sum schliesst ab. Der Zustand bleibt unveraendert - Sum darf mehrfach
// gerufen werden, und die Standardbibliothek erwartet das.
func (s *blake2b) Sum(dst []byte) []byte {
	copyState := *s
	var last [Blake2bBlockLen]byte
	copy(last[:], copyState.block[:copyState.filled])
	copyState.counted += uint64(copyState.filled)
	copyState.compress(last[:], copyState.counted, true)
	var out [Blake2bMaxLen]byte
	for i, word := range copyState.h {
		binary.LittleEndian.PutUint64(out[i*8:], word)
	}
	return append(dst, out[:copyState.size]...)
}

// compress ist die Rundenfunktion F aus der Beschreibung.
func (s *blake2b) compress(block []byte, counted uint64, final bool) {
	var m [16]uint64
	for i := 0; i < 16; i++ {
		m[i] = binary.LittleEndian.Uint64(block[i*8:])
	}
	var v [16]uint64
	copy(v[:8], s.h[:])
	copy(v[8:], blake2bIV[:])
	v[12] ^= counted
	// Nur die unteren 64 Bit des Zaehlers werden gefuehrt. Die oberen
	// braeuchte erst eine Nachricht von mehr als 16 Exabyte; sie bleiben
	// null, und das steht hier, damit es nicht als Versehen gelesen wird.
	if final {
		v[14] = ^v[14]
	}
	for round := 0; round < 12; round++ {
		sigma := &blake2bSigma[round%10]
		blake2bMix(&v, 0, 4, 8, 12, m[sigma[0]], m[sigma[1]])
		blake2bMix(&v, 1, 5, 9, 13, m[sigma[2]], m[sigma[3]])
		blake2bMix(&v, 2, 6, 10, 14, m[sigma[4]], m[sigma[5]])
		blake2bMix(&v, 3, 7, 11, 15, m[sigma[6]], m[sigma[7]])
		blake2bMix(&v, 0, 5, 10, 15, m[sigma[8]], m[sigma[9]])
		blake2bMix(&v, 1, 6, 11, 12, m[sigma[10]], m[sigma[11]])
		blake2bMix(&v, 2, 7, 8, 13, m[sigma[12]], m[sigma[13]])
		blake2bMix(&v, 3, 4, 9, 14, m[sigma[14]], m[sigma[15]])
	}
	for i := 0; i < 8; i++ {
		s.h[i] ^= v[i] ^ v[i+8]
	}
}

// blake2bMix ist die Funktion G. Die vier Drehungen 32, 24, 16 und 63 sind
// die Zahlen der Beschreibung; sie sind der Unterschied zu BLAKE2s und
// duerfen nicht aus dem Gedaechtnis eingesetzt werden.
func blake2bMix(v *[16]uint64, a, b, c, d int, x, y uint64) {
	v[a] += v[b] + x
	v[d] = rotateRight64(v[d]^v[a], 32)
	v[c] += v[d]
	v[b] = rotateRight64(v[b]^v[c], 24)
	v[a] += v[b] + y
	v[d] = rotateRight64(v[d]^v[a], 16)
	v[c] += v[d]
	v[b] = rotateRight64(v[b]^v[c], 63)
}

func rotateRight64(value uint64, n uint) uint64 {
	return value>>n | value<<(64-n)
}

// Blake2bSum rechnet in einem Zug.
func Blake2bSum(size int, key, data []byte) ([]byte, error) {
	state, err := newBlake2b(size, key)
	if err != nil {
		return nil, err
	}
	state.Write(data)
	return state.Sum(nil), nil
}
