// Argon2 nach RFC 9106, die Spielarten d, i und id.
//
// WOZU: der KeePass-Behaelter in der Fassung 4 rechnet damit den Schluessel
// aus dem Kennwort. Ohne Argon2 ist keine KDBX-4-Datei lesbar, und das ist
// die Fassung, die KeePass heute schreibt. Nach der Beschreibung gebaut,
// kein fremdes Paket - siehe blake2b.go, auf dem es aufsitzt.
//
// WAS DIE PRUEFUNG WERT IST: RFC 9106 nennt in Abschnitt 5 fuer alle drei
// Spielarten einen vollstaendigen Testwert, mit Kennwort, Salz, Geheimnis
// und zusaetzlichen Daten. Die Testdatei rechnet genau diese drei und
// vergleicht. Das ist eine FREMDE Gegenprobe: die Zahlen stammen nicht aus
// diesem Programm. Ohne sie waere eine selbstgebaute
// Schluesselableitungsfunktion nur gegen sich selbst geprueft, und eine
// falsche faellt nirgends auf - sie liefert einfach einen anderen
// Schluessel, und die KeePass-Datei bliebe mit der Meldung "falsches
// Kennwort" zu.
package classes

import "encoding/binary"

// Argon2Type ist die Spielart.
type Argon2Type uint32

// Die Zahlen sind die der Beschreibung; sie gehen in den ersten Hashwert
// ein, und darum ist eine falsche Zahl ein falscher Schluessel.
const (
	Argon2D  Argon2Type = 0
	Argon2I  Argon2Type = 1
	Argon2ID Argon2Type = 2
)

// Argon2Version ist die Fassung, die dieses Programm rechnet: 0x13, also
// 1.3. Die aeltere 0x10 rechnet die Bezugsstellen anders und ist hier NICHT
// gebaut - eine Datei in der alten Fassung wird abgewiesen und nicht mit der
// neuen gerechnet, denn das ergaebe stillschweigend einen falschen
// Schluessel.
const Argon2Version = 0x13

// Argon2BlockLen ist die Blocklaenge in Byte.
const Argon2BlockLen = 1024

// argon2Words ist die Blocklaenge in 64-Bit-Woertern.
const argon2Words = Argon2BlockLen / 8

// argon2SyncPoints ist die Zahl der Abschnitte je Durchgang. Vier, fest in
// der Beschreibung.
const argon2SyncPoints = 4

// Argon2MaxMemory ist die groesste Speichermenge in KiB, die dieses
// Programm zulaesst.
//
// Die Beschreibung erlaubt bis zu 2^32-1 KiB, also vier Terabyte. Eine
// KeePass-Datei nennt die Menge SELBST, und sie ist eine fremde Eingabe:
// ohne Obergrenze waere eine praeparierte Datei der Weg, mit zwei Zeilen im
// Dateikopf den Arbeitsspeicher zu fuellen. Ein Gigabyte ist mehr, als
// KeePass jemals einstellt, und wenig genug, um es zu ueberleben.
const Argon2MaxMemory = 1 << 20

// Argon2MaxTime ist die hoechste Zahl von Durchgaengen. Aus demselben
// Grund: eine Datei, die eine Milliarde Durchgaenge nennt, ist kein Tresor,
// sondern ein Stillstand.
const Argon2MaxTime = 1 << 14

// Argon2MaxLanes ist die hoechste Zahl von Spuren. Die Beschreibung nennt
// 2^24; so viele Spuren hat kein Rechner.
const Argon2MaxLanes = 1 << 10

