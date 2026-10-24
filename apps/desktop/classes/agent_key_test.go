// Pruefungen fuer das Lesen von SSH-Schluesseln und das Unterschreiben.
//
// Der Maßstab ist aussen: jede Zeile, die unser Leser fuer den oeffentlichen
// Teil errechnet, muss Zeichen fuer Zeichen die sein, die ssh-keygen selbst
// ausgibt (agent_golden_test.go). Jede Unterschrift wird mit der
// Standardbibliothek nachgeprueft, nicht mit unserem eigenen Code.
package classes

import (
	"crypto"
	"crypto/ecdsa"
	"crypto/ed25519"
	"crypto/elliptic"
	"crypto/rand"
	"crypto/rsa"
	"crypto/sha256"
	"crypto/sha512"
	"encoding/pem"
	"math/big"
	"strings"
	"testing"
)

// goldenKeys ist die Liste der Pruefschluessel mit der Zeile, die
// ssh-keygen dazu ausgibt.
var goldenKeys = []struct {
	name    string
	private string
	public  string
	algo    string
}{
	{"ed25519", goldenEd25519Private, goldenEd25519Public, algEd25519},
	{"ecdsa-p256", goldenEcdsaP256Private, goldenEcdsaP256Public, algP256},
	{"rsa-2048", goldenRsa2048Private, goldenRsa2048Public, algRSA},
	{"pkcs8-ed25519", goldenPkcs8Ed25519Private, goldenPkcs8Ed25519Public, algEd25519},
}

// Der oeffentliche Teil, den wir errechnen, muss der von ssh-keygen sein.
// Das ist die eine Pruefung, die nicht durch einen Fehler auf beiden Seiten
// gruen werden kann: die Vergleichszeile kommt von einem fremden Programm.
func TestPublicLineMatchesSshKeygen(t *testing.T) {
	for _, golden := range goldenKeys {
		t.Run(golden.name, func(t *testing.T) {
			key, err := ParseSSHKey([]byte(golden.private))
			if err != nil {
				t.Fatalf("lesen: %v", err)
			}
			defer key.Wipe()
			if key.Algorithm != golden.algo {
				t.Errorf("Verfahren %q, erwartet %q", key.Algorithm, golden.algo)
			}
			// Der Name des Eintrags steht in der dritten Spalte und kommt
			// bei uns aus dem Tresor, nicht aus der Datei - darum werden
			// nur die ersten zwei Spalten verglichen.
			want := strings.Join(strings.Fields(golden.public)[:2], " ")
			line, err := SSHPublicLine(key.Blob, "")
			if err != nil {
				t.Fatalf("Zeile bauen: %v", err)
			}
			if got := strings.Join(strings.Fields(line)[:2], " "); got != want {
				t.Errorf("oeffentlicher Teil weicht ab\n  wir:       %s\n  ssh-keygen: %s", got, want)
			}
		})
	}
}

// Der Abdruck muss die Form haben, die ssh anzeigt.
func TestFingerprintForm(t *testing.T) {
	key, err := ParseSSHKey([]byte(goldenEd25519Private))
	if err != nil {
		t.Fatalf("lesen: %v", err)
	}
	defer key.Wipe()
	print := key.Fingerprint()
	if !strings.HasPrefix(print, "SHA256:") || len(print) != len("SHA256:")+43 {
		t.Errorf("Abdruck hat nicht die Form von ssh: %q", print)
	}
	if strings.ContainsAny(print[7:], "=") {
		t.Errorf("Abdruck enthaelt Fuellzeichen: %q", print)
	}
}

// Jede Unterschrift wird mit der Standardbibliothek nachgeprueft - also mit
// Code, der unseren nicht kennt.
func TestSignaturesVerify(t *testing.T) {
	data := []byte("das ist der Text, der unterschrieben wird")
	for _, golden := range goldenKeys {
		t.Run(golden.name, func(t *testing.T) {
			key, err := ParseSSHKey([]byte(golden.private))
			if err != nil {
				t.Fatalf("lesen: %v", err)
			}
			defer key.Wipe()
			signature, err := key.Sign(data, sshAgentRsaSha2_256)
			if err != nil {
				t.Fatalf("unterschreiben: %v", err)
			}
			algorithm, raw := splitSignature(t, signature)
			switch {
			case key.ed25519Key != nil:
				if algorithm != algEd25519 {
					t.Fatalf("Verfahren %q", algorithm)
				}
				public := key.ed25519Key.Public().(ed25519.PublicKey)
				if !ed25519.Verify(public, data, raw) {
					t.Error("die Unterschrift haelt nicht")
				}
			case key.ecdsaKey != nil:
				r, s := splitEcdsa(t, raw)
				digest := sha256.Sum256(data)
				if !ecdsa.Verify(&key.ecdsaKey.PublicKey, digest[:], r, s) {
					t.Error("die Unterschrift haelt nicht")
				}
			case key.rsaKey != nil:
				if algorithm != algRSA256 {
					t.Fatalf("mit dem Zusatzbit fuer SHA-256 muss %q kommen, nicht %q",
						algRSA256, algorithm)
				}
				digest := sha256.Sum256(data)
				if err := rsa.VerifyPKCS1v15(&key.rsaKey.PublicKey, crypto.SHA256, digest[:], raw); err != nil {
					t.Errorf("die Unterschrift haelt nicht: %v", err)
				}
			}
		})
	}
}

