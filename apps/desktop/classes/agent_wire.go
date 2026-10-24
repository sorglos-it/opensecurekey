// Das Datenformat von SSH, selbst geschrieben.
//
// RFC 4251 Abschnitt 5 nennt genau fuenf Bauteile, und nur die werden hier
// gebraucht: byte, uint32, string (vier Bytes Laenge, dann die Bytes) und
// mpint (eine ganze Zahl als string, Zweierkomplement, oberstes Bit frei).
// Daraus besteht jede Nachricht des Agenten und jeder Schluesselabdruck.
//
// Eigener Code: das Format ist Teil der Spezifikation, die Umsetzung nicht.
package classes

import (
	"encoding/binary"
	"math/big"
)

// wireMax ist die Obergrenze fuer eine einzelne Zeichenkette im Format.
// Ohne Grenze koennte eine Gegenseite mit vier Bytes Laengenangabe vier
// Gigabyte Speicher anfordern.
const wireMax = 1 << 20

// wireWriter baut eine Nachricht auf. Er kann nicht scheitern - was nicht
// passt, kommt gar nicht erst hinein.
type wireWriter struct {
	buf []byte
}

// Byte haengt ein einzelnes Byte an.
func (w *wireWriter) Byte(b byte) {
	w.buf = append(w.buf, b)
}

// Uint32 haengt eine Zahl in der Reihenfolge des Netzes an.
func (w *wireWriter) Uint32(v uint32) {
	w.buf = binary.BigEndian.AppendUint32(w.buf, v)
}

// String haengt Bytes mit vorangestellter Laenge an.
func (w *wireWriter) String(data []byte) {
	w.Uint32(uint32(len(data)))
	w.buf = append(w.buf, data...)
}

// Name ist String fuer einen Namen eines Verfahrens oder einer Kurve.
// Nicht "Text": sichtbarer Text kommt im Projekt immer aus einer
// Sprachdatei, und .Text( ist dafuer belegt.
func (w *wireWriter) Name(text string) {
	w.Uint32(uint32(len(text)))
	w.buf = append(w.buf, text...)
}

// Mpint haengt eine nicht negative ganze Zahl an. Ist das oberste Bit
// gesetzt, kommt ein Nullbyte davor - sonst waere die Zahl negativ.
func (w *wireWriter) Mpint(value *big.Int) {
	if value.Sign() == 0 {
		w.Uint32(0)
		return
	}
	data := value.Bytes()
	if data[0]&0x80 != 0 {
		w.Uint32(uint32(len(data)) + 1)
		w.buf = append(w.buf, 0)
		w.buf = append(w.buf, data...)
		return
	}
	w.String(data)
}

// Nested haengt eine fertige Nachricht als string an - so steckt in einer
// Unterschrift die Unterschrift selbst.
func (w *wireWriter) Nested(inner *wireWriter) {
	w.String(inner.buf)
}

// Bytes gibt das Gebaute zurueck.
func (w *wireWriter) Bytes() []byte { return w.buf }

// Len nennt die bisherige Laenge.
func (w *wireWriter) Len() int { return len(w.buf) }

// wireReader liest eine Nachricht. Jeder Lesevorgang prueft die Laenge -
// eine Nachricht von aussen ist nie vertrauenswuerdig.
type wireReader struct {
	data []byte
	pos  int
}

func newWireReader(data []byte) *wireReader {
	return &wireReader{data: data}
}

// Byte liest ein Byte.
func (r *wireReader) Byte() (byte, error) {
	if r.pos+1 > len(r.data) {
		return 0, Fail("error.agent.format")
	}
	b := r.data[r.pos]
	r.pos++
	return b, nil
}

// Uint32 liest eine Zahl.
func (r *wireReader) Uint32() (uint32, error) {
	if r.pos+4 > len(r.data) {
		return 0, Fail("error.agent.format")
	}
	v := binary.BigEndian.Uint32(r.data[r.pos:])
	r.pos += 4
	return v, nil
}

// String liest Bytes mit Laengenangabe. Der zurueckgegebene Ausschnitt zeigt
// in die gelesene Nachricht - wer ihn behalten will, kopiert ihn.
func (r *wireReader) String() ([]byte, error) {
	size, err := r.Uint32()
	if err != nil {
		return nil, err
	}
	if size > wireMax || r.pos+int(size) > len(r.data) {
		return nil, Fail("error.agent.format")
	}
	out := r.data[r.pos : r.pos+int(size)]
	r.pos += int(size)
	return out, nil
}

// Name liest einen Namen. Namen im Format sind US-ASCII; alles andere waere
// ein Angriff auf die Anzeige.
func (r *wireReader) Name() (string, error) {
	data, err := r.String()
	if err != nil {
		return "", err
	}
	for _, b := range data {
		if b < 0x20 || b > 0x7e {
			return "", Fail("error.agent.format")
		}
	}
	return string(data), nil
}

// Mpint liest eine ganze Zahl. Negative Zahlen kommen in Schluesseln nicht
// vor und werden abgewiesen, statt still zu etwas anderem zu werden.
func (r *wireReader) Mpint() (*big.Int, error) {
	data, err := r.String()
	if err != nil {
		return nil, err
	}
	if len(data) > 0 && data[0]&0x80 != 0 {
		return nil, Fail("error.agent.format")
	}
	return new(big.Int).SetBytes(data), nil
}

// Skip ueberspringt Bytes.
func (r *wireReader) Skip(count int) error {
	if count < 0 || r.pos+count > len(r.data) {
		return Fail("error.agent.format")
	}
	r.pos += count
	return nil
}

// Rest gibt zurueck, was noch nicht gelesen ist.
func (r *wireReader) Rest() []byte { return r.data[r.pos:] }

// Done sagt, ob die Nachricht restlos gelesen ist. Ein Rest ist ein Fehler:
// er hiesse, dass die Gegenseite etwas anderes gemeint hat als wir lesen.
func (r *wireReader) Done() bool { return r.pos == len(r.data) }
