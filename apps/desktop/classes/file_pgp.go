// Das Format OpenPGP (RFC 9580), nach seiner oeffentlichen Beschreibung
// gebaut. Kein fremdes Paket; gerechnet wird mit crypto/ecdh, crypto/hkdf und
// crypto/aes samt GCM aus der Standardbibliothek und mit dem Umschlag aus
// keywrap.go.
//
// Geschrieben wird die neue Form des Standards, nicht die alte:
//
//   - Sitzungsschluesselpaket Fassung 6 mit X25519 (Verfahren 25). Der
//     Empfaenger wird NICHT genannt - das Feld fuer seinen Fingerabdruck
//     bleibt leer. Das ist im Standard erlaubt und hier das Richtige: eine
//     Datei soll nicht verraten, fuer welchen Stick sie ist.
//   - Datenpaket Fassung 2 mit AES-256-GCM, abschnittsweise. Jeder Abschnitt
//     traegt sein eigenes Siegel, und am Ende steht ein Siegel ueber die
//     Gesamtlaenge - daran faellt eine abgeschnittene Datei auf.
//
// Warum diese Wahl und nicht die alte Form mit cv25519: das gemeinsame
// Geheimnis, das der Chip liefert, ist das rohe X25519-Ergebnis
// (osk_se050_ecdh). Die neue Form verlangt genau das und leitet selbst mit
// HKDF weiter. Die alte Form zieht noch den Fingerabdruck des Schluessels in
// die Ableitung ein - dafuer braeuchte es ein vollstaendiges, selbst
// unterschriebenes Schluesselpaket, und unterschreiben kann dieses Geraet
// nur mit vorangestelltem Zweck (NodeSign). Die alte Form ist also nicht
// einfach altmodisch, sie ist mit diesem Geraet nicht erreichbar.
package classes

import (
	"bufio"
	"crypto/aes"
	"crypto/cipher"
	"crypto/hkdf"
	"crypto/sha256"
	"encoding/binary"
	"io"
)

// Die Zahlen des Standards. Jede steht nur hier.
const (
	pgpTagPKESK   = 1  // Sitzungsschluessel fuer einen Empfaenger
	pgpTagSEIPD   = 18 // verschluesselte Daten mit Siegel
	pgpTagLiteral = 11 // die Nutzdaten selbst

	pgpPKESKVersion = 6
	pgpSEIPDVersion = 2

	pgpAlgoX25519 = 25 // oeffentliches Verfahren
	pgpAlgoAES256 = 9  // Blockverfahren
	pgpAlgoGCM    = 3  // AEAD-Verfahren

	pgpSaltLen     = 32 // Salz im Datenpaket
	pgpSessionLen  = 32 // Sitzungsschluessel fuer AES-256
	pgpKEKLen      = 16 // Umschlagschluessel: AES-128 Key Wrap
	pgpWrappedLen  = pgpSessionLen + 8
	pgpGCMNonceLen = 12
	pgpGCMTagLen   = 16

	pgpInfoX25519 = "OpenPGP X25519"

	// pgpChunkBits ist die Potenz, die im Kopf des Datenpakets steht:
	// Abschnittslaenge = 1 << (pgpChunkBits + 6). Sie wird aus
	// FileChunkSize gerechnet, damit die Zahl nicht zweimal dasteht.
	pgpChunkBits = 10
)

// pgpLiteralBinary ist die Kennzeichnung "rohe Bytes" - nicht "Text", denn
// bei Text darf ein Leser Zeilenenden umschreiben, und dann kommt nicht mehr
// dasselbe heraus.
const pgpLiteralBinary = 'b'

// pgpChunkSize ist die Abschnittslaenge, wie der Kopf sie ansagt. Dass sie
// FileChunkSize entspricht, prueft ein Fall.
const pgpChunkSize = 1 << (pgpChunkBits + 6)

// pgpEncrypt schreibt eine OpenPGP-Nachricht.
func pgpEncrypt(recipients []FileRecipient, in io.Reader, out io.Writer) error {
	sessionKey, err := fileRandom(pgpSessionLen)
	if err != nil {
		return err
	}
	defer Wipe(sessionKey)

	for _, recipient := range recipients {
		packet, err := pgpSessionPacket(recipient, sessionKey)
		if err != nil {
			return err
		}
		if _, err := out.Write(packet); err != nil {
			return Fail("error.file.write")
		}
	}
	return pgpDataPacket(sessionKey, in, out)
}

