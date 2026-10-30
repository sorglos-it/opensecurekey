// Der Weg hinein aus KeePass: der Behaelter KDBX, Fassung 3.1 und 4.
//
// NACH DER BESCHREIBUNG GEBAUT, kein fremder Quelltext und auch kein
// uebernommener Aufbau. Das Format ist oeffentlich beschrieben, und das ist
// die Vorgabe dieses Projekts: was beschrieben ist, wird nach der
// Beschreibung gebaut. Die Teile, die dafuer fehlten, sind daneben
// entstanden - blake2b.go, argon2.go, salsa20.go, jeder mit den Testwerten
// seiner eigenen Beschreibung.
//
// EINE KDBX-DATEI IST DIE GEFAEHRLICHSTE EINGABE, die dieses Programm
// annimmt. Sie kommt von aussen, der Anwender haelt sie fuer harmlos, und
// hinter dem Dateikopf steht ein ganzer Baukasten: welches Verfahren, wie
// viel Speicher, wie viele Durchgaenge, wie lang jeder Block. Jede dieser
// Zahlen ist eine Angabe des Angreifers. Darum:
//
//   - Jede Laenge wird gegen das geprueft, was wirklich noch da ist, bevor
//     irgendetwas zugeschnitten wird.
//   - Die Argon2-Parameter haben Obergrenzen (siehe argon2.go). Eine Datei,
//     die vier Terabyte Speicher verlangt, wird abgewiesen - nicht
//     gerechnet.
//   - Die entpackte Groesse ist begrenzt. Ein Gigabyte XML aus zwei Kilobyte
//     gzip ist eine bekannte Falle.
//   - Die Echtheit wird GEPRUEFT, bevor der Inhalt ausgewertet wird: in der
//     Fassung 4 mit HMAC ueber Kopf und jeden Block. Eine verfaelschte Datei
//     kommt nie bis zum XML-Leser.
//   - Entschluesselt wird erst, wenn der Dateikopf stimmt, und ausgewertet
//     erst, wenn das Siegel stimmt.
//
// Und was dieses Programm NICHT kann, mit klarer Meldung statt eines
// Fehlschlags: Twofish als Verfahren, Schluesseldateien, und der alte
// Behaelter KDB (Fassung 1 und 2).
package classes

import (
	"bytes"
	"compress/gzip"
	"crypto/aes"
	"crypto/cipher"
	"crypto/hmac"
	"crypto/sha256"
	"crypto/sha512"
	"crypto/subtle"
	"encoding/base64"
	"encoding/binary"
	"encoding/hex"
	"encoding/xml"
	"io"
	"strings"
)

// Die zwei Kennzahlen am Anfang jeder KDBX-Datei.
const (
	kdbxSignature1 = 0x9AA2D903
	kdbxSignature2 = 0xB54BFB67
	// kdbxSignature2Old steht in den Behaeltern der Fassungen 1 und 2.
	// Sie werden erkannt, um sagen zu koennen, was es ist - gelesen
	// werden sie nicht.
	kdbxSignature2Old1 = 0xB54BFB65
	kdbxSignature2Old2 = 0xB54BFB66
)

// Obergrenzen. Siehe den Kopf dieser Datei.
const (
	// KdbxMaxFileLen ist die groesste Datei.
	KdbxMaxFileLen = 512 << 20
	// KdbxMaxPlainLen ist die groesste Menge entpacktes XML.
	KdbxMaxPlainLen = 256 << 20
	// KdbxMaxField ist die groesste Laenge eines Kopffeldes.
	KdbxMaxField = 1 << 20
	// KdbxMaxBlock ist die groesste Laenge eines Datenblocks.
	KdbxMaxBlock = 64 << 20
	// KdbxMaxRounds ist die hoechste Zahl von AES-Durchgaengen. Die alte
	// Fassung schluesselt mit wiederholtem AES, und die Zahl steht in der
	// Datei: ohne Grenze waere das ein Stillstand auf Zuruf. Hundert
	// Millionen sind etwa eine Minute - mehr stellt kein Mensch ein.
	KdbxMaxRounds = 100_000_000
	// KdbxMaxProtected ist die hoechste Zahl verdeckter Werte.
	KdbxMaxProtected = 4 * CxfMaxItems
)

// Die Kennungen der Verfahren, als Text in Hexschreibweise. Sie stehen so
// in der Beschreibung; der Vergleich laeuft ueber die Bytes.
const (
	kdbxCipherAES      = "31c1f2e6bf714350be5805216afc5aff"
	kdbxCipherChaCha20 = "d6038a2b8b6f4cb5a524339a31dbb59a"
	kdbxCipherTwofish  = "ad68f29f576f4bb9a36ad47af965346c"
	kdbxKdfAES         = "c9d9f39a628a4460bf740d08c18a4fea"
	kdbxKdfArgon2d     = "ef636ddf8c29444b91f7a9a403e30a0c"
	kdbxKdfArgon2id    = "9e298b1956db4773b23dfc3ec6f0a1e6"
)

// Die Nummern der Felder im aeusseren Dateikopf.
const (
	kdbxHdrEnd           = 0
	kdbxHdrCipherID      = 2
	kdbxHdrCompression   = 3
	kdbxHdrMasterSeed    = 4
	kdbxHdrTransformSeed = 5
	kdbxHdrRounds        = 6
	kdbxHdrIV            = 7
	kdbxHdrInnerKey      = 8
	kdbxHdrStreamStart   = 9
	kdbxHdrInnerID       = 10
	kdbxHdrKdfParams     = 11
)

// Die Nummern der Felder im inneren Dateikopf, nur Fassung 4.
const (
	kdbxInnerEnd    = 0
	kdbxInnerID     = 1
	kdbxInnerKey    = 2
	kdbxInnerBinary = 3
)

// Die Nummern der Verfahren, mit denen die Kennwoerter im XML verdeckt
// werden.
const (
	kdbxStreamNone     = 0
	kdbxStreamArcFour  = 1
	kdbxStreamSalsa20  = 2
	kdbxStreamChaCha20 = 3
)

// salsa20KdbxNonce ist der Einmalwert, den KeePass fuer Salsa20 benutzt. Er
// ist FEST - in der Datei steht er nicht. Das ist so beschrieben.
var salsa20KdbxNonce = []byte{0xE8, 0x30, 0x09, 0x4B, 0x97, 0x20, 0x5D, 0x2A}

// --- Lesen --------------------------------------------------------------------

// KdbxImport liest eine KeePass-Datei und macht daraus einen Plan.
//
// Das Kennwort wird ueberschrieben, bevor diese Funktion zurueckkommt.
func KdbxImport(in io.Reader, password []byte, profile uint8) (*CsvPlan, error) {
	defer Wipe(password)
	if in == nil {
		return nil, Fail("error.kdbx.read")
	}
	data, err := io.ReadAll(io.LimitReader(in, KdbxMaxFileLen+1))
	if err != nil {
		return nil, Fail("error.kdbx.read")
	}
	if len(data) > KdbxMaxFileLen {
		return nil, Fail("error.kdbx.toobig")
	}
	head, plain, err := kdbxOpen(data, password)
	if err != nil {
		return nil, err
	}
	defer Wipe(plain)
	return kdbxReadXML(head, plain, profile)
}

