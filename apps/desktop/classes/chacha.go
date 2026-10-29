// ChaCha20-Poly1305 nach RFC 8439, selbst gerechnet.
//
// Warum selbst: das Projekt bindet keine fremden Pakete ein, und die
// Standardbibliothek von Go hat dieses Verfahren nicht - sie hat AES-GCM,
// aber die beiden Dateiformate, die das Dateiwerkzeug schreibt, verlangen
// ChaCha20-Poly1305 (age) beziehungsweise AES-GCM (OpenPGP). Das eine ist
// also vorhanden, das andere steht hier.
//
// Gebaut nach RFC 8439 Abschnitt 2 (ChaCha20), Abschnitt 2.5 (Poly1305) und
// Abschnitt 2.8 (das zusammengesetzte Verfahren). Geprueft wird gegen die
// Zahlenbeispiele des RFC selbst - das ist der Punkt: diese Datei wird
// nicht gegen die eigene Erwartung gemessen, sondern gegen eine
// veroeffentlichte Vorgabe.
//
// Die Umsetzung ist absichtlich geradeaus und nicht auf Zeit getrimmt. Sie
// ist nicht gegen Zeitmessung gehaertet; sie rechnet nur mit Zahlen fester
// Breite und verzweigt nicht nach Geheimnissen, aber Poly1305 ohne
// Uebertragsabhaengigkeit zu bauen waere eine eigene Aufgabe.
package classes

import (
	"crypto/cipher"
	"crypto/subtle"
	"encoding/binary"
	"errors"
)

// Die Laengen des Verfahrens, in Byte. Jede steht nur hier.
const (
	ChaChaKeyLen   = 32
	ChaChaNonceLen = 12
	ChaChaTagLen   = 16
	chachaBlockLen = 64
)

// chachaAEAD ist ein Schluessel, mit dem versiegelt und geoeffnet wird.
type chachaAEAD struct {
	key [ChaChaKeyLen]byte
}

// NewChaCha20Poly1305 nimmt einen 32 Byte langen Schluessel und liefert das
// Verfahren als cipher.AEAD - dieselbe Form, in der die Standardbibliothek
// AES-GCM herausgibt, damit beide Dateiformate denselben Umgang haben.
func NewChaCha20Poly1305(key []byte) (cipher.AEAD, error) {
	if len(key) != ChaChaKeyLen {
		return nil, Fail("error.file.key")
	}
	a := &chachaAEAD{}
	copy(a.key[:], key)
	return a, nil
}

func (a *chachaAEAD) NonceSize() int { return ChaChaNonceLen }
func (a *chachaAEAD) Overhead() int  { return ChaChaTagLen }

func (a *chachaAEAD) Seal(dst, nonce, plaintext, additional []byte) []byte {
	if len(nonce) != ChaChaNonceLen {
		panic("chacha20poly1305: falsche Nonce-Laenge")
	}
	out, tail := sliceForAppend(dst, len(plaintext)+ChaChaTagLen)
	ciphertext := tail[:len(plaintext)]
	chachaXOR(&a.key, nonce, 1, plaintext, ciphertext)
	tag := chachaTag(&a.key, nonce, additional, ciphertext)
	copy(tail[len(plaintext):], tag[:])
	return out
}

// errFileTamper ist der eine Fehler, den das Oeffnen bei jeder Abweichung
// meldet: falscher Schluessel, verfaelschte Bytes, vertauschte Reihenfolge.
// Welche davon es war, sagt die Meldung nicht - die Unterscheidung waere ein
// Hinweis fuer den Angreifer und fuer den Nutzer keine Hilfe.
var errFileTamper = errors.New("file: verfaelscht oder falscher Empfaenger")

func (a *chachaAEAD) Open(dst, nonce, ciphertext, additional []byte) ([]byte, error) {
	if len(nonce) != ChaChaNonceLen {
		return nil, errFileTamper
	}
	if len(ciphertext) < ChaChaTagLen {
		return nil, errFileTamper
	}
	body := ciphertext[:len(ciphertext)-ChaChaTagLen]
	want := ciphertext[len(ciphertext)-ChaChaTagLen:]
	tag := chachaTag(&a.key, nonce, additional, body)
	// Gleichheit in gleichbleibender Zeit: ein frueher Abbruch wuerde
	// verraten, wie viele Bytes des Siegels schon stimmen.
	if subtle.ConstantTimeCompare(tag[:], want) != 1 {
		return nil, errFileTamper
	}
	out, tail := sliceForAppend(dst, len(body))
	chachaXOR(&a.key, nonce, 1, body, tail)
	return out, nil
}

