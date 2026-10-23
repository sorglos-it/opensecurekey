// CBOR, selbst geschrieben und absichtlich streng: nur die Typen, die das
// Protokoll in shared/protocol.md braucht, nur bestimmte Laengen, feste
// Obergrenzen. Alles andere ist ein Formatfehler - ein Geraet, das mehr
// schickt, ist entweder kaputt oder nicht das Geraet.
package classes

import (
	"math"
	"sort"
	"unicode/utf8"
)

// Obergrenzen. Sie gelten beim Lesen wie beim Schreiben.
const (
	CborMaxBytes = 16384 // eine ganze Nachricht
	CborMaxDepth = 8     // Verschachtelung
	CborMaxItems = 512   // Eintraege je Abbildung oder Liste
	CborMaxText  = 4096  // ein Text oder ein Byteblock
)

// CborMap ist eine Abbildung mit Textschluesseln - das Protokoll kennt nur solche.
type CborMap map[string]any

// CborEncode schreibt einen Wert. Erlaubt sind CborMap, []any, string,
// []byte, uint64, int64, int, bool und nil.
func CborEncode(v any) ([]byte, error) {
	out, err := encodeValue(nil, v, 0)
	if err != nil {
		return nil, err
	}
	if len(out) > CborMaxBytes {
		return nil, Fail("error.cbor.toobig")
	}
	return out, nil
}

// CborDecode liest einen Wert und laesst nichts uebrig: Reste gelten als Fehler.
func CborDecode(data []byte) (any, error) {
	if len(data) == 0 || len(data) > CborMaxBytes {
		return nil, Fail("error.cbor.format")
	}
	v, rest, err := decodeValue(data, 0)
	if err != nil {
		return nil, err
	}
	if len(rest) != 0 {
		return nil, Fail("error.cbor.format")
	}
	return v, nil
}

func encodeValue(out []byte, v any, depth int) ([]byte, error) {
	if depth > CborMaxDepth {
		return nil, Fail("error.cbor.depth")
	}
	switch t := v.(type) {
	case nil:
		return append(out, 0xf6), nil
	case bool:
		if t {
			return append(out, 0xf5), nil
		}
		return append(out, 0xf4), nil
	case uint64:
		return encodeHead(out, 0, t), nil
	case uint32:
		return encodeHead(out, 0, uint64(t)), nil
	case int:
		return encodeInt(out, int64(t)), nil
	case int64:
		return encodeInt(out, t), nil
	case string:
		if len(t) > CborMaxText {
			return nil, Fail("error.cbor.toobig")
		}
		out = encodeHead(out, 3, uint64(len(t)))
		return append(out, t...), nil
	case []byte:
		if len(t) > CborMaxText {
			return nil, Fail("error.cbor.toobig")
		}
		out = encodeHead(out, 2, uint64(len(t)))
		return append(out, t...), nil
	case []any:
		if len(t) > CborMaxItems {
			return nil, Fail("error.cbor.toobig")
		}
		out = encodeHead(out, 4, uint64(len(t)))
		for _, e := range t {
			var err error
			if out, err = encodeValue(out, e, depth+1); err != nil {
				return nil, err
			}
		}
		return out, nil
	case CborMap:
		if len(t) > CborMaxItems {
			return nil, Fail("error.cbor.toobig")
		}
		keys := make([]string, 0, len(t))
		for k := range t {
			keys = append(keys, k)
		}
		// Feste Reihenfolge: gleiche Eingabe gibt immer gleiche Bytes.
		sort.Strings(keys)
		out = encodeHead(out, 5, uint64(len(t)))
		for _, k := range keys {
			var err error
			if out, err = encodeValue(out, k, depth+1); err != nil {
				return nil, err
			}
			if out, err = encodeValue(out, t[k], depth+1); err != nil {
				return nil, err
			}
		}
		return out, nil
	default:
		return nil, Fail("error.cbor.type")
	}
}

func encodeInt(out []byte, i int64) []byte {
	if i < 0 {
		return encodeHead(out, 1, uint64(-(i + 1)))
	}
	return encodeHead(out, 0, uint64(i))
}

// encodeHead schreibt Typ und Laenge immer so kurz wie moeglich.
func encodeHead(out []byte, major byte, n uint64) []byte {
	m := major << 5
	switch {
	case n < 24:
		return append(out, m|byte(n))
	case n <= math.MaxUint8:
		return append(out, m|24, byte(n))
	case n <= math.MaxUint16:
		return append(out, m|25, byte(n>>8), byte(n))
	case n <= math.MaxUint32:
		return append(out, m|26, byte(n>>24), byte(n>>16), byte(n>>8), byte(n))
	default:
		return append(out, m|27,
			byte(n>>56), byte(n>>48), byte(n>>40), byte(n>>32),
			byte(n>>24), byte(n>>16), byte(n>>8), byte(n))
	}
}