// pgpSessionPacket baut das Sitzungsschluesselpaket fuer einen Empfaenger.
func pgpSessionPacket(recipient FileRecipient, sessionKey []byte) ([]byte, error) {
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

	kek, err := pgpWrapKey(shared, ephemeralPublic, recipient.Public)
	if err != nil {
		return nil, err
	}
	defer Wipe(kek)
	wrapped, err := KeyWrap(kek, sessionKey)
	if err != nil {
		return nil, err
	}

	body := make([]byte, 0, 80)
	body = append(body, pgpPKESKVersion)
	body = append(body, 0) // kein Fingerabdruck: Empfaenger nicht genannt
	body = append(body, pgpAlgoX25519)
	body = append(body, ephemeralPublic...)
	body = append(body, byte(len(wrapped)))
	body = append(body, wrapped...)
	return pgpPacket(pgpTagPKESK, body), nil
}

// pgpWrapKey leitet den Umschlagschluessel ab. Die drei Groessen gehen in
// genau dieser Reihenfolge hinein (RFC 9580 Abschnitt 12.5) - eine andere
// Reihenfolge ergibt einen anderen Schluessel und damit eine Nachricht, die
// niemand sonst oeffnen kann.
func pgpWrapKey(shared, ephemeralPublic, recipientPublic []byte) ([]byte, error) {
	ikm := make([]byte, 0, len(shared)+2*FileKeyLen)
	ikm = append(ikm, shared...)
	ikm = append(ikm, ephemeralPublic...)
	ikm = append(ikm, recipientPublic...)
	defer Wipe(ikm)
	kek, err := hkdf.Key(sha256.New, ikm, nil, pgpInfoX25519, pgpKEKLen)
	if err != nil {
		return nil, Fail("error.file.internal")
	}
	return kek, nil
}

// pgpDataPacket schreibt das Datenpaket: Kopf, Salz, dann die Abschnitte.
func pgpDataPacket(sessionKey []byte, in io.Reader, out io.Writer) error {
	salt, err := fileRandom(pgpSaltLen)
	if err != nil {
		return err
	}
	header := []byte{pgpSEIPDVersion, pgpAlgoAES256, pgpAlgoGCM, pgpChunkBits}
	aead, iv, err := pgpAEAD(sessionKey, salt, header)
	if err != nil {
		return err
	}
	defer Wipe(iv)

	// Der Rumpf des Datenpakets ist erst am Ende lang bekannt, darum wird
	// er in Teilstuecken geschrieben - das sieht der Standard dafuer vor.
	body := newPgpPartialWriter(out, pgpTagSEIPD)
	if _, err := body.Write(header); err != nil {
		return err
	}
	if _, err := body.Write(salt); err != nil {
		return err
	}

	// Verschluesselt wird nicht die Datei, sondern das Nutzdatenpaket um
	// sie herum - auch das in Teilstuecken, denn seine Laenge ist genauso
	// unbekannt.
	plain := newPgpChunkWriter(body, aead, iv, header)
	literal := newPgpPartialWriter(plain, pgpTagLiteral)
	literalHeader := []byte{pgpLiteralBinary, 0, 0, 0, 0, 0}
	if _, err := literal.Write(literalHeader); err != nil {
		return err
	}
	if _, err := io.Copy(literal, in); err != nil {
		return Fail("error.file.read")
	}
	if err := literal.Close(); err != nil {
		return err
	}
	if err := plain.Close(); err != nil {
		return err
	}
	return body.Close()
}

// pgpAEAD leitet Schluessel und Nonce-Anfang ab. Der Kopf des Datenpakets
// geht in die Ableitung ein: wer darin die Verfahren aendert, bekommt einen
// anderen Schluessel und damit kein lesbares Ergebnis.
func pgpAEAD(sessionKey, salt, header []byte) (cipher.AEAD, []byte, error) {
	info := append([]byte{0xc0 | pgpTagSEIPD}, header...)
	material, err := hkdf.Key(sha256.New, sessionKey, salt, string(info),
		pgpSessionLen+pgpGCMNonceLen-8)
	if err != nil {
		return nil, nil, Fail("error.file.internal")
	}
	defer Wipe(material)
	block, err := aes.NewCipher(material[:pgpSessionLen])
	if err != nil {
		return nil, nil, Fail("error.file.internal")
	}
	aead, err := cipher.NewGCM(block)
	if err != nil {
		return nil, nil, Fail("error.file.internal")
	}
	iv := make([]byte, pgpGCMNonceLen-8)
	copy(iv, material[pgpSessionLen:])
	return aead, iv, nil
}

