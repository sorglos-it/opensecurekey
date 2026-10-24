// Oeffentliche SSH-Schluessel lesen - ohne den privaten Teil.
//
// Warum es diese Datei gibt: ein oeffentlicher Schluessel ist kein
// Geheimnis, und das Geraet schickt ihn mit der Liste mit (Feld `pub`,
// shared/protocol.md Befehl 4). Wer die oeffentlichen Groessen - Kurvenpunkt,
// Modulus, Exponent - bisher nennen wollte, musste den ganzen Schluessel per
// `reveal` holen und den privaten Teil sofort wieder ueberschreiben. Das
// kostete einen Tastendruck je Schluessel, und wer lernt, Tastendruecke
// wegzudruecken, drueckt auch den weg, der zaehlt.
//
// Gelesen wird der oeffentliche Teil im Format von SSH (RFC 4253 Abschnitt
// 6.6, RFC 5656 Abschnitt 3.1, RFC 8709 Abschnitt 4): Name des Verfahrens,
// dann die Groessen. Kein fremdes Paket - gerechnet wird mit crypto/ecdh,
// crypto/elliptic und math/big aus der Standardbibliothek.
//
// Streng gelesen, weil diese Bytes von aussen kommen:
//   - die Nachricht muss restlos aufgehen, ein Rest ist ein Fehler
//   - der Punkt muss auf der Kurve liegen; das prueft crypto/ecdh
//   - neu geschrieben muss derselbe Bytestrom herauskommen. Damit faellt
//     jede nicht kuerzeste Schreibweise auf, bevor sie zu einem Schluessel
//     wird, den ein zweites Programm anders liest als wir.
package classes

import (
	"bytes"
	"crypto/ecdsa"
	"crypto/ed25519"
	"crypto/sha256"
	"encoding/base64"
	"math/big"
)

// minRsaBits ist die Untergrenze fuer RSA, dieselbe wie beim privaten
// Schluessel: darunter ist er nicht mehr Stand der Technik.
const minRsaBits = 2048

// SSHPublicKey ist ein gelesener oeffentlicher Schluessel. Genau eines der
// drei Felder ist gesetzt.
type SSHPublicKey struct {
	// Blob sind die gelesenen Bytes, kopiert.
	Blob []byte
	// Algorithm ist der Name des Verfahrens, etwa ssh-ed25519.
	Algorithm string

	Ed25519 ed25519.PublicKey
	Ecdsa   *ecdsa.PublicKey
	Rsa     *rsaPublicParts
}

// rsaPublicParts sind die beiden Zahlen eines RSA-Schluessels. Eine eigene
// kleine Form statt rsa.PublicKey: der Exponent darf hier groesser sein als
// ein int, und abgewiesen wird er mit einer Meldung statt mit einem
// Ueberlauf.
type rsaPublicParts struct {
	Modulus  *big.Int
	Exponent *big.Int
}

// Fingerprint ist der Abdruck, den ssh anzeigt - gleiche Rechnung wie beim
// privaten Schluessel, damit beide Wege denselben Abdruck nennen.
func (k *SSHPublicKey) Fingerprint() string {
	sum := sha256.Sum256(k.Blob)
	return "SHA256:" + base64.RawStdEncoding.EncodeToString(sum[:])
}

// ParseSSHPublicBlob liest den oeffentlichen Teil eines Schluessels.
func ParseSSHPublicBlob(blob []byte) (*SSHPublicKey, error) {
	if len(blob) == 0 || len(blob) > wireMax {
		return nil, Fail("error.agent.pubformat")
	}
	reader := newWireReader(blob)
	algorithm, err := reader.Name()
	if err != nil {
		return nil, Fail("error.agent.pubformat")
	}
	key := &SSHPublicKey{Algorithm: algorithm}
	switch algorithm {
	case algEd25519:
		err = readPublicEd25519(reader, key)
	case algP256, algP384, algP521:
		err = readPublicEcdsa(reader, key)
	case algRSA:
		err = readPublicRsa(reader, key)
	default:
		return nil, Fail("error.agent.keytype", algorithm)
	}
	if err != nil {
		return nil, err
	}
	if !reader.Done() {
		// Ein Rest heisst: die Gegenseite hat etwas anderes gemeint, als
		// wir gelesen haben.
		return nil, Fail("error.agent.pubformat")
	}
	// Gegenprobe: neu geschrieben muss genau dasselbe herauskommen. Sonst
	// war die Schreibweise nicht die kuerzeste, und ein zweites Programm
	// koennte daraus einen anderen Abdruck rechnen als wir.
	again, err := key.rebuild()
	if err != nil {
		return nil, err
	}
	if !bytes.Equal(again, blob) {
		return nil, Fail("error.agent.pubformat")
	}
	key.Blob = again
	return key, nil
}