// kdbxOpen entschluesselt die Datei und gibt den Dateikopf und das XML
// heraus. Der Kopf wird mitgegeben, weil darin das Verfahren steht, mit dem
// die Kennwoerter IM XML noch ein zweites Mal verdeckt sind.
func kdbxOpen(data, password []byte) (*kdbxHead, []byte, error) {
	reader := &kdbxReader{data: data}
	if reader.left() < 12 {
		return nil, nil, Fail("error.kdbx.format")
	}
	if reader.uint32() != kdbxSignature1 {
		return nil, nil, Fail("error.kdbx.notkdbx")
	}
	switch second := reader.uint32(); second {
	case kdbxSignature2:
	case kdbxSignature2Old1, kdbxSignature2Old2:
		// Der alte Behaelter. Es wird gesagt, was es ist - "unlesbar"
		// waere hier eine Auskunft, mit der niemand weiterkommt.
		return nil, nil, Fail("error.kdbx.oldcontainer")
	default:
		return nil, nil, Fail("error.kdbx.notkdbx")
	}
	version := reader.uint32()
	major := version >> 16
	if major != 3 && major != 4 {
		return nil, nil, Fail("error.kdbx.version", kdbxVersionText(version))
	}

	head, err := kdbxHeader(reader, major)
	if err != nil {
		return nil, nil, err
	}
	headerBytes := data[:reader.at]
	head.major = major
	head.minor = version & 0xffff
	sum := sha256.Sum256(headerBytes)
	head.headerHash = sum[:]

	var plain []byte
	if major == 4 {
		plain, err = kdbx4Body(reader, head, headerBytes, password)
	} else {
		plain, err = kdbx3Body(reader, head, password)
	}
	if err != nil {
		return nil, nil, err
	}
	return head, plain, nil
}

func kdbxVersionText(version uint32) string {
	return hex.EncodeToString([]byte{byte(version >> 24), byte(version >> 16),
		byte(version >> 8), byte(version)})
}

// kdbxHead sind die gelesenen Kopffelder.
type kdbxHead struct {
	// major ist die Fassung des Behaelters, 3 oder 4. Sie entscheidet,
	// WIE die Echtheit geprueft wird - siehe kdbxReadXML.
	major uint32
	// minor ist die Unterfassung. Sie entscheidet mit: die Pruefsumme des
	// Dateikopfes in Meta/HeaderHash gibt es erst ab 3.1. Format300.kdbx
	// von KeePassXC - eine echte Datei in 3.0 - hat sie nicht, und sie zu
	// verlangen hiesse, alte Datenbanken abzuweisen.
	minor uint32
	// headerHash ist SHA-256 ueber den Dateikopf. In der Fassung 4 steht
	// er in der Datei und wird vor dem Entschluesseln geprueft; in der
	// Fassung 3 steht er IM XML, also erst nach dem Entschluesseln.
	headerHash    []byte
	cipher        []byte
	compressed    bool
	masterSeed    []byte
	transformSeed []byte
	rounds        uint64
	iv            []byte
	innerKey      []byte
	streamStart   []byte
	innerID       uint32
	kdfParams     map[string]kdbxValue
}

// kdbxHeader liest den aeusseren Dateikopf. Die Laengenangabe ist in der
// Fassung 3 zwei Byte breit und in der Fassung 4 vier - ein Unterschied,
// der sich beim Verwechseln als "unlesbare Datei" zeigt und nicht als
// Fehler.
func kdbxHeader(reader *kdbxReader, major uint32) (*kdbxHead, error) {
	head := &kdbxHead{innerID: kdbxStreamNone}
	for {
		if reader.left() < 1 {
			return nil, Fail("error.kdbx.format")
		}
		id := reader.byteValue()
		var size uint32
		if major == 4 {
			if reader.left() < 4 {
				return nil, Fail("error.kdbx.format")
			}
			size = reader.uint32()
		} else {
			if reader.left() < 2 {
				return nil, Fail("error.kdbx.format")
			}
			size = uint32(reader.uint16())
		}
		if size > KdbxMaxField {
			return nil, Fail("error.kdbx.field")
		}
		body, ok := reader.take(int(size))
		if !ok {
			return nil, Fail("error.kdbx.format")
		}
		if id == kdbxHdrEnd {
			break
		}
		switch id {
		case kdbxHdrCipherID:
			if len(body) != 16 {
				return nil, Fail("error.kdbx.cipher")
			}
			head.cipher = body
		case kdbxHdrCompression:
			if len(body) != 4 {
				return nil, Fail("error.kdbx.format")
			}
			switch binary.LittleEndian.Uint32(body) {
			case 0:
				head.compressed = false
			case 1:
				head.compressed = true
			default:
				return nil, Fail("error.kdbx.compression")
			}
		case kdbxHdrMasterSeed:
			if len(body) != 32 {
				return nil, Fail("error.kdbx.format")
			}
			head.masterSeed = body
		case kdbxHdrTransformSeed:
			head.transformSeed = body
		case kdbxHdrRounds:
			if len(body) != 8 {
				return nil, Fail("error.kdbx.format")
			}
			head.rounds = binary.LittleEndian.Uint64(body)
		case kdbxHdrIV:
			head.iv = body
		case kdbxHdrInnerKey:
			head.innerKey = body
		case kdbxHdrStreamStart:
			head.streamStart = body
		case kdbxHdrInnerID:
			if len(body) != 4 {
				return nil, Fail("error.kdbx.format")
			}
			head.innerID = binary.LittleEndian.Uint32(body)
		case kdbxHdrKdfParams:
			params, err := kdbxVariantDict(body)
			if err != nil {
				return nil, err
			}
			head.kdfParams = params
		}
	}
	if len(head.cipher) != 16 || len(head.masterSeed) != 32 {
		return nil, Fail("error.kdbx.format")
	}
	return head, nil
}

// --- Schluessel ---------------------------------------------------------------

// kdbxCompositeKey ist der Schluessel aus dem Kennwort. KeePass hasht das
// Kennwort einmal und den Zusammenschluss noch einmal - auch wenn es nur ein
// Teil ist. Eine Schluesseldatei waere ein zweiter Teil; sie ist hier nicht
// gebaut, und eine Datei, die eine braucht, laesst sich dann nicht
// oeffnen - mit der Meldung error.kdbx.keyfile und nicht mit "falsches
// Kennwort".
func kdbxCompositeKey(password []byte) []byte {
	inner := sha256.Sum256(password)
	outer := sha256.Sum256(inner[:])
	Wipe(inner[:])
	return outer[:]
}