// pgpChunkNonce baut die Nonce eines Abschnitts: der feste Anfang, dann die
// Nummer des Abschnitts.
func pgpChunkNonce(iv []byte, index uint64) []byte {
	nonce := make([]byte, pgpGCMNonceLen)
	copy(nonce, iv)
	binary.BigEndian.PutUint64(nonce[len(iv):], index)
	return nonce
}

// pgpAAD ist das Beiwerk eines Abschnitts: der Kopf des Pakets.
func pgpAAD(header []byte) []byte {
	return append([]byte{0xc0 | pgpTagSEIPD}, header...)
}

// pgpFinalAAD ist das Beiwerk des Schlusssiegels: derselbe Kopf und die
// Gesamtlaenge des Klartexts. Darum faellt eine abgeschnittene Nachricht
// auf, auch wenn jeder einzelne Abschnitt fuer sich stimmt.
func pgpFinalAAD(header []byte, total uint64) []byte {
	aad := pgpAAD(header)
	var length [8]byte
	binary.BigEndian.PutUint64(length[:], total)
	return append(aad, length[:]...)
}

// pgpChunkWriter nimmt Klartext und schreibt versiegelte Abschnitte.
type pgpChunkWriter struct {
	out    io.Writer
	aead   cipher.AEAD
	iv     []byte
	header []byte
	buf    []byte
	index  uint64
	total  uint64
}

func newPgpChunkWriter(out io.Writer, aead cipher.AEAD, iv, header []byte) *pgpChunkWriter {
	return &pgpChunkWriter{out: out, aead: aead, iv: iv, header: header,
		buf: make([]byte, 0, pgpChunkSize)}
}

func (w *pgpChunkWriter) Write(data []byte) (int, error) {
	written := len(data)
	for len(data) > 0 {
		room := pgpChunkSize - len(w.buf)
		take := len(data)
		if take > room {
			take = room
		}
		w.buf = append(w.buf, data[:take]...)
		data = data[take:]
		if len(w.buf) == pgpChunkSize {
			if err := w.flush(); err != nil {
				return 0, err
			}
		}
	}
	return written, nil
}

func (w *pgpChunkWriter) flush() error {
	sealed := w.aead.Seal(nil, pgpChunkNonce(w.iv, w.index), w.buf, pgpAAD(w.header))
	if _, err := w.out.Write(sealed); err != nil {
		return Fail("error.file.write")
	}
	w.total += uint64(len(w.buf))
	w.buf = w.buf[:0]
	w.index++
	return nil
}

// Close schreibt den letzten Abschnitt und das Schlusssiegel.
func (w *pgpChunkWriter) Close() error {
	if len(w.buf) > 0 {
		if err := w.flush(); err != nil {
			return err
		}
	}
	final := w.aead.Seal(nil, pgpChunkNonce(w.iv, w.index), nil,
		pgpFinalAAD(w.header, w.total))
	if _, err := w.out.Write(final); err != nil {
		return Fail("error.file.write")
	}
	return nil
}

// --- Paketlaengen --------------------------------------------------------

// pgpPacket schreibt ein Paket, dessen Laenge schon bekannt ist.
func pgpPacket(tag int, body []byte) []byte {
	out := []byte{byte(0xc0 | tag)}
	out = append(out, pgpLength(len(body))...)
	return append(out, body...)
}

// pgpLength schreibt eine Laenge in der Form des Standards: ein Byte bis
// 191, zwei bis 8383, sonst fuenf.
func pgpLength(n int) []byte {
	switch {
	case n < 192:
		return []byte{byte(n)}
	case n < 8384:
		n -= 192
		return []byte{byte(n>>8 + 192), byte(n)}
	default:
		out := []byte{0xff, 0, 0, 0, 0}
		binary.BigEndian.PutUint32(out[1:], uint32(n))
		return out
	}
}

// pgpPartialWriter schreibt ein Paket unbekannter Laenge: Teilstuecke von
// jeweils einer Zweierpotenz, zum Schluss ein Stueck mit genannter Laenge.
// Genau so sieht der Standard es vor (RFC 9580 Abschnitt 4.2.1.4).
type pgpPartialWriter struct {
	out     io.Writer
	tag     int
	buf     []byte
	started bool
	closed  bool
}