// Die Zusatzbits muessen das Verfahren wirklich umschalten. Ohne diese
// Pruefung koennte der Code sie lesen und trotzdem immer SHA-1 nehmen.
func TestRsaFlagsChooseHash(t *testing.T) {
	data := []byte("noch ein Text")
	key, err := ParseSSHKey([]byte(goldenRsa2048Private))
	if err != nil {
		t.Fatalf("lesen: %v", err)
	}
	defer key.Wipe()
	cases := []struct {
		flags uint32
		want  string
		hash  func([]byte) []byte
		id    crypto.Hash
	}{
		{0, algRSA, func(b []byte) []byte { return nil }, 0},
		{sshAgentRsaSha2_256, algRSA256, func(b []byte) []byte {
			sum := sha256.Sum256(b)
			return sum[:]
		}, crypto.SHA256},
		{sshAgentRsaSha2_512, algRSA512, func(b []byte) []byte {
			sum := sha512.Sum512(b)
			return sum[:]
		}, crypto.SHA512},
		// Sind beide Bits gesetzt, gilt das groessere - so macht es OpenSSH.
		{sshAgentRsaSha2_256 | sshAgentRsaSha2_512, algRSA512, func(b []byte) []byte {
			sum := sha512.Sum512(b)
			return sum[:]
		}, crypto.SHA512},
	}
	for _, one := range cases {
		signature, err := key.Sign(data, one.flags)
		if err != nil {
			t.Fatalf("unterschreiben mit %d: %v", one.flags, err)
		}
		algorithm, raw := splitSignature(t, signature)
		if algorithm != one.want {
			t.Errorf("Bits %d ergeben %q, erwartet %q", one.flags, algorithm, one.want)
			continue
		}
		if one.id == 0 {
			continue
		}
		digest := one.hash(data)
		if err := rsa.VerifyPKCS1v15(&key.rsaKey.PublicKey, one.id, digest, raw); err != nil {
			t.Errorf("Bits %d: Unterschrift haelt nicht: %v", one.flags, err)
		}
	}
}

// Ein Schluessel mit Passwort muss mit genau dieser Auskunft abgewiesen
// werden - nicht als Formatfehler. Sonst sucht der Anwender am falschen Ort.
func TestLockedKeyIsNamedAsSuch(t *testing.T) {
	_, err := ParseSSHKey([]byte(goldenLockedPrivate))
	problem, ok := err.(*Problem)
	if !ok || problem.Key != "error.agent.keylocked" {
		t.Fatalf("erwartet error.agent.keylocked, bekommen %v", err)
	}
}

// Kaputte Eingaben duerfen nie durchkommen und nie in Panik enden.
func TestBrokenKeysAreRefused(t *testing.T) {
	good := []byte(goldenEd25519Private)
	cases := map[string][]byte{
		"leer":            {},
		"kein PEM":        []byte("das ist kein Schluessel"),
		"PEM ohne Inhalt": []byte("-----BEGIN OPENSSH PRIVATE KEY-----\n\n-----END OPENSSH PRIVATE KEY-----\n"),
		"halbiert":        good[:len(good)/2],
	}
	for name, data := range cases {
		if _, err := ParseSSHKey(data); err == nil {
			t.Errorf("%s: wurde angenommen", name)
		}
	}
	// Jedes einzelne gekippte Bit im Behaelter muss entweder abgewiesen
	// werden oder einen anderen oeffentlichen Teil ergeben - nie still
	// denselben Schluessel wie das Original.
	original, err := ParseSSHKey(good)
	if err != nil {
		t.Fatalf("lesen: %v", err)
	}
	defer original.Wipe()
	broken := 0
	for index := 60; index < len(good)-40 && index < 200; index++ {
		if good[index] == '\n' {
			continue
		}
		bent := append([]byte(nil), good...)
		bent[index] ^= 0x01
		key, parseErr := ParseSSHKey(bent)
		if parseErr != nil {
			broken++
			continue
		}
		if constantEqual(key.Blob, original.Blob) {
			// Base64 traegt Fuellbits: manche Kippungen aendern nichts.
			// Dann muss der Schluessel derselbe sein, und das ist in
			// Ordnung - ein ANDERER Schluessel unter demselben Namen
			// waere der Fehler.
			key.Wipe()
			continue
		}
		key.Wipe()
	}
	if broken == 0 {
		t.Error("keine einzige Kippung wurde abgewiesen - prueft der Leser ueberhaupt?")
	}
}