// sliceForAppend verlaengert dst um n Byte und gibt beides zurueck: das
// ganze Stueck und den neuen Teil.
func sliceForAppend(dst []byte, n int) (head, tail []byte) {
	if total := len(dst) + n; cap(dst) >= total {
		head = dst[:total]
	} else {
		head = make([]byte, total)
		copy(head, dst)
	}
	return head, head[len(dst):]
}

// --- ChaCha20, RFC 8439 Abschnitt 2.3 -----------------------------------

// chachaConstants sind die vier Startwoerter, die im RFC als die Bytes von
// "expand 32-byte k" stehen.
var chachaConstants = [4]uint32{0x61707865, 0x3320646e, 0x79622d32, 0x6b206574}

// chachaXOR verschluesselt src nach dst - dasselbe in beide Richtungen, denn
// der Schluesselstrom wird nur daraufgelegt.
func chachaXOR(key *[ChaChaKeyLen]byte, nonce []byte, counter uint32, src, dst []byte) {
	var state [16]uint32
	state[0], state[1], state[2], state[3] = chachaConstants[0], chachaConstants[1],
		chachaConstants[2], chachaConstants[3]
	for i := 0; i < 8; i++ {
		state[4+i] = binary.LittleEndian.Uint32(key[i*4:])
	}
	state[13] = binary.LittleEndian.Uint32(nonce[0:])
	state[14] = binary.LittleEndian.Uint32(nonce[4:])
	state[15] = binary.LittleEndian.Uint32(nonce[8:])

	var block [chachaBlockLen]byte
	for offset := 0; offset < len(src); offset += chachaBlockLen {
		state[12] = counter
		counter++
		chachaBlock(&state, &block)
		end := offset + chachaBlockLen
		if end > len(src) {
			end = len(src)
		}
		for i := offset; i < end; i++ {
			dst[i] = src[i] ^ block[i-offset]
		}
	}
}

// chachaBlock rechnet die 20 Runden und schreibt die 64 Byte Schluesselstrom.
func chachaBlock(state *[16]uint32, out *[chachaBlockLen]byte) {
	var x [16]uint32
	copy(x[:], state[:])
	for round := 0; round < 10; round++ {
		// Spalten
		chachaQuarter(&x, 0, 4, 8, 12)
		chachaQuarter(&x, 1, 5, 9, 13)
		chachaQuarter(&x, 2, 6, 10, 14)
		chachaQuarter(&x, 3, 7, 11, 15)
		// Diagonalen
		chachaQuarter(&x, 0, 5, 10, 15)
		chachaQuarter(&x, 1, 6, 11, 12)
		chachaQuarter(&x, 2, 7, 8, 13)
		chachaQuarter(&x, 3, 4, 9, 14)
	}
	for i := 0; i < 16; i++ {
		binary.LittleEndian.PutUint32(out[i*4:], x[i]+state[i])
	}
}

func chachaQuarter(x *[16]uint32, a, b, c, d int) {
	x[a] += x[b]
	x[d] = rotateLeft32(x[d]^x[a], 16)
	x[c] += x[d]
	x[b] = rotateLeft32(x[b]^x[c], 12)
	x[a] += x[b]
	x[d] = rotateLeft32(x[d]^x[a], 8)
	x[c] += x[d]
	x[b] = rotateLeft32(x[b]^x[c], 7)
}

func rotateLeft32(v uint32, n uint) uint32 { return v<<n | v>>(32-n) }

// --- Poly1305, RFC 8439 Abschnitt 2.5 -----------------------------------
//
// Gerechnet wird modulo 2^130 - 5 in fuenf Gliedern von je 26 Bit. Die
// Aufteilung ist so gewaehlt, dass jedes Teilprodukt in 64 Bit passt und
// der Uebertrag erst am Ende ueber alle Glieder laeuft.

type poly1305 struct {
	r   [5]uint32
	s   [4]uint32 // die zweite Haelfte des Schluessels, am Ende addiert
	h   [5]uint32
	buf [16]byte
	n   int
}