func newPgpPartialWriter(out io.Writer, tag int) *pgpPartialWriter {
	return &pgpPartialWriter{out: out, tag: tag, buf: make([]byte, 0, pgpChunkSize)}
}

func (w *pgpPartialWriter) Write(data []byte) (int, error) {
	written := len(data)
	for len(data) > 0 {
		room := pgpChunkSize - len(w.buf)
		take := len(data)
		if take > room {
			take = room
		}
		w.buf = append(w.buf, data[:take]...)
		data = data[take:]
		if len(w.buf) == pgpChunkSize {
			if err := w.emitPartial(); err != nil {
				return 0, err
			}
		}
	}
	return written, nil
}

// emitPartial schreibt ein Teilstueck. Das erste tragt auch den Pakettyp.
func (w *pgpPartialWriter) emitPartial() error {
	var head []byte
	if !w.started {
		head = append(head, byte(0xc0|w.tag))
		w.started = true
	}
	// Ein Teilstueck nennt seine Laenge als Potenz: 224 + Exponent.
	head = append(head, byte(224|(pgpChunkBits+6)))
	if _, err := w.out.Write(head); err != nil {
		return Fail("error.file.write")
	}
	if _, err := w.out.Write(w.buf); err != nil {
		return Fail("error.file.write")
	}
	w.buf = w.buf[:0]
	return nil
}

// Close schliesst das Paket mit einem Stueck genannter Laenge ab - erst daran
// ist das Paket zu Ende.
func (w *pgpPartialWriter) Close() error {
	if w.closed {
		return nil
	}
	w.closed = true
	var head []byte
	if !w.started {
		head = append(head, byte(0xc0|w.tag))
		w.started = true
	}
	head = append(head, pgpLength(len(w.buf))...)
	if _, err := w.out.Write(head); err != nil {
		return Fail("error.file.write")
	}
	if len(w.buf) > 0 {
		if _, err := w.out.Write(w.buf); err != nil {
			return Fail("error.file.write")
		}
	}
	w.buf = w.buf[:0]
	return nil
}

// pgpBodyReader liest den Rumpf eines Pakets, auch wenn er in Teilstuecken
// steht. Streng gelesen: ein Teilstueck mit krummer Laenge, ein Strom, der
// mitten im Paket endet, und ein Stueck, dessen Laenge nicht aufgeht, sind
// Fehler.
type pgpBodyReader struct {
	source    *bufio.Reader
	remaining int
	partial   bool
	done      bool
	// broken merkt sich, dass eine Laengenangabe mehr versprochen hat,
	// als wirklich da war. Das muss gemerkt werden: wer darueber
	// hinwegliest, haelt eine gelogene Laenge fuer harmlos, und damit
	// waere ein Byte der Datei ohne Wirkung - also auch ohne Pruefung.
	broken bool
}

// pgpReadPacket liest den Kopf des naechsten Pakets und gibt den Pakettyp
// samt einem Leser fuer den Rumpf.
func pgpReadPacket(source *bufio.Reader) (int, *pgpBodyReader, error) {
	first, err := source.ReadByte()
	if err != nil {
		return 0, nil, Fail("error.file.format")
	}
	// Nur die neue Paketform. Die alte waere ein zweiter Weg mit eigenen
	// Fallstricken, und dieses Werkzeug schreibt sie nie.
	if first&0xc0 != 0xc0 {
		return 0, nil, Fail("error.file.format")
	}
	tag := int(first & 0x3f)
	body := &pgpBodyReader{source: source}
	if err := body.nextRun(); err != nil {
		return 0, nil, err
	}
	return tag, body, nil
}

// nextRun liest die naechste Laengenangabe.
func (r *pgpBodyReader) nextRun() error {
	first, err := r.source.ReadByte()
	if err != nil {
		return Fail("error.file.format")
	}
	switch {
	case first < 192:
		r.remaining, r.partial = int(first), false
	case first < 224:
		second, err := r.source.ReadByte()
		if err != nil {
			return Fail("error.file.format")
		}
		r.remaining = (int(first)-192)<<8 + int(second) + 192
		r.partial = false
	case first < 255:
		r.remaining = 1 << (first & 0x1f)
		r.partial = true
	default:
		var length [4]byte
		if _, err := io.ReadFull(r.source, length[:]); err != nil {
			return Fail("error.file.format")
		}
		r.remaining = int(binary.BigEndian.Uint32(length[:]))
		r.partial = false
	}
	return nil
}