// kdbxTransform laesst die Schluesselableitung laufen.
func kdbxTransform(head *kdbxHead, composite []byte, major uint32) ([]byte, error) {
	if major == 3 {
		if len(head.transformSeed) != 32 {
			return nil, Fail("error.kdbx.format")
		}
		return kdbxAesKdf(composite, head.transformSeed, head.rounds)
	}
	if head.kdfParams == nil {
		return nil, Fail("error.kdbx.nokdf")
	}
	uuid, ok := head.kdfParams["$UUID"]
	if !ok || len(uuid.bytes) != 16 {
		return nil, Fail("error.kdbx.nokdf")
	}
	switch hex.EncodeToString(uuid.bytes) {
	case kdbxKdfAES:
		seed := head.kdfParams["S"].bytes
		rounds := head.kdfParams["R"].number
		if len(seed) != 32 {
			return nil, Fail("error.kdbx.format")
		}
		return kdbxAesKdf(composite, seed, rounds)
	case kdbxKdfArgon2d:
		return kdbxArgon2(head.kdfParams, composite, Argon2D)
	case kdbxKdfArgon2id:
		return kdbxArgon2(head.kdfParams, composite, Argon2ID)
	}
	return nil, Fail("error.kdbx.kdf", hex.EncodeToString(uuid.bytes))
}

// kdbxAesKdf ist die alte Ableitung: AES ohne Verkettung, immer wieder auf
// denselben 32 Byte, und am Ende ein SHA-256 darueber.
func kdbxAesKdf(composite, seed []byte, rounds uint64) ([]byte, error) {
	if rounds == 0 || rounds > KdbxMaxRounds {
		return nil, Fail("error.kdbx.rounds")
	}
	block, err := aes.NewCipher(seed)
	if err != nil {
		return nil, Fail("error.kdbx.format")
	}
	buffer := make([]byte, 32)
	copy(buffer, composite)
	defer Wipe(buffer)
	for i := uint64(0); i < rounds; i++ {
		// Zwei Bloecke, jeder fuer sich - das ist ECB, und es ist hier
		// keine Schwaeche: verschluesselt wird nichts, es wird nur
		// Rechenzeit verbraucht.
		block.Encrypt(buffer[:16], buffer[:16])
		block.Encrypt(buffer[16:], buffer[16:])
	}
	sum := sha256.Sum256(buffer)
	return sum[:], nil
}

// kdbxArgon2 liest die Parameter aus der Datei und laesst Argon2 rechnen.
func kdbxArgon2(params map[string]kdbxValue, composite []byte, kind Argon2Type) ([]byte, error) {
	salt := params["S"].bytes
	lanes := params["P"].number
	memoryBytes := params["M"].number
	passes := params["I"].number
	version := params["V"].number
	if version != 0 && version != Argon2Version {
		// Die alte Fassung 0x10 rechnet anders. Sie wird nicht mit der
		// neuen gerechnet - das ergaebe stillschweigend einen anderen
		// Schluessel und die Meldung "falsches Kennwort".
		return nil, Fail("error.kdbx.argonversion")
	}
	// KeePass nennt den Speicher in BYTE, Argon2 rechnet in KiB. Eine
	// Stelle, an der sich ein Faktor 1024 verstecken laesst, und der
	// Fehler zeigt sich nur als falsches Kennwort.
	memory := memoryBytes / 1024
	if memory == 0 || memory > Argon2MaxMemory ||
		lanes == 0 || lanes > Argon2MaxLanes ||
		passes == 0 || passes > Argon2MaxTime {
		return nil, Fail("error.kdbx.argonparams")
	}
	return Argon2(kind, composite, salt, params["K"].bytes, params["A"].bytes,
		uint32(passes), uint32(memory), uint32(lanes), 32)
}

// --- Fassung 4 ----------------------------------------------------------------

// kdbx4Body prueft das Siegel und entschluesselt.
func kdbx4Body(reader *kdbxReader, head *kdbxHead, headerBytes, password []byte) ([]byte, error) {
	storedHash, ok := reader.take(32)
	if !ok {
		return nil, Fail("error.kdbx.format")
	}
	storedMac, ok := reader.take(32)
	if !ok {
		return nil, Fail("error.kdbx.format")
	}
	// Erst die Pruefsumme des Kopfes. Sie braucht kein Kennwort und sagt,
	// ob die Datei ueberhaupt heil ist - eine kaputte Datei soll nicht als
	// "falsches Kennwort" durchgehen.
	headerHash := sha256.Sum256(headerBytes)
	if subtle.ConstantTimeCompare(headerHash[:], storedHash) != 1 {
		return nil, Fail("error.kdbx.headerhash")
	}

	composite := kdbxCompositeKey(password)
	defer Wipe(composite)
	transformed, err := kdbxTransform(head, composite, 4)
	if err != nil {
		return nil, err
	}
	defer Wipe(transformed)

	masterKey := sha256.Sum256(append(append([]byte{}, head.masterSeed...), transformed...))
	defer Wipe(masterKey[:])
	macBase := sha512.Sum512(append(append(append([]byte{}, head.masterSeed...),
		transformed...), 0x01))
	defer Wipe(macBase[:])

	// Das Siegel des Kopfes benutzt den Blockschluessel zur Nummer
	// 0xFFFFFFFFFFFFFFFF - das ist in der Beschreibung so, und es ist der
	// Grund, warum der Kopf nicht als Block Null zaehlt.
	headerMac := kdbxBlockMac(macBase[:], ^uint64(0))
	defer Wipe(headerMac)
	mac := hmac.New(sha256.New, headerMac)
	mac.Write(headerBytes)
	if subtle.ConstantTimeCompare(mac.Sum(nil), storedMac) != 1 {
		// Der Kopf ist heil, aber das Siegel passt nicht. Das heisst
		// fast immer: falsches Kennwort. Es kann auch heissen: jemand hat
		// die Datei veraendert. Beides wird gesagt, denn das Programm
		// kann es nicht unterscheiden.
		return nil, Fail("error.kdbx.wrongkey")
	}

	body, err := kdbx4Blocks(reader, macBase[:])
	if err != nil {
		return nil, err
	}
	defer Wipe(body)

	plain, err := kdbxDecrypt(head, masterKey[:], body)
	if err != nil {
		return nil, err
	}
	if head.compressed {
		unpacked, err := kdbxUnzip(plain)
		Wipe(plain)
		if err != nil {
			return nil, err
		}
		plain = unpacked
	}
	// Der innere Dateikopf steht VOR dem XML und nennt das Verfahren, mit
	// dem die Kennwoerter verdeckt sind.
	return kdbx4Inner(head, plain)
}

// kdbx4Blocks setzt die Blöcke zusammen. Jeder traegt sein eigenes Siegel,
// und ein Block ohne gueltiges Siegel beendet das Lesen mit einem Fehler -
// nicht mit einer kuerzeren Datei.
func kdbx4Blocks(reader *kdbxReader, macBase []byte) ([]byte, error) {
	var out []byte
	for index := uint64(0); ; index++ {
		storedMac, ok := reader.take(32)
		if !ok {
			return nil, Fail("error.kdbx.format")
		}
		if reader.left() < 4 {
			return nil, Fail("error.kdbx.format")
		}
		size := reader.uint32()
		if size > KdbxMaxBlock {
			return nil, Fail("error.kdbx.block")
		}
		data, ok := reader.take(int(size))
		if !ok {
			return nil, Fail("error.kdbx.format")
		}
		key := kdbxBlockMac(macBase, index)
		mac := hmac.New(sha256.New, key)
		var number [12]byte
		binary.LittleEndian.PutUint64(number[0:], index)
		binary.LittleEndian.PutUint32(number[8:], size)
		mac.Write(number[:8])
		mac.Write(number[8:])
		mac.Write(data)
		valid := subtle.ConstantTimeCompare(mac.Sum(nil), storedMac) == 1
		Wipe(key)
		if !valid {
			return nil, Fail("error.kdbx.blockmac")
		}
		if size == 0 {
			// Der leere Block ist das Ende - und sein Siegel ist gerade
			// geprueft worden. Ein abgeschnittener Strom hat ihn nicht
			// und ist damit ein Fehler, keine kuerzere Datei.
			break
		}
		if len(out)+len(data) > KdbxMaxPlainLen {
			return nil, Fail("error.kdbx.toobig")
		}
		out = append(out, data...)
	}
	return out, nil
}