// Argon2 rechnet den Schluessel.
//
//	password  das Kennwort
//	salt      das Salz, mindestens 8 Byte
//	secret    ein zusaetzliches Geheimnis, darf leer sein
//	ad        zusaetzliche Daten, duerfen leer sein
//	time      Zahl der Durchgaenge, mindestens 1
//	memory    Speicher in KiB, mindestens 8*lanes
//	lanes     Zahl der Spuren, mindestens 1
//	tagLen    gewuenschte Schluessellaenge, mindestens 4
//
// Gerechnet wird in EINEM Strang. Die Beschreibung erlaubt, die Spuren
// gleichzeitig zu rechnen, und das ist schneller; es ist hier nicht gebaut,
// weil es am Ergebnis nichts aendert und weil ein Fehler in der
// Gleichzeitigkeit sich als falscher Schluessel zeigt, nicht als Absturz.
// Was das kostet, steht in docs/cxf.md.
func Argon2(kind Argon2Type, password, salt, secret, ad []byte,
	time, memory, lanes uint32, tagLen uint32) ([]byte, error) {
	switch kind {
	case Argon2D, Argon2I, Argon2ID:
	default:
		return nil, Fail("error.argon2.type")
	}
	if lanes < 1 || lanes > Argon2MaxLanes {
		return nil, Fail("error.argon2.lanes")
	}
	if time < 1 || time > Argon2MaxTime {
		return nil, Fail("error.argon2.time")
	}
	if memory > Argon2MaxMemory {
		return nil, Fail("error.argon2.memory")
	}
	if memory < 8*lanes {
		return nil, Fail("error.argon2.memory")
	}
	if len(salt) < 8 {
		return nil, Fail("error.argon2.salt")
	}
	if tagLen < 4 || tagLen > 1<<20 {
		return nil, Fail("error.argon2.taglen")
	}

	// Die Speichermenge wird auf ein Vielfaches von 4*lanes abgerundet -
	// jede Spur bekommt gleich viele Bloecke, und jede Spur hat vier
	// Abschnitte.
	blocks := memory / (argon2SyncPoints * lanes) * (argon2SyncPoints * lanes)
	laneLen := blocks / lanes
	segmentLen := laneLen / argon2SyncPoints

	h0, err := argon2First(kind, password, salt, secret, ad, time, memory, lanes, tagLen)
	if err != nil {
		return nil, err
	}
	defer Wipe(h0)

	memoryBlocks := make([][argon2Words]uint64, blocks)
	if err := argon2Seed(memoryBlocks, h0, lanes, laneLen); err != nil {
		return nil, err
	}
	if err := argon2Fill(kind, memoryBlocks, time, lanes, laneLen, segmentLen); err != nil {
		return nil, err
	}

	// Der letzte Block jeder Spur wird zusammengefasst.
	var last [argon2Words]uint64
	for lane := uint32(0); lane < lanes; lane++ {
		block := &memoryBlocks[lane*laneLen+laneLen-1]
		for i := range last {
			last[i] ^= block[i]
		}
	}
	raw := argon2Bytes(&last)
	tag, err := argon2LongHash(raw, tagLen)
	Wipe(raw)
	// Der ganze Speicher wird ueberschrieben. Er enthaelt Zwischenwerte,
	// aus denen sich das Kennwort nicht zurueckrechnen laesst - aber er
	// enthaelt den Schluessel, und der soll nicht im Halden liegenbleiben,
	// bis die Speicherverwaltung ihn irgendwann wiederverwendet.
	for i := range memoryBlocks {
		for j := range memoryBlocks[i] {
			memoryBlocks[i][j] = 0
		}
	}
	if err != nil {
		return nil, err
	}
	return tag, nil
}

// argon2First rechnet H0: den Hashwert ueber alle Parameter UND alle
// Eingaben. Dass die Parameter mit eingehen, ist der Grund, warum ein
// anderer Speicherwert einen anderen Schluessel ergibt - und warum eine
// KeePass-Datei ihre Parameter mitliefern muss.
func argon2First(kind Argon2Type, password, salt, secret, ad []byte,
	time, memory, lanes, tagLen uint32) ([]byte, error) {
	state, err := newBlake2b(64, nil)
	if err != nil {
		return nil, err
	}
	var number [4]byte
	put := func(value uint32) {
		binary.LittleEndian.PutUint32(number[:], value)
		state.Write(number[:])
	}
	putBlock := func(data []byte) {
		put(uint32(len(data)))
		state.Write(data)
	}
	put(lanes)
	put(tagLen)
	put(memory)
	put(time)
	put(Argon2Version)
	put(uint32(kind))
	putBlock(password)
	putBlock(salt)
	putBlock(secret)
	putBlock(ad)
	return state.Sum(nil), nil
}