// atCleanEnd sagt, ob der Rumpf sauber zu Ende ist: keine Laengenangabe hat
// mehr versprochen, als geliefert wurde, und es steht kein Rest aus.
func (r *pgpBodyReader) atCleanEnd() error {
	if r.broken || r.remaining > 0 {
		return Fail("error.file.truncated")
	}
	if r.partial && !r.done {
		return Fail("error.file.truncated")
	}
	return nil
}

func (r *pgpBodyReader) Read(out []byte) (int, error) {
	for r.remaining == 0 {
		if !r.partial || r.done {
			r.done = true
			return 0, io.EOF
		}
		if err := r.nextRun(); err != nil {
			return 0, err
		}
	}
	if len(out) > r.remaining {
		out = out[:r.remaining]
	}
	n, err := r.source.Read(out)
	r.remaining -= n
	if err == io.EOF && r.remaining > 0 {
		r.broken = true
		return n, Fail("error.file.truncated")
	}
	if err != nil && err != io.EOF {
		return n, Fail("error.file.read")
	}
	return n, nil
}

// --- Lesen ---------------------------------------------------------------

// pgpDecrypt liest eine OpenPGP-Nachricht.
func pgpDecrypt(identity FileIdentity, in io.Reader, out io.Writer) error {
	source := bufio.NewReaderSize(in, pgpChunkSize+pgpGCMTagLen+64)
	var sessionKey []byte
	defer func() { Wipe(sessionKey) }()
	sawRecipient := false

	for {
		tag, body, err := pgpReadPacket(source)
		if err != nil {
			return err
		}
		switch tag {
		case pgpTagPKESK:
			sawRecipient = true
			if sessionKey != nil {
				// Schon offen; dieses Fach gehoert einem anderen.
				if _, err := io.Copy(io.Discard, body); err != nil {
					return err
				}
				continue
			}
			key, err := pgpOpenSession(identity, body)
			if err != nil {
				return err
			}
			sessionKey = key
		case pgpTagSEIPD:
			if sessionKey == nil {
				if !sawRecipient {
					return Fail("error.file.norecipient")
				}
				return Fail("error.file.wrongkey")
			}
			return pgpReadData(sessionKey, body, out)
		default:
			return Fail("error.file.format")
		}
	}
}

// pgpOpenSession oeffnet ein Sitzungsschluesselpaket. Ein Fach, das nicht
// aufgeht, ist kein Fehler der Datei - es gehoert einem anderen Empfaenger;
// deshalb kommt dann nil und kein Abbruch.
func pgpOpenSession(identity FileIdentity, body io.Reader) ([]byte, error) {
	raw, err := io.ReadAll(io.LimitReader(body, 512))
	if err != nil {
		return nil, Fail("error.file.format")
	}
	if len(raw) < 4 || raw[0] != pgpPKESKVersion {
		return nil, Fail("error.file.format")
	}
	// Ein genannter Empfaenger wird uebersprungen: wir verschluesseln
	// anonym, lesen aber auch Dateien, die den Empfaenger nennen.
	offset := 1
	named := int(raw[offset])
	offset++
	offset += named
	if offset+1 > len(raw) {
		return nil, Fail("error.file.format")
	}
	if raw[offset] != pgpAlgoX25519 {
		return nil, Fail("error.file.algorithm")
	}
	offset++
	if offset+FileKeyLen+1 > len(raw) {
		return nil, Fail("error.file.format")
	}
	ephemeralPublic := raw[offset : offset+FileKeyLen]
	offset += FileKeyLen
	wrappedLen := int(raw[offset])
	offset++
	if wrappedLen != pgpWrappedLen || offset+wrappedLen != len(raw) {
		return nil, Fail("error.file.format")
	}
	wrapped := raw[offset:]

	own := identity.Public()
	if len(own) != FileKeyLen {
		return nil, Fail("error.file.identity")
	}
	// Hier rechnet das GERAET.
	shared, err := identity.Agree(ephemeralPublic)
	if err != nil {
		return nil, err
	}
	kek, err := pgpWrapKey(shared, ephemeralPublic, own)
	Wipe(shared)
	if err != nil {
		return nil, err
	}
	sessionKey, err := KeyUnwrap(kek, wrapped)
	Wipe(kek)
	if err != nil {
		return nil, nil // nicht fuer diese Kennung
	}
	if len(sessionKey) != pgpSessionLen {
		return nil, Fail("error.file.format")
	}
	return sessionKey, nil
}