func readPublicEd25519(reader *wireReader, key *SSHPublicKey) error {
	point, err := reader.String()
	if err != nil {
		return Fail("error.agent.pubformat")
	}
	if len(point) != ed25519.PublicKeySize {
		return Fail("error.agent.pubformat")
	}
	key.Ed25519 = ed25519.PublicKey(append([]byte(nil), point...))
	return nil
}

func readPublicEcdsa(reader *wireReader, key *SSHPublicKey) error {
	curveName, err := reader.Name()
	if err != nil {
		return Fail("error.agent.pubformat")
	}
	point, err := reader.String()
	if err != nil {
		return Fail("error.agent.pubformat")
	}
	curve, wantName, ok := curveFor(key.Algorithm)
	if !ok || curveName != wantName {
		// Der Kurvenname steht zweimal im Schluessel - im Verfahrensnamen
		// und noch einmal einzeln. Stimmen sie nicht ueberein, ist der
		// Schluessel nicht eindeutig und wird abgewiesen.
		return Fail("error.agent.keytype", key.Algorithm)
	}
	// Liegt der Punkt auf der Kurve? Das prueft crypto/ecdh mit, und ohne
	// diese Pruefung koennte eine Rechnung mit einem Punkt ausserhalb der
	// Kurve Geheimnisse verraten.
	ecdhCurve, _, ok := ecdhFor(key.Algorithm)
	if !ok {
		return Fail("error.agent.keytype", key.Algorithm)
	}
	if _, err := ecdhCurve.NewPublicKey(point); err != nil {
		return Fail("error.agent.pubformat")
	}
	x, y, err := splitPoint(curve, point)
	if err != nil {
		return Fail("error.agent.pubformat")
	}
	key.Ecdsa = &ecdsa.PublicKey{Curve: curve, X: x, Y: y}
	return nil
}

func readPublicRsa(reader *wireReader, key *SSHPublicKey) error {
	// Beim oeffentlichen Teil steht der Exponent VOR dem Modulus - anders
	// als im privaten Abschnitt. Vertauscht ergaebe das einen Schluessel,
	// der zu nichts passt.
	exponent, err := reader.Mpint()
	if err != nil {
		return Fail("error.agent.pubformat")
	}
	modulus, err := reader.Mpint()
	if err != nil {
		return Fail("error.agent.pubformat")
	}
	if modulus.BitLen() < minRsaBits {
		// Lieber klar ablehnen als still etwas Schwaches anbieten.
		return Fail("error.agent.keyweak")
	}
	// Ein gerader oder zu kleiner Exponent ergibt keinen brauchbaren
	// Schluessel; 3 ist der kleinste, der vorkommt.
	if exponent.Cmp(big.NewInt(3)) < 0 || exponent.Bit(0) == 0 {
		return Fail("error.agent.pubformat")
	}
	if exponent.Cmp(modulus) >= 0 {
		return Fail("error.agent.pubformat")
	}
	key.Rsa = &rsaPublicParts{Modulus: modulus, Exponent: exponent}
	return nil
}

// rebuild schreibt den gelesenen Schluessel wieder in das Format von SSH.
func (k *SSHPublicKey) rebuild() ([]byte, error) {
	out := &wireWriter{}
	switch {
	case k.Ed25519 != nil:
		out.Name(algEd25519)
		out.String(k.Ed25519)
	case k.Ecdsa != nil:
		_, curveName, ok := algorithmForCurve(k.Ecdsa.Curve)
		if !ok {
			return nil, Fail("error.agent.keytype", k.Algorithm)
		}
		point, err := markPoint(k.Ecdsa.Curve, k.Ecdsa.X, k.Ecdsa.Y)
		if err != nil {
			return nil, err
		}
		out.Name(k.Algorithm)
		out.Name(curveName)
		out.String(point)
	case k.Rsa != nil:
		out.Name(algRSA)
		out.Mpint(k.Rsa.Exponent)
		out.Mpint(k.Rsa.Modulus)
	default:
		return nil, Fail("error.agent.pubformat")
	}
	return out.Bytes(), nil
}