// kdbxBlockMac rechnet den Schluessel eines Blocks: SHA-512 ueber die
// Blocknummer und den Grundschluessel.
func kdbxBlockMac(macBase []byte, index uint64) []byte {
	var number [8]byte
	binary.LittleEndian.PutUint64(number[:], index)
	sum := sha512.Sum512(append(number[:], macBase...))
	return sum[:]
}

// kdbx4Inner liest den inneren Dateikopf und gibt das XML heraus.
func kdbx4Inner(head *kdbxHead, plain []byte) ([]byte, error) {
	reader := &kdbxReader{data: plain}
	for {
		if reader.left() < 5 {
			return nil, Fail("error.kdbx.format")
		}
		id := reader.byteValue()
		size := reader.uint32()
		if size > KdbxMaxField {
			return nil, Fail("error.kdbx.field")
		}
		body, ok := reader.take(int(size))
		if !ok {
			return nil, Fail("error.kdbx.format")
		}
		switch id {
		case kdbxInnerEnd:
			rest, ok := reader.take(reader.left())
			if !ok || len(rest) == 0 {
				return nil, Fail("error.kdbx.noxml")
			}
			return append([]byte(nil), rest...), nil
		case kdbxInnerID:
			if len(body) != 4 {
				return nil, Fail("error.kdbx.format")
			}
			head.innerID = binary.LittleEndian.Uint32(body)
		case kdbxInnerKey:
			head.innerKey = append([]byte(nil), body...)
		case kdbxInnerBinary:
			// Anhaenge. Dieser Tresor haelt keine; sie werden
			// uebergangen, und dass sie da waren, sagt der Bericht
			// (siehe kdbxReadXML).
		}
	}
}

// --- Fassung 3 ----------------------------------------------------------------

// kdbx3Body entschluesselt die aeltere Fassung.
//
// Sie hat KEIN Siegel ueber dem Ganzen: ob das Kennwort stimmt, zeigt sich
// daran, dass die ersten 32 Byte des Klartexts den Wert aus dem Dateikopf
// treffen. Danach traegt jeder Block eine Pruefsumme - eine Pruefsumme, kein
// Siegel: sie findet einen Uebertragungsfehler und keine Faelschung. Das ist
// eine Schwaeche DES FORMATS, nicht dieses Lesers, und sie steht in
// docs/cxf.md.
func kdbx3Body(reader *kdbxReader, head *kdbxHead, password []byte) ([]byte, error) {
	if len(head.streamStart) != 32 {
		return nil, Fail("error.kdbx.format")
	}
	composite := kdbxCompositeKey(password)
	defer Wipe(composite)
	transformed, err := kdbxTransform(head, composite, 3)
	if err != nil {
		return nil, err
	}
	defer Wipe(transformed)
	masterKey := sha256.Sum256(append(append([]byte{}, head.masterSeed...), transformed...))
	defer Wipe(masterKey[:])

	body, ok := reader.take(reader.left())
	if !ok {
		return nil, Fail("error.kdbx.format")
	}
	plain, err := kdbxDecrypt(head, masterKey[:], body)
	if err != nil {
		return nil, err
	}
	defer Wipe(plain)
	if len(plain) < 32 ||
		subtle.ConstantTimeCompare(plain[:32], head.streamStart) != 1 {
		return nil, Fail("error.kdbx.wrongkey")
	}
	joined, err := kdbx3Blocks(&kdbxReader{data: plain[32:]})
	if err != nil {
		return nil, err
	}
	if head.compressed {
		unpacked, err := kdbxUnzip(joined)
		Wipe(joined)
		if err != nil {
			return nil, err
		}
		joined = unpacked
	}
	return joined, nil
}

// kdbx3Blocks setzt die Blöcke der alten Fassung zusammen.
func kdbx3Blocks(reader *kdbxReader) ([]byte, error) {
	var out []byte
	for expected := uint32(0); ; expected++ {
		if reader.left() < 40 {
			return nil, Fail("error.kdbx.format")
		}
		index := reader.uint32()
		if index != expected {
			// Die Reihenfolge steht in der Datei. Sie zu befolgen statt
			// sie zu pruefen hiesse, eine umsortierte Datei
			// anzunehmen.
			return nil, Fail("error.kdbx.blockorder")
		}
		storedHash, _ := reader.take(32)
		size := reader.uint32()
		if size > KdbxMaxBlock {
			return nil, Fail("error.kdbx.block")
		}
		if size == 0 {
			// Der Abschlussblock traegt eine Pruefsumme aus lauter
			// Nullen. Steht dort etwas anderes, ist die Datei nicht
			// die, die sie zu sein behauptet.
			for _, b := range storedHash {
				if b != 0 {
					return nil, Fail("error.kdbx.blockhash")
				}
			}
			break
		}
		data, ok := reader.take(int(size))
		if !ok {
			return nil, Fail("error.kdbx.format")
		}
		sum := sha256.Sum256(data)
		if subtle.ConstantTimeCompare(sum[:], storedHash) != 1 {
			return nil, Fail("error.kdbx.blockhash")
		}
		if len(out)+len(data) > KdbxMaxPlainLen {
			return nil, Fail("error.kdbx.toobig")
		}
		out = append(out, data...)
	}
	return out, nil
}

// --- Verfahren ----------------------------------------------------------------

// kdbxDecrypt entschluesselt den Rumpf.
func kdbxDecrypt(head *kdbxHead, key, body []byte) ([]byte, error) {
	switch hex.EncodeToString(head.cipher) {
	case kdbxCipherAES:
		if len(head.iv) != aes.BlockSize {
			return nil, Fail("error.kdbx.format")
		}
		if len(body) == 0 || len(body)%aes.BlockSize != 0 {
			return nil, Fail("error.kdbx.blocklen")
		}
		block, err := aes.NewCipher(key)
		if err != nil {
			return nil, Fail("error.kdbx.format")
		}
		out := make([]byte, len(body))
		cipher.NewCBCDecrypter(block, head.iv).CryptBlocks(out, body)
		return kdbxUnpad(out)
	case kdbxCipherChaCha20:
		if len(head.iv) != 12 {
			return nil, Fail("error.kdbx.format")
		}
		var fixed [ChaChaKeyLen]byte
		copy(fixed[:], key)
		defer Wipe(fixed[:])
		out := make([]byte, len(body))
		chachaXOR(&fixed, head.iv, 0, body, out)
		return out, nil
	case kdbxCipherTwofish:
		// Twofish ist nicht gebaut. Gesagt wird, WAS fehlt - "unlesbar"
		// liesse den Anwender raten, ob sein Kennwort falsch ist.
		return nil, Fail("error.kdbx.twofish")
	}
	return nil, Fail("error.kdbx.cipher")
}

