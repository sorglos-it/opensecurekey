// Die selbst gerechneten Verfahren gegen VEROEFFENTLICHTE Zahlenbeispiele.
//
// Warum das hier wichtiger ist als ein Rundlauf: ein Rundlauf prueft nur,
// dass dieselbe Rechnung vorwaerts und rueckwaerts zusammenpasst. Waere die
// Rechnung falsch, waere sie beide Male gleich falsch, und alles waere
// gruen - genau die Falle, in die dieses Projekt am 29.10.2026 gelaufen ist.
// Diese Zahlen stammen nicht von hier: sie stehen in RFC 8439 (ChaCha20,
// Poly1305) und RFC 3394 (AES Key Wrap).
package classes

import (
	"bytes"
	"encoding/hex"
	"testing"
)

// fileHex liest eine Bytefolge aus der Schreibweise des RFC.
func fileHex(t *testing.T, text string) []byte {
	t.Helper()
	clean := make([]byte, 0, len(text))
	for i := 0; i < len(text); i++ {
		switch c := text[i]; {
		case c == ' ' || c == '\n' || c == '\t' || c == ':':
		default:
			clean = append(clean, c)
		}
	}
	out, err := hex.DecodeString(string(clean))
	if err != nil {
		t.Fatalf("Zahlenbeispiel unlesbar: %v", err)
	}
	return out
}

// rfc8439Plaintext ist der Satz, den beide Beispiele des RFC
// verschluesseln. Dass es derselbe ist, war nicht angenommen, sondern
// gemessen: mit einem anderen Verb wich das Chiffrat in GENAU EINEM Byte
// ab, und zwar um den Unterschied der beiden Buchstaben. Daran war zu
// sehen, dass der Satz falsch war und nicht die Rechnung.
const rfc8439Plaintext = "Ladies and Gentlemen of the class of '99: If I could offer you " +
	"only one tip for the future, sunscreen would be it."

// RFC 8439 Abschnitt 2.4.2: ChaCha20 allein.
func TestChaCha20RFC8439(t *testing.T) {
	var key [ChaChaKeyLen]byte
	for i := range key {
		key[i] = byte(i)
	}
	nonce := fileHex(t, "000000000000004a00000000")
	want := fileHex(t, `
		6e 2e 35 9a 25 68 f9 80 41 ba 07 28 dd 0d 69 81
		e9 7e 7a ec 1d 43 60 c2 0a 27 af cc fd 9f ae 0b
		f9 1b 65 c5 52 47 33 ab 8f 59 3d ab cd 62 b3 57
		16 39 d6 24 e6 51 52 ab 8f 53 0c 35 9f 08 61 d8
		07 ca 0d bf 50 0d 6a 61 56 a3 8e 08 8a 22 b6 5e
		52 bc 51 4d 16 cc f8 06 81 8c e9 1a b7 79 37 36
		5a f9 0b bf 74 a3 5b e6 b4 0b 8e ed f2 78 5e 42
		87 4d`)

	plain := []byte(rfc8439Plaintext)
	if len(plain) != len(want) {
		t.Fatalf("Klartext %d Byte, Beispiel %d Byte", len(plain), len(want))
	}
	got := make([]byte, len(plain))
	chachaXOR(&key, nonce, 1, plain, got)
	if !bytes.Equal(got, want) {
		t.Fatalf("ChaCha20 weicht vom RFC ab:\n got %x\nwill %x", got, want)
	}
}

// RFC 8439 Abschnitt 2.5.2: Poly1305 allein.
func TestPoly1305RFC8439(t *testing.T) {
	var key [32]byte
	copy(key[:], fileHex(t, `85:d6:be:78:57:55:6d:33:7f:44:52:fe:42:d5:06:a8:
		01:03:80:8a:fb:0d:b2:fd:4a:bf:f6:af:41:49:f5:1b`))
	want := fileHex(t, "a8:06:1d:c1:30:51:36:c6:c2:2b:8b:af:0c:01:27:a9")

	p := newPoly1305(&key)
	p.write([]byte("Cryptographic Forum Research Group"))
	got := p.sum()
	if !bytes.Equal(got[:], want) {
		t.Fatalf("Poly1305 weicht vom RFC ab:\n got %x\nwill %x", got, want)
	}
}

// Dasselbe, aber in krummen Stuecken geschrieben. Das Siegel muss gleich
// bleiben - sonst ist das abschnittsweise Arbeiten nicht zu gebrauchen.
func TestPoly1305StueckweiseGleich(t *testing.T) {
	var key [32]byte
	copy(key[:], fileHex(t, `85:d6:be:78:57:55:6d:33:7f:44:52:fe:42:d5:06:a8:
		01:03:80:8a:fb:0d:b2:fd:4a:bf:f6:af:41:49:f5:1b`))
	message := []byte("Cryptographic Forum Research Group")
	for _, cut := range []int{1, 3, 7, 15, 16, 17, 31, 33} {
		p := newPoly1305(&key)
		for offset := 0; offset < len(message); offset += cut {
			end := offset + cut
			if end > len(message) {
				end = len(message)
			}
			p.write(message[offset:end])
		}
		got := p.sum()
		p2 := newPoly1305(&key)
		p2.write(message)
		want := p2.sum()
		if got != want {
			t.Fatalf("Stuecke von %d Byte ergeben ein anderes Siegel", cut)
		}
	}
}

