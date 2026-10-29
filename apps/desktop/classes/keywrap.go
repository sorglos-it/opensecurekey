// AES Key Wrap nach RFC 3394, selbst gerechnet.
//
// Gebraucht fuer OpenPGP: dort wird der Sitzungsschluessel einer Nachricht
// nicht mit einem AEAD-Verfahren eingepackt, sondern mit diesem
// Umschlagverfahren (RFC 9580 Abschnitt 12.5). Die Standardbibliothek von Go
// hat AES, aber nicht den Umschlag.
//
// Das Verfahren ist kurz: sechs Durchgaenge ueber alle Glieder, in jedem
// Schritt wird ein 64-Bit-Pruefwert mit dem naechsten Glied durch AES
// geschickt, und der Schrittzaehler geht in den Pruefwert ein. Beim Oeffnen
// laeuft dasselbe rueckwaerts; stimmt der Pruefwert am Ende nicht, ist der
// Umschlag verfaelscht oder der Schluessel falsch.
//
// Geprueft gegen die Zahlenbeispiele aus RFC 3394 Abschnitt 4.
package classes

import (
	"crypto/aes"
	"crypto/subtle"
	"encoding/binary"
)

// keyWrapIV ist der vorgegebene Anfangswert aus RFC 3394 Abschnitt 2.2.3.1.
// An ihm erkennt das Oeffnen, dass es geklappt hat.
var keyWrapIV = [8]byte{0xa6, 0xa6, 0xa6, 0xa6, 0xa6, 0xa6, 0xa6, 0xa6}

// KeyWrap packt `plain` mit `kek` ein. `plain` muss ein Vielfaches von 8
// Byte sein und mindestens 16 Byte lang - kuerzer kennt das Verfahren nicht.
func KeyWrap(kek, plain []byte) ([]byte, error) {
	if len(plain) < 16 || len(plain)%8 != 0 {
		return nil, Fail("error.file.key")
	}
	block, err := aes.NewCipher(kek)
	if err != nil {
		return nil, Fail("error.file.key")
	}
	n := len(plain) / 8
	out := make([]byte, len(plain)+8)
	copy(out[0:8], keyWrapIV[:])
	copy(out[8:], plain)

	var buf [16]byte
	for round := 0; round < 6; round++ {
		for i := 1; i <= n; i++ {
			copy(buf[0:8], out[0:8])
			copy(buf[8:16], out[i*8:])
			block.Encrypt(buf[:], buf[:])
			// Der Schrittzaehler macht gleiche Glieder unterscheidbar.
			step := uint64(round*n + i)
			var counter [8]byte
			binary.BigEndian.PutUint64(counter[:], step)
			for j := 0; j < 8; j++ {
				buf[j] ^= counter[j]
			}
			copy(out[0:8], buf[0:8])
			copy(out[i*8:], buf[8:16])
		}
	}
	return out, nil
}

// KeyUnwrap oeffnet einen Umschlag. Stimmt der Pruefwert nicht, kommt
// errFileTamper - dieselbe Meldung wie bei einer verfaelschten Datei, denn
// es ist derselbe Befund.
func KeyUnwrap(kek, wrapped []byte) ([]byte, error) {
	if len(wrapped) < 24 || len(wrapped)%8 != 0 {
		return nil, errFileTamper
	}
	block, err := aes.NewCipher(kek)
	if err != nil {
		return nil, Fail("error.file.key")
	}
	n := len(wrapped)/8 - 1
	work := make([]byte, len(wrapped))
	copy(work, wrapped)

	var buf [16]byte
	for round := 5; round >= 0; round-- {
		for i := n; i >= 1; i-- {
			copy(buf[0:8], work[0:8])
			step := uint64(round*n + i)
			var counter [8]byte
			binary.BigEndian.PutUint64(counter[:], step)
			for j := 0; j < 8; j++ {
				buf[j] ^= counter[j]
			}
			copy(buf[8:16], work[i*8:])
			block.Decrypt(buf[:], buf[:])
			copy(work[0:8], buf[0:8])
			copy(work[i*8:], buf[8:16])
		}
	}
	if subtle.ConstantTimeCompare(work[0:8], keyWrapIV[:]) != 1 {
		return nil, errFileTamper
	}
	return work[8:], nil
}
