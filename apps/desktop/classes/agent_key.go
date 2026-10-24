// SSH-Schluessel lesen und mit ihnen unterschreiben.
//
// Der Tresor gibt das Geheimnis so heraus, wie es hineingelegt wurde: als
// Schluesseldatei. Diese Datei versteht die zwei Formen, in denen solche
// Dateien vorkommen - den Behaelter "openssh-key-v1" und die aelteren
// PEM-Formen - und rechnet daraus den oeffentlichen Teil und die
// Unterschrift.
//
// Kein fremdes Paket: die Formate stehen in der Spezifikation, gerechnet
// wird mit crypto/ed25519, crypto/ecdsa und crypto/rsa aus der
// Standardbibliothek.
//
// Was hier NICHT geht und mit klarer Meldung abgewiesen wird: ein mit
// Passwort verschluesselter Schluessel. Ein Geraet, das Geheimnisse haelt,
// soll nicht noch eine zweite Passwortfrage stellen; wer so einen Schluessel
// hat, legt ihn entschluesselt in den Tresor.
package classes

import (
	"bytes"
	"crypto"
	"crypto/ecdh"
	"crypto/ecdsa"
	"crypto/ed25519"
	"crypto/elliptic"
	"crypto/rand"
	"crypto/rsa"
	"crypto/sha1"
	"crypto/sha256"
	"crypto/sha512"
	"crypto/x509"
	"encoding/base64"
	"encoding/pem"
	"math/big"
)

// Die Namen der Algorithmen, so wie sie auf der Leitung stehen.
const (
	algEd25519 = "ssh-ed25519"
	algRSA     = "ssh-rsa"
	algRSA256  = "rsa-sha2-256"
	algRSA512  = "rsa-sha2-512"
	algP256    = "ecdsa-sha2-nistp256"
	algP384    = "ecdsa-sha2-nistp384"
	algP521    = "ecdsa-sha2-nistp521"
)

// opensshMagic steht am Anfang des Behaelters, mit abschliessender Null.
var opensshMagic = []byte("openssh-key-v1\x00")

// SSHKey ist ein gelesener Schluessel: der oeffentliche Teil zum Zeigen, der
// private zum Unterschreiben.
type SSHKey struct {
	// Blob ist der oeffentliche Teil im Format von SSH. Genau diese Bytes
	// nennt der Agent als Identitaet, und genau sie schickt ssh zurueck,
	// wenn es unterschreiben lassen will.
	Blob []byte
	// Algorithm ist der Name des Schluessels, etwa ssh-ed25519.
	Algorithm string

	ed25519Key ed25519.PrivateKey
	ecdsaKey   *ecdsa.PrivateKey
	rsaKey     *rsa.PrivateKey
}

// Fingerprint ist der Abdruck, den ssh anzeigt: SHA256 ueber den
// oeffentlichen Teil, in base64 ohne Fuellzeichen.
func (k *SSHKey) Fingerprint() string {
	sum := sha256.Sum256(k.Blob)
	return "SHA256:" + base64.RawStdEncoding.EncodeToString(sum[:])
}

// Wipe ueberschreibt, was sich ueberschreiben laesst.
//
// Beim Ed25519-Schluessel ist das der ganze private Teil. Bei RSA und ECDSA
// steckt die Zahl in einem big.Int, und dessen Innenleben laesst sich von
// aussen nicht sicher loeschen - dort wird der Wert auf null gesetzt und der
// Verweis fallen gelassen. Das ist weniger, als man will, und es steht
// deshalb hier und in docs/anbindungen.md.
func (k *SSHKey) Wipe() {
	if k.ed25519Key != nil {
		Wipe(k.ed25519Key)
		k.ed25519Key = nil
	}
	if k.ecdsaKey != nil {
		k.ecdsaKey.D.SetInt64(0)
		k.ecdsaKey = nil
	}
	if k.rsaKey != nil {
		k.rsaKey.D.SetInt64(0)
		for _, prime := range k.rsaKey.Primes {
			prime.SetInt64(0)
		}
		k.rsaKey = nil
	}
}

