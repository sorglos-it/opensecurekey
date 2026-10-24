// Ein CBOR-Leser fuer genau eine Aufgabe: die Nutzlast einer Lizenz.
//
// Es ist nicht der ganze Standard und soll es nicht sein. Gelesen werden
// vorzeichenlose Zahlen, Texte, Felder und Abbildungen, und von den
// Laengenformen nur "unter 24", "ein Byte" und "zwei Byte". Alles andere
// wird abgewiesen, statt geraten - eine Nutzlast, die mehr enthaelt, kommt
// nicht von unserem Unterschreiber.
//
// Jede Laenge wird gegen das Pufferende geprueft, bevor gelesen wird. Diese
// Bytes sind unterschrieben, aber der Leser muss auch mit unsinnigen
// zurechtkommen: er laeuft, bevor die Unterschrift stimmt, wenn jemand ihn
// falsch aufruft.
package oskguard

import "encoding/binary"

// cborDepth ist die Tiefe, die diese Nutzlast braucht: Abbildung, darin ein
// Feld. Mehr ist ein Fehler.
const cborDepth = 4

// cborMap liest eine Abbildung mit Textschluesseln aus den ganzen Bytes.
// Bleibt etwas uebrig, ist das ein Fehler - angehaengte Bytes werden nicht
// stillschweigend uebergangen.
func cborMap(data []byte) (map[string]any, error) {
	value, rest, err := cborValue(data, 0)
	if err != nil {
		return nil, err
	}
	if len(rest) != 0 {
		return nil, fail("error.license.format")
	}
	out, ok := value.(map[string]any)
	if !ok {
		return nil, fail("error.license.format")
	}
	return out, nil
}

// cborValue liest einen Wert und gibt den Rest zurueck.
func cborValue(data []byte, depth int) (any, []byte, error) {
	if depth > cborDepth || len(data) == 0 {
		return nil, nil, fail("error.license.format")
	}
	major := data[0] >> 5
	count, rest, err := cborHead(data)
	if err != nil {
		return nil, nil, err
	}
	switch major {
	case 0: // vorzeichenlose Zahl
		return count, rest, nil
	case 3: // Text
		if count > uint64(len(rest)) {
			return nil, nil, fail("error.license.format")
		}
		return string(rest[:count]), rest[count:], nil
	case 4: // Feld
		out := make([]any, 0, cborRoom(count, len(rest)))
		for i := uint64(0); i < count; i++ {
			var entry any
			entry, rest, err = cborValue(rest, depth+1)
			if err != nil {
				return nil, nil, err
			}
			out = append(out, entry)
		}
		return out, rest, nil
	case 5: // Abbildung
		out := make(map[string]any, cborRoom(count, len(rest)))
		for i := uint64(0); i < count; i++ {
			var key, entry any
			key, rest, err = cborValue(rest, depth+1)
			if err != nil {
				return nil, nil, err
			}
			name, isText := key.(string)
			if !isText {
				return nil, nil, fail("error.license.format")
			}
			if _, twice := out[name]; twice {
				// Zweimal derselbe Schluessel: dann waere nicht
				// entschieden, welcher gilt.
				return nil, nil, fail("error.license.format")
			}
			entry, rest, err = cborValue(rest, depth+1)
			if err != nil {
				return nil, nil, err
			}
			out[name] = entry
		}
		return out, rest, nil
	}
	return nil, nil, fail("error.license.format")
}

// cborHead liest Hauptsorte und Anzahl. Nur die drei kurzen Laengenformen -
// eine Lizenz braucht keine groesseren, und was wir nicht brauchen, lesen
// wir nicht.
func cborHead(data []byte) (uint64, []byte, error) {
	low := data[0] & 0x1f
	switch {
	case low < 24:
		return uint64(low), data[1:], nil
	case low == 24:
		if len(data) < 2 {
			return 0, nil, fail("error.license.format")
		}
		if data[1] < 24 {
			// Nicht die kuerzeste Form - das ist kein gueltiges CBOR.
			return 0, nil, fail("error.license.format")
		}
		return uint64(data[1]), data[2:], nil
	case low == 25:
		if len(data) < 3 {
			return 0, nil, fail("error.license.format")
		}
		value := uint64(binary.BigEndian.Uint16(data[1:3]))
		if value < 256 {
			return 0, nil, fail("error.license.format")
		}
		return value, data[3:], nil
	case low == 26:
		// Vier Byte. Die braucht eine Lizenz wirklich: ein Zeitstempel in
		// Sekunden passt nicht in zwei Byte. Das war beim ersten Versuch
		// vergessen, und die Pruefung gegen die Attrappe hat es gefunden.
		if len(data) < 5 {
			return 0, nil, fail("error.license.format")
		}
		value := uint64(binary.BigEndian.Uint32(data[1:5]))
		if value <= 0xffff {
			return 0, nil, fail("error.license.format")
		}
		return value, data[5:], nil
	case low == 27:
		if len(data) < 9 {
			return 0, nil, fail("error.license.format")
		}
		value := binary.BigEndian.Uint64(data[1:9])
		if value <= 0xffffffff {
			return 0, nil, fail("error.license.format")
		}
		return value, data[9:], nil
	}
	return 0, nil, fail("error.license.format")
}

// cborRoom gibt eine Vorab-Groesse, die nie groesser ist als das, was im
// Puffer ueberhaupt noch stehen kann. Sonst koennte eine erfundene Anzahl
// beliebig viel Speicher anfordern.
func cborRoom(count uint64, left int) int {
	if count > uint64(left) {
		return left
	}
	return int(count)
}
