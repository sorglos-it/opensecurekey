// Die Bausteine gegen die Testwerte ihrer eigenen Beschreibung.
//
// WARUM DAS HIER DER WICHTIGSTE TEST DIESES STRANGS IST: BLAKE2b, Argon2
// und Salsa20 sind in diesem Programm selbst gebaut, weil es keine
// Abhaengigkeiten haben darf. Eine selbstgebaute Hashfunktion, die nur
// gegen sich selbst geprueft ist, ist NICHT geprueft - sie liefert dann
// eben einen anderen Wert, und nichts stuerzt ab. Eine KeePass-Datei waere
// einfach nicht zu oeffnen, mit der Meldung "falsches Kennwort", und
// niemand kaeme auf die Hashfunktion.
//
// Die Zahlen hier stammen darum NICHT aus diesem Programm. Sie stehen in
// RFC 7693 Anhang A, in RFC 9106 Abschnitt 5 und im ECRYPT-Testsatz zu
// Salsa20. Beim Bauen hat das etwas gebracht: der Salsa20-Test war rot,
// weil in der Rundenfunktion zwei Woerter vertauscht waren. Der Fehler sah
// im Quelltext richtig aus und war an keinem Kommentar zu erkennen.
package classes

import (
	"encoding/hex"
	"strings"
	"testing"
)

// hexBytes liest einen Testwert.
func hexBytes(t *testing.T, text string) []byte {
	t.Helper()
	raw, err := hex.DecodeString(strings.ReplaceAll(text, " ", ""))
	if err != nil {
		t.Fatalf("Testwert unlesbar: %v", err)
	}
	return raw
}

// wiederholt baut die Eingaben der Argon2-Testwerte: lauter gleiche Byte.
func wiederholt(value byte, n int) []byte {
	out := make([]byte, n)
	for i := range out {
		out[i] = value
	}
	return out
}

// RFC 7693, Anhang A: BLAKE2b-512 von "abc". Dazu die zwei Werte aus der
// offiziellen Werteliste, die die Sonderfaelle treffen - die leere
// Nachricht und der Lauf mit Schluessel.
func TestBlake2bTestwerteDerBeschreibung(t *testing.T) {
	faelle := []struct {
		name     string
		key      []byte
		data     []byte
		erwartet string
	}{
		{
			name: "abc, RFC 7693 Anhang A",
			data: []byte("abc"),
			erwartet: "ba80a53f981c4d0d6a2797b69f12f6e9" +
				"4c212f14685ac4b74b12bb6fdbffa2d1" +
				"7d87c5392aab792dc252d5de4533cc95" +
				"18d38aa8dbf1925ab92386edd4009923",
		},
		{
			// Die leere Nachricht. Sie ist der Fall, in dem der letzte
			// Block ein Block aus Nullen ist - und der, den man beim
			// Bauen vergisst.
			name: "leere Nachricht",
			erwartet: "786a02f742015903c6c6fd852552d272" +
				"912f4740e15847618a86e217f71f5419" +
				"d25e1031afee585313896444934eb04b" +
				"903a685b1448b755d56f701afe9be2ce",
		},
		{
			// Mit Schluessel und leerer Nachricht. Pruft, dass der
			// Schluesselblock vorangestellt wird UND dass eine leere
			// Nachricht dann nicht als leer gilt.
			name: "Schluessel 00..3f, leere Nachricht",
			key:  zaehlende(64),
			erwartet: "10ebb67700b1868efb4417987acf4690" +
				"ae9d972fb7a590c2f02871799aaa4786" +
				"b5e996e8f0f4eb981fc214b005f42d2f" +
				"f4233499391653df7aefcbc13fc51568",
		},
	}
	for _, fall := range faelle {
		got, err := Blake2bSum(Blake2bMaxLen, fall.key, fall.data)
		if err != nil {
			t.Fatalf("%s: %v", fall.name, err)
		}
		want := hexBytes(t, fall.erwartet)
		if hex.EncodeToString(got) != hex.EncodeToString(want) {
			t.Errorf("%s:\n  bekommen %x\n  erwartet %x", fall.name, got, want)
		}
	}
}

// zaehlende baut 00 01 02 ... - der Schluessel der offiziellen Werteliste.
func zaehlende(n int) []byte {
	out := make([]byte, n)
	for i := range out {
		out[i] = byte(i)
	}
	return out
}