// ParseSSHKey liest eine Schluesseldatei. data wird nur gelesen; der
// Aufrufer bleibt dafuer zustaendig, den Puffer danach zu ueberschreiben.
func ParseSSHKey(data []byte) (*SSHKey, error) {
	block, _ := pem.Decode(data)
	if block == nil {
		return nil, Fail("error.agent.keyformat")
	}
	switch block.Type {
	case "OPENSSH PRIVATE KEY":
		return parseOpenSSHKey(block.Bytes)
	case "PRIVATE KEY":
		key, err := x509.ParsePKCS8PrivateKey(block.Bytes)
		if err != nil {
			return nil, Fail("error.agent.keyformat")
		}
		return fromCryptoKey(key)
	case "RSA PRIVATE KEY":
		key, err := x509.ParsePKCS1PrivateKey(block.Bytes)
		if err != nil {
			return nil, Fail("error.agent.keyformat")
		}
		return fromCryptoKey(key)
	case "EC PRIVATE KEY":
		key, err := x509.ParseECPrivateKey(block.Bytes)
		if err != nil {
			return nil, Fail("error.agent.keyformat")
		}
		return fromCryptoKey(key)
	case "ENCRYPTED PRIVATE KEY":
		return nil, Fail("error.agent.keylocked")
	}
	return nil, Fail("error.agent.keyformat")
}

// parseOpenSSHKey liest den Behaelter openssh-key-v1.
func parseOpenSSHKey(body []byte) (*SSHKey, error) {
	if !bytes.HasPrefix(body, opensshMagic) {
		return nil, Fail("error.agent.keyformat")
	}
	reader := newWireReader(body[len(opensshMagic):])
	cipherName, err := reader.Name()
	if err != nil {
		return nil, err
	}
	kdfName, err := reader.Name()
	if err != nil {
		return nil, err
	}
	if _, err := reader.String(); err != nil { // kdfoptions
		return nil, err
	}
	if cipherName != "none" || kdfName != "none" {
		// Ein Schluessel mit Passwort. Das ist kein Formatfehler, und die
		// Meldung sagt genau das - sonst sucht der Anwender am falschen Ort.
		return nil, Fail("error.agent.keylocked")
	}
	count, err := reader.Uint32()
	if err != nil {
		return nil, err
	}
	if count != 1 {
		// Mehrere Schluessel in einer Datei kommen in der Praxis nicht vor,
		// und welcher gemeint waere, steht nirgends.
		return nil, Fail("error.agent.keyformat")
	}
	if _, err := reader.String(); err != nil { // oeffentlicher Teil, wird neu gerechnet
		return nil, err
	}
	secret, err := reader.String()
	if err != nil {
		return nil, err
	}
	return parseOpenSSHSecret(secret)
}

// parseOpenSSHSecret liest den unverschluesselten privaten Abschnitt.
func parseOpenSSHSecret(secret []byte) (*SSHKey, error) {
	reader := newWireReader(secret)
	first, err := reader.Uint32()
	if err != nil {
		return nil, err
	}
	second, err := reader.Uint32()
	if err != nil {
		return nil, err
	}
	if first != second {
		// Die beiden Zahlen sind gleich, wenn richtig entschluesselt wurde.
		// Bei "none" heisst ein Unterschied: die Datei ist beschaedigt.
		return nil, Fail("error.agent.keyformat")
	}
	keyType, err := reader.Name()
	if err != nil {
		return nil, err
	}
	switch keyType {
	case algEd25519:
		return readOpenSSHEd25519(reader)
	case algP256, algP384, algP521:
		return readOpenSSHEcdsa(reader, keyType)
	case algRSA:
		return readOpenSSHRsa(reader)
	}
	return nil, Fail("error.agent.keytype", keyType)
}

func readOpenSSHEd25519(reader *wireReader) (*SSHKey, error) {
	public, err := reader.String()
	if err != nil {
		return nil, err
	}
	private, err := reader.String()
	if err != nil {
		return nil, err
	}
	if len(public) != ed25519.PublicKeySize || len(private) != ed25519.PrivateKeySize {
		return nil, Fail("error.agent.keyformat")
	}
	// Der private Teil endet auf den oeffentlichen. Stimmt das nicht,
	// wuerden Unterschrift und genannte Identitaet nicht zusammenpassen.
	if !bytes.Equal(private[ed25519.SeedSize:], public) {
		return nil, Fail("error.agent.keyformat")
	}
	key := ed25519.PrivateKey(append([]byte(nil), private...))
	return newEd25519Key(key), nil
}