// pgpReadData entschluesselt das Datenpaket und gibt die Nutzdaten heraus.
func pgpReadData(sessionKey []byte, body *pgpBodyReader, out io.Writer) error {
	var header [4]byte
	if _, err := io.ReadFull(body, header[:]); err != nil {
		return Fail("error.file.format")
	}
	if header[0] != pgpSEIPDVersion {
		return Fail("error.file.format")
	}
	if header[1] != pgpAlgoAES256 || header[2] != pgpAlgoGCM {
		return Fail("error.file.algorithm")
	}
	chunkSize := 1 << (uint(header[3]) + 6)
	salt := make([]byte, pgpSaltLen)
	if _, err := io.ReadFull(body, salt); err != nil {
		return Fail("error.file.format")
	}
	aead, iv, err := pgpAEAD(sessionKey, salt, header[:])
	if err != nil {
		return err
	}
	defer Wipe(iv)

	// Der Klartext der Abschnitte ist das Nutzdatenpaket. Es wird gelesen,
	// waehrend die Abschnitte hereinkommen - nichts wird ganz gesammelt.
	literal := newPgpLiteralWriter(out)
	source := bufio.NewReaderSize(body, chunkSize+2*pgpGCMTagLen+64)
	sealedLen := chunkSize + pgpGCMTagLen
	var index, total uint64
	for {
		// Ein Stueck mehr ansehen als ein voller Abschnitt plus
		// Schlusssiegel: nur so ist zu erkennen, ob noch ein Abschnitt
		// folgt oder ob das hier der letzte ist.
		ahead, _ := source.Peek(sealedLen + pgpGCMTagLen + 1)
		if len(ahead) < sealedLen+pgpGCMTagLen+1 {
			// Der Rest sind der letzte Abschnitt und das Schlusssiegel.
			if len(ahead) < pgpGCMTagLen {
				return Fail("error.file.truncated")
			}
			lastLen := len(ahead) - pgpGCMTagLen
			if lastLen > 0 {
				plain, err := aead.Open(nil, pgpChunkNonce(iv, index),
					ahead[:lastLen], pgpAAD(header[:]))
				if err != nil {
					return Fail("error.file.tampered")
				}
				if _, err := literal.Write(plain); err != nil {
					return err
				}
				total += uint64(len(plain))
				index++
			}
			if _, err := aead.Open(nil, pgpChunkNonce(iv, index),
				ahead[lastLen:], pgpFinalAAD(header[:], total)); err != nil {
				return Fail("error.file.tampered")
			}
			if _, err := source.Discard(len(ahead)); err != nil {
				return Fail("error.file.read")
			}
			if _, err := source.Peek(1); err == nil {
				return Fail("error.file.format")
			}
			// Erst jetzt steht fest, ob die Laengenangaben der Huelle
			// gehalten haben, was sie versprachen.
			if err := body.atCleanEnd(); err != nil {
				return err
			}
			return literal.Close()
		}
		plain, err := aead.Open(nil, pgpChunkNonce(iv, index), ahead[:sealedLen],
			pgpAAD(header[:]))
		if err != nil {
			return Fail("error.file.tampered")
		}
		if _, err := literal.Write(plain); err != nil {
			return err
		}
		total += uint64(len(plain))
		index++
		if _, err := source.Discard(sealedLen); err != nil {
			return Fail("error.file.read")
		}
	}
}

// pgpLiteralWriter liest das Nutzdatenpaket, waehrend es hereinkommt, und
// gibt nur die Daten heraus - ohne Pakethuelle, ohne Namen, ohne Datum.
type pgpLiteralWriter struct {
	out      io.Writer
	header   []byte
	inHeader bool
	lengths  pgpLengthScanner
}

func newPgpLiteralWriter(out io.Writer) *pgpLiteralWriter {
	return &pgpLiteralWriter{out: out, inHeader: true}
}