// kdbxUnpad nimmt die Auffuellung nach PKCS#7 ab.
//
// Die Laenge der Auffuellung steht im letzten Byte, also in Daten, die
// gerade entschluesselt wurden - bei falschem Schluessel ist sie Unsinn.
// Darum wird sie geprueft, und zwar jede Stelle: eine Auffuellung, die nur
// am letzten Byte geprueft wird, laesst sich von aussen durchprobieren.
func kdbxUnpad(data []byte) ([]byte, error) {
	if len(data) == 0 || len(data)%aes.BlockSize != 0 {
		return nil, Fail("error.kdbx.blocklen")
	}
	count := int(data[len(data)-1])
	if count == 0 || count > aes.BlockSize || count > len(data) {
		return nil, Fail("error.kdbx.wrongkey")
	}
	bad := 0
	for _, b := range data[len(data)-count:] {
		bad |= int(b) ^ count
	}
	if bad != 0 {
		return nil, Fail("error.kdbx.wrongkey")
	}
	return data[:len(data)-count], nil
}

// kdbxUnzip entpackt. Die Obergrenze gilt BEIM Entpacken: eine kleine Datei,
// die zu einem Gigabyte aufgeht, ist eine bekannte Falle, und ein
// io.LimitReader ist dagegen die ganze Abwehr.
func kdbxUnzip(data []byte) ([]byte, error) {
	zipped, err := gzip.NewReader(bytes.NewReader(data))
	if err != nil {
		return nil, Fail("error.kdbx.gzip")
	}
	defer zipped.Close()
	out, err := io.ReadAll(io.LimitReader(zipped, KdbxMaxPlainLen+1))
	if err != nil {
		return nil, Fail("error.kdbx.gzip")
	}
	if len(out) > KdbxMaxPlainLen {
		return nil, Fail("error.kdbx.toobig")
	}
	return out, nil
}

// kdbxInnerStream baut den Strom, mit dem die Kennwoerter im XML verdeckt
// sind.
func kdbxInnerStream(head *kdbxHead) (func([]byte), error) {
	switch head.innerID {
	case kdbxStreamNone:
		// Ohne Verdecken stehen die Kennwoerter im Klartext im XML. Das
		// ist eine Einstellung, die KeePass zulaesst.
		return func([]byte) {}, nil
	case kdbxStreamSalsa20:
		if len(head.innerKey) == 0 {
			return nil, Fail("error.kdbx.innerkey")
		}
		// Der Schluessel wird gehasht: in der Datei steht ein Wert
		// beliebiger Laenge, Salsa20 will 32 Byte.
		sum := sha256.Sum256(head.innerKey)
		stream, err := NewSalsa20(sum[:], salsa20KdbxNonce)
		Wipe(sum[:])
		if err != nil {
			return nil, err
		}
		return stream.XOR, nil
	case kdbxStreamChaCha20:
		if len(head.innerKey) == 0 {
			return nil, Fail("error.kdbx.innerkey")
		}
		// SHA-512 ueber den Schluessel: die ersten 32 Byte sind der
		// Schluessel, die naechsten 12 der Einmalwert. So beschrieben.
		sum := sha512.Sum512(head.innerKey)
		stream, err := NewChaCha20Stream(sum[:32], sum[32:44])
		Wipe(sum[:])
		if err != nil {
			return nil, err
		}
		return stream.XOR, nil
	case kdbxStreamArcFour:
		// RC4 ist gebrochen und wird nicht gebaut. KeePass schreibt es
		// seit Jahren nicht mehr.
		return nil, Fail("error.kdbx.rc4")
	}
	return nil, Fail("error.kdbx.innerstream")
}

// --- Das XML ------------------------------------------------------------------

// kdbxEntryField ist ein Feld eines Eintrags.
type kdbxEntryField struct {
	key   string
	value string
	// hidden ist der Platz im Strom, wenn der Wert verdeckt war, sonst -1.
	hidden int
}

// kdbxEntry ist ein gelesener Eintrag.
type kdbxEntry struct {
	fields []kdbxEntryField
	group  string
	// history heisst: eine alte Fassung eines Eintrags. KeePass legt sie
	// im Eintrag selbst ab. Sie werden NICHT uebernommen - sonst lagen
	// alle alten Kennwoerter mit im Tresor, und niemand haette danach
	// gefragt. Ihre verdeckten Werte zaehlen aber im Strom mit, und wer
	// das uebersieht, entschluesselt alles Folgende falsch.
	history bool
	// chain sind die Gruppenkennungen auf dem Weg zu diesem Eintrag. Daran
	// wird erkannt, ob er im Papierkorb von KeePass liegt.
	chain []string
}

// KdbxMaxXMLDepth ist die tiefste Schachtelung im XML. Gruppen in Gruppen
// sind erlaubt, aber nicht ohne Ende: eine Datei mit zehntausend Ebenen
// soll hier anstossen und nicht im Stapelspeicher.
const KdbxMaxXMLDepth = 64

// kdbxXML ist das gelesene XML.
type kdbxXML struct {
	entries []kdbxEntry
	// hidden sind die verdeckten Werte in der Reihenfolge, in der sie in
	// der Datei stehen. Diese Reihenfolge IST der Schluesselstrom - wer
	// einen ueberspringt, entschluesselt alle folgenden falsch.
	hidden [][]byte
	// recycleUUID ist die Gruppe, die KeePass als Papierkorb fuehrt.
	recycleUUID string
	// headerHash ist der Wert aus Meta/HeaderHash. Nur die Fassung 3
	// schreibt ihn, und dort ist er die EINZIGE Pruefung, die den
	// Dateikopf abdeckt.
	headerHash []byte
	// binaries zaehlt die Anhaenge. Dieser Tresor haelt keine; dass es
	// welche gab, gehoert in den Bericht.
	binaries int
}