// Der oeffentliche Punkt einer ECDSA-Datei wird nicht geglaubt, sondern aus
// der geheimen Zahl nachgerechnet. Das ist nicht Formtreue, sondern der
// Riegel gegen eine Datei, die einen Punkt NENNT und mit einer anderen Zahl
// unterschreibt - der Agent bote dann eine Identitaet an und unterschriebe
// mit einer zweiten, und der Server lehnte ohne erkennbaren Grund ab.
func TestEcdsaPointIsCrossChecked(t *testing.T) {
	// Vorrichtungspruefung: derselbe Bauweg mit dem RICHTIGEN Punkt muss
	// durchgehen. Sonst weist der Leser nur unseren Nachbau ab, und der
	// Fall sagt nichts ueber den Punkt.
	good, err := ParseSSHKey(buildEcdsaContainer(t, false))
	if err != nil {
		t.Fatalf("der Nachbau mit richtigem Punkt wird abgewiesen: %v", err)
	}
	reference, err := ParseSSHKey([]byte(goldenEcdsaP256Private))
	if err != nil {
		t.Fatalf("Vergleichsschluessel: %v", err)
	}
	defer reference.Wipe()
	defer good.Wipe()
	if !constantEqual(good.Blob, reference.Blob) {
		t.Fatal("der Nachbau ergibt einen anderen Schluessel als die echte Datei")
	}

	// Und jetzt derselbe Bauweg mit einem fremden Punkt.
	if _, err := ParseSSHKey(buildEcdsaContainer(t, true)); err == nil {
		t.Fatal("eine Datei mit falschem Punkt wurde angenommen")
	}
}

// buildEcdsaContainer baut einen Behaelter openssh-key-v1 um die geheime
// Zahl des Pruefschluessels. Mit bend wird der genannte Punkt gegen den
// einer anderen Kurve gleicher Groesse getauscht.
//
// Dieser Nachbau dient NUR den Faellen, die etwas ablehnen sollen. Dass der
// Leser richtig liest, ist an den Zeilen von ssh-keygen gemessen, nicht hier.
func buildEcdsaContainer(t *testing.T, bend bool) []byte {
	t.Helper()
	source, err := ParseSSHKey([]byte(goldenEcdsaP256Private))
	if err != nil {
		t.Fatalf("Pruefschluessel: %v", err)
	}
	defer source.Wipe()
	point, err := markPoint(source.ecdsaKey.Curve, source.ecdsaKey.X, source.ecdsaKey.Y)
	if err != nil {
		t.Fatalf("Punkt bauen: %v", err)
	}
	if bend {
		// Ein echter Punkt, aber der eines anderen Schluessels: so kann der
		// Leser ihn nicht an der Form erkennen, sondern nur am Rechnen.
		other, otherErr := ecdsaOtherPoint(t)
		if otherErr != nil {
			t.Fatalf("zweiter Punkt: %v", otherErr)
		}
		point = other
	}

	secret := &wireWriter{}
	secret.Uint32(0x4f534b31)
	secret.Uint32(0x4f534b31)
	secret.Name(algP256)
	secret.Name("nistp256")
	secret.String(point)
	secret.Mpint(source.ecdsaKey.D)
	secret.Name("nachbau")
	body := secret.Bytes()
	for pad := byte(1); len(body)%8 != 0; pad++ {
		body = append(body, pad)
	}

	out := &wireWriter{}
	out.Name("none")
	out.Name("none")
	out.Name("")
	out.Uint32(1)
	out.String(source.Blob)
	out.String(body)

	full := append(append([]byte(nil), opensshMagic...), out.Bytes()...)
	return pem.EncodeToMemory(&pem.Block{Type: "OPENSSH PRIVATE KEY", Bytes: full})
}

// ecdsaOtherPoint liefert den Punkt eines frisch erzeugten P-256-Schluessels.
func ecdsaOtherPoint(t *testing.T) ([]byte, error) {
	t.Helper()
	key, err := ecdsa.GenerateKey(elliptic.P256(), rand.Reader)
	if err != nil {
		return nil, err
	}
	return markPoint(key.Curve, key.X, key.Y)
}

// --- Helfer -----------------------------------------------------------------

func splitSignature(t *testing.T, signature []byte) (string, []byte) {
	t.Helper()
	reader := newWireReader(signature)
	algorithm, err := reader.Name()
	if err != nil {
		t.Fatalf("Verfahren lesen: %v", err)
	}
	raw, err := reader.String()
	if err != nil {
		t.Fatalf("Unterschrift lesen: %v", err)
	}
	if !reader.Done() {
		t.Fatal("hinter der Unterschrift steht noch etwas")
	}
	return algorithm, raw
}

func splitEcdsa(t *testing.T, raw []byte) (*big.Int, *big.Int) {
	t.Helper()
	reader := newWireReader(raw)
	r, err := reader.Mpint()
	if err != nil {
		t.Fatalf("r lesen: %v", err)
	}
	s, err := reader.Mpint()
	if err != nil {
		t.Fatalf("s lesen: %v", err)
	}
	if !reader.Done() {
		t.Fatal("hinter s steht noch etwas")
	}
	return r, s
}
