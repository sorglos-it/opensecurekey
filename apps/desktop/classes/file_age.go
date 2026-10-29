// Das Format age (age-encryption.org/v1), nach seiner oeffentlichen
// Beschreibung gebaut. Kein fremdes Paket; gerechnet wird mit crypto/ecdh,
// crypto/hkdf und crypto/hmac aus der Standardbibliothek und mit dem
// ChaCha20-Poly1305 aus chacha.go.
//
// Aufbau einer Datei:
//
//	age-encryption.org/v1
//	-> X25519 <fluechtiger Punkt>
//	<eingepackter Dateischluessel>
//	--- <Siegel ueber den ganzen Kopf>
//	<16 Byte Salz><Abschnitt><Abschnitt>...
//
// Zwei Stellen, an denen das Format mehr verlangt als "verschluesseln":
//
//   - Der Kopf traegt ein Siegel (HMAC). Ohne es koennte jemand einen
//     Empfaenger hinzufuegen oder wegnehmen, ohne dass es auffaellt.
//   - Der letzte Abschnitt ist als letzter GEKENNZEICHNET. Darum ist eine
//     abgeschnittene Datei ein Fehler und nicht einfach eine kuerzere Datei.
//     Das ist der Grund, warum hier nicht einfach Abschnitt um Abschnitt
//     entschluesselt wird, bis der Strom endet.
package classes

import (
	"bufio"
	"bytes"
	"crypto/hkdf"
	"crypto/hmac"
	"crypto/sha256"
	"encoding/base64"
	"encoding/binary"
	"io"
	"strings"
)

// Die festen Groessen des Formats, in Byte.
const (
	ageFileKeyLen = 16 // der Dateischluessel, aus dem alles Weitere kommt
	ageSaltLen    = 16 // Salz vor dem ersten Abschnitt
	ageMacLen     = 32 // HMAC-SHA256 ueber den Kopf
	ageWrapLen    = 32 // eingepackter Dateischluessel: 16 + Siegel
	ageWrapColumn = 64 // so breit schreibt age die Zeilen eines Fachs
)

// Die Zeichenketten des Formats. Sie stehen genau so in der Beschreibung;
// eine andere Schreibweise ergibt eine Datei, die kein anderes Programm
// lesen kann.
const (
	ageHeaderLine   = "age-encryption.org/v1"
	ageStanzaStart  = "->"
	ageFooterStart  = "---"
	ageStanzaX25519 = "X25519"
	ageInfoX25519   = "age-encryption.org/v1/X25519"
	ageInfoHeader   = "header"
	ageInfoPayload  = "payload"
)

// ageBase64 ist die Schreibweise des Formats: normale Zeichen, keine
// Auffuellzeichen.
var ageBase64 = base64.RawStdEncoding

// ageEncrypt schreibt eine age-Datei.
func ageEncrypt(recipients []FileRecipient, in io.Reader, out io.Writer) error {
	fileKey, err := fileRandom(ageFileKeyLen)
	if err != nil {
		return err
	}
	defer Wipe(fileKey)

	var header bytes.Buffer
	header.WriteString(ageHeaderLine)
	header.WriteByte('\n')
	for _, recipient := range recipients {
		stanza, err := ageWrapFileKey(recipient, fileKey)
		if err != nil {
			return err
		}
		header.Write(stanza)
	}
	header.WriteString(ageFooterStart)

	mac, err := ageHeaderMac(fileKey, header.Bytes())
	if err != nil {
		return err
	}
	if _, err := out.Write(header.Bytes()); err != nil {
		return Fail("error.file.write")
	}
	if _, err := io.WriteString(out, " "+ageBase64.EncodeToString(mac)+"\n"); err != nil {
		return Fail("error.file.write")
	}
	return agePayloadWrite(fileKey, in, out)
}