func newPoly1305(key *[32]byte) *poly1305 {
	p := &poly1305{}
	// r wird beschnitten, wie der RFC es vorschreibt: die oberen Bits
	// jedes Gliedes fallen weg, damit die Multiplikation nicht ueberlaeuft.
	t0 := binary.LittleEndian.Uint32(key[0:])
	t1 := binary.LittleEndian.Uint32(key[4:])
	t2 := binary.LittleEndian.Uint32(key[8:])
	t3 := binary.LittleEndian.Uint32(key[12:])
	p.r[0] = t0 & 0x3ffffff
	p.r[1] = (t0>>26 | t1<<6) & 0x3ffff03
	p.r[2] = (t1>>20 | t2<<12) & 0x3ffc0ff
	p.r[3] = (t2>>14 | t3<<18) & 0x3f03fff
	p.r[4] = (t3 >> 8) & 0x00fffff
	for i := 0; i < 4; i++ {
		p.s[i] = binary.LittleEndian.Uint32(key[16+i*4:])
	}
	return p
}

func (p *poly1305) write(data []byte) {
	if p.n > 0 {
		take := copy(p.buf[p.n:], data)
		p.n += take
		data = data[take:]
		if p.n < 16 {
			return
		}
		p.blocks(p.buf[:], true)
		p.n = 0
	}
	full := len(data) - len(data)%16
	if full > 0 {
		p.blocks(data[:full], true)
	}
	if rest := data[full:]; len(rest) > 0 {
		p.n = copy(p.buf[:], rest)
	}
}

// blocks verarbeitet ganze Bloecke. `padded` sagt, ob das hohe Bit (2^128)
// gesetzt wird - beim letzten, kuerzeren Block uebernimmt das flush().
func (p *poly1305) blocks(data []byte, padded bool) {
	var hibit uint32
	if padded {
		hibit = 1 << 24
	}
	h0, h1, h2, h3, h4 := p.h[0], p.h[1], p.h[2], p.h[3], p.h[4]
	r0, r1, r2, r3, r4 := p.r[0], p.r[1], p.r[2], p.r[3], p.r[4]
	s1, s2, s3, s4 := r1*5, r2*5, r3*5, r4*5

	for len(data) >= 16 {
		t0 := binary.LittleEndian.Uint32(data[0:])
		t1 := binary.LittleEndian.Uint32(data[4:])
		t2 := binary.LittleEndian.Uint32(data[8:])
		t3 := binary.LittleEndian.Uint32(data[12:])
		h0 += t0 & 0x3ffffff
		h1 += (t0>>26 | t1<<6) & 0x3ffffff
		h2 += (t1>>20 | t2<<12) & 0x3ffffff
		h3 += (t2>>14 | t3<<18) & 0x3ffffff
		h4 += t3>>8 | hibit

		d0 := uint64(h0)*uint64(r0) + uint64(h1)*uint64(s4) + uint64(h2)*uint64(s3) +
			uint64(h3)*uint64(s2) + uint64(h4)*uint64(s1)
		d1 := uint64(h0)*uint64(r1) + uint64(h1)*uint64(r0) + uint64(h2)*uint64(s4) +
			uint64(h3)*uint64(s3) + uint64(h4)*uint64(s2)
		d2 := uint64(h0)*uint64(r2) + uint64(h1)*uint64(r1) + uint64(h2)*uint64(r0) +
			uint64(h3)*uint64(s4) + uint64(h4)*uint64(s3)
		d3 := uint64(h0)*uint64(r3) + uint64(h1)*uint64(r2) + uint64(h2)*uint64(r1) +
			uint64(h3)*uint64(r0) + uint64(h4)*uint64(s4)
		d4 := uint64(h0)*uint64(r4) + uint64(h1)*uint64(r3) + uint64(h2)*uint64(r2) +
			uint64(h3)*uint64(r1) + uint64(h4)*uint64(r0)

		carry := d0 >> 26
		h0 = uint32(d0) & 0x3ffffff
		d1 += carry
		carry = d1 >> 26
		h1 = uint32(d1) & 0x3ffffff
		d2 += carry
		carry = d2 >> 26
		h2 = uint32(d2) & 0x3ffffff
		d3 += carry
		carry = d3 >> 26
		h3 = uint32(d3) & 0x3ffffff
		d4 += carry
		carry = d4 >> 26
		h4 = uint32(d4) & 0x3ffffff
		// Was oben herausfaellt, kommt mal fuenf unten wieder herein -
		// das ist die Rechnung modulo 2^130 - 5.
		h0 += uint32(carry) * 5
		h1 += h0 >> 26
		h0 &= 0x3ffffff

		data = data[16:]
	}
	p.h[0], p.h[1], p.h[2], p.h[3], p.h[4] = h0, h1, h2, h3, h4
}