// kdbxParseXML liest das XML Zeichen fuer Zeichen.
//
// Als Strom und nicht als Baum, und das ist kein Geschmack: die verdeckten
// Werte muessen in GENAU der Reihenfolge gesammelt werden, in der sie in der
// Datei stehen, einschliesslich der alten Fassungen unter History. Ein
// Leser, der zuerst einen Baum baut und dann hineinsieht, verliert diese
// Reihenfolge, und dann ist jedes Kennwort hinter dem ersten Fehler Unsinn.
func kdbxParseXML(plain []byte) (*kdbxXML, error) {
	decoder := xml.NewDecoder(bytes.NewReader(plain))
	out := &kdbxXML{}

	var path []string
	var hiddenHere []bool
	var groupNames []string
	var groupUUIDs []string
	var text strings.Builder
	historyDepth := 0
	entryDepth := 0
	var entry *kdbxEntry
	var field *kdbxEntryField

	// at sagt, ob der Weg von hinten her so aussieht.
	at := func(names ...string) bool {
		if len(path) < len(names) {
			return false
		}
		for i, name := range names {
			if path[len(path)-len(names)+i] != name {
				return false
			}
		}
		return true
	}

	for {
		token, err := decoder.Token()
		if err == io.EOF {
			break
		}
		if err != nil {
			return nil, Fail("error.kdbx.xml")
		}
		switch element := token.(type) {
		case xml.StartElement:
			if len(path) >= KdbxMaxXMLDepth {
				return nil, Fail("error.kdbx.deep")
			}
			path = append(path, element.Name.Local)
			protected := false
			for _, attr := range element.Attr {
				if attr.Name.Local == "Protected" &&
					strings.EqualFold(attr.Value, "true") {
					protected = true
				}
			}
			hiddenHere = append(hiddenHere, protected)
			text.Reset()
			switch element.Name.Local {
			case "Group":
				groupNames = append(groupNames, "")
				groupUUIDs = append(groupUUIDs, "")
			case "History":
				historyDepth++
			case "Entry":
				if entryDepth == 0 {
					entry = &kdbxEntry{
						group:   kdbxGroupPath(groupNames),
						history: historyDepth > 0,
					}
					entry.chain = append([]string(nil), groupUUIDs...)
				}
				entryDepth++
			case "String":
				if entry != nil {
					field = &kdbxEntryField{hidden: -1}
				}
			}

		case xml.CharData:
			// Nur so viel Text sammeln, wie ein Feld haben darf. Eine
			// Datei mit einem Feld von einem Gigabyte ist kein Tresor.
			if text.Len() < CsvMaxFieldLen {
				text.Write(element)
			}

		case xml.EndElement:
			if len(path) == 0 {
				return nil, Fail("error.kdbx.xml")
			}
			name := path[len(path)-1]
			protected := hiddenHere[len(hiddenHere)-1]
			value := text.String()
			slot := -1
			if protected {
				if len(out.hidden) >= KdbxMaxProtected {
					return nil, Fail("error.kdbx.manyitems")
				}
				raw, err := kdbxBase64(value)
				if err != nil {
					return nil, err
				}
				slot = len(out.hidden)
				out.hidden = append(out.hidden, raw)
				value = ""
			}

			switch {
			case name == "Name" && at("Group", "Name") && len(groupNames) > 0:
				groupNames[len(groupNames)-1] = value
			case name == "UUID" && at("Group", "UUID") && len(groupUUIDs) > 0:
				groupUUIDs[len(groupUUIDs)-1] = strings.TrimSpace(value)
			case name == "RecycleBinUUID" && at("Meta", "RecycleBinUUID"):
				out.recycleUUID = strings.TrimSpace(value)
			case name == "HeaderHash" && at("Meta", "HeaderHash"):
				raw, err := kdbxBase64(value)
				if err != nil {
					return nil, err
				}
				out.headerHash = raw
			case name == "Key" && field != nil:
				field.key = value
			case name == "Value" && field != nil:
				field.value = value
				field.hidden = slot
			case name == "String" && entry != nil && field != nil:
				entry.fields = append(entry.fields, *field)
				field = nil
			case name == "Binary" && entry != nil && entryDepth > 0:
				out.binaries++
			case name == "Entry":
				entryDepth--
				if entryDepth == 0 && entry != nil {
					out.entries = append(out.entries, *entry)
					entry = nil
				}
			case name == "History":
				historyDepth--
			case name == "Group":
				if len(groupNames) > 0 {
					groupNames = groupNames[:len(groupNames)-1]
					groupUUIDs = groupUUIDs[:len(groupUUIDs)-1]
				}
			}
			path = path[:len(path)-1]
			hiddenHere = hiddenHere[:len(hiddenHere)-1]
			text.Reset()
		}
	}
	if entryDepth != 0 || len(path) != 0 {
		return nil, Fail("error.kdbx.xml")
	}
	return out, nil
}

// kdbxGroupPath baut den Weg durch die Gruppen. Die erste Gruppe ist der
// Name der Datenbank selbst und steht nicht im Weg.
func kdbxGroupPath(names []string) string {
	var parts []string
	for i, name := range names {
		if i == 0 {
			continue
		}
		if trimmed := strings.TrimSpace(name); trimmed != "" {
			parts = append(parts, trimmed)
		}
	}
	return strings.Join(parts, "/")
}

// Die Feldnamen, die KeePass fest vorgibt.
const (
	kdbxFieldTitle    = "Title"
	kdbxFieldUser     = "UserName"
	kdbxFieldPassword = "Password"
	kdbxFieldURL      = "URL"
	kdbxFieldNotes    = "Notes"
)

// kdbxReadXML liest das XML und macht daraus einen Plan.
func kdbxReadXML(head *kdbxHead, plain []byte, profile uint8) (*CsvPlan, error) {
	tree, err := kdbxParseXML(plain)
	if err != nil {
		return nil, err
	}
	// DIE ECHTHEIT DER FASSUNG 3, und sie wird erst hier geprueft.
	//
	// Die Fassung 4 siegelt den Dateikopf mit HMAC, vor dem
	// Entschluesseln. Die Fassung 3 hat das nicht: ihr Kopf ist durch
	// NICHTS gedeckt, bis auf diesen einen Wert, und der steht im
	// verschluesselten XML - also erst hier zu haben.
	//
	// WARUM DAS WICHTIG IST: im Kopf der Fassung 3 steht der Schluessel,
	// mit dem die Kennwoerter im XML verdeckt sind. Wer ihn verbiegt,
	// bekommt eine Datei, die sich oeffnen laesst, deren Eintraege alle
	// dastehen - und deren Kennwoerter still falsch sind. Gefunden von
	// TestKdbxVerfaelschtWirdAbgewiesen: 0x01 auf Stelle 145 wurde
	// vorher ANGENOMMEN.
	//
	// Fehlt der Wert, wird die Datei abgewiesen. Das ist die strengere
	// Wahl: eine Fassung-3-Datei ohne ihn hat keine Pruefung ueber dem
	// Kopf, und ein Tresor, dessen Kennwoerter still falsch sein koennen,
	// ist schlimmer als eine Meldung. KeePass schreibt den Wert seit der
	// Fassung 3.
	warnungen := []CxfLine(nil)
	if head.major == 3 {
		switch {
		case len(tree.headerHash) > 0:
			if subtle.ConstantTimeCompare(tree.headerHash, head.headerHash) != 1 {
				return nil, Fail("error.kdbx.headerhash")
			}
		case head.minor >= 1:
			// Ab 3.1 gehoert der Wert dazu. Fehlt er, stimmt an der
			// Datei etwas nicht.
			return nil, Fail("error.kdbx.noheaderhash")
		default:
			// 3.0 kennt ihn noch nicht. Die Datei wird gelesen - aber
			// ihr Dateikopf ist durch nichts gedeckt, und das wird
			// GESAGT. Es stillschweigend hinzunehmen hiesse, einen
			// Tresor mit still falschen Kennwoertern fuer geprueft
			// auszugeben.
			warnungen = append(warnungen, CxfLine{
				ReasonKey: "kdbx.warn.noheaderhash"})
		}
	}
	reveal, err := kdbxInnerStream(head)
	if err != nil {
		return nil, err
	}
	// Der Strom laeuft EINMAL ueber alle verdeckten Werte, in ihrer
	// Reihenfolge. Darum werden sie hintereinandergelegt und in einem Zug
	// aufgedeckt - jedes Stueck einzeln zu behandeln waere dasselbe,
	// solange keines uebersprungen wird, und das ist genau der Fehler, der
	// sich hier nicht machen soll.
	var joined []byte
	bounds := make([]int, len(tree.hidden)+1)
	for i, blob := range tree.hidden {
		bounds[i] = len(joined)
		joined = append(joined, blob...)
	}
	bounds[len(tree.hidden)] = len(joined)
	reveal(joined)
	defer Wipe(joined)

	plan := &CsvPlan{Warnings: warnungen}
	for _, item := range tree.entries {
		if item.history {
			// Eine alte Fassung. Ihre verdeckten Werte haben im Strom
			// mitgezaehlt - uebernommen wird sie nicht.
			continue
		}
		if tree.recycleUUID != "" && kdbxInChain(item.chain, tree.recycleUUID) {
			plan.Left = append(plan.Left, CxfLine{
				Name:      kdbxFieldValue(item, joined, bounds, kdbxFieldTitle),
				ReasonKey: "kdbx.left.recycled",
			})
			continue
		}
		kdbxEntryToPlan(item, joined, bounds, profile, plan)
	}
	if tree.binaries > 0 {
		// Anhaenge. Sie kommen nicht mit, und das steht im Bericht -
		// genau einmal, mit der Zahl.
		plan.Left = append(plan.Left, CxfLine{ReasonKey: "kdbx.left.attachments"})
	}
	if len(plan.Candidates) == 0 && len(plan.Left) == 0 {
		return nil, Fail("error.kdbx.nothing")
	}
	return plan, nil
}

