// Pruefung des Lesers fuer oeffentliche Schluessel.
//
// Gemessen wird gegen die Pruefschluessel aus agent_golden_test.go: deren
// oeffentliche Zeilen hat das echte ssh-keygen ausgegeben. Baute die
// Pruefung ihre Schluessel selbst zusammen, pruefte sie nur unseren eigenen
// Schreiber - und beide koennten denselben Irrtum teilen.
package classes

import (
	"bytes"
	"crypto/ed25519"
	"encoding/base64"
	"math/big"
	"strings"
	"testing"
)

// blobAusZeile holt den Mittelteil einer Zeile aus authorized_keys.
func blobAusZeile(t *testing.T, line string) []byte {
	t.Helper()
	parts := strings.Fields(line)
	if len(parts) < 2 {
		t.Fatalf("Zeile hat keinen oeffentlichen Teil: %q", line)
	}
	blob, err := base64.StdEncoding.DecodeString(parts[1])
	if err != nil {
		t.Fatalf("base64: %v", err)
	}
	return blob
}

// Die drei Verfahren, die vorkommen, je mit privatem und oeffentlichem Teil.
var pubPruefsaetze = []struct {
	name    string
	private string
	public  string
}{
	{"ed25519", goldenEd25519Private, goldenEd25519Public},
	{"ecdsa-p256", goldenEcdsaP256Private, goldenEcdsaP256Public},
	{"rsa-2048", goldenRsa2048Private, goldenRsa2048Public},
}

// Der oeffentliche Teil, aus ssh-keygens Zeile gelesen, muss genau der sein,
// den wir aus dem privaten Schluessel rechnen. Das ist der Kern: ohne diese
// Gleichheit wuerde PKCS#11 andere Merkmale nennen als der ssh-Agent.
func TestPublicBlobTrifftPrivatenTeil(t *testing.T) {
	for _, satz := range pubPruefsaetze {
		t.Run(satz.name, func(t *testing.T) {
			blob := blobAusZeile(t, satz.public)
			pub, err := ParseSSHPublicBlob(blob)
			if err != nil {
				t.Fatalf("oeffentlichen Teil lesen: %v", err)
			}
			key, err := ParseSSHKey([]byte(satz.private))
			if err != nil {
				t.Fatalf("privaten Teil lesen: %v", err)
			}
			defer key.Wipe()
			if !bytes.Equal(pub.Blob, key.Blob) {
				t.Errorf("die Bytes des oeffentlichen Teils stimmen nicht")
			}
			if pub.Algorithm != key.Algorithm {
				t.Errorf("Verfahren %q, erwartet %q", pub.Algorithm, key.Algorithm)
			}
			if pub.Fingerprint() != key.Fingerprint() {
				t.Errorf("Abdruck %q, erwartet %q",
					pub.Fingerprint(), key.Fingerprint())
			}
		})
	}
}

// Die Zahlen muessen dieselben sein wie im privaten Schluessel - ein
// Schluessel, dessen Modulus oder Punkt um ein Bit abweicht, wuerde von
// fremder Software ohne Erklaerung abgelehnt.
func TestPublicZahlenTreffen(t *testing.T) {
	edBlob := blobAusZeile(t, goldenEd25519Public)
	edPub, err := ParseSSHPublicBlob(edBlob)
	if err != nil {
		t.Fatalf("ed25519: %v", err)
	}
	edKey, err := ParseSSHKey([]byte(goldenEd25519Private))
	if err != nil {
		t.Fatalf("ed25519 privat: %v", err)
	}
	defer edKey.Wipe()
	if !bytes.Equal(edPub.Ed25519, []byte(edKey.ed25519Key.Public().(ed25519.PublicKey))) {
		t.Errorf("ed25519: der Punkt weicht ab")
	}

	ecPub, err := ParseSSHPublicBlob(blobAusZeile(t, goldenEcdsaP256Public))
	if err != nil {
		t.Fatalf("ecdsa: %v", err)
	}
	ecKey, err := ParseSSHKey([]byte(goldenEcdsaP256Private))
	if err != nil {
		t.Fatalf("ecdsa privat: %v", err)
	}
	defer ecKey.Wipe()
	if ecPub.Ecdsa.X.Cmp(ecKey.ecdsaKey.X) != 0 ||
		ecPub.Ecdsa.Y.Cmp(ecKey.ecdsaKey.Y) != 0 {
		t.Errorf("ecdsa: der Punkt weicht ab")
	}
	if ecPub.Ecdsa.Curve != ecKey.ecdsaKey.Curve {
		t.Errorf("ecdsa: andere Kurve")
	}

	rsaPub, err := ParseSSHPublicBlob(blobAusZeile(t, goldenRsa2048Public))
	if err != nil {
		t.Fatalf("rsa: %v", err)
	}
	rsaKey, err := ParseSSHKey([]byte(goldenRsa2048Private))
	if err != nil {
		t.Fatalf("rsa privat: %v", err)
	}
	defer rsaKey.Wipe()
	if rsaPub.Rsa.Modulus.Cmp(rsaKey.rsaKey.N) != 0 {
		t.Errorf("rsa: der Modulus weicht ab")
	}
	if rsaPub.Rsa.Exponent.Int64() != int64(rsaKey.rsaKey.E) {
		t.Errorf("rsa: Exponent %s, erwartet %d",
			rsaPub.Rsa.Exponent, rsaKey.rsaKey.E)
	}
	if rsaPub.Rsa.Modulus.BitLen() != 2048 {
		t.Errorf("rsa: %d Bit, erwartet 2048", rsaPub.Rsa.Modulus.BitLen())
	}
}