// argon2Seed fuellt die ersten zwei Bloecke jeder Spur.
func argon2Seed(blocks [][argon2Words]uint64, h0 []byte, lanes, laneLen uint32) error {
	input := make([]byte, 64+8)
	copy(input, h0)
	defer Wipe(input)
	for lane := uint32(0); lane < lanes; lane++ {
		for column := uint32(0); column < 2; column++ {
			binary.LittleEndian.PutUint32(input[64:], column)
			binary.LittleEndian.PutUint32(input[68:], lane)
			raw, err := argon2LongHash(input, Argon2BlockLen)
			if err != nil {
				return err
			}
			argon2Words64(raw, &blocks[lane*laneLen+column])
			Wipe(raw)
		}
	}
	return nil
}

// argon2Fill ist die eigentliche Arbeit: Durchgang fuer Durchgang,
// Abschnitt fuer Abschnitt, Spur fuer Spur.
func argon2Fill(kind Argon2Type, blocks [][argon2Words]uint64,
	time, lanes, laneLen, segmentLen uint32) error {
	var addresses [argon2Words]uint64
	var input [argon2Words]uint64
	var zero [argon2Words]uint64
	for pass := uint32(0); pass < time; pass++ {
		for slice := uint32(0); slice < argon2SyncPoints; slice++ {
			// Die Spielart entscheidet, WORAUS die Bezugsstelle kommt:
			// bei d aus dem vorigen Block (also aus dem Kennwort - das
			// macht sie schnell und zugleich angreifbar ueber den
			// Speicherzugriff), bei i aus einem Zaehler (also
			// unabhaengig vom Kennwort). id macht den ersten halben
			// Durchgang wie i und den Rest wie d.
			independent := kind == Argon2I ||
				(kind == Argon2ID && pass == 0 && slice < argon2SyncPoints/2)
			for lane := uint32(0); lane < lanes; lane++ {
				if independent {
					input = zero
					input[0] = uint64(pass)
					input[1] = uint64(lane)
					input[2] = uint64(slice)
					input[3] = uint64(len(blocks))
					input[4] = uint64(time)
					input[5] = uint64(kind)
				}
				start := uint32(0)
				if pass == 0 && slice == 0 {
					// Die ersten zwei Bloecke jeder Spur stehen schon
					// aus argon2Seed.
					start = 2
					if independent {
						// Hier, einmal vorab: der erste Satz
						// Bezugszahlen wird in der Schleife erst bei
						// Index 128 neu gerechnet, und ohne diesen
						// Aufruf stuenden die ersten 126 Bloecke
						// dieses Abschnitts auf lauter Nullen.
						argon2Addresses(&input, &addresses)
					}
				}
				for index := start; index < segmentLen; index++ {
					column := slice*segmentLen + index
					here := lane*laneLen + column
					previous := here - 1
					if column == 0 {
						previous = lane*laneLen + laneLen - 1
					}
					var j1, j2 uint32
					if independent {
						if index%argon2Words == 0 {
							argon2Addresses(&input, &addresses)
						}
						value := addresses[index%argon2Words]
						j1 = uint32(value)
						j2 = uint32(value >> 32)
					} else {
						value := blocks[previous][0]
						j1 = uint32(value)
						j2 = uint32(value >> 32)
					}
					reference := argon2Reference(j1, j2, pass, slice, lane, index,
						lanes, laneLen, segmentLen)
					argon2Mix(&blocks[here], &blocks[previous], &blocks[reference],
						pass > 0)
				}
			}
		}
	}
	return nil
}