// kdbxInChain sagt, ob eine Gruppenkennung im Weg eines Eintrags vorkommt.
func kdbxInChain(chain []string, uuid string) bool {
	for _, entry := range chain {
		if entry == uuid {
			return true
		}
	}
	return false
}

// kdbxFieldValue holt den Wert eines Feldes - aufgedeckt, wenn er verdeckt
// war.
func kdbxFieldValue(entry kdbxEntry, joined []byte, bounds []int, key string) string {
	for _, field := range entry.fields {
		if !strings.EqualFold(field.key, key) {
			continue
		}
		if field.hidden >= 0 && field.hidden+1 < len(bounds) {
			return string(joined[bounds[field.hidden]:bounds[field.hidden+1]])
		}
		return field.value
	}
	return ""
}

// kdbxEntryToPlan macht aus einem KeePass-Eintrag bis zu zwei Eintraege des
// Tresors - und nennt, was von ihm nicht mitkommt.
//
// Der Tresor haelt je Eintrag EIN Geheimnis. Ein KeePass-Eintrag kann
// Kennwort, Notiz, Einmalkennwort und eigene Felder zugleich tragen. Was
// davon nicht in einen Eintrag passt, wird darum nicht zusammengeruehrt -
// ein Kennwort, dem die Notiz angehaengt ist, waere beim naechsten Anmelden
// falsch.
func kdbxEntryToPlan(entry kdbxEntry, joined []byte, bounds []int, profile uint8,
	plan *CsvPlan) {
	get := func(key string) string { return kdbxFieldValue(entry, joined, bounds, key) }
	title := strings.TrimSpace(get(kdbxFieldTitle))
	site := strings.TrimSpace(get(kdbxFieldURL))
	user := strings.TrimSpace(get(kdbxFieldUser))
	password := get(kdbxFieldPassword)
	notes := get(kdbxFieldNotes)

	name := title
	if name == "" {
		name = csvHostOf(site)
	}
	if name == "" {
		name = user
	}
	if entry.group != "" && name != "" {
		// Der Gruppenweg gehoert in die Adresse, nicht in den Namen: der
		// Name ist kurz begrenzt, und zwei Eintraege gleichen Namens in
		// verschiedenen Gruppen sollen sich noch unterscheiden lassen.
		if site == "" {
			site = entry.group
		}
	}

	added := false
	if strings.TrimSpace(password) != "" && name != "" {
		meta := Item{
			Profile:  profile,
			Type:     ItemLogin,
			Name:     cxfShorten(name, MaxNameLen),
			Site:     cxfShorten(csvSiteOf(site, user), MaxSiteLen),
			Mirrored: true,
		}
		line := CxfLine{Name: meta.Name, Type: meta.Type}
		if candidate, line, ok := cxfFinish(meta, []byte(password), line); ok {
			plan.Candidates = append(plan.Candidates, candidate)
			plan.Taken = append(plan.Taken, line)
			added = true
		} else {
			plan.Left = append(plan.Left, line)
		}
	}

	// Einmalkennwort. KeePass legt es als eigenes Feld ab, und die Namen
	// sind je Erweiterung verschieden - darum dieselbe Namensliste wie bei
	// CSV, an einer Stelle gefuehrt.
	for _, field := range entry.fields {
		if csvHeaders[csvNormalize(field.key)] != csvTotp {
			continue
		}
		value := strings.TrimSpace(kdbxFieldValue(entry, joined, bounds, field.key))
		if value == "" || name == "" {
			continue
		}
		meta := Item{
			Profile:  profile,
			Type:     ItemTotp,
			Name:     cxfShorten(name+" (TOTP)", MaxNameLen),
			Site:     cxfShorten(site, MaxSiteLen),
			Mirrored: true,
		}
		line := CxfLine{Name: meta.Name, Type: meta.Type}
		if candidate, line, ok := cxfFinish(meta, []byte(value), line); ok {
			plan.Candidates = append(plan.Candidates, candidate)
			plan.Taken = append(plan.Taken, line)
			added = true
		} else {
			plan.Left = append(plan.Left, line)
		}
		break
	}

	// Die Notiz. Nur als EIGENER Eintrag, und nur wenn der Eintrag kein
	// Kennwort hatte: sonst wuerde jeder Eintrag mit Notiz den Tresor
	// doppelt fuellen. Hatte er beides, steht die Notiz im Bericht - sie
	// geht verloren, und das soll man wissen.
	if strings.TrimSpace(notes) != "" && name != "" {
		if !added {
			meta := Item{
				Profile:  profile,
				Type:     ItemNote,
				Name:     cxfShorten(name, MaxNameLen),
				Site:     cxfShorten(site, MaxSiteLen),
				Mirrored: true,
			}
			line := CxfLine{Name: meta.Name, Type: meta.Type}
			if candidate, line, ok := cxfFinish(meta, []byte(notes), line); ok {
				plan.Candidates = append(plan.Candidates, candidate)
				plan.Taken = append(plan.Taken, line)
				added = true
			} else {
				plan.Left = append(plan.Left, line)
			}
		} else {
			plan.Left = append(plan.Left, CxfLine{
				Name:      cxfShorten(name, MaxNameLen),
				ReasonKey: "kdbx.left.notes",
			})
		}
	}

	// Eigene Felder: alles, was keinen der festen Namen traegt und auch
	// kein Einmalkennwort ist.
	for _, field := range entry.fields {
		switch {
		case strings.EqualFold(field.key, kdbxFieldTitle),
			strings.EqualFold(field.key, kdbxFieldUser),
			strings.EqualFold(field.key, kdbxFieldPassword),
			strings.EqualFold(field.key, kdbxFieldURL),
			strings.EqualFold(field.key, kdbxFieldNotes):
			continue
		}
		if csvHeaders[csvNormalize(field.key)] == csvTotp {
			continue
		}
		plan.UnusedColumns = append(plan.UnusedColumns,
			cxfShorten(field.key, MaxNameLen))
	}

	if !added && strings.TrimSpace(password) == "" && strings.TrimSpace(notes) == "" {
		plan.Left = append(plan.Left, CxfLine{
			Name:      cxfShorten(name, MaxNameLen),
			ReasonKey: "csv.left.nopassword",
		})
	}
}