func readOpenSSHEcdsa(reader *wireReader, keyType string) (*SSHKey, error) {
	curveName, err := reader.Name()
	if err != nil {
		return nil, err
	}
	point, err := reader.String()
	if err != nil {
		return nil, err
	}
	secret, err := reader.Mpint()
	if err != nil {
		return nil, err
	}
	curve, wantName, ok := curveFor(keyType)
	if !ok || curveName != wantName {
		return nil, Fail("error.agent.keytype", keyType)
	}
	// Der Punkt wird nicht aus der Datei uebernommen, sondern aus der
	// geheimen Zahl neu gerechnet und dann verglichen. Das ist die
	// Gegenprobe: stimmt der Punkt in der Datei nicht, wuerde der Agent eine
	// andere Identitaet anbieten als die, mit der er unterschreibt - der
	// Server lehnte ab, ohne dass jemand erkennt, warum.
	derived, err := derivePoint(keyType, secret)
	if err != nil {
		return nil, err
	}
	if !bytes.Equal(derived, point) {
		return nil, Fail("error.agent.keyformat")
	}
	x, y, err := splitPoint(curve, derived)
	if err != nil {
		return nil, err
	}
	key := &ecdsa.PrivateKey{
		PublicKey: ecdsa.PublicKey{Curve: curve, X: x, Y: y},
		D:         secret,
	}
	return newEcdsaKey(key)
}

// derivePoint rechnet den oeffentlichen Punkt aus der geheimen Zahl. Dabei
// prueft crypto/ecdh gleich mit, dass die Zahl im erlaubten Bereich liegt.
func derivePoint(algorithm string, secret *big.Int) ([]byte, error) {
	ecdhCurve, size, ok := ecdhFor(algorithm)
	if !ok {
		return nil, Fail("error.agent.keytype", algorithm)
	}
	raw := secret.Bytes()
	if len(raw) > size {
		return nil, Fail("error.agent.keyformat")
	}
	scalar := make([]byte, size)
	copy(scalar[size-len(raw):], raw)
	defer Wipe(scalar)
	private, err := ecdhCurve.NewPrivateKey(scalar)
	if err != nil {
		return nil, Fail("error.agent.keyformat")
	}
	return private.PublicKey().Bytes(), nil
}

// splitPoint zerlegt einen unkomprimierten Punkt (0x04, X, Y) in die zwei
// Zahlen, die crypto/ecdsa erwartet.
func splitPoint(curve elliptic.Curve, point []byte) (*big.Int, *big.Int, error) {
	size := (curve.Params().BitSize + 7) / 8
	if len(point) != 1+2*size || point[0] != 4 {
		return nil, nil, Fail("error.agent.keyformat")
	}
	x := new(big.Int).SetBytes(point[1 : 1+size])
	y := new(big.Int).SetBytes(point[1+size:])
	return x, y, nil
}

// markPoint baut den unkomprimierten Punkt aus zwei Zahlen.
func markPoint(curve elliptic.Curve, x, y *big.Int) ([]byte, error) {
	size := (curve.Params().BitSize + 7) / 8
	if x == nil || y == nil || x.Sign() < 0 || y.Sign() < 0 ||
		len(x.Bytes()) > size || len(y.Bytes()) > size {
		return nil, Fail("error.agent.keyformat")
	}
	out := make([]byte, 1+2*size)
	out[0] = 4
	x.FillBytes(out[1 : 1+size])
	y.FillBytes(out[1+size:])
	return out, nil
}