// argon2Addresses rechnet den naechsten Satz Bezugszahlen fuer die
// Spielarten i und id: zweimal die Mischfunktion ueber einen Zaehlerblock.
func argon2Addresses(input, out *[argon2Words]uint64) {
	input[6]++
	var zero [argon2Words]uint64
	var once [argon2Words]uint64
	argon2Mix(&once, &zero, input, false)
	argon2Mix(out, &zero, &once, false)
}

// argon2Reference sucht die Bezugsstelle. Das ist die Stelle, an der ein
// Fehler NICHT auffaellt: das Ergebnis ist dann einfach ein anderer
// Schluessel, und keine Zeile stuerzt ab. Darum stehen die Testwerte der
// Beschreibung in der Testdatei.
func argon2Reference(j1, j2, pass, slice, lane, index, lanes, laneLen, segmentLen uint32) uint32 {
	// Aus welcher Spur die Bezugsstelle kommt. Im ersten Abschnitt des
	// ersten Durchgangs gibt es noch keine anderen Spuren.
	refLane := j2 % lanes
	if pass == 0 && slice == 0 {
		refLane = lane
	}
	// Wie viele Bloecke in Frage kommen.
	var size uint32
	sameLane := refLane == lane
	switch {
	case pass == 0:
		if sameLane {
			size = slice*segmentLen + index - 1
		} else {
			size = slice * segmentLen
			if index == 0 {
				size--
			}
		}
	default:
		if sameLane {
			size = laneLen - segmentLen + index - 1
		} else {
			size = laneLen - segmentLen
			if index == 0 {
				size--
			}
		}
	}
	// Die Verteilung aus der Beschreibung: zum Ende des Bereichs hin
	// dichter. Gerechnet wird in 64 Bit, damit das Quadrat nicht
	// ueberlaeuft.
	x := (uint64(j1) * uint64(j1)) >> 32
	y := (uint64(size) * x) >> 32
	relative := uint64(size) - 1 - y

	start := uint32(0)
	if pass != 0 && slice != argon2SyncPoints-1 {
		start = (slice + 1) * segmentLen
	}
	column := uint32((uint64(start) + relative) % uint64(laneLen))
	return refLane*laneLen + column
}

// --- Die Mischfunktion G ------------------------------------------------------

// argon2Mix ist G: zwei Bloecke hinein, einer heraus. Mit xorWith wird das
// Ergebnis in den Zielblock hineingerechnet statt ihn zu ersetzen - so
// verlangt es die Beschreibung ab dem zweiten Durchgang.
func argon2Mix(out, x, y *[argon2Words]uint64, xorWith bool) {
	var r, q [argon2Words]uint64
	for i := range r {
		r[i] = x[i] ^ y[i]
	}
	q = r
	// Erst die acht Zeilen, dann die acht Spalten. Jede Zeile sind
	// sechzehn Woerter, jede Spalte acht Paare im Abstand von sechzehn.
	for i := 0; i < 8; i++ {
		base := i * 16
		argon2Round(&q, base+0, base+1, base+2, base+3, base+4, base+5, base+6, base+7,
			base+8, base+9, base+10, base+11, base+12, base+13, base+14, base+15)
	}
	for i := 0; i < 8; i++ {
		base := i * 2
		argon2Round(&q, base, base+1, base+16, base+17, base+32, base+33, base+48, base+49,
			base+64, base+65, base+80, base+81, base+96, base+97, base+112, base+113)
	}
	for i := range out {
		value := q[i] ^ r[i]
		if xorWith {
			out[i] ^= value
		} else {
			out[i] = value
		}
	}
}