// --- Lesehilfe ----------------------------------------------------------------

// kdbxReader liest aus einem Puffer und laeuft nie darueber hinaus. Jede
// Entnahme sagt, ob sie geklappt hat; eine Entnahme, die nicht passt, gibt
// nichts heraus.
type kdbxReader struct {
	data []byte
	at   int
}

func (r *kdbxReader) left() int { return len(r.data) - r.at }

func (r *kdbxReader) take(n int) ([]byte, bool) {
	if n < 0 || n > r.left() {
		return nil, false
	}
	out := r.data[r.at : r.at+n]
	r.at += n
	return out, true
}

func (r *kdbxReader) byteValue() byte {
	value := r.data[r.at]
	r.at++
	return value
}

func (r *kdbxReader) uint16() uint16 {
	value := binary.LittleEndian.Uint16(r.data[r.at:])
	r.at += 2
	return value
}

func (r *kdbxReader) uint32() uint32 {
	value := binary.LittleEndian.Uint32(r.data[r.at:])
	r.at += 4
	return value
}

// --- Das Woerterbuch des Dateikopfes ------------------------------------------

// kdbxValue ist ein Wert aus dem Woerterbuch der Fassung 4. Zahl oder Bytes
// oder Text - je nach Art, und die Art steht davor.
type kdbxValue struct {
	number uint64
	bytes  []byte
	text   string
}

// Die Arten des Woerterbuchs, als Zahlen der Beschreibung.
const (
	kdbxVarUint32 = 0x04
	kdbxVarUint64 = 0x05
	kdbxVarBool   = 0x08
	kdbxVarInt32  = 0x0C
	kdbxVarInt64  = 0x0D
	kdbxVarString = 0x18
	kdbxVarBytes  = 0x42
)

// kdbxVariantDict liest das Woerterbuch mit den Parametern der
// Schluesselableitung.
func kdbxVariantDict(data []byte) (map[string]kdbxValue, error) {
	reader := &kdbxReader{data: data}
	if reader.left() < 2 {
		return nil, Fail("error.kdbx.kdfparams")
	}
	version := reader.uint16()
	// Nur die obere Haelfte der Fassungsnummer muss passen; das ist in der
	// Beschreibung so geregelt, damit neue Arten hinzukommen koennen.
	if version>>8 != 1 {
		return nil, Fail("error.kdbx.kdfparams")
	}
	out := map[string]kdbxValue{}
	for {
		if reader.left() < 1 {
			return nil, Fail("error.kdbx.kdfparams")
		}
		kind := reader.byteValue()
		if kind == 0 {
			return out, nil
		}
		if reader.left() < 4 {
			return nil, Fail("error.kdbx.kdfparams")
		}
		nameLen := reader.uint32()
		if nameLen > KdbxMaxField {
			return nil, Fail("error.kdbx.kdfparams")
		}
		nameBytes, ok := reader.take(int(nameLen))
		if !ok {
			return nil, Fail("error.kdbx.kdfparams")
		}
		if reader.left() < 4 {
			return nil, Fail("error.kdbx.kdfparams")
		}
		valueLen := reader.uint32()
		if valueLen > KdbxMaxField {
			return nil, Fail("error.kdbx.kdfparams")
		}
		body, ok := reader.take(int(valueLen))
		if !ok {
			return nil, Fail("error.kdbx.kdfparams")
		}
		value := kdbxValue{bytes: append([]byte(nil), body...)}
		switch kind {
		case kdbxVarUint32, kdbxVarInt32:
			if len(body) != 4 {
				return nil, Fail("error.kdbx.kdfparams")
			}
			value.number = uint64(binary.LittleEndian.Uint32(body))
		case kdbxVarUint64, kdbxVarInt64:
			if len(body) != 8 {
				return nil, Fail("error.kdbx.kdfparams")
			}
			value.number = binary.LittleEndian.Uint64(body)
		case kdbxVarBool:
			if len(body) != 1 {
				return nil, Fail("error.kdbx.kdfparams")
			}
			if body[0] != 0 {
				value.number = 1
			}
		case kdbxVarString:
			value.text = string(body)
		case kdbxVarBytes:
		default:
			return nil, Fail("error.kdbx.kdfparams")
		}
		out[string(nameBytes)] = value
	}
}

// kdbxBase64 liest einen verdeckten Wert. Eine kaputte Base64-Zeichenkette
// ist ein Fehler und wird nicht als leerer Wert genommen: sonst stuende im
// Tresor ein Eintrag ohne Kennwort, und niemand wuesste, warum.
func kdbxBase64(text string) ([]byte, error) {
	trimmed := strings.TrimSpace(text)
	if trimmed == "" {
		return nil, nil
	}
	raw, err := base64.StdEncoding.DecodeString(trimmed)
	if err != nil {
		return nil, Fail("error.kdbx.base64")
	}
	return raw, nil
}

// --- ChaCha20 als laufender Strom ---------------------------------------------

// ChaCha20Stream ist ein Schluesselstrom, der ueber mehrere Aufrufe hinweg
// weiterlaeuft - das Gegenstueck zu Salsa20Stream in salsa20.go.
//
// chachaXOR in chacha.go rechnet einen Puffer am Stueck und fängt dabei
// immer bei einer genannten Blocknummer an. Fuer den Dateiwerkzeug-Weg ist
// das richtig; hier wird ein Strom gebraucht, der sich merkt, wo er steht.
type ChaCha20Stream struct {
	key     [ChaChaKeyLen]byte
	nonce   []byte
	counter uint32
	block   [chachaBlockLen]byte
	used    int
}

// NewChaCha20Stream baut den Strom.
func NewChaCha20Stream(key, nonce []byte) (*ChaCha20Stream, error) {
	if len(key) != ChaChaKeyLen {
		return nil, Fail("error.chacha.key")
	}
	if len(nonce) != ChaChaNonceLen {
		return nil, Fail("error.chacha.nonce")
	}
	s := &ChaCha20Stream{nonce: append([]byte(nil), nonce...), used: chachaBlockLen}
	copy(s.key[:], key)
	return s, nil
}

// XOR verknuepft data mit dem Strom - an der Stelle, an der er steht.
func (s *ChaCha20Stream) XOR(data []byte) {
	for i := range data {
		if s.used == chachaBlockLen {
			var zeros [chachaBlockLen]byte
			chachaXOR(&s.key, s.nonce, s.counter, zeros[:], s.block[:])
			s.counter++
			s.used = 0
		}
		data[i] ^= s.block[s.used]
		s.used++
	}
}