func decodeValue(data []byte, depth int) (any, []byte, error) {
	if depth > CborMaxDepth {
		return nil, nil, Fail("error.cbor.depth")
	}
	if len(data) == 0 {
		return nil, nil, Fail("error.cbor.format")
	}
	major := data[0] >> 5
	minor := data[0] & 0x1f
	if major == 7 {
		switch minor {
		case 20:
			return false, data[1:], nil
		case 21:
			return true, data[1:], nil
		case 22:
			return nil, data[1:], nil
		}
		return nil, nil, Fail("error.cbor.type")
	}
	n, rest, err := decodeHead(data)
	if err != nil {
		return nil, nil, err
	}
	switch major {
	case 0:
		return n, rest, nil
	case 1:
		if n > math.MaxInt64 {
			return nil, nil, Fail("error.cbor.format")
		}
		return -1 - int64(n), rest, nil
	case 2, 3:
		if n > CborMaxText || uint64(len(rest)) < n {
			return nil, nil, Fail("error.cbor.format")
		}
		body := rest[:n]
		rest = rest[n:]
		if major == 2 {
			b := make([]byte, n)
			copy(b, body)
			return b, rest, nil
		}
		if !utf8.Valid(body) {
			return nil, nil, Fail("error.cbor.format")
		}
		return string(body), rest, nil
	case 4:
		if n > CborMaxItems {
			return nil, nil, Fail("error.cbor.toobig")
		}
		list := make([]any, 0, n)
		for i := uint64(0); i < n; i++ {
			var v any
			if v, rest, err = decodeValue(rest, depth+1); err != nil {
				return nil, nil, err
			}
			list = append(list, v)
		}
		return list, rest, nil
	case 5:
		if n > CborMaxItems {
			return nil, nil, Fail("error.cbor.toobig")
		}
		m := make(CborMap, n)
		for i := uint64(0); i < n; i++ {
			var kv, vv any
			if kv, rest, err = decodeValue(rest, depth+1); err != nil {
				return nil, nil, err
			}
			key, ok := kv.(string)
			if !ok {
				return nil, nil, Fail("error.cbor.type")
			}
			if _, double := m[key]; double {
				return nil, nil, Fail("error.cbor.format")
			}
			if vv, rest, err = decodeValue(rest, depth+1); err != nil {
				return nil, nil, err
			}
			m[key] = vv
		}
		return m, rest, nil
	}
	return nil, nil, Fail("error.cbor.type")
}

// decodeHead liest Typ und Laenge und verlangt die kuerzeste Schreibweise -
// zwei Schreibweisen fuer dieselbe Zahl sind ein Einfallstor.
func decodeHead(data []byte) (uint64, []byte, error) {
	minor := data[0] & 0x1f
	switch {
	case minor < 24:
		return uint64(minor), data[1:], nil
	case minor == 24:
		if len(data) < 2 || data[1] < 24 {
			return 0, nil, Fail("error.cbor.format")
		}
		return uint64(data[1]), data[2:], nil
	case minor == 25:
		if len(data) < 3 {
			return 0, nil, Fail("error.cbor.format")
		}
		n := uint64(data[1])<<8 | uint64(data[2])
		if n <= math.MaxUint8 {
			return 0, nil, Fail("error.cbor.format")
		}
		return n, data[3:], nil
	case minor == 26:
		if len(data) < 5 {
			return 0, nil, Fail("error.cbor.format")
		}
		n := uint64(data[1])<<24 | uint64(data[2])<<16 | uint64(data[3])<<8 | uint64(data[4])
		if n <= math.MaxUint16 {
			return 0, nil, Fail("error.cbor.format")
		}
		return n, data[5:], nil
	case minor == 27:
		if len(data) < 9 {
			return 0, nil, Fail("error.cbor.format")
		}
		var n uint64
		for i := 1; i <= 8; i++ {
			n = n<<8 | uint64(data[i])
		}
		if n <= math.MaxUint32 {
			return 0, nil, Fail("error.cbor.format")
		}
		return n, data[9:], nil
	}
	return 0, nil, Fail("error.cbor.format")
}

// CborUint holt eine vorzeichenlose Zahl aus einer Abbildung.
func CborUint(m CborMap, key string) (uint64, bool) {
	v, ok := m[key].(uint64)
	return v, ok
}

// CborText holt einen Text aus einer Abbildung.
func CborText(m CborMap, key string) (string, bool) {
	v, ok := m[key].(string)
	return v, ok
}

// CborBytes holt einen Byteblock aus einer Abbildung.
func CborBytes(m CborMap, key string) ([]byte, bool) {
	v, ok := m[key].([]byte)
	return v, ok
}

// CborSub holt eine verschachtelte Abbildung.
func CborSub(m CborMap, key string) (CborMap, bool) {
	v, ok := m[key].(CborMap)
	return v, ok
}

// CborList holt eine Liste.
func CborList(m CborMap, key string) ([]any, bool) {
	v, ok := m[key].([]any)
	return v, ok
}