// ageWrapFileKey packt den Dateischluessel fuer einen Empfaenger ein und
// schreibt das Fach dafuer.
//
// Das fluechtige Schluesselpaar ist je Empfaenger und je Datei neu. Das Salz
// der Ableitung sind beide Punkte zusammen - so gehoert ein eingepackter
// Schluessel zu genau diesem Paar und laesst sich nicht in eine andere Datei
// umhaengen.
func ageWrapFileKey(recipient FileRecipient, fileKey []byte) ([]byte, error) {
	peer, err := ecdhPublic(recipient.Public)
	if err != nil {
		return nil, err
	}
	ephemeral, err := fileEphemeral()
	if err != nil {
		return nil, err
	}
	shared, err := ephemeral.ECDH(peer)
	if err != nil {
		return nil, Fail("error.file.recipient")
	}
	defer Wipe(shared)

	ephemeralPublic := ephemeral.PublicKey().Bytes()
	salt := append(append([]byte{}, ephemeralPublic...), recipient.Public...)
	wrapKey, err := hkdf.Key(sha256.New, shared, salt, ageInfoX25519, ChaChaKeyLen)
	if err != nil {
		return nil, Fail("error.file.internal")
	}
	defer Wipe(wrapKey)

	aead, err := NewChaCha20Poly1305(wrapKey)
	if err != nil {
		return nil, err
	}
	// Die Nonce ist hier Null, und das ist richtig: der Schluessel wird aus
	// einem je Datei neuen fluechtigen Punkt abgeleitet und genau einmal
	// benutzt.
	var zeroNonce [ChaChaNonceLen]byte
	wrapped := aead.Seal(nil, zeroNonce[:], fileKey, nil)

	var stanza strings.Builder
	stanza.WriteString(ageStanzaStart + " " + ageStanzaX25519 + " ")
	stanza.WriteString(ageBase64.EncodeToString(ephemeralPublic))
	stanza.WriteByte('\n')
	stanza.WriteString(ageWrapLines(ageBase64.EncodeToString(wrapped)))
	return []byte(stanza.String()), nil
}

// ageWrapLines bricht den Rumpf eines Fachs auf die Breite des Formats um.
// Geht er genau auf, kommt eine leere Zeile dazu - daran erkennt der Leser
// das Ende, und ohne sie waere das Fach nicht eindeutig zu Ende.
func ageWrapLines(text string) string {
	var b strings.Builder
	for len(text) > ageWrapColumn {
		b.WriteString(text[:ageWrapColumn])
		b.WriteByte('\n')
		text = text[ageWrapColumn:]
	}
	b.WriteString(text)
	b.WriteByte('\n')
	return b.String()
}

// ageHeaderMac siegelt den Kopf. Der Schluessel dafuer kommt aus dem
// Dateischluessel - wer den Kopf aendert, muesste ihn kennen.
func ageHeaderMac(fileKey, header []byte) ([]byte, error) {
	key, err := hkdf.Key(sha256.New, fileKey, nil, ageInfoHeader, ageMacLen)
	if err != nil {
		return nil, Fail("error.file.internal")
	}
	defer Wipe(key)
	mac := hmac.New(sha256.New, key)
	mac.Write(header)
	return mac.Sum(nil), nil
}

// agePayloadWrite schreibt das Salz und danach die Abschnitte.
func agePayloadWrite(fileKey []byte, in io.Reader, out io.Writer) error {
	salt, err := fileRandom(ageSaltLen)
	if err != nil {
		return err
	}
	if _, err := out.Write(salt); err != nil {
		return Fail("error.file.write")
	}
	streamKey, err := hkdf.Key(sha256.New, fileKey, salt, ageInfoPayload, ChaChaKeyLen)
	if err != nil {
		return Fail("error.file.internal")
	}
	defer Wipe(streamKey)
	aead, err := NewChaCha20Poly1305(streamKey)
	if err != nil {
		return err
	}

	// Gepuffert gelesen, damit ein Blick nach vorn moeglich ist: ob ein
	// Abschnitt der letzte ist, entscheidet nicht seine Laenge, sondern ob
	// noch etwas kommt. Eine Datei, die genau auf ein Vielfaches der
	// Abschnittslaenge aufgeht, haette sonst einen leeren letzten
	// Abschnitt - und den verbietet das Format.
	source := bufio.NewReaderSize(in, FileChunkSize)
	buf := make([]byte, FileChunkSize)
	sealed := make([]byte, 0, FileChunkSize+ChaChaTagLen)
	var counter uint64
	for {
		n, readErr := io.ReadFull(source, buf)
		if readErr != nil && readErr != io.EOF && readErr != io.ErrUnexpectedEOF {
			return Fail("error.file.read")
		}
		last := true
		if _, err := source.Peek(1); err == nil {
			last = false
		}
		nonce := ageStreamNonce(counter, last)
		sealed = aead.Seal(sealed[:0], nonce[:], buf[:n], nil)
		if _, err := out.Write(sealed); err != nil {
			return Fail("error.file.write")
		}
		if last {
			return nil
		}
		counter++
		if counter == ageStreamMaxCounter {
			return Fail("error.file.toolong")
		}
	}
}

// ageStreamMaxCounter ist die Grenze des Zaehlers: er ist 11 Byte breit, aber
// hier reichen 64 Bit, und darueber hinaus wird abgebrochen statt
// stillschweigend eine Nonce zu wiederholen.
const ageStreamMaxCounter = ^uint64(0)