func readOpenSSHRsa(reader *wireReader) (*SSHKey, error) {
	modulus, err := reader.Mpint()
	if err != nil {
		return nil, err
	}
	exponent, err := reader.Mpint()
	if err != nil {
		return nil, err
	}
	secret, err := reader.Mpint()
	if err != nil {
		return nil, err
	}
	if _, err := reader.Mpint(); err != nil { // iqmp, wird neu gerechnet
		return nil, err
	}
	primeP, err := reader.Mpint()
	if err != nil {
		return nil, err
	}
	primeQ, err := reader.Mpint()
	if err != nil {
		return nil, err
	}
	if !exponent.IsInt64() {
		return nil, Fail("error.agent.keyformat")
	}
	key := &rsa.PrivateKey{
		PublicKey: rsa.PublicKey{N: modulus, E: int(exponent.Int64())},
		D:         secret,
		Primes:    []*big.Int{primeP, primeQ},
	}
	key.Precompute()
	if err := key.Validate(); err != nil {
		return nil, Fail("error.agent.keyformat")
	}
	return newRsaKey(key)
}

// fromCryptoKey nimmt einen Schluessel aus den PEM-Formen.
func fromCryptoKey(key any) (*SSHKey, error) {
	switch typed := key.(type) {
	case ed25519.PrivateKey:
		return newEd25519Key(typed), nil
	case *ecdsa.PrivateKey:
		return newEcdsaKey(typed)
	case *rsa.PrivateKey:
		return newRsaKey(typed)
	}
	return nil, Fail("error.agent.keytype", "PEM")
}

func newEd25519Key(key ed25519.PrivateKey) *SSHKey {
	blob := &wireWriter{}
	blob.Name(algEd25519)
	blob.String(key.Public().(ed25519.PublicKey))
	return &SSHKey{Blob: blob.Bytes(), Algorithm: algEd25519, ed25519Key: key}
}

func newEcdsaKey(key *ecdsa.PrivateKey) (*SSHKey, error) {
	algorithm, curveName, ok := algorithmForCurve(key.Curve)
	if !ok {
		return nil, Fail("error.agent.keytype", "ecdsa")
	}
	point, err := markPoint(key.Curve, key.X, key.Y)
	if err != nil {
		return nil, err
	}
	blob := &wireWriter{}
	blob.Name(algorithm)
	blob.Name(curveName)
	blob.String(point)
	return &SSHKey{Blob: blob.Bytes(), Algorithm: algorithm, ecdsaKey: key}, nil
}

// ecdhFor nennt die Kurve aus crypto/ecdh und die Laenge ihrer Zahlen.
func ecdhFor(algorithm string) (ecdh.Curve, int, bool) {
	switch algorithm {
	case algP256:
		return ecdh.P256(), 32, true
	case algP384:
		return ecdh.P384(), 48, true
	case algP521:
		return ecdh.P521(), 66, true
	}
	return nil, 0, false
}

func newRsaKey(key *rsa.PrivateKey) (*SSHKey, error) {
	if key.N.BitLen() < 2048 {
		// Unter 2048 Bit ist der Schluessel nicht mehr Stand der Technik.
		// Lieber klar ablehnen als still etwas Schwaches anbieten.
		return nil, Fail("error.agent.keyweak")
	}
	blob := &wireWriter{}
	blob.Name(algRSA)
	blob.Mpint(big.NewInt(int64(key.E)))
	blob.Mpint(key.N)
	return &SSHKey{Blob: blob.Bytes(), Algorithm: algRSA, rsaKey: key}, nil
}

// curveFor nennt Kurve und Kurvennamen zu einem Algorithmusnamen.
func curveFor(algorithm string) (elliptic.Curve, string, bool) {
	switch algorithm {
	case algP256:
		return elliptic.P256(), "nistp256", true
	case algP384:
		return elliptic.P384(), "nistp384", true
	case algP521:
		return elliptic.P521(), "nistp521", true
	}
	return nil, "", false
}

// algorithmForCurve ist der Weg zurueck.
func algorithmForCurve(curve elliptic.Curve) (string, string, bool) {
	switch curve {
	case elliptic.P256():
		return algP256, "nistp256", true
	case elliptic.P384():
		return algP384, "nistp384", true
	case elliptic.P521():
		return algP521, "nistp521", true
	}
	return "", "", false
}