// Die kurze Ausgabelaenge muss auch stimmen: Argon2 ruft BLAKE2b mit 32
// Byte, und eine Laenge, die nur bei 64 richtig ist, faellt sonst erst in
// der Schluesselableitung auf.
func TestBlake2bKurzeAusgabe(t *testing.T) {
	got, err := Blake2bSum(32, nil, []byte("abc"))
	if err != nil {
		t.Fatalf("blake2b: %v", err)
	}
	if len(got) != 32 {
		t.Fatalf("Laenge %d, erwartet 32", len(got))
	}
	// BLAKE2b-256 von "abc", aus der offiziellen Werteliste.
	want := "bddd813c634239723171ef3fee98579b94964e3bb1cb3e427262c8c068d52319"
	if hex.EncodeToString(got) != want {
		t.Errorf("bekommen %x, erwartet %s", got, want)
	}
}

// Eine Ausgabe, die laenger ist als ein BLAKE2b-Hashwert, laeuft durch H'
// aus der Argon2-Beschreibung. Dass die Kette stimmt, zeigt erst der
// Argon2-Testwert unten - hier wird nur geprueft, dass die Laenge passt und
// dass zwei verschiedene Laengen nicht dasselbe ergeben.
func TestArgon2LangerHashwertHatDieLaenge(t *testing.T) {
	for _, laenge := range []uint32{1, 32, 63, 64, 65, 96, 1024, 1025} {
		got, err := argon2LongHash([]byte("eingabe"), laenge)
		if err != nil {
			t.Fatalf("%d Byte: %v", laenge, err)
		}
		if uint32(len(got)) != laenge {
			t.Fatalf("%d Byte verlangt, %d bekommen", laenge, len(got))
		}
	}
	// Die Laenge geht in den Hashwert ein. Waere das nicht so, waere der
	// kuerzere Wert der Anfang des laengeren - und genau das soll H'
	// verhindern.
	kurz, _ := argon2LongHash([]byte("eingabe"), 32)
	lang, _ := argon2LongHash([]byte("eingabe"), 64)
	if hex.EncodeToString(lang[:32]) == hex.EncodeToString(kurz) {
		t.Error("der kurze Wert ist der Anfang des langen - die Laenge geht " +
			"nicht in den Hashwert ein")
	}
}

// RFC 9106, Abschnitt 5: alle drei Spielarten, mit Kennwort, Salz,
// Geheimnis und zusaetzlichen Daten.
//
// Das ist die fremde Gegenprobe fuer die ganze Schluesselableitung: wenn
// diese drei Zahlen stimmen, stimmen H0, H', die Mischfunktion G, die
// Permutation P, die Suche nach der Bezugsstelle und - bei i und id - die
// Erzeugung der Bezugszahlen aus dem Zaehler. Jeder einzelne Fehler darin
// ergibt einen anderen Wert.
func TestArgon2TestwerteDerBeschreibung(t *testing.T) {
	faelle := []struct {
		name     string
		kind     Argon2Type
		erwartet string
	}{
		{"Argon2d, RFC 9106 Abschnitt 5.1", Argon2D,
			"512b391b6f1162975371d30919734294f868e3be3984f3c1a13a4db9fabe4acb"},
		{"Argon2i, RFC 9106 Abschnitt 5.2", Argon2I,
			"c814d9d1dc7f37aa13f0d77f2494bda1c8de6b016dd388d29952a4c4672b6ce8"},
		{"Argon2id, RFC 9106 Abschnitt 5.3", Argon2ID,
			"0d640df58d78766c08c037a34a8b53c9d01ef0452d75b65eb52520e96b01e659"},
	}
	for _, fall := range faelle {
		got, err := Argon2(fall.kind,
			wiederholt(0x01, 32), // Kennwort
			wiederholt(0x02, 16), // Salz
			wiederholt(0x03, 8),  // Geheimnis
			wiederholt(0x04, 12), // zusaetzliche Daten
			3,                    // Durchgaenge
			32,                   // Speicher in KiB
			4,                    // Spuren
			32)                   // Schluessellaenge
		if err != nil {
			t.Fatalf("%s: %v", fall.name, err)
		}
		if hex.EncodeToString(got) != fall.erwartet {
			t.Errorf("%s:\n  bekommen %x\n  erwartet %s", fall.name, got, fall.erwartet)
		}
	}
}