// argon2Round ist die Permutation P ueber sechzehn Woertern. Die Namen a
// bis p stehen fuer v0 bis v15 der Beschreibung, in dieser Reihenfolge.
//
// Die ersten vier Aufrufe gehen senkrecht durch die vier Spalten, die
// letzten vier schraeg - genau wie bei BLAKE2b, und genau diese Schraege
// verbindet die Spalten miteinander. Wer sie vertauscht, bekommt eine
// Funktion, die laeuft und einen anderen Schluessel liefert.
func argon2Round(v *[argon2Words]uint64, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p int) {
	argon2GB(v, a, e, i, m)
	argon2GB(v, b, f, j, n)
	argon2GB(v, c, g, k, o)
	argon2GB(v, d, h, l, p)
	argon2GB(v, a, f, k, p)
	argon2GB(v, b, g, l, m)
	argon2GB(v, c, h, i, n)
	argon2GB(v, d, e, j, o)
}

// argon2GB ist die Funktion GB: die Mischfunktion von BLAKE2b, aber mit
// einem zusaetzlichen Produkt der unteren 32 Bit. Dieses Produkt ist der
// Unterschied zu BLAKE2b, und es ist der Grund, warum sich Argon2 nicht
// billig auf einer Grafikkarte rechnen laesst.
func argon2GB(v *[argon2Words]uint64, a, b, c, d int) {
	v[a] = argon2Add(v[a], v[b])
	v[d] = rotateRight64(v[d]^v[a], 32)
	v[c] = argon2Add(v[c], v[d])
	v[b] = rotateRight64(v[b]^v[c], 24)
	v[a] = argon2Add(v[a], v[b])
	v[d] = rotateRight64(v[d]^v[a], 16)
	v[c] = argon2Add(v[c], v[d])
	v[b] = rotateRight64(v[b]^v[c], 63)
}

// argon2Add ist a + b + 2*unten32(a)*unten32(b).
func argon2Add(a, b uint64) uint64 {
	return a + b + 2*uint64(uint32(a))*uint64(uint32(b))
}

// --- H', der Hashwert beliebiger Laenge ---------------------------------------

// argon2LongHash ist H' aus der Beschreibung: BLAKE2b mit einer Laenge, die
// groesser sein darf als 64 Byte. Bis 64 ist es ein einfacher Aufruf mit der
// Laenge davor; darueber eine Kette, von der je 32 Byte genommen werden.
func argon2LongHash(data []byte, length uint32) ([]byte, error) {
	var prefix [4]byte
	binary.LittleEndian.PutUint32(prefix[:], length)
	if length <= Blake2bMaxLen {
		state, err := newBlake2b(int(length), nil)
		if err != nil {
			return nil, err
		}
		state.Write(prefix[:])
		state.Write(data)
		return state.Sum(nil), nil
	}
	out := make([]byte, 0, length)
	state, err := newBlake2b(Blake2bMaxLen, nil)
	if err != nil {
		return nil, err
	}
	state.Write(prefix[:])
	state.Write(data)
	current := state.Sum(nil)
	out = append(out, current[:32]...)
	// Solange mehr als 64 Byte fehlen, kommt die naechste Runde und es
	// werden wieder 32 Byte genommen. Der letzte Schritt liefert die
	// restlichen bis zu 64.
	for uint32(len(out))+Blake2bMaxLen < length {
		next, err := Blake2bSum(Blake2bMaxLen, nil, current)
		if err != nil {
			return nil, err
		}
		Wipe(current)
		current = next
		out = append(out, current[:32]...)
	}
	next, err := Blake2bSum(Blake2bMaxLen, nil, current)
	if err != nil {
		return nil, err
	}
	Wipe(current)
	out = append(out, next[:length-uint32(len(out))]...)
	Wipe(next)
	return out, nil
}

// --- Bloecke und Byte ---------------------------------------------------------

func argon2Words64(data []byte, out *[argon2Words]uint64) {
	for i := 0; i < argon2Words; i++ {
		out[i] = binary.LittleEndian.Uint64(data[i*8:])
	}
}

func argon2Bytes(block *[argon2Words]uint64) []byte {
	out := make([]byte, Argon2BlockLen)
	for i, word := range block {
		binary.LittleEndian.PutUint64(out[i*8:], word)
	}
	return out
}