// Die Zusatzbits aus dem Agentenprotokoll, mit denen ssh bei RSA eine
// neuere Streuwertfunktion verlangt. Ohne sie gilt noch SHA-1.
const (
	sshAgentRsaSha2_256 = 2
	sshAgentRsaSha2_512 = 4
)

// Sign unterschreibt data und gibt die Unterschrift im Format von SSH
// zurueck: der Name des Verfahrens, dann die Unterschrift selbst.
func (k *SSHKey) Sign(data []byte, flags uint32) ([]byte, error) {
	switch {
	case k.ed25519Key != nil:
		signature := ed25519.Sign(k.ed25519Key, data)
		return packSignature(algEd25519, signature), nil
	case k.ecdsaKey != nil:
		return k.signEcdsa(data)
	case k.rsaKey != nil:
		return k.signRsa(data, flags)
	}
	return nil, Fail("error.agent.keytype", "?")
}

func (k *SSHKey) signEcdsa(data []byte) ([]byte, error) {
	digest, err := hashFor(k.Algorithm, data)
	if err != nil {
		return nil, err
	}
	r, s, err := ecdsa.Sign(rand.Reader, k.ecdsaKey, digest)
	if err != nil {
		return nil, Fail("error.agent.sign")
	}
	// Bei ECDSA stecken die zwei Zahlen in einer eigenen Nachricht,
	// und die wiederum ist die Unterschrift (RFC 5656 Abschnitt 3.1.2).
	inner := &wireWriter{}
	inner.Mpint(r)
	inner.Mpint(s)
	return packSignature(k.Algorithm, inner.Bytes()), nil
}

func (k *SSHKey) signRsa(data []byte, flags uint32) ([]byte, error) {
	algorithm := algRSA
	hash := crypto.SHA1
	switch {
	case flags&sshAgentRsaSha2_512 != 0:
		algorithm, hash = algRSA512, crypto.SHA512
	case flags&sshAgentRsaSha2_256 != 0:
		algorithm, hash = algRSA256, crypto.SHA256
	}
	var digest []byte
	switch hash {
	case crypto.SHA512:
		sum := sha512.Sum512(data)
		digest = sum[:]
	case crypto.SHA256:
		sum := sha256.Sum256(data)
		digest = sum[:]
	default:
		// SHA-1 ist gebrochen. Es bleibt hier, weil ein alter Server ohne
		// diese Form keine Anmeldung zulaesst - aber ssh verlangt es nur,
		// wenn es keine Zusatzbits schickt, und das tut heute keines mehr.
		sum := sha1.Sum(data)
		digest = sum[:]
	}
	signature, err := rsa.SignPKCS1v15(rand.Reader, k.rsaKey, hash, digest)
	if err != nil {
		return nil, Fail("error.agent.sign")
	}
	return packSignature(algorithm, signature), nil
}

// hashFor streut nach der Kurve: je groesser die Kurve, desto groesser der
// Streuwert (RFC 5656 Abschnitt 6.2.1).
func hashFor(algorithm string, data []byte) ([]byte, error) {
	switch algorithm {
	case algP256:
		sum := sha256.Sum256(data)
		return sum[:], nil
	case algP384:
		sum := sha512.Sum384(data)
		return sum[:], nil
	case algP521:
		sum := sha512.Sum512(data)
		return sum[:], nil
	}
	return nil, Fail("error.agent.keytype", algorithm)
}

// packSignature baut die Unterschrift im Format von SSH.
func packSignature(algorithm string, signature []byte) []byte {
	out := &wireWriter{}
	out.Name(algorithm)
	out.String(signature)
	return out.Bytes()
}

// SSHPublicLine ist die Zeile, die in authorized_keys gehoert:
// Verfahren, oeffentlicher Teil in base64, Name des Eintrags.
func SSHPublicLine(blob []byte, comment string) (string, error) {
	reader := newWireReader(blob)
	algorithm, err := reader.Name()
	if err != nil {
		return "", err
	}
	line := algorithm + " " + base64.StdEncoding.EncodeToString(blob)
	if clean := CleanAskedBy(comment); clean != "" {
		line += " " + clean
	}
	return line, nil
}