// Die Obergrenzen sind keine Zierde: die Parameter stehen in der
// KeePass-Datei, also in einer fremden Eingabe. Eine Datei, die vier
// Terabyte verlangt, muss abgewiesen werden, nicht gerechnet.
func TestArgon2WeistUnmoeglicheParameterAb(t *testing.T) {
	faelle := []struct {
		name                        string
		time, memory, lanes, tagLen uint32
		salt                        int
	}{
		{"zu viel Speicher", 1, Argon2MaxMemory + 1, 1, 32, 16},
		{"zu viele Durchgaenge", Argon2MaxTime + 1, 64, 1, 32, 16},
		{"zu viele Spuren", 1, 1 << 20, Argon2MaxLanes + 1, 32, 16},
		{"keine Durchgaenge", 0, 64, 1, 32, 16},
		{"keine Spuren", 1, 64, 0, 32, 16},
		{"zu wenig Speicher fuer die Spuren", 1, 8, 4, 32, 16},
		{"Salz zu kurz", 1, 64, 1, 32, 7},
		{"Schluessel zu kurz", 1, 64, 1, 3, 16},
	}
	for _, fall := range faelle {
		_, err := Argon2(Argon2D, []byte("kennwort"), wiederholt(2, fall.salt),
			nil, nil, fall.time, fall.memory, fall.lanes, fall.tagLen)
		if err == nil {
			t.Errorf("%s: angenommen, erwartet war eine Absage", fall.name)
		}
	}
	// Eine Spielart, die es nicht gibt, wird auch nicht geraten.
	if _, err := Argon2(Argon2Type(7), []byte("k"), wiederholt(2, 16), nil, nil,
		1, 64, 1, 32); err == nil {
		t.Error("unbekannte Spielart angenommen")
	}
}

// ECRYPT-Testsatz 6, Vektor 0: Salsa20/20 mit 256-Bit-Schluessel.
//
// Dieser Test war beim Bauen rot. Zwei Woerter in der Rundenfunktion waren
// vertauscht; das Verfahren lief, lieferte einen Strom und entschluesselte
// jede KeePass-Datei zu Unsinn. Ohne einen Wert von aussen waere das nicht
// aufgefallen.
func TestSalsa20TestwertDesEcryptSatzes(t *testing.T) {
	key := hexBytes(t, "0053A6F94C9FF24598EB3E91E4378ADD"+
		"3083D6297CCF2275C81B6EC11467BA0D")
	nonce := hexBytes(t, "0D74DB42A91077DE")
	stream, err := NewSalsa20(key, nonce)
	if err != nil {
		t.Fatalf("salsa20: %v", err)
	}
	got := make([]byte, 64)
	stream.XOR(got)
	want := "f5fad53f79f9df58c4aea0d0ed9a9601" +
		"f278112ca7180d565b420a48019670ea" +
		"f24ce493a86263f677b46ace1924773d" +
		"2bb25571e1aa8593758fc382b1280b71"
	if hex.EncodeToString(got) != want {
		t.Errorf("bekommen %x\nerwartet %s", got, want)
	}
}

// Der Strom muss ueber mehrere Aufrufe hinweg weiterlaufen - KeePass deckt
// die Kennwoerter eines nach dem anderen auf, und wer den Zaehler bei jedem
// Aufruf zurueckstellt, bekommt beim zweiten Kennwort Unsinn.
func TestSalsa20LaeuftUeberAufrufeWeiter(t *testing.T) {
	key := zaehlende(32)
	nonce := zaehlende(8)
	einmal, _ := NewSalsa20(key, nonce)
	ganz := make([]byte, 200)
	einmal.XOR(ganz)

	stueckweise, _ := NewSalsa20(key, nonce)
	zusammen := make([]byte, 0, 200)
	for _, laenge := range []int{1, 7, 56, 64, 72} {
		teil := make([]byte, laenge)
		stueckweise.XOR(teil)
		zusammen = append(zusammen, teil...)
	}
	if hex.EncodeToString(zusammen) != hex.EncodeToString(ganz) {
		t.Error("der Strom laeuft nicht ueber die Aufrufe hinweg weiter")
	}
}

// Ein Schluessel oder Einmalwert falscher Laenge wird abgewiesen und nicht
// aufgefuellt.
func TestSalsa20WeistFalscheLaengenAb(t *testing.T) {
	if _, err := NewSalsa20(zaehlende(16), zaehlende(8)); err == nil {
		t.Error("Schluessel mit 16 Byte angenommen")
	}
	if _, err := NewSalsa20(zaehlende(32), zaehlende(12)); err == nil {
		t.Error("Einmalwert mit 12 Byte angenommen")
	}
}