// ageStreamNonce baut die Nonce eines Abschnitts: elf Byte Zaehler, dann ein
// Byte, das den letzten Abschnitt kennzeichnet.
func ageStreamNonce(counter uint64, last bool) [ChaChaNonceLen]byte {
	var nonce [ChaChaNonceLen]byte
	binary.BigEndian.PutUint64(nonce[3:11], counter)
	if last {
		nonce[11] = 1
	}
	return nonce
}

// ageDecrypt liest eine age-Datei.
func ageDecrypt(identity FileIdentity, in io.Reader, out io.Writer) error {
	reader := bufio.NewReaderSize(in, 4096)
	header, stanzas, err := ageReadHeader(reader)
	if err != nil {
		return err
	}
	fileKey, err := ageUnwrapFileKey(identity, stanzas)
	if err != nil {
		return err
	}
	defer Wipe(fileKey)

	mac, err := ageHeaderMac(fileKey, header.bytes)
	if err != nil {
		return err
	}
	if !hmac.Equal(mac, header.mac) {
		return Fail("error.file.tampered")
	}
	return agePayloadRead(fileKey, reader, out)
}

// ageStanza ist ein Fach des Kopfes.
type ageStanza struct {
	kind string
	args []string
	body []byte
}

// ageHeaderBytes sind die gelesenen Kopfbytes, so wie sie gesiegelt wurden,
// und das Siegel selbst.
type ageHeaderBytes struct {
	bytes []byte
	mac   []byte
}

// ageReadHeader liest den Kopf Zeile um Zeile. Gelesen wird streng: eine
// unbekannte erste Zeile, ein Fach ohne Art, eine zu lange Zeile oder ein
// Siegel falscher Laenge sind Fehler.
func ageReadHeader(reader *bufio.Reader) (ageHeaderBytes, []ageStanza, error) {
	var raw bytes.Buffer
	line, err := ageReadLine(reader)
	if err != nil {
		return ageHeaderBytes{}, nil, err
	}
	if line != ageHeaderLine {
		return ageHeaderBytes{}, nil, Fail("error.file.format")
	}
	raw.WriteString(line)
	raw.WriteByte('\n')

	var stanzas []ageStanza
	var current *ageStanza
	for {
		line, err := ageReadLine(reader)
		if err != nil {
			return ageHeaderBytes{}, nil, err
		}
		switch {
		case strings.HasPrefix(line, ageFooterStart):
			raw.WriteString(ageFooterStart)
			rest := strings.TrimPrefix(line, ageFooterStart)
			if !strings.HasPrefix(rest, " ") {
				return ageHeaderBytes{}, nil, Fail("error.file.format")
			}
			mac, err := ageBase64.DecodeString(strings.TrimPrefix(rest, " "))
			if err != nil || len(mac) != ageMacLen {
				return ageHeaderBytes{}, nil, Fail("error.file.format")
			}
			if current != nil {
				stanzas = append(stanzas, *current)
			}
			return ageHeaderBytes{bytes: raw.Bytes(), mac: mac}, stanzas, nil
		case strings.HasPrefix(line, ageStanzaStart+" "):
			if current != nil {
				stanzas = append(stanzas, *current)
			}
			parts := strings.Split(strings.TrimPrefix(line, ageStanzaStart+" "), " ")
			if len(parts) == 0 || parts[0] == "" {
				return ageHeaderBytes{}, nil, Fail("error.file.format")
			}
			current = &ageStanza{kind: parts[0], args: parts[1:]}
			raw.WriteString(line)
			raw.WriteByte('\n')
		default:
			if current == nil {
				return ageHeaderBytes{}, nil, Fail("error.file.format")
			}
			if len(line) > ageWrapColumn {
				return ageHeaderBytes{}, nil, Fail("error.file.format")
			}
			chunk, err := ageBase64.DecodeString(line)
			if err != nil {
				return ageHeaderBytes{}, nil, Fail("error.file.format")
			}
			current.body = append(current.body, chunk...)
			raw.WriteString(line)
			raw.WriteByte('\n')
			if len(line) < ageWrapColumn {
				// Fach zu Ende; das naechste muss ein `->` oder `---` sein.
				stanzas = append(stanzas, *current)
				current = nil
			}
		}
	}
}

// ageReadLine liest eine Zeile ohne Zeilenende. Ein Strom, der im Kopf
// endet, ist ein Fehler.
func ageReadLine(reader *bufio.Reader) (string, error) {
	line, err := reader.ReadString('\n')
	if err != nil {
		return "", Fail("error.file.format")
	}
	return strings.TrimSuffix(line, "\n"), nil
}