func (w *pgpLiteralWriter) Write(data []byte) (int, error) {
	written := len(data)
	for len(data) > 0 {
		if w.inHeader {
			// Gesammelt wird nur die Huelle: Pakettyp, Laengenform,
			// Kennzeichnung, Namenslaenge, Name, Datum.
			w.header = append(w.header, data[0])
			data = data[1:]
			done, err := w.tryHeader()
			if err != nil {
				return 0, err
			}
			if done {
				w.inHeader = false
			}
			continue
		}
		n, err := w.lengths.pass(data, w.out)
		if err != nil {
			return 0, err
		}
		data = data[n:]
	}
	return written, nil
}

// tryHeader sagt, ob die Huelle vollstaendig gelesen ist.
func (w *pgpLiteralWriter) tryHeader() (bool, error) {
	buf := w.header
	if len(buf) < 1 {
		return false, nil
	}
	if buf[0]&0xc0 != 0xc0 || int(buf[0]&0x3f) != pgpTagLiteral {
		return false, Fail("error.file.format")
	}
	rest := buf[1:]
	consumed, ok, err := w.lengths.start(rest)
	if err != nil || !ok {
		return false, err
	}
	rest = rest[consumed:]
	if len(rest) < 2 {
		return false, nil
	}
	if rest[0] != pgpLiteralBinary {
		return false, Fail("error.file.format")
	}
	nameLen := int(rest[1])
	if len(rest) < 2+nameLen+4 {
		return false, nil
	}
	// Die Huelle gehoert nicht zu den Daten, zaehlt aber gegen die
	// Laengenangabe des Pakets.
	w.lengths.consume(1 + 1 + nameLen + 4)
	return true, nil
}

func (w *pgpLiteralWriter) Close() error {
	if w.inHeader {
		return Fail("error.file.format")
	}
	return w.lengths.finish()
}

// pgpLengthScanner verfolgt die Laengenangaben eines Pakets, das in
// Teilstuecken ankommt, und gibt die Nutzbytes weiter.
type pgpLengthScanner struct {
	remaining int
	partial   bool
	pending   []byte // angefangene Laengenangabe
	ended     bool
}

// start liest die erste Laengenangabe. Reicht das Gelesene noch nicht, kommt
// ok == false und es wird erneut gefragt.
func (s *pgpLengthScanner) start(buf []byte) (int, bool, error) {
	return s.readLength(buf)
}

func (s *pgpLengthScanner) readLength(buf []byte) (int, bool, error) {
	if len(buf) < 1 {
		return 0, false, nil
	}
	switch first := buf[0]; {
	case first < 192:
		s.remaining, s.partial = int(first), false
		return 1, true, nil
	case first < 224:
		if len(buf) < 2 {
			return 0, false, nil
		}
		s.remaining = (int(first)-192)<<8 + int(buf[1]) + 192
		s.partial = false
		return 2, true, nil
	case first < 255:
		s.remaining = 1 << (first & 0x1f)
		s.partial = true
		return 1, true, nil
	default:
		if len(buf) < 5 {
			return 0, false, nil
		}
		s.remaining = int(binary.BigEndian.Uint32(buf[1:5]))
		s.partial = false
		return 5, true, nil
	}
}

// consume zieht Bytes ab, die zur Huelle gehoerten.
func (s *pgpLengthScanner) consume(n int) { s.remaining -= n }

// pass gibt so viele Bytes wie moeglich weiter und liest zwischendurch die
// naechste Laengenangabe. Rueckgabe: wie viele Bytes von data verbraucht sind.
func (s *pgpLengthScanner) pass(data []byte, out io.Writer) (int, error) {
	if s.ended {
		return 0, Fail("error.file.format")
	}
	if s.remaining == 0 {
		if !s.partial {
			s.ended = true
			return 0, Fail("error.file.format")
		}
		s.pending = append(s.pending, data[0])
		used, ok, err := s.readLength(s.pending)
		if err != nil {
			return 0, err
		}
		if ok {
			s.pending = s.pending[used:]
		}
		return 1, nil
	}
	take := len(data)
	if take > s.remaining {
		take = s.remaining
	}
	if _, err := out.Write(data[:take]); err != nil {
		return 0, Fail("error.file.write")
	}
	s.remaining -= take
	return take, nil
}

// finish prueft, dass das Paket zu Ende ist und nicht mitten in einer
// Laengenangabe aufhoert.
func (s *pgpLengthScanner) finish() error {
	if s.partial || s.remaining != 0 || len(s.pending) > 0 {
		return Fail("error.file.truncated")
	}
	return nil
}