// Was nicht aufgeht, muss abgewiesen werden. Jede Zeile hier ist ein Weg,
// auf dem sonst ein Schluessel entstuende, den ein zweites Programm anders
// liest als wir.
func TestPublicBlobWeistKaputtesAb(t *testing.T) {
	gut := blobAusZeile(t, goldenEd25519Public)
	ec := blobAusZeile(t, goldenEcdsaP256Public)

	// Ein Byte hinten angehaengt: die Nachricht geht nicht mehr auf.
	mitRest := append(append([]byte(nil), gut...), 0)
	// Ein Byte abgeschnitten.
	zuKurz := gut[:len(gut)-1]
	// Punkt um ein Bit verdreht: liegt nicht mehr auf der Kurve.
	nebenKurve := append([]byte(nil), ec...)
	nebenKurve[len(nebenKurve)-1] ^= 0x01
	// Unbekanntes Verfahren.
	fremd := &wireWriter{}
	fremd.Name("ssh-dss")
	fremd.String([]byte("egal"))
	// Nicht kuerzeste Schreibweise: eine fuehrende Null vor dem Exponenten.
	nichtKuerzest := &wireWriter{}
	nichtKuerzest.Name(algRSA)
	nichtKuerzest.String([]byte{0x00, 0x00, 0x01, 0x00, 0x01})
	nichtKuerzest.String(bytes.Repeat([]byte{0x7f}, 256))
	// Kurvenname und Verfahrensname passen nicht zusammen.
	falscheKurve := &wireWriter{}
	falscheKurve.Name(algP256)
	falscheKurve.Name("nistp384")
	falscheKurve.String(bytes.Repeat([]byte{0x04}, 65))
	// RSA zu kurz: 1024 Bit.
	zuSchwach := &wireWriter{}
	zuSchwach.Name(algRSA)
	zuSchwach.Mpint(big.NewInt(0x10001))
	zuSchwach.String(append([]byte{0x7f}, bytes.Repeat([]byte{0xff}, 127)...))
	// Gerader Exponent.
	geraderExponent := &wireWriter{}
	geraderExponent.Name(algRSA)
	geraderExponent.Mpint(big.NewInt(4))
	geraderExponent.String(append([]byte{0x7f}, bytes.Repeat([]byte{0xff}, 255)...))

	faelle := []struct {
		name string
		blob []byte
	}{
		{"leer", nil},
		{"ein Byte zu viel", mitRest},
		{"abgeschnitten", zuKurz},
		{"Punkt neben der Kurve", nebenKurve},
		{"fremdes Verfahren", fremd.Bytes()},
		{"nicht kuerzeste Schreibweise", nichtKuerzest.Bytes()},
		{"Kurvenname passt nicht", falscheKurve.Bytes()},
		{"RSA zu schwach", zuSchwach.Bytes()},
		{"gerader Exponent", geraderExponent.Bytes()},
	}
	for _, fall := range faelle {
		t.Run(fall.name, func(t *testing.T) {
			if _, err := ParseSSHPublicBlob(fall.blob); err == nil {
				t.Errorf("angenommen, erwartet abgewiesen")
			}
		})
	}
}

// Gegenprobe zur Liste darueber: der unveraenderte Schluessel wird
// angenommen. Ohne diese Zeile waere die Liste auch dann gruen, wenn der
// Leser ueberhaupt nichts mehr annimmt.
func TestPublicBlobNimmtGutenAn(t *testing.T) {
	for _, satz := range pubPruefsaetze {
		if _, err := ParseSSHPublicBlob(blobAusZeile(t, satz.public)); err != nil {
			t.Errorf("%s abgewiesen: %v", satz.name, err)
		}
	}
}