// ageUnwrapFileKey probiert jedes Fach, das zu diesem Verfahren gehoert.
// Passt keines, heisst das: diese Datei ist nicht fuer diese Kennung.
func ageUnwrapFileKey(identity FileIdentity, stanzas []ageStanza) ([]byte, error) {
	own := identity.Public()
	if len(own) != FileKeyLen {
		return nil, Fail("error.file.identity")
	}
	seen := false
	for _, stanza := range stanzas {
		if stanza.kind != ageStanzaX25519 || len(stanza.args) != 1 {
			continue
		}
		ephemeralPublic, err := ageBase64.DecodeString(stanza.args[0])
		if err != nil || len(ephemeralPublic) != FileKeyLen {
			return nil, Fail("error.file.format")
		}
		if len(stanza.body) != ageWrapLen {
			return nil, Fail("error.file.format")
		}
		seen = true
		// Hier rechnet das GERAET. Das Programm sieht nur das Ergebnis,
		// und das ist je Datei ein anderer Wert.
		shared, err := identity.Agree(ephemeralPublic)
		if err != nil {
			return nil, err
		}
		salt := append(append([]byte{}, ephemeralPublic...), own...)
		wrapKey, hkdfErr := hkdf.Key(sha256.New, shared, salt, ageInfoX25519, ChaChaKeyLen)
		Wipe(shared)
		if hkdfErr != nil {
			return nil, Fail("error.file.internal")
		}
		aead, err := NewChaCha20Poly1305(wrapKey)
		Wipe(wrapKey)
		if err != nil {
			return nil, err
		}
		var zeroNonce [ChaChaNonceLen]byte
		fileKey, err := aead.Open(nil, zeroNonce[:], stanza.body, nil)
		if err != nil {
			continue // dieses Fach gehoert einem anderen Empfaenger
		}
		if len(fileKey) != ageFileKeyLen {
			return nil, Fail("error.file.format")
		}
		return fileKey, nil
	}
	if !seen {
		return nil, Fail("error.file.norecipient")
	}
	return nil, Fail("error.file.wrongkey")
}

// agePayloadRead entschluesselt die Abschnitte. Der Strom muss mit einem als
// letztem gekennzeichneten Abschnitt enden; sonst ist die Datei
// abgeschnitten, und das wird gemeldet.
func agePayloadRead(fileKey []byte, reader *bufio.Reader, out io.Writer) error {
	salt := make([]byte, ageSaltLen)
	if _, err := io.ReadFull(reader, salt); err != nil {
		return Fail("error.file.format")
	}
	streamKey, err := hkdf.Key(sha256.New, fileKey, salt, ageInfoPayload, ChaChaKeyLen)
	if err != nil {
		return Fail("error.file.internal")
	}
	defer Wipe(streamKey)
	aead, err := NewChaCha20Poly1305(streamKey)
	if err != nil {
		return err
	}

	sealedLen := FileChunkSize + ChaChaTagLen
	buf := make([]byte, sealedLen)
	plain := make([]byte, 0, FileChunkSize)
	var counter uint64
	for {
		n, readErr := io.ReadFull(reader, buf)
		atEnd := readErr == io.EOF || readErr == io.ErrUnexpectedEOF
		if readErr != nil && !atEnd {
			return Fail("error.file.read")
		}
		if n == 0 {
			// Der Strom ist zu Ende, aber kein Abschnitt war als letzter
			// gekennzeichnet: abgeschnitten.
			return Fail("error.file.truncated")
		}
		if n < ChaChaTagLen {
			return Fail("error.file.tampered")
		}
		// Ein voller Abschnitt kann der letzte sein oder nicht; das sagt
		// die Nonce. Es wird beides probiert, und nur eines gilt.
		last := n < sealedLen
		nonce := ageStreamNonce(counter, last)
		opened, err := aead.Open(plain[:0], nonce[:], buf[:n], nil)
		if err != nil && !last {
			// Vielleicht ist es doch der letzte Abschnitt und nur
			// zufaellig genau so lang.
			lastNonce := ageStreamNonce(counter, true)
			opened, err = aead.Open(plain[:0], lastNonce[:], buf[:n], nil)
			if err == nil {
				last = true
			}
		}
		if err != nil {
			return Fail("error.file.tampered")
		}
		if len(opened) == 0 && counter > 0 {
			// Ein leerer Abschnitt ist nur erlaubt, wenn er der einzige ist.
			return Fail("error.file.tampered")
		}
		if _, err := out.Write(opened); err != nil {
			return Fail("error.file.write")
		}
		if last {
			if !atEnd {
				// Nach dem letzten Abschnitt darf nichts mehr kommen.
				if _, err := reader.Peek(1); err == nil {
					return Fail("error.file.tampered")
				}
			}
			return nil
		}
		counter++
	}
}