// RFC 8439 Abschnitt 2.8.2: das zusammengesetzte Verfahren.
func TestChaCha20Poly1305RFC8439(t *testing.T) {
	key := fileHex(t, `80 81 82 83 84 85 86 87 88 89 8a 8b 8c 8d 8e 8f
		90 91 92 93 94 95 96 97 98 99 9a 9b 9c 9d 9e 9f`)
	nonce := fileHex(t, "07 00 00 00 40 41 42 43 44 45 46 47")
	aad := fileHex(t, "50 51 52 53 c0 c1 c2 c3 c4 c5 c6 c7")
	want := fileHex(t, `
		d3 1a 8d 34 64 8e 60 db 7b 86 af bc 53 ef 7e c2
		a4 ad ed 51 29 6e 08 fe a9 e2 b5 a7 36 ee 62 d6
		3d be a4 5e 8c a9 67 12 82 fa fb 69 da 92 72 8b
		1a 71 de 0a 9e 06 0b 29 05 d6 a5 b6 7e cd 3b 36
		92 dd bd 7f 2d 77 8b 8c 98 03 ae e3 28 09 1b 58
		fa b3 24 e4 fa d6 75 94 55 85 80 8b 48 31 d7 bc
		3f f4 de f0 8e 4b 7a 9d e5 76 d2 65 86 ce c6 4b
		61 16`)
	wantTag := fileHex(t, "1a e1 0b 59 4f 09 e2 6a 7e 90 2e cb d0 60 06 91")

	aead, err := NewChaCha20Poly1305(key)
	if err != nil {
		t.Fatalf("Schluessel: %v", err)
	}
	sealed := aead.Seal(nil, nonce, []byte(rfc8439Plaintext), aad)
	body := sealed[:len(sealed)-ChaChaTagLen]
	tag := sealed[len(sealed)-ChaChaTagLen:]
	if !bytes.Equal(body, want) {
		t.Errorf("Chiffrat weicht vom RFC ab:\n got %x\nwill %x", body, want)
	}
	if !bytes.Equal(tag, wantTag) {
		t.Errorf("Siegel weicht vom RFC ab:\n got %x\nwill %x", tag, wantTag)
	}

	opened, err := aead.Open(nil, nonce, sealed, aad)
	if err != nil {
		t.Fatalf("Oeffnen: %v", err)
	}
	if string(opened) != rfc8439Plaintext {
		t.Fatal("geoeffnet kommt etwas anderes heraus")
	}
}

// Ein Siegel, das zu anderem Beiwerk gehoert, darf nicht gelten.
func TestChaChaBeiwerkGehoertDazu(t *testing.T) {
	key := make([]byte, ChaChaKeyLen)
	nonce := make([]byte, ChaChaNonceLen)
	aead, err := NewChaCha20Poly1305(key)
	if err != nil {
		t.Fatalf("Schluessel: %v", err)
	}
	sealed := aead.Seal(nil, nonce, []byte("Nachricht"), []byte("Beiwerk A"))
	if _, err := aead.Open(nil, nonce, sealed, []byte("Beiwerk B")); err == nil {
		t.Fatal("fremdes Beiwerk wurde angenommen")
	}
}

// RFC 3394 Abschnitt 4.1: der Umschlag mit einem 128-Bit-Schluessel.
func TestKeyWrapRFC3394(t *testing.T) {
	kek := fileHex(t, "000102030405060708090A0B0C0D0E0F")
	plain := fileHex(t, "00112233445566778899AABBCCDDEEFF")
	want := fileHex(t, "1FA68B0A8112B447AEF34BD8FB5A7B829D3E862371D2CFE5")

	wrapped, err := KeyWrap(kek, plain)
	if err != nil {
		t.Fatalf("Einpacken: %v", err)
	}
	if !bytes.Equal(wrapped, want) {
		t.Fatalf("Umschlag weicht vom RFC ab:\n got %x\nwill %x", wrapped, want)
	}
	back, err := KeyUnwrap(kek, wrapped)
	if err != nil {
		t.Fatalf("Oeffnen: %v", err)
	}
	if !bytes.Equal(back, plain) {
		t.Fatal("geoeffnet kommt etwas anderes heraus")
	}
}

// Ein verfaelschter Umschlag faellt auf - das ist seine zweite Aufgabe
// neben dem Einpacken.
func TestKeyWrapMerktVerfaelschung(t *testing.T) {
	kek := fileHex(t, "000102030405060708090A0B0C0D0E0F")
	plain := make([]byte, pgpSessionLen)
	wrapped, err := KeyWrap(kek, plain)
	if err != nil {
		t.Fatalf("Einpacken: %v", err)
	}
	for position := range wrapped {
		kaputt := append([]byte{}, wrapped...)
		kaputt[position] ^= 0x01
		if _, err := KeyUnwrap(kek, kaputt); err == nil {
			t.Fatalf("verfaelschtes Byte %d wurde angenommen", position)
		}
	}
	anderer := fileHex(t, "0f0e0d0c0b0a09080706050403020100")
	if _, err := KeyUnwrap(anderer, wrapped); err == nil {
		t.Fatal("falscher Schluessel hat den Umschlag geoeffnet")
	}
}