// flush schliesst ab: ein angefangener Block wird mit einer Eins begrenzt
// und mit Nullen aufgefuellt.
func (p *poly1305) sum() [ChaChaTagLen]byte {
	if p.n > 0 {
		p.buf[p.n] = 1
		for i := p.n + 1; i < 16; i++ {
			p.buf[i] = 0
		}
		p.blocks(p.buf[:], false)
	}

	h0, h1, h2, h3, h4 := p.h[0], p.h[1], p.h[2], p.h[3], p.h[4]
	// Uebertrag vollstaendig durchziehen.
	h1 += h0 >> 26
	h0 &= 0x3ffffff
	h2 += h1 >> 26
	h1 &= 0x3ffffff
	h3 += h2 >> 26
	h2 &= 0x3ffffff
	h4 += h3 >> 26
	h3 &= 0x3ffffff
	h0 += (h4 >> 26) * 5
	h4 &= 0x3ffffff
	h1 += h0 >> 26
	h0 &= 0x3ffffff

	// h - (2^130 - 5) rechnen und nur nehmen, wenn es nicht negativ wurde.
	g0 := h0 + 5
	g1 := h1 + g0>>26
	g0 &= 0x3ffffff
	g2 := h2 + g1>>26
	g1 &= 0x3ffffff
	g3 := h3 + g2>>26
	g2 &= 0x3ffffff
	g4 := h4 + g3>>26 - (1 << 26)
	g3 &= 0x3ffffff

	mask := (g4 >> 31) - 1 // 0xffffffff, wenn g nicht negativ ist
	h0 = h0&^mask | g0&mask
	h1 = h1&^mask | g1&mask
	h2 = h2&^mask | g2&mask
	h3 = h3&^mask | g3&mask
	h4 = h4&^mask | g4&mask

	// Zurueck in vier Woerter von 32 Bit und die zweite Schluesselhaelfte
	// addieren - das ist die Addition modulo 2^128.
	f0 := uint64(h0|h1<<26) + uint64(p.s[0])
	f1 := uint64(h1>>6|h2<<20) + uint64(p.s[1])
	f2 := uint64(h2>>12|h3<<14) + uint64(p.s[2])
	f3 := uint64(h3>>18|h4<<8) + uint64(p.s[3])
	f1 += f0 >> 32
	f2 += f1 >> 32
	f3 += f2 >> 32

	var tag [ChaChaTagLen]byte
	binary.LittleEndian.PutUint32(tag[0:], uint32(f0))
	binary.LittleEndian.PutUint32(tag[4:], uint32(f1))
	binary.LittleEndian.PutUint32(tag[8:], uint32(f2))
	binary.LittleEndian.PutUint32(tag[12:], uint32(f3))
	return tag
}

// chachaTag bildet das Siegel nach RFC 8439 Abschnitt 2.8: Beiwerk,
// Chiffrat, beides auf 16 Byte aufgefuellt, dann die beiden Laengen.
func chachaTag(key *[ChaChaKeyLen]byte, nonce, additional, ciphertext []byte) [ChaChaTagLen]byte {
	var otk [chachaBlockLen]byte
	chachaXOR(key, nonce, 0, otk[:], otk[:])
	var polyKey [32]byte
	copy(polyKey[:], otk[:32])

	p := newPoly1305(&polyKey)
	p.write(additional)
	p.write(chachaPadding(len(additional)))
	p.write(ciphertext)
	p.write(chachaPadding(len(ciphertext)))
	var lengths [16]byte
	binary.LittleEndian.PutUint64(lengths[0:], uint64(len(additional)))
	binary.LittleEndian.PutUint64(lengths[8:], uint64(len(ciphertext)))
	p.write(lengths[:])
	return p.sum()
}

var chachaZeros [16]byte

func chachaPadding(n int) []byte {
	if rest := n % 16; rest != 0 {
		return chachaZeros[:16-rest]
	}
	return nil
}
